#pragma once

/**
 * @file ptk_codec.h
 * @brief Protocol encoding and decoding utilities
 *
 * Provides type definitions and encoding/decoding functions for protocol
 * data using safe byte arrays. Supports various endianness and byte ordering.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ptk_err.h"
#include "ptk_array.h"

//=============================================================================
// TYPE DEFINITIONS
//=============================================================================

// Standard type aliases for protocol definitions (without endian suffix)
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;

//=============================================================================
// ENDIANNESS DEFINITIONS
//=============================================================================

typedef enum {
    PTK_CODEC_BIG_ENDIAN,
    PTK_CODEC_BIG_ENDIAN_BYTE_SWAP,
    PTK_CODEC_LITTLE_ENDIAN,
    PTK_CODEC_LITTLE_ENDIAN_BYTE_SWAP,
} ptk_codec_endianness_t;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

typedef struct ptk_buf_t ptk_buf_t;

//=============================================================================
// ENCODING FUNCTIONS
//=============================================================================

/**
 * @brief Encode 8-bit value to buffer
 */
ptk_err ptk_codec_produce_u8(ptk_buf_t *buf, u8 value);

/**
 * @brief Encode 16-bit value to buffer
 */
ptk_err ptk_codec_produce_u16(ptk_buf_t *buf, u16 value, ptk_codec_endianness_t endianness);

/**
 * @brief Encode 32-bit value to buffer
 */
ptk_err ptk_codec_produce_u32(ptk_buf_t *buf, u32 value, ptk_codec_endianness_t endianness);

/**
 * @brief Encode 64-bit value to buffer
 */
ptk_err ptk_codec_produce_u64(ptk_buf_t *buf, u64 value, ptk_codec_endianness_t endianness);

//=============================================================================
// DECODING FUNCTIONS
//=============================================================================

/**
 * @brief Decode 8-bit value from buffer
 * @param buf Buffer to read from
 * @param value Pointer to store decoded value
 * @param peek If true, don't advance buffer position
 */
ptk_err ptk_codec_consume_u8(ptk_buf_t *buf, u8 *value, bool peek);

/**
 * @brief Decode 16-bit value from buffer
 */
ptk_err ptk_codec_consume_u16(ptk_buf_t *buf, u16 *value, ptk_codec_endianness_t endianness, bool peek);

/**
 * @brief Decode 32-bit value from buffer
 */
ptk_err ptk_codec_consume_u32(ptk_buf_t *buf, u32 *value, ptk_codec_endianness_t endianness, bool peek);

/**
 * @brief Decode 64-bit value from buffer
 */
ptk_err ptk_codec_consume_u64(ptk_buf_t *buf, u64 *value, ptk_codec_endianness_t endianness, bool peek);

//=============================================================================
// SIGNED INTEGER WRAPPERS
//=============================================================================

static inline ptk_err ptk_codec_produce_i8(ptk_buf_t *buf, i8 value) { 
    return ptk_codec_produce_u8(buf, (u8)value); 
}

static inline ptk_err ptk_codec_produce_i16(ptk_buf_t *buf, i16 value, ptk_codec_endianness_t endianness) { 
    return ptk_codec_produce_u16(buf, (u16)value, endianness); 
}

static inline ptk_err ptk_codec_produce_i32(ptk_buf_t *buf, i32 value, ptk_codec_endianness_t endianness) { 
    return ptk_codec_produce_u32(buf, (u32)value, endianness); 
}

static inline ptk_err ptk_codec_produce_i64(ptk_buf_t *buf, i64 value, ptk_codec_endianness_t endianness) { 
    return ptk_codec_produce_u64(buf, (u64)value, endianness); 
}

static inline ptk_err ptk_codec_consume_i8(ptk_buf_t *buf, i8 *value, bool peek) { 
    return ptk_codec_consume_u8(buf, (u8 *)value, peek); 
}

static inline ptk_err ptk_codec_consume_i16(ptk_buf_t *buf, i16 *value, ptk_codec_endianness_t endianness, bool peek) { 
    return ptk_codec_consume_u16(buf, (u16 *)value, endianness, peek); 
}

static inline ptk_err ptk_codec_consume_i32(ptk_buf_t *buf, i32 *value, ptk_codec_endianness_t endianness, bool peek) { 
    return ptk_codec_consume_u32(buf, (u32 *)value, endianness, peek); 
}

static inline ptk_err ptk_codec_consume_i64(ptk_buf_t *buf, i64 *value, ptk_codec_endianness_t endianness, bool peek) { 
    return ptk_codec_consume_u64(buf, (u64 *)value, endianness, peek); 
}

//=============================================================================
// FLOATING POINT WRAPPERS
//=============================================================================

static inline ptk_err ptk_codec_produce_f32(ptk_buf_t *buf, f32 value, ptk_codec_endianness_t endianness) { 
    return ptk_codec_produce_u32(buf, *(u32 *)&value, endianness); 
}

static inline ptk_err ptk_codec_produce_f64(ptk_buf_t *buf, f64 value, ptk_codec_endianness_t endianness) { 
    return ptk_codec_produce_u64(buf, *(u64 *)&value, endianness); 
}

static inline ptk_err ptk_codec_consume_f32(ptk_buf_t *buf, f32 *value, ptk_codec_endianness_t endianness, bool peek) { 
    return ptk_codec_consume_u32(buf, (u32 *)value, endianness, peek); 
}

static inline ptk_err ptk_codec_consume_f64(ptk_buf_t *buf, f64 *value, ptk_codec_endianness_t endianness, bool peek) { 
    return ptk_codec_consume_u64(buf, (u64 *)value, endianness, peek); 
}

//=============================================================================
// BYTE ARRAY ENCODING/DECODING
//=============================================================================

/**
 * @brief Encode value directly to byte array at specific offset
 */
ptk_err ptk_codec_encode_u8_to_array(u8_array_t *data, size_t offset, u8 value);
ptk_err ptk_codec_encode_u16_to_array(u8_array_t *data, size_t offset, u16 value, ptk_codec_endianness_t endianness);
ptk_err ptk_codec_encode_u32_to_array(u8_array_t *data, size_t offset, u32 value, ptk_codec_endianness_t endianness);
ptk_err ptk_codec_encode_u64_to_array(u8_array_t *data, size_t offset, u64 value, ptk_codec_endianness_t endianness);

/**
 * @brief Decode value directly from byte array at specific offset
 */
ptk_err ptk_codec_decode_u8_from_array(const u8_array_t *data, size_t offset, u8 *value);
ptk_err ptk_codec_decode_u16_from_array(const u8_array_t *data, size_t offset, u16 *value, ptk_codec_endianness_t endianness);
ptk_err ptk_codec_decode_u32_from_array(const u8_array_t *data, size_t offset, u32 *value, ptk_codec_endianness_t endianness);
ptk_err ptk_codec_decode_u64_from_array(const u8_array_t *data, size_t offset, u64 *value, ptk_codec_endianness_t endianness);

//=============================================================================
// BYTE ORDER MAP UTILITIES
//=============================================================================

/**
 * @brief Apply custom byte order mapping to data
 * @param dest Destination array
 * @param dest_offset Offset in destination
 * @param src Source data
 * @param src_size Size of source data
 * @param byte_order_map Array mapping destination byte positions to source positions
 */
ptk_err ptk_codec_apply_byte_order_map(u8_array_t *dest, size_t dest_offset,
                                      const u8 *src, size_t src_size,
                                      const size_t *byte_order_map);

/**
 * @brief Reverse byte order mapping for decoding
 */
ptk_err ptk_codec_reverse_byte_order_map(u8 *dest, size_t dest_size,
                                        const u8_array_t *src, size_t src_offset,
                                        const size_t *byte_order_map);

//=============================================================================
// VALIDATION UTILITIES
//=============================================================================

/**
 * @brief Validate that array has sufficient data at offset
 */
ptk_err ptk_codec_validate_array_bounds(const u8_array_t *data, size_t offset, size_t required_size);

/**
 * @brief Validate buffer has sufficient data for operation
 */
ptk_err ptk_codec_validate_buffer_bounds(const ptk_buf_t *buf, size_t required_size);