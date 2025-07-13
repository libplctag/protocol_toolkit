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
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *server_ip;
    uint16_t server_port;
    arithmetic_operation_t operation;
    f32 operand1;
    f32 operand2;
} client_config_t;

static void client_config_destructor(void *ptr) {
    client_config_t *config = (client_config_t*)ptr;
    if (config && config->server_ip) {
        ptk_free(&config->server_ip);
    }
}

static ptk_err send_arithmetic_request(ptk_sock *sock, arithmetic_operation_t op, f32 op1, f32 op2) {
    info("Sending arithmetic request: op=%d, operand1=%f, operand2=%f", op, op1, op2);
    
    arithmetic_request_t *request = arithmetic_request_create(op, op1, op2);
    if (!request) {
        error("Failed to create arithmetic request");
        return PTK_ERR_NO_RESOURCES;
    }
    
    ptk_buf *request_buf = ptk_buf_alloc(64);
    if (!request_buf) {
        error("Failed to allocate request buffer");
        ptk_free(&request);
        return PTK_ERR_NO_RESOURCES;
    }
    
    ptk_err err = arithmetic_request_serialize(request_buf, (ptk_serializable_t*)request);
    ptk_free(&request);
    
    if (err != PTK_OK) {
        error("Failed to serialize request: %d", err);
        ptk_free(&request_buf);
        return err;
    }
    
    debug("Sending %d bytes to server", ptk_buf_get_len(request_buf));
    debug_buf(request_buf);
    
    ptk_buf_array_t *buf_array = ptk_buf_array_create();
    if (!buf_array) {
        error("Failed to create buffer array");
        ptk_free(&request_buf);
        return PTK_ERR_NO_RESOURCES;
    }
    
    ptk_buf_array_append(buf_array, request_buf);
    
    err = ptk_tcp_socket_send(sock, buf_array, 5000);
    ptk_free(&buf_array);
    
    if (err != PTK_OK) {
        error("Failed to send request: %d", err);
        return err;
    }
    
    info("Request sent successfully");
    return PTK_OK;
}

static ptk_err receive_arithmetic_response(ptk_sock *sock, f64 *result) {
    info("Waiting for arithmetic response");
    
    ptk_buf *response_buf = ptk_tcp_socket_recv(sock, false, 5000);
    if (!response_buf) {
        ptk_err err = ptk_get_err();
        error("Failed to receive response: %d", err);
        return err;
    }
    
    debug("Received %d bytes from server", ptk_buf_get_len(response_buf));
    debug_buf(response_buf);
    
    arithmetic_response_t *response = arithmetic_response_create(ARITH_OP_ADD, 0.0);
    if (!response) {
        error("Failed to create response object");
        ptk_free(&response_buf);
        return PTK_ERR_NO_RESOURCES;
    }
    
    ptk_err err = arithmetic_response_deserialize(response_buf, (ptk_serializable_t*)response);
    ptk_free(&response_buf);
    
    if (err != PTK_OK) {
        error("Failed to deserialize response: %d", err);
        ptk_free(&response);
        return err;
    }
    
    *result = response->result;
    info("Received result: %f (operation_inverted=0x%02x)", response->result, response->operation_inverted);
    
    ptk_free(&response);
    return PTK_OK;
}

static void client_threadlet(void *param) {
    ptk_shared_handle_t config_handle = *(ptk_shared_handle_t*)param;
    
    info("Client threadlet started");
    
    use_shared(config_handle, client_config_t *config) {
        info("Connecting to server %s:%d", config->server_ip, config->server_port);
        
        ptk_address_t server_addr;
        ptk_err err = ptk_address_init(&server_addr, config->server_ip, config->server_port);
        if (err != PTK_OK) {
            error("Failed to initialize server address: %d", err);
            return;
        }
        
        ptk_sock *client_sock = ptk_tcp_socket_connect(&server_addr, 5000);
        if (!client_sock) {
            error("Failed to connect to server: %d", ptk_get_err());
            return;
        }
        
        info("Connected to server successfully");
        
        err = send_arithmetic_request(client_sock, config->operation, config->operand1, config->operand2);
        if (err != PTK_OK) {
            error("Failed to send request: %d", err);
            ptk_free(&client_sock);
            return;
        }
        
        f64 result;
        err = receive_arithmetic_response(client_sock, &result);
        if (err != PTK_OK) {
            error("Failed to receive response: %d", err);
            ptk_free(&client_sock);
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
        ptk_free(&client_sock);
        
    } on_shared_fail {
        error("Failed to acquire client configuration");
    }
    
    info("Client threadlet exiting");
}

static arithmetic_operation_t parse_operation(const char *op_str) {
    if (!op_str) return ARITH_OP_ADD;
    
    switch (op_str[0]) {
        case '+': return ARITH_OP_ADD;
        case '-': return ARITH_OP_SUB;
        case '*': 
        case 'x': 
        case 'X': return ARITH_OP_MUL;
        case '/': return ARITH_OP_DIV;
        default:
            warn("Unknown operation '%s', defaulting to addition", op_str);
            return ARITH_OP_ADD;
    }
}

int main(int argc, char *argv[]) {
    char *server_ip = NULL;
    uint16_t server_port = 12345;
    char *operation_str = NULL;
    char *operand1_str = NULL;
    char *operand2_str = NULL;
    bool help = false;
    
    ptk_config_field_t config_fields[] = {
        {"server", 's', PTK_CONFIG_STRING, &server_ip, "Server IP address", "127.0.0.1"},
        {"port", 'p', PTK_CONFIG_UINT16, &server_port, "Server port", "12345"},
        {"operation", 'o', PTK_CONFIG_STRING, &operation_str, "Operation (+, -, *, /)", "+"},
        {"operand1", '1', PTK_CONFIG_STRING, &operand1_str, "First operand", "5.0"},
        {"operand2", '2', PTK_CONFIG_STRING, &operand2_str, "Second operand", "3.0"},
        {"help", 'h', PTK_CONFIG_HELP, &help, "Show this help message", NULL},
        PTK_CONFIG_END
    };
    
    ptk_err err = ptk_config_parse(argc, argv, config_fields, "arithmetic_client");
    if (err == 1) {
        return 0;
    } else if (err != PTK_OK) {
        return 1;
    }
    
    if (!server_ip) {
        server_ip = ptk_alloc(16, NULL);
        if (server_ip) {
            strcpy(server_ip, "127.0.0.1");
        }
    }
    
    if (!operation_str) {
        operation_str = ptk_alloc(2, NULL);
        if (operation_str) {
            strcpy(operation_str, "+");
        }
    }
    
    if (!operand1_str) {
        operand1_str = ptk_alloc(8, NULL);
        if (operand1_str) {
            strcpy(operand1_str, "5.0");
        }
    }
    
    if (!operand2_str) {
        operand2_str = ptk_alloc(8, NULL);
        if (operand2_str) {
            strcpy(operand2_str, "3.0");
        }
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
    
    client_config_t *config = ptk_alloc(sizeof(client_config_t), client_config_destructor);
    if (!config) {
        error("Failed to allocate client configuration");
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    config->server_ip = server_ip;
    config->server_port = server_port;
    config->operation = parse_operation(operation_str);
    config->operand1 = operand1_str ? (f32)atof(operand1_str) : 5.0f;
    config->operand2 = operand2_str ? (f32)atof(operand2_str) : 3.0f;
    
    if (operation_str) ptk_free(&operation_str);
    if (operand1_str) ptk_free(&operand1_str);
    if (operand2_str) ptk_free(&operand2_str);
    
    ptk_shared_handle_t config_handle = ptk_shared_wrap(config);
    if (!PTK_SHARED_IS_VALID(config_handle)) {
        error("Failed to wrap configuration in shared memory");
        ptk_free(&config);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    ptk_shared_handle_t *handle_param = ptk_alloc(sizeof(ptk_shared_handle_t), NULL);
    if (!handle_param) {
        error("Failed to allocate handle parameter");
        ptk_shared_release(config_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    *handle_param = config_handle;
    
    threadlet_t *client_threadlet = ptk_threadlet_create(client_threadlet, handle_param);
    if (!client_threadlet) {
        error("Failed to create client threadlet");
        ptk_free(&handle_param);
        ptk_shared_release(config_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    err = ptk_threadlet_resume(client_threadlet);
    if (err != PTK_OK) {
        error("Failed to start client threadlet: %d", err);
        ptk_free(&client_threadlet);
        ptk_free(&handle_param);
        ptk_shared_release(config_handle);
        ptk_shared_shutdown();
        ptk_shutdown();
        return 1;
    }
    
    info("Client threadlet started, waiting for completion");
    
    err = ptk_threadlet_join(client_threadlet, 10000);
    if (err != PTK_OK) {
        warn("Client threadlet join failed: %d", err);
    }
    
    info("Shutting down client");
    
    ptk_shared_shutdown();
    ptk_shutdown();
    
    return 0;
}