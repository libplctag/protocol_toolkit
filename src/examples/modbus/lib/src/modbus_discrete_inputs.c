#include "modbus_internal.h"
#include <ptk_log.h>

//=============================================================================
// READ DISCRETE INPUTS REQUEST (0x02)
//=============================================================================

ptk_err modbus_read_discrete_inputs_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_discrete_inputs_req_t *req = (modbus_read_discrete_inputs_req_t *)obj;

    // Serialize in big-endian format (Modbus standard)
    return ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, req->function_code, ptk_buf_byte_swap_u16(req->starting_address),
                             ptk_buf_byte_swap_u16(req->quantity_of_inputs));
}

ptk_err modbus_read_discrete_inputs_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_discrete_inputs_req_t *req = (modbus_read_discrete_inputs_req_t *)obj;

    uint8_t function_code;
    uint16_t starting_address, quantity_of_inputs;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &starting_address, &quantity_of_inputs);

    if(err != PTK_OK) {
        error("Failed to deserialize read discrete inputs request");
        return err;
    }

    // Convert from big-endian and validate
    req->function_code = function_code;
    req->starting_address = ptk_buf_byte_swap_u16(starting_address);
    req->quantity_of_inputs = ptk_buf_byte_swap_u16(quantity_of_inputs);

    if(req->function_code != MODBUS_FUNC_READ_DISCRETE_INPUTS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", req->function_code, MODBUS_FUNC_READ_DISCRETE_INPUTS);
        return PTK_ERR_INVALID_PARAM;
    }

    return modbus_validate_request_params(req->starting_address, req->quantity_of_inputs, 0xFFFF,
                                          2000);  // Max 2000 inputs per spec
}

modbus_read_discrete_inputs_req_t *modbus_read_discrete_inputs_req_create(void *parent) {
    modbus_read_discrete_inputs_req_t *req =
        ptk_alloc(parent, sizeof(modbus_read_discrete_inputs_req_t), modbus_read_discrete_inputs_req_destructor);
    if(!req) {
        error("Failed to allocate read discrete inputs request");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&req->base, MODBUS_READ_DISCRETE_INPUTS_REQ_TYPE, modbus_read_discrete_inputs_req_serialize,
                         modbus_read_discrete_inputs_req_deserialize);

    // Initialize request fields
    req->function_code = MODBUS_FUNC_READ_DISCRETE_INPUTS;
    req->starting_address = 0;
    req->quantity_of_inputs = 0;

    debug("Created read discrete inputs request");
    return req;
}

void modbus_read_discrete_inputs_req_destructor(void *ptr) {
    if(!ptr) { return; }
    debug("Destroying read discrete inputs request");
}

//=============================================================================
// READ DISCRETE INPUTS RESPONSE (0x02)
//=============================================================================

ptk_err modbus_read_discrete_inputs_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_discrete_inputs_resp_t *resp = (modbus_read_discrete_inputs_resp_t *)obj;

    // Serialize header
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, resp->function_code, resp->byte_count);

    if(err != PTK_OK) {
        error("Failed to serialize read discrete inputs response header");
        return err;
    }

    // Serialize input status data
    if(resp->input_status && resp->byte_count > 0) {
        for(size_t i = 0; i < resp->byte_count; i++) {
            uint8_t byte_value;
            err = modbus_byte_array_get(resp->input_status, i, &byte_value);
            if(err != PTK_OK) {
                error("Failed to get input status byte %zu", i);
                return err;
            }

            err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, byte_value);
            if(err != PTK_OK) {
                error("Failed to serialize input status byte %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

ptk_err modbus_read_discrete_inputs_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_discrete_inputs_resp_t *resp = (modbus_read_discrete_inputs_resp_t *)obj;

    uint8_t function_code, byte_count;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &byte_count);

    if(err != PTK_OK) {
        error("Failed to deserialize read discrete inputs response header");
        return err;
    }

    resp->function_code = function_code;
    resp->byte_count = byte_count;

    if(resp->function_code != MODBUS_FUNC_READ_DISCRETE_INPUTS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", resp->function_code, MODBUS_FUNC_READ_DISCRETE_INPUTS);
        return PTK_ERR_INVALID_PARAM;
    }

    // Deserialize input status data
    if(resp->byte_count > 0) {
        if(!resp->input_status) {
            error("Input status array not initialized");
            return PTK_ERR_INVALID_PARAM;
        }

        err = modbus_byte_array_resize(resp->input_status, resp->byte_count);
        if(err != PTK_OK) {
            error("Failed to resize input status array");
            return err;
        }

        for(size_t i = 0; i < resp->byte_count; i++) {
            uint8_t byte_value;
            err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &byte_value);
            if(err != PTK_OK) {
                error("Failed to deserialize input status byte %zu", i);
                return err;
            }

            err = modbus_byte_array_set(resp->input_status, i, byte_value);
            if(err != PTK_OK) {
                error("Failed to set input status byte %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

modbus_read_discrete_inputs_resp_t *modbus_read_discrete_inputs_resp_create(void *parent) {
    modbus_read_discrete_inputs_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_read_discrete_inputs_resp_t), modbus_read_discrete_inputs_resp_destructor);
    if(!resp) {
        error("Failed to allocate read discrete inputs response");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&resp->base, MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE, modbus_read_discrete_inputs_resp_serialize,
                         modbus_read_discrete_inputs_resp_deserialize);

    // Initialize response fields
    resp->function_code = MODBUS_FUNC_READ_DISCRETE_INPUTS;
    resp->byte_count = 0;
    resp->input_status = NULL;  // Will be allocated when needed

    debug("Created read discrete inputs response");
    return resp;
}

void modbus_read_discrete_inputs_resp_destructor(void *ptr) {
    if(!ptr) { return; }

    modbus_read_discrete_inputs_resp_t *resp = (modbus_read_discrete_inputs_resp_t *)ptr;
    debug("Destroying read discrete inputs response");

    // Array cleanup is handled automatically by parent-child allocation
    if(resp->input_status) { modbus_byte_array_dispose(resp->input_status); }
}
