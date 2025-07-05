
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

struct buf {
    uint8_t *data;
    size_t cursor;
    size_t len;
};

typedef struct buf buf;

typedef enum {
    BUF_OK,
    BUF_ERR_OUT_OF_BOUNDS,
    BUF_ERR_NULL_PTR,
    BUF_ERR_NO_RESOURCES,
    BUF_ERR_BAD_FORMAT,
} buf_err_t;

typedef uint8_t buf_u8;
typedef uint16_t buf_u16_be;
typedef uint16_t buf_u16_le;
typedef uint32_t buf_u32_be;
typedef uint32_t buf_u32_be_bs;
typedef uint32_t buf_u32_le;
typedef uint32_t buf_u32_le_bs;
typedef uint64_t buf_u64_be;
typedef uint64_t buf_u64_be_bs;
typedef uint64_t buf_u64_le;
typedef uint64_t buf_u64_le_bs;

typedef int8_t buf_i8;
typedef int32_t buf_i32_be;
typedef int32_t buf_i32_be_bs;
typedef int32_t buf_i32_le;
typedef int32_t buf_i32_le_bs;
typedef int64_t buf_i64_be;
typedef int64_t buf_i64_be_bs;
typedef int64_t buf_i64_le;
typedef int64_t buf_i64_le_bs;

typedef float buf_f32_be;
typedef float buf_f32_be_bs;
typedef float buf_f32_le;
typedef float buf_f32_le_bs;
typedef double buf_f64_be;
typedef double buf_f64_be_bs;
typedef double buf_f64_le;
typedef double buf_f64_le_bs;

static inline buf_err_t buf_alloc(buf **b, size_t len) {
    if(!b) { return BUF_ERR_NULL_PTR; }
    *b = malloc(sizeof(buf));
    if(!*b) { return BUF_ERR_NO_RESOURCES; }
    (*b)->data = malloc(len);
    if(!(*b)->data) { free(*b); return BUF_ERR_NO_RESOURCES; }
    (*b)->cursor = 0;
    (*b)->len = len;
    return BUF_OK;
}

static inline buf_err_t buf_resize(buf *b, size_t len) {
    uint8_t *new_data = NULL;
    if(!b) { return BUF_ERR_NULL_PTR; }
    new_data = realloc(b->data, len);
    if(!new_data) { return BUF_ERR_NO_RESOURCES; }
    b->data = new_data;
    b->len = len;
    return BUF_OK;
}

static inline void buf_free(buf *b) {
    if(!b) { return; }
    free(b->data);
    free(b);
}

static inline size_t buf_len(buf *b) {
    if(!b) { return 0; }
    return b->len - b->cursor;
}

static inline size_t buf_get_cursor(buf *b) {
    if(!b) { return 0; }
    return b->cursor;
}

static inline void buf_set_cursor(buf *b, size_t cursor) {
    if(!b) { return; }
    if(cursor > b->len) { cursor = b->len; }
    b->cursor = cursor;
}

/**
 * @brief extract a portion of data from source buffer into a new buffer
 *
 * @param src - the source buffer to extract from
 * @param start_offset - byte offset from current cursor position to start extraction
 * @param length - number of bytes to extract
 * @param extracted - pointer to store the newly allocated buffer containing copied data
 * @return BUF_OK on success, error code on failure
 */
extern buf_err_t buf_extract_buf(buf *src, size_t start_offset, size_t length, buf **extracted);

/*
 * Supported format strings.
 *
 * ">" - fields after this are in big endian format
 * "<" - fields after this are in little endian format
 * "$" - fields after this are byte swapped on 16-bit boundaries.
 * "." - skip a byte
 * " " - ignore spaces.
 *
 * "u8" - the value to decode/encode is a single unsigned byte.
 * "i8" - the value to decode/encode is a single signed byte.
 * "u16" - 2-byte unsigned integer
 * "i16" - 2-byte signed integer
 * "u32" - 4-byte unsigned integer
 * "i32" - 4-byte signed integer
 * "u64" - 8-byte unsigned integer
 * "i64" - 8-byte signed integer
 *
 * "f32" - 32-bit float
 * "f64" - 64-bit float
 *
 * Bit array types:
 * "B" - bit array packed into bytes (for decode: bool *bits, for encode: const bool *bits)
 *       Must be prefixed with bit count, e.g. "B8" for 8 bits, "B16" for 16 bits
 *       Bits are packed LSB first within each byte
 *
 * Array notation:
 * - Prefix any type with a number for arrays: "4u16" = array of 4 uint16_t values
 * - For bit arrays: "B12" = 12-bit array (uses 2 bytes, 4 bits unused)
 */


/**
 * @brief decode data from the source buffer according to the format string.
 *
 * @param src - the buffer from which to take data
 * @param peek - if true, do not update the buffer's cursor.  If false, update the cursor.
 * @param tmpl - a C string containing characters indicating what types of values to decode.
 * @param ... Pointer arguments for the values to decode into.
 */
extern buf_err_t buf_decode(buf *src, bool peek, const char *fmt, ...);

/**
 * @brief encode data into the destination buffer according to the format string.
 *
 * @param dst - the buffer in which to encode the data.
 * @param expand - if true, then expand the buffer instead of running out of space. If false, do not change the buffer size.
 * @param fmt - a C string containing character indicating what the type of the values following are.
 * @param ... - the values to encode.
 */
extern buf_err_t buf_encode(buf *dst, bool expand,const char *fmt, ...);
