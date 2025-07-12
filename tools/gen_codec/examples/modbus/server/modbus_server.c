#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../modbus.h"
#include "ptk_alloc.h"
#include "ptk_array.h"
#include "ptk_buf.h"
#include "ptk_codec.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include "ptk_socket.h"

// Global state for graceful shutdown
static volatile bool g_running = true;

// Modbus server state
typedef struct modbus_server_state {
    bool coils[10000];             // Digital outputs
    bool inputs[10000];            // Digital inputs
    uint16_t holding_regs[10000];  // Read/write registers
    uint16_t input_regs[10000];    // Read-only registers
} modbus_server_state_t;

static modbus_server_state_t g_server_state = {0};

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    (void)sig;
    g_running = false;
    ptk_log_info("Received shutdown signal");
}

// Encode Modbus TCP header to buffer
ptk_err encode_modbus_tcp_header(ptk_buf *buf, const modbus_tcp_header_t *header) {
    ptk_err err;

    // Encode transaction ID (big-endian)
    err = ptk_codec_produce_u16(buf, header->transaction_id, PTK_CODEC_BIG_ENDIAN);
    if(err != PTK_OK) { return err; }

    // Protocol ID (big-endian)
    err = ptk_codec_produce_u16(buf, header->protocol_id, PTK_CODEC_BIG_ENDIAN);
    if(err != PTK_OK) { return err; }

    // Length (big-endian)
    err = ptk_codec_produce_u16(buf, header->length, PTK_CODEC_BIG_ENDIAN);
    if(err != PTK_OK) { return err; }

    // Unit ID
    err = ptk_codec_produce_u8(buf, header->unit_id);
    if(err != PTK_OK) { return err; }

    return PTK_OK;
}

// Decode Modbus TCP header from buffer
ptk_err decode_modbus_tcp_header(ptk_buf *buf, modbus_tcp_header_t *header) {
    ptk_err err;

    // Transaction ID (big-endian)
    err = ptk_codec_consume_u16(buf, &header->transaction_id, PTK_CODEC_BIG_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    // Protocol ID (big-endian)
    err = ptk_codec_consume_u16(buf, &header->protocol_id, PTK_CODEC_BIG_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    // Length (big-endian)
    err = ptk_codec_consume_u16(buf, &header->length, PTK_CODEC_BIG_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    // Unit ID
    err = ptk_codec_consume_u8(buf, &header->unit_id, false);
    if(err != PTK_OK) { return err; }

    return PTK_OK;
}

// Process Read Coils request
ptk_err process_read_coils(ptk_allocator_t *alloc, ptk_buf *request_buf, ptk_buf *response_buf,
                           const modbus_tcp_header_t *req_header) {
    ptk_err err;
    uint8_t function_code;
    uint16_t start_addr, quantity;

    // Decode request PDU
    err = ptk_codec_consume_u8(request_buf, &function_code, false);
    if(err != PTK_OK) { return err; }

    err = ptk_codec_consume_u16(request_buf, &start_addr, PTK_CODEC_BIG_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    err = ptk_codec_consume_u16(request_buf, &quantity, PTK_CODEC_BIG_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    ptk_log_info("Read Coils: start=%u, quantity=%u", start_addr, quantity);

    // Validate request
    if(quantity == 0 || quantity > 2000) {
        // Send exception response
        modbus_tcp_header_t resp_header = *req_header;
        resp_header.length = 3;  // Unit ID + Exception Function Code + Exception Code

        err = encode_modbus_tcp_header(response_buf, &resp_header);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, function_code | 0x80);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, ILLEGAL_DATA_VALUE);
        if(err != PTK_OK) { return err; }

        return PTK_OK;
    }

    if(start_addr + quantity > 10000) {
        // Send exception response
        modbus_tcp_header_t resp_header = *req_header;
        resp_header.length = 3;

        err = encode_modbus_tcp_header(response_buf, &resp_header);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, function_code | 0x80);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, ILLEGAL_DATA_ADDRESS);
        if(err != PTK_OK) { return err; }

        return PTK_OK;
    }

    // Calculate response byte count
    uint8_t byte_count = (quantity + 7) / 8;

    // Build response header
    modbus_tcp_header_t resp_header = *req_header;
    resp_header.length = 2 + byte_count;  // Function code + byte count + data

    err = encode_modbus_tcp_header(response_buf, &resp_header);
    if(err != PTK_OK) { return err; }

    // Response PDU
    err = ptk_codec_produce_u8(response_buf, function_code);
    if(err != PTK_OK) { return err; }

    err = ptk_codec_produce_u8(response_buf, byte_count);
    if(err != PTK_OK) { return err; }

    // Pack coil data into bytes
    for(int byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t coil_byte = 0;
        for(int bit_idx = 0; bit_idx < 8; bit_idx++) {
            int coil_idx = start_addr + byte_idx * 8 + bit_idx;
            if(coil_idx < start_addr + quantity && g_server_state.coils[coil_idx]) { coil_byte |= (1 << bit_idx); }
        }
        err = ptk_codec_produce_u8(response_buf, coil_byte);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

// Process Read Holding Registers request
ptk_err process_read_holding_registers(ptk_allocator_t *alloc, ptk_buf *request_buf, ptk_buf *response_buf,
                                       const modbus_tcp_header_t *req_header) {
    ptk_err err;
    uint8_t function_code;
    uint16_t start_addr, quantity;

    // Decode request PDU
    err = ptk_codec_consume_u8(request_buf, &function_code, false);
    if(err != PTK_OK) { return err; }

    err = ptk_codec_consume_u16(request_buf, &start_addr, PTK_CODEC_BIG_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    err = ptk_codec_consume_u16(request_buf, &quantity, PTK_CODEC_BIG_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    ptk_log_info("Read Holding Registers: start=%u, quantity=%u", start_addr, quantity);

    // Validate request
    if(quantity == 0 || quantity > 125) {
        modbus_tcp_header_t resp_header = *req_header;
        resp_header.length = 3;

        err = encode_modbus_tcp_header(response_buf, &resp_header);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, function_code | 0x80);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, ILLEGAL_DATA_VALUE);
        if(err != PTK_OK) { return err; }

        return PTK_OK;
    }

    if(start_addr + quantity > 10000) {
        modbus_tcp_header_t resp_header = *req_header;
        resp_header.length = 3;

        err = encode_modbus_tcp_header(response_buf, &resp_header);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, function_code | 0x80);
        if(err != PTK_OK) { return err; }

        err = ptk_codec_produce_u8(response_buf, ILLEGAL_DATA_ADDRESS);
        if(err != PTK_OK) { return err; }

        return PTK_OK;
    }

    // Calculate response byte count
    uint8_t byte_count = quantity * 2;

    // Build response header
    modbus_tcp_header_t resp_header = *req_header;
    resp_header.length = 2 + byte_count;

    err = encode_modbus_tcp_header(response_buf, &resp_header);
    if(err != PTK_OK) { return err; }

    // Response PDU
    err = ptk_codec_produce_u8(response_buf, function_code);
    if(err != PTK_OK) { return err; }

    err = ptk_codec_produce_u8(response_buf, byte_count);
    if(err != PTK_OK) { return err; }

    // Send register values (big-endian)
    for(int i = 0; i < quantity; i++) {
        err = ptk_codec_produce_u16(response_buf, g_server_state.holding_regs[start_addr + i], PTK_CODEC_BIG_ENDIAN);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

// Process incoming Modbus request
ptk_err process_modbus_request(ptk_allocator_t *alloc, ptk_buf *request_buf, ptk_buf *response_buf) {
    ptk_err err;
    modbus_tcp_header_t header;

    // Decode Modbus TCP header
    err = decode_modbus_tcp_header(request_buf, &header);
    if(err != PTK_OK) {
        ptk_log_error("Failed to decode Modbus TCP header: %s", ptk_err_to_string(err));
        return err;
    }

    ptk_log_info("Modbus request: transaction_id=%u, length=%u, unit_id=%u", header.transaction_id, header.length,
                 header.unit_id);

    // Validate protocol ID
    if(header.protocol_id != 0) {
        ptk_log_error("Invalid protocol ID: %u", header.protocol_id);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Get function code (peek, don't consume yet)
    uint8_t function_code;
    err = ptk_codec_consume_u8(request_buf, &function_code, true);  // Peek
    if(err != PTK_OK) {
        ptk_log_error("No function code in request");
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    ptk_log_info("Processing function code: 0x%02X", function_code);

    // Process based on function code
    switch(function_code) {
        case READ_COILS: return process_read_coils(alloc, request_buf, response_buf, &header);

        case READ_HOLDING_REGISTERS: return process_read_holding_registers(alloc, request_buf, response_buf, &header);

        default:
            ptk_log_info("Unsupported function code: 0x%02X", function_code);

            // Send exception response
            modbus_tcp_header_t resp_header = header;
            resp_header.length = 3;

            err = encode_modbus_tcp_header(response_buf, &resp_header);
            if(err != PTK_OK) { return err; }

            err = ptk_codec_produce_u8(response_buf, function_code | 0x80);
            if(err != PTK_OK) { return err; }

            err = ptk_codec_produce_u8(response_buf, ILLEGAL_FUNCTION);
            if(err != PTK_OK) { return err; }

            return PTK_OK;
    }
}

// Handle client connection
ptk_err handle_client(ptk_allocator_t *alloc, ptk_sock *client) {
    ptk_err err;
    ptk_buf *request_buf = NULL;
    ptk_buf *response_buf = NULL;

    ptk_log_info("New client connected");

    // Create buffers
    err = ptk_buf_create(alloc, &request_buf, 1024);
    if(err != PTK_OK) {
        ptk_log_error("Failed to create request buffer: %s", ptk_err_to_string(err));
        goto cleanup;
    }

    err = ptk_buf_create(alloc, &response_buf, 1024);
    if(err != PTK_OK) {
        ptk_log_error("Failed to create response buffer: %s", ptk_err_to_string(err));
        goto cleanup;
    }

    // Client communication loop
    while(g_running) {
        // Reset buffers
        ptk_buf_reset(request_buf);
        ptk_buf_reset(response_buf);

        // Read request from client
        err = ptk_tcp_socket_recv(client, request_buf);
        if(err == PTK_ERR_CLOSED) {
            ptk_log_info("Client disconnected");
            break;
        } else if(err == PTK_ERR_ABORT) {
            ptk_log_info("Client connection aborted");
            break;
        } else if(err != PTK_OK) {
            ptk_log_error("Failed to read from client: %s", ptk_err_to_string(err));
            break;
        }

        size_t request_len;
        ptk_buf_len(&request_len, request_buf);
        if(request_len == 0) {
            continue;  // Keep-alive or empty read
        }

        ptk_log_info("Received %zu bytes from client", request_len);

        // Process the Modbus request
        err = process_modbus_request(alloc, request_buf, response_buf);
        if(err != PTK_OK) {
            ptk_log_error("Failed to process Modbus request: %s", ptk_err_to_string(err));
            continue;
        }

        // Send response back to client
        err = ptk_tcp_socket_send(client, response_buf);
        if(err != PTK_OK) {
            ptk_log_error("Failed to write response to client: %s", ptk_err_to_string(err));
            break;
        }

        size_t response_len;
        ptk_buf_len(&response_len, response_buf);
        ptk_log_info("Sent %zu bytes response to client", response_len);
    }

cleanup:
    if(request_buf) {
        // Buffer cleanup handled by allocator
    }
    if(response_buf) {
        // Buffer cleanup handled by allocator
    }

    ptk_socket_close(client);
    return err;
}

// Get default system allocator (we need to check what's available)
ptk_allocator_t *get_system_allocator(void) {
    // This is a placeholder - we need to check the actual allocator API
    static ptk_allocator_vtable_t vtable = {0};
    static ptk_allocator_t allocator = {&vtable, NULL};
    return &allocator;
}

// Initialize server state with test data
void initialize_server_state(void) {
    // Set some test coils
    g_server_state.coils[0] = true;
    g_server_state.coils[1] = false;
    g_server_state.coils[2] = true;
    g_server_state.coils[100] = true;

    // Set some test holding registers
    g_server_state.holding_regs[0] = 0x1234;
    g_server_state.holding_regs[1] = 0x5678;
    g_server_state.holding_regs[2] = 0x9ABC;
    g_server_state.holding_regs[100] = 42;
    g_server_state.holding_regs[101] = 100;

    ptk_log_info("Server state initialized with test data");
}

int main(int argc, char *argv[]) {
    int port = 502;                // Standard Modbus port
    const char *host = "0.0.0.0";  // Listen on all interfaces

    if(argc > 1) { port = atoi(argv[1]); }

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    ptk_log_info("Starting Modbus TCP server on %s:%d", host, port);

    // Initialize server state
    initialize_server_state();

    // Create allocator (using system allocator for simplicity)
    ptk_allocator_t *alloc = get_system_allocator();

    // Create server socket
    ptk_sock *server = NULL;
    ptk_err err = ptk_tcp_socket_listen(&server, host, port, 10);
    if(err != PTK_OK) {
        ptk_log_error("Failed to create server socket: %s", ptk_err_to_string(err));
        return 1;
    }

    ptk_log_info("Modbus TCP server listening on port %d", port);

    // Main server loop
    while(g_running) {
        ptk_sock *client = NULL;

        // Accept new client connection
        err = ptk_tcp_socket_accept(server, &client);
        if(err == PTK_ERR_ABORT) {
            ptk_log_info("Server accept aborted");
            break;
        } else if(err != PTK_OK) {
            ptk_log_error("Failed to accept client connection: %s", ptk_err_to_string(err));
            continue;
        }

        // Handle client in the same thread (for simplicity)
        // In a production server, you'd spawn a new thread or use async I/O
        handle_client(alloc, client);
    }

    // Cleanup
    ptk_socket_close(server);
    ptk_log_info("Modbus TCP server stopped");

    return 0;
}