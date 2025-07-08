#include "modbus_internal.h"

//=============================================================================
// CLIENT FUNCTIONS - DISCRETE INPUTS READ OPERATIONS
//=============================================================================

ptk_err client_send_read_discrete_input_req(modbus_connection *conn, uint16_t input_addr) {
    if (!conn || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    conn->transaction_id = modbus_next_transaction_id(conn);
    
    // Use shared buffer for PDU

    ptk_buf_t *pdu_buf = conn->buffer;

    // Reset buffer for new operation

    ptk_err err = ptk_buf_set_start(pdu_buf, 0);

    if (err != PTK_OK) {
        return err;
    }

    err = ptk_buf_set_end(pdu_buf, 0);

    if (err != PTK_OK) {
        return err;
    }

    // Produce PDU: function_code, starting_address, quantity
    err = ptk_buf_produce(pdu_buf, ">bww", 
        MODBUS_FC_READ_DISCRETE_INPUTS,
        input_addr,
        (uint16_t)1
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err client_send_read_discrete_inputs_req(modbus_connection *conn, uint16_t base_input, uint16_t num_inputs) {
    if (!conn || conn->is_server || num_inputs == 0 || num_inputs > 2000) {
        return PTK_ERR_INVALID_PARAM;
    }

    conn->transaction_id = modbus_next_transaction_id(conn);
    
    // Use shared buffer for PDU

    ptk_buf_t *pdu_buf = conn->buffer;

    // Reset buffer for new operation

    ptk_err err = ptk_buf_set_start(pdu_buf, 0);

    if (err != PTK_OK) {
        return err;
    }

    err = ptk_buf_set_end(pdu_buf, 0);

    if (err != PTK_OK) {
        return err;
    }

    // Produce PDU: function_code, starting_address, quantity
    err = ptk_buf_produce(pdu_buf, ">bww", 
        MODBUS_FC_READ_DISCRETE_INPUTS,
        base_input,
        num_inputs
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

//=============================================================================
// CLIENT FUNCTIONS - DISCRETE INPUTS READ RESPONSES
//=============================================================================

ptk_err client_recv_read_discrete_input_resp(modbus_connection *conn, bool *input_value) {
    if (!conn || !input_value || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = conn->buffer;
    
    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }
    err = ptk_buf_set_end(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }

    err = modbus_recv_frame(conn, pdu_buf);
    if (err != PTK_OK) {
        return err;
    }

    // Parse response: function_code, byte_count, input_status
    uint8_t function_code, byte_count, input_status;
    
    err = ptk_buf_consume(pdu_buf, false, ">bbb", 
        &function_code, &byte_count, &input_status
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate response
    if (function_code != MODBUS_FC_READ_DISCRETE_INPUTS || byte_count != 1) {        return PTK_ERR_PROTOCOL_ERROR;
    }

    *input_value = (input_status & 0x01) != 0;    return PTK_OK;
}

ptk_err client_recv_read_discrete_inputs_resp(modbus_connection *conn, modbus_bool_array_t **input_values) {
    if (!conn || !input_values || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = conn->buffer;
    
    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }
    err = ptk_buf_set_end(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }

    err = modbus_recv_frame(conn, pdu_buf);
    if (err != PTK_OK) {
        return err;
    }

    // Parse response header: function_code, byte_count
    uint8_t function_code, byte_count;
    
    err = ptk_buf_consume(pdu_buf, false, ">bb", 
        &function_code, &byte_count
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate response
    if (function_code != MODBUS_FC_READ_DISCRETE_INPUTS) {        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Create array
    modbus_bool_array_t *array = ptk_alloc(conn->allocator, sizeof(modbus_bool_array_t));
    if (!array) {        return PTK_ERR_NO_RESOURCES;
    }

    err = modbus_bool_array_create(conn->allocator, array);
    if (err != PTK_OK) {
        ptk_free(conn->allocator, array);
    return err;
}

    // Read packed boolean values
    for (size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t packed_byte;
        err = ptk_buf_consume(pdu_buf, false, ">b", &packed_byte);
        if (err != PTK_OK) {
            modbus_bool_array_dispose(array);
            ptk_free(conn->allocator, array);
    return err;
}
        
        // Unpack bits (LSB first)
        for (size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            bool input_state = (packed_byte & (1 << bit_idx)) != 0;
            err = modbus_bool_array_append(array, input_state);
            if (err != PTK_OK) {
                modbus_bool_array_dispose(array);
                ptk_free(conn->allocator, array);
    return err;
}
        }
    }

    *input_values = array;    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - DISCRETE INPUTS READ REQUESTS
//=============================================================================

ptk_err server_recv_read_discrete_input_req(modbus_connection *conn, uint16_t *input_addr) {
    if (!conn || !input_addr || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = conn->buffer;
    
    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }
    err = ptk_buf_set_end(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }

    err = modbus_recv_frame(conn, pdu_buf);
    if (err != PTK_OK) {
        return err;
    }

    // Parse request: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;
    
    err = ptk_buf_consume(pdu_buf, false, ">bww", 
        &function_code, &starting_address, &quantity
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate request
    if (function_code != MODBUS_FC_READ_DISCRETE_INPUTS || quantity != 1) {        return PTK_ERR_INVALID_PARAM;
    }

    *input_addr = starting_address;    return PTK_OK;
}

ptk_err server_recv_read_discrete_inputs_req(modbus_connection *conn, uint16_t *base_input, uint16_t *num_inputs) {
    if (!conn || !base_input || !num_inputs || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = conn->buffer;
    
    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }
    err = ptk_buf_set_end(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }

    err = modbus_recv_frame(conn, pdu_buf);
    if (err != PTK_OK) {
        return err;
    }

    // Parse request: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;
    
    err = ptk_buf_consume(pdu_buf, false, ">bww", 
        &function_code, &starting_address, &quantity
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate request
    if (function_code != MODBUS_FC_READ_DISCRETE_INPUTS) {        return PTK_ERR_INVALID_PARAM;
    }

    *base_input = starting_address;
    *num_inputs = quantity;    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - DISCRETE INPUTS READ RESPONSES
//=============================================================================

ptk_err server_send_read_discrete_input_resp(modbus_connection *conn, bool input_value) {
    if (!conn || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = conn->buffer;
    
    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }
    err = ptk_buf_set_end(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }

    // Produce response PDU: function_code, byte_count, input_status
    err = ptk_buf_produce(pdu_buf, ">bbb", 
        MODBUS_FC_READ_DISCRETE_INPUTS,
        (uint8_t)1,  // byte count
        input_value ? 0x01 : 0x00
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err server_send_read_discrete_inputs_resp(modbus_connection *conn, const modbus_bool_array_t *input_values) {
    if (!conn || !input_values || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    size_t num_inputs = input_values->len;
    if (num_inputs == 0 || num_inputs > 2000) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Calculate byte count (8 bits per byte)
    size_t byte_count = (num_inputs + 7) / 8;

    // Create PDU buffer
    size_t pdu_size = 2 + byte_count;  // function_code + byte_count + input_data
    ptk_buf_t *pdu_buf = conn->buffer;
    
    // Reset buffer for new operation
    ptk_err err = ptk_buf_set_start(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }
    err = ptk_buf_set_end(pdu_buf, 0);
    if (err != PTK_OK) {
        return err;
    }

    // Produce response header: function_code, byte_count
    err = ptk_buf_produce(pdu_buf, ">bb", 
        MODBUS_FC_READ_DISCRETE_INPUTS,
        (uint8_t)byte_count
    );

    if (err != PTK_OK) {
        return err;
    }

    // Pack inputs into bytes (LSB first)
    for (size_t byte_idx = 0; byte_idx < byte_count; byte_idx++) {
        uint8_t packed_byte = 0;
        for (size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            size_t input_idx = byte_idx * 8 + bit_idx;
            if (input_idx < num_inputs && input_values->elements[input_idx]) {
                packed_byte |= (1 << bit_idx);
            }
        }
        
        err = ptk_buf_produce(pdu_buf, ">b", packed_byte);
        if (err != PTK_OK) {
        return err;
    }
    }

    return modbus_send_frame(conn, pdu_buf);
}
