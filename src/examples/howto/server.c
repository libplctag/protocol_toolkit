#include <ptk.h>
#include <ptk_sock.h>
#include <ptk_buf.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_config.h>
#include <ptk_err.h>
#include <ptk_utils.h>

#include "arithmetic_protocol.h"

// Global flag for shutdown signal
static volatile bool shutdown_requested = false;
static ptk_thread_handle_t g_server_thread = PTK_SHARED_INVALID_HANDLE;

// PTK interrupt handler for graceful shutdown
static void interrupt_handler(void) {
    info("Received interrupt signal, shutting down...");
    shutdown_requested = true;
    
    // Signal the server thread to abort
    if (ptk_shared_is_valid(g_server_thread)) {
        ptk_thread_signal(g_server_thread, PTK_THREAD_SIGNAL_ABORT);
    }
}

typedef struct {
    ptk_sock *client_sock;
    ptk_address_t client_addr;
} client_connection_t;

static void client_connection_destructor(void *ptr) {
    client_connection_t *conn = (client_connection_t*)ptr;
    if (conn && conn->client_sock) {
        debug("Cleaning up client connection");
        ptk_local_free(&conn->client_sock);
    }
}

static ptk_f64_t perform_arithmetic(arithmetic_operation_t op, ptk_f32_t operand1, ptk_f32_t operand2) {
    switch (op) {
        case ARITH_OP_ADD:
            debug("Performing addition: %f + %f", operand1, operand2);
            return (ptk_f64_t)operand1 + (ptk_f64_t)operand2;
        case ARITH_OP_SUB:
            debug("Performing subtraction: %f - %f", operand1, operand2);
            return (ptk_f64_t)operand1 - (ptk_f64_t)operand2;
        case ARITH_OP_MUL:
            debug("Performing multiplication: %f * %f", operand1, operand2);
            return (ptk_f64_t)operand1 * (ptk_f64_t)operand2;
        case ARITH_OP_DIV:
            debug("Performing division: %f / %f", operand1, operand2);
            if (operand2 == 0.0f) {
                warn("Division by zero attempted");
                return 0.0;
            }
            return (ptk_f64_t)operand1 / (ptk_f64_t)operand2;
        default:
            error("Unknown arithmetic operation: %d", op);
            return 0.0;
    }
}

static void handle_client_connection(void) {
    size_t num_args = ptk_thread_get_arg_count();
    if (num_args != 1) {
        error("Client handler requires exactly 1 argument (connection handle), got %zu", num_args);
        return;
    }

    ptk_shared_handle_t conn_handle = ptk_thread_get_handle_arg(0);
    info("Client handler thread started");
    
    use_shared(conn_handle, client_connection_t*, conn, PTK_TIME_WAIT_FOREVER) {
        char *client_ip = ptk_address_to_string(&conn->client_addr);
        info("Handling client connection from %s:%d", 
             client_ip ? client_ip : "unknown", 
             ptk_address_get_port(&conn->client_addr));
        if (client_ip) ptk_local_free(&client_ip);
        
        while (!shutdown_requested) {
            // Check for abort signals
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
                info("Client handler received abort signal");
                break;
            }
            
            ptk_buf *request_buf = ptk_tcp_socket_recv(conn->client_sock, 1000); // 1 second timeout
            if (!request_buf) {
                ptk_err_t err = ptk_get_err();
                if (err == PTK_ERR_TIMEOUT) {
                    // Timeout is normal, check for signals and continue
                    continue;
                } else if (err == PTK_ERR_CLOSED) {
                    info("Client connection closed");
                    break;
                } else if (err == PTK_ERR_SIGNAL || err == PTK_ERR_ABORT) {
                    info("Client receive interrupted by signal");
                    break;
                } else {
                    warn("Error receiving from client: %d", err);
                    break;
                }
            }
            
            debug("Received %d bytes from client", ptk_buf_get_len(request_buf));
            debug_buf(request_buf);
            
            arithmetic_request_t *request = arithmetic_request_create(ARITH_OP_ADD, 0, 0);
            if (!request) {
                error("Failed to create request object");
                ptk_local_free(&request_buf);
                break;
            }
            
            ptk_err_t err = arithmetic_request_deserialize(request_buf, (ptk_serializable_t*)request);
            ptk_local_free(&request_buf);
            
            if (err != PTK_OK) {
                error("Failed to deserialize request: %d", err);
                ptk_local_free(&request);
                continue;
            }
            
            ptk_f64_t result = perform_arithmetic((arithmetic_operation_t)request->operation, 
                                          request->operand1, request->operand2);
            
            arithmetic_response_t *response = arithmetic_response_create(
                (arithmetic_operation_t)request->operation, result);
            ptk_local_free(&request);
            
            if (!response) {
                error("Failed to create response object");
                break;
            }
            
            ptk_buf *response_buf = ptk_buf_alloc(64);
            if (!response_buf) {
                error("Failed to allocate response buffer");
                ptk_local_free(&response);
                break;
            }
            
            err = arithmetic_response_serialize(response_buf, (ptk_serializable_t*)response);
            ptk_local_free(&response);
            
            if (err != PTK_OK) {
                error("Failed to serialize response: %d", err);
                ptk_local_free(&response_buf);
                break;
            }
            
            debug("Sending %d bytes to client", ptk_buf_get_len(response_buf));
            debug_buf(response_buf);
            
            err = ptk_tcp_socket_send(conn->client_sock, response_buf, 5000);
            
            if (err != PTK_OK) {
                error("Failed to send response: %d", err);
                break;
            }
            
            info("Successfully processed arithmetic request");
        }
        
        info("Client handler thread exiting");
    } on_shared_fail {
        error("Failed to acquire client connection");
    }
    // No manual release needed; use_shared handles it
}

static void server_thread_func(void) {
    size_t num_args = ptk_thread_get_arg_count();
    if (num_args != 1) {
        error("Server thread requires exactly 1 argument (server address handle), got %zu", num_args);
        return;
    }

    ptk_shared_handle_t addr_handle = ptk_thread_get_handle_arg(0);
    ptk_address_t *server_addr = NULL;
    
    use_shared(addr_handle, ptk_address_t*, addr, PTK_TIME_WAIT_FOREVER) {
        server_addr = ptk_address_create(ptk_address_to_string(addr), ptk_address_get_port(addr));
    } on_shared_fail {
        error("Failed to acquire server address");
        return;
    }
    
    if (!server_addr) {
        error("Failed to copy server address");
        return;
    }
    
    info("Server threadlet started on port %d", ptk_address_get_port(server_addr));
    
    ptk_sock *server_sock = ptk_tcp_server_create(server_addr);
    if (!server_sock) {
        error("Failed to create server socket: %d", ptk_get_err());
        return;
    }
    
    info("Server listening for connections");
    
    while (!shutdown_requested) {
        // Check for abort signals
        if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
            info("Server thread received abort signal");
            break;
        }
        
        ptk_address_t client_addr;
        ptk_sock *client_sock = ptk_tcp_accept(server_sock, &client_addr, 1000); // 1 second timeout
        if (!client_sock) {
            ptk_err_t err = ptk_get_err();
            if (err == PTK_ERR_TIMEOUT) {
                // Timeout is normal, check for signals and continue
                continue;
            } else if (err == PTK_ERR_SIGNAL || err == PTK_ERR_ABORT) {
                info("Server accept interrupted by signal");
                break;
            } else {
                warn("Accept failed: %d", err);
                continue;
            }
        }
        
        ptk_shared_handle_t conn_handle = ptk_shared_alloc(sizeof(client_connection_t), client_connection_destructor);
        if (!ptk_shared_is_valid(conn_handle)) {
            error("Failed to allocate shared client connection");
            ptk_local_free(&client_sock);
            continue;
        }
        use_shared(conn_handle, client_connection_t*, conn, PTK_TIME_NO_WAIT) {
            conn->client_sock = client_sock;
            conn->client_addr = client_addr;
        } on_shared_fail {
            error("Failed to acquire shared client connection for initialization");
            ptk_local_free(&client_sock);
            ptk_shared_free(&conn_handle);
            continue;
        }
        
        // Create a new thread for this client connection
        ptk_thread_handle_t client_thread = ptk_thread_create();
        if (!ptk_shared_is_valid(client_thread)) {
            error("Failed to create client thread");
            ptk_local_free(&client_sock);
            ptk_shared_free(&conn_handle);
            continue;
        }
        
        ptk_err_t err = ptk_thread_add_handle_arg(client_thread, 1, &conn_handle);
        if (err != PTK_OK) {
            error("Failed to add connection handle to client thread: %d", err);
            ptk_shared_release(client_thread);
            ptk_local_free(&client_sock);
            ptk_shared_free(&conn_handle);
            continue;
        }
        
        err = ptk_thread_set_run_function(client_thread, handle_client_connection);
        if (err != PTK_OK) {
            error("Failed to set client thread function: %d", err);
            ptk_shared_release(client_thread);
            ptk_shared_free(&conn_handle);
            continue;
        }
        
        err = ptk_thread_start(client_thread);
        if (err != PTK_OK) {
            error("Failed to start client thread: %d", err);
            ptk_shared_release(client_thread);
            ptk_shared_free(&conn_handle);
            continue;
        }
        
        info("Created new client handler thread");
        
        // Clean up any dead child threads periodically
        ptk_thread_cleanup_dead_children(ptk_thread_self(), PTK_TIME_NO_WAIT);
        
        // Clear any pending child death signals
        if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
            ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
        }
    }
    
    info("Server thread exiting");
    ptk_local_free(&server_sock);
    ptk_local_free(&server_addr);
    
    // Clean up all remaining child threads
    ptk_thread_handle_t self = ptk_thread_self();
    int child_count = ptk_thread_count_children(self);
    if (child_count > 0) {
        info("Signaling %d child threads to shutdown", child_count);
        ptk_thread_signal_all_children(self, PTK_THREAD_SIGNAL_ABORT);
        
        // Wait for children to clean up
        ptk_thread_cleanup_dead_children(self, 5000);
    }
}

int main(int argc, char *argv[]) {
    uint16_t port = 12345;
    bool help = false;
    
    ptk_config_field_t config_fields[] = {
        {"port", 'p', PTK_CONFIG_UINT16, &port, "Port to listen on", "12345"},
        {"help", 'h', PTK_CONFIG_HELP, &help, "Show this help message", NULL},
        PTK_CONFIG_END
    };
    
    ptk_err_t err = ptk_config_parse(argc, argv, config_fields, "arithmetic_server");
    if (err == 1) {
        return 0;
    } else if (err != PTK_OK) {
        return 1;
    }
    
    ptk_log_level_set(PTK_LOG_LEVEL_DEBUG);
    
    info("Starting Protocol Toolkit arithmetic server");
    
    err = ptk_startup();
    if (err != PTK_OK) {
        error("Failed to initialize PTK: %d", err);
        return 1;
    }
    
    err = ptk_shared_init();
    if (err != PTK_OK) {
        error("Failed to initialize shared memory: %d", err);
        ptk_shutdown();
        return 1;
    }
    
    ptk_address_t *server_addr = ptk_address_create_any(port);
    if (!server_addr) {
        error("Failed to create server address: %d", ptk_get_err());
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    // Create shared handle for server address
    ptk_shared_handle_t addr_handle = ptk_shared_alloc(sizeof(ptk_address_t), NULL);
    if (!ptk_shared_is_valid(addr_handle)) {
        error("Failed to create shared server address");
        ptk_local_free(&server_addr);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    // Copy address to shared memory
    use_shared(addr_handle, ptk_address_t*, shared_addr, PTK_TIME_NO_WAIT) {
        *shared_addr = *server_addr;
    } on_shared_fail {
        error("Failed to initialize shared server address");
        ptk_local_free(&server_addr);
        ptk_shared_free(&addr_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    ptk_local_free(&server_addr);
    
    // Set up PTK interrupt handler for graceful shutdown
    err = ptk_set_interrupt_handler(interrupt_handler);
    if (err != PTK_OK) {
        error("Failed to set interrupt handler: %d", err);
        ptk_shared_free(&addr_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    // Create and start server thread
    g_server_thread = ptk_thread_create();
    if (!ptk_shared_is_valid(g_server_thread)) {
        error("Failed to create server thread");
        ptk_shared_free(&addr_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    err = ptk_thread_add_handle_arg(g_server_thread, 1, &addr_handle);
    if (err != PTK_OK) {
        error("Failed to add address handle to server thread: %d", err);
        ptk_shared_release(g_server_thread);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    err = ptk_thread_set_run_function(g_server_thread, server_thread_func);
    if (err != PTK_OK) {
        error("Failed to set server thread function: %d", err);
        ptk_shared_release(g_server_thread);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    err = ptk_thread_start(g_server_thread);
    if (err != PTK_OK) {
        error("Failed to start server thread: %d", err);
        ptk_shared_release(g_server_thread);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    info("Server thread started, waiting for completion");
    
    // Wait for server thread and handle signals
    while (!shutdown_requested) {
        ptk_err_t wait_result = ptk_thread_wait(1000); // Check every second
        if (wait_result == PTK_ERR_SIGNAL) {
            uint64_t signals = ptk_thread_get_pending_signals();
            if (signals & PTK_THREAD_SIGNAL_ABORT_MASK) {
                info("Received shutdown signal, stopping server");
                ptk_thread_signal(g_server_thread, PTK_THREAD_SIGNAL_ABORT);
                break;
            }
            ptk_thread_clear_signals(signals);
        }
        
        // Clean up any dead child threads
        ptk_thread_cleanup_dead_children(ptk_thread_self(), PTK_TIME_NO_WAIT);
    }
    
    if (shutdown_requested) {
        info("Shutdown requested via signal, stopping server");
        ptk_thread_signal(g_server_thread, PTK_THREAD_SIGNAL_ABORT);
    }
    
    // Wait for server thread to finish
    ptk_thread_wait(5000);
    ptk_shared_release(g_server_thread);
    
    info("Shutting down server");
    
    ptk_shared_shutdown();
    ptk_shutdown();
    
    return 0;
}