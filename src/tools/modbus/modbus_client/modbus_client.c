/**
 * @file modbus_client.c
 * @brief Modbus TCP Client test program
 */

#include "modbus.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

//=============================================================================
// CLIENT STRUCTURES
//=============================================================================

/**
 * @brief Modbus client structure
 */
struct modbus_client {
    ptk_loop *loop;
    ptk_sock *socket;
    modbus_client_config_t config;
    uint16_t transaction_id;
    bool connected;
    bool request_pending;
    buf *pending_response;
    ptk_mutex *response_mutex;
    ptk_cond_var *response_cond;
    modbus_err_t last_error;
};

//=============================================================================
// GLOBAL STATE
//=============================================================================

static bool g_running = true;

static void signal_handler(void) {
    info("Received interrupt signal, stopping client...");
    g_running = false;
}

//=============================================================================
// CLIENT EVENT HANDLERS
//=============================================================================

static void client_event_handler(const ptk_event *event) {
    modbus_client_t *client = (modbus_client_t *)ptk_event_get_user_data(event);
    if(!client) {
        error("Client event handler called with null client data");
        return;
    }

    ptk_event_type event_type = ptk_event_get_type(event);

    switch(event_type) {
        case PTK_EVENT_CONNECT:
            info("Connected to Modbus server");
            client->connected = true;
            break;

        case PTK_EVENT_READ: {
            trace("Received response from server");

            buf **data = ptk_event_get_data(event);
            if(!data || !*data) {
                error("Received read event with no data");
                break;
            }

            // Store response for synchronous processing
            ptk_mutex_wait_lock(client->response_mutex, THREAD_WAIT_FOREVER);

            if(client->pending_response) { buf_dispose(&client->pending_response); }
            client->pending_response = *data;
            *data = NULL;  // Take ownership
            client->request_pending = false;

            ptk_cond_var_signal(client->response_cond);
            ptk_mutex_unlock(client->response_mutex);
            break;
        }

        case PTK_EVENT_WRITE_DONE: trace("Request sent to server"); break;

        case PTK_EVENT_CLOSE:
            info("Disconnected from server");
            client->connected = false;
            client->request_pending = false;

            ptk_mutex_wait_lock(client->response_mutex, THREAD_WAIT_FOREVER);
            ptk_cond_var_signal(client->response_cond);
            ptk_mutex_unlock(client->response_mutex);
            break;

        case PTK_EVENT_ERROR: {
            ptk_err error_code = ptk_event_get_error(event);
            error("Client error: %s", ptk_err_string(error_code));
            client->connected = false;
            client->request_pending = false;
            client->last_error = MODBUS_ERR_CONNECTION_FAILED;

            ptk_mutex_wait_lock(client->response_mutex, THREAD_WAIT_FOREVER);
            ptk_cond_var_signal(client->response_cond);
            ptk_mutex_unlock(client->response_mutex);
            break;
        }

        default: warn("Unexpected client event type: %d", event_type); break;
    }
}

//=============================================================================
// CLIENT IMPLEMENTATION
//=============================================================================

modbus_err_t modbus_client_create(ptk_loop *loop, modbus_client_t **client, const modbus_client_config_t *config) {
    if(!loop || !client || !config) { return MODBUS_ERR_NULL_PTR; }

    *client = calloc(1, sizeof(modbus_client_t));
    if(!*client) {
        error("Failed to allocate client structure");
        return MODBUS_ERR_NO_RESOURCES;
    }

    modbus_client_t *c = *client;
    c->loop = loop;
    c->config = *config;
    c->transaction_id = 1;

    // Create synchronization primitives
    if(ptk_mutex_create(&c->response_mutex) != PTK_OK || ptk_cond_var_create(&c->response_cond) != PTK_OK) {
        error("Failed to create synchronization primitives");
        modbus_client_destroy(c);
        *client = NULL;
        return MODBUS_ERR_NO_RESOURCES;
    }

    // Connect to server
    ptk_tcp_client_opts tcp_opts = {.host = config->host,
                                    .port = config->port ? config->port : MODBUS_TCP_PORT,
                                    .callback = client_event_handler,
                                    .user_data = c,
                                    .connect_timeout_ms = config->timeout_ms ? config->timeout_ms : 5000,
                                    .keep_alive = true,
                                    .read_buffer_size = 8192};

    ptk_err result = ptk_tcp_connect(loop, &c->socket, &tcp_opts);
    if(result != PTK_OK) {
        error("Failed to connect to %s:%d: %s", config->host, tcp_opts.port, ptk_err_string(result));
        modbus_client_destroy(c);
        *client = NULL;
        return MODBUS_ERR_CONNECTION_FAILED;
    }

    return MODBUS_OK;
}

void modbus_client_destroy(modbus_client_t *client) {
    if(!client) { return; }

    if(client->socket) { ptk_close(client->socket); }

    if(client->pending_response) { buf_dispose(&client->pending_response); }

    if(client->response_mutex) { ptk_mutex_destroy(client->response_mutex); }

    if(client->response_cond) { ptk_cond_var_destroy(client->response_cond); }

    free(client);
}

static modbus_err_t send_request_and_wait_response(modbus_client_t *client, buf *request_buf, buf **response_buf) {
    if(!client || !request_buf || !response_buf) { return MODBUS_ERR_NULL_PTR; }

    *response_buf = NULL;

    if(!client->connected) { return MODBUS_ERR_CONNECTION_FAILED; }

    ptk_mutex_wait_lock(client->response_mutex, THREAD_WAIT_FOREVER);

    // Clear any pending response
    if(client->pending_response) {
        buf_dispose(&client->pending_response);
        client->pending_response = NULL;
    }

    client->request_pending = true;
    client->last_error = MODBUS_OK;

    ptk_mutex_unlock(client->response_mutex);

    // Send request
    ptk_err send_result = ptk_tcp_write(client->socket, &request_buf);
    if(send_result != PTK_OK) {
        error("Failed to send request: %s", ptk_err_string(send_result));
        client->request_pending = false;
        return MODBUS_ERR_CONNECTION_FAILED;
    }

    // Wait for response
    ptk_mutex_wait_lock(client->response_mutex, THREAD_WAIT_FOREVER);

    while(client->request_pending && client->connected) {
        ptk_err wait_result = ptk_cond_var_wait(client->response_cond, client->response_mutex,
                                                client->config.timeout_ms ? client->config.timeout_ms : 5000);
        if(wait_result != PTK_OK) {
            error("Timeout waiting for response");
            client->request_pending = false;
            ptk_mutex_unlock(client->response_mutex);
            return MODBUS_ERR_TIMEOUT;
        }
    }

    if(!client->connected) {
        ptk_mutex_unlock(client->response_mutex);
        return MODBUS_ERR_CONNECTION_FAILED;
    }

    if(client->last_error != MODBUS_OK) {
        ptk_mutex_unlock(client->response_mutex);
        return client->last_error;
    }

    *response_buf = client->pending_response;
    client->pending_response = NULL;

    ptk_mutex_unlock(client->response_mutex);

    return MODBUS_OK;
}

modbus_err_t modbus_client_read_holding_registers(modbus_client_t *client, uint16_t address, uint16_t count, uint16_t *values) {
    if(!client || !values) { return MODBUS_ERR_NULL_PTR; }

    if(count == 0 || count > MODBUS_MAX_REGISTERS) { return MODBUS_ERR_INVALID_PARAM; }

    // Create MBAP header
    modbus_mbap_header_t mbap_header;
    buf_u16_be_set(&mbap_header.transaction_id, client->transaction_id++);
    buf_u16_be_set(&mbap_header.protocol_id, 0);
    buf_u16_be_set(&mbap_header.length, 6);  // 1 (unit) + 1 (func) + 2 (addr) + 2 (count)
    buf_u8_set(&mbap_header.unit_id, client->config.unit_id);

    // Create request
    modbus_read_holding_registers_req_t req;
    buf_u8_set(&req.function_code, MODBUS_FC_READ_HOLDING_REGISTERS);
    buf_u16_be_set(&req.starting_address, address);
    buf_u16_be_set(&req.quantity_of_registers, count);

    // Encode request
    buf *request_buf = NULL;
    buf_err_t buf_result = buf_create(&request_buf, 12);
    if(buf_result != BUF_OK) { return MODBUS_ERR_NO_RESOURCES; }

    modbus_err_t result = modbus_mbap_header_encode(request_buf, &mbap_header);
    if(result != MODBUS_OK) {
        buf_dispose(&request_buf);
        return result;
    }

    result = modbus_read_holding_registers_req_encode(request_buf, &req);
    if(result != MODBUS_OK) {
        buf_dispose(&request_buf);
        return result;
    }

    // Send request and wait for response
    buf *response_buf = NULL;
    result = send_request_and_wait_response(client, request_buf, &response_buf);
    if(result != MODBUS_OK) { return result; }

    // Decode response
    modbus_mbap_header_t response_header;
    result = modbus_mbap_header_decode(&response_header, response_buf);
    if(result != MODBUS_OK) {
        buf_dispose(&response_buf);
        return result;
    }

    // Read function code
    uint8_t function_code;
    if(buf_read_u8(response_buf, &function_code) != BUF_OK) {
        buf_dispose(&response_buf);
        return MODBUS_ERR_PARSE_ERROR;
    }

    // Check for exception response
    if(function_code & 0x80) {
        uint8_t exception_code;
        if(buf_read_u8(response_buf, &exception_code) != BUF_OK) {
            buf_dispose(&response_buf);
            return MODBUS_ERR_PARSE_ERROR;
        }

        warn("Modbus exception response: function 0x%02X, exception 0x%02X", function_code & 0x7F, exception_code);

        buf_dispose(&response_buf);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }

    // Read byte count
    uint8_t byte_count;
    if(buf_read_u8(response_buf, &byte_count) != BUF_OK) {
        buf_dispose(&response_buf);
        return MODBUS_ERR_PARSE_ERROR;
    }

    if(byte_count != count * 2) {
        error("Invalid byte count in response: expected %u, got %u", count * 2, byte_count);
        buf_dispose(&response_buf);
        return MODBUS_ERR_PARSE_ERROR;
    }

    // Read register values
    for(uint16_t i = 0; i < count; i++) {
        if(buf_read_u16_be(response_buf, &values[i]) != BUF_OK) {
            buf_dispose(&response_buf);
            return MODBUS_ERR_PARSE_ERROR;
        }
    }

    buf_dispose(&response_buf);
    return MODBUS_OK;
}

modbus_err_t modbus_client_write_single_register(modbus_client_t *client, uint16_t address, uint16_t value) {
    if(!client) { return MODBUS_ERR_NULL_PTR; }

    // Create MBAP header
    modbus_mbap_header_t mbap_header;
    buf_u16_be_set(&mbap_header.transaction_id, client->transaction_id++);
    buf_u16_be_set(&mbap_header.protocol_id, 0);
    buf_u16_be_set(&mbap_header.length, 6);  // 1 (unit) + 1 (func) + 2 (addr) + 2 (value)
    buf_u8_set(&mbap_header.unit_id, client->config.unit_id);

    // Create request
    modbus_write_single_register_req_t req;
    buf_u8_set(&req.function_code, MODBUS_FC_WRITE_SINGLE_REGISTER);
    buf_u16_be_set(&req.register_address, address);
    buf_u16_be_set(&req.register_value, value);

    // Encode request
    buf *request_buf = NULL;
    buf_err_t buf_result = buf_create(&request_buf, 12);
    if(buf_result != BUF_OK) { return MODBUS_ERR_NO_RESOURCES; }

    modbus_err_t result = modbus_mbap_header_encode(request_buf, &mbap_header);
    if(result != MODBUS_OK) {
        buf_dispose(&request_buf);
        return result;
    }

    // Manually encode write single register request
    buf_write_u8(request_buf, MODBUS_FC_WRITE_SINGLE_REGISTER);
    buf_write_u16_be(request_buf, address);
    buf_write_u16_be(request_buf, value);

    // Send request and wait for response
    buf *response_buf = NULL;
    result = send_request_and_wait_response(client, request_buf, &response_buf);
    if(result != MODBUS_OK) { return result; }

    // Decode response header
    modbus_mbap_header_t response_header;
    result = modbus_mbap_header_decode(&response_header, response_buf);
    if(result != MODBUS_OK) {
        buf_dispose(&response_buf);
        return result;
    }

    // Read function code
    uint8_t function_code;
    if(buf_read_u8(response_buf, &function_code) != BUF_OK) {
        buf_dispose(&response_buf);
        return MODBUS_ERR_PARSE_ERROR;
    }

    // Check for exception response
    if(function_code & 0x80) {
        uint8_t exception_code;
        if(buf_read_u8(response_buf, &exception_code) != BUF_OK) {
            buf_dispose(&response_buf);
            return MODBUS_ERR_PARSE_ERROR;
        }

        warn("Modbus exception response: function 0x%02X, exception 0x%02X", function_code & 0x7F, exception_code);

        buf_dispose(&response_buf);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }

    // Verify echo of request
    uint16_t echo_address, echo_value;
    if(buf_read_u16_be(response_buf, &echo_address) != BUF_OK || buf_read_u16_be(response_buf, &echo_value) != BUF_OK) {
        buf_dispose(&response_buf);
        return MODBUS_ERR_PARSE_ERROR;
    }

    if(echo_address != address || echo_value != value) {
        error("Write single register response mismatch");
        buf_dispose(&response_buf);
        return MODBUS_ERR_PARSE_ERROR;
    }

    buf_dispose(&response_buf);
    return MODBUS_OK;
}

//=============================================================================
// TEST FUNCTIONS
//=============================================================================

static void test_read_holding_registers(modbus_client_t *client) {
    info("Testing read holding registers...");

    uint16_t values[10];
    modbus_err_t result = modbus_client_read_holding_registers(client, 0, 4, values);

    if(result == MODBUS_OK) {
        info("Read holding registers 0-3:");
        for(int i = 0; i < 4; i++) { info("  Register %d: %u (0x%04X)", i, values[i], values[i]); }
    } else {
        error("Failed to read holding registers: %s", modbus_err_string(result));
    }
}

static void test_write_single_register(modbus_client_t *client) {
    info("Testing write single register...");

    uint16_t test_value = 42;
    modbus_err_t result = modbus_client_write_single_register(client, 0, test_value);

    if(result == MODBUS_OK) {
        info("Successfully wrote value %u to register 0", test_value);

        // Read back to verify
        uint16_t read_value;
        result = modbus_client_read_holding_registers(client, 0, 1, &read_value);
        if(result == MODBUS_OK) {
            if(read_value == test_value) {
                info("Verified: register 0 contains %u", read_value);
            } else {
                error("Verification failed: expected %u, got %u", test_value, read_value);
            }
        } else {
            error("Failed to read back register: %s", modbus_err_string(result));
        }
    } else {
        error("Failed to write single register: %s", modbus_err_string(result));
    }
}

static void test_sequential_registers(modbus_client_t *client) {
    info("Testing sequential registers 100-109...");

    uint16_t values[10];
    modbus_err_t result = modbus_client_read_holding_registers(client, 100, 10, values);

    if(result == MODBUS_OK) {
        info("Read holding registers 100-109:");
        for(int i = 0; i < 10; i++) { info("  Register %d: %u", 100 + i, values[i]); }
    } else {
        error("Failed to read sequential registers: %s", modbus_err_string(result));
    }
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

static void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -H, --host HOST         Server host (default: 127.0.0.1)\n");
    printf("  -p, --port PORT         Server port (default: 502)\n");
    printf("  -u, --unit-id ID        Unit identifier (default: 1)\n");
    printf("  -t, --timeout MS        Request timeout in milliseconds (default: 5000)\n");
    printf("  -v, --verbose           Enable verbose logging\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                      # Connect to localhost:502\n", program_name);
    printf("  %s -H 192.168.1.100     # Connect to specific host\n", program_name);
    printf("  %s -p 1502 -u 5         # Connect to port 1502 with unit ID 5\n", program_name);
}

int main(int argc, char *argv[]) {
    // Set default log level
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);

    // Parse command line arguments
    const char *host = "127.0.0.1";
    int port = MODBUS_TCP_PORT;
    uint8_t unit_id = 1;
    uint32_t timeout_ms = 5000;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if(strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--host") == 0) {
            if(++i >= argc) {
                error("Option %s requires an argument", argv[i - 1]);
                return 1;
            }
            host = argv[i];
        } else if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if(++i >= argc) {
                error("Option %s requires an argument", argv[i - 1]);
                return 1;
            }
            port = atoi(argv[i]);
            if(port <= 0 || port > 65535) {
                error("Invalid port number: %s", argv[i]);
                return 1;
            }
        } else if(strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unit-id") == 0) {
            if(++i >= argc) {
                error("Option %s requires an argument", argv[i - 1]);
                return 1;
            }
            unit_id = atoi(argv[i]);
            if(unit_id == 0) {
                error("Invalid unit ID: %s", argv[i]);
                return 1;
            }
        } else if(strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if(++i >= argc) {
                error("Option %s requires an argument", argv[i - 1]);
                return 1;
            }
            timeout_ms = atoi(argv[i]);
            if(timeout_ms == 0) {
                error("Invalid timeout: %s", argv[i]);
                return 1;
            }
        } else if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            ptk_log_level_set(PTK_LOG_LEVEL_TRACE);
        } else {
            error("Unknown option: %s", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    info("Starting Modbus TCP Client");
    info("  Server: %s:%d", host, port);
    info("  Unit ID: %u", unit_id);
    info("  Timeout: %u ms", timeout_ms);

    // Create event loop
    ptk_loop *loop = NULL;
    ptk_loop_opts loop_opts = {.worker_threads = 2, .max_events = 64, .auto_start = true};

    ptk_err loop_result = ptk_loop_create(&loop, &loop_opts);
    if(loop_result != PTK_OK) {
        error("Failed to create event loop: %s", ptk_err_string(loop_result));
        return 1;
    }

    // Create client
    modbus_client_t *client = NULL;
    modbus_client_config_t client_config = {.host = host, .port = port, .unit_id = unit_id, .timeout_ms = timeout_ms};

    modbus_err_t client_result = modbus_client_create(loop, &client, &client_config);
    if(client_result != MODBUS_OK) {
        error("Failed to create client: %s", modbus_err_string(client_result));
        ptk_loop_destroy(loop);
        return 1;
    }

    // Set up signal handling
    ptk_set_interrupt_handler(signal_handler);

    info("Client created. Waiting for connection...");

// Wait a bit for connection to establish
#ifdef _WIN32
    Sleep(1000);
#else
    usleep(1000000);
#endif

    // Run tests
    if(client->connected) {
        info("Connected! Running tests...");

        test_read_holding_registers(client);
        test_write_single_register(client);
        test_sequential_registers(client);

        info("All tests completed successfully!");
    } else {
        error("Failed to connect to server");
    }

    // Cleanup
    info("Shutting down client...");
    modbus_client_destroy(client);
    ptk_loop_destroy(loop);

    info("Client shutdown complete");
    return 0;
}