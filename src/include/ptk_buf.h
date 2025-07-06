#pragma once

/**
 * @file minimal_buf_api.h
 * @brief Minimal buffer API for embedded systems
 *
 * Ultra-simple buffer management with only essential operations.
 * No pools, no complex chaining - just basic buffer operations.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ptk_err.h"

typedef enum {
    PTK_BUF_BIG_ENDIAN,
    PTK_BUF_BIG_ENDIAN_BYTE_SWAP,
    PTK_BUF_LITTLE_ENDIAN,
    PTK_BUF_LITTLE_ENDIAN_BYTE_SWAP,
} ptk_buf_endianness;


//=============================================================================
// BUFFER STRUCTURE
//=============================================================================

/**
 * @brief Simple buffer structure
 */
typedef struct ptk_buf {
    uint8_t *data;              // Buffer data
    size_t capacity;            // Total buffer capacity
    size_t start;
    size_t end;
} ptk_buf;

//=============================================================================
// BUFFER OPERATIONS
//=============================================================================

/**
 * @brief Make a new buffer from existing data.
 *
 * Sets start and end to zero.  Sets the capacity to the passed size.
 *
 * @param buf Pointer to buffer to update
 * @param data pointer to the raw memory buffer
 * @param size Initial buffer size
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_buf_make(ptk_buf *buf, uint8_t *data, size_t size);

/**
 * @brief Gets the amount of data between start and end.
 *
 * Note that the end index is exclusive.
 *
 * @param len
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_len(size_t *len, ptk_buf *buf);

/**
 * @brief Get the capacity of the buffer.
 *
 * @param cap
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_cap(size_t *cap, ptk_buf *buf);

/**
 * @brief Get the current start position
 *
 * @param start
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_start(size_t *start, ptk_buf *buf);

/**
 * @brief Get a data pointer to the start index
 *
 * @param ptr
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_start_ptr(uint8_t **ptr, ptk_buf *buf);

/**
 * @brief Set the start positions
 *
 * Sets the start position to anywhere between 0 and end.
 *
 * @param start
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_set_start(ptk_buf *buf, size_t start);

/**
 * @brief Get the current end position
 *
 *
 * @param end
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_end(size_t *end, ptk_buf *buf);

/**
 * @brief Get a data pointer to the end index.
 *
 * @param ptr
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_end_ptr(uint8_t **ptr, ptk_buf *buf);

/**
 * @brief Set the end position
 *
 * Sets the end position to anywhere between start and capacity.
 *
 * @param buf
 * @param end
 * @return ptk_err
 */
ptk_err ptk_buf_set_end(ptk_buf *buf, size_t end);

/**
 * @brief Get the remaining space from the end index to the capacity of the buffer.
 *
 * @param remaining
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_remaining(size_t *remaining, ptk_buf *buf);

/**
 * @brief Moves the data between start and end to the new start position
 *
 * This moves the data between start and end to the new start position.  It updates
 * start to the new start and updates end so that it is correct with the new position
 * of the data within the buffer.
 *
 * @param buf
 * @param new_start
 * @return ptk_err OK on success, OUT_OF_BOUNDs if the move would truncate data.
 */
ptk_err ptk_buf_move_to(ptk_buf *buf, size_t new_start);


//=============================================================================
// ENDIAN-SPECIFIC HELPERS
//=============================================================================

/**
 * Consumers use the data at the start index and change it after consuming
 * the data, unless peek is set to true.
 *
 * Producers set the new data at the end and change the index after.
 */

/**
 * @brief Write 8-bit value
 */
ptk_err ptk_buf_produce_u8(ptk_buf *buf, uint8_t value);

/**
 * @brief Write 16-bit value
 */
ptk_err ptk_buf_produce_u16(ptk_buf *buf, uint16_t value, ptk_buf_endianness endianness);

/**
 * @brief Write 32-bit value
 */
ptk_err ptk_buf_produce_u32(ptk_buf *buf, uint32_t value, ptk_buf_endianness endianness);

/**
 * @brief Write 64-bit value
 */
ptk_err ptk_buf_produce_u64(ptk_buf *buf, uint64_t value, ptk_buf_endianness endianness);



/**
 * @brief Read 8-bit value
 *
 * If peek is set, then do not change the start index.
 */
ptk_err ptk_buf_consume_u8(ptk_buf *buf, uint8_t *value, bool peek);

/**
 * @brief Read 16-bit value
 */
ptk_err ptk_buf_consume_u16(ptk_buf *buf, uint16_t *value, ptk_buf_endianness endianness, bool peek);

/**
 * @brief Read 32-bit value
 */
ptk_err ptk_buf_consume_u32(ptk_buf *buf, uint32_t *value, ptk_buf_endianness endianness, bool peek);

/**
 * @brief Read 64-bit value
 */
ptk_err ptk_buf_consume_u64(ptk_buf *buf, uint64_t *value, ptk_buf_endianness endianness, bool peek);


/* wrappers for signed integers and floating point */
static inline ptk_err ptk_buf_produce_i8(ptk_buf *buf, int8_t value) { return ptk_buf_produce_u8(buf, (uint8_t)value); }
static inline ptk_err ptk_buf_produce_i16(ptk_buf *buf, int16_t value, ptk_buf_endianness endianness) { return ptk_buf_produce_u16(buf, (uint16_t)value, endianness); }
static inline ptk_err ptk_buf_produce_i32(ptk_buf *buf, int32_t value, ptk_buf_endianness endianness) { return ptk_buf_produce_u32(buf, (uint32_t)value, endianness); }
static inline ptk_err ptk_buf_produce_i64(ptk_buf *buf, int64_t value, ptk_buf_endianness endianness) { return ptk_buf_produce_u64(buf, (uint64_t)value, endianness); }

static inline ptk_err ptk_buf_consume_i8(ptk_buf *buf, int8_t *value, bool peek) { return ptk_buf_consume_u8(buf, (uint8_t *)value, peek); }
static inline ptk_err ptk_buf_consume_i16(ptk_buf *buf, int16_t *value, ptk_buf_endianness endianness, bool peek) { return ptk_buf_consume_u16(buf, (uint16_t *)value, endianness, peek); }
static inline ptk_err ptk_buf_consume_i32(ptk_buf *buf, int32_t *value, ptk_buf_endianness endianness, bool peek) { return ptk_buf_consume_u32(buf, (uint32_t *)value, endianness, peek); }
static inline ptk_err ptk_buf_consume_i64(ptk_buf *buf, int64_t *value, ptk_buf_endianness endianness, bool peek) { return ptk_buf_consume_u64(buf, (uint64_t *)value, endianness, peek); }

static inline ptk_err ptk_buf_produce_f32(ptk_buf *buf, float value, ptk_buf_endianness endianness) { return ptk_buf_produce_u32(buf, *(uint32_t *)&value, endianness); }
static inline ptk_err ptk_buf_produce_f64(ptk_buf *buf, double value, ptk_buf_endianness endianness) { return ptk_buf_produce_u64(buf, *(uint64_t *)&value, endianness); }

static inline ptk_err ptk_buf_consume_f32(ptk_buf *buf, float *value, ptk_buf_endianness endianness, bool peek) { return ptk_buf_consume_u32(buf, (uint32_t *)value, endianness, peek); }
static inline ptk_err ptk_buf_consume_f64(ptk_buf *buf, double *value, ptk_buf_endianness endianness, bool peek) { return ptk_buf_consume_u64(buf, (uint64_t *)value, endianness, peek); }
