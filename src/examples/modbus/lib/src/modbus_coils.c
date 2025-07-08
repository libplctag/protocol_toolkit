#include "modbus_internal.h"

//=============================================================================
// CLIENT FUNCTIONS - COILS READ OPERATIONS
//=============================================================================

ptk_err client_send_read_coil_req(modbus_connection *conn, uint16_t coil_addr) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    conn->transaction_id = modbus_next_transaction_id(conn);

    // Use shared buffer for PDU

    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation

    ptk_err err = ptk_buf_set_start(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    err = ptk_buf_set_end(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    // Produce PDU: function_code, starting_address, quantity
    err = ptk_buf_produce(pdu_buf, ">bww", MODBUS_FC_READ_COILS, coil_addr, (uint16_t)1);

    if(err != PTK_OK) { return err; }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err client_send_read_coils_req(modbus_connection *conn, uint16_t base_coil, uint16_t num_coils) {
    if(!conn || conn->is_server || num_coils == 0 || num_coils > 2000) { return PTK_ERR_INVALID_PARAM; }

    conn->transaction_id = modbus_next_transaction_id(conn);

    // Use shared buffer for PDU

    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation

    ptk_err err = ptk_buf_set_start(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    err = ptk_buf_set_end(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    // Produce PDU: function_code, starting_address, quantity
    err = ptk_buf_produce(pdu_buf, ">bww", MODBUS_FC_READ_COILS, base_coil, num_coils);

    if(err != PTK_OK) { return err; }

    return modbus_send_frame(conn, pdu_buf);
}

//=============================================================================
// CLIENT FUNCTIONS - COILS WRITE OPERATIONS
//=============================================================================

ptk_err client_send_write_coil_req(modbus_connection *conn, uint16_t coil_addr, bool coil_value) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    conn->transaction_id = modbus_next_transaction_id(conn);

    // Use shared buffer for PDU

    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation

    ptk_err err = ptk_buf_set_start(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    err = ptk_buf_set_end(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    // Produce PDU: function_code, coil_address, coil_value (0x0000 for OFF, 0xFF00 for ON)
    err = ptk_buf_produce(pdu_buf, ">bww", MODBUS_FC_WRITE_SINGLE_COIL, coil_addr, coil_value ? 0xFF00 : 0x0000);

    if(err != PTK_OK) { return err; }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err client_send_write_coils_req(modbus_connection *conn, uint16_t base_coil, const modbus_bool_array_t *coil_values) {
    if(!conn || !coil_values || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    size_t num_coils = coil_values->len;
    if(num_coils == 0 || num_coils > 1968) {  // Max 1968 coils per Modbus spec
        return PTK_ERR_INVALID_PARAM;
    }

    conn->transaction_id = modbus_next_transaction_id(conn);

    // Calculate number of bytes needed for packed coils
    size_t byte_count = (num_coils + 7) / 8;  // Round up to nearest byte

    // Use shared buffer for PDU

    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation

    ptk_err err = ptk_buf_set_start(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    err = ptk_buf_set_end(pdu_buf, 0);

    if(err != PTK_OK) { return err; }

    // Produce PDU: function_code, starting_address, quantity, byte_count
    err = ptk_buf_produce(pdu_buf, ">bwwb", MODBUS_FC_WRITE_MULTIPLE_COILS, base_coil, (uint16_t)num_coils, (uint8_t)byte_count);

    if(err != PTK_OK) { return err; }

    // Pack coils into bytes (LSB first)
    for(size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t packed_byte = 0;
        for(size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            size_t coil_idx = byte_idx * 8 + bit_idx;
            if(coil_idx < num_coils && coil_values->elements[coil_idx]) { packed_byte |= (1 << bit_idx); }
        }

        err = ptk_buf_produce(pdu_buf, ">b", packed_byte);
        if(err != PTK_OK) { return err; }
    }

    return modbus_send_frame(conn, pdu_buf);
}

//=============================================================================
// CLIENT FUNCTIONS - COILS READ RESPONSES
//=============================================================================

ptk_err client_recv_read_coil_resp(modbus_connection *conn, bool *coil_value) {
    if(!conn || !coil_value || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse response: function_code, byte_count, coil_status
    uint8_t function_code, byte_count, coil_status;

    err = ptk_buf_consume(pdu_buf, false, ">bbb", &function_code, &byte_count, &coil_status);

    if(err != PTK_OK) { return err; }

    // Validate response
    if(function_code != MODBUS_FC_READ_COILS || byte_count != 1) { return PTK_ERR_PROTOCOL_ERROR; }

    *coil_value = (coil_status & 0x01) != 0;
    return PTK_OK;
}

ptk_err client_recv_read_coils_resp(modbus_connection *conn, modbus_bool_array_t **coil_values) {
    if(!conn || !coil_values || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse response header: function_code, byte_count
    uint8_t function_code, byte_count;

    err = ptk_buf_consume(pdu_buf, false, ">bb", &function_code, &byte_count);

    if(err != PTK_OK) { return err; }

    // Validate response
    if(function_code != MODBUS_FC_READ_COILS) { return PTK_ERR_PROTOCOL_ERROR; }

    // Create array
    modbus_bool_array_t *array = ptk_alloc(conn->allocator, sizeof(modbus_bool_array_t));
    if(!array) { return PTK_ERR_NO_RESOURCES; }

    err = modbus_bool_array_create(conn->allocator, array);
    if(err != PTK_OK) {
        ptk_free(conn->allocator, array);
        return err;
    }

    // Read packed boolean values
    for(size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t packed_byte;
        err = ptk_buf_consume(pdu_buf, false, ">b", &packed_byte);
        if(err != PTK_OK) {
            modbus_bool_array_dispose(array);
            ptk_free(conn->allocator, array);
            return err;
        }

        // Unpack bits (LSB first)
        for(size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            bool coil_state = (packed_byte & (1 << bit_idx)) != 0;
            err = modbus_bool_array_append(array, coil_state);
            if(err != PTK_OK) {
                modbus_bool_array_dispose(array);
                ptk_free(conn->allocator, array);
                return err;
            }
        }
    }

    *coil_values = array;
    return PTK_OK;
}

//=============================================================================
// CLIENT FUNCTIONS - COILS WRITE RESPONSES
//=============================================================================

ptk_err client_recv_write_coil_resp(modbus_connection *conn) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse response: function_code, coil_address, coil_value
    uint8_t function_code;
    uint16_t coil_addr, coil_value;

    err = ptk_buf_consume(pdu_buf, false, ">bww", &function_code, &coil_addr, &coil_value);

    if(err != PTK_OK) { return err; }

    // Validate response
    if(function_code != MODBUS_FC_WRITE_SINGLE_COIL) { return PTK_ERR_PROTOCOL_ERROR; }
    return PTK_OK;
}

ptk_err client_recv_write_coils_resp(modbus_connection *conn) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse response: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;

    err = ptk_buf_consume(pdu_buf, false, ">bww", &function_code, &starting_address, &quantity);

    if(err != PTK_OK) { return err; }

    // Validate response
    if(function_code != MODBUS_FC_WRITE_MULTIPLE_COILS) { return PTK_ERR_PROTOCOL_ERROR; }
    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - COILS READ REQUESTS
//=============================================================================

ptk_err server_recv_read_coil_req(modbus_connection *conn, uint16_t *coil_addr) {
    if(!conn || !coil_addr || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse request: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;

    err = ptk_buf_consume(pdu_buf, false, ">bww", &function_code, &starting_address, &quantity);

    if(err != PTK_OK) { return err; }

    // Validate request
    if(function_code != MODBUS_FC_READ_COILS || quantity != 1) { return PTK_ERR_INVALID_PARAM; }

    *coil_addr = starting_address;
    return PTK_OK;
}

ptk_err server_recv_read_coils_req(modbus_connection *conn, uint16_t *base_coil, uint16_t *num_coils) {
    if(!conn || !base_coil || !num_coils || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse request: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;

    err = ptk_buf_consume(pdu_buf, false, ">bww", &function_code, &starting_address, &quantity);

    if(err != PTK_OK) { return err; }

    // Validate request
    if(function_code != MODBUS_FC_READ_COILS) { return PTK_ERR_INVALID_PARAM; }

    *base_coil = starting_address;
    *num_coils = quantity;
    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - COILS WRITE REQUESTS
//=============================================================================

ptk_err server_recv_write_coil_req(modbus_connection *conn, uint16_t *coil_addr, bool *coil_value) {
    if(!conn || !coil_addr || !coil_value || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse request: function_code, coil_address, coil_value
    uint8_t function_code;
    uint16_t c_addr, c_value;

    err = ptk_buf_consume(pdu_buf, false, ">bww", &function_code, &c_addr, &c_value);

    if(err != PTK_OK) { return err; }

    // Validate request
    if(function_code != MODBUS_FC_WRITE_SINGLE_COIL) { return PTK_ERR_INVALID_PARAM; }

    // Validate coil value (must be 0x0000 for OFF or 0xFF00 for ON)
    if(c_value != 0x0000 && c_value != 0xFF00) { return PTK_ERR_INVALID_PARAM; }

    *coil_addr = c_addr;
    *coil_value = (c_value == 0xFF00);
    return PTK_OK;
}

ptk_err server_recv_write_coils_req(modbus_connection *conn, uint16_t *base_coil, modbus_bool_array_t **coil_values) {
    if(!conn || !base_coil || !coil_values || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) { return err; }

    // Parse request header: function_code, starting_address, quantity, byte_count
    uint8_t function_code, byte_count;
    uint16_t starting_address, quantity;

    err = ptk_buf_consume(pdu_buf, false, ">bwwb", &function_code, &starting_address, &quantity, &byte_count);

    if(err != PTK_OK) { return err; }

    // Validate request
    if(function_code != MODBUS_FC_WRITE_MULTIPLE_COILS) { return PTK_ERR_INVALID_PARAM; }

    // Create array
    modbus_bool_array_t *array = ptk_alloc(conn->allocator, sizeof(modbus_bool_array_t));
    if(!array) { return PTK_ERR_NO_RESOURCES; }

    err = modbus_bool_array_create(conn->allocator, array);
    if(err != PTK_OK) {
        ptk_free(conn->allocator, array);
        return err;
    }

    // Read packed coil values
    for(size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t packed_byte;
        err = ptk_buf_consume(pdu_buf, false, ">b", &packed_byte);
        if(err != PTK_OK) {
            modbus_bool_array_dispose(array);
            ptk_free(conn->allocator, array);
            return err;
        }

        // Unpack bits (LSB first)
        for(size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            size_t coil_idx = byte_idx * 8 + bit_idx;
            if(coil_idx < quantity) {
                bool coil_state = (packed_byte & (1 << bit_idx)) != 0;
                err = modbus_bool_array_append(array, coil_state);
                if(err != PTK_OK) {
                    modbus_bool_array_dispose(array);
                    ptk_free(conn->allocator, array);
                    return err;
                }
            }
        }
    }

    *base_coil = starting_address;
    *coil_values = array;
    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - COILS READ RESPONSES
//=============================================================================

ptk_err server_send_read_coil_resp(modbus_connection *conn, bool coil_value) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    // Produce response PDU: function_code, byte_count, coil_status
    err = ptk_buf_produce(pdu_buf, ">bbb", MODBUS_FC_READ_COILS,
                          (uint8_t)1,  // byte count
                          coil_value ? 0x01 : 0x00);

    if(err != PTK_OK) { return err; }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err server_send_read_coils_resp(modbus_connection *conn, const modbus_bool_array_t *coil_values) {
    if(!conn || !coil_values || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    size_t num_coils = coil_values->len;
    if(num_coils == 0 || num_coils > 2000) { return PTK_ERR_INVALID_PARAM; }

    // Calculate byte count (8 bits per byte)
    size_t byte_count = (num_coils + 7) / 8;

    // Create PDU buffer
    size_t pdu_size = 2 + byte_count;  // function_code + byte_count + coil_data
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    // Produce response header: function_code, byte_count
    err = ptk_buf_produce(pdu_buf, ">bb", MODBUS_FC_READ_COILS, (uint8_t)byte_count);

    if(err != PTK_OK) { return err; }

    // Pack coils into bytes (LSB first)
    for(size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t packed_byte = 0;
        for(size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            size_t coil_idx = byte_idx * 8 + bit_idx;
            if(coil_idx < num_coils && coil_values->elements[coil_idx]) { packed_byte |= (1 << bit_idx); }
        }

        err = ptk_buf_produce(pdu_buf, ">b", packed_byte);
        if(err != PTK_OK) { return err; }
    }

    return modbus_send_frame(conn, pdu_buf);
}

//=============================================================================
// SERVER FUNCTIONS - COILS WRITE RESPONSES
//=============================================================================

ptk_err server_send_write_coil_resp(modbus_connection *conn) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // For write single coil, echo back the original request
    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    // Response format is identical to request for write single coil
    // Note: In a real implementation, you'd store the original request values
    err = ptk_buf_produce(pdu_buf, ">bww", MODBUS_FC_WRITE_SINGLE_COIL,
                          (uint16_t)0,  // coil address (should be stored from request)
                          (uint16_t)0   // coil value (should be stored from request)
    );

    if(err != PTK_OK) { return err; }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err server_send_write_coils_resp(modbus_connection *conn) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // For write multiple coils, respond with function code, starting address, and quantity
    // Create PDU buffer
    ptk_buf *pdu_buf = conn->buffer;

    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if(err != PTK_OK) { return err; }
    err = ptk_buf_set_end(pdu_buf, 0);
    if(err != PTK_OK) { return err; }

    // Note: In a real implementation, you'd store the original request values
    err = ptk_buf_produce(pdu_buf, ">bww", MODBUS_FC_WRITE_MULTIPLE_COILS,
                          (uint16_t)0,  // starting address (should be stored from request)
                          (uint16_t)0   // quantity (should be stored from request)
    );

    if(err != PTK_OK) { return err; }

    return modbus_send_frame(conn, pdu_buf);
}
