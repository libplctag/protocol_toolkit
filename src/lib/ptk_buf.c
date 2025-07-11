#include "ptk_buf.h"
#include "ptk_log.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// NON-INLINE BUFFER FUNCTIONS
//=============================================================================

ptk_buf *ptk_buf_alloc(size_t size) {
    ptk_buf *buf = ptk_alloc(sizeof(ptk_buf), NULL);
    if(!buf) { return NULL; }
    buf->data = ptk_alloc(size, NULL);
    if(!buf->data) {
        ptk_free(buf);
        return NULL;
    }
    buf->data_len = size;
    buf->cursor = 0;
    return buf;
}

ptk_buf *ptk_buf_alloc_from_data(const u8 *data, size_t size) {
    ptk_buf *buf = ptk_buf_alloc(size);
    if(!buf) { return NULL; }
    memcpy(buf->data, data, size);
    buf->cursor = 0;
    return buf;
}

ptk_buf *ptk_buf_realloc(ptk_buf *buf, size_t new_size) {
    if(!buf) { return NULL; }
    u8 *new_data = ptk_realloc(buf->data, new_size);
    if(!new_data) { return NULL; }
    buf->data = new_data;
    buf->data_len = new_size;
    if(buf->cursor > new_size) { buf->cursor = new_size; }
    return buf;
}

size_t ptk_buf_get_size(const ptk_buf *buf) {
    if(!buf) { return 0; }
    return buf->data_len;
}

size_t ptk_buf_get_cursor(const ptk_buf *buf) {
    if(!buf) { return 0; }
    return buf->cursor;
}

ptk_err ptk_buf_set_cursor(ptk_buf *buf, size_t cursor) {
    if(!buf) { return PTK_ERR_NULL_PTR; }
    if(cursor > buf->data_len) { return PTK_ERR_OUT_OF_BOUNDS; }
    buf->cursor = cursor;
    return PTK_OK;
}

ptk_err ptrk_buf_move_block(ptk_buf *buf, size_t new_position, size_t start, size_t len) {
    if(!buf || !buf->data) { return PTK_ERR_NULL_PTR; }
    if(start + len > buf->data_len || new_position + len > buf->data_len) { return PTK_ERR_OUT_OF_BOUNDS; }
    memmove(buf->data + new_position, buf->data + start, len);
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
        case PTK_BUF_TYPE_SERIALIZABLE: return 0;
        default: return 0;
    }
}

/**
 * Helper function to write value with endianness conversion
 */
static ptk_err ptk_buf_write_typed_value(ptk_buf *buf, ptk_buf_type_t type, ptk_buf_endian_t endian, va_list *args) {
    if(!buf || !args) { return PTK_ERR_NULL_PTR; }

    if(type == PTK_BUF_TYPE_SERIALIZABLE) {
        ptk_serializable_t *obj = va_arg(*args, ptk_serializable_t *);
        if(!obj || !obj->serialize) { return PTK_ERR_INVALID_PARAM; }
        return obj->serialize(buf, obj);
    }

    size_t type_size = ptk_buf_type_size(type);
    if(type_size == 0) { return PTK_ERR_INVALID_PARAM; }
    if(buf->cursor + type_size > buf->data_len) { return PTK_ERR_BUFFER_TOO_SMALL; }

    uint8_t *write_ptr = buf->data + buf->cursor;
    bool needs_swap = false;
    if(endian == PTK_BUF_LITTLE_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        needs_swap = true;
#endif
    } else if(endian == PTK_BUF_BIG_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        needs_swap = true;
#endif
    }
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
            float value = (float)va_arg(*args, double);
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
    buf->cursor += type_size;
    return PTK_OK;
}

/**
 * Helper function to read value with endianness conversion
 */
static ptk_err ptk_buf_read_typed_value(ptk_buf *buf, bool peek, ptk_buf_type_t type, ptk_buf_endian_t endian, va_list *args) {
    if(!buf || !args) { return PTK_ERR_NULL_PTR; }
    if(type == PTK_BUF_TYPE_SERIALIZABLE) {
        ptk_serializable_t *obj = va_arg(*args, ptk_serializable_t *);
        if(!obj || !obj->deserialize) { return PTK_ERR_INVALID_PARAM; }
        size_t original_cursor = buf->cursor;
        ptk_err result = obj->deserialize(buf, obj);
        if(peek) { buf->cursor = original_cursor; }
        return result;
    }
    size_t type_size = ptk_buf_type_size(type);
    if(type_size == 0) { return PTK_ERR_INVALID_PARAM; }
    if(buf->cursor + type_size > buf->data_len) { return PTK_ERR_BUFFER_TOO_SMALL; }
    uint8_t *read_ptr = buf->data + buf->cursor;
    bool needs_swap = false;
    if(endian == PTK_BUF_LITTLE_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        needs_swap = true;
#endif
    } else if(endian == PTK_BUF_BIG_ENDIAN) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        needs_swap = true;
#endif
    }
    void *dest_ptr = va_arg(*args, void *);
    if(!dest_ptr) { return PTK_ERR_NULL_PTR; }
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
    if(!peek) { buf->cursor += type_size; }
    return PTK_OK;
}

/**
 * Implementation function for type-safe serialization
 */
ptk_err ptk_buf_serialize_impl(ptk_buf *buf, ptk_buf_endian_t endian, size_t count, ...) {
    if(!buf) { return PTK_ERR_NULL_PTR; }
    if(count == 0) { return PTK_OK; }
    va_list args;
    va_start(args, count);
    size_t original_cursor = buf->cursor;
    ptk_err result = PTK_OK;
    for(size_t i = 0; i < count && result == PTK_OK; i++) {
        ptk_buf_type_t type = va_arg(args, ptk_buf_type_t);
        if(type < PTK_BUF_TYPE_U8 || type > PTK_BUF_TYPE_SERIALIZABLE) {
            result = PTK_ERR_INVALID_PARAM;
            break;
        }
        result = ptk_buf_write_typed_value(buf, type, endian, &args);
        if(result != PTK_OK) { break; }
    }
    va_end(args);
    if(result != PTK_OK) { buf->cursor = original_cursor; }
    return result;
}

/**
 * Implementation function for type-safe deserialization
 */
ptk_err ptk_buf_deserialize_impl(ptk_buf *buf, bool peek, ptk_buf_endian_t endian, size_t count, ...) {
    if(!buf) { return PTK_ERR_NULL_PTR; }
    if(count == 0) { return PTK_OK; }
    va_list args;
    va_start(args, count);
    size_t original_cursor = buf->cursor;
    ptk_err result = PTK_OK;
    for(size_t i = 0; i < count && result == PTK_OK; i++) {
        ptk_buf_type_t type = va_arg(args, ptk_buf_type_t);
        if(type < PTK_BUF_TYPE_U8 || type > PTK_BUF_TYPE_SERIALIZABLE) {
            result = PTK_ERR_INVALID_PARAM;
            break;
        }
        result = ptk_buf_read_typed_value(buf, peek, type, endian, &args);
        if(result != PTK_OK) { break; }
    }
    va_end(args);
    if(result != PTK_OK || peek) { buf->cursor = original_cursor; }
    return result;
}