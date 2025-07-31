#include "protocol_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

// Modbus TCP Configuration
#define MODBUS_TCP_PORT 5020
#define MAX_CLIENTS 10
#define MODBUS_TCP_HEADER_SIZE 6
#define MODBUS_PDU_MAX_SIZE 253
#define MODBUS_MAX_FRAME_SIZE (MODBUS_TCP_HEADER_SIZE + MODBUS_PDU_MAX_SIZE)

// Register counts (as requested: 100 of each type)
#define NUM_COILS 100
#define NUM_DISCRETE_INPUTS 100
#define NUM_HOLDING_REGISTERS 100
#define NUM_INPUT_REGISTERS 100

// Event IDs
enum {
    EVENT_SERVER_ACCEPT = 1,
    EVENT_CLIENT_STARTUP,
    EVENT_CLIENT_READ,
    EVENT_CLIENT_WRITE,
    EVENT_CLIENT_TIMEOUT,
    EVENT_CLIENT_DISCONNECT,
    EVENT_HEARTBEAT
};

// Server State Machine States
enum { SERVER_STATE_INIT = 0, SERVER_STATE_LISTENING, SERVER_STATE_ACCEPTING, SERVER_STATE_ERROR };

// Client State Machine States
enum {
    CLIENT_STATE_STARTUP = 0,
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_READING_HEADER,
    CLIENT_STATE_READING_PDU,
    CLIENT_STATE_PROCESSING,
    CLIENT_STATE_SENDING_RESPONSE,
    CLIENT_STATE_DISCONNECTING
};

// Modbus Function Codes
enum {
    MODBUS_FC_READ_COILS = 0x01,
    MODBUS_FC_READ_DISCRETE_INPUTS = 0x02,
    MODBUS_FC_READ_HOLDING_REGISTERS = 0x03,
    MODBUS_FC_READ_INPUT_REGISTERS = 0x04,
    MODBUS_FC_WRITE_SINGLE_COIL = 0x05,
    MODBUS_FC_WRITE_SINGLE_REGISTER = 0x06,
    MODBUS_FC_WRITE_MULTIPLE_COILS = 0x0F,
    MODBUS_FC_WRITE_MULTIPLE_REGISTERS = 0x10
};

// Modbus Exception Codes
enum {
    MODBUS_EX_NONE = 0x00,
    MODBUS_EX_ILLEGAL_FUNCTION = 0x01,
    MODBUS_EX_ILLEGAL_DATA_ADDRESS = 0x02,
    MODBUS_EX_ILLEGAL_DATA_VALUE = 0x03,
    MODBUS_EX_SLAVE_DEVICE_FAILURE = 0x04
};

// Modbus TCP Header
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
} __attribute__((packed)) modbus_tcp_header_t;

// Modbus Register Storage
typedef struct {
    bool coils[NUM_COILS];
    bool discrete_inputs[NUM_DISCRETE_INPUTS];
    uint16_t holding_registers[NUM_HOLDING_REGISTERS];
    uint16_t input_registers[NUM_INPUT_REGISTERS];
} modbus_registers_t;

// Client Connection Context
typedef struct {
    int client_id;
    ptk_socket_t socket;
    ptk_state_machine_t state_machine;
    ptk_event_source_t startup_source;
    ptk_event_source_t read_source;
    ptk_event_source_t write_source;
    ptk_event_source_t timeout_source;

    // Modbus protocol state
    uint8_t rx_buffer[MODBUS_MAX_FRAME_SIZE];
    uint8_t tx_buffer[MODBUS_MAX_FRAME_SIZE];
    size_t rx_bytes_received;
    size_t tx_bytes_to_send;
    size_t tx_bytes_sent;
    modbus_tcp_header_t current_header;
    bool header_complete;

    // Transition tables for this client
    ptk_transition_table_t connection_table;
    ptk_transition_table_t protocol_table;
    ptk_transition_t *connection_transitions;
    ptk_transition_t *protocol_transitions;
    ptk_transition_table_t *tables[2];
    ptk_event_source_t *sources[4];

    bool in_use;
} client_context_t;

// Server Context
typedef struct {
    ptk_socket_t server_socket;
    ptk_state_machine_t server_state_machine;
    ptk_event_source_t accept_source;
    ptk_event_source_t heartbeat_source;

    // Server transition tables
    ptk_transition_table_t server_table;
    ptk_transition_t *server_transitions;
    ptk_transition_table_t *server_tables[1];
    ptk_event_source_t *server_sources[2];

    // Client management
    client_context_t clients[MAX_CLIENTS];
    int next_client_id;

    // Modbus register storage (shared across all clients)
    modbus_registers_t registers;

    ptk_loop_t *loop;
} server_context_t;

// Global server instance
static server_context_t g_server;

// Forward declarations
static void server_accept_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms);
static void server_heartbeat_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms);
static void client_startup_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms);
static void client_read_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms);
static void client_write_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms);
static void client_timeout_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms);
static void client_disconnect_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms);

// Utility functions
static uint16_t modbus_read_uint16_be(const uint8_t *data) { return (data[0] << 8) | data[1]; }

static void modbus_write_uint16_be(uint8_t *data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

static uint8_t modbus_process_request(client_context_t *client, const uint8_t *pdu, size_t pdu_len, uint8_t *response,
                                      size_t *response_len) {
    if(pdu_len < 1) { return MODBUS_EX_ILLEGAL_FUNCTION; }

    uint8_t function_code = pdu[0];
    modbus_registers_t *regs = &g_server.registers;

    switch(function_code) {
        case MODBUS_FC_READ_COILS: {
            if(pdu_len < 5) { return MODBUS_EX_ILLEGAL_FUNCTION; }

            uint16_t start_addr = modbus_read_uint16_be(&pdu[1]);
            uint16_t quantity = modbus_read_uint16_be(&pdu[3]);

            if(start_addr >= NUM_COILS || (start_addr + quantity) > NUM_COILS || quantity == 0 || quantity > 2000) {
                return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
            }

            uint8_t byte_count = (quantity + 7) / 8;
            response[0] = function_code;
            response[1] = byte_count;

            // Pack coils into bytes
            for(int i = 0; i < byte_count; i++) {
                uint8_t byte_val = 0;
                for(int bit = 0; bit < 8 && (i * 8 + bit) < quantity; bit++) {
                    if(regs->coils[start_addr + i * 8 + bit]) { byte_val |= (1 << bit); }
                }
                response[2 + i] = byte_val;
            }

            *response_len = 2 + byte_count;
            return MODBUS_EX_NONE;
        }

        case MODBUS_FC_READ_HOLDING_REGISTERS: {
            if(pdu_len < 5) { return MODBUS_EX_ILLEGAL_FUNCTION; }

            uint16_t start_addr = modbus_read_uint16_be(&pdu[1]);
            uint16_t quantity = modbus_read_uint16_be(&pdu[3]);

            if(start_addr >= NUM_HOLDING_REGISTERS || (start_addr + quantity) > NUM_HOLDING_REGISTERS || quantity == 0
               || quantity > 125) {
                return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
            }

            response[0] = function_code;
            response[1] = quantity * 2;  // byte count

            for(int i = 0; i < quantity; i++) {
                modbus_write_uint16_be(&response[2 + i * 2], regs->holding_registers[start_addr + i]);
            }

            *response_len = 2 + quantity * 2;
            return MODBUS_EX_NONE;
        }

        case MODBUS_FC_WRITE_SINGLE_COIL: {
            if(pdu_len < 5) { return MODBUS_EX_ILLEGAL_FUNCTION; }

            uint16_t addr = modbus_read_uint16_be(&pdu[1]);
            uint16_t value = modbus_read_uint16_be(&pdu[3]);

            if(addr >= NUM_COILS) { return MODBUS_EX_ILLEGAL_DATA_ADDRESS; }

            if(value != 0x0000 && value != 0xFF00) { return MODBUS_EX_ILLEGAL_DATA_VALUE; }

            regs->coils[addr] = (value == 0xFF00);

            // Echo the request as response
            memcpy(response, pdu, 5);
            *response_len = 5;
            return MODBUS_EX_NONE;
        }

        case MODBUS_FC_WRITE_SINGLE_REGISTER: {
            if(pdu_len < 5) { return MODBUS_EX_ILLEGAL_FUNCTION; }

            uint16_t addr = modbus_read_uint16_be(&pdu[1]);
            uint16_t value = modbus_read_uint16_be(&pdu[3]);

            if(addr >= NUM_HOLDING_REGISTERS) { return MODBUS_EX_ILLEGAL_DATA_ADDRESS; }

            regs->holding_registers[addr] = value;

            // Echo the request as response
            memcpy(response, pdu, 5);
            *response_len = 5;
            return MODBUS_EX_NONE;
        }

        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS: {
            if(pdu_len < 6) { return MODBUS_EX_ILLEGAL_FUNCTION; }

            uint16_t start_addr = modbus_read_uint16_be(&pdu[1]);
            uint16_t quantity = modbus_read_uint16_be(&pdu[3]);
            uint8_t byte_count = pdu[5];

            if(start_addr >= NUM_HOLDING_REGISTERS || (start_addr + quantity) > NUM_HOLDING_REGISTERS || quantity == 0
               || quantity > 123 || byte_count != (quantity * 2) || pdu_len < (6 + byte_count)) {
                return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
            }

            for(int i = 0; i < quantity; i++) {
                regs->holding_registers[start_addr + i] = modbus_read_uint16_be(&pdu[6 + i * 2]);
            }

            response[0] = function_code;
            modbus_write_uint16_be(&response[1], start_addr);
            modbus_write_uint16_be(&response[3], quantity);
            *response_len = 5;
            return MODBUS_EX_NONE;
        }

        default: return MODBUS_EX_ILLEGAL_FUNCTION;
    }
}

// Client State Machine Actions
static void client_startup_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    client_context_t *client = (client_context_t *)sm->user_data;

    printf("[Client %d] Starting up, activating timeout timer\n", client->client_id);

    // Attach the timeout timer now that client is actively running
    ptk_error_t result = ptk_sm_attach_event_source(sm, &client->timeout_source);
    if(result != PTK_SUCCESS) {
        printf("[Client %d] Failed to attach timeout timer during startup\n", client->client_id);
        sm->current_state = CLIENT_STATE_DISCONNECTING;
        return;
    }

    // Transition to reading header state
    sm->current_state = CLIENT_STATE_READING_HEADER;
}

static void client_read_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    client_context_t *client = (client_context_t *)sm->user_data;

    if(sm->current_state == CLIENT_STATE_READING_HEADER && !client->header_complete) {
        // Read Modbus TCP header
        size_t bytes_needed = MODBUS_TCP_HEADER_SIZE - client->rx_bytes_received;
        size_t bytes_read = 0;

        ptk_error_t result =
            ptk_socket_receive(&client->socket, client->rx_buffer + client->rx_bytes_received, bytes_needed, &bytes_read);

        if(result == PTK_SUCCESS && bytes_read > 0) {
            client->rx_bytes_received += bytes_read;

            if(client->rx_bytes_received >= MODBUS_TCP_HEADER_SIZE) {
                // Parse header
                memcpy(&client->current_header, client->rx_buffer, MODBUS_TCP_HEADER_SIZE);
                client->current_header.transaction_id = ntohs(client->current_header.transaction_id);
                client->current_header.protocol_id = ntohs(client->current_header.protocol_id);
                client->current_header.length = ntohs(client->current_header.length);

                client->header_complete = true;

                if(client->current_header.length > 1) {
                    // Need to read PDU
                    sm->current_state = CLIENT_STATE_READING_PDU;
                } else {
                    // Complete frame, process it
                    sm->current_state = CLIENT_STATE_PROCESSING;
                }
            }
        }
    } else if(sm->current_state == CLIENT_STATE_READING_PDU) {
        // Read Modbus PDU
        size_t pdu_length = client->current_header.length - 1;  // Subtract unit ID
        size_t total_needed = MODBUS_TCP_HEADER_SIZE + pdu_length;
        size_t bytes_needed = total_needed - client->rx_bytes_received;
        size_t bytes_read = 0;

        ptk_error_t result =
            ptk_socket_receive(&client->socket, client->rx_buffer + client->rx_bytes_received, bytes_needed, &bytes_read);

        if(result == PTK_SUCCESS && bytes_read > 0) {
            client->rx_bytes_received += bytes_read;

            if(client->rx_bytes_received >= total_needed) { sm->current_state = CLIENT_STATE_PROCESSING; }
        }
    }

    // If we're in processing state, handle the request
    if(sm->current_state == CLIENT_STATE_PROCESSING) {
        // Process Modbus request
        const uint8_t *pdu = client->rx_buffer + MODBUS_TCP_HEADER_SIZE;
        size_t pdu_len = client->current_header.length - 1;

        uint8_t response_pdu[MODBUS_PDU_MAX_SIZE];
        size_t response_pdu_len = 0;
        uint8_t exception = modbus_process_request(client, pdu, pdu_len, response_pdu, &response_pdu_len);

        // Build response frame
        modbus_tcp_header_t response_header = client->current_header;
        response_header.length = htons(response_pdu_len + 1);  // +1 for unit ID
        response_header.transaction_id = htons(response_header.transaction_id);
        response_header.protocol_id = htons(response_header.protocol_id);

        if(exception != MODBUS_EX_NONE) {
            // Build exception response
            response_pdu[0] = pdu[0] | 0x80;  // Set exception bit
            response_pdu[1] = exception;
            response_pdu_len = 2;
            response_header.length = htons(3);  // 2 bytes PDU + 1 byte unit ID
        }

        // Copy response to tx buffer
        memcpy(client->tx_buffer, &response_header, MODBUS_TCP_HEADER_SIZE);
        memcpy(client->tx_buffer + MODBUS_TCP_HEADER_SIZE, response_pdu, response_pdu_len);
        client->tx_bytes_to_send = MODBUS_TCP_HEADER_SIZE + response_pdu_len;
        client->tx_bytes_sent = 0;

        sm->current_state = CLIENT_STATE_SENDING_RESPONSE;

        printf("[Client %d] Processed function 0x%02X, sending %zu byte response\n", client->client_id, pdu[0],
               client->tx_bytes_to_send);
    }
}

static void client_write_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    client_context_t *client = (client_context_t *)sm->user_data;

    if(sm->current_state == CLIENT_STATE_SENDING_RESPONSE) {
        size_t bytes_remaining = client->tx_bytes_to_send - client->tx_bytes_sent;

        ptk_error_t result = ptk_socket_send(&client->socket, client->tx_buffer + client->tx_bytes_sent, bytes_remaining);

        if(result == PTK_SUCCESS) {
            client->tx_bytes_sent = client->tx_bytes_to_send;

            // Reset for next request
            client->rx_bytes_received = 0;
            client->header_complete = false;
            sm->current_state = CLIENT_STATE_READING_HEADER;

            printf("[Client %d] Response sent, waiting for next request\n", client->client_id);
        }
    }
}

static void client_timeout_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    client_context_t *client = (client_context_t *)sm->user_data;
    printf("[Client %d] Connection timeout, disconnecting\n", client->client_id);
    sm->current_state = CLIENT_STATE_DISCONNECTING;
}

static void client_disconnect_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    client_context_t *client = (client_context_t *)sm->user_data;

    printf("[Client %d] Disconnecting\n", client->client_id);

    // Deactivate the timeout timer by finding it in the loop and marking it unused
    for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
        if(sm->loop->macos.timers[i].in_use && sm->loop->macos.timers[i].source == &client->timeout_source) {
            sm->loop->macos.timers[i].in_use = false;
            client->timeout_source.macos.active = false;
            break;
        }
    }

    close(client->socket.socket_fd);
    client->in_use = false;
}

// Server State Machine Actions
static void server_accept_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    server_context_t *server = (server_context_t *)sm->user_data;

    // Find available client slot
    client_context_t *client = NULL;
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!server->clients[i].in_use) {
            client = &server->clients[i];
            break;
        }
    }

    if(!client) {
        printf("[Server] No available client slots, rejecting connection\n");
        return;
    }

    // Accept the connection
    ptk_error_t result = ptk_socket_accept(&server->server_socket, &client->socket);
    if(result == PTK_SUCCESS) {
        client->in_use = true;
        client->client_id = server->next_client_id++;
        client->rx_bytes_received = 0;
        client->header_complete = false;
        client->state_machine.current_state = CLIENT_STATE_STARTUP;

        // Register client socket events
        ptk_error_t reg_result =
            ptk_socket_register_events(server->loop, &client->socket, &client->read_source, &client->write_source);
        if(reg_result != PTK_SUCCESS) {
            printf("[Server] Failed to register client socket events\n");
            close(client->socket.socket_fd);
            client->in_use = false;
            return;
        }

        // Add client state machine to loop
        ptk_sm_add_to_loop(server->loop, &client->state_machine);

        // Trigger startup event to activate the timeout timer
        ptk_sm_handle_event(&client->state_machine, EVENT_CLIENT_STARTUP, &client->startup_source, 0);

        printf("[Server] Accepted client %d (fd: %d)\n", client->client_id, client->socket.socket_fd);
    } else {
        printf("[Server] Failed to accept connection\n");
    }
}

static void server_heartbeat_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    server_context_t *server = (server_context_t *)sm->user_data;

    int active_clients = 0;
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(server->clients[i].in_use) { active_clients++; }
    }

    printf("[Server] Heartbeat - %d active clients, registers: coils[0]=%s, holding[0]=%d\n", active_clients,
           server->registers.coils[0] ? "ON" : "OFF", server->registers.holding_registers[0]);
}

// Refactor server and client state machine initialization
static ptk_error_t init_client(client_context_t *client, ptk_loop_t *loop) {
    // Dynamically allocate transition tables
    client->connection_transitions = malloc(sizeof(ptk_transition_t) * 10);
    client->protocol_transitions = malloc(sizeof(ptk_transition_t) * 15);

    // Initialize connection transition table
    ptk_tt_init(&client->connection_table, client->connection_transitions, 10);
    ptk_tt_add_transition(&client->connection_table, CLIENT_STATE_STARTUP, EVENT_CLIENT_STARTUP, CLIENT_STATE_READING_HEADER,
                          NULL, client_startup_action);
    ptk_tt_add_transition(&client->connection_table, CLIENT_STATE_CONNECTED, EVENT_CLIENT_READ, CLIENT_STATE_READING_HEADER, NULL,
                          client_read_action);
    ptk_tt_add_transition(&client->connection_table, CLIENT_STATE_READING_HEADER, EVENT_CLIENT_READ, CLIENT_STATE_READING_HEADER,
                          NULL, client_read_action);
    ptk_tt_add_transition(&client->connection_table, CLIENT_STATE_READING_PDU, EVENT_CLIENT_READ, CLIENT_STATE_READING_PDU, NULL,
                          client_read_action);
    ptk_tt_add_transition(&client->connection_table, CLIENT_STATE_PROCESSING, EVENT_CLIENT_READ, CLIENT_STATE_PROCESSING, NULL,
                          client_read_action);

    // Initialize protocol transition table
    ptk_tt_init(&client->protocol_table, client->protocol_transitions, 15);
    ptk_tt_add_transition(&client->protocol_table, CLIENT_STATE_SENDING_RESPONSE, EVENT_CLIENT_WRITE, CLIENT_STATE_READING_HEADER,
                          NULL, client_write_action);
    ptk_tt_add_transition(&client->protocol_table, CLIENT_STATE_READING_HEADER, EVENT_CLIENT_TIMEOUT, CLIENT_STATE_DISCONNECTING,
                          NULL, client_timeout_action);
    ptk_tt_add_transition(&client->protocol_table, CLIENT_STATE_READING_PDU, EVENT_CLIENT_TIMEOUT, CLIENT_STATE_DISCONNECTING,
                          NULL, client_timeout_action);
    ptk_tt_add_transition(&client->protocol_table, CLIENT_STATE_PROCESSING, EVENT_CLIENT_TIMEOUT, CLIENT_STATE_DISCONNECTING,
                          NULL, client_timeout_action);
    ptk_tt_add_transition(&client->protocol_table, CLIENT_STATE_DISCONNECTING, EVENT_CLIENT_DISCONNECT,
                          CLIENT_STATE_DISCONNECTING, NULL, client_disconnect_action);

    // Attach tables and sources
    client->tables[0] = &client->connection_table;
    client->tables[1] = &client->protocol_table;
    ptk_sm_attach_table(&client->state_machine, &client->connection_table);
    ptk_sm_attach_table(&client->state_machine, &client->protocol_table);

    return PTK_SUCCESS;
}

static ptk_error_t init_server(server_context_t *server, ptk_loop_t *loop) {
    // Dynamically allocate server transition table
    server->server_transitions = malloc(sizeof(ptk_transition_t) * 8);

    // Initialize server transition table
    ptk_tt_init(&server->server_table, server->server_transitions, 8);
    ptk_tt_add_transition(&server->server_table, SERVER_STATE_LISTENING, EVENT_SERVER_ACCEPT, SERVER_STATE_ACCEPTING, NULL,
                          server_accept_action);
    ptk_tt_add_transition(&server->server_table, SERVER_STATE_ACCEPTING, EVENT_SERVER_ACCEPT, SERVER_STATE_LISTENING, NULL,
                          server_accept_action);
    ptk_tt_add_transition(&server->server_table, SERVER_STATE_LISTENING, EVENT_HEARTBEAT, SERVER_STATE_LISTENING, NULL,
                          server_heartbeat_action);

    // Attach table and sources
    server->server_tables[0] = &server->server_table;
    ptk_sm_attach_table(&server->server_state_machine, &server->server_table);

    return PTK_SUCCESS;
}

int main() {
    printf("Starting Modbus TCP Server on port %d\n", MODBUS_TCP_PORT);
    printf("Register configuration: %d coils, %d discrete inputs, %d holding registers, %d input registers\n", NUM_COILS,
           NUM_DISCRETE_INPUTS, NUM_HOLDING_REGISTERS, NUM_INPUT_REGISTERS);

    // Initialize event loop
    ptk_loop_t loop;
    ptk_error_t result = ptk_loop_init(&loop, &g_server.server_state_machine);
    if(result != PTK_SUCCESS) {
        printf("Failed to initialize event loop: %d\n", result);
        return 1;
    }

    // Initialize server and clients
    init_server(&g_server, &loop);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        init_client(&g_server.clients[i], &loop);
    }

    // Open server socket
    result = ptk_socket_open_tcp_server(&g_server.server_socket, NULL, MODBUS_TCP_PORT, &g_server);
    if(result != PTK_SUCCESS) {
        printf("Failed to open server socket: %d\n", result);
        return 1;
    }

    // Register server socket for accept events
    result = ptk_socket_register_events(&loop, &g_server.server_socket, &g_server.accept_source, NULL);
    if(result != PTK_SUCCESS) {
        printf("Failed to register server socket events: %d\n", result);
        return 1;
    }

    printf("Server listening on port %d\n", MODBUS_TCP_PORT);
    printf("Demonstrating multiple state machines:\n");
    printf("- 1 server state machine (accept connections, heartbeat)\n");
    printf("- Up to %d client state machines (per connection)\n", MAX_CLIENTS);
    printf("- Each client has 2 transition tables (connection + protocol states)\n");
    printf("- Server has 1 transition table (server states)\n");
    printf("\nPress Ctrl+C to stop\n\n");

    // Add server state machine to loop
    ptk_sm_add_to_loop(&loop, &g_server.server_state_machine);

    // Run the event loop
    ptk_loop_run(&loop);

    // Cleanup dynamically allocated resources
    for(int i = 0; i < MAX_CLIENTS; i++) {
        free(g_server.clients[i].connection_transitions);
        free(g_server.clients[i].protocol_transitions);
    }
    free(g_server.server_transitions);

    // Cleanup
    close(g_server.server_socket.socket_fd);
    printf("\nServer stopped\n");

    return 0;
}
