/**
 * PTK - Data Serialization Implementation
 * 
 * Type-safe serialization/deserialization with endianness control.
 * All functions work with slices and advance the slice position.
 */


#include "ptk_serialization.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// --- Platform-independent endianness helpers ---
static void ptk_write16(uint8_t* out, uint16_t value, ptk_endian_t endian) {
    if (endian == PTK_ENDIAN_LITTLE) {
        out[0] = (uint8_t)(value & 0xFF);
        out[1] = (uint8_t)((value >> 8) & 0xFF);
    } else {
        out[0] = (uint8_t)((value >> 8) & 0xFF);
        out[1] = (uint8_t)(value & 0xFF);
    }
}
static uint16_t ptk_read16(const uint8_t* in, ptk_endian_t endian) {
    if (endian == PTK_ENDIAN_LITTLE) {
        return (uint16_t)in[0] | ((uint16_t)in[1] << 8);
    } else {
        return ((uint16_t)in[0] << 8) | (uint16_t)in[1];
    }
}
static void ptk_write32(uint8_t* out, uint32_t value, ptk_endian_t endian) {
    if (endian == PTK_ENDIAN_LITTLE) {
        out[0] = (uint8_t)(value & 0xFF);
        out[1] = (uint8_t)((value >> 8) & 0xFF);
        out[2] = (uint8_t)((value >> 16) & 0xFF);
        out[3] = (uint8_t)((value >> 24) & 0xFF);
    } else {
        out[0] = (uint8_t)((value >> 24) & 0xFF);
        out[1] = (uint8_t)((value >> 16) & 0xFF);
        out[2] = (uint8_t)((value >> 8) & 0xFF);
        out[3] = (uint8_t)(value & 0xFF);
    }
}
static uint32_t ptk_read32(const uint8_t* in, ptk_endian_t endian) {
    if (endian == PTK_ENDIAN_LITTLE) {
        return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    } else {
        return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | (uint32_t)in[3];
    }
}
static void ptk_write64(uint8_t* out, uint64_t value, ptk_endian_t endian) {
    if (endian == PTK_ENDIAN_LITTLE) {
        for (int i = 0; i < 8; ++i) out[i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    } else {
        for (int i = 0; i < 8; ++i) out[i] = (uint8_t)((value >> (8 * (7 - i))) & 0xFF);
    }
}
static uint64_t ptk_read64(const uint8_t* in, ptk_endian_t endian) {
    uint64_t v = 0;
    if (endian == PTK_ENDIAN_LITTLE) {
        for (int i = 7; i >= 0; --i) v = (v << 8) | in[i];
    } else {
        for (int i = 0; i < 8; ++i) v = (v << 8) | in[i];
    }
    return v;
}

// --- Slice helpers ---
static inline ptk_slice_bytes_t ptk_slice_bytes_advance(ptk_slice_bytes_t slice, size_t n) {
    if (n > slice.len) n = slice.len;
    slice.data += n;
    slice.len -= n;
    return slice;
}

// --- Serialization functions ---
ptk_slice_bytes_t ptk_serialize_u8(ptk_slice_bytes_t slice, uint8_t value) {
    if (slice.len < 1) return (ptk_slice_bytes_t){slice.data, 0};
    slice.data[0] = value;
    return ptk_slice_bytes_advance(slice, 1);
}
ptk_slice_bytes_t ptk_serialize_i8(ptk_slice_bytes_t slice, int8_t value) {
    return ptk_serialize_u8(slice, (uint8_t)value);
}
ptk_slice_bytes_t ptk_serialize_u16(ptk_slice_bytes_t slice, uint16_t value, ptk_endian_t endian) {
    if (slice.len < 2) return (ptk_slice_bytes_t){slice.data, 0};
    ptk_write16(slice.data, value, endian);
    return ptk_slice_bytes_advance(slice, 2);
}
ptk_slice_bytes_t ptk_serialize_i16(ptk_slice_bytes_t slice, int16_t value, ptk_endian_t endian) {
    return ptk_serialize_u16(slice, (uint16_t)value, endian);
}
ptk_slice_bytes_t ptk_serialize_u32(ptk_slice_bytes_t slice, uint32_t value, ptk_endian_t endian) {
    if (slice.len < 4) return (ptk_slice_bytes_t){slice.data, 0};
    ptk_write32(slice.data, value, endian);
    return ptk_slice_bytes_advance(slice, 4);
}
ptk_slice_bytes_t ptk_serialize_i32(ptk_slice_bytes_t slice, int32_t value, ptk_endian_t endian) {
    return ptk_serialize_u32(slice, (uint32_t)value, endian);
}
ptk_slice_bytes_t ptk_serialize_u64(ptk_slice_bytes_t slice, uint64_t value, ptk_endian_t endian) {
    if (slice.len < 8) return (ptk_slice_bytes_t){slice.data, 0};
    ptk_write64(slice.data, value, endian);
    return ptk_slice_bytes_advance(slice, 8);
}
ptk_slice_bytes_t ptk_serialize_i64(ptk_slice_bytes_t slice, int64_t value, ptk_endian_t endian) {
    return ptk_serialize_u64(slice, (uint64_t)value, endian);
}
ptk_slice_bytes_t ptk_serialize_f32(ptk_slice_bytes_t slice, float value, ptk_endian_t endian) {
    union { float f; uint32_t u; } conv; conv.f = value;
    return ptk_serialize_u32(slice, conv.u, endian);
}
ptk_slice_bytes_t ptk_serialize_f64(ptk_slice_bytes_t slice, double value, ptk_endian_t endian) {
    union { double d; uint64_t u; } conv; conv.d = value;
    return ptk_serialize_u64(slice, conv.u, endian);
}

// --- Deserialization functions ---
uint8_t ptk_deserialize_u8(ptk_slice_bytes_t* slice, bool peek) {
    if (!slice || slice->len < 1) return 0;
    uint8_t v = slice->data[0];
    if (!peek) *slice = ptk_slice_bytes_advance(*slice, 1);
    return v;
}
int8_t ptk_deserialize_i8(ptk_slice_bytes_t* slice, bool peek) {
    return (int8_t)ptk_deserialize_u8(slice, peek);
}
uint16_t ptk_deserialize_u16(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    if (!slice || slice->len < 2) return 0;
    uint16_t v = ptk_read16(slice->data, endian);
    if (!peek) *slice = ptk_slice_bytes_advance(*slice, 2);
    return v;
}
int16_t ptk_deserialize_i16(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    return (int16_t)ptk_deserialize_u16(slice, peek, endian);
}
uint32_t ptk_deserialize_u32(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    if (!slice || slice->len < 4) return 0;
    uint32_t v = ptk_read32(slice->data, endian);
    if (!peek) *slice = ptk_slice_bytes_advance(*slice, 4);
    return v;
}
int32_t ptk_deserialize_i32(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    return (int32_t)ptk_deserialize_u32(slice, peek, endian);
}
uint64_t ptk_deserialize_u64(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    if (!slice || slice->len < 8) return 0;
    uint64_t v = ptk_read64(slice->data, endian);
    if (!peek) *slice = ptk_slice_bytes_advance(*slice, 8);
    return v;
}
int64_t ptk_deserialize_i64(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    return (int64_t)ptk_deserialize_u64(slice, peek, endian);
}
float ptk_deserialize_f32(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    uint32_t u = ptk_deserialize_u32(slice, peek, endian);
    union { float f; uint32_t u; } conv; conv.u = u;
    return conv.f;
}
double ptk_deserialize_f64(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian) {
    uint64_t u = ptk_deserialize_u64(slice, peek, endian);
    union { double d; uint64_t u; } conv; conv.u = u;
    return conv.d;
}



