#include "modbus_internal.h"
#include <ptk_log.h>

//=============================================================================
// EXCEPTION RESPONSE IMPLEMENTATION
//=============================================================================

ptk_err modbus_exception_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_exception_resp_t *resp = (modbus_exception_resp_t *)obj;

    // Serialize exception response (2 bytes total)
    return ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, resp->exception_function_code, resp->exception_code);
}

ptk_err modbus_exception_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_exception_resp_t *resp = (modbus_exception_resp_t *)obj;

    uint8_t exception_function_code, exception_code;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &exception_function_code, &exception_code);

    if(err != PTK_OK) {
        error("Failed to deserialize exception response");
        return err;
    }

    resp->exception_function_code = exception_function_code;
    resp->exception_code = exception_code;

    // Validate exception function code (must be original function code + 0x80)
    if(resp->exception_function_code < 0x80) {
        error("Invalid exception function code: 0x%02X (must be >= 0x80)", resp->exception_function_code);
        return PTK_ERR_INVALID_PARAM;
    }

    // Validate exception code (standard codes are 1-4, but allow others)
    if(resp->exception_code == 0) {
        error("Invalid exception code: 0 (must be non-zero)");
        return PTK_ERR_INVALID_PARAM;
    }

    return PTK_OK;
}

modbus_exception_resp_t *modbus_exception_resp_create(void *parent) {
    modbus_exception_resp_t *resp = ptk_alloc(parent, sizeof(modbus_exception_resp_t), modbus_exception_resp_destructor);
    if(!resp) {
        error("Failed to allocate exception response");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&resp->base, MODBUS_EXCEPTION_RESP_TYPE, modbus_exception_resp_serialize,
                         modbus_exception_resp_deserialize);

    // Initialize response fields
    resp->exception_function_code = 0x80;                      // Default invalid
    resp->exception_code = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;  // Default error

    debug("Created exception response");
    return resp;
}

void modbus_exception_resp_destructor(void *ptr) {
    if(!ptr) { return; }
    debug("Destroying exception response");
}

//=============================================================================
// EXCEPTION HELPER FUNCTIONS
//=============================================================================

modbus_exception_resp_t *modbus_create_exception_response(void *parent, uint8_t function_code, uint8_t exception_code) {
    modbus_exception_resp_t *resp = modbus_exception_resp_create(parent);
    if(!resp) { return NULL; }

    resp->exception_function_code = function_code | 0x80;  // Set exception bit
    resp->exception_code = exception_code;

    debug("Created exception response for function 0x%02X with code %u", function_code, exception_code);

    return resp;
}

const char *modbus_get_exception_description(uint8_t exception_code) {
    switch(exception_code) {
        case MODBUS_EXCEPTION_ILLEGAL_FUNCTION: return "Illegal Function";

        case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS: return "Illegal Data Address";

        case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE: return "Illegal Data Value";

        case MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE: return "Slave Device Failure";

        default: return "Unknown Exception";
    }
}

ptk_err modbus_validate_exception_for_function(uint8_t function_code, uint8_t exception_code) {
    // Basic validation - all exception codes are valid for all functions
    // More specific validation could be added here based on Modbus spec

    switch(exception_code) {
        case MODBUS_EXCEPTION_ILLEGAL_FUNCTION:
        case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
        case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE:
        case MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE: return PTK_OK;

        default:
            // Allow vendor-specific exception codes (0x05-0xFF)
            if(exception_code >= 0x05) {
                debug("Using vendor-specific exception code: %u", exception_code);
                return PTK_OK;
            }

            error("Invalid exception code: %u", exception_code);
            return PTK_ERR_INVALID_PARAM;
    }
}
