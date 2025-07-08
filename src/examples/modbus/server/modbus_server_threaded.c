#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <modbus.h>
#include <ptk_alloc.h>
#include <ptk_buf.h>
#include <ptk_config.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <ptk_socket.h>
#include <ptk_thread.h>
#include <ptk_utils.h>

//=============================================================================
// CONSTANTS AND DEFAULTS
//=============================================================================

#define DEFAULT_LISTEN_ADDR "0.0.0.0:502"
#define DEFAULT_NUM_HOLDING_REGS 100
#define DEFAULT_NUM_INPUT_REGS 100
#define DEFAULT_NUM_COILS 100
#define DEFAULT_NUM_DISCRETE_INPUTS 100
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Modbus protocol limits
#define MODBUS_MAX_HOLDING_REGS 65535
#define MODBUS_MAX_INPUT_REGS 65535
#define MODBUS_MAX_COILS 65535
#define MODBUS_MAX_DISCRETE_INPUTS 65535

// Modbus exception codes
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE 0x03
#define MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE 0x04

//=============================================================================
// GLOBAL STATE
//=============================================================================

typedef struct modbus_server_state {
    ptk_allocator_t *allocator;
    modbus_connection *server_conn;
    bool should_shutdown;

    // Register storage with thread safety using simple approach
    uint16_t *holding_registers;
    uint16_t num_holding_regs;
    ptk_mutex *holding_registers_mutex;

    uint16_t *input_registers;
    uint16_t num_input_regs;
    ptk_mutex *input_registers_mutex;

    bool *coils;
    uint16_t num_coils;
    ptk_mutex *coils_mutex;

    bool *discrete_inputs;
    uint16_t num_discrete_inputs;
    ptk_mutex *discrete_inputs_mutex;

    // Client management
    int active_clients;
    ptk_mutex *client_count_mutex;
} modbus_server_state_t;

static modbus_server_state_t *g_server_state = NULL;

//=============================================================================
// CLIENT THREAD CONTEXT
//=============================================================================

typedef struct client_thread_context {
    modbus_connection *client_conn;
    ptk_buf *client_buffer;
    modbus_server_state_t *server_state;
    int client_id;
} client_thread_context_t;

//=============================================================================
// SIGNAL HANDLING
//=============================================================================

void signal_handler(int signum) {
    if(g_server_state) {
        info("Received signal %d, shutting down server...", signum);
        g_server_state->should_shutdown = true;
    }
}

void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

//=============================================================================
// ARGUMENT PARSING
//=============================================================================

typedef struct server_config {
    const char *listen_addr;
    uint16_t num_holding_regs;
    uint16_t num_input_regs;
    uint16_t num_coils;
    uint16_t num_discrete_inputs;
} server_config_t;

int parse_arguments(int argc, char *argv[], server_config_t *config) {
    // Set defaults
    config->listen_addr = DEFAULT_LISTEN_ADDR;
    config->num_holding_regs = DEFAULT_NUM_HOLDING_REGS;
    config->num_input_regs = DEFAULT_NUM_INPUT_REGS;
    config->num_coils = DEFAULT_NUM_COILS;
    config->num_discrete_inputs = DEFAULT_NUM_DISCRETE_INPUTS;

    // Define configuration fields
    ptk_config_field_t fields[] = {
        {"listen-addr", 'l', PTK_CONFIG_STRING, &config->listen_addr, "Listen address and port", DEFAULT_LISTEN_ADDR},
        {"num-holding-regs", 0, PTK_CONFIG_UINT16, &config->num_holding_regs, "Number of holding registers (max: 65535)", "100"},
        {"num-input-regs", 0, PTK_CONFIG_UINT16, &config->num_input_regs, "Number of input registers (max: 65535)", "100"},
        {"num-coils", 0, PTK_CONFIG_UINT16, &config->num_coils, "Number of coils (max: 65535)", "100"},
        {"num-discrete-inputs", 0, PTK_CONFIG_UINT16, &config->num_discrete_inputs, "Number of discrete inputs (max: 65535)",
         "100"},
        PTK_CONFIG_END};

    int result = ptk_config_parse(argc, argv, fields, "Modbus TCP Multi-threaded Server");
    if(result == 1) {
        return 1;  // Help was shown
    } else if(result != 0) {
        error("Failed to parse arguments");
        return -1;
    }

    // Validate ranges
    if(config->num_holding_regs > MODBUS_MAX_HOLDING_REGS) {
        error("Invalid number of holding registers: %d (max: %d)", config->num_holding_regs, MODBUS_MAX_HOLDING_REGS);
        return -1;
    }
    if(config->num_input_regs > MODBUS_MAX_INPUT_REGS) {
        error("Invalid number of input registers: %d (max: %d)", config->num_input_regs, MODBUS_MAX_INPUT_REGS);
        return -1;
    }
    if(config->num_coils > MODBUS_MAX_COILS) {
        error("Invalid number of coils: %d (max: %d)", config->num_coils, MODBUS_MAX_COILS);
        return -1;
    }
    if(config->num_discrete_inputs > MODBUS_MAX_DISCRETE_INPUTS) {
        error("Invalid number of discrete inputs: %d (max: %d)", config->num_discrete_inputs, MODBUS_MAX_DISCRETE_INPUTS);
        return -1;
    }

    return 0;
}

//=============================================================================
// DESTRUCTOR FUNCTIONS FOR AUTOMATIC CLEANUP
//=============================================================================

void modbus_connection_destructor(void *conn) {
    if(conn) { modbus_close((modbus_connection *)conn); }
}

void ptk_buf_destructor(void *buf) {
    if(buf) { ptk_buf_dispose((ptk_buf *)buf); }
}

void ptk_mutex_destructor(void *mutex) {
    if(mutex) { ptk_mutex_destroy((ptk_mutex *)mutex); }
}

void ptk_thread_destructor(void *thread) {
    if(thread) { ptk_thread_destroy((ptk_thread *)thread); }
}

//=============================================================================
// SERVER STATE MANAGEMENT
//=============================================================================

modbus_server_state_t *create_server_state(ptk_allocator_t *allocator, const server_config_t *config) {
    modbus_server_state_t *state = ptk_alloc(allocator, sizeof(modbus_server_state_t));
    if(!state) {
        error("Failed to allocate server state");
        return NULL;
    }

    memset(state, 0, sizeof(modbus_server_state_t));
    state->allocator = allocator;
    state->should_shutdown = false;
    state->active_clients = 0;

    // Initialize mutexes
    state->holding_registers_mutex = ptk_mutex_create(allocator);
    state->input_registers_mutex = ptk_mutex_create(allocator);
    state->coils_mutex = ptk_mutex_create(allocator);
    state->discrete_inputs_mutex = ptk_mutex_create(allocator);
    state->client_count_mutex = ptk_mutex_create(allocator);

    if(!state->holding_registers_mutex || !state->input_registers_mutex || !state->coils_mutex || !state->discrete_inputs_mutex
       || !state->client_count_mutex) {
        error("Failed to initialize mutexes");
        return NULL;
    }

    // Store configuration
    state->num_holding_regs = config->num_holding_regs;
    state->num_input_regs = config->num_input_regs;
    state->num_coils = config->num_coils;
    state->num_discrete_inputs = config->num_discrete_inputs;

    // Allocate register storage
    if(state->num_holding_regs > 0) {
        state->holding_registers = ptk_alloc(allocator, state->num_holding_regs * sizeof(uint16_t));
        if(!state->holding_registers) {
            error("Failed to allocate holding registers");
            return NULL;
        }
        // Initialize with some test values
        for(int i = 0; i < state->num_holding_regs; i++) { state->holding_registers[i] = i + 1000; }
    }

    if(state->num_input_regs > 0) {
        state->input_registers = ptk_alloc(allocator, state->num_input_regs * sizeof(uint16_t));
        if(!state->input_registers) {
            error("Failed to allocate input registers");
            return NULL;
        }
        // Initialize with some test values
        for(int i = 0; i < state->num_input_regs; i++) { state->input_registers[i] = i + 2000; }
    }

    if(state->num_coils > 0) {
        state->coils = ptk_alloc(allocator, state->num_coils * sizeof(bool));
        if(!state->coils) {
            error("Failed to allocate coils");
            return NULL;
        }
        // Initialize some test values
        for(int i = 0; i < state->num_coils; i++) { state->coils[i] = (i % 2 == 0); }
    }

    if(state->num_discrete_inputs > 0) {
        state->discrete_inputs = ptk_alloc(allocator, state->num_discrete_inputs * sizeof(bool));
        if(!state->discrete_inputs) {
            error("Failed to allocate discrete inputs");
            return NULL;
        }
        // Initialize some test values
        for(int i = 0; i < state->num_discrete_inputs; i++) { state->discrete_inputs[i] = (i % 3 == 0); }
    }

    info("Server state created: %d holding regs, %d input regs, %d coils, %d discrete inputs", state->num_holding_regs,
         state->num_input_regs, state->num_coils, state->num_discrete_inputs);

    return state;
}

void destroy_server_state(modbus_server_state_t *state) {
    if(!state) { return; }

    // Clean up server connection (not allocated from our allocator)
    if(state->server_conn) { modbus_close(state->server_conn); }

    // Note: All other resources (mutexes, register arrays, etc.) will be automatically
    // cleaned up when the arena allocator is destroyed in main()
}

//=============================================================================
// SIMPLIFIED MODBUS REQUEST HANDLER
//=============================================================================

int handle_client_request(modbus_connection *conn, modbus_server_state_t *state) {
    ptk_err err;

    // Try to handle a holding register read request
    uint16_t register_addr;
    err = server_recv_read_holding_register_req(conn, &register_addr);
    if(err == PTK_OK) {
        // Handle holding register read request
        ptk_mutex_wait_lock(state->holding_registers_mutex, PTK_TIME_WAIT_FOREVER);

        if(register_addr < state->num_holding_regs) {
            uint16_t value = state->holding_registers[register_addr];
            ptk_mutex_unlock(state->holding_registers_mutex);

            err = server_send_read_holding_register_resp(conn, value);
            if(err != PTK_OK) {
                error("Failed to send holding register response: %d", err);
                return -1;
            }
            return 0;
        } else {
            ptk_mutex_unlock(state->holding_registers_mutex);
            // Send exception response for invalid address
            err = server_send_exception_resp(conn, 0x03, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            if(err != PTK_OK) {
                error("Failed to send exception response: %d", err);
                return -1;
            }
            return 0;
        }
    }

    // Try to handle a holding register write request
    uint16_t write_addr, write_value;
    err = server_recv_write_holding_register_req(conn, &write_addr, &write_value);
    if(err == PTK_OK) {
        // Handle holding register write request
        ptk_mutex_wait_lock(state->holding_registers_mutex, PTK_TIME_WAIT_FOREVER);

        if(write_addr < state->num_holding_regs) {
            state->holding_registers[write_addr] = write_value;
            ptk_mutex_unlock(state->holding_registers_mutex);

            err = server_send_write_holding_register_resp(conn);
            if(err != PTK_OK) {
                error("Failed to send write holding register response: %d", err);
                return -1;
            }
            return 0;
        } else {
            ptk_mutex_unlock(state->holding_registers_mutex);
            // Send exception response for invalid address
            err = server_send_exception_resp(conn, 0x06, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            if(err != PTK_OK) {
                error("Failed to send exception response: %d", err);
                return -1;
            }
            return 0;
        }
    }

    // Try to handle an input register read request
    err = server_recv_read_input_register_req(conn, &register_addr);
    if(err == PTK_OK) {
        ptk_mutex_wait_lock(state->input_registers_mutex, PTK_TIME_WAIT_FOREVER);

        if(register_addr < state->num_input_regs) {
            uint16_t value = state->input_registers[register_addr];
            ptk_mutex_unlock(state->input_registers_mutex);

            err = server_send_read_input_register_resp(conn, value);
            if(err != PTK_OK) {
                error("Failed to send input register response: %d", err);
                return -1;
            }
            return 0;
        } else {
            ptk_mutex_unlock(state->input_registers_mutex);
            err = server_send_exception_resp(conn, 0x04, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            if(err != PTK_OK) {
                error("Failed to send exception response: %d", err);
                return -1;
            }
            return 0;
        }
    }

    // Try to handle coil read/write requests
    uint16_t coil_addr;
    err = server_recv_read_coil_req(conn, &coil_addr);
    if(err == PTK_OK) {
        ptk_mutex_wait_lock(state->coils_mutex, PTK_TIME_WAIT_FOREVER);

        if(coil_addr < state->num_coils) {
            bool value = state->coils[coil_addr];
            ptk_mutex_unlock(state->coils_mutex);

            err = server_send_read_coil_resp(conn, value);
            if(err != PTK_OK) {
                error("Failed to send coil response: %d", err);
                return -1;
            }
            return 0;
        } else {
            ptk_mutex_unlock(state->coils_mutex);
            err = server_send_exception_resp(conn, 0x01, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            if(err != PTK_OK) {
                error("Failed to send exception response: %d", err);
                return -1;
            }
            return 0;
        }
    }

    // Try to handle coil write request
    bool coil_value;
    err = server_recv_write_coil_req(conn, &coil_addr, &coil_value);
    if(err == PTK_OK) {
        ptk_mutex_wait_lock(state->coils_mutex, PTK_TIME_WAIT_FOREVER);

        if(coil_addr < state->num_coils) {
            state->coils[coil_addr] = coil_value;
            ptk_mutex_unlock(state->coils_mutex);

            err = server_send_write_coil_resp(conn);
            if(err != PTK_OK) {
                error("Failed to send write coil response: %d", err);
                return -1;
            }
            return 0;
        } else {
            ptk_mutex_unlock(state->coils_mutex);
            err = server_send_exception_resp(conn, 0x05, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            if(err != PTK_OK) {
                error("Failed to send exception response: %d", err);
                return -1;
            }
            return 0;
        }
    }

    // Try to handle discrete input read request
    uint16_t input_addr;
    err = server_recv_read_discrete_input_req(conn, &input_addr);
    if(err == PTK_OK) {
        ptk_mutex_wait_lock(state->discrete_inputs_mutex, PTK_TIME_WAIT_FOREVER);

        if(input_addr < state->num_discrete_inputs) {
            bool value = state->discrete_inputs[input_addr];
            ptk_mutex_unlock(state->discrete_inputs_mutex);

            err = server_send_read_discrete_input_resp(conn, value);
            if(err != PTK_OK) {
                error("Failed to send discrete input response: %d", err);
                return -1;
            }
            return 0;
        } else {
            ptk_mutex_unlock(state->discrete_inputs_mutex);
            err = server_send_exception_resp(conn, 0x02, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            if(err != PTK_OK) {
                error("Failed to send exception response: %d", err);
                return -1;
            }
            return 0;
        }
    }

    // If we reach here, no handler matched the request
    warn("Unhandled or invalid request");
    server_send_exception_resp(conn, 0x00, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
    return -1;
}

//=============================================================================
// CLIENT THREAD HANDLER
//=============================================================================

void client_thread_handler(void *arg) {
    client_thread_context_t *ctx = (client_thread_context_t *)arg;
    modbus_server_state_t *state = ctx->server_state;
    modbus_connection *client_conn = ctx->client_conn;
    int client_id = ctx->client_id;

    info("Client thread %d started", client_id);

    // Handle client requests in a loop
    while(!state->should_shutdown) {
        int result = handle_client_request(client_conn, state);
        if(result != 0) {
            info("Client %d disconnected or error occurred", client_id);
            break;
        }
    }

    // Clean up client connection
    modbus_close(client_conn);
    ptk_buf_dispose(ctx->client_buffer);

    // Update client count
    ptk_mutex_wait_lock(state->client_count_mutex, PTK_TIME_WAIT_FOREVER);
    state->active_clients--;
    int remaining_clients = state->active_clients;
    ptk_mutex_unlock(state->client_count_mutex);

    info("Client thread %d finished. %d clients remaining", client_id, remaining_clients);

    // Note: Context will be cleaned up automatically when the arena allocator is destroyed
}

//=============================================================================
// MULTI-THREADED SERVER LOGIC
//=============================================================================

int start_server(modbus_server_state_t *state, const char *listen_addr) {
    // Parse address and port
    char addr_copy[256];
    strncpy(addr_copy, listen_addr, sizeof(addr_copy) - 1);
    addr_copy[sizeof(addr_copy) - 1] = '\0';

    char *colon = strrchr(addr_copy, ':');
    if(!colon) {
        error("Invalid listen address format. Expected 'host:port'");
        return -1;
    }

    *colon = '\0';
    char *host = addr_copy;
    int port = atoi(colon + 1);

    if(port <= 0 || port > 65535) {
        error("Invalid port number: %d", port);
        return -1;
    }

    // Create address structure
    ptk_address_t addr;
    ptk_err err = ptk_address_create(&addr, host, (uint16_t)port);
    if(err != PTK_OK) {
        error("Failed to create address from %s:%d: error %d", host, port, err);
        return -1;
    }

    info("Created address for %s:%d", host, port);

    // Create server buffer with automatic cleanup if arena allocator is used
    ptk_buf *server_buffer = ptk_buf_create(state->allocator, BUFFER_SIZE);
    if(!server_buffer) {
        error("Failed to create server buffer");
        return -1;
    }

    info("Created server buffer");

    // Start Modbus server
    state->server_conn = modbus_open_server(state->allocator, &addr, 1, server_buffer);
    if(!state->server_conn) {
        error("Failed to start Modbus server on %s:%d - modbus_open_server returned NULL", host, port);
        ptk_buf_dispose(server_buffer);
        return -1;
    }

    info("Modbus multi-threaded server listening on %s:%d", host, port);
    info("Max concurrent clients: %d", MAX_CLIENTS);
    info("Supported functions: Read/Write Holding Registers (single), Read Input Registers (single)");
    info("                      Read/Write Coils (single), Read Discrete Inputs (single)");

    int client_id_counter = 0;

    // Main accept loop
    while(!state->should_shutdown) {
        // Check if we have room for more clients
        ptk_mutex_wait_lock(state->client_count_mutex, PTK_TIME_WAIT_FOREVER);
        int current_clients = state->active_clients;
        ptk_mutex_unlock(state->client_count_mutex);

        if(current_clients >= MAX_CLIENTS) {
            info("Maximum client limit reached (%d), waiting...", MAX_CLIENTS);
            usleep(100000);  // Sleep 100ms
            continue;
        }

        // Create buffer for new client
        ptk_buf *client_buffer = ptk_buf_create(state->allocator, BUFFER_SIZE);
        if(!client_buffer) {
            error("Failed to create client buffer");
            continue;
        }

        // Accept new client connection
        modbus_connection *client_conn = server_accept_connection(state->server_conn, client_buffer);
        if(!client_conn) {
            if(!state->should_shutdown) { error("Failed to accept client connection"); }
            ptk_buf_dispose(client_buffer);
            continue;
        }

        // Create client thread context
        client_thread_context_t *ctx = ptk_alloc(state->allocator, sizeof(client_thread_context_t));
        if(!ctx) {
            error("Failed to allocate client context");
            modbus_close(client_conn);
            ptk_buf_dispose(client_buffer);
            continue;
        }

        ctx->client_conn = client_conn;
        ctx->client_buffer = client_buffer;
        ctx->server_state = state;
        ctx->client_id = ++client_id_counter;

        // Update client count
        ptk_mutex_wait_lock(state->client_count_mutex, PTK_TIME_WAIT_FOREVER);
        state->active_clients++;
        int total_clients = state->active_clients;
        ptk_mutex_unlock(state->client_count_mutex);

        info("Accepted new client connection (ID: %d, Total: %d)", ctx->client_id, total_clients);

        // Create client thread
        ptk_thread *client_thread = ptk_thread_create(state->allocator, client_thread_handler, ctx);
        if(!client_thread) {
            error("Failed to create client thread");
            modbus_close(client_conn);
            ptk_buf_dispose(client_buffer);
            // Note: ctx will be cleaned up automatically when allocator is destroyed

            // Decrement client count
            ptk_mutex_wait_lock(state->client_count_mutex, PTK_TIME_WAIT_FOREVER);
            state->active_clients--;
            ptk_mutex_unlock(state->client_count_mutex);
            continue;
        }

        // Don't join the thread - let it run independently
        // The thread will clean itself up when done
    }

    info("Server accept loop shutting down...");

    // Wait for all client threads to finish
    while(true) {
        ptk_mutex_wait_lock(state->client_count_mutex, PTK_TIME_WAIT_FOREVER);
        int remaining_clients = state->active_clients;
        ptk_mutex_unlock(state->client_count_mutex);

        if(remaining_clients == 0) { break; }

        info("Waiting for %d client threads to finish...", remaining_clients);
        usleep(500000);  // Sleep 500ms
    }

    info("All client threads finished");
    return 0;
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    // Parse command line arguments
    server_config_t config;
    int parse_result = parse_arguments(argc, argv, &config);
    if(parse_result != 0) {
        return parse_result > 0 ? 0 : 1;  // Help message returns 0, errors return 1
    }

    // Setup signal handlers
    setup_signal_handlers();

    // Create arena allocator for automatic cleanup
    ptk_allocator_t *allocator = allocator_arena_create(64 * 1024 * 1024, 8);  // 64MB arena, 8-byte alignment
    if(!allocator) {
        error("Failed to create arena allocator");
        return 1;
    }

    // Create server state
    modbus_server_state_t *server_state = create_server_state(allocator, &config);
    if(!server_state) {
        error("Failed to create server state");
        ptk_allocator_destroy(allocator);
        return 1;
    }

    g_server_state = server_state;

    info("Starting Modbus TCP Multi-threaded Server...");
    info("Configuration:");
    info("  Listen address: %s", config.listen_addr);
    info("  Holding registers: %d", config.num_holding_regs);
    info("  Input registers: %d", config.num_input_regs);
    info("  Coils: %d", config.num_coils);
    info("  Discrete inputs: %d", config.num_discrete_inputs);
    info("  Max concurrent clients: %d", MAX_CLIENTS);

    // Start the server
    int result = start_server(server_state, config.listen_addr);

    info("Server shutting down...");

    // Clean up server state (mainly closes server connection)
    destroy_server_state(server_state);

    // Destroy arena allocator - this automatically cleans up ALL allocated resources:
    // - Server state structure
    // - All mutexes
    // - All register arrays
    // - All client thread contexts
    // - All buffers allocated from this allocator
    // - All threads created with this allocator
    info("Cleaning up all resources...");
    ptk_allocator_destroy(allocator);
    info("All resources cleaned up automatically");

    return result;
}
