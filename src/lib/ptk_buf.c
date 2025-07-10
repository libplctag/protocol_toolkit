#include "ptk_buf.h"
#include "ptk_log.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// NON-INLINE BUFFER FUNCTIONS
//=============================================================================

u8 *ptk_buf_get_start_ptr(ptk_buf *buf) {
    if(!buf) {
        error("ptk_buf_get_start_ptr called with NULL buffer");
        return NULL;
    }

    return buf->data + buf->start;
}

ptk_err ptk_buf_set_start(ptk_buf *buf, size_t start) {
    if(!buf) {
        error("ptk_buf_set_start called with NULL buffer");
        return PTK_ERR_NULL_PTR;
    }

    if(start > buf->end) {
        error("ptk_buf_set_start: start %zu > end %zu", start, buf->end);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    buf->start = start;
    trace("Buffer start set to %zu", start);
    return PTK_OK;
}

//=============================================================================
// BUFFER PRODUCE/CONSUME FUNCTIONS
//=============================================================================

/**
 * @brief Parse format string and determine required size
 */


//=============================================================================
// TYPE-SAFE SERIALIZATION IMPLEMENTATION
//=============================================================================

/**
 * Helper function to get type size
 */
static size_t ptk_buf_type_size(ptk_buf_type_t type) {
    switch(type) {
        case PTK_BUF_TYPE_U8:
        case PTK_BUF_TYPE_S8: return 1;
        case PTK_BUF_TYPE_U16:
        case PTK_BUF_TYPE_S16: return 2;
        case PTK_BUF_TYPE_U32:
        case PTK_BUF_TYPE_S32:
        case PTK_BUF_TYPE_FLOAT: return 4;
        case PTK_BUF_TYPE_U64:
        case PTK_BUF_TYPE_S64:
        case PTK_BUF_TYPE_DOUBLE: return 8;
        case PTK_BUF_TYPE_SERIALIZABLE: return 0;  // Variable size, handled by object's serialize method
        default: return 0;
    }
}

/**
 * Helper function to write value with endianness conversion
 */
static ptk_err ptk_buf_write_typed_value(ptk_buf *buf, ptk_buf_type_t type, ptk_buf_endian_t endian, va_list *args) {
    if(!buf || !args) { return PTK_ERR_NULL_PTR; }

    // Handle serializable objects separately
    if(type == PTK_BUF_TYPE_SERIALIZABLE) {
        ptk_serializable_t *obj = va_arg(*args, ptk_serializable_t *);
        if(!obj) {
            error("ptk_buf_write_typed_value: NULL serializable object");
            return PTK_ERR_NULL_PTR;
        }
        if(!obj->serialize) {
            error("ptk_buf_write_typed_value: Serializable object has NULL serialize method");
            return PTK_ERR_INVALID_PARAM;
        }

        // Call the object's serialize method
        ptk_err result = obj->serialize(buf, obj);
        if(result != PTK_OK) {
            error("ptk_buf_write_typed_value: Serializable object serialize failed: %s", ptk_err_to_string(result));
        }
        return result;
    }

    size_t type_size = ptk_buf_type_size(type);
    if(type_size == 0) {
        error("ptk_buf_write_typed_value: Invalid type %d", type);
        return PTK_ERR_INVALID_PARAM;
    }

    // Check if we have enough space
    if(ptk_buf_get_remaining(buf) < type_size) {
        error("ptk_buf_write_typed_value: Not enough space for type %d (need %zu, have %zu)", type, type_size,
              ptk_buf_get_remaining(buf));
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    uint8_t *write_ptr = ptk_buf_get_end_ptr(buf);
    bool needs_swap = false;

    // Determine if byte swapping is needed
    if(endian == PTK_BUF_LITTLE_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        needs_swap = true;
#endif
    } else if(endian == PTK_BUF_BIG_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        needs_swap = true;
#endif
    }
    // PTK_BUF_NATIVE_ENDIAN never needs swapping

    switch(type) {
        case PTK_BUF_TYPE_U8:
        case PTK_BUF_TYPE_S8: {
            uint8_t value = (uint8_t)va_arg(*args, int);
            *write_ptr = value;
            break;
        }
        case PTK_BUF_TYPE_U16:
        case PTK_BUF_TYPE_S16: {
            uint16_t value = (uint16_t)va_arg(*args, int);
            if(needs_swap) { value = __builtin_bswap16(value); }
            memcpy(write_ptr, &value, 2);
            break;
        }
        case PTK_BUF_TYPE_U32:
        case PTK_BUF_TYPE_S32: {
            uint32_t value = va_arg(*args, uint32_t);
            if(needs_swap) { value = __builtin_bswap32(value); }
            memcpy(write_ptr, &value, 4);
            break;
        }
        case PTK_BUF_TYPE_U64:
        case PTK_BUF_TYPE_S64: {
            uint64_t value = va_arg(*args, uint64_t);
            if(needs_swap) { value = __builtin_bswap64(value); }
            memcpy(write_ptr, &value, 8);
            break;
        }
        case PTK_BUF_TYPE_FLOAT: {
            float value = (float)va_arg(*args, double);  // float promoted to double in varargs
            if(needs_swap) {
                uint32_t *int_value = (uint32_t *)&value;
                *int_value = __builtin_bswap32(*int_value);
            }
            memcpy(write_ptr, &value, 4);
            break;
        }
        case PTK_BUF_TYPE_DOUBLE: {
            double value = va_arg(*args, double);
            if(needs_swap) {
                uint64_t *int_value = (uint64_t *)&value;
                *int_value = __builtin_bswap64(*int_value);
            }
            memcpy(write_ptr, &value, 8);
            break;
        }
        default: return PTK_ERR_INVALID_PARAM;
    }

    // Advance buffer
    buf->end += type_size;
    trace("ptk_buf_write_typed_value: Wrote %zu bytes for type %d", type_size, type);
    return PTK_OK;
}

/**
 * Helper function to read value with endianness conversion
 */
static ptk_err ptk_buf_read_typed_value(ptk_buf *buf, bool peek, ptk_buf_type_t type, ptk_buf_endian_t endian, va_list *args) {
    if(!buf || !args) { return PTK_ERR_NULL_PTR; }

    // Handle serializable objects separately
    if(type == PTK_BUF_TYPE_SERIALIZABLE) {
        ptk_serializable_t *obj = va_arg(*args, ptk_serializable_t *);
        if(!obj) {
            error("ptk_buf_read_typed_value: NULL serializable object");
            return PTK_ERR_NULL_PTR;
        }
        if(!obj->deserialize) {
            error("ptk_buf_read_typed_value: Serializable object has NULL deserialize method");
            return PTK_ERR_INVALID_PARAM;
        }

        // For peek operations with serializable objects, we need to save and restore buffer position
        size_t original_start = buf->start;
        ptk_err result = obj->deserialize(buf, obj);
        if(result != PTK_OK) {
            error("ptk_buf_read_typed_value: Serializable object deserialize failed: %s", ptk_err_to_string(result));
        }

        // Restore position if peeking
        if(peek && result == PTK_OK) { buf->start = original_start; }

        return result;
    }

    size_t type_size = ptk_buf_type_size(type);
    if(type_size == 0) {
        error("ptk_buf_read_typed_value: Invalid type %d", type);
        return PTK_ERR_INVALID_PARAM;
    }

    // Check if we have enough data
    if(ptk_buf_len(buf) < type_size) {
        error("ptk_buf_read_typed_value: Not enough data for type %d (need %zu, have %zu)", type, type_size, ptk_buf_len(buf));
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    uint8_t *read_ptr = ptk_buf_get_start_ptr(buf);
    bool needs_swap = false;

    // Determine if byte swapping is needed
    if(endian == PTK_BUF_LITTLE_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        needs_swap = true;
#endif
    } else if(endian == PTK_BUF_BIG_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        needs_swap = true;
#endif
    }
    // PTK_BUF_NATIVE_ENDIAN never needs swapping

    void *dest_ptr = va_arg(*args, void *);
    if(!dest_ptr) {
        error("ptk_buf_read_typed_value: NULL destination pointer for type %d", type);
        return PTK_ERR_NULL_PTR;
    }

    switch(type) {
        case PTK_BUF_TYPE_U8:
        case PTK_BUF_TYPE_S8: {
            *(uint8_t *)dest_ptr = *read_ptr;
            break;
        }
        case PTK_BUF_TYPE_U16:
        case PTK_BUF_TYPE_S16: {
            uint16_t value;
            memcpy(&value, read_ptr, 2);
            if(needs_swap) { value = __builtin_bswap16(value); }
            *(uint16_t *)dest_ptr = value;
            break;
        }
        case PTK_BUF_TYPE_U32:
        case PTK_BUF_TYPE_S32: {
            uint32_t value;
            memcpy(&value, read_ptr, 4);
            if(needs_swap) { value = __builtin_bswap32(value); }
            *(uint32_t *)dest_ptr = value;
            break;
        }
        case PTK_BUF_TYPE_U64:
        case PTK_BUF_TYPE_S64: {
            uint64_t value;
            memcpy(&value, read_ptr, 8);
            if(needs_swap) { value = __builtin_bswap64(value); }
            *(uint64_t *)dest_ptr = value;
            break;
        }
        case PTK_BUF_TYPE_FLOAT: {
            float value;
            memcpy(&value, read_ptr, 4);
            if(needs_swap) {
                uint32_t *int_value = (uint32_t *)&value;
                *int_value = __builtin_bswap32(*int_value);
            }
            *(float *)dest_ptr = value;
            break;
        }
        case PTK_BUF_TYPE_DOUBLE: {
            double value;
            memcpy(&value, read_ptr, 8);
            if(needs_swap) {
                uint64_t *int_value = (uint64_t *)&value;
                *int_value = __builtin_bswap64(*int_value);
            }
            *(double *)dest_ptr = value;
            break;
        }
        default: return PTK_ERR_INVALID_PARAM;
    }

    // Advance buffer only if not peeking
    if(!peek) {
        buf->start += type_size;
        trace("ptk_buf_read_typed_value: Read %zu bytes for type %d", type_size, type);
    } else {
        trace("ptk_buf_read_typed_value: Peeked %zu bytes for type %d", type_size, type);
    }

    return PTK_OK;
}

/**
 * Implementation function for type-safe serialization
 */
ptk_err ptk_buf_serialize_impl(ptk_buf *buf, ptk_buf_endian_t endian, size_t count, ...) {
    if(!buf) {
        error("ptk_buf_serialize_impl: NULL buffer");
        return PTK_ERR_NULL_PTR;
    }

    if(count == 0) {
        trace("ptk_buf_serialize_impl: No fields to serialize");
        return PTK_OK;
    }

    trace("ptk_buf_serialize_impl: Serializing %zu fields with endianness %d", count, endian);

    va_list args;
    va_start(args, count);

    size_t original_end = buf->end;
    ptk_err result = PTK_OK;

    for(size_t i = 0; i < count && result == PTK_OK; i++) {
        // Get type enum
        ptk_buf_type_t type = va_arg(args, ptk_buf_type_t);

        // Validate type
        if(type < PTK_BUF_TYPE_U8 || type > PTK_BUF_TYPE_SERIALIZABLE) {
            error("ptk_buf_serialize_impl: Invalid type %d at field %zu", type, i);
            result = PTK_ERR_INVALID_PARAM;
            break;
        }

        // Write the value
        result = ptk_buf_write_typed_value(buf, type, endian, &args);
        if(result != PTK_OK) {
            error("ptk_buf_serialize_impl: Failed to write field %zu (type %d): %s", i, type, ptk_err_to_string(result));
            break;
        }
    }

    va_end(args);

    if(result != PTK_OK) {
        // Rollback on error
        warn("ptk_buf_serialize_impl: Rolling back buffer end from %zu to %zu", buf->end, original_end);
        buf->end = original_end;
        buf->last_err = result;
    } else {
        info("ptk_buf_serialize_impl: Successfully serialized %zu fields (%zu bytes)", count, buf->end - original_end);
    }

    return result;
}

/**
 * Implementation function for type-safe deserialization
 */
ptk_err ptk_buf_deserialize_impl(ptk_buf *buf, bool peek, ptk_buf_endian_t endian, size_t count, ...) {
    if(!buf) {
        error("ptk_buf_deserialize_impl: NULL buffer");
        return PTK_ERR_NULL_PTR;
    }

    if(count == 0) {
        trace("ptk_buf_deserialize_impl: No fields to deserialize");
        return PTK_OK;
    }

    trace("ptk_buf_deserialize_impl: Deserializing %zu fields with endianness %d (peek=%s)", count, endian,
          peek ? "true" : "false");

    va_list args;
    va_start(args, count);

    size_t original_start = buf->start;
    ptk_err result = PTK_OK;

    for(size_t i = 0; i < count && result == PTK_OK; i++) {
        // Get type enum
        ptk_buf_type_t type = va_arg(args, ptk_buf_type_t);

        // Validate type
        if(type < PTK_BUF_TYPE_U8 || type > PTK_BUF_TYPE_SERIALIZABLE) {
            error("ptk_buf_deserialize_impl: Invalid type %d at field %zu", type, i);
            result = PTK_ERR_INVALID_PARAM;
            break;
        }

        // For peek operations, we always read as if not peeking, but we'll restore the original position at the end
        result = ptk_buf_read_typed_value(buf, false, type, endian, &args);
        if(result != PTK_OK) {
            error("ptk_buf_deserialize_impl: Failed to read field %zu (type %d): %s", i, type, ptk_err_to_string(result));
            break;
        }
    }

    va_end(args);

    if(result != PTK_OK) {
        // Rollback on error
        warn("ptk_buf_deserialize_impl: Rolling back buffer start from %zu to %zu", buf->start, original_start);
        buf->start = original_start;
        buf->last_err = result;
    } else {
        if(peek) {
            // Restore original position for peek operations
            buf->start = original_start;
            info("ptk_buf_deserialize_impl: Successfully peeked %zu fields", count);
        } else {
            info("ptk_buf_deserialize_impl: Successfully deserialized %zu fields (%zu bytes)", count,
                 buf->start - original_start);
        }
    }

    return result;
}