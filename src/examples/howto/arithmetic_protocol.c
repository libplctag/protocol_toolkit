#include "arithmetic_protocol.h"
#include "crc.h"
#include <ptk_log.h>

static void arithmetic_request_destructor(void *ptr) {
    if (ptr) {
        debug("Destroying arithmetic request");
    }
}

static void arithmetic_response_destructor(void *ptr) {
    if (ptr) {
        debug("Destroying arithmetic response");
    }
}

arithmetic_request_t *arithmetic_request_create(arithmetic_operation_t op, f32 op1, f32 op2) {
    arithmetic_request_t *req = ptk_alloc(sizeof(arithmetic_request_t), arithmetic_request_destructor);
    if (!req) {
        error("Failed to allocate arithmetic request");
        return NULL;
    }
    
    req->base.serialize = arithmetic_request_serialize;
    req->base.deserialize = arithmetic_request_deserialize;
    req->operation = (u8)op;
    req->operand1 = op1;
    req->operand2 = op2;
    req->crc = 0;
    
    return req;
}

arithmetic_response_t *arithmetic_response_create(arithmetic_operation_t original_op, f64 result) {
    arithmetic_response_t *resp = ptk_alloc(sizeof(arithmetic_response_t), arithmetic_response_destructor);
    if (!resp) {
        error("Failed to allocate arithmetic response");
        return NULL;
    }
    
    resp->base.serialize = arithmetic_response_serialize;
    resp->base.deserialize = arithmetic_response_deserialize;
    resp->operation_inverted = ~((u8)original_op);
    resp->result = result;
    resp->crc = 0;
    
    return resp;
}

ptk_err arithmetic_request_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    arithmetic_request_t *req = (arithmetic_request_t*)obj;
    
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, 
                                   req->operation, req->operand1, req->operand2);
    if (err != PTK_OK) {
        error("Failed to serialize request fields: %d", err);
        return err;
    }
    
    buf_size_t data_len = ptk_buf_get_len(buf) - 2;
    u8 *data_start = buf->data + buf->start;
    req->crc = crc16_calculate(data_start, data_len);
    
    err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, req->crc);
    if (err != PTK_OK) {
        error("Failed to serialize CRC: %d", err);
        return err;
    }
    
    debug("Serialized arithmetic request: op=%d, op1=%f, op2=%f, crc=0x%04x", 
          req->operation, req->operand1, req->operand2, req->crc);
    
    return PTK_OK;
}

ptk_err arithmetic_request_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    arithmetic_request_t *req = (arithmetic_request_t*)obj;
    
    buf_size_t start_pos = ptk_buf_get_start(buf);
    
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN,
                                     &req->operation, &req->operand1, &req->operand2, &req->crc);
    if (err != PTK_OK) {
        error("Failed to deserialize request: %d", err);
        return err;
    }
    
    buf_size_t data_len = 9;
    u8 *data_start = buf->data + start_pos;
    u16 calculated_crc = crc16_calculate(data_start, data_len);
    
    if (calculated_crc != req->crc) {
        error("CRC mismatch: calculated=0x%04x, received=0x%04x", calculated_crc, req->crc);
        ptk_set_err(PTK_ERR_CHECKSUM_FAILED);
        return PTK_ERR_CHECKSUM_FAILED;
    }
    
    debug("Deserialized arithmetic request: op=%d, op1=%f, op2=%f, crc=0x%04x",
          req->operation, req->operand1, req->operand2, req->crc);
    
    return PTK_OK;
}

ptk_err arithmetic_response_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    arithmetic_response_t *resp = (arithmetic_response_t*)obj;
    
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                                   resp->operation_inverted, resp->result);
    if (err != PTK_OK) {
        error("Failed to serialize response fields: %d", err);
        return err;
    }
    
    buf_size_t data_len = ptk_buf_get_len(buf) - 1;
    u8 *data_start = buf->data + buf->start;
    resp->crc = crc8_calculate(data_start, data_len);
    
    err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, resp->crc);
    if (err != PTK_OK) {
        error("Failed to serialize CRC: %d", err);
        return err;
    }
    
    debug("Serialized arithmetic response: op_inv=0x%02x, result=%f, crc=0x%02x",
          resp->operation_inverted, resp->result, resp->crc);
    
    return PTK_OK;
}

ptk_err arithmetic_response_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    arithmetic_response_t *resp = (arithmetic_response_t*)obj;
    
    buf_size_t start_pos = ptk_buf_get_start(buf);
    
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN,
                                     &resp->operation_inverted, &resp->result, &resp->crc);
    if (err != PTK_OK) {
        error("Failed to deserialize response: %d", err);
        return err;
    }
    
    buf_size_t data_len = 9;
    u8 *data_start = buf->data + start_pos;
    u8 calculated_crc = crc8_calculate(data_start, data_len);
    
    if (calculated_crc != resp->crc) {
        error("CRC mismatch: calculated=0x%02x, received=0x%02x", calculated_crc, resp->crc);
        ptk_set_err(PTK_ERR_CHECKSUM_FAILED);
        return PTK_ERR_CHECKSUM_FAILED;
    }
    
    debug("Deserialized arithmetic response: op_inv=0x%02x, result=%f, crc=0x%02x",
          resp->operation_inverted, resp->result, resp->crc);
    
    return PTK_OK;
}