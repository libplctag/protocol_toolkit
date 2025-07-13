#include <ptk.h>
#include <ptk_sock.h>
#include <ptk_buf.h>
#include <ptk_alloc.h>
#include <ptk_shared.h>
#include <ptk_threadlet.h>
#include <ptk_log.h>
#include <ptk_config.h>
#include <ptk_err.h>

#include "arithmetic_protocol.h"

typedef struct {
    ptk_sock *client_sock;
    ptk_address_t client_addr;
} client_connection_t;

static void client_connection_destructor(void *ptr) {
    client_connection_t *conn = (client_connection_t*)ptr;
    if (conn && conn->client_sock) {
        debug("Cleaning up client connection");
        ptk_free(&conn->client_sock);
    }
}

static f64 perform_arithmetic(arithmetic_operation_t op, f32 operand1, f32 operand2) {
    switch (op) {
        case ARITH_OP_ADD:
            debug("Performing addition: %f + %f", operand1, operand2);
            return (f64)operand1 + (f64)operand2;
        case ARITH_OP_SUB:
            debug("Performing subtraction: %f - %f", operand1, operand2);
            return (f64)operand1 - (f64)operand2;
        case ARITH_OP_MUL:
            debug("Performing multiplication: %f * %f", operand1, operand2);
            return (f64)operand1 * (f64)operand2;
        case ARITH_OP_DIV:
            debug("Performing division: %f / %f", operand1, operand2);
            if (operand2 == 0.0f) {
                warn("Division by zero attempted");
                return 0.0;
            }
            return (f64)operand1 / (f64)operand2;
        default:
            error("Unknown arithmetic operation: %d", op);
            return 0.0;
    }
}

static void handle_client_connection(void *param) {
    ptk_shared_handle_t conn_handle = *(ptk_shared_handle_t*)param;
    
    info("Client handler threadlet started");
    
    use_shared(conn_handle, client_connection_t *conn) {
        char *client_ip = ptk_address_to_string(&conn->client_addr);
        info("Handling client connection from %s:%d", 
             client_ip ? client_ip : "unknown", 
             ptk_address_get_port(&conn->client_addr));
        if (client_ip) ptk_free(&client_ip);
        
        while (true) {
            ptk_buf *request_buf = ptk_tcp_socket_recv(conn->client_sock, false, 5000);
            if (!request_buf) {
                ptk_err err = ptk_get_err();
                if (err == PTK_ERR_TIMEOUT) {
                    debug("Client receive timeout, continuing...");
                    continue;
                } else if (err == PTK_ERR_CLOSED) {
                    info("Client connection closed");
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
                ptk_free(&request_buf);
                break;
            }
            
            ptk_err err = arithmetic_request_deserialize(request_buf, (ptk_serializable_t*)request);
            ptk_free(&request_buf);
            
            if (err != PTK_OK) {
                error("Failed to deserialize request: %d", err);
                ptk_free(&request);
                continue;
            }
            
            f64 result = perform_arithmetic((arithmetic_operation_t)request->operation, 
                                          request->operand1, request->operand2);
            
            arithmetic_response_t *response = arithmetic_response_create(
                (arithmetic_operation_t)request->operation, result);
            ptk_free(&request);
            
            if (!response) {
                error("Failed to create response object");
                break;
            }
            
            ptk_buf *response_buf = ptk_buf_alloc(64);
            if (!response_buf) {
                error("Failed to allocate response buffer");
                ptk_free(&response);
                break;
            }
            
            err = arithmetic_response_serialize(response_buf, (ptk_serializable_t*)response);
            ptk_free(&response);
            
            if (err != PTK_OK) {
                error("Failed to serialize response: %d", err);
                ptk_free(&response_buf);
                break;
            }
            
            debug("Sending %d bytes to client", ptk_buf_get_len(response_buf));
            debug_buf(response_buf);
            
            ptk_buf_array_t *buf_array = ptk_buf_array_create();
            if (!buf_array) {
                error("Failed to create buffer array");
                ptk_free(&response_buf);
                break;
            }
            
            ptk_buf_array_append(buf_array, response_buf);
            
            err = ptk_tcp_socket_send(conn->client_sock, buf_array, 5000);
            ptk_free(&buf_array);
            
            if (err != PTK_OK) {
                error("Failed to send response: %d", err);
                break;
            }
            
            info("Successfully processed arithmetic request");
        }
        
        info("Client handler threadlet exiting");
    } on_shared_fail {
        error("Failed to acquire client connection");
    }
    
    ptk_shared_release(conn_handle);
}

static void server_threadlet(void *param) {
    ptk_address_t *server_addr = (ptk_address_t*)param;
    
    info("Server threadlet started on port %d", ptk_address_get_port(server_addr));
    
    ptk_sock *server_sock = ptk_tcp_socket_listen(server_addr, 10);
    if (!server_sock) {
        error("Failed to create server socket: %d", ptk_get_err());
        return;
    }
    
    info("Server listening for connections");
    
    while (true) {
        ptk_sock *client_sock = ptk_tcp_socket_accept(server_sock, 0);
        if (!client_sock) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_ABORT) {
                info("Server accept aborted");
                break;
            } else {
                warn("Accept failed: %d", err);
                continue;
            }
        }
        
        client_connection_t *conn = ptk_alloc(sizeof(client_connection_t), client_connection_destructor);
        if (!conn) {
            error("Failed to allocate client connection");
            ptk_free(&client_sock);
            continue;
        }
        
        conn->client_sock = client_sock;
        
        ptk_shared_handle_t conn_handle = ptk_shared_wrap(conn);
        if (!PTK_SHARED_IS_VALID(conn_handle)) {
            error("Failed to wrap client connection in shared memory");
            ptk_free(&conn);
            continue;
        }
        
        ptk_shared_handle_t *handle_param = ptk_alloc(sizeof(ptk_shared_handle_t), NULL);
        if (!handle_param) {
            error("Failed to allocate handle parameter");
            ptk_shared_release(conn_handle);
            continue;
        }
        *handle_param = conn_handle;
        
        threadlet_t *client_threadlet = ptk_threadlet_create(handle_client_connection, handle_param);
        if (!client_threadlet) {
            error("Failed to create client threadlet");
            ptk_free(&handle_param);
            ptk_shared_release(conn_handle);
            continue;
        }
        
        ptk_err err = ptk_threadlet_resume(client_threadlet);
        if (err != PTK_OK) {
            error("Failed to start client threadlet: %d", err);
            ptk_free(&client_threadlet);
            ptk_free(&handle_param);
            ptk_shared_release(conn_handle);
            continue;
        }
        
        info("Created new client handler threadlet");
    }
    
    info("Server threadlet exiting");
    ptk_free(&server_sock);
}

int main(int argc, char *argv[]) {
    uint16_t port = 12345;
    bool help = false;
    
    ptk_config_field_t config_fields[] = {
        {"port", 'p', PTK_CONFIG_UINT16, &port, "Port to listen on", "12345"},
        {"help", 'h', PTK_CONFIG_HELP, &help, "Show this help message", NULL},
        PTK_CONFIG_END
    };
    
    ptk_err err = ptk_config_parse(argc, argv, config_fields, "arithmetic_server");
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
    
    ptk_address_t server_addr;
    err = ptk_address_init_any(&server_addr, port);
    if (err != PTK_OK) {
        error("Failed to initialize server address: %d", err);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    threadlet_t *server_threadlet = ptk_threadlet_create(server_threadlet, &server_addr);
    if (!server_threadlet) {
        error("Failed to create server threadlet");
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    err = ptk_threadlet_resume(server_threadlet);
    if (err != PTK_OK) {
        error("Failed to start server threadlet: %d", err);
        ptk_free(&server_threadlet);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    info("Server threadlet started, waiting for completion");
    
    err = ptk_threadlet_join(server_threadlet, 0);
    if (err != PTK_OK) {
        warn("Server threadlet join failed: %d", err);
    }
    
    info("Shutting down server");
    
    ptk_shared_shutdown();
    ptk_shutdown();
    
    return 0;
}