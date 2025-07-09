#include "modbus_internal.h"

//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Increment transaction ID for the connection
 */
uint16_t modbus_next_transaction_id(modbus_connection *conn) { return ++conn->transaction_id; }

/**
 * @brief Send Modbus TCP frame with header and PDU
 */
ptk_err modbus_send_frame(modbus_connection *conn, ptk_buf *pdu_buf) {
    if(!conn || !pdu_buf) { return PTK_ERR_NULL_PTR; }

    size_t pdu_len = ptk_buf_len(pdu_buf);
    if(pdu_len > MODBUS_MAX_PDU_SIZE) { return PTK_ERR_BUFFER_TOO_SMALL; }

    // Check if buffer has enough capacity for header + PDU
    size_t total_len = MODBUS_HEADER_SIZE + pdu_len;
    if(ptk_buf_cap(pdu_buf) < total_len) { return PTK_ERR_BUFFER_TOO_SMALL; }

    // Move PDU data to make room for header at the beginning
    ptk_err err = ptk_buf_move_to(pdu_buf, MODBUS_HEADER_SIZE);
    if(err != PTK_OK) { return err; }

    // Set start position to beginning of buffer to add header
    err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    // Produce Modbus TCP header at the beginning: transaction_id, protocol_id, length, unit_id
    err = ptk_buf_serialize(pdu_buf, PTK_BUF_BIG_ENDIAN, conn->transaction_id,
                            (uint16_t)0,              // Protocol ID (always 0 for Modbus TCP)
                            (uint16_t)(pdu_len + 1),  // Length = PDU length + unit ID
                            conn->unit_id);

    if(err != PTK_OK) { return err; }

    // Reset start position to beginning of complete frame
    err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    // Send complete frame via TCP socket
    return ptk_tcp_socket_write(conn->socket, pdu_buf);
}

/**
 * @brief Receive Modbus TCP frame and extract PDU
 */
ptk_err modbus_recv_frame(modbus_connection *conn, ptk_buf *pdu_buf) {
    if(!conn || !pdu_buf) { return PTK_ERR_NULL_PTR; }

    // Use the shared buffer for receiving complete frame
    ptk_buf *frame_buf = conn->buffer;

    // Reset buffer for new receive operation
    ptk_err err = ptk_buf_set_start(frame_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(frame_buf, 0);
    if(err != PTK_OK) { return err; }

    // Read frame from socket
    err = ptk_tcp_socket_read(frame_buf, conn->socket);
    if(err != PTK_OK) { return err; }

    // Parse Modbus TCP header
    uint16_t transaction_id, protocol_id, length;
    uint8_t unit_id;

    err = ptk_buf_deserialize(frame_buf, false, PTK_BUF_BIG_ENDIAN, &transaction_id, &protocol_id, &length, &unit_id);

    if(err != PTK_OK) { return err; }

    // Validate header
    if(protocol_id != 0 || unit_id != conn->unit_id) { return PTK_ERR_PROTOCOL_ERROR; }

    // Store transaction ID for response
    conn->transaction_id = transaction_id;

    // Calculate PDU length
    size_t pdu_len = length - 1;  // Subtract unit ID
    if(pdu_len > MODBUS_MAX_PDU_SIZE) { return PTK_ERR_PROTOCOL_ERROR; }

    // Copy PDU data to output buffer
    uint8_t *pdu_data = ptk_buf_get_start_ptr(frame_buf);
    if(pdu_len > 0) {
        memcpy(ptk_buf_get_end_ptr(pdu_buf), pdu_data, pdu_len);
        err = ptk_buf_set_end(pdu_buf, ptk_buf_get_end(pdu_buf) + pdu_len);
    }

    return err;
}

//=============================================================================
// CONNECTION MANAGEMENT FUNCTIONS
//=============================================================================

modbus_connection *modbus_open_client(ptk_allocator_t *allocator, ptk_address_t *addr, uint8_t unit_id, ptk_buf *buffer) {
    if(!allocator || !addr || !buffer) { return NULL; }

    modbus_connection *conn = ptk_alloc(allocator, sizeof(modbus_connection));
    if(!conn) { return NULL; }

    memset(conn, 0, sizeof(modbus_connection));
    conn->allocator = allocator;
    conn->address = *addr;
    conn->unit_id = unit_id;
    conn->transaction_id = 0;
    conn->is_server = false;
    conn->is_connected = false;
    conn->buffer = buffer;

    // Connect to server
    conn->socket = ptk_tcp_socket_connect(allocator, addr);
    if(!conn->socket) {
        ptk_free(allocator, conn);
        return NULL;
    }

    conn->is_connected = true;
    info("Modbus client connected to server, unit ID: %d", unit_id);

    return conn;
}

modbus_connection *modbus_open_server(ptk_allocator_t *allocator, ptk_address_t *addr, uint8_t unit_id, ptk_buf *buffer) {
    if(!allocator || !addr || !buffer) { return NULL; }

    modbus_connection *conn = ptk_alloc(allocator, sizeof(modbus_connection));
    if(!conn) { return NULL; }

    memset(conn, 0, sizeof(modbus_connection));
    conn->allocator = allocator;
    conn->address = *addr;
    conn->unit_id = unit_id;
    conn->transaction_id = 0;
    conn->is_server = true;
    conn->is_connected = false;
    conn->buffer = buffer;

    // Start listening
    conn->socket = ptk_tcp_socket_listen(allocator, addr, 5);
    if(!conn->socket) {
        error("ptk_tcp_socket_listen failed for Modbus server");
        ptk_free(allocator, conn);
        return NULL;
    }

    info("Modbus server listening on address, unit ID: %d", unit_id);

    return conn;
}

ptk_err modbus_close(modbus_connection *conn) {
    if(!conn) { return PTK_ERR_NULL_PTR; }

    ptk_err err = PTK_OK;

    if(conn->socket) { err = ptk_socket_close(conn->socket); }

    ptk_free(conn->allocator, conn);

    info("Modbus connection closed");
    return err;
}

//=============================================================================
// SERVER FUNCTIONS
//=============================================================================

modbus_connection *server_accept_connection(modbus_connection *server_connection, ptk_buf *buffer) {
    if(!server_connection || !server_connection->is_server || !buffer) { return NULL; }

    ptk_sock *client_socket = ptk_tcp_socket_accept(server_connection->socket);
    if(!client_socket) { return NULL; }

    modbus_connection *client_conn = ptk_alloc(server_connection->allocator, sizeof(modbus_connection));
    if(!client_conn) {
        ptk_socket_close(client_socket);
        return NULL;
    }

    memset(client_conn, 0, sizeof(modbus_connection));
    client_conn->allocator = server_connection->allocator;
    client_conn->socket = client_socket;
    client_conn->unit_id = server_connection->unit_id;
    client_conn->transaction_id = 0;
    client_conn->is_server = false;  // This is now a client connection from server's perspective
    client_conn->is_connected = true;
    client_conn->buffer = buffer;

    info("Modbus server accepted client connection");

    return client_conn;
}

//=============================================================================
// SERVER FUNCTIONS - ERROR RESPONSES
//=============================================================================

ptk_err server_send_exception_resp(modbus_connection *conn, uint8_t function_code, uint8_t exception_code) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Use shared buffer for PDU
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    // Produce exception response PDU: exception_function_code, exception_code
    err = ptk_buf_serialize(pdu_buf, PTK_BUF_BIG_ENDIAN, (uint8_t)(function_code | 0x80),  // Set exception bit
                            exception_code);

    if(err != PTK_OK) { return err; }

    return modbus_send_frame(conn, pdu_buf);
}
