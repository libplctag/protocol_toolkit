#include "modbus_internal.h"
#include <ptk_log.h>

//=============================================================================
// READ COILS REQUEST (0x01)
//=============================================================================

ptk_err modbus_read_coils_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_coils_req_t *req = (modbus_read_coils_req_t *)obj;

    // Serialize in big-endian format (Modbus standard)
    return ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, req->function_code, ptk_buf_byte_swap_u16(req->starting_address),
                             ptk_buf_byte_swap_u16(req->quantity_of_coils));
}

ptk_err modbus_read_coils_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_coils_req_t *req = (modbus_read_coils_req_t *)obj;

    uint8_t function_code;
    uint16_t starting_address, quantity_of_coils;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &starting_address, &quantity_of_coils);

    if(err != PTK_OK) {
        error("Failed to deserialize read coils request");
        return err;
    }

    // Convert from big-endian and validate
    req->function_code = function_code;
    req->starting_address = ptk_buf_byte_swap_u16(starting_address);
    req->quantity_of_coils = ptk_buf_byte_swap_u16(quantity_of_coils);

    if(req->function_code != MODBUS_FUNC_READ_COILS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", req->function_code, MODBUS_FUNC_READ_COILS);
        return PTK_ERR_INVALID_PARAM;
    }

    return modbus_validate_request_params(req->starting_address, req->quantity_of_coils, 0xFFFF,
                                          2000);  // Max 2000 coils per spec
}

modbus_read_coils_req_t *modbus_read_coils_req_create(void *parent) {
    modbus_read_coils_req_t *req = ptk_alloc(parent, sizeof(modbus_read_coils_req_t), modbus_read_coils_req_destructor);
    if(!req) {
        error("Failed to allocate read coils request");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&req->base, MODBUS_READ_COILS_REQ_TYPE, modbus_read_coils_req_serialize,
                         modbus_read_coils_req_deserialize);

    // Initialize request fields
    req->function_code = MODBUS_FUNC_READ_COILS;
    req->starting_address = 0;
    req->quantity_of_coils = 0;

    debug("Created read coils request");
    return req;
}

void modbus_read_coils_req_destructor(void *ptr) {
    if(!ptr) { return; }
    debug("Destroying read coils request");
}

//=============================================================================
// READ COILS RESPONSE (0x01)
//=============================================================================

ptk_err modbus_read_coils_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_coils_resp_t *resp = (modbus_read_coils_resp_t *)obj;

    // Serialize header
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, resp->function_code, resp->byte_count);

    if(err != PTK_OK) {
        error("Failed to serialize read coils response header");
        return err;
    }

    // Serialize coil status data
    if(resp->coil_status && resp->byte_count > 0) {
        for(size_t i = 0; i < resp->byte_count; i++) {
            uint8_t byte_value;
            err = modbus_byte_array_get(resp->coil_status, i, &byte_value);
            if(err != PTK_OK) {
                error("Failed to get coil status byte %zu", i);
                return err;
            }

            err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, byte_value);
            if(err != PTK_OK) {
                error("Failed to serialize coil status byte %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

ptk_err modbus_read_coils_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_read_coils_resp_t *resp = (modbus_read_coils_resp_t *)obj;

    uint8_t function_code, byte_count;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &byte_count);

    if(err != PTK_OK) {
        error("Failed to deserialize read coils response header");
        return err;
    }

    resp->function_code = function_code;
    resp->byte_count = byte_count;

    if(resp->function_code != MODBUS_FUNC_READ_COILS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", resp->function_code, MODBUS_FUNC_READ_COILS);
        return PTK_ERR_INVALID_PARAM;
    }

    // Deserialize coil status data
    if(resp->byte_count > 0) {
        if(!resp->coil_status) {
            error("Coil status array not initialized");
            return PTK_ERR_INVALID_PARAM;
        }

        err = modbus_byte_array_resize(resp->coil_status, resp->byte_count);
        if(err != PTK_OK) {
            error("Failed to resize coil status array");
            return err;
        }

        for(size_t i = 0; i < resp->byte_count; i++) {
            uint8_t byte_value;
            err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &byte_value);
            if(err != PTK_OK) {
                error("Failed to deserialize coil status byte %zu", i);
                return err;
            }

            err = modbus_byte_array_set(resp->coil_status, i, byte_value);
            if(err != PTK_OK) {
                error("Failed to set coil status byte %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

modbus_read_coils_resp_t *modbus_read_coils_resp_create(void *parent) {
    modbus_read_coils_resp_t *resp = ptk_alloc(parent, sizeof(modbus_read_coils_resp_t), modbus_read_coils_resp_destructor);
    if(!resp) {
        error("Failed to allocate read coils response");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&resp->base, MODBUS_READ_COILS_RESP_TYPE, modbus_read_coils_resp_serialize,
                         modbus_read_coils_resp_deserialize);

    // Initialize response fields
    resp->function_code = MODBUS_FUNC_READ_COILS;
    resp->byte_count = 0;

    // Create coil status array as child
    resp->coil_status = ptk_alloc(resp, sizeof(modbus_byte_array_t), NULL);
    if(!resp->coil_status) {
        error("Failed to allocate coil status array");
        ptk_free(resp);
        return NULL;
    }

    // Initialize the array - we need to pass an allocator
    // For now, we'll use NULL and handle this differently
    resp->coil_status = NULL;  // Will be allocated when needed

    debug("Created read coils response");
    return resp;
}

void modbus_read_coils_resp_destructor(void *ptr) {
    if(!ptr) { return; }

    modbus_read_coils_resp_t *resp = (modbus_read_coils_resp_t *)ptr;
    debug("Destroying read coils response");

    // Array cleanup is handled automatically by parent-child allocation
    if(resp->coil_status) { modbus_byte_array_dispose(resp->coil_status); }
}

//=============================================================================
// WRITE SINGLE COIL REQUEST (0x05)
//=============================================================================

ptk_err modbus_write_single_coil_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_single_coil_req_t *req = (modbus_write_single_coil_req_t *)obj;

    // Serialize in big-endian format
    return ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, req->function_code, ptk_buf_byte_swap_u16(req->output_address),
                             ptk_buf_byte_swap_u16(req->output_value));
}

ptk_err modbus_write_single_coil_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_single_coil_req_t *req = (modbus_write_single_coil_req_t *)obj;

    uint8_t function_code;
    uint16_t output_address, output_value;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &output_address, &output_value);

    if(err != PTK_OK) {
        error("Failed to deserialize write single coil request");
        return err;
    }

    // Convert from big-endian and validate
    req->function_code = function_code;
    req->output_address = ptk_buf_byte_swap_u16(output_address);
    req->output_value = ptk_buf_byte_swap_u16(output_value);

    if(req->function_code != MODBUS_FUNC_WRITE_SINGLE_COIL) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", req->function_code, MODBUS_FUNC_WRITE_SINGLE_COIL);
        return PTK_ERR_INVALID_PARAM;
    }

    // Validate coil value (must be 0x0000 or 0xFF00)
    if(req->output_value != 0x0000 && req->output_value != 0xFF00) {
        error("Invalid coil value: 0x%04X (must be 0x0000 or 0xFF00)", req->output_value);
        return PTK_ERR_INVALID_PARAM;
    }

    return PTK_OK;
}

modbus_write_single_coil_req_t *modbus_write_single_coil_req_create(void *parent) {
    modbus_write_single_coil_req_t *req =
        ptk_alloc(parent, sizeof(modbus_write_single_coil_req_t), modbus_write_single_coil_req_destructor);
    if(!req) {
        error("Failed to allocate write single coil request");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&req->base, MODBUS_WRITE_SINGLE_COIL_REQ_TYPE, modbus_write_single_coil_req_serialize,
                         modbus_write_single_coil_req_deserialize);

    // Initialize request fields
    req->function_code = MODBUS_FUNC_WRITE_SINGLE_COIL;
    req->output_address = 0;
    req->output_value = 0x0000;  // OFF by default

    debug("Created write single coil request");
    return req;
}

void modbus_write_single_coil_req_destructor(void *ptr) {
    if(!ptr) { return; }
    debug("Destroying write single coil request");
}

//=============================================================================
// WRITE SINGLE COIL RESPONSE (0x05)
//=============================================================================

ptk_err modbus_write_single_coil_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_single_coil_resp_t *resp = (modbus_write_single_coil_resp_t *)obj;

    // Response is identical to request format
    return ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, resp->function_code, ptk_buf_byte_swap_u16(resp->output_address),
                             ptk_buf_byte_swap_u16(resp->output_value));
}

ptk_err modbus_write_single_coil_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_single_coil_resp_t *resp = (modbus_write_single_coil_resp_t *)obj;

    uint8_t function_code;
    uint16_t output_address, output_value;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &output_address, &output_value);

    if(err != PTK_OK) {
        error("Failed to deserialize write single coil response");
        return err;
    }

    // Convert from big-endian and validate
    resp->function_code = function_code;
    resp->output_address = ptk_buf_byte_swap_u16(output_address);
    resp->output_value = ptk_buf_byte_swap_u16(output_value);

    if(resp->function_code != MODBUS_FUNC_WRITE_SINGLE_COIL) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", resp->function_code, MODBUS_FUNC_WRITE_SINGLE_COIL);
        return PTK_ERR_INVALID_PARAM;
    }

    // Validate coil value
    if(resp->output_value != 0x0000 && resp->output_value != 0xFF00) {
        error("Invalid coil value: 0x%04X (must be 0x0000 or 0xFF00)", resp->output_value);
        return PTK_ERR_INVALID_PARAM;
    }

    return PTK_OK;
}

modbus_write_single_coil_resp_t *modbus_write_single_coil_resp_create(void *parent) {
    modbus_write_single_coil_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_write_single_coil_resp_t), modbus_write_single_coil_resp_destructor);
    if(!resp) {
        error("Failed to allocate write single coil response");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&resp->base, MODBUS_WRITE_SINGLE_COIL_RESP_TYPE, modbus_write_single_coil_resp_serialize,
                         modbus_write_single_coil_resp_deserialize);

    // Initialize response fields
    resp->function_code = MODBUS_FUNC_WRITE_SINGLE_COIL;
    resp->output_address = 0;
    resp->output_value = 0x0000;

    debug("Created write single coil response");
    return resp;
}

void modbus_write_single_coil_resp_destructor(void *ptr) {
    if(!ptr) { return; }
    debug("Destroying write single coil response");
}

//=============================================================================
// WRITE MULTIPLE COILS REQUEST (0x0F)
//=============================================================================

ptk_err modbus_write_multiple_coils_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_multiple_coils_req_t *req = (modbus_write_multiple_coils_req_t *)obj;

    // Serialize header
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, req->function_code, ptk_buf_byte_swap_u16(req->starting_address),
                                    ptk_buf_byte_swap_u16(req->quantity_of_outputs), req->byte_count);

    if(err != PTK_OK) {
        error("Failed to serialize write multiple coils request header");
        return err;
    }

    // Serialize output values
    if(req->output_values && req->byte_count > 0) {
        for(size_t i = 0; i < req->byte_count; i++) {
            uint8_t byte_value;
            err = modbus_byte_array_get(req->output_values, i, &byte_value);
            if(err != PTK_OK) {
                error("Failed to get output value byte %zu", i);
                return err;
            }

            err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, byte_value);
            if(err != PTK_OK) {
                error("Failed to serialize output value byte %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

ptk_err modbus_write_multiple_coils_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_multiple_coils_req_t *req = (modbus_write_multiple_coils_req_t *)obj;

    uint8_t function_code, byte_count;
    uint16_t starting_address, quantity_of_outputs;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &starting_address, &quantity_of_outputs,
                                      &byte_count);

    if(err != PTK_OK) {
        error("Failed to deserialize write multiple coils request header");
        return err;
    }

    // Convert from big-endian and validate
    req->function_code = function_code;
    req->starting_address = ptk_buf_byte_swap_u16(starting_address);
    req->quantity_of_outputs = ptk_buf_byte_swap_u16(quantity_of_outputs);
    req->byte_count = byte_count;

    if(req->function_code != MODBUS_FUNC_WRITE_MULTIPLE_COILS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", req->function_code, MODBUS_FUNC_WRITE_MULTIPLE_COILS);
        return PTK_ERR_INVALID_PARAM;
    }

    // Validate parameters
    err =
        modbus_validate_request_params(req->starting_address, req->quantity_of_outputs, 0xFFFF, 1968);  // Max 1968 coils per spec
    if(err != PTK_OK) { return err; }

    // Validate byte count
    size_t expected_bytes = (req->quantity_of_outputs + 7) / 8;
    if(req->byte_count != expected_bytes) {
        error("Invalid byte count: %u (expected %zu)", req->byte_count, expected_bytes);
        return PTK_ERR_INVALID_PARAM;
    }

    // Deserialize output values
    if(req->byte_count > 0) {
        if(!req->output_values) {
            error("Output values array not initialized");
            return PTK_ERR_INVALID_PARAM;
        }

        err = modbus_byte_array_resize(req->output_values, req->byte_count);
        if(err != PTK_OK) {
            error("Failed to resize output values array");
            return err;
        }

        for(size_t i = 0; i < req->byte_count; i++) {
            uint8_t byte_value;
            err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &byte_value);
            if(err != PTK_OK) {
                error("Failed to deserialize output value byte %zu", i);
                return err;
            }

            err = modbus_byte_array_set(req->output_values, i, byte_value);
            if(err != PTK_OK) {
                error("Failed to set output value byte %zu", i);
                return err;
            }
        }
    }

    return PTK_OK;
}

modbus_write_multiple_coils_req_t *modbus_write_multiple_coils_req_create(void *parent) {
    modbus_write_multiple_coils_req_t *req =
        ptk_alloc(parent, sizeof(modbus_write_multiple_coils_req_t), modbus_write_multiple_coils_req_destructor);
    if(!req) {
        error("Failed to allocate write multiple coils request");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&req->base, MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE, modbus_write_multiple_coils_req_serialize,
                         modbus_write_multiple_coils_req_deserialize);

    // Initialize request fields
    req->function_code = MODBUS_FUNC_WRITE_MULTIPLE_COILS;
    req->starting_address = 0;
    req->quantity_of_outputs = 0;
    req->byte_count = 0;
    req->output_values = NULL;  // Will be allocated when needed

    debug("Created write multiple coils request");
    return req;
}

void modbus_write_multiple_coils_req_destructor(void *ptr) {
    if(!ptr) { return; }

    modbus_write_multiple_coils_req_t *req = (modbus_write_multiple_coils_req_t *)ptr;
    debug("Destroying write multiple coils request");

    // Array cleanup is handled automatically by parent-child allocation
    if(req->output_values) { modbus_byte_array_dispose(req->output_values); }
}

//=============================================================================
// WRITE MULTIPLE COILS RESPONSE (0x0F)
//=============================================================================

ptk_err modbus_write_multiple_coils_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_multiple_coils_resp_t *resp = (modbus_write_multiple_coils_resp_t *)obj;

    // Serialize response (echo of request parameters)
    return ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, resp->function_code, ptk_buf_byte_swap_u16(resp->starting_address),
                             ptk_buf_byte_swap_u16(resp->quantity_of_outputs));
}

ptk_err modbus_write_multiple_coils_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_write_multiple_coils_resp_t *resp = (modbus_write_multiple_coils_resp_t *)obj;

    uint8_t function_code;
    uint16_t starting_address, quantity_of_outputs;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &function_code, &starting_address, &quantity_of_outputs);

    if(err != PTK_OK) {
        error("Failed to deserialize write multiple coils response");
        return err;
    }

    // Convert from big-endian and validate
    resp->function_code = function_code;
    resp->starting_address = ptk_buf_byte_swap_u16(starting_address);
    resp->quantity_of_outputs = ptk_buf_byte_swap_u16(quantity_of_outputs);

    if(resp->function_code != MODBUS_FUNC_WRITE_MULTIPLE_COILS) {
        error("Invalid function code: 0x%02X (expected 0x%02X)", resp->function_code, MODBUS_FUNC_WRITE_MULTIPLE_COILS);
        return PTK_ERR_INVALID_PARAM;
    }

    return modbus_validate_request_params(resp->starting_address, resp->quantity_of_outputs, 0xFFFF, 1968);
}

modbus_write_multiple_coils_resp_t *modbus_write_multiple_coils_resp_create(void *parent) {
    modbus_write_multiple_coils_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_write_multiple_coils_resp_t), modbus_write_multiple_coils_resp_destructor);
    if(!resp) {
        error("Failed to allocate write multiple coils response");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&resp->base, MODBUS_WRITE_MULTIPLE_COILS_RESP_TYPE, modbus_write_multiple_coils_resp_serialize,
                         modbus_write_multiple_coils_resp_deserialize);

    // Initialize response fields
    resp->function_code = MODBUS_FUNC_WRITE_MULTIPLE_COILS;
    resp->starting_address = 0;
    resp->quantity_of_outputs = 0;

    debug("Created write multiple coils response");
    return resp;
}

void modbus_write_multiple_coils_resp_destructor(void *ptr) {
    if(!ptr) { return; }
    debug("Destroying write multiple coils response");
}
