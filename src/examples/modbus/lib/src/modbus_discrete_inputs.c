#include "../include/modbus.h"

//=============================================================================
// READ DISCRETE INPUTS (0x02) - REQUEST
//=============================================================================

static ptk_err modbus_read_discrete_inputs_req_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_discrete_inputs_req_t *req = (modbus_read_discrete_inputs_req_t *)obj;

    return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->function_code, req->starting_address, req->quantity_of_inputs);
}

static ptk_err modbus_read_discrete_inputs_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_discrete_inputs_req_t *req = (modbus_read_discrete_inputs_req_t *)obj;

    return ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &req->function_code, &req->starting_address,
                               &req->quantity_of_inputs);
}

static void modbus_read_discrete_inputs_req_dispose(modbus_read_discrete_inputs_req_t *req) { (void)req; }

modbus_read_discrete_inputs_req_t *modbus_read_discrete_inputs_req_create(void *parent) {
    modbus_read_discrete_inputs_req_t *req =
        ptk_alloc(parent, sizeof(modbus_read_discrete_inputs_req_t), (void (*)(void *))modbus_read_discrete_inputs_req_dispose);
    if(!req) { return NULL; }

    modbus_pdu_base_init(&req->base, MODBUS_READ_DISCRETE_INPUTS_REQ_TYPE);
    req->base.buf_base.serialize = modbus_read_discrete_inputs_req_serialize;
    req->base.buf_base.deserialize = modbus_read_discrete_inputs_req_deserialize;
    req->function_code = MODBUS_FC_READ_DISCRETE_INPUTS;
    req->starting_address = 0;
    req->quantity_of_inputs = 0;

    return req;
}

ptk_err modbus_read_discrete_inputs_req_send(modbus_connection *conn, modbus_read_discrete_inputs_req_t *obj,
                                             ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    // Validate parameters
    ptk_err err = modbus_validate_quantity(obj->quantity_of_inputs, 2000);  // Max 2000 inputs per spec
    if(err != PTK_OK) { return err; }

    err = modbus_validate_address_range(obj->starting_address, obj->quantity_of_inputs, 0xFFFF);
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
// READ DISCRETE INPUTS (0x02) - RESPONSE
//=============================================================================

static ptk_err modbus_read_discrete_inputs_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_discrete_inputs_resp_t *resp = (modbus_read_discrete_inputs_resp_t *)obj;

    if(!modbus_bit_array_is_valid(resp->input_status)) { return PTK_ERR_INVALID_PARAM; }

    uint8_t *bytes;
    size_t byte_count;
    ptk_err err = modbus_bit_array_to_bytes(resp->input_status, &bytes, &byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize function code and byte count, then the input data
    err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, resp->function_code, (uint8_t)byte_count);
    if(err != PTK_OK) { return err; }

    // Serialize the input bytes directly
    for(size_t i = 0; i < byte_count; i++) {
        err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, bytes[i]);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static ptk_err modbus_read_discrete_inputs_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    modbus_read_discrete_inputs_resp_t *resp = (modbus_read_discrete_inputs_resp_t *)obj;

    uint8_t byte_count;
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN, &resp->function_code, &byte_count);
    if(err != PTK_OK) { return err; }

    // Calculate number of bits based on existing input_status array if present
    size_t num_bits = resp->input_status ? modbus_bit_array_len(resp->input_status) : byte_count * 8;

    // Read the input bytes
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
    if(!resp->input_status) {
        err = modbus_bit_array_from_bytes(resp, bytes, num_bits, &resp->input_status);
    } else {
        // Update existing array
        uint8_t *existing_bytes;
        size_t existing_byte_count;
        err = modbus_bit_array_to_bytes(resp->input_status, &existing_bytes, &existing_byte_count);
        if(err == PTK_OK && existing_byte_count >= byte_count) {
            memcpy(existing_bytes, bytes, byte_count);
        } else {
            err = PTK_ERR_BUFFER_TOO_SMALL;
        }
    }

    ptk_free(bytes);
    return err;
}

static void modbus_read_discrete_inputs_resp_dispose(modbus_read_discrete_inputs_resp_t *resp) {
    // input_status will be freed automatically as child allocation
    (void)resp;
}

modbus_read_discrete_inputs_resp_t *modbus_read_discrete_inputs_resp_create(void *parent, size_t num_inputs) {
    if(num_inputs == 0) { return NULL; }

    modbus_read_discrete_inputs_resp_t *resp =
        ptk_alloc(parent, sizeof(modbus_read_discrete_inputs_resp_t), (void (*)(void *))modbus_read_discrete_inputs_resp_dispose);
    if(!resp) { return NULL; }

    modbus_pdu_base_init(&resp->base, MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE);
    resp->base.buf_base.serialize = modbus_read_discrete_inputs_resp_serialize;
    resp->base.buf_base.deserialize = modbus_read_discrete_inputs_resp_deserialize;
    resp->function_code = MODBUS_FC_READ_DISCRETE_INPUTS;

    resp->input_status = modbus_bit_array_create(resp, num_inputs);
    if(!resp->input_status) {
        ptk_free(resp);
        return NULL;
    }

    return resp;
}

ptk_err modbus_read_discrete_inputs_resp_send(modbus_connection *conn, modbus_read_discrete_inputs_resp_t *obj,
                                              ptk_duration_ms timeout_ms) {
    if(!conn || !obj) { return PTK_ERR_NULL_PTR; }

    if(!modbus_bit_array_is_valid(obj->input_status)) { return PTK_ERR_INVALID_PARAM; }

    // Serialize to connection's TX buffer
    ptk_buf_set_start(conn->tx_buffer, 0);
    ptk_buf_set_end(conn->tx_buffer, 0);

    ptk_err err = obj->base.buf_base.serialize(conn->tx_buffer, &obj->base.buf_base);
    if(err != PTK_OK) { return err; }

    // TODO: Send via socket when transport layer is implemented
    (void)timeout_ms;
    return PTK_ERR_UNSUPPORTED;
}
