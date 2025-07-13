#pragma once

#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_alloc.h>

typedef enum {
    ARITH_OP_ADD = 1,
    ARITH_OP_SUB = 2, 
    ARITH_OP_MUL = 3,
    ARITH_OP_DIV = 4
} arithmetic_operation_t;

typedef struct {
    ptk_serializable_t base;
    u8 operation;
    f32 operand1;
    f32 operand2;
    u16 crc;
} arithmetic_request_t;

typedef struct {
    ptk_serializable_t base;
    u8 operation_inverted;
    f64 result;
    u8 crc;
} arithmetic_response_t;

arithmetic_request_t *arithmetic_request_create(arithmetic_operation_t op, f32 op1, f32 op2);
arithmetic_response_t *arithmetic_response_create(arithmetic_operation_t original_op, f64 result);

ptk_err arithmetic_request_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err arithmetic_request_deserialize(ptk_buf *buf, ptk_serializable_t *obj);

ptk_err arithmetic_response_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err arithmetic_response_deserialize(ptk_buf *buf, ptk_serializable_t *obj);