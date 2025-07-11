#include "../include/modbus.h"

//=============================================================================
// READ COILS (0x01) - REQUEST
//=============================================================================

static ptk_err modbus_read_coils_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_coils_req_t *req = (modbus_read_coils_req_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->function_code, req->starting_address, req->quantity_of_coils);
}

static ptk_err modbus_read_coils_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_coils_req_t *req = (modbus_read_coils_req_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &req->function_code, &req->starting_address,
                               &req->quantity_of_coils);
}

static void modbus_read_coils_req_dispose(modbus_read_coils_req_t *req) {
    // No special cleanup needed
    (void)req;
}

modbus_read_coils_req_t *modbus_read_coils_req_create(void *parent) {
    modbus_read_coils_req_t *req =
        ptk_alloc(parent, sizeof(modbus_read_coils_req_t), (void (*)(void *))modbus_read_coils_req_dispose);
    if(!req) { return NULL; }

    modbus_pdu_base_init(&req->base, MODBUS_READ_COILS_REQ_TYPE);
    req->base.buf_base.serialize = modbus_read_coils_req_serialize;
    req->base.buf_base.deserialize = modbus_read_coils_req_deserialize;
    req->function_code = MODBUS_FC_READ_COILS;
    req->starting_address = 0;
    req->quantity_of_coils = 0;

    return req;
}

ptk_err modbus_read_coils_req_send(modbus_connection *conn, modbus_read_coils_req_t *obj, ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    // Validate parameters
    ptk_err err = modbus_validate_quantity(obj->quantity_of_coils, 2000);  // Max 2000 coils per spec
    if(err != PTK_OK) { return err; }

    err = modbus_validate_address_range(obj->starting_address, obj->quantity_of_coils, 0xFFFF);
    if(err != PTK_OK) { return err; }

    // Serialize to connection's TX buffer
    ptk_buf_set_start(conn->tx_buffer, 0);
    ptk_buf_set_end(conn->tx_buffer, 0);

    err = obj->base.buf_base.serialize(conn->tx_buffer, &obj->base.buf_base);
    if(err != PTK_OK) { return err; }

    // TODO: Send via socket when transport layer is implemented
    (void)timeout_ms;
    return PTK_ERR_UNSUPPORTED;
}

//=============================================================================
// READ COILS (0x01) - RESPONSE
//=============================================================================

static ptk_err modbus_read_coils_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_coils_resp_t *resp = (modbus_read_coils_resp_t *)obj;

    if(!modbus_bit_array_is_valid(resp->coil_status)) { return PTK_ERR_INVALID_PARAM; }

    uint8_t *bytes;
    size_t byte_count;
    ptk_err err = modbus_bit_array_to_bytes(resp->coil_status, &bytes, &byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize function code and byte count, then the coil data
    err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->function_code, (uint8_t)byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize the coil bytes directly
    for(size_t i = 0; i < byte_count; i++) {
        err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, bytes[i]);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static ptk_err modbus_read_coils_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_coils_resp_t *resp = (modbus_read_coils_resp_t *)obj;

    uint8_t byte_count;
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->function_code, &byte_count);
    if(err != PTK_OK) { return err; }

    // Calculate number of bits based on existing coil_status array if present
    size_t num_bits = resp->coil_status ? modbus_bit_array_len(resp->coil_status) : byte_count * 8;

    // Read the coil bytes
    uint8_t *bytes = ptk_alloc(NULL, byte_count, NULL);
    if(!bytes) { return PTK_ERR_NO_RESOURCES; }

    for(size_t i = 0; i < byte_count; i++) {
        err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &bytes[i]);
        if(err != PTK_OK) {
            ptk_free(bytes);
            return err;
        }
    }

    // Create or update bit array
    if(!resp->coil_status) {
        err = modbus_bit_array_from_bytes(resp, bytes, num_bits, &resp->coil_status);
    } else {
        // Update existing array
        uint8_t *existing_bytes;
        size_t existing_byte_count;
        err = modbus_bit_array_to_bytes(resp->coil_status, &existing_bytes, &existing_byte_count);
        if(err == PTK_OK && existing_byte_count >= byte_count) {
            memcpy(existing_bytes, bytes, byte_count);
        } else {
            err = PTK_ERR_BUFFER_TOO_SMALL;
        }
    }

    ptk_free(bytes);
    return err;
}

static void modbus_read_coils_resp_dispose(modbus_read_coils_resp_t *resp) {
    // coil_status will be freed automatically as child allocation
    (void)resp;
}

modbus_read_coils_resp_t *modbus_read_coils_resp_create(void *parent, size_t num_coils) {
    if(num_coils == 0) { return NULL; }

    modbus_read_coils_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_read_coils_resp_t), (void (*)(void *))modbus_read_coils_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_READ_COILS_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_read_coils_resp_serialize;
    resp->base.buf_base.deserialize = modbus_read_coils_resp_deserialize;
    resp->function_code = MODBUS_FC_READ_COILS;

    resp->coil_status = modbus_bit_array_create(resp, num_coils);
    if(!resp->coil_status) {
        ptk_free(resp);
        return NULL;
    }

    return resp;
}

ptk_err modbus_read_coils_resp_send(modbus_connection *conn, modbus_read_coils_resp_t *obj, ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    if(!modbus_bit_array_is_valid(obj->coil_status)) { return PTK_ERR_INVALID_PARAM; }

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
// WRITE SINGLE COIL (0x05) - REQUEST
//=============================================================================

static ptk_err modbus_write_single_coil_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_coil_req_t *req = (modbus_write_single_coil_req_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->function_code, req->output_address, req->output_value);
}

static ptk_err modbus_write_single_coil_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_coil_req_t *req = (modbus_write_single_coil_req_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &req->function_code, &req->output_address, &req->output_value);
}

static void modbus_write_single_coil_req_dispose(modbus_write_single_coil_req_t *req) { (void)req; }

modbus_write_single_coil_req_t *modbus_write_single_coil_req_create(void *parent) {
    modbus_write_single_coil_req_t *req =
        ptk_alloc(parent, sizeof(modbus_write_single_coil_req_t), (void (*)(void *))modbus_write_single_coil_req_dispose);
    if(!req) { return NULL; }

    modbus_pdu_base_init(&req->base, MODBUS_WRITE_SINGLE_COIL_REQ_TYPE);
    req->base.buf_base.serialize = modbus_write_single_coil_req_serialize;
    req->base.buf_base.deserialize = modbus_write_single_coil_req_deserialize;
    req->function_code = MODBUS_FC_WRITE_SINGLE_COIL;
    req->output_address = 0;
    req->output_value = 0x0000;  // OFF by default

    return req;
}

ptk_err modbus_write_single_coil_req_send(modbus_connection *conn, modbus_write_single_coil_req_t *obj,
                                          ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    // Validate output value (must be 0x0000 or 0xFF00)
    if(obj->output_value != 0x0000 && obj->output_value != 0xFF00) { return PTK_ERR_INVALID_PARAM; }

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
// WRITE SINGLE COIL (0x05) - RESPONSE
//=============================================================================

static ptk_err modbus_write_single_coil_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_coil_resp_t *resp = (modbus_write_single_coil_resp_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->function_code, resp->output_address, resp->output_value);
}

static ptk_err modbus_write_single_coil_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_coil_resp_t *resp = (modbus_write_single_coil_resp_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->function_code, &resp->output_address, &resp->output_value);
}

static void modbus_write_single_coil_resp_dispose(modbus_write_single_coil_resp_t *resp) { (void)resp; }

modbus_write_single_coil_resp_t *modbus_write_single_coil_resp_create(void *parent) {
    modbus_write_single_coil_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_write_single_coil_resp_t), (void (*)(void *))modbus_write_single_coil_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_WRITE_SINGLE_COIL_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_write_single_coil_resp_serialize;
    resp->base.buf_base.deserialize = modbus_write_single_coil_resp_deserialize;
    resp->function_code = MODBUS_FC_WRITE_SINGLE_COIL;
    resp->output_address = 0;
    resp->output_value = 0x0000;

    return resp;
}

ptk_err modbus_write_single_coil_resp_send(modbus_connection *conn, modbus_write_single_coil_resp_t *obj,
                                           ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

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
// WRITE MULTIPLE COILS (0x0F) - REQUEST
//=============================================================================

static ptk_err modbus_write_multiple_coils_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_coils_req_t *req = (modbus_write_multiple_coils_req_t *)obj;

    if(!modbus_bit_array_is_valid(req->output_values)) { return PTK_ERR_INVALID_PARAM; }

    uint16_t quantity = (uint16_t)modbus_bit_array_len(req->output_values);
    uint8_t *bytes;
    size_t byte_count;
    ptk_err err = modbus_bit_array_to_bytes(req->output_values, &bytes, &byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize function code, address, quantity, byte count
    err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->function_code, req->starting_address, quantity, (uint8_t)byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize the coil bytes
    for(size_t i = 0; i < byte_count; i++) {
        err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, bytes[i]);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static ptk_err modbus_write_multiple_coils_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_coils_req_t *req = (modbus_write_multiple_coils_req_t *)obj;

    uint16_t quantity;
    uint8_t byte_count;
    ptk_err err =
        ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &req->function_code, &req->starting_address, &quantity, &byte_count);
    if(err != PTK_OK) { return err; }

    // Read the coil bytes
    uint8_t *bytes = ptk_alloc(NULL, byte_count, NULL);
    if(!bytes) { return PTK_ERR_NO_RESOURCES; }

    for(size_t i = 0; i < byte_count; i++) {
        err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &bytes[i]);
        if(err != PTK_OK) {
            ptk_free(bytes);
            return err;
        }
    }

    // Create bit array from received data
    err = modbus_bit_array_from_bytes(req, bytes, quantity, &req->output_values);
    ptk_free(bytes);

    return err;
}

static void modbus_write_multiple_coils_req_dispose(modbus_write_multiple_coils_req_t *req) { (void)req; }

modbus_write_multiple_coils_req_t *modbus_write_multiple_coils_req_create(void *parent, size_t num_coils) {
    if(num_coils == 0) { return NULL; }

    modbus_write_multiple_coils_req_t *req =
        ptk_alloc(parent, sizeof(modbus_write_multiple_coils_req_t), (void (*)(void *))modbus_write_multiple_coils_req_dispose);
    if(!req) { return NULL; }

    modbus_pdu_base_init(&req->base, MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE);
    req->base.buf_base.serialize = modbus_write_multiple_coils_req_serialize;
    req->base.buf_base.deserialize = modbus_write_multiple_coils_req_deserialize;
    req->function_code = MODBUS_FC_WRITE_MULTIPLE_COILS;
    req->starting_address = 0;

    req->output_values = modbus_bit_array_create(req, num_coils);
    if(!req->output_values) {
        ptk_free(req);
        return NULL;
    }

    return req;
}

ptk_err modbus_write_multiple_coils_req_send(modbus_connection *conn, modbus_write_multiple_coils_req_t *obj,
                                             ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    if(!modbus_bit_array_is_valid(obj->output_values)) { return PTK_ERR_INVALID_PARAM; }

    size_t quantity = modbus_bit_array_len(obj->output_values);
    ptk_err err = modbus_validate_quantity((uint16_t)quantity, 1968);  // Max 1968 coils per spec
    if(err != PTK_OK) { return err; }

    // Serialize to connection's TX buffer
    ptk_buf_set_start(conn->tx_buffer, 0);
    ptk_buf_set_end(conn->tx_buffer, 0);

    err = obj->base.buf_base.serialize(conn->tx_buffer, &obj->base.buf_base);
    if(err != PTK_OK) { return err; }

    // TODO: Send via socket when transport layer is implemented
    (void)timeout_ms;
    return PTK_ERR_UNSUPPORTED;
}

//=============================================================================
// WRITE MULTIPLE COILS (0x0F) - RESPONSE
//=============================================================================

static ptk_err modbus_write_multiple_coils_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_coils_resp_t *resp = (modbus_write_multiple_coils_resp_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->function_code, resp->starting_address, resp->quantity_of_outputs);
}

static ptk_err modbus_write_multiple_coils_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_coils_resp_t *resp = (modbus_write_multiple_coils_resp_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->function_code, &resp->starting_address,
                               &resp->quantity_of_outputs);
}

static void modbus_write_multiple_coils_resp_dispose(modbus_write_multiple_coils_resp_t *resp) { (void)resp; }

modbus_write_multiple_coils_resp_t *modbus_write_multiple_coils_resp_create(void *parent) {
    modbus_write_multiple_coils_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_write_multiple_coils_resp_t), (void (*)(void *))modbus_write_multiple_coils_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_WRITE_MULTIPLE_COILS_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_write_multiple_coils_resp_serialize;
    resp->base.buf_base.deserialize = modbus_write_multiple_coils_resp_deserialize;
    resp->function_code = MODBUS_FC_WRITE_MULTIPLE_COILS;
    resp->starting_address = 0;
    resp->quantity_of_outputs = 0;

    return resp;
}

ptk_err modbus_write_multiple_coils_resp_send(modbus_connection *conn, modbus_write_multiple_coils_resp_t *obj,
                                              ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    // Serialize to connection's TX buffer
    ptk_buf_set_start(conn->tx_buffer, 0);
    ptk_buf_set_end(conn->tx_buffer, 0);

    ptk_err err = obj->base.buf_base.serialize(conn->tx_buffer, &obj->base.buf_base);
    if(err != PTK_OK) { return err; }

    // TODO: Send via socket when transport layer is implemented
    (void)timeout_ms;
    return PTK_ERR_UNSUPPORTED;
}
