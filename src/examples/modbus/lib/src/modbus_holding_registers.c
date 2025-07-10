#include "../include/modbus.h"

//=============================================================================
// READ HOLDING REGISTERS (0x03) - REQUEST
//=============================================================================

static ptk_err modbus_read_holding_registers_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_holding_registers_req_t *req = (modbus_read_holding_registers_req_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->function_code, req->starting_address, req->quantity_of_registers);
}

static ptk_err modbus_read_holding_registers_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_holding_registers_req_t *req = (modbus_read_holding_registers_req_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &req->function_code, &req->starting_address,
                               &req->quantity_of_registers);
}

static void modbus_read_holding_registers_req_dispose(modbus_read_holding_registers_req_t *req) { (void)req; }

modbus_read_holding_registers_req_t *modbus_read_holding_registers_req_create(void *parent) {
    modbus_read_holding_registers_req_t *req = ptk_alloc(parent, sizeof(modbus_read_holding_registers_req_t),
                                                         (void (*)(void *))modbus_read_holding_registers_req_dispose);
    if(!req) { return NULL; }

    modbus_pdu_base_init(&req->base, MODBUS_READ_HOLDING_REGISTERS_REQ_TYPE);
    req->base.buf_base.serialize = modbus_read_holding_registers_req_serialize;
    req->base.buf_base.deserialize = modbus_read_holding_registers_req_deserialize;
    req->function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    req->starting_address = 0;
    req->quantity_of_registers = 0;

    return req;
}

ptk_err modbus_read_holding_registers_req_send(modbus_connection *conn, modbus_read_holding_registers_req_t *obj,
                                               ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    // Validate parameters
    ptk_err err = modbus_validate_quantity(obj->quantity_of_registers, 125);  // Max 125 registers per spec
    if(err != PTK_OK) { return err; }

    err = modbus_validate_address_range(obj->starting_address, obj->quantity_of_registers, 0xFFFF);
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
// READ HOLDING REGISTERS (0x03) - RESPONSE
//=============================================================================

static ptk_err modbus_read_holding_registers_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_holding_registers_resp_t *resp = (modbus_read_holding_registers_resp_t *)obj;

    if(!modbus_register_array_is_valid(resp->register_values)) { return PTK_ERR_INVALID_PARAM; }

    size_t register_count = modbus_register_array_len(resp->register_values);
    uint8_t byte_count = (uint8_t)(register_count * 2);

    // Serialize function code and byte count
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->function_code, byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize each register value (16-bit big-endian)
    for(size_t i = 0; i < register_count; i++) {
        uint16_t value;
        err = modbus_register_array_get(resp->register_values, i, &value);
        if(err != PTK_OK) { return err; }

        err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, value);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static ptk_err modbus_read_holding_registers_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_holding_registers_resp_t *resp = (modbus_read_holding_registers_resp_t *)obj;

    uint8_t byte_count;
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->function_code, &byte_count);
    if(err != PTK_OK) { return err; }

    size_t register_count = byte_count / 2;

    // Create register array if it doesn't exist
    if(!resp->register_values) {
        resp->register_values = modbus_register_array_create(register_count, NULL);
        if(!resp->register_values) { return PTK_ERR_NO_RESOURCES; }
    } else {
        // Resize existing array if needed
        err = modbus_register_array_resize(resp->register_values, register_count);
        if(err != PTK_OK) { return err; }
    }

    // Read register values
    for(size_t i = 0; i < register_count; i++) {
        uint16_t value;
        err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &value);
        if(err != PTK_OK) { return err; }

        err = modbus_register_array_set(resp->register_values, i, value);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static void modbus_read_holding_registers_resp_dispose(modbus_read_holding_registers_resp_t *resp) {
    // register_values will be freed automatically as child allocation
    (void)resp;
}

modbus_read_holding_registers_resp_t *modbus_read_holding_registers_resp_create(void *parent, size_t num_registers) {
    if(num_registers == 0) { return NULL; }

    modbus_read_holding_registers_resp_t *resp = ptk_alloc(parent, sizeof(modbus_read_holding_registers_resp_t),
                                                           (void (*)(void *))modbus_read_holding_registers_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_READ_HOLDING_REGISTERS_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_read_holding_registers_resp_serialize;
    resp->base.buf_base.deserialize = modbus_read_holding_registers_resp_deserialize;
    resp->function_code = MODBUS_FC_READ_HOLDING_REGISTERS;

    resp->register_values = modbus_register_array_create(num_registers, NULL);
    if(!resp->register_values) {
        ptk_free(resp);
        return NULL;
    }

    return resp;
}

ptk_err modbus_read_holding_registers_resp_send(modbus_connection *conn, modbus_read_holding_registers_resp_t *obj,
                                                ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    if(!modbus_register_array_is_valid(obj->register_values)) { return PTK_ERR_INVALID_PARAM; }

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
// WRITE SINGLE REGISTER (0x06) - REQUEST
//=============================================================================

static ptk_err modbus_write_single_register_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_register_req_t *req = (modbus_write_single_register_req_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->function_code, req->register_address, req->register_value);
}

static ptk_err modbus_write_single_register_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_register_req_t *req = (modbus_write_single_register_req_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &req->function_code, &req->register_address, &req->register_value);
}

static void modbus_write_single_register_req_dispose(modbus_write_single_register_req_t *req) { (void)req; }

modbus_write_single_register_req_t *modbus_write_single_register_req_create(void *parent) {
    modbus_write_single_register_req_t *req =
        ptk_alloc(parent, sizeof(modbus_write_single_register_req_t), (void (*)(void *))modbus_write_single_register_req_dispose);
    if(!req) { return NULL; }

    modbus_pdu_base_init(&req->base, MODBUS_WRITE_SINGLE_REGISTER_REQ_TYPE);
    req->base.buf_base.serialize = modbus_write_single_register_req_serialize;
    req->base.buf_base.deserialize = modbus_write_single_register_req_deserialize;
    req->function_code = MODBUS_FC_WRITE_SINGLE_REGISTER;
    req->register_address = 0;
    req->register_value = 0;

    return req;
}

ptk_err modbus_write_single_register_req_send(modbus_connection *conn, modbus_write_single_register_req_t *obj,
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
// WRITE SINGLE REGISTER (0x06) - RESPONSE
//=============================================================================

static ptk_err modbus_write_single_register_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_register_resp_t *resp = (modbus_write_single_register_resp_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->function_code, resp->register_address, resp->register_value);
}

static ptk_err modbus_write_single_register_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_single_register_resp_t *resp = (modbus_write_single_register_resp_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->function_code, &resp->register_address,
                               &resp->register_value);
}

static void modbus_write_single_register_resp_dispose(modbus_write_single_register_resp_t *resp) { (void)resp; }

modbus_write_single_register_resp_t *modbus_write_single_register_resp_create(void *parent) {
    modbus_write_single_register_resp_t *resp = ptk_alloc(parent, sizeof(modbus_write_single_register_resp_t),
                                                          (void (*)(void *))modbus_write_single_register_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_WRITE_SINGLE_REGISTER_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_write_single_register_resp_serialize;
    resp->base.buf_base.deserialize = modbus_write_single_register_resp_deserialize;
    resp->function_code = MODBUS_FC_WRITE_SINGLE_REGISTER;
    resp->register_address = 0;
    resp->register_value = 0;

    return resp;
}

ptk_err modbus_write_single_register_resp_send(modbus_connection *conn, modbus_write_single_register_resp_t *obj,
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
// WRITE MULTIPLE REGISTERS (0x10) - REQUEST
//=============================================================================

static ptk_err modbus_write_multiple_registers_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_registers_req_t *req = (modbus_write_multiple_registers_req_t *)obj;

    if(!modbus_register_array_is_valid(req->register_values)) { return PTK_ERR_INVALID_PARAM; }

    size_t register_count = modbus_register_array_len(req->register_values);
    uint16_t quantity = (uint16_t)register_count;
    uint8_t byte_count = (uint8_t)(register_count * 2);

    // Serialize function code, address, quantity, byte count
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->function_code, req->starting_address, quantity, byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize the register values
    for(size_t i = 0; i < register_count; i++) {
        uint16_t value;
        err = modbus_register_array_get(req->register_values, i, &value);
        if(err != PTK_OK) { return err; }

        err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, value);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static ptk_err modbus_write_multiple_registers_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_registers_req_t *req = (modbus_write_multiple_registers_req_t *)obj;

    uint16_t quantity;
    uint8_t byte_count;
    ptk_err err =
        ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &req->function_code, &req->starting_address, &quantity, &byte_count);
    if(err != PTK_OK) { return err; }

    // Create register array
    req->register_values = modbus_register_array_create(quantity, NULL);
    if(!req->register_values) { return PTK_ERR_NO_RESOURCES; }

    // Read register values
    for(size_t i = 0; i < quantity; i++) {
        uint16_t value;
        err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &value);
        if(err != PTK_OK) { return err; }

        err = modbus_register_array_set(req->register_values, i, value);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static void modbus_write_multiple_registers_req_dispose(modbus_write_multiple_registers_req_t *req) { (void)req; }

modbus_write_multiple_registers_req_t *modbus_write_multiple_registers_req_create(void *parent, size_t num_registers) {
    if(num_registers == 0) { return NULL; }

    modbus_write_multiple_registers_req_t *req = ptk_alloc(parent, sizeof(modbus_write_multiple_registers_req_t),
                                                           (void (*)(void *))modbus_write_multiple_registers_req_dispose);
    if(!req) { return NULL; }

    modbus_pdu_base_init(&req->base, MODBUS_WRITE_MULTIPLE_REGISTERS_REQ_TYPE);
    req->base.buf_base.serialize = modbus_write_multiple_registers_req_serialize;
    req->base.buf_base.deserialize = modbus_write_multiple_registers_req_deserialize;
    req->function_code = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    req->starting_address = 0;

    req->register_values = modbus_register_array_create(num_registers, NULL);
    if(!req->register_values) {
        ptk_free(req);
        return NULL;
    }

    return req;
}

ptk_err modbus_write_multiple_registers_req_send(modbus_connection *conn, modbus_write_multiple_registers_req_t *obj,
                                                 ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    if(!modbus_register_array_is_valid(obj->register_values)) { return PTK_ERR_INVALID_PARAM; }

    size_t quantity = modbus_register_array_len(obj->register_values);
    ptk_err err = modbus_validate_quantity((uint16_t)quantity, 123);  // Max 123 registers per spec
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
// WRITE MULTIPLE REGISTERS (0x10) - RESPONSE
//=============================================================================

static ptk_err modbus_write_multiple_registers_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_registers_resp_t *resp = (modbus_write_multiple_registers_resp_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->function_code, resp->starting_address, resp->quantity_of_registers);
}

static ptk_err modbus_write_multiple_registers_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_write_multiple_registers_resp_t *resp = (modbus_write_multiple_registers_resp_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->function_code, &resp->starting_address,
                               &resp->quantity_of_registers);
}

static void modbus_write_multiple_registers_resp_dispose(modbus_write_multiple_registers_resp_t *resp) { (void)resp; }

modbus_write_multiple_registers_resp_t *modbus_write_multiple_registers_resp_create(void *parent) {
    modbus_write_multiple_registers_resp_t *resp = ptk_alloc(parent, sizeof(modbus_write_multiple_registers_resp_t),
                                                             (void (*)(void *))modbus_write_multiple_registers_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_WRITE_MULTIPLE_REGISTERS_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_write_multiple_registers_resp_serialize;
    resp->base.buf_base.deserialize = modbus_write_multiple_registers_resp_deserialize;
    resp->function_code = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    resp->starting_address = 0;
    resp->quantity_of_registers = 0;

    return resp;
}

ptk_err modbus_write_multiple_registers_resp_send(modbus_connection *conn, modbus_write_multiple_registers_resp_t *obj,
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
