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
 * Type-specific serialization/deserialization with endianness control.
 * All functions work with slices and advance the slice position.
 */

// Write functions - advance slice and return new position
ptk_slice_bytes_t ptk_serialize_u8(ptk_slice_bytes_t slice, uint8_t value);
ptk_slice_bytes_t ptk_serialize_i8(ptk_slice_bytes_t slice, int8_t value);
ptk_slice_bytes_t ptk_serialize_u16(ptk_slice_bytes_t slice, uint16_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_serialize_i16(ptk_slice_bytes_t slice, int16_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_serialize_u32(ptk_slice_bytes_t slice, uint32_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_serialize_i32(ptk_slice_bytes_t slice, int32_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_serialize_u64(ptk_slice_bytes_t slice, uint64_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_serialize_i64(ptk_slice_bytes_t slice, int64_t value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_serialize_f32(ptk_slice_bytes_t slice, float value, ptk_endian_t endian);
ptk_slice_bytes_t ptk_serialize_f64(ptk_slice_bytes_t slice, double value, ptk_endian_t endian);

// Read functions - advance slice and return value
uint8_t  ptk_deserialize_u8(ptk_slice_bytes_t* slice, bool peek);
int8_t   ptk_deserialize_i8(ptk_slice_bytes_t* slice, bool peek);
uint16_t ptk_deserialize_u16(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);
int16_t  ptk_deserialize_i16(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);
uint32_t ptk_deserialize_u32(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);
int32_t  ptk_deserialize_i32(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);
uint64_t ptk_deserialize_u64(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);
int64_t  ptk_deserialize_i64(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);
float    ptk_deserialize_f32(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);
double   ptk_deserialize_f64(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian);

#ifdef __cplusplus
}
#endif

#endif /* PTK_SERIALIZATION_H */