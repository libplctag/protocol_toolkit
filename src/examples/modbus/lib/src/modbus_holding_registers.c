#include "modbus_internal.h"

//=============================================================================
// CLIENT FUNCTIONS - HOLDING REGISTERS READ OPERATIONS
//=============================================================================

ptk_err client_send_read_holding_register_req(modbus_connection *conn, uint16_t register_addr) {
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
        MODBUS_FC_READ_HOLDING_REGISTERS,
        register_addr,
        (uint16_t)1
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err client_send_read_holding_registers_req(modbus_connection *conn, uint16_t base_register, uint16_t num_registers) {
    if (!conn || conn->is_server || num_registers == 0 || num_registers > 125) {
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
        MODBUS_FC_READ_HOLDING_REGISTERS,
        base_register,
        num_registers
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

//=============================================================================
// CLIENT FUNCTIONS - HOLDING REGISTERS WRITE OPERATIONS
//=============================================================================

ptk_err client_send_write_holding_register_req(modbus_connection *conn, uint16_t register_addr, uint16_t register_value) {
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

    // Produce PDU: function_code, register_address, register_value
    err = ptk_buf_produce(pdu_buf, ">bww", 
        MODBUS_FC_WRITE_SINGLE_REGISTER,
        register_addr,
        register_value
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err client_send_write_holding_registers_req(modbus_connection *conn, uint16_t base_register, const modbus_register_array_t *register_values) {
    if (!conn || !register_values || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    size_t num_registers = register_values->len;
    if (num_registers == 0 || num_registers > 123) {  // Max 123 registers per Modbus spec
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

    // Produce PDU: function_code, starting_address, quantity, byte_count
    err = ptk_buf_produce(pdu_buf, ">bwwb", 
        MODBUS_FC_WRITE_MULTIPLE_REGISTERS,
        base_register,
        (uint16_t)num_registers,
        (uint8_t)(num_registers * 2)
    );

    if (err != PTK_OK) {
        return err;
    }

    // Produce register values
    for (size_t i = 0; i < num_registers; i++) {
        err = ptk_buf_produce(pdu_buf, ">w", register_values->elements[i]);
        if (err != PTK_OK) {
        return err;
    }
    }

    return modbus_send_frame(conn, pdu_buf);
}

//=============================================================================
// CLIENT FUNCTIONS - HOLDING REGISTERS READ RESPONSES
//=============================================================================

ptk_err client_recv_read_read_holding_register_resp(modbus_connection *conn, uint16_t *register_value) {
    if (!conn || !register_value || conn->is_server) {
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

    // Parse response: function_code, byte_count, register_value
    uint8_t function_code, byte_count;
    uint16_t reg_value;
    
    err = ptk_buf_consume(pdu_buf, false, ">bbw", 
        &function_code, &byte_count, &reg_value
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate response
    if (function_code != MODBUS_FC_READ_HOLDING_REGISTERS || byte_count != 2) {        return PTK_ERR_PROTOCOL_ERROR;
    }

    *register_value = reg_value;    return PTK_OK;
}

ptk_err client_recv_read_read_holding_registers_resp(modbus_connection *conn, modbus_register_array_t **register_values) {
    if (!conn || !register_values || conn->is_server) {
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
    if (function_code != MODBUS_FC_READ_HOLDING_REGISTERS || byte_count % 2 != 0) {        return PTK_ERR_PROTOCOL_ERROR;
    }

    size_t num_registers = byte_count / 2;
    
    // Create array
    modbus_register_array_t *array = ptk_alloc(conn->allocator, sizeof(modbus_register_array_t));
    if (!array) {        return PTK_ERR_NO_RESOURCES;
    }

    err = modbus_register_array_create(conn->allocator, array);
    if (err != PTK_OK) {
        ptk_free(conn->allocator, array);
    return err;
}

    // Read register values
    for (size_t i = 0; i < num_registers; i++) {
        uint16_t reg_value;
        err = ptk_buf_consume(pdu_buf, false, ">w", &reg_value);
        if (err != PTK_OK) {
            modbus_register_array_dispose(array);
            ptk_free(conn->allocator, array);
    return err;
}
        
        err = modbus_register_array_append(array, reg_value);
        if (err != PTK_OK) {
            modbus_register_array_dispose(array);
            ptk_free(conn->allocator, array);
    return err;
}
    }

    *register_values = array;    return PTK_OK;
}

//=============================================================================
// CLIENT FUNCTIONS - HOLDING REGISTERS WRITE RESPONSES
//=============================================================================

ptk_err client_recv_write_holding_register_resp(modbus_connection *conn) {
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

    err = modbus_recv_frame(conn, pdu_buf);
    if (err != PTK_OK) {
        return err;
    }

    // Parse response: function_code, register_address, register_value
    uint8_t function_code;
    uint16_t register_addr, register_value;
    
    err = ptk_buf_consume(pdu_buf, false, ">bww", 
        &function_code, &register_addr, &register_value
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate response
    if (function_code != MODBUS_FC_WRITE_SINGLE_REGISTER) {        return PTK_ERR_PROTOCOL_ERROR;
    }    return PTK_OK;
}

ptk_err client_recv_write_holding_registers_resp(modbus_connection *conn) {
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

    err = modbus_recv_frame(conn, pdu_buf);
    if (err != PTK_OK) {
        return err;
    }

    // Parse response: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;
    
    err = ptk_buf_consume(pdu_buf, false, ">bww", 
        &function_code, &starting_address, &quantity
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate response
    if (function_code != MODBUS_FC_WRITE_MULTIPLE_REGISTERS) {        return PTK_ERR_PROTOCOL_ERROR;
    }    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - HOLDING REGISTERS READ REQUESTS
//=============================================================================

ptk_err server_recv_read_holding_register_req(modbus_connection *conn, uint16_t *register_addr) {
    if (!conn || !register_addr || conn->is_server) {
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
    if (function_code != MODBUS_FC_READ_HOLDING_REGISTERS || quantity != 1) {        return PTK_ERR_INVALID_PARAM;
    }

    *register_addr = starting_address;    return PTK_OK;
}

ptk_err server_recv_read_holding_registers_req(modbus_connection *conn, uint16_t *base_register, uint16_t *num_registers) {
    if (!conn || !base_register || !num_registers || conn->is_server) {
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
    if (function_code != MODBUS_FC_READ_HOLDING_REGISTERS) {        return PTK_ERR_INVALID_PARAM;
    }

    *base_register = starting_address;
    *num_registers = quantity;    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - HOLDING REGISTERS WRITE REQUESTS
//=============================================================================

ptk_err server_recv_write_holding_register_req(modbus_connection *conn, uint16_t *register_addr, uint16_t *register_value) {
    if (!conn || !register_addr || !register_value || conn->is_server) {
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

    // Parse request: function_code, register_address, register_value
    uint8_t function_code;
    uint16_t reg_addr, reg_value;
    
    err = ptk_buf_consume(pdu_buf, false, ">bww", 
        &function_code, &reg_addr, &reg_value
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate request
    if (function_code != MODBUS_FC_WRITE_SINGLE_REGISTER) {        return PTK_ERR_INVALID_PARAM;
    }

    *register_addr = reg_addr;
    *register_value = reg_value;    return PTK_OK;
}

ptk_err server_recv_write_holding_registers_req(modbus_connection *conn, uint16_t *base_register, modbus_register_array_t **register_values) {
    if (!conn || !base_register || !register_values || conn->is_server) {
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

    // Parse request header: function_code, starting_address, quantity, byte_count
    uint8_t function_code, byte_count;
    uint16_t starting_address, quantity;
    
    err = ptk_buf_consume(pdu_buf, false, ">bwwb", 
        &function_code, &starting_address, &quantity, &byte_count
    );
    
    if (err != PTK_OK) {
        return err;
    }

    // Validate request
    if (function_code != MODBUS_FC_WRITE_MULTIPLE_REGISTERS || byte_count != quantity * 2) {        return PTK_ERR_INVALID_PARAM;
    }

    // Create array
    modbus_register_array_t *array = ptk_alloc(conn->allocator, sizeof(modbus_register_array_t));
    if (!array) {        return PTK_ERR_NO_RESOURCES;
    }

    err = modbus_register_array_create(conn->allocator, array);
    if (err != PTK_OK) {
        ptk_free(conn->allocator, array);
    return err;
}

    // Read register values
    for (size_t i = 0; i < quantity; i++) {
        uint16_t reg_value;
        err = ptk_buf_consume(pdu_buf, false, ">w", &reg_value);
        if (err != PTK_OK) {
            modbus_register_array_dispose(array);
            ptk_free(conn->allocator, array);
    return err;
}
        
        err = modbus_register_array_append(array, reg_value);
        if (err != PTK_OK) {
            modbus_register_array_dispose(array);
            ptk_free(conn->allocator, array);
    return err;
}
    }

    *base_register = starting_address;
    *register_values = array;    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - HOLDING REGISTERS READ RESPONSES
//=============================================================================

ptk_err server_send_read_holding_register_resp(modbus_connection *conn, uint16_t register_value) {
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

    // Produce response PDU: function_code, byte_count, register_value
    err = ptk_buf_produce(pdu_buf, ">bbw", 
        MODBUS_FC_READ_HOLDING_REGISTERS,
        (uint8_t)2,  // byte count
        register_value
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err server_send_read_holding_registers_resp(modbus_connection *conn, const modbus_register_array_t *register_values) {
    if (!conn || !register_values || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    size_t num_registers = register_values->len;
    if (num_registers == 0 || num_registers > 125) {
        return PTK_ERR_INVALID_PARAM;
    }

    // Create PDU buffer
    size_t pdu_size = 2 + (num_registers * 2);  // function_code + byte_count + register_values
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
        MODBUS_FC_READ_HOLDING_REGISTERS,
        (uint8_t)(num_registers * 2)
    );

    if (err != PTK_OK) {
        return err;
    }

    // Produce register values
    for (size_t i = 0; i < num_registers; i++) {
        err = ptk_buf_produce(pdu_buf, ">w", register_values->elements[i]);
        if (err != PTK_OK) {
        return err;
    }
    }

    return modbus_send_frame(conn, pdu_buf);
}

//=============================================================================
// SERVER FUNCTIONS - HOLDING REGISTERS WRITE RESPONSES
//=============================================================================

ptk_err server_send_write_holding_register_resp(modbus_connection *conn) {
    if (!conn || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    // For write single register, echo back the original request (already stored in transaction)
    // This assumes the register write was successful
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

    // Response format is identical to request for write single register
    // Note: In a real implementation, you'd store the original request values
    // For now, this is a placeholder that assumes success
    err = ptk_buf_produce(pdu_buf, ">bww", 
        MODBUS_FC_WRITE_SINGLE_REGISTER,
        (uint16_t)0,  // register address (should be stored from request)
        (uint16_t)0   // register value (should be stored from request)
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}

ptk_err server_send_write_holding_registers_resp(modbus_connection *conn) {
    if (!conn || conn->is_server) {
        return PTK_ERR_INVALID_PARAM;
    }

    // For write multiple registers, respond with function code, starting address, and quantity
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

    // Note: In a real implementation, you'd store the original request values
    // For now, this is a placeholder that assumes success
    err = ptk_buf_produce(pdu_buf, ">bww", 
        MODBUS_FC_WRITE_MULTIPLE_REGISTERS,
        (uint16_t)0,  // starting address (should be stored from request)
        (uint16_t)0   // quantity (should be stored from request)
    );

    if (err != PTK_OK) {
        return err;
    }

    return modbus_send_frame(conn, pdu_buf);
}
