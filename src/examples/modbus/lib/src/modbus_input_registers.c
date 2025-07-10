#include "modbus_internal.h"
#include <ptk_log.h>

//=============================================================================
// READ INPUT REGISTERS REQUEST (0x04)
//=============================================================================

ptk_err modbus_read_input_registers_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_input_registers_req_t *req = (modbus_read_input_registers_req_t *)obj;

    // Serialize in big-endian format (Modbus standard)
    return ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, req->function_code, ptk_buf_byte_swap_u16(req->starting_address),
                             ptk_buf_byte_swap_u16(req->quantity_of_registers));
}

ptk_err modbus_read_input_registers_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_input_registers_req_t *req = (modbus_read_input_registers_req_t *)obj;

    uint8_t function_code;
    uint16_t starting_address, quantity_of_registers;

    ptk_err err =
        ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &starting_address, &quantity_of_registers);

    if(err != PTK_OK) {
        error("Failed to deserialize read input registers request");
        return err;
    }

    // Convert from big-endian and validate
    req->function_code = function_code;
    req->starting_address = ptk_buf_byte_swap_u16(starting_address);
    req->quantity_of_registers = ptk_buf_byte_swap_u16(quantity_of_registers);

    if(req->function_code != MODBUS_FUNC_READ_INPUT_REGISTERS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", req->function_code, MODBUS_FUNC_READ_INPUT_REGISTERS);
        return PTK_ERR_INVALID_PARAM;
    }

    return modbus_validate_request_params(req->starting_address, req->quantity_of_registers, 0xFFFF,
                                          125);  // Max 125 registers per spec
}

modbus_read_input_registers_req_t *modbus_read_input_registers_req_create(void *parent) {
    modbus_read_input_registers_req_t *req =
        ptk_alloc(parent, sizeof(modbus_read_input_registers_req_t), modbus_read_input_registers_req_destructor);
    if(!req) {
        error("Failed to allocate read input registers request");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&req->base, MODBUS_READ_INPUT_REGISTERS_REQ_TYPE, modbus_read_input_registers_req_serialize,
                         modbus_read_input_registers_req_deserialize);

    // Initialize request fields
    req->function_code = MODBUS_FUNC_READ_INPUT_REGISTERS;
    req->starting_address = 0;
    req->quantity_of_registers = 0;

    debug("Created read input registers request");
    return req;
}

void modbus_read_input_registers_req_destructor(void *ptr) {
    if(!ptr) { return; }
    debug("Destroying read input registers request");
}

//=============================================================================
// READ INPUT REGISTERS RESPONSE (0x04)
//=============================================================================

ptk_err modbus_read_input_registers_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_input_registers_resp_t *resp = (modbus_read_input_registers_resp_t *)obj;

    // Serialize header
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, resp->function_code, resp->byte_count);

    if(err != PTK_OK) {
        error("Failed to serialize read input registers response header");
        return err;
    }

    // Serialize register values (each register is 2 bytes, big-endian)
    if(resp->register_values && resp->byte_count > 0) {
        size_t num_registers = resp->byte_count / 2;
        for(size_t i = 0; i < num_registers; i++) {
            uint16_t register_value;
            err = modbus_register_array_get(resp->register_values, i, &register_value);
            if(err != PTK_OK) {
                error("Failed to get register value %zu", i);
                return err;
            }

            // Convert to big-endian and serialize
            err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, ptk_buf_byte_swap_u16(register_value));
            if(err != PTK_OK) {
                error("Failed to serialize register value %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

ptk_err modbus_read_input_registers_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_input_registers_resp_t *resp = (modbus_read_input_registers_resp_t *)obj;

    uint8_t function_code, byte_count;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &byte_count);

    if(err != PTK_OK) {
        error("Failed to deserialize read input registers response header");
        return err;
    }

    resp->function_code = function_code;
    resp->byte_count = byte_count;

    if(resp->function_code != MODBUS_FUNC_READ_INPUT_REGISTERS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", resp->function_code, MODBUS_FUNC_READ_INPUT_REGISTERS);
        return PTK_ERR_INVALID_PARAM;
    }

    // Validate byte count (must be even for 16-bit registers)
    if(resp->byte_count % 2 != 0) {
        error("Invalid byte count: %u (must be even for 16-bit registers)", resp->byte_count);
        return PTK_ERR_INVALID_PARAM;
    }

    // Deserialize register values
    if(resp->byte_count > 0) {
        if(!resp->register_values) {
            error("Register values array not initialized");
            return PTK_ERR_INVALID_PARAM;
        }

        size_t num_registers = resp->byte_count / 2;
        err = modbus_register_array_resize(resp->register_values, num_registers);
        if(err != PTK_OK) {
            error("Failed to resize register values array");
            return err;
        }

        for(size_t i = 0; i < num_registers; i++) {
            uint16_t register_value;
            err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &register_value);
            if(err != PTK_OK) {
                error("Failed to deserialize register value %zu", i);
                return err;
            }

            // Convert from big-endian
            register_value = ptk_buf_byte_swap_u16(register_value);

            err = modbus_register_array_set(resp->register_values, i, register_value);
            if(err != PTK_OK) {
                error("Failed to set register value %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

modbus_read_input_registers_resp_t *modbus_read_input_registers_resp_create(void *parent) {
    modbus_read_input_registers_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_read_input_registers_resp_t), modbus_read_input_registers_resp_destructor);
    if(!resp) {
        error("Failed to allocate read input registers response");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&resp->base, MODBUS_READ_INPUT_REGISTERS_RESP_TYPE, modbus_read_input_registers_resp_serialize,
                         modbus_read_input_registers_resp_deserialize);

    // Initialize response fields
    resp->function_code = MODBUS_FUNC_READ_INPUT_REGISTERS;
    resp->byte_count = 0;
    resp->register_values = NULL;  // Will be allocated when needed

    debug("Created read input registers response");
    return resp;
}

void modbus_read_input_registers_resp_destructor(void *ptr) {
    if(!ptr) { return; }

    modbus_read_input_registers_resp_t *resp = (modbus_read_input_registers_resp_t *)ptr;
    debug("Destroying read input registers response");

    // Array cleanup is handled automatically by parent-child allocation
    if(resp->register_values) { modbus_register_array_dispose(resp->register_values); }
}
