#include "modbus_internal.h"

//=============================================================================
// CLIENT FUNCTIONS - INPUT REGISTERS READ OPERATIONS
//=============================================================================

ptk_err client_send_read_input_register_req(modbus_connection *conn, uint16_t register_addr) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    conn->transaction_id = modbus_next_transaction_id(conn);

    // Create buffer with capacity for header + PDU
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + 5);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    // Produce PDU: function_code, starting_address, quantity
    ptk_err err = ptk_buf_produce(pdu_buf, ">bww", MODBUS_FC_READ_INPUT_REGISTERS, register_addr, (uint16_t)1);

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    err = modbus_send_frame(conn, pdu_buf);
    ptk_buf_dispose(pdu_buf);
    return err;
}

ptk_err client_send_read_input_registers_req(modbus_connection *conn, uint16_t base_register, uint16_t num_registers) {
    if(!conn || conn->is_server || num_registers == 0 || num_registers > 125) { return PTK_ERR_INVALID_PARAM; }

    conn->transaction_id = modbus_next_transaction_id(conn);

    // Create buffer with capacity for header + PDU
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + 5);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    // Produce PDU: function_code, starting_address, quantity
    ptk_err err = ptk_buf_produce(pdu_buf, ">bww", MODBUS_FC_READ_INPUT_REGISTERS, base_register, num_registers);

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    err = modbus_send_frame(conn, pdu_buf);
    ptk_buf_dispose(pdu_buf);
    return err;
}

//=============================================================================
// CLIENT FUNCTIONS - INPUT REGISTERS READ RESPONSES
//=============================================================================

ptk_err client_recv_read_input_register_resp(modbus_connection *conn, uint16_t *register_value) {
    if(!conn || !register_value || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + MODBUS_MAX_PDU_SIZE);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    ptk_err err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Parse response: function_code, byte_count, register_value
    uint8_t function_code, byte_count;
    uint16_t reg_value;

    err = ptk_buf_consume(pdu_buf, false, ">bbw", &function_code, &byte_count, &reg_value);

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Validate response
    if(function_code != MODBUS_FC_READ_INPUT_REGISTERS || byte_count != 2) {
        ptk_buf_dispose(pdu_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    *register_value = reg_value;
    ptk_buf_dispose(pdu_buf);
    return PTK_OK;
}

ptk_err client_recv_read_input_registers_resp(modbus_connection *conn, modbus_register_array_t **register_values) {
    if(!conn || !register_values || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + MODBUS_MAX_PDU_SIZE);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    ptk_err err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Parse response header: function_code, byte_count
    uint8_t function_code, byte_count;

    err = ptk_buf_consume(pdu_buf, false, ">bb", &function_code, &byte_count);

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Validate response
    if(function_code != MODBUS_FC_READ_INPUT_REGISTERS || byte_count % 2 != 0) {
        ptk_buf_dispose(pdu_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    size_t num_registers = byte_count / 2;

    // Create array
    modbus_register_array_t *array = ptk_alloc(conn->allocator, sizeof(modbus_register_array_t));
    if(!array) {
        ptk_buf_dispose(pdu_buf);
        return PTK_ERR_NO_RESOURCES;
    }

    err = modbus_register_array_create(conn->allocator, array);
    if(err != PTK_OK) {
        ptk_free(conn->allocator, array);
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Read register values
    for(size_t i = 0; i < num_registers; i++) {
        uint16_t reg_value;
        err = ptk_buf_consume(pdu_buf, false, ">w", &reg_value);
        if(err != PTK_OK) {
            modbus_register_array_dispose(array);
            ptk_free(conn->allocator, array);
            ptk_buf_dispose(pdu_buf);
            return err;
        }

        err = modbus_register_array_append(array, reg_value);
        if(err != PTK_OK) {
            modbus_register_array_dispose(array);
            ptk_free(conn->allocator, array);
            ptk_buf_dispose(pdu_buf);
            return err;
        }
    }

    *register_values = array;
    ptk_buf_dispose(pdu_buf);
    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - INPUT REGISTERS READ REQUESTS
//=============================================================================

ptk_err server_recv_read_input_register_req(modbus_connection *conn, uint16_t *register_addr) {
    if(!conn || !register_addr || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + MODBUS_MAX_PDU_SIZE);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    ptk_err err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Parse request: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;

    err = ptk_buf_consume(pdu_buf, false, ">bww", &function_code, &starting_address, &quantity);

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Validate request
    if(function_code != MODBUS_FC_READ_INPUT_REGISTERS || quantity != 1) {
        ptk_buf_dispose(pdu_buf);
        return PTK_ERR_INVALID_PARAM;
    }

    *register_addr = starting_address;
    ptk_buf_dispose(pdu_buf);
    return PTK_OK;
}

ptk_err server_recv_read_input_registers_req(modbus_connection *conn, uint16_t *base_register, uint16_t *num_registers) {
    if(!conn || !base_register || !num_registers || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + MODBUS_MAX_PDU_SIZE);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    ptk_err err = modbus_recv_frame(conn, pdu_buf);
    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Parse request: function_code, starting_address, quantity
    uint8_t function_code;
    uint16_t starting_address, quantity;

    err = ptk_buf_consume(pdu_buf, false, ">bww", &function_code, &starting_address, &quantity);

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Validate request
    if(function_code != MODBUS_FC_READ_INPUT_REGISTERS) {
        ptk_buf_dispose(pdu_buf);
        return PTK_ERR_INVALID_PARAM;
    }

    *base_register = starting_address;
    *num_registers = quantity;
    ptk_buf_dispose(pdu_buf);
    return PTK_OK;
}

//=============================================================================
// SERVER FUNCTIONS - INPUT REGISTERS READ RESPONSES
//=============================================================================

ptk_err server_send_read_input_register_resp(modbus_connection *conn, uint16_t register_value) {
    if(!conn || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + 4);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    // Produce response PDU: function_code, byte_count, register_value
    ptk_err err = ptk_buf_produce(pdu_buf, ">bbw", MODBUS_FC_READ_INPUT_REGISTERS,
                                  (uint8_t)2,  // byte count
                                  register_value);

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    err = modbus_send_frame(conn, pdu_buf);
    ptk_buf_dispose(pdu_buf);
    return err;
}

ptk_err server_send_read_input_registers_resp(modbus_connection *conn, const modbus_register_array_t *register_values) {
    if(!conn || !register_values || conn->is_server) { return PTK_ERR_INVALID_PARAM; }

    size_t num_registers = register_values->len;
    if(num_registers == 0 || num_registers > 125) { return PTK_ERR_INVALID_PARAM; }

    // Create PDU buffer
    size_t pdu_size = 2 + (num_registers * 2);  // function_code + byte_count + register_values
    ptk_buf_t *pdu_buf = ptk_buf_create(conn->allocator, MODBUS_HEADER_SIZE + pdu_size);
    if(!pdu_buf) { return PTK_ERR_NO_RESOURCES; }

    // Produce response header: function_code, byte_count
    ptk_err err = ptk_buf_produce(pdu_buf, ">bb", MODBUS_FC_READ_INPUT_REGISTERS, (uint8_t)(num_registers * 2));

    if(err != PTK_OK) {
        ptk_buf_dispose(pdu_buf);
        return err;
    }

    // Produce register values
    for(size_t i = 0; i < num_registers; i++) {
        err = ptk_buf_produce(pdu_buf, ">w", register_values->elements[i]);
        if(err != PTK_OK) {
            ptk_buf_dispose(pdu_buf);
            return err;
        }
    }

    err = modbus_send_frame(conn, pdu_buf);
    ptk_buf_dispose(pdu_buf);
    return err;
}
