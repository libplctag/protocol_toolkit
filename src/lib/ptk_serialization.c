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
extern void ptk_set_error_internal(ptk_status_t error);


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

/**
 * TYPE-SAFE MULTI-FIELD SERIALIZATION IMPLEMENTATION
 */

#include <stdarg.h>

/**
 * Get size of a type for serialization
 */
static size_t get_type_size(ptk_serialize_type_t type) {
    switch (type) {
        case PTK_SERIALIZE_TYPE_U8:
        case PTK_SERIALIZE_TYPE_S8:
            return 1;
        case PTK_SERIALIZE_TYPE_U16:
        case PTK_SERIALIZE_TYPE_S16:
            return 2;
        case PTK_SERIALIZE_TYPE_U32:
        case PTK_SERIALIZE_TYPE_S32:
        case PTK_SERIALIZE_TYPE_FLOAT:
            return 4;
        case PTK_SERIALIZE_TYPE_U64:
        case PTK_SERIALIZE_TYPE_S64:
        case PTK_SERIALIZE_TYPE_DOUBLE:
            return 8;
        case PTK_SERIALIZE_TYPE_SERIALIZABLE:
            return 0; // Variable size, handled separately
        default:
            return 0;
    }
}

/**
 * Serialize a single value to a slice
 */
static ptk_slice_bytes_t serialize_value(ptk_slice_bytes_t slice, ptk_serialize_type_t type, 
                                        ptk_endian_t endian, va_list* args) {
    switch (type) {
        case PTK_SERIALIZE_TYPE_U8: {
            uint8_t val = (uint8_t)va_arg(*args, int);
            return ptk_write_uint8(slice, val);
        }
        case PTK_SERIALIZE_TYPE_S8: {
            int8_t val = (int8_t)va_arg(*args, int);
            return ptk_write_int8(slice, val);
        }
        case PTK_SERIALIZE_TYPE_U16: {
            uint16_t val = (uint16_t)va_arg(*args, int);
            return ptk_write_uint16(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_S16: {
            int16_t val = (int16_t)va_arg(*args, int);
            return ptk_write_int16(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_U32: {
            uint32_t val = va_arg(*args, uint32_t);
            return ptk_write_uint32(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_S32: {
            int32_t val = va_arg(*args, int32_t);
            return ptk_write_int32(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_U64: {
            uint64_t val = va_arg(*args, uint64_t);
            return ptk_write_uint64(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_S64: {
            int64_t val = va_arg(*args, int64_t);
            return ptk_write_int64(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_FLOAT: {
            float val = (float)va_arg(*args, double);
            return ptk_write_float32(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_DOUBLE: {
            double val = va_arg(*args, double);
            return ptk_write_float64(slice, val, endian);
        }
        case PTK_SERIALIZE_TYPE_SERIALIZABLE: {
            ptk_serializable_t* obj = va_arg(*args, ptk_serializable_t*);
            if (!obj || !obj->serialize) {
                ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
                return (ptk_slice_bytes_t){.data = slice.data, .len = 0};
            }
            ptk_status_t status = obj->serialize(&slice, obj);
            if (status != PTK_OK) {
                ptk_set_error_internal(status);
                return (ptk_slice_bytes_t){.data = slice.data, .len = 0};
            }
            return slice;
        }
        default:
            ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
            return (ptk_slice_bytes_t){.data = slice.data, .len = 0};
    }
}

/**
 * Deserialize a single value from a slice
 */
static ptk_status_t deserialize_value(ptk_slice_bytes_t* slice, bool peek, ptk_serialize_type_t type,
                                     ptk_endian_t endian, va_list* args) {
    ptk_slice_bytes_t temp_slice = *slice;
    
    switch (type) {
        case PTK_SERIALIZE_TYPE_U8: {
            uint8_t* ptr = va_arg(*args, uint8_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_uint8(&temp_slice);
            break;
        }
        case PTK_SERIALIZE_TYPE_S8: {
            int8_t* ptr = va_arg(*args, int8_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_int8(&temp_slice);
            break;
        }
        case PTK_SERIALIZE_TYPE_U16: {
            uint16_t* ptr = va_arg(*args, uint16_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_uint16(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_S16: {
            int16_t* ptr = va_arg(*args, int16_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_int16(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_U32: {
            uint32_t* ptr = va_arg(*args, uint32_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_uint32(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_S32: {
            int32_t* ptr = va_arg(*args, int32_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_int32(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_U64: {
            uint64_t* ptr = va_arg(*args, uint64_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_uint64(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_S64: {
            int64_t* ptr = va_arg(*args, int64_t*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_int64(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_FLOAT: {
            float* ptr = va_arg(*args, float*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_float32(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_DOUBLE: {
            double* ptr = va_arg(*args, double*);
            if (!ptr) return PTK_ERROR_INVALID_PARAM;
            *ptr = ptk_read_float64(&temp_slice, endian);
            break;
        }
        case PTK_SERIALIZE_TYPE_SERIALIZABLE: {
            ptk_serializable_t* obj = va_arg(*args, ptk_serializable_t*);
            if (!obj || !obj->deserialize) {
                return PTK_ERROR_INVALID_PARAM;
            }
            ptk_status_t status = obj->deserialize(&temp_slice, obj);
            if (status != PTK_OK) {
                return status;
            }
            break;
        }
        default:
            return PTK_ERROR_INVALID_PARAM;
    }
    
    // Only advance slice if not peeking and no error occurred
    if (!peek) {
        *slice = temp_slice;
    }
    
    return PTK_OK;
}

/**
 * Implementation function for type-safe serialization
 */
ptk_status_t ptk_serialize_impl(ptk_slice_bytes_t* slice, ptk_endian_t endian, size_t count, ...) {
    if (!slice) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    va_list args;
    va_start(args, count);
    
    ptk_slice_bytes_t current_slice = *slice;
    
    // Process each field (type, value pairs)
    for (size_t i = 0; i < count; i++) {
        ptk_serialize_type_t type = va_arg(args, ptk_serialize_type_t);
        
        current_slice = serialize_value(current_slice, type, endian, &args);
        
        // Check if serialization failed
        if (current_slice.len == 0 && current_slice.data == slice->data) {
            va_end(args);
            return PTK_ERROR_BUFFER_TOO_SMALL;
        }
    }
    
    va_end(args);
    
    // Update the slice to new position
    *slice = current_slice;
    return PTK_OK;
}

/**
 * Implementation function for type-safe deserialization
 */
ptk_status_t ptk_deserialize_impl(ptk_slice_bytes_t* slice, bool peek, ptk_endian_t endian, size_t count, ...) {
    if (!slice) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    va_list args;
    va_start(args, count);
    
    ptk_slice_bytes_t temp_slice = *slice;
    
    // Process each field (type, pointer pairs)
    for (size_t i = 0; i < count; i++) {
        ptk_serialize_type_t type = va_arg(args, ptk_serialize_type_t);
        
        ptk_status_t status = deserialize_value(&temp_slice, peek, type, endian, &args);
        if (status != PTK_OK) {
            va_end(args);
            return status;
        }
    }
    
    va_end(args);
    
    // Only update slice if not peeking
    if (!peek) {
        *slice = temp_slice;
    }
    
    return PTK_OK;
}