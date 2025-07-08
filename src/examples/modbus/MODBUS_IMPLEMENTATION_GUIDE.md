# Modbus Server and Client Implementation Guide

This document describes how to create Modbus TCP server and client applications using the Protocol Toolkit library, focusing on the base set of register operations with simplicity and clarity as primary goals.

## Table of Contents

1. [Overview](#overview)
2. [Protocol Toolkit Library Usage](#protocol-toolkit-library-usage)
3. [Modbus Protocol Basics](#modbus-protocol-basics)
4. [Implementation Approach](#implementation-approach)
5. [Modbus Client Implementation](#modbus-client-implementation)
6. [Modbus Server Implementation](#modbus-server-implementation)
7. [Error Handling](#error-handling)
8. [Example Usage](#example-usage)
9. [Testing and Debugging](#testing-and-debugging)

## Overview

Modbus TCP is a simple protocol for industrial communication. This guide shows how to implement both server and client using the Protocol Toolkit's socket and buffer APIs. We prioritize:

- **Simplicity**: Direct buffer manipulation over complex structs
- **Clarity**: Readable code over micro-optimizations
- **Maintainability**: Easy to understand and modify

The base register operations we'll support:
- **Function 03**: Read Holding Registers
- **Function 04**: Read Input Registers
- **Function 06**: Write Single Register
- **Function 16**: Write Multiple Registers

## Protocol Toolkit Library Usage

The Protocol Toolkit provides these key components for Modbus implementation:

**Note**: This guide uses the current Protocol Toolkit APIs from `src/include/`. All buffer operations use explicit endianness parameters (`PTK_BUF_BIG_ENDIAN` for Modbus TCP) and peek flags for consume operations.

### Buffer Management (`ptk_buf.h`)

```c
#include "ptk_buf.h"

// Create buffer from caller-provided memory
uint8_t buffer_data[256];
ptk_buf_t buffer;
ptk_buf_create(&buffer, buffer_data, sizeof(buffer_data));

// Write data (producer operations - move end index)
ptk_buf_produce_u16(&buffer, transaction_id, PTK_BUF_BIG_ENDIAN);
ptk_buf_produce_u8(&buffer, unit_id);

// Read data (consumer operations - move start index)
uint16_t transaction_id;
ptk_buf_consume_u16(&buffer, &transaction_id, PTK_BUF_BIG_ENDIAN, false);
```

### Socket Communication (`ptk_socket.h`)

```c
#include "ptk_socket.h"

// TCP Server
ptk_sock *server;
ptk_tcp_socket_listen(&server, "0.0.0.0", 502, 5);
ptk_tcp_socket_accept(server, &client);
ptk_tcp_socket_read(client, &recv_buffer);
ptk_tcp_socket_write(client, &send_buffer);

// TCP Client
ptk_sock *client;
ptk_tcp_socket_connect(&client, "192.168.1.100", 502);
ptk_tcp_socket_write(client, &request_buffer);
ptk_tcp_socket_read(client, &response_buffer);
```

### Error Handling (`ptk_err.h`)

```c
#include "ptk_err.h"

ptk_err err = ptk_tcp_socket_connect(&client, host, port);
if (err != PTK_OK) {
    printf("Connection failed: %s\n", ptk_err_to_string(err));
    return err;
}
```

## Modbus Protocol Basics

### MBAP Header (7 bytes)
```
+---------------+---------------+---------------+---------------+
| Transaction ID (2 bytes)      | Protocol ID (2 bytes)        |
+---------------+---------------+---------------+---------------+
| Length (2 bytes)              | Unit ID (1 byte)             |
+---------------+---------------+---------------+
```

### PDU Format
```
+---------------+---------------+---------------+
| Function Code | Data (0-252 bytes)           |
+---------------+---------------+---------------+
```

### Complete Modbus TCP Frame
```
MBAP Header (7 bytes) + Function Code (1 byte) + Data (variable)
```

## Implementation Approach

Rather than creating specific structs for each PDU type, we use direct buffer operations. This approach is:

- **Simpler**: No struct definitions to maintain
- **Clearer**: Buffer operations are explicit and visible
- **More flexible**: Easy to handle variable-length data
- **Less error-prone**: No padding or alignment issues

### Example: Reading Registers Request

Instead of:
```c
// Complex struct approach (NOT recommended)
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t quantity;
} __attribute__((packed)) read_holding_registers_request_t;
```

We use:
```c
// Simple buffer approach (RECOMMENDED)
ptk_buf_produce_u16(&buffer, transaction_id, PTK_BUF_BIG_ENDIAN);    // MBAP header
ptk_buf_produce_u16(&buffer, 0, PTK_BUF_BIG_ENDIAN);                 // Protocol ID
ptk_buf_produce_u16(&buffer, 6, PTK_BUF_BIG_ENDIAN);                 // Length
ptk_buf_produce_u8(&buffer, unit_id);                                // Unit ID
ptk_buf_produce_u8(&buffer, 0x03);                                   // Function code
ptk_buf_produce_u16(&buffer, start_address, PTK_BUF_BIG_ENDIAN);     // Start address
ptk_buf_produce_u16(&buffer, quantity, PTK_BUF_BIG_ENDIAN);          // Quantity
```

## Modbus Client Implementation

### Basic Client Structure

```c
#include "ptk_socket.h"
#include "ptk_buf.h"
#include "ptk_err.h"

typedef struct {
    ptk_sock *socket;
    char *host;
    int port;
    uint8_t unit_id;
    uint16_t transaction_id;
} modbus_client_t;

// Create and connect client
ptk_err modbus_client_create(modbus_client_t **client, const char *host, int port, uint8_t unit_id) {
    modbus_client_t *c = calloc(1, sizeof(modbus_client_t));
    if (!c) return PTK_ERR_NO_RESOURCES;

    c->host = strdup(host);
    c->port = port;
    c->unit_id = unit_id;
    c->transaction_id = 1;

    ptk_err err = ptk_tcp_socket_connect(&c->socket, host, port);
    if (err != PTK_OK) {
        free(c->host);
        free(c);
        return err;
    }

    *client = c;
    return PTK_OK;
}
```

### Building MBAP Header

```c
static ptk_err build_mbap_header(ptk_buf_t *buffer, uint16_t transaction_id,
                                uint16_t length, uint8_t unit_id) {
    ptk_err err;

    if ((err = ptk_buf_produce_u16(buffer, transaction_id, PTK_BUF_BIG_ENDIAN)) != PTK_OK) return err;
    if ((err = ptk_buf_produce_u16(buffer, 0, PTK_BUF_BIG_ENDIAN)) != PTK_OK) return err;        // Protocol ID
    if ((err = ptk_buf_produce_u16(buffer, length, PTK_BUF_BIG_ENDIAN)) != PTK_OK) return err;
    if ((err = ptk_buf_produce_u8(buffer, unit_id)) != PTK_OK) return err;

    return PTK_OK;
}
```

### Read Holding Registers (Function 03)

```c
ptk_err modbus_read_holding_registers(modbus_client_t *client,
                                     uint16_t start_address,
                                     uint16_t quantity,
                                     uint16_t *values) {
    if (!client || !values || quantity == 0 || quantity > 125) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Build request
    uint8_t request_data[12];  // MBAP (7) + Function (1) + Address (2) + Quantity (2)
    ptk_buf_t request;
    ptk_buf_create(&request, request_data, sizeof(request_data));

    ptk_err err = build_mbap_header(&request, client->transaction_id++, 6, client->unit_id);
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u8(&request, 0x03);              // Function code
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u16(&request, start_address, PTK_BUF_BIG_ENDIAN);
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u16(&request, quantity, PTK_BUF_BIG_ENDIAN);
    if (err != PTK_OK) return err;

    // Send request
    err = ptk_tcp_socket_write(client->socket, &request);
    if (err != PTK_OK) return err;

    // Receive response
    uint8_t response_data[256];
    ptk_buf_t response;
    ptk_buf_create(&response, response_data, sizeof(response_data));

    err = ptk_tcp_socket_read(client->socket, &response);
    if (err != PTK_OK) return err;

    // Parse response
    return parse_read_registers_response(&response, quantity, values);
}
```

### Parsing Response

```c
static ptk_err parse_read_registers_response(ptk_buf_t *response, uint16_t expected_quantity, uint16_t *values) {
    ptk_err err;

    // Skip MBAP header (7 bytes)
    ptk_buf_move_to(response, 7);

    // Read function code
    uint8_t function_code;
    err = ptk_buf_consume_u8(response, &function_code, false);
    if (err != PTK_OK) return err;

    // Check for error response
    if (function_code & 0x80) {
        uint8_t exception_code;
        ptk_buf_consume_u8(response, &exception_code, false);
        printf("Modbus exception: 0x%02X\n", exception_code);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    if (function_code != 0x03) {
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Read byte count
    uint8_t byte_count;
    err = ptk_buf_consume_u8(response, &byte_count, false);
    if (err != PTK_OK) return err;

    if (byte_count != expected_quantity * 2) {
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Read register values
    for (uint16_t i = 0; i < expected_quantity; i++) {
        err = ptk_buf_consume_u16(response, &values[i], PTK_BUF_BIG_ENDIAN, false);
        if (err != PTK_OK) return err;
    }

    return PTK_OK;
}
```

### Write Single Register (Function 06)

```c
ptk_err modbus_write_single_register(modbus_client_t *client,
                                    uint16_t address,
                                    uint16_t value) {
    if (!client) return PTK_ERR_INVALID_PARAM;

    // Build request
    uint8_t request_data[12];  // MBAP (7) + Function (1) + Address (2) + Value (2)
    ptk_buf_t request;
    ptk_buf_create(&request, request_data, sizeof(request_data));

    ptk_err err = build_mbap_header(&request, client->transaction_id++, 6, client->unit_id);
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u8(&request, 0x06);      // Function code
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u16(&request, address, PTK_BUF_BIG_ENDIAN);
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u16(&request, value, PTK_BUF_BIG_ENDIAN);
    if (err != PTK_OK) return err;

    // Send request
    err = ptk_tcp_socket_write(client->socket, &request);
    if (err != PTK_OK) return err;

    // Receive and validate echo response
    uint8_t response_data[12];
    ptk_buf_t response;
    ptk_buf_create(&response, response_data, sizeof(response_data));

    err = ptk_tcp_socket_read(client->socket, &response);
    if (err != PTK_OK) return err;

    // For write single register, response should echo the request
    return validate_echo_response(&request, &response);
}
```

## Modbus Server Implementation

### Basic Server Structure

```c
typedef struct {
    ptk_sock *listen_socket;
    int port;
    bool running;

    // Register storage
    uint16_t holding_registers[10000];
    uint16_t input_registers[10000];

    // Thread management
    ptk_thread *server_thread;
    ptk_mutex *registers_mutex;
} modbus_server_t;

ptk_err modbus_server_create(modbus_server_t **server, int port) {
    modbus_server_t *s = calloc(1, sizeof(modbus_server_t));
    if (!s) return PTK_ERR_NO_RESOURCES;

    s->port = port;
    s->running = true;

    ptk_err err = ptk_mutex_create(&s->registers_mutex);
    if (err != PTK_OK) {
        free(s);
        return err;
    }

    err = ptk_tcp_socket_listen(&s->listen_socket, "0.0.0.0", port, 5);
    if (err != PTK_OK) {
        ptk_mutex_destroy(s->registers_mutex);
        free(s);
        return err;
    }

    *server = s;
    return PTK_OK;
}
```

### Server Main Loop

```c
static void server_thread_func(void *arg) {
    modbus_server_t *server = (modbus_server_t *)arg;

    printf("Modbus server listening on port %d\n", server->port);

    while (server->running) {
        ptk_sock *client_socket;
        ptk_err err = ptk_tcp_socket_accept(server->listen_socket, &client_socket);

        if (err == PTK_ERR_ABORT) {
            break;  // Server shutdown
        } else if (err != PTK_OK) {
            printf("Accept failed: %s\n", ptk_err_to_string(err));
            continue;
        }

        // Handle client in same thread (simple approach)
        handle_client_connection(server, client_socket);
        ptk_socket_close(client_socket);
    }
}
```

### Request Processing

```c
static void handle_client_connection(modbus_server_t *server, ptk_sock *client) {
    uint8_t buffer_data[256];
    ptk_buf_t buffer;

    while (server->running) {
        // Read request
        ptk_buf_create(&buffer, buffer_data, sizeof(buffer_data));
        ptk_err err = ptk_tcp_socket_read(client, &buffer);

        if (err == PTK_ERR_CLOSED || err == PTK_ERR_ABORT) {
            break;
        } else if (err != PTK_OK) {
            printf("Read error: %s\n", ptk_err_to_string(err));
            break;
        }

        // Process request and build response
        ptk_buf_t response;
        ptk_buf_create(&response, buffer_data, sizeof(buffer_data));

        err = process_modbus_request(server, &buffer, &response);
        if (err != PTK_OK) {
            continue;  // Skip malformed requests
        }

        // Send response
        err = ptk_tcp_socket_write(client, &response);
        if (err != PTK_OK) {
            printf("Write error: %s\n", ptk_err_to_string(err));
            break;
        }
    }
}
```

### Request Processing Logic

```c
static ptk_err process_modbus_request(modbus_server_t *server, ptk_buf_t *request, ptk_buf_t *response) {
    // Parse MBAP header
    uint16_t transaction_id, protocol_id, length;
    uint8_t unit_id, function_code;

    ptk_err err = ptk_buf_consume_u16(request, &transaction_id, PTK_BUF_BIG_ENDIAN, false);
    if (err != PTK_OK) return err;

    err = ptk_buf_consume_u16(request, &protocol_id, PTK_BUF_BIG_ENDIAN, false);
    if (err != PTK_OK) return err;

    err = ptk_buf_consume_u16(request, &length, PTK_BUF_BIG_ENDIAN, false);
    if (err != PTK_OK) return err;

    err = ptk_buf_consume_u8(request, &unit_id, false);
    if (err != PTK_OK) return err;

    err = ptk_buf_consume_u8(request, &function_code, false);
    if (err != PTK_OK) return err;

    // Process based on function code
    switch (function_code) {
        case 0x03:  // Read Holding Registers
            return handle_read_holding_registers(server, request, response,
                                               transaction_id, unit_id);
        case 0x04:  // Read Input Registers
            return handle_read_input_registers(server, request, response,
                                             transaction_id, unit_id);
        case 0x06:  // Write Single Register
            return handle_write_single_register(server, request, response,
                                               transaction_id, unit_id);
        case 0x10:  // Write Multiple Registers
            return handle_write_multiple_registers(server, request, response,
                                                  transaction_id, unit_id);
        default:
            return send_exception_response(response, transaction_id, unit_id,
                                         function_code, 0x01);  // Illegal function
    }
}
```

### Read Holding Registers Handler

```c
static ptk_err handle_read_holding_registers(modbus_server_t *server,
                                           ptk_buf_t *request, ptk_buf_t *response,
                                           uint16_t transaction_id, uint8_t unit_id) {
    uint16_t start_address, quantity;
    ptk_err err;

    // Parse request
    err = ptk_buf_consume_u16(request, &start_address, PTK_BUF_BIG_ENDIAN, false);
    if (err != PTK_OK) return err;

    err = ptk_buf_consume_u16(request, &quantity, PTK_BUF_BIG_ENDIAN, false);
    if (err != PTK_OK) return err;

    // Validate parameters
    if (quantity == 0 || quantity > 125 ||
        start_address + quantity > 10000) {
        return send_exception_response(response, transaction_id, unit_id,
                                     0x03, 0x02);  // Illegal data address
    }

    // Build response header
    uint16_t response_length = 3 + (quantity * 2);  // Unit ID + Function + Byte count + Data
    err = build_mbap_header(response, transaction_id, response_length, unit_id);
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u8(response, 0x03);  // Function code
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u8(response, quantity * 2);  // Byte count
    if (err != PTK_OK) return err;

    // Add register values (with mutex protection)
    ptk_mutex_wait_lock(server->registers_mutex, PTK_TIME_WAIT_FOREVER);

    for (uint16_t i = 0; i < quantity; i++) {
        err = ptk_buf_produce_u16(response, server->holding_registers[start_address + i], PTK_BUF_BIG_ENDIAN);
        if (err != PTK_OK) {
            ptk_mutex_unlock(server->registers_mutex);
            return err;
        }
    }

    ptk_mutex_unlock(server->registers_mutex);
    return PTK_OK;
}
```

### Exception Response

```c
static ptk_err send_exception_response(ptk_buf_t *response, uint16_t transaction_id,
                                     uint8_t unit_id, uint8_t function_code,
                                     uint8_t exception_code) {
    ptk_err err = build_mbap_header(response, transaction_id, 3, unit_id);
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u8(response, function_code | 0x80);  // Set error bit
    if (err != PTK_OK) return err;

    err = ptk_buf_produce_u8(response, exception_code);
    if (err != PTK_OK) return err;

    return PTK_OK;
}
```

## Error Handling

### Client Error Handling

```c
ptk_err modbus_client_operation(modbus_client_t *client, ...) {
    ptk_err err = ptk_tcp_socket_write(client->socket, &request);

    switch (err) {
        case PTK_OK:
            break;  // Continue

        case PTK_ERR_CLOSED:
            printf("Connection closed by server\n");
            return err;

        case PTK_ERR_ABORT:
            printf("Operation aborted\n");
            return err;

        case PTK_ERR_TIMEOUT:
            printf("Operation timed out\n");
            return err;

        default:
            printf("Network error: %s\n", ptk_err_to_string(err));
            return err;
    }

    // Continue with response handling...
}
```

### Server Error Handling

```c
static void handle_client_with_error_recovery(modbus_server_t *server, ptk_sock *client) {
    int consecutive_errors = 0;
    const int MAX_ERRORS = 5;

    while (server->running && consecutive_errors < MAX_ERRORS) {
        ptk_err err = handle_single_request(server, client);

        if (err == PTK_OK) {
            consecutive_errors = 0;  // Reset error count
        } else if (err == PTK_ERR_CLOSED || err == PTK_ERR_ABORT) {
            break;  // Clean disconnect
        } else {
            consecutive_errors++;
            printf("Request error: %s\n", ptk_err_to_string(err));
        }
    }

    if (consecutive_errors >= MAX_ERRORS) {
        printf("Too many consecutive errors, disconnecting client\n");
    }
}
```

## Example Usage

### Client Example

```c
int main() {
    modbus_client_t *client;
    ptk_err err = modbus_client_create(&client, "192.168.1.100", 502, 1);
    if (err != PTK_OK) {
        printf("Failed to create client: %s\n", ptk_err_to_string(err));
        return 1;
    }

    // Read 10 holding registers starting at address 0
    uint16_t values[10];
    err = modbus_read_holding_registers(client, 0, 10, values);
    if (err == PTK_OK) {
        for (int i = 0; i < 10; i++) {
            printf("Register %d: %u\n", i, values[i]);
        }
    } else {
        printf("Read failed: %s\n", ptk_err_to_string(err));
    }

    // Write a single register
    err = modbus_write_single_register(client, 5, 1234);
    if (err == PTK_OK) {
        printf("Write successful\n");
    } else {
        printf("Write failed: %s\n", ptk_err_to_string(err));
    }

    modbus_client_destroy(client);
    return 0;
}
```

### Server Example

```c
int main() {
    modbus_server_t *server;
    ptk_err err = modbus_server_create(&server, 502);
    if (err != PTK_OK) {
        printf("Failed to create server: %s\n", ptk_err_to_string(err));
        return 1;
    }

    // Initialize some test data
    for (int i = 0; i < 100; i++) {
        server->holding_registers[i] = i * 10;
        server->input_registers[i] = i * 5;
    }

    err = modbus_server_start(server);
    if (err != PTK_OK) {
        printf("Failed to start server: %s\n", ptk_err_to_string(err));
        return 1;
    }

    printf("Server running. Press Enter to stop...\n");
    getchar();

    modbus_server_stop(server);
    modbus_server_destroy(server);
    return 0;
}
```

## Testing and Debugging

### Protocol Debugging

```c
static void dump_buffer(const char *label, ptk_buf_t *buffer) {
    size_t len;
    ptk_buf_len(&len, buffer);

    uint8_t *data;
    ptk_buf_get_start_ptr(&data, buffer);

    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// Use in client/server for debugging
dump_buffer("Request", &request_buffer);
dump_buffer("Response", &response_buffer);
```

### Connection Testing

```c
ptk_err test_connection(const char *host, int port) {
    ptk_sock *test_socket;
    ptk_err err = ptk_tcp_socket_connect(&test_socket, host, port);

    if (err == PTK_OK) {
        printf("Connection to %s:%d successful\n", host, port);
        ptk_socket_close(test_socket);
    } else {
        printf("Connection to %s:%d failed: %s\n", host, port, ptk_err_to_string(err));
    }

    return err;
}
```

### Register Validation

```c
static bool validate_register_range(uint16_t address, uint16_t quantity, uint16_t max_registers) {
    if (quantity == 0 || quantity > 125) {
        return false;  // Invalid quantity
    }

    if (address >= max_registers) {
        return false;  // Address out of range
    }

    if (address + quantity > max_registers) {
        return false;  // Range exceeds available registers
    }

    return true;
}
```

## Conclusion

This implementation approach using the Protocol Toolkit library provides:

- **Simple buffer operations** instead of complex struct definitions
- **Clear error handling** with meaningful error messages
- **Thread-safe register access** with mutex protection
- **Robust network communication** with proper connection management
- **Easy debugging** with buffer dump utilities
- **Modular design** that's easy to extend and maintain

The code is slightly longer than a struct-based approach but significantly clearer and more maintainable. Each operation is explicit, making it easy to understand the protocol flow and debug issues.

For production use, consider adding:
- Configuration file support
- Logging with different levels
- Persistent register storage
- Multiple unit ID support
- Connection pooling for clients
- Performance monitoring