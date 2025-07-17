#pragma once

#include <ptk_buf.h>
#include <ptk_err.h>

typedef enum {
    ARITH_OP_ADD = 1,
    ARITH_OP_SUB = 2, 
    ARITH_OP_MUL = 3,
    ARITH_OP_DIV = 4
} arithmetic_operation_t;

typedef struct {
    ptk_serializable_t base;
    ptk_u8_t operation;
    ptk_f32_t operand1;
    ptk_f32_t operand2;
    ptk_u16_t crc;
} arithmetic_request_t;

typedef struct {
    ptk_serializable_t base;
    ptk_u8_t operation_inverted;
    ptk_f64_t result;
    ptk_u8_t crc;
} arithmetic_response_t;

arithmetic_request_t *arithmetic_request_create(arithmetic_operation_t op, ptk_f32_t op1, ptk_f32_t op2);
arithmetic_response_t *arithmetic_response_create(arithmetic_operation_t original_op, ptk_f64_t result);

ptk_err_t arithmetic_request_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err_t arithmetic_request_deserialize(ptk_buf *buf, ptk_serializable_t *obj);

ptk_err_t arithmetic_response_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err_t arithmetic_response_deserialize(ptk_buf *buf, ptk_serializable_t *obj);