#include "ptk_buf.h"
#include "ptk_log.h"
#include "ptk_alloc.h"
#include "ptk_err.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// NON-INLINE BUFFER FUNCTIONS
//=============================================================================

ptk_buf *ptk_buf_alloc(buf_size_t size) {
    if (size == 0) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    ptk_buf *buf = ptk_alloc(sizeof(ptk_buf), NULL);
    if(!buf) { 
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL; 
    }
    
    buf->data = ptk_alloc(size, NULL);
    if(!buf->data) {
        ptk_free(buf);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    buf->data_len = size;
    buf->start = 0;
    buf->end = 0;
    return buf;
}

ptk_buf *ptk_buf_alloc_from_data(const u8 *data, buf_size_t size) {
    if (!data) {
        ptk_set_err(PTK_ERR_NULL_PTR);
        return NULL;
    }
    
    ptk_buf *buf = ptk_buf_alloc(size);
    if(!buf) { 
        return NULL; 
    }
    
    memcpy(buf->data, data, size);
    buf->start = 0;
    buf->end = size;
    return buf;
}

ptk_buf *ptk_buf_realloc(ptk_buf *buf, buf_size_t new_size) {
    if(!buf) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return NULL; 
    }
    if(new_size == 0) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    u8 *new_data = ptk_realloc(buf->data, new_size);
    if(!new_data) { 
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL; 
    }
    
    buf->data = new_data;
    buf->data_len = new_size;
    
    // Adjust start and end if they exceed new size
    if(buf->start > new_size) { buf->start = new_size; }
    if(buf->end > new_size) { buf->end = new_size; }
    
    return buf;
}

buf_size_t ptk_buf_get_len(const ptk_buf *buf) {
    if(!buf) { return 0; }
    return buf->end - buf->start;
}

buf_size_t ptk_buf_get_capacity(const ptk_buf *buf) {
    if(!buf) { return 0; }
    return buf->data_len;
}

buf_size_t ptk_buf_get_start(const ptk_buf *buf) {
    if(!buf) { return 0; }
    return buf->start;
}

ptk_err ptk_buf_set_start(ptk_buf *buf, buf_size_t start) {
    if(!buf) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR; 
    }
    if(start > buf->data_len) { 
        ptk_set_err(PTK_ERR_OUT_OF_BOUNDS);
        return PTK_ERR_OUT_OF_BOUNDS; 
    }
    buf->start = start;
    return PTK_OK;
}

buf_size_t ptk_buf_get_end(const ptk_buf *buf) {
    if(!buf) { return 0; }
    return buf->end;
}

ptk_err ptk_buf_set_end(ptk_buf *buf, buf_size_t end) {
    if(!buf) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR; 
    }
    if(end > buf->data_len) { 
        ptk_set_err(PTK_ERR_OUT_OF_BOUNDS);
        return PTK_ERR_OUT_OF_BOUNDS; 
    }
    buf->end = end;
    return PTK_OK;
}

ptk_err ptk_buf_move_block(ptk_buf *buf, buf_size_t new_position) {
    if(!buf || !buf->data) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR; 
    }
    
    buf_size_t block_size = buf->end - buf->start;
    if(new_position + block_size > buf->data_len) { 
        ptk_set_err(PTK_ERR_OUT_OF_BOUNDS);
        return PTK_ERR_OUT_OF_BOUNDS; 
    }
    
    memmove(buf->data + new_position, buf->data + buf->start, block_size);
    buf->start = new_position;
    buf->end = new_position + block_size;
    
    return PTK_OK;
}

//=============================================================================
// SIMPLE BYTE ACCESS IMPLEMENTATION
//=============================================================================

ptk_err ptk_buf_set_u8(ptk_buf *buf, u8 val) {
    if (!buf) {
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR;
    }
    
    if (buf->end >= buf->data_len) {
        ptk_set_err(PTK_ERR_BUFFER_TOO_SMALL);
        return PTK_ERR_BUFFER_TOO_SMALL;
    }
    
    buf->data[buf->end] = val;
    buf->end++;
    
    return PTK_OK;
}

u8 ptk_buf_get_u8(ptk_buf *buf) {
    if (!buf) {
        ptk_set_err(PTK_ERR_NULL_PTR);
        return 0;
    }
    
    if (buf->start >= buf->end) {
        ptk_set_err(PTK_ERR_BUFFER_TOO_SMALL);
        return 0;
    }
    
    u8 val = buf->data[buf->start];
    buf->start++;
    
    return val;
}

//=============================================================================
// GENERIC BYTE MANIPULATION AND ENDIANNESS HELPERS
//=============================================================================

/**
 * Generic byte array to integer conversion (always reads as little endian)
 */
static inline uint64_t bytes_to_u64(const uint8_t *bytes, size_t size) {
    uint64_t result = 0;
    for (size_t i = 0; i < size && i < 8; i++) {
        result |= ((uint64_t)bytes[i]) << (i * 8);
    }
    return result;
}

/**
 * Generic integer to byte array conversion (always writes as little endian)
 */
static inline void u64_to_bytes(uint64_t value, uint8_t *bytes, size_t size) {
    for (size_t i = 0; i < size && i < 8; i++) {
        bytes[i] = (uint8_t)((value >> (i * 8)) & 0xFF);
    }
}

/**
 * Unified endianness conversion function
 * Converts value from/to specified endianness by reversing byte order if needed
 */
static inline uint64_t convert_endian(uint64_t value, size_t size, ptk_buf_endian_t endian) {
    if (endian == PTK_BUF_LITTLE_ENDIAN) return value;
    
    uint64_t result = 0;
    for (size_t i = 0; i < size && i < 8; i++) {
        uint8_t byte = (value >> (i * 8)) & 0xFF;
        result |= ((uint64_t)byte) << ((size - 1 - i) * 8);
    }
    return result;
}

/**
 * Type size lookup table
 */
static const size_t type_sizes[] = {
    [PTK_BUF_TYPE_U8] = 1,
    [PTK_BUF_TYPE_S8] = 1,
    [PTK_BUF_TYPE_U16] = 2,
    [PTK_BUF_TYPE_S16] = 2,
    [PTK_BUF_TYPE_U32] = 4,
    [PTK_BUF_TYPE_S32] = 4,
    [PTK_BUF_TYPE_FLOAT] = 4,
    [PTK_BUF_TYPE_U64] = 8,
    [PTK_BUF_TYPE_S64] = 8,
    [PTK_BUF_TYPE_DOUBLE] = 8,
    [PTK_BUF_TYPE_SERIALIZABLE] = 0,
};

/**
 * Wrapper functions for the macros
 */
static inline size_t ptk_buf_type_size(ptk_buf_type_t type) {
    if (type >= sizeof(type_sizes) / sizeof(type_sizes[0])) return 0;
    return type_sizes[type];
}

static inline uint64_t ptk_buf_convert_endian(uint64_t value, size_t size, ptk_buf_endian_t endian) {
    return convert_endian(value, size, endian);
}

static inline void ptk_buf_u64_to_bytes(uint64_t value, uint8_t *bytes, size_t size) {
    u64_to_bytes(value, bytes, size);
}

static inline uint64_t ptk_buf_bytes_to_u64(const uint8_t *bytes, size_t size) {
    return bytes_to_u64(bytes, size);
}

//=============================================================================
// CONVENIENCE MACROS FOR TYPE-SPECIFIC FUNCTIONS
//=============================================================================

/**
 * Macro to generate type-specific write functions
 * Usage: PTK_BUF_GENERATE_WRITE_FUNC(u16, uint16_t, PTK_BUF_TYPE_U16)
 */
#define PTK_BUF_GENERATE_WRITE_FUNC(suffix, ctype, type_enum) \
static inline ptk_err ptk_buf_write_##suffix(ptk_buf *buf, ctype value, ptk_buf_endian_t endian) { \
    if(!buf) return PTK_ERR_NULL_PTR; \
    size_t type_size = ptk_buf_type_size(type_enum); \
    if(buf->end + type_size > buf->data_len) return PTK_ERR_BUFFER_TOO_SMALL; \
    uint64_t u64_val = (uint64_t)value; \
    if(type_enum == PTK_BUF_TYPE_FLOAT) memcpy(&u64_val, &value, 4); \
    if(type_enum == PTK_BUF_TYPE_DOUBLE) memcpy(&u64_val, &value, 8); \
    u64_val = ptk_buf_convert_endian(u64_val, type_size, endian); \
    ptk_buf_u64_to_bytes(u64_val, buf->data + buf->end, type_size); \
    buf->end += type_size; \
    return PTK_OK; \
}

/**
 * Macro to generate type-specific read functions
 * Usage: PTK_BUF_GENERATE_READ_FUNC(u16, uint16_t, PTK_BUF_TYPE_U16)
 */
#define PTK_BUF_GENERATE_READ_FUNC(suffix, ctype, type_enum) \
static inline ptk_err ptk_buf_read_##suffix(ptk_buf *buf, bool peek, ctype *dest, ptk_buf_endian_t endian) { \
    if(!buf || !dest) return PTK_ERR_NULL_PTR; \
    size_t type_size = ptk_buf_type_size(type_enum); \
    if(buf->start + type_size > buf->end) return PTK_ERR_BUFFER_TOO_SMALL; \
    uint64_t value = ptk_buf_bytes_to_u64(buf->data + buf->start, type_size); \
    value = ptk_buf_convert_endian(value, type_size, endian); \
    if(type_enum == PTK_BUF_TYPE_FLOAT) { uint32_t tmp = (uint32_t)value; memcpy(dest, &tmp, 4); } \
    else if(type_enum == PTK_BUF_TYPE_DOUBLE) { memcpy(dest, &value, 8); } \
    else *dest = (ctype)value; \
    if(!peek) buf->start += type_size; \
    return PTK_OK; \
}

// Generate commonly used type-specific functions
PTK_BUF_GENERATE_WRITE_FUNC(u8, uint8_t, PTK_BUF_TYPE_U8)
PTK_BUF_GENERATE_WRITE_FUNC(u16, uint16_t, PTK_BUF_TYPE_U16)
PTK_BUF_GENERATE_WRITE_FUNC(u32, uint32_t, PTK_BUF_TYPE_U32)
PTK_BUF_GENERATE_WRITE_FUNC(u64, uint64_t, PTK_BUF_TYPE_U64)
PTK_BUF_GENERATE_WRITE_FUNC(float, float, PTK_BUF_TYPE_FLOAT)
PTK_BUF_GENERATE_WRITE_FUNC(double, double, PTK_BUF_TYPE_DOUBLE)

PTK_BUF_GENERATE_READ_FUNC(u8, uint8_t, PTK_BUF_TYPE_U8)
PTK_BUF_GENERATE_READ_FUNC(u16, uint16_t, PTK_BUF_TYPE_U16)
PTK_BUF_GENERATE_READ_FUNC(u32, uint32_t, PTK_BUF_TYPE_U32)
PTK_BUF_GENERATE_READ_FUNC(u64, uint64_t, PTK_BUF_TYPE_U64)
PTK_BUF_GENERATE_READ_FUNC(float, float, PTK_BUF_TYPE_FLOAT)
PTK_BUF_GENERATE_READ_FUNC(double, double, PTK_BUF_TYPE_DOUBLE)

//=============================================================================
// TYPE-SAFE SERIALIZATION IMPLEMENTATION
//=============================================================================

/**
 * Helper function to get type size using lookup table
 */
static inline size_t get_type_size(ptk_buf_type_t type) {
    if (type >= sizeof(type_sizes)/sizeof(type_sizes[0])) {
        return 0;
    }
    return type_sizes[type];
}

/**
 * Helper function to write value with endianness conversion
 */
static ptk_err write_typed_value(ptk_buf *buf, ptk_buf_type_t type, ptk_buf_endian_t endian, va_list *args) {
    if(!buf || !args) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR; 
    }

    if(type == PTK_BUF_TYPE_SERIALIZABLE) {
        ptk_serializable_t *obj = va_arg(*args, ptk_serializable_t *);
        if(!obj || !obj->serialize) { return PTK_ERR_INVALID_PARAM; }
        return obj->serialize(buf, obj);
    }

    size_t type_size = ptk_buf_type_size(type);
    if(type_size == 0) { return PTK_ERR_INVALID_PARAM; }
    if(buf->end + type_size > buf->data_len) { return PTK_ERR_BUFFER_TOO_SMALL; }

    uint64_t value = 0;
    
    // Extract value from va_list based on type
    switch(type) {
        case PTK_BUF_TYPE_U8:
        case PTK_BUF_TYPE_S8:
            value = (uint8_t)va_arg(*args, int);
            break;
        case PTK_BUF_TYPE_U16:
        case PTK_BUF_TYPE_S16:
            value = (uint16_t)va_arg(*args, int);
            break;
        case PTK_BUF_TYPE_U32:
        case PTK_BUF_TYPE_S32:
            value = va_arg(*args, uint32_t);
            break;
        case PTK_BUF_TYPE_U64:
        case PTK_BUF_TYPE_S64:
            value = va_arg(*args, uint64_t);
            break;
        case PTK_BUF_TYPE_FLOAT: {
            float f = (float)va_arg(*args, double);
            memcpy(&value, &f, 4);
            break;
        }
        case PTK_BUF_TYPE_DOUBLE: {
            double d = va_arg(*args, double);
            memcpy(&value, &d, 8);
            break;
        }
        default: 
            ptk_set_err(PTK_ERR_INVALID_PARAM);
            return PTK_ERR_INVALID_PARAM;
    }
    
    // Convert endianness and write to buffer
    value = ptk_buf_convert_endian(value, type_size, endian);
    ptk_buf_u64_to_bytes(value, buf->data + buf->end, type_size);
    buf->end += type_size;
    
    return PTK_OK;
}

/**
 * Helper function to read value with endianness conversion
 */
static ptk_err read_typed_value(ptk_buf *buf, bool peek, ptk_buf_type_t type, ptk_buf_endian_t endian, va_list *args) {
    if(!buf || !args) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR; 
    }
    
    if(type == PTK_BUF_TYPE_SERIALIZABLE) {
        ptk_serializable_t *obj = va_arg(*args, ptk_serializable_t *);
        if(!obj || !obj->deserialize) { return PTK_ERR_INVALID_PARAM; }
        buf_size_t original_start = buf->start;
        ptk_err result = obj->deserialize(buf, obj);
        if(peek) { buf->start = original_start; }
        return result;
    }
    
    size_t type_size = ptk_buf_type_size(type);
    if(type_size == 0) { return PTK_ERR_INVALID_PARAM; }
    if(buf->start + type_size > buf->end) { return PTK_ERR_BUFFER_TOO_SMALL; }
    
    void *dest_ptr = va_arg(*args, void *);
    if(!dest_ptr) { return PTK_ERR_NULL_PTR; }
    
    // Read bytes as little endian, convert endianness, then store
    uint64_t value = ptk_buf_bytes_to_u64(buf->data + buf->start, type_size);
    value = ptk_buf_convert_endian(value, type_size, endian);
    
    // Store value based on type
    switch(type) {
        case PTK_BUF_TYPE_U8:
        case PTK_BUF_TYPE_S8:
            *(uint8_t *)dest_ptr = (uint8_t)value;
            break;
        case PTK_BUF_TYPE_U16:
        case PTK_BUF_TYPE_S16:
            *(uint16_t *)dest_ptr = (uint16_t)value;
            break;
        case PTK_BUF_TYPE_U32:
        case PTK_BUF_TYPE_S32:
            *(uint32_t *)dest_ptr = (uint32_t)value;
            break;
        case PTK_BUF_TYPE_U64:
        case PTK_BUF_TYPE_S64:
            *(uint64_t *)dest_ptr = value;
            break;
        case PTK_BUF_TYPE_FLOAT: {
            uint32_t int_val = (uint32_t)value;
            memcpy(dest_ptr, &int_val, 4);
            break;
        }
        case PTK_BUF_TYPE_DOUBLE:
            memcpy(dest_ptr, &value, 8);
            break;
        default: 
            ptk_set_err(PTK_ERR_INVALID_PARAM);
            return PTK_ERR_INVALID_PARAM;
    }
    
    if(!peek) { buf->start += type_size; }
    return PTK_OK;
}

/**
 * Implementation function for type-safe serialization
 */
ptk_err ptk_buf_serialize_impl(ptk_buf *buf, ptk_buf_endian_t endian, buf_size_t count, ...) {
    if(!buf) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR; 
    }
    if(count == 0) { return PTK_OK; }
    va_list args;
    va_start(args, count);
    buf_size_t original_end = buf->end;
    ptk_err result = PTK_OK;
    for(buf_size_t i = 0; i < count && result == PTK_OK; i++) {
        ptk_buf_type_t type = va_arg(args, ptk_buf_type_t);
        if(type < PTK_BUF_TYPE_U8 || type > PTK_BUF_TYPE_SERIALIZABLE) {
            result = PTK_ERR_INVALID_PARAM;
            ptk_set_err(PTK_ERR_INVALID_PARAM);
            break;
        }
        result = write_typed_value(buf, type, endian, &args);
        if(result != PTK_OK) { break; }
    }
    va_end(args);
    if(result != PTK_OK) { 
        buf->end = original_end; 
        ptk_set_err(result);
    }
    return result;
}

/**
 * Implementation function for type-safe deserialization
 */
ptk_err ptk_buf_deserialize_impl(ptk_buf *buf, bool peek, ptk_buf_endian_t endian, buf_size_t count, ...) {
    if(!buf) { 
        ptk_set_err(PTK_ERR_NULL_PTR);
        return PTK_ERR_NULL_PTR; 
    }
    if(count == 0) { return PTK_OK; }
    va_list args;
    va_start(args, count);
    buf_size_t original_start = buf->start;
    ptk_err result = PTK_OK;
    for(buf_size_t i = 0; i < count && result == PTK_OK; i++) {
        ptk_buf_type_t type = va_arg(args, ptk_buf_type_t);
        if(type < PTK_BUF_TYPE_U8 || type > PTK_BUF_TYPE_SERIALIZABLE) {
            result = PTK_ERR_INVALID_PARAM;
            ptk_set_err(PTK_ERR_INVALID_PARAM);
            break;
        }
        result = read_typed_value(buf, peek, type, endian, &args);
        if(result != PTK_OK) { break; }
    }
    va_end(args);
    if(result != PTK_OK || peek) { 
        buf->start = original_start; 
        if(result != PTK_OK) ptk_set_err(result);
    }
    return result;
}