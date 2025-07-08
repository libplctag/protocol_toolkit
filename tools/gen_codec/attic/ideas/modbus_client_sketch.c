// Modbus Client Sketch - demonstrates usage of generated PDL headers
// This shows the key patterns for request/response with field-indexed messages

#include "modbus_basic.h"  // Generated from modbus_basic.pdl
#include "ptk_allocator.h"
#include "ptk_buf.h"
#include "ptk_log.h"
#include "ptk_socket.h"

// Example: Read 10 holding registers starting at address 100
ptk_err read_holding_registers_example(ptk_socket_t *socket) {
    ptk_allocator_t *arena_alloc = &PTK_SYSTEM_ALLOCATOR;
    ptk_err err;

    modbus_context_t *ctx = create_modbus_context();

    // ===== REQUEST SETUP =====

    // 0. set up the send buffer
    pkt_buf_t *send_buf = NULL;
    err = ptk_buf_create(malloc_alloc, &send_buf, 1024);

    // 1. Create the request message hierarchy

    read_holding_registers_request *request_msg;
    err = read_holding_registers_request_create(arena_alloc, ctx, &request_msg);
    if(err != PTK_OK) { return err; }

    // 2. Set header fields
    request_msg->starting_address = 42;
    request_msg->quantity = 10;

    // 3. encode the whole message.
    ptk_buf encoded_request = {0};
    read_holding_registers_request_encode(arena_alloc, ctx, send_buf, request_msg);

    // 4. Send via socket
    err = ptk_socket_send(socket, &send_buf);
    if(err != PTK_OK) { goto cleanup; }

    info("Sent read holding registers request: %zu bytes", buf_get_start(&send_buf));

    /* alloc_reset is a convenience macro wrapping arena_alloc->reset(arena_alloc) */
    alloc_reset(arena_alloc);

    // ===== RESPONSE RECEPTION =====

    // 5. set up receive buffer
    ptk_buf *recv_buf = NULL;
    err = ptk_buf_create(malloc_alloc, &recv_buf, 1024);
    if(err != PTK_OK) { goto cleanup_recv; }

    modbus_pdu_t *response_msg = NULL;


    do {
        err = ptk_socket_receive(socket, recv_buf);
        if(err != PTK_OK) { goto cleanup_recv; }

        info("Received response: %zu bytes", bytes_received);

        // ===== RESPONSE DECODING =====

        // 6. Create response message and decode
        err = modbus_pdu_decode(arena_alloc, ctx, &response_msg, recv_buf);
    } while(err == PTK_ERR_INSUFFICIENT_DATA);

    if(err != PTK_OK) { goto cleanup_response; }

    // 7. determine type of response:

    switch(response_msg->payload.payload_type) {
        case MODBUS_PDU_DATA_TYPE_READ_HOLDING_REGISTERS_RESPONSE:
            process_holding_reg_response(arena_alloc, ctx, response_msg->payload.payload_value.read_holding_registers);
            break;
        case MODBUS_PDU_DATA_TYPE_READ_COILS_RESPONSE:
            process_coil_response(arena_alloc, ctx, response_msg.payload.payload_value.read_coils);
            break;
    }

    /* ... */

cleanup_response:
    response_msg->vtable->dispose(arena_alloc, response_msg);
cleanup_recv:
    u8_array_dispose(receive_buffer);
cleanup:
    request_msg->vtable->dispose(arena_alloc, request_msg);
    return err;
}

// Example: Read 16 coils starting at address 200
ptk_err read_coils_example(ptk_socket_t *socket) {
    ptk_allocator_t *arena_alloc = &PTK_SYSTEM_ALLOCATOR;
    ptk_err err;

    // ===== REQUEST SETUP =====

    modbus_message_t *request_msg;
    err = modbus_message_create(arena_alloc, &request_msg);
    if(err != PTK_OK) { return err; }

    // Set header
    err = request_msg->vtable->set_header_transaction_id(arena_alloc, request_msg, 0x5678);
    if(err != PTK_OK) { goto cleanup; }

    err = request_msg->vtable->set_header_unit_id(arena_alloc, request_msg, 1);
    if(err != PTK_OK) { goto cleanup; }

    // Set function code
    err = request_msg->vtable->set_pdu_function_code(arena_alloc, request_msg, MODBUS_READ_COILS);
    if(err != PTK_OK) { goto cleanup; }

    // Set request parameters
    read_coils_request_t *req_data;
    err = request_msg->vtable->get_pdu_data_as_read_coils_request(arena_alloc, request_msg, &req_data);
    if(err != PTK_OK) { goto cleanup; }

    err = req_data->vtable->set_starting_address(arena_alloc, req_data, 200);
    if(err != PTK_OK) { goto cleanup; }

    err = req_data->vtable->set_quantity(arena_alloc, req_data, 16);
    if(err != PTK_OK) { goto cleanup; }

    err = request_msg->vtable->finalize(arena_alloc, request_msg);
    if(err != PTK_OK) { goto cleanup; }

    // ===== SOCKET TRANSMISSION =====

    u8_array_t *encoded_request;
    err = request_msg->vtable->get_encoded_data(arena_alloc, request_msg, &encoded_request);
    if(err != PTK_OK) { goto cleanup; }

    ptk_buf send_buf;
    err = ptk_buf_create(&send_buf, encoded_request);
    if(err != PTK_OK) { goto cleanup; }

    err = ptk_socket_send(socket, &send_buf);
    if(err != PTK_OK) { goto cleanup; }

    // ===== RESPONSE RECEPTION =====

    u8_array_t *receive_buffer;
    err = u8_array_create(&receive_buffer);
    if(err != PTK_OK) { goto cleanup; }

    err = u8_array_resize(receive_buffer, 1024);
    if(err != PTK_OK) { goto cleanup_recv; }

    ptk_buf recv_buf;
    err = ptk_buf_create(&recv_buf, receive_buffer);
    if(err != PTK_OK) { goto cleanup_recv; }

    size_t bytes_received;
    err = ptk_socket_receive(socket, &recv_buf, &bytes_received);
    if(err != PTK_OK) { goto cleanup_recv; }

    // ===== RESPONSE DECODING =====

    modbus_message_t *response_msg;
    err = modbus_message_create(arena_alloc, &response_msg);
    if(err != PTK_OK) { goto cleanup_recv; }

    err = response_msg->vtable->decode(arena_alloc, response_msg, &recv_buf);
    if(err != PTK_OK) { goto cleanup_response; }

    // Access coil response data
    read_coils_response_t *resp_data;
    err = response_msg->vtable->get_pdu_data_as_read_coils_response(arena_alloc, response_msg, &resp_data);
    if(err != PTK_OK) { goto cleanup_response; }

    // Get coil status bytes
    size_t byte_count;
    err = resp_data->vtable->get_coil_status_length(arena_alloc, resp_data, &byte_count);
    if(err != PTK_OK) { goto cleanup_response; }

    info("Received %zu coil status bytes:", byte_count);

    // Access individual coil status bytes
    for(size_t i = 0; i < byte_count; i++) {
        u8 coil_byte;
        err = resp_data->vtable->get_coil_status_element(arena_alloc, resp_data, i, &coil_byte);
        if(err != PTK_OK) { goto cleanup_response; }

        info("Coil byte [%zu]: 0x%02X", i, coil_byte);

        // Extract individual coil bits
        for(int bit = 0; bit < 8; bit++) {
            bool coil_state = (coil_byte >> bit) & 0x01;
            info("  Coil %zu: %s", i * 8 + bit, coil_state ? "ON" : "OFF");
        }
    }

    err = PTK_OK;

cleanup_response:
    response_msg->vtable->dispose(arena_alloc, response_msg);
cleanup_recv:
    u8_array_dispose(receive_buffer);
cleanup:
    request_msg->vtable->dispose(arena_alloc, request_msg);
    return err;
}

// Main function showing complete client workflow
int main(int argc, char *argv[]) {
    if(argc != 3) {
        printf("Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    // Initialize socket
    ptk_socket_t *socket;
    ptk_err err = ptk_socket_create_tcp(&socket, argv[1], atoi(argv[2]));
    if(err != PTK_OK) {
        error("Failed to create socket: %s", ptk_err_string(err));
        return 1;
    }

    // Connect to Modbus server
    err = ptk_socket_connect(socket);
    if(err != PTK_OK) {
        error("Failed to connect: %s", ptk_err_string(err));
        ptk_socket_destroy(socket);
        return 1;
    }

    info("Connected to Modbus server %s:%s", argv[1], argv[2]);

    // Perform read operations
    err = read_holding_registers_example(socket);
    if(err != PTK_OK) { error("Read holding registers failed: %s", ptk_err_string(err)); }

    err = read_coils_example(socket);
    if(err != PTK_OK) { error("Read coils failed: %s", ptk_err_string(err)); }

    // Cleanup
    ptk_socket_destroy(socket);
    info("Modbus client finished");
    return 0;
}