
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ptk_err.h"


#define PTK_BUF_CHUNK_SIZE (1024)


/**
 * @brief This provides the API for data buffer handling.
 *
 * Buffers are allocated out of pools.
 *
 * The data within buffers, fixed size chunks, is also allocated out of pools.
 *
 * This system is _NOT_ threadsafe.
 *
 */


typedef struct ptk_buf_manager ptk_buf_manager;
typedef struct ptk_buf ptk_buf;


/**
 * @brief Creates a new buffer manager.
 *
 * @param manager
 * @param buffer_free_low_water_mark
 * @param buffer_free_high_water_mark
 * @param buffer_chunk_free_low_water_mark
 * @param buffer_chunk_free_high_water_mark
 * @return ptk_err
 */
extern ptk_err ptk_buf_manager_create(ptk_buf_manager *manager, size_t buffer_free_low_water_mark, size_t buffer_free_high_water_mark, size_t chunk_free_low_water_mark, size_t chunk_free_high_water_mark, size_t chunk_size);
extern ptk_err ptk_buf_manager_dispose(ptk_buf_manager *manager);


/**
 * @brief Common types of integers and floating point
 *
 * These are typedefed so that you can use them in structs and at a glance
 * know what the endianness is.
 */


/* bool and bit strings */
typedef uint8_t ptk_bool;

/* bit strings are decoded and encoded as arrays of bool. */
typedef uint16_t ptk_bit_str_u16_be;
typedef uint16_t ptk_bit_str_u16_le;

typedef uint32_t ptk_bit_str_u32_be;
typedef uint32_t ptk_bit_str_u32_be_bs;
typedef uint32_t ptk_bit_str_u32_le;
typedef uint32_t ptk_bit_str_u32_le_bs;

typedef uint64_t ptk_bit_str_u64_be;
typedef uint64_t ptk_bit_str_u64_be_bs;
typedef uint64_t ptk_bit_str_u64_le;
typedef uint64_t ptk_bit_str_u64_le_bs;

/* integers */
typedef uint8_t ptk_u8;
typedef uint16_t ptk_u16_be;
typedef uint16_t ptk_u16_le;
typedef uint32_t ptk_u32_be;
typedef uint32_t ptk_u32_be_bs;
typedef uint32_t ptk_u32_le;
typedef uint32_t ptk_u32_le_bs;
typedef uint64_t ptk_u64_be;
typedef uint64_t ptk_u64_be_bs;
typedef uint64_t ptk_u64_le;
typedef uint64_t ptk_u64_le_bs;

typedef int8_t ptk_i8;
typedef int32_t ptk_i32_be;
typedef int32_t ptk_i32_be_bs;
typedef int32_t ptk_i32_le;
typedef int32_t ptk_i32_le_bs;
typedef int64_t ptk_i64_be;
typedef int64_t ptk_i64_be_bs;
typedef int64_t ptk_i64_le;
typedef int64_t ptk_i64_le_bs;

/* floating point */
typedef float ptk_f32_be;
typedef float ptk_f32_be_bs;
typedef float ptk_f32_le;
typedef float ptk_f32_le_bs;
typedef double ptk_f64_be;
typedef double ptk_f64_be_bs;
typedef double ptk_f64_le;
typedef double ptk_f64_le_bs;



/**
 * @brief Allocate a new buffer out of the pool and give it at least as much space as required.
 *
 * The cursor is set to zero.
 *
 * @param new_buf
 * @param initial_size
 * @return ptk_err
 */
extern ptk_err ptk_buf_alloc(ptk_buf **new_buf, size_t initial_size);

/**
 * @brief Create a new buffer containing data from an existing buffer.
 *
 * The destination buffer is now and the data is copied to it.
 *
 * @param dest
 * @param data a pointer to the data to insert.
 * @param size the number of bytes to insert into the buffer.
 * @param expand_to_fit if true, will expand the buffer when the cursor would go out of bounds.
 * @return ptk_err
 */
extern ptk_err ptk_buf_alloc_from_buf(ptk_buf **new_buf, ptk_buf *src, size_t start, size_t len);

/**
 * @brief Release all resources used by the buffer.
 *
 * @param buf The buffer to release.
 */
extern void ptk_buf_free(ptk_buf *buf);

/**
 * @brief Get the length of the data in the buffer.
 *
 * @param len
 * @param buf
 * @return ptk_err
 */
extern ptk_err ptk_buf_get_len(size_t *len, ptk_buf *buf);


/**
 * @brief Get the current cursor position
 *
 * @param cursor point to where to store the cursor value.
 * @param buf buffer in which to seek.
 * @return error code
 */
extern ptk_err ptk_buf_get_cursor(size_t *cursor, ptk_buf *buf);

/**
 * @brief Seek the cursor to the given position.
 *
 * This will expand the buffer by adding chunks if the cursor is beyond the
 * capacity with the current number of chunks.
 *
 * @param buf
 * @param offset
 * @param expand_to_fit if true, then expand the buffer.
 * @return Error if the cursor is out of bounds and expand_to_fit is false.
 */
extern ptk_err ptk_buf_set_cursor(ptk_buf *buf, size_t offset, bool expand_to_fit);


/**
 * @brief Add raw data to the buffer.
 *
 * This directly adds on to the end of the buffer's data.  It does not move
 * the cursor.  It does change the length of the buffer.
 *
 * @param buf  the buffer in which to add the data
 * @param data the data to add
 * @param len the amount of data to add in bytes
 * @return ptk_err
 */
extern ptk_err ptk_buf_add_raw_data(ptk_buf *buf, const void *data, size_t len);


/*
 * Supported format strings.
 *
 * ptk_bool
 *
 * ptk_bit_str_u16_be
 * ptk_bit_str_u16_le
 *
 * ptk_bit_str_u32_be
 * ptk_bit_str_u32_be_bs
 * ptk_bit_str_u32_le
 * ptk_bit_str_u32_le_bs
 *
 * ptk_bit_str_u64_be
 * ptk_bit_str_u64_be_bs
 * ptk_bit_str_u64_le
 * ptk_bit_str_u64_le_bs
 *
 * ptk_u8
 * ptk_u16_be
 * ptk_u16_le
 * ptk_u32_be
 * ptk_u32_be_bs
 * ptk_u32_le
 * ptk_u32_le_bs
 * ptk_u64_be
 * ptk_u64_be_bs
 * ptk_u64_le
 * ptk_u64_le_bs
 *
 * ptk_i8
 * ptk_i32_be
 * ptk_i32_be_bs
 * ptk_i32_le
 * ptk_i32_le_bs
 * ptk_i64_be
 * ptk_i64_be_bs
 * ptk_i64_le
 * ptk_i64_le_bs
 *
 * ptk_f32_be
 * ptk_f32_be_bs
 * ptk_f32_le
 * ptk_f32_le_bs
 * ptk_f64_be
 * ptk_f64_be_bs
 * ptk_f64_le
 * ptk_f64_le_bs
 *
 *
 * Array notation:
 * - Suffix any type with "[SIZE]".
 * - ptk_u8[FIELD_SIZE]
 * - ptk_f64_be_bs[42]
 *
 */


/**
 * @brief decode data from the source buffer according to the format string.
 *
 * @param src - the buffer from which to take data
 * @param peek - if true, do not update the buffer's cursor.  If false, update the cursor.
 * @param tmpl - a C string containing characters indicating what types of values to decode.
 * @param ... Pointer arguments for the values to decode into.
 */
extern ptk_err ptk_buf_decode(ptk_buf *src, bool peek, const char *fmt, ...);

/**
 * @brief encode data into the destination buffer according to the format string.
 *
 * @param dst - the buffer in which to encode the data.
 * @param expand - if true, then expand the buffer instead of running out of space. If false, do not change the buffer size.
 * @param fmt - a C string containing character indicating what the type of the values following are.
 * @param ... - the values to encode.
 */
extern ptk_err ptk_buf_encode(ptk_buf *dst, bool expand, const char *fmt, ...);
