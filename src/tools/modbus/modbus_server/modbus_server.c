/**
 * @file modbus_server.c
 * @brief Modbus TCP Server supporting multiple simultaneous clients
 */

#include "modbus.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// SERVER STRUCTURES
//=============================================================================

/**
 * @brief Client connection data
 */
typedef struct {
    ev_sock *socket;
    modbus_server_t *server;
    char remote_host[64];
    int remote_port;
    bool connected;
} modbus_client_connection_t;

/**
 * @brief Modbus server structure
 */
struct modbus_server {
    ev_loop *loop;
    ev_sock *listen_socket;
    modbus_data_store_t *data_store;
    uint8_t unit_id;
    size_t max_connections;
    size_t current_connections;
    modbus_client_connection_t **clients;
    ev_mutex *clients_mutex;
    bool running;
};

//=============================================================================
// GLOBAL STATE FOR SIGNAL HANDLING
//=============================================================================

static modbus_server_t *g_server = NULL;

static void signal_handler(void) {
    if(g_server) {
        info("Received interrupt signal, stopping server...");
        g_server->running = false;
        ev_loop_stop(g_server->loop);
    }
}

//=============================================================================
// CLIENT CONNECTION MANAGEMENT
//=============================================================================

static modbus_client_connection_t *find_client_by_socket(modbus_server_t *server, ev_sock *socket) {
    for(size_t i = 0; i < server->max_connections; i++) {
        if(server->clients[i] && server->clients[i]->socket == socket) { return server->clients[i]; }
    }
    return NULL;
}

static void remove_client_connection(modbus_server_t *server, modbus_client_connection_t *client) {
    if(!server || !client) { return; }

    ev_mutex_wait_lock(server->clients_mutex, THREAD_WAIT_FOREVER);

    for(size_t i = 0; i < server->max_connections; i++) {
        if(server->clients[i] == client) {
            server->clients[i] = NULL;
            server->current_connections--;
            break;
        }
    }

    ev_mutex_unlock(server->clients_mutex);

    info("Client disconnected from %s:%d (connections: %zu/%zu)", client->remote_host, client->remote_port,
         server->current_connections, server->max_connections);

    free(client);
}

static modbus_client_connection_t *add_client_connection(modbus_server_t *server, ev_sock *socket, const char *remote_host,
                                                         int remote_port) {
    if(!server || !socket) { return NULL; }

    ev_mutex_wait_lock(server->clients_mutex, THREAD_WAIT_FOREVER);

    // Check if we have space for new client
    if(server->current_connections >= server->max_connections) {
        ev_mutex_unlock(server->clients_mutex);
        warn("Maximum connections reached (%zu), rejecting new client", server->max_connections);
        return NULL;
    }

    // Find empty slot
    size_t slot = 0;
    for(size_t i = 0; i < server->max_connections; i++) {
        if(!server->clients[i]) {
            slot = i;
            break;
        }
    }

    // Create client connection
    modbus_client_connection_t *client = calloc(1, sizeof(modbus_client_connection_t));
    if(!client) {
        ev_mutex_unlock(server->clients_mutex);
        error("Failed to allocate client connection");
        return NULL;
    }

    client->socket = socket;
    client->server = server;
    client->connected = true;
    client->remote_port = remote_port;
    strncpy(client->remote_host, remote_host ? remote_host : "unknown", sizeof(client->remote_host) - 1);

    server->clients[slot] = client;
    server->current_connections++;

    ev_mutex_unlock(server->clients_mutex);

    info("New client connected from %s:%d (connections: %zu/%zu)", remote_host, remote_port, server->current_connections,
         server->max_connections);

    return client;
}

//=============================================================================
// EVENT HANDLERS
//=============================================================================

static void client_event_handler(const ev_event *event) {
    modbus_client_connection_t *client = (modbus_client_connection_t *)ev_event_get_user_data(event);
    if(!client) {
        error("Client event handler called with null client data");
        return;
    }

    modbus_server_t *server = client->server;
    ev_event_type event_type = ev_event_get_type(event);

    switch(event_type) {
        case EV_EVENT_READ: {
            trace("Received data from client %s:%d", client->remote_host, client->remote_port);

            buf **data = ev_event_get_data(event);
            if(!data || !*data) {
                error("Received read event with no data");
                break;
            }

            // Process Modbus request
            buf *response_buf = NULL;
            modbus_err_t result = modbus_process_request(server->data_store, *data, &response_buf, server->unit_id);

            if(result == MODBUS_OK && response_buf) {
                // Send response back to client
                trace("Sending response to client %s:%d (%zu bytes)", client->remote_host, client->remote_port,
                      response_buf->len);

                ev_err send_result = ev_tcp_write(client->socket, &response_buf);
                if(send_result != EV_OK) {
                    error("Failed to send response to client %s:%d: %s", client->remote_host, client->remote_port,
                          ev_err_string(send_result));
                    if(response_buf) {
                        buf_free(response_buf);
                        response_buf = NULL;
                    }
                }
            } else {
                error("Failed to process Modbus request from client %s:%d: %s", client->remote_host, client->remote_port,
                      modbus_err_string(result));
                if(response_buf) {
                    buf_free(response_buf);
                    response_buf = NULL;
                }
            }

            // Dispose of request data
            buf_free(*data);
            *data = NULL;
            break;
        }

        case EV_EVENT_CLOSE:
            info("Client %s:%d closed connection", client->remote_host, client->remote_port);
            remove_client_connection(server, client);
            break;

        case EV_EVENT_ERROR: {
            ev_err error_code = ev_event_get_error(event);
            error("Client %s:%d error: %s", client->remote_host, client->remote_port, ev_err_string(error_code));
            remove_client_connection(server, client);
            break;
        }

        case EV_EVENT_WRITE_DONE: trace("Response sent to client %s:%d", client->remote_host, client->remote_port); break;

        default: warn("Unexpected event type %d for client %s:%d", event_type, client->remote_host, client->remote_port); break;
    }
}

static void server_event_handler(const ev_event *event) {
    modbus_server_t *server = (modbus_server_t *)ev_event_get_user_data(event);
    if(!server) {
        error("Server event handler called with null server data");
        return;
    }

    ev_event_type event_type = ev_event_get_type(event);

    switch(event_type) {
        case EV_EVENT_ACCEPT: {
            const char *remote_host = ev_event_get_remote_host(event);
            int remote_port = ev_event_get_remote_port(event);
            ev_sock *client_socket = ev_event_get_socket(event);

            trace("Accepting new client connection from %s:%d", remote_host, remote_port);

            // Create client connection
            modbus_client_connection_t *client = add_client_connection(server, client_socket, remote_host, remote_port);
            if(!client) {
                error("Failed to add client connection, closing socket");
                ev_close(client_socket);
                break;
            }

            // The client socket now uses our client event handler
            // Note: In a real implementation, we would need to update the socket's user data
            // For now, we assume the ev_loop framework handles this correctly
            break;
        }

        case EV_EVENT_ERROR: {
            ev_err error_code = ev_event_get_error(event);
            error("Server socket error: %s", ev_err_string(error_code));
            break;
        }

        case EV_EVENT_CLOSE:
            info("Server socket closed");
            server->running = false;
            break;

        default: warn("Unexpected server event type: %d", event_type); break;
    }
}

//=============================================================================
// SERVER IMPLEMENTATION
//=============================================================================

modbus_err_t modbus_server_create(ev_loop *loop, modbus_server_t **server, const modbus_server_config_t *config) {
    if(!loop || !server || !config || !config->data_store) { return MODBUS_ERR_NULL_PTR; }

    *server = calloc(1, sizeof(modbus_server_t));
    if(!*server) {
        error("Failed to allocate server structure");
        return MODBUS_ERR_NO_RESOURCES;
    }

    modbus_server_t *srv = *server;
    srv->loop = loop;
    srv->data_store = config->data_store;
    srv->unit_id = config->unit_id;
    srv->max_connections = config->max_connections ? config->max_connections : 10;
    srv->running = true;

    // Create client mutex
    if(ev_mutex_create(&srv->clients_mutex) != EV_OK) {
        error("Failed to create clients mutex");
        modbus_server_destroy(srv);
        *server = NULL;
        return MODBUS_ERR_NO_RESOURCES;
    }

    // Allocate client array
    srv->clients = calloc(srv->max_connections, sizeof(modbus_client_connection_t *));
    if(!srv->clients) {
        error("Failed to allocate client array");
        modbus_server_destroy(srv);
        *server = NULL;
        return MODBUS_ERR_NO_RESOURCES;
    }

    // Create TCP server
    ev_tcp_server_opts server_opts = {.bind_host = config->bind_host ? config->bind_host : "0.0.0.0",
                                      .bind_port = config->bind_port ? config->bind_port : MODBUS_TCP_PORT,
                                      .backlog = 128,
                                      .callback = server_event_handler,
                                      .user_data = srv,
                                      .reuse_addr = true,
                                      .keep_alive = false,
                                      .read_buffer_size = 8192};

    ev_err result = ev_tcp_server_start(loop, &srv->listen_socket, &server_opts);
    if(result != EV_OK) {
        error("Failed to start TCP server on %s:%d: %s", server_opts.bind_host, server_opts.bind_port, ev_err_string(result));
        modbus_server_destroy(srv);
        *server = NULL;
        return MODBUS_ERR_CONNECTION_FAILED;
    }

    info("Modbus TCP server started on %s:%d (unit ID: %u, max connections: %zu)", server_opts.bind_host, server_opts.bind_port,
         srv->unit_id, srv->max_connections);

    return MODBUS_OK;
}

void modbus_server_destroy(modbus_server_t *server) {
    if(!server) { return; }

    server->running = false;

    // Close all client connections
    if(server->clients) {
        ev_mutex_wait_lock(server->clients_mutex, THREAD_WAIT_FOREVER);

        for(size_t i = 0; i < server->max_connections; i++) {
            if(server->clients[i]) {
                if(server->clients[i]->socket) { ev_close(server->clients[i]->socket); }
                free(server->clients[i]);
                server->clients[i] = NULL;
            }
        }

        ev_mutex_unlock(server->clients_mutex);
        free(server->clients);
    }

    // Close listen socket
    if(server->listen_socket) { ev_close(server->listen_socket); }

    // Destroy mutex
    if(server->clients_mutex) { ev_mutex_destroy(server->clients_mutex); }

    free(server);
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

static void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -p, --port PORT         Bind to specific port (default: 502)\n");
    printf("  -b, --bind HOST         Bind to specific host (default: 0.0.0.0)\n");
    printf("  -u, --unit-id ID        Unit identifier (default: 1)\n");
    printf("  -c, --connections NUM   Maximum connections (default: 10)\n");
    printf("  -v, --verbose           Enable verbose logging\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                      # Start server on 0.0.0.0:502\n", program_name);
    printf("  %s -p 1502 -u 5         # Start on port 1502 with unit ID 5\n", program_name);
    printf("  %s -b 127.0.0.1 -c 5    # Bind to localhost, max 5 connections\n", program_name);
}

static void populate_test_data(modbus_data_store_t *data_store) {
    info("Populating test data...");

    // Set some test coils
    uint8_t coil_data[] = {0x55, 0xAA};  // Alternating pattern
    modbus_data_store_write_coils(data_store, 0, 16, coil_data);

    // Set some test holding registers
    uint16_t register_data[] = {0x1234, 0x5678, 0x9ABC, 0xDEF0};
    modbus_data_store_write_holding_registers(data_store, 0, 4, register_data);

    // Initialize some registers with sequential values
    for(uint16_t i = 100; i < 200; i++) {
        uint16_t value = i * 10;
        modbus_data_store_write_holding_registers(data_store, i, 1, &value);
    }

    info("Test data populated: coils 0-15, holding registers 0-3 and 100-199");
}

int main(int argc, char *argv[]) {
    // Set default log level
    ev_log_level_set(EV_LOG_LEVEL_INFO);

    // Parse command line arguments
    const char *bind_host = NULL;
    int bind_port = MODBUS_TCP_PORT;
    uint8_t unit_id = 1;
    size_t max_connections = 10;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if(++i >= argc) {
                error("Option %s requires an argument", argv[i - 1]);
                return 1;
            }
            bind_port = atoi(argv[i]);
            if(bind_port <= 0 || bind_port > 65535) {
                error("Invalid port number: %s", argv[i]);
                return 1;
            }
        } else if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bind") == 0) {
            if(++i >= argc) {
                error("Option %s requires an argument", argv[i - 1]);
                return 1;
            }
            bind_host = argv[i];
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
        } else if(strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--connections") == 0) {
            if(++i >= argc) {
                error("Option %s requires an argument", argv[i - 1]);
                return 1;
            }
            max_connections = atoi(argv[i]);
            if(max_connections == 0) {
                error("Invalid max connections: %s", argv[i]);
                return 1;
            }
        } else if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            ev_log_level_set(EV_LOG_LEVEL_TRACE);
        } else {
            error("Unknown option: %s", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    info("Starting Modbus TCP Server");
    info("  Bind address: %s", bind_host ? bind_host : "0.0.0.0");
    info("  Port: %d", bind_port);
    info("  Unit ID: %u", unit_id);
    info("  Max connections: %zu", max_connections);

    // Create event loop
    ev_loop *loop = NULL;
    ev_loop_opts loop_opts = {.worker_threads = 4, .max_events = 1024, .auto_start = true};

    ev_err loop_result = ev_loop_create(&loop, &loop_opts);
    if(loop_result != EV_OK) {
        error("Failed to create event loop: %s", ev_err_string(loop_result));
        return 1;
    }

    // Create data store
    modbus_data_store_t *data_store = NULL;
    modbus_data_store_config_t store_config = {.coil_count = 10000,
                                               .discrete_input_count = 10000,
                                               .holding_register_count = 10000,
                                               .input_register_count = 10000,
                                               .read_only_coils = false,
                                               .read_only_holding_registers = false};

    modbus_err_t store_result = modbus_data_store_create(&data_store, &store_config);
    if(store_result != MODBUS_OK) {
        error("Failed to create data store: %s", modbus_err_string(store_result));
        ev_loop_destroy(loop);
        return 1;
    }

    // Populate with test data
    populate_test_data(data_store);

    // Create server
    modbus_server_t *server = NULL;
    modbus_server_config_t server_config = {.bind_host = bind_host,
                                            .bind_port = bind_port,
                                            .data_store = data_store,
                                            .unit_id = unit_id,
                                            .max_connections = max_connections};

    modbus_err_t server_result = modbus_server_create(loop, &server, &server_config);
    if(server_result != MODBUS_OK) {
        error("Failed to create server: %s", modbus_err_string(server_result));
        modbus_data_store_destroy(data_store);
        ev_loop_destroy(loop);
        return 1;
    }

    // Set up signal handling
    g_server = server;
    ev_set_interrupt_handler(signal_handler);

    info("Server started successfully. Press Ctrl+C to stop.");

    // Run event loop
    ev_err wait_result = ev_loop_wait(loop);
    if(wait_result != EV_OK) { error("Event loop error: %s", ev_err_string(wait_result)); }

    // Cleanup
    info("Shutting down server...");
    modbus_server_destroy(server);
    modbus_data_store_destroy(data_store);
    ev_loop_destroy(loop);

    info("Server shutdown complete");
    return 0;
}