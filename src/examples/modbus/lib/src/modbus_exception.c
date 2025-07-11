#include "../include/modbus.h"

//=============================================================================
// EXCEPTION RESPONSE
//=============================================================================

static ptk_err modbus_exception_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_exception_resp_t *resp = (modbus_exception_resp_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->exception_function_code, resp->exception_code);
}

static ptk_err modbus_exception_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_exception_resp_t *resp = (modbus_exception_resp_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->exception_function_code, &resp->exception_code);
}

static void modbus_exception_resp_dispose(modbus_exception_resp_t *resp) { (void)resp; }

modbus_exception_resp_t *modbus_exception_resp_create(void *parent) {
    modbus_exception_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_exception_resp_t), (void (*)(void *))modbus_exception_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_EXCEPTION_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_exception_resp_serialize;
    resp->base.buf_base.deserialize = modbus_exception_resp_deserialize;
    resp->exception_function_code = 0x80;                      // Will be set to original function code + 0x80
    resp->exception_code = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;  // Default

    return resp;
}

ptk_err modbus_exception_resp_send(modbus_connection *conn, modbus_exception_resp_t *obj, ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    // Validate exception function code has 0x80 bit set
    if((obj->exception_function_code & 0x80) == 0) { return PTK_ERR_INVALID_PARAM; }

    // Validate exception code is in valid range
    if(obj->exception_code < 1 || obj->exception_code > 4) { return PTK_ERR_INVALID_PARAM; }

    // Serialize to connection's TX buffer
    ptk_buf_set_start(conn->tx_buffer, 0);
    ptk_buf_set_end(conn->tx_buffer, 0);

    ptk_err err = obj->base.buf_base.serialize(conn->tx_buffer, &obj->base.buf_base);
    if(err != PTK_OK) { return err; }

    // TODO: Send via socket when transport layer is implemented
    (void)timeout_ms;
    return PTK_ERR_UNSUPPORTED;
}

//=============================================================================
// EXCEPTION UTILITY FUNCTIONS
//=============================================================================

modbus_exception_resp_t *modbus_create_exception_response(void *parent, uint8_t original_function_code, uint8_t exception_code) {
    modbus_exception_resp_t *resp = modbus_exception_resp_create(parent);
    if(!resp) { return NULL; }

    resp->exception_function_code = original_function_code | 0x80;
    resp->exception_code = exception_code;

    return resp;
}
