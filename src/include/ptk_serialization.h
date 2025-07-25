#ifndef PTK_SERIALIZATION_H
#define PTK_SERIALIZATION_H

#include "ptk_types.h"
#include "ptk_slice.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Data Serialization Functions
 * 
 * Type-safe serialization/deserialization with endianness control.
 * All functions work with slices and advance the slice position.
 */

/**
 * Write functions - advance slice and return new position
 * Return empty slice on error (insufficient space)
 */

/* 8-bit values (no endianness) */
ptk_slice_bytes_t ptk_write_uint8(ptk_slice_bytes_t slice, uint8_t value);
ptk_slice_bytes_t ptk_write_int8(ptk_slice_bytes_t slice, int8_t value);

/* 16-bit values with endianness */
ptk_slice_bytes_t ptk_write_uint16(ptk_slice_bytes_t slice, uint16_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_int16(ptk_slice_bytes_t slice, int16_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_uint16_le(ptk_slice_bytes_t slice, uint16_t value);
ptk_slice_bytes_t ptk_write_uint16_be(ptk_slice_bytes_t slice, uint16_t value);

/* 32-bit values with endianness */
ptk_slice_bytes_t ptk_write_uint32(ptk_slice_bytes_t slice, uint32_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_int32(ptk_slice_bytes_t slice, int32_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_uint32_le(ptk_slice_bytes_t slice, uint32_t value);
ptk_slice_bytes_t ptk_write_uint32_be(ptk_slice_bytes_t slice, uint32_t value);

/* 64-bit values with endianness */
ptk_slice_bytes_t ptk_write_uint64(ptk_slice_bytes_t slice, uint64_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_int64(ptk_slice_bytes_t slice, int64_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_uint64_le(ptk_slice_bytes_t slice, uint64_t value);
ptk_slice_bytes_t ptk_write_uint64_be(ptk_slice_bytes_t slice, uint64_t value);

/* Floating point values with endianness */
ptk_slice_bytes_t ptk_write_float32(ptk_slice_bytes_t slice, float value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_float64(ptk_slice_bytes_t slice, double value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_write_float32_le(ptk_slice_bytes_t slice, float value);
ptk_slice_bytes_t ptk_write_float32_be(ptk_slice_bytes_t slice, float value);
ptk_slice_bytes_t ptk_write_float64_le(ptk_slice_bytes_t slice, double value);
ptk_slice_bytes_t ptk_write_float64_be(ptk_slice_bytes_t slice, double value);

/**
 * Read functions - advance slice and return value
 * Set error in thread-local storage on failure
 */

/* 8-bit values (no endianness) */
uint8_t ptk_read_uint8(ptk_slice_bytes_t* slice);
int8_t ptk_read_int8(ptk_slice_bytes_t* slice);

/* 16-bit values with endianness */
uint16_t ptk_read_uint16(ptk_slice_bytes_t* slice, ptk_endian_t endian);
int16_t ptk_read_int16(ptk_slice_bytes_t* slice, ptk_endian_t endian);
uint16_t ptk_read_uint16_le(ptk_slice_bytes_t* slice);
uint16_t ptk_read_uint16_be(ptk_slice_bytes_t* slice);

/* 32-bit values with endianness */
uint32_t ptk_read_uint32(ptk_slice_bytes_t* slice, ptk_endian_t endian);
int32_t ptk_read_int32(ptk_slice_bytes_t* slice, ptk_endian_t endian);
uint32_t ptk_read_uint32_le(ptk_slice_bytes_t* slice);
uint32_t ptk_read_uint32_be(ptk_slice_bytes_t* slice);

/* 64-bit values with endianness */
uint64_t ptk_read_uint64(ptk_slice_bytes_t* slice, ptk_endian_t endian);
int64_t ptk_read_int64(ptk_slice_bytes_t* slice, ptk_endian_t endian);
uint64_t ptk_read_uint64_le(ptk_slice_bytes_t* slice);
uint64_t ptk_read_uint64_be(ptk_slice_bytes_t* slice);

/* Floating point values with endianness */
float ptk_read_float32(ptk_slice_bytes_t* slice, ptk_endian_t endian);
double ptk_read_float64(ptk_slice_bytes_t* slice, ptk_endian_t endian);
float ptk_read_float32_le(ptk_slice_bytes_t* slice);
float ptk_read_float32_be(ptk_slice_bytes_t* slice);
double ptk_read_float64_le(ptk_slice_bytes_t* slice);
double ptk_read_float64_be(ptk_slice_bytes_t* slice);

/**
 * Bulk operations for arrays
 */
ptk_slice_bytes_t ptk_write_bytes(ptk_slice_bytes_t dest, ptk_slice_bytes_t src);
ptk_slice_bytes_t ptk_read_bytes(ptk_slice_bytes_t* src, ptk_slice_bytes_t dest);

/**
 * Utility functions for endianness conversion
 */
uint16_t ptk_bswap16(uint16_t value);
uint32_t ptk_bswap32(uint32_t value);
uint64_t ptk_bswap64(uint64_t value);

bool ptk_is_little_endian(void);
bool ptk_is_big_endian(void);

#ifdef __cplusplus
}
#endif

#endif /* PTK_SERIALIZATION_H */