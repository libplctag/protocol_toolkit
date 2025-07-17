#include <ptk.h>
#include <ptk_sock.h>
#include <ptk_buf.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_config.h>
#include <ptk_err.h>

#include "arithmetic_protocol.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *server_ip;
    uint16_t server_port;
    arithmetic_operation_t operation;
    ptk_f32_t operand1;
    ptk_f32_t operand2;
} client_config_t;

static void client_config_destructor(void *ptr) {
    client_config_t *config = (client_config_t*)ptr;
    if (config && config->server_ip) {
        ptk_local_free(&config->server_ip);
    }
}

#define CLIENT_THREAD_PTR_ARG_TYPE 1

static ptk_err_t send_arithmetic_request(ptk_sock *sock, arithmetic_operation_t op, ptk_f32_t op1, ptk_f32_t op2) {
    info("Sending arithmetic request: op=%d, operand1=%f, operand2=%f", op, op1, op2);
    
    arithmetic_request_t *request = arithmetic_request_create(op, op1, op2);
    if (!request) {
        error("Failed to create arithmetic request");
        return PTK_ERR_NO_RESOURCES;
    }
    
    ptk_buf *request_buf = ptk_buf_alloc(64);
    if (!request_buf) {
        error("Failed to allocate request buffer");
        ptk_local_free(&request);
        return PTK_ERR_NO_RESOURCES;
    }
    
    ptk_err_t err = arithmetic_request_serialize(request_buf, (ptk_serializable_t*)request);
    ptk_local_free(&request);
    
    if (err != PTK_OK) {
        error("Failed to serialize request: %d", err);
        ptk_local_free(&request_buf);
        return err;
    }
    
    debug("Sending %d bytes to server", ptk_buf_get_len(request_buf));
    debug_buf(request_buf);
            
            
    err = ptk_tcp_socket_send(sock, request_buf, 5000);
    ptk_local_free(&request_buf);

    if (err != PTK_OK) {
        error("Failed to send request: %d", err);
        return err;
    }
    
    info("Request sent successfully");
    return PTK_OK;
}

static ptk_err_t receive_arithmetic_response(ptk_sock *sock, ptk_f64_t *result) {
    info("Waiting for arithmetic response");
    
    ptk_buf *response_buf = ptk_tcp_socket_recv(sock, 5000);
    if (!response_buf) {
        ptk_err_t err = ptk_get_err();
        error("Failed to receive response: %d", err);
        return err;
    }
    
    debug("Received %d bytes from server", ptk_buf_get_len(response_buf));
    debug_buf(response_buf);
    
    arithmetic_response_t *response = arithmetic_response_create(ARITH_OP_ADD, 0.0);
    if (!response) {
        error("Failed to create response object");
        ptk_local_free(&response_buf);
        return PTK_ERR_NO_RESOURCES;
    }
    
    ptk_err_t err = arithmetic_response_deserialize(response_buf, (ptk_serializable_t*)response);
    ptk_local_free(&response_buf);
    
    if (err != PTK_OK) {
        error("Failed to deserialize response: %d", err);
        ptk_local_free(&response);
        return err;
    }
    
    *result = response->result;
    info("Received result: %f (operation_inverted=0x%02x)", response->result, response->operation_inverted);
    
    ptk_local_free(&response);
    return PTK_OK;
}

static void client_thread_run_func(void) {
    size_t num_args = ptk_thread_get_arg_count();
    if (num_args != 1) {
        error("this requires exactly 1 argument (config handle), got %zu", num_args);
        return;
    }

    if(ptk_thread_get_arg_type(0) != CLIENT_THREAD_PTR_ARG_TYPE) {
        error("argument must be a handle!");
        return;
    }

    ptk_shared_handle_t config_handle = ptk_thread_get_handle_arg(0);

    info("Client thread started");

    // Remove this duplicate - it's handled below

    use_shared(config_handle, client_config_t*, config, PTK_TIME_WAIT_FOREVER) {
        info("Connecting to server %s:%d", config->server_ip, config->server_port);
        
        ptk_address_t *server_addr = ptk_address_create(config->server_ip, config->server_port);
        if (!server_addr) {
            error("Failed to create server address: %d", ptk_get_err());
            return;
        }
        
        ptk_sock *client_sock = ptk_tcp_connect(server_addr, 5000);
        if (!client_sock) {
            error("Failed to connect to server: %d", ptk_get_err());
            ptk_local_free(&server_addr);
            return;
        }
        ptk_local_free(&server_addr);
        
        info("Connected to server successfully");
        
        ptk_err_t err = send_arithmetic_request(client_sock, config->operation, config->operand1, config->operand2);
        if (err != PTK_OK) {
            error("Failed to send request: %d", err);
            ptk_local_free(&client_sock);
            return;
        }
        
        ptk_f64_t result;
        err = receive_arithmetic_response(client_sock, &result);
        if (err != PTK_OK) {
            error("Failed to receive response: %d", err);
            ptk_local_free(&client_sock);
            return;
        }
        
        const char *op_str;
        switch (config->operation) {
            case ARITH_OP_ADD: op_str = "+"; break;
            case ARITH_OP_SUB: op_str = "-"; break;
            case ARITH_OP_MUL: op_str = "*"; break;
            case ARITH_OP_DIV: op_str = "/"; break;
            default: op_str = "?"; break;
        }
        
        printf("Result: %f %s %f = %f\n", config->operand1, op_str, config->operand2, result);
        
        info("Closing connection to server");
        ptk_local_free(&client_sock);
        
    } on_shared_fail {
        error("Failed to acquire client configuration");
    }
    
    info("Client threadlet exiting");
}


int main(int argc, char *argv[]) {
    bool help = false;
    client_config_t *config = NULL;

    config = ptk_local_alloc(sizeof(client_config_t), client_config_destructor);
    
    // Initialize defaults
    config->server_port = 12345;
    config->operation = ARITH_OP_ADD;
    config->operand1 = 5.0f;
    config->operand2 = 3.0f;
    
    ptk_config_field_t config_fields[] = {
        {"server", 's', PTK_CONFIG_STRING, &config->server_ip, "Server IP address", "127.0.0.1"},
        {"port", 'p', PTK_CONFIG_UINT16, &config->server_port, "Server port", "12345"},
        {"help", 'h', PTK_CONFIG_HELP, &help, "Show this help message", NULL},
        PTK_CONFIG_END
    };
    
    ptk_err_t err = ptk_config_parse(argc, argv, config_fields, "arithmetic_client");
    if (err == 1) {
        return 0;
    } else if (err != PTK_OK) {
        return 1;
    }
    
    if (!config->server_ip) {
        config->server_ip = ptk_local_alloc(16, NULL);
        if (config->server_ip) strcpy(config->server_ip, "127.0.0.1");
    }
    
    ptk_log_level_set(PTK_LOG_LEVEL_DEBUG);
    
    info("Starting Protocol Toolkit arithmetic client");
    
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
    
    ptk_shared_handle_t config_handle = ptk_shared_alloc(sizeof(client_config_t), client_config_destructor);
    if (!ptk_shared_is_valid(config_handle)) {
        ptk_local_free(&config);
        error("Failed to create shared config");
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    // Copy config to shared memory
    use_shared(config_handle, client_config_t*, shared_config, PTK_TIME_NO_WAIT) {
        *shared_config = *config;
        if (config->server_ip) {
            shared_config->server_ip = ptk_local_alloc(strlen(config->server_ip) + 1, NULL);
            strcpy(shared_config->server_ip, config->server_ip);
        }
    } on_shared_fail {
        error("Failed to initialize shared config");
        ptk_local_free(&config);
        ptk_shared_free(&config_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    ptk_local_free(&config);
    
    ptk_thread_handle_t client_thread = ptk_thread_create();
    if (!ptk_shared_is_valid(client_thread)) {
        error("Failed to create client thread");
        ptk_shared_free(&config_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }

    err = ptk_thread_add_handle_arg(client_thread, CLIENT_THREAD_PTR_ARG_TYPE, &config_handle);
    if (err != PTK_OK) {
        error("Failed to add argument to client thread: %d", err);
        ptk_shared_release(client_thread);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    err = ptk_thread_set_run_function(client_thread, client_thread_run_func);
    if (err != PTK_OK) {
        error("Failed to set thread function: %d", err);
        ptk_shared_release(client_thread);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    err = ptk_thread_start(client_thread);
    if (err != PTK_OK) {
        error("Failed to start client thread: %d", err);
        ptk_shared_release(client_thread);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    info("Client thread started, waiting for completion");
    
    // Wait for thread to complete
    ptk_thread_wait(10000);
    
    info("Shutting down client");
    
    ptk_shared_shutdown();
    ptk_shutdown();
    
    return 0;
}