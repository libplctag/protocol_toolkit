/**
 * PTK - Data Serialization Implementation
 * 
 * Type-safe serialization/deserialization with endianness control.
 * All functions work with slices and advance the slice position.
 */

#include "ptk_serialization.h"
#include <string.h>
#include <stdint.h>

/* Forward declarations for error handling */
extern void ptk_clear_error(void);
extern void ptk_set_error_internal_internal(ptk_status_t error);


/**
 * Utility functions for endianness conversion
 */
extern uint16_t ptk_bswap16(uint16_t value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

extern uint32_t ptk_bswap32(uint32_t value) {
    return ((value & 0xFF) << 24) |
           (((value >> 8) & 0xFF) << 16) |
           (((value >> 16) & 0xFF) << 8) |
           ((value >> 24) & 0xFF);
}

extern uint64_t ptk_bswap64(uint64_t value) {
    return ((value & 0xFFULL) << 56) |
           (((value >> 8) & 0xFFULL) << 48) |
           (((value >> 16) & 0xFFULL) << 40) |
           (((value >> 24) & 0xFFULL) << 32) |
           (((value >> 32) & 0xFFULL) << 24) |
           (((value >> 40) & 0xFFULL) << 16) |
           (((value >> 48) & 0xFFULL) << 8) |
           ((value >> 56) & 0xFFULL);
}

extern bool ptk_is_little_endian(void) {
    static const uint32_t test = 0x12345678;
    return (*(const uint8_t*)&test) == 0x78;
}

extern bool ptk_is_big_endian(void) {
    return !ptk_is_little_endian();
}

/**
 * Internal helper to convert value based on endianness
 */
static uint16_t convert_uint16(uint16_t value, ptk_endian_t endian) {
    switch (endian) {
        case PTK_ENDIAN_LITTLE:
            return ptk_is_little_endian() ? value : ptk_bswap16(value);
        case PTK_ENDIAN_BIG:
            return ptk_is_big_endian() ? value : ptk_bswap16(value);
        case PTK_ENDIAN_HOST:
        default:
            return value;
    }
}

static uint32_t convert_uint32(uint32_t value, ptk_endian_t endian) {
    switch (endian) {
        case PTK_ENDIAN_LITTLE:
            return ptk_is_little_endian() ? value : ptk_bswap32(value);
        case PTK_ENDIAN_BIG:
            return ptk_is_big_endian() ? value : ptk_bswap32(value);
        case PTK_ENDIAN_HOST:
        default:
            return value;
    }
}

static uint64_t convert_uint64(uint64_t value, ptk_endian_t endian) {
    switch (endian) {
        case PTK_ENDIAN_LITTLE:
            return ptk_is_little_endian() ? value : ptk_bswap64(value);
        case PTK_ENDIAN_BIG:
            return ptk_is_big_endian() ? value : ptk_bswap64(value);
        case PTK_ENDIAN_HOST:
        default:
            return value;
    }
}

/**
 * Write functions - advance slice and return new position
 */

extern ptk_slice_bytes_t ptk_write_uint8(ptk_slice_bytes_t slice, uint8_t value) {
    if (slice.len < 1) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return (ptk_slice_bytes_t){.data = slice.data, .len = 0};
    }
    
    slice.data[0] = value;
    ptk_clear_error();
    return ptk_slice_bytes_advance(slice, 1);
}

extern ptk_slice_bytes_t ptk_write_int8(ptk_slice_bytes_t slice, int8_t value) {
    return ptk_write_uint8(slice, (uint8_t)value);
}

extern ptk_slice_bytes_t ptk_write_uint16(ptk_slice_bytes_t slice, uint16_t value, ptk_endian_t endian) {
    if (slice.len < 2) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return (ptk_slice_bytes_t){.data = slice.data, .len = 0};
    }
    
    uint16_t converted = convert_uint16(value, endian);
    memcpy(slice.data, &converted, 2);
    
    ptk_clear_error();
    return ptk_slice_bytes_advance(slice, 2);
}

extern ptk_slice_bytes_t ptk_write_int16(ptk_slice_bytes_t slice, int16_t value, ptk_endian_t endian) {
    return ptk_write_uint16(slice, (uint16_t)value, endian);
}

extern ptk_slice_bytes_t ptk_write_uint32(ptk_slice_bytes_t slice, uint32_t value, ptk_endian_t endian) {
    if (slice.len < 4) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return (ptk_slice_bytes_t){.data = slice.data, .len = 0};
    }
    
    uint32_t converted = convert_uint32(value, endian);
    memcpy(slice.data, &converted, 4);
    
    ptk_clear_error();
    return ptk_slice_bytes_advance(slice, 4);
}

extern ptk_slice_bytes_t ptk_write_int32(ptk_slice_bytes_t slice, int32_t value, ptk_endian_t endian) {
    return ptk_write_uint32(slice, (uint32_t)value, endian);
}

extern ptk_slice_bytes_t ptk_write_uint64(ptk_slice_bytes_t slice, uint64_t value, ptk_endian_t endian) {
    if (slice.len < 8) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return (ptk_slice_bytes_t){.data = slice.data, .len = 0};
    }
    
    uint64_t converted = convert_uint64(value, endian);
    memcpy(slice.data, &converted, 8);
    
    ptk_clear_error();
    return ptk_slice_bytes_advance(slice, 8);
}

extern ptk_slice_bytes_t ptk_write_int64(ptk_slice_bytes_t slice, int64_t value, ptk_endian_t endian) {
    return ptk_write_uint64(slice, (uint64_t)value, endian);
}

extern ptk_slice_bytes_t ptk_write_float32(ptk_slice_bytes_t slice, float value, ptk_endian_t endian) {
    /* Use union for type punning */
    union { float f; uint32_t u; } converter;
    converter.f = value;
    return ptk_write_uint32(slice, converter.u, endian);
}

extern ptk_slice_bytes_t ptk_write_float64(ptk_slice_bytes_t slice, double value, ptk_endian_t endian) {
    /* Use union for type punning */
    union { double d; uint64_t u; } converter;
    converter.d = value;
    return ptk_write_uint64(slice, converter.u, endian);
}

/**
 * Convenience functions for specific endianness
 */
extern ptk_slice_bytes_t ptk_write_uint16_le(ptk_slice_bytes_t slice, uint16_t value) {
    return ptk_write_uint16(slice, value, PTK_ENDIAN_LITTLE);
}

extern ptk_slice_bytes_t ptk_write_uint16_be(ptk_slice_bytes_t slice, uint16_t value) {
    return ptk_write_uint16(slice, value, PTK_ENDIAN_BIG);
}

extern ptk_slice_bytes_t ptk_write_uint32_le(ptk_slice_bytes_t slice, uint32_t value) {
    return ptk_write_uint32(slice, value, PTK_ENDIAN_LITTLE);
}

extern ptk_slice_bytes_t ptk_write_uint32_be(ptk_slice_bytes_t slice, uint32_t value) {
    return ptk_write_uint32(slice, value, PTK_ENDIAN_BIG);
}

extern ptk_slice_bytes_t ptk_write_uint64_le(ptk_slice_bytes_t slice, uint64_t value) {
    return ptk_write_uint64(slice, value, PTK_ENDIAN_LITTLE);
}

extern ptk_slice_bytes_t ptk_write_uint64_be(ptk_slice_bytes_t slice, uint64_t value) {
    return ptk_write_uint64(slice, value, PTK_ENDIAN_BIG);
}

extern ptk_slice_bytes_t ptk_write_float32_le(ptk_slice_bytes_t slice, float value) {
    return ptk_write_float32(slice, value, PTK_ENDIAN_LITTLE);
}

extern ptk_slice_bytes_t ptk_write_float32_be(ptk_slice_bytes_t slice, float value) {
    return ptk_write_float32(slice, value, PTK_ENDIAN_BIG);
}

extern ptk_slice_bytes_t ptk_write_float64_le(ptk_slice_bytes_t slice, double value) {
    return ptk_write_float64(slice, value, PTK_ENDIAN_LITTLE);
}

extern ptk_slice_bytes_t ptk_write_float64_be(ptk_slice_bytes_t slice, double value) {
    return ptk_write_float64(slice, value, PTK_ENDIAN_BIG);
}

/**
 * Read functions - advance slice and return value
 */

extern uint8_t ptk_read_uint8(ptk_slice_bytes_t* slice) {
    if (!slice || slice->len < 1) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return 0;
    }
    
    uint8_t value = slice->data[0];
    *slice = ptk_slice_bytes_advance(*slice, 1);
    
    ptk_clear_error();
    return value;
}

extern int8_t ptk_read_int8(ptk_slice_bytes_t* slice) {
    return (int8_t)ptk_read_uint8(slice);
}

extern uint16_t ptk_read_uint16(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    if (!slice || slice->len < 2) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return 0;
    }
    
    uint16_t value;
    memcpy(&value, slice->data, 2);
    value = convert_uint16(value, endian);
    
    *slice = ptk_slice_bytes_advance(*slice, 2);
    
    ptk_clear_error();
    return value;
}

extern int16_t ptk_read_int16(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    return (int16_t)ptk_read_uint16(slice, endian);
}

extern uint32_t ptk_read_uint32(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    if (!slice || slice->len < 4) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return 0;
    }
    
    uint32_t value;
    memcpy(&value, slice->data, 4);
    value = convert_uint32(value, endian);
    
    *slice = ptk_slice_bytes_advance(*slice, 4);
    
    ptk_clear_error();
    return value;
}

extern int32_t ptk_read_int32(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    return (int32_t)ptk_read_uint32(slice, endian);
}

extern uint64_t ptk_read_uint64(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    if (!slice || slice->len < 8) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return 0;
    }
    
    uint64_t value;
    memcpy(&value, slice->data, 8);
    value = convert_uint64(value, endian);
    
    *slice = ptk_slice_bytes_advance(*slice, 8);
    
    ptk_clear_error();
    return value;
}

extern int64_t ptk_read_int64(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    return (int64_t)ptk_read_uint64(slice, endian);
}

extern float ptk_read_float32(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    uint32_t value = ptk_read_uint32(slice, endian);
    union { float f; uint32_t u; } converter;
    converter.u = value;
    return converter.f;
}

extern double ptk_read_float64(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    uint64_t value = ptk_read_uint64(slice, endian);
    union { double d; uint64_t u; } converter;
    converter.u = value;
    return converter.d;
}

/**
 * Convenience functions for specific endianness
 */
extern uint16_t ptk_read_uint16_le(ptk_slice_bytes_t* slice) {
    return ptk_read_uint16(slice, PTK_ENDIAN_LITTLE);
}

extern uint16_t ptk_read_uint16_be(ptk_slice_bytes_t* slice) {
    return ptk_read_uint16(slice, PTK_ENDIAN_BIG);
}

extern uint32_t ptk_read_uint32_le(ptk_slice_bytes_t* slice) {
    return ptk_read_uint32(slice, PTK_ENDIAN_LITTLE);
}

extern uint32_t ptk_read_uint32_be(ptk_slice_bytes_t* slice) {
    return ptk_read_uint32(slice, PTK_ENDIAN_BIG);
}

extern uint64_t ptk_read_uint64_le(ptk_slice_bytes_t* slice) {
    return ptk_read_uint64(slice, PTK_ENDIAN_LITTLE);
}

extern uint64_t ptk_read_uint64_be(ptk_slice_bytes_t* slice) {
    return ptk_read_uint64(slice, PTK_ENDIAN_BIG);
}

extern float ptk_read_float32_le(ptk_slice_bytes_t* slice) {
    return ptk_read_float32(slice, PTK_ENDIAN_LITTLE);
}

extern float ptk_read_float32_be(ptk_slice_bytes_t* slice) {
    return ptk_read_float32(slice, PTK_ENDIAN_BIG);
}

extern double ptk_read_float64_le(ptk_slice_bytes_t* slice) {
    return ptk_read_float64(slice, PTK_ENDIAN_LITTLE);
}

extern double ptk_read_float64_be(ptk_slice_bytes_t* slice) {
    return ptk_read_float64(slice, PTK_ENDIAN_BIG);
}

/**
 * Bulk operations for arrays
 */
extern ptk_slice_bytes_t ptk_write_bytes(ptk_slice_bytes_t dest, ptk_slice_bytes_t src) {
    if (dest.len < src.len) {
        ptk_set_error_internal(PTK_ERROR_BUFFER_TOO_SMALL);
        return (ptk_slice_bytes_t){.data = dest.data, .len = 0};
    }
    
    memcpy(dest.data, src.data, src.len);
    
    ptk_clear_error();
    return ptk_slice_bytes_advance(dest, src.len);
}

extern ptk_slice_bytes_t ptk_read_bytes(ptk_slice_bytes_t* src, ptk_slice_bytes_t dest) {
    if (!src) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return (ptk_slice_bytes_t){.data = NULL, .len = 0};
    }
    
    size_t copy_len = (src->len < dest.len) ? src->len : dest.len;
    
    memcpy(dest.data, src->data, copy_len);
    *src = ptk_slice_bytes_advance(*src, copy_len);
    
    ptk_clear_error();
    return ptk_slice_bytes_truncate(dest, copy_len);
}