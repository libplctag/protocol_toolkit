#pragma once

/**
 * @file ptk_buf.h
 * @brief Safe buffer API using type-safe arrays
 *
 * Buffer management using safe byte arrays with start/end pointers
 * for stream processing operations.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "ptk_err.h"
#include "ptk_array.h"
#include "ptk_alloc.h"


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
// BUFFER STRUCTURE
//=============================================================================

typedef struct ptk_buf {
    ptk_allocator_t *allocator;
    u8 *data;
    size_t capacity;
    size_t start;  // Start position for reading
    size_t end;    // End position (exclusive)
    ptk_err last_err;
} ptk_buf;


//=============================================================================
// BUFFER OPERATIONS
//=============================================================================

static inline ptk_buf *ptk_buf_create(ptk_allocator_t *alloc, size_t size) {
    ptk_buf *buf = NULL;

    if(!alloc) { return NULL; }

    buf = ptk_alloc(alloc, sizeof(ptk_buf));
    if(!buf) { return NULL; }

    buf->data = ptk_alloc(alloc, size);
    if(!buf->data) {
        ptk_free(alloc, buf);
        return NULL;
    }

    /* store the allocator for later. */
    buf->allocator = alloc;
    buf->capacity = size;
    buf->start = 0;
    buf->end = 0;
    buf->last_err = PTK_OK;

    return buf;
}

static inline ptk_err ptk_buf_dispose(ptk_buf *buf) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(buf->allocator) {
        if(buf->data) {
            ptk_free(buf->allocator, buf->data);
            buf->data = NULL;
        }

        ptk_free(buf->allocator, buf);
    } else {
        return PTK_ERR_NULL_PTR;
    }

    return PTK_OK;
}

static inline size_t ptk_buf_len(ptk_buf *buf) {
    if(!buf) { return SIZE_MAX; }

    return buf->end - buf->start;
}

static inline size_t ptk_buf_cap(ptk_buf *buf) {
    if(!buf) { return SIZE_MAX; }

    return buf->capacity;
}

static inline size_t ptk_buf_get_start(ptk_buf *buf) {
    if(!buf) { return SIZE_MAX; }

    return buf->start;
}

extern u8 *ptk_buf_get_start_ptr(ptk_buf *buf);

extern ptk_err ptk_buf_set_start(ptk_buf *buf, size_t start);

static inline size_t ptk_buf_get_end(ptk_buf *buf) {
    if(!buf) { return SIZE_MAX; }

    return buf->end;
}

static inline u8 *ptk_buf_get_end_ptr(ptk_buf *buf) {
    if(!buf) { return NULL; }

    return buf->data + buf->end;
}

static inline ptk_err ptk_buf_set_end(ptk_buf *buf, size_t end) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(end < buf->start || end > buf->capacity) { return PTK_ERR_OUT_OF_BOUNDS; }

    buf->end = end;
    return PTK_OK;
}

static inline size_t ptk_buf_get_remaining(ptk_buf *buf) {
    if(!buf) { return SIZE_MAX; }

    return buf->capacity - buf->end;
}

/**
 * @brief Moves the block of data between start and end to the new start location.
 *
 * @param buf
 * @param new_start
 * @return ptk_err
 */
static inline ptk_err ptk_buf_move_to(ptk_buf *buf, size_t new_start) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    size_t data_len = buf->end - buf->start;

    // Check if new position would fit the data
    if(new_start + data_len > buf->capacity) { return PTK_ERR_OUT_OF_BOUNDS; }

    // Move data if there's any and positions are different
    if(data_len > 0 && new_start != buf->start) { memmove(buf->data + new_start, buf->data + buf->start, data_len); }

    buf->start = new_start;
    buf->end = new_start + data_len;

    return PTK_OK;
}

//=============================================================================
// TYPE-SAFE SERIALIZATION API
//=============================================================================

// Forward declarations for type-safe serialization
typedef enum {
    PTK_BUF_TYPE_U8 = 1,     // uint8_t
    PTK_BUF_TYPE_U16,        // uint16_t
    PTK_BUF_TYPE_U32,        // uint32_t
    PTK_BUF_TYPE_U64,        // uint64_t
    PTK_BUF_TYPE_S8,         // int8_t
    PTK_BUF_TYPE_S16,        // int16_t
    PTK_BUF_TYPE_S32,        // int32_t
    PTK_BUF_TYPE_S64,        // int64_t
    PTK_BUF_TYPE_FLOAT,      // float
    PTK_BUF_TYPE_DOUBLE,     // double
} ptk_buf_type_t;

typedef enum {
    PTK_BUF_LITTLE_ENDIAN = 0,
    PTK_BUF_BIG_ENDIAN = 1,
    PTK_BUF_NATIVE_ENDIAN = 2
} ptk_buf_endian_t;

/**
 * Implementation function for type-safe serialization
 *
 * @param buf Buffer to write to
 * @param endian Endianness for multi-byte values
 * @param count Number of fields to serialize
 * @param ... Alternating type_enum, value pairs
 * @return Error code
 */
extern ptk_err ptk_buf_serialize_impl(ptk_buf *buf, ptk_buf_endian_t endian, size_t count, ...);

/**
 * Implementation function for type-safe deserialization
 *
 * @param buf Buffer to read from
 * @param peek Whether to peek (don't advance buffer position)
 * @param endian Endianness for multi-byte values
 * @param count Number of fields to deserialize
 * @param ... Alternating type_enum, pointer pairs
 * @return Error code
 */
extern ptk_err ptk_buf_deserialize_impl(ptk_buf *buf, bool peek, ptk_buf_endian_t endian, size_t count, ...);

/* byte swap helpers */

/**
 * @brief swap the bytes in each 16-bit word
 *
 * @param value
 * @return u32
 */
static inline u32 ptk_buf_byte_swap_u32(u32 value) {
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0xFF000000) >> 24);
}

static inline u64 ptk_buf_byte_swap_u64(u64 value) {
    return ((value & 0x00000000000000FFULL) << 56) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x00000000FF000000ULL) << 8)  |
           ((value & 0x000000FF00000000ULL) >> 8)  |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0xFF00000000000000ULL) >> 56);
}

//=============================================================================
// TYPE-SAFE SERIALIZATION MACROS
//=============================================================================

/**
 * Map C types to type enumeration using _Generic
 */
#define PTK_BUF_TYPE_OF(x) _Generic((x), \
    uint8_t:  PTK_BUF_TYPE_U8, \
    uint16_t: PTK_BUF_TYPE_U16, \
    uint32_t: PTK_BUF_TYPE_U32, \
    uint64_t: PTK_BUF_TYPE_U64, \
    int8_t:   PTK_BUF_TYPE_S8, \
    int16_t:  PTK_BUF_TYPE_S16, \
    int32_t:  PTK_BUF_TYPE_S32, \
    int64_t:  PTK_BUF_TYPE_S64, \
    float:    PTK_BUF_TYPE_FLOAT, \
    double:   PTK_BUF_TYPE_DOUBLE, \
    default:  0 \
)

/**
 * For pointers, dereference to get the type
 */
#define PTK_BUF_TYPE_OF_PTR(ptr) PTK_BUF_TYPE_OF(*(ptr))

//=============================================================================
// ARGUMENT COUNTING MACROS
//=============================================================================

/**
 * Count arguments (up to 64 arguments, so 32 fields max)
 */
#define PTK_BUF_ARG_COUNT(...) \
    PTK_BUF_ARG_COUNT_IMPL(__VA_ARGS__, \
    64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49, \
    48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33, \
    32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17, \
    16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)

#define PTK_BUF_ARG_COUNT_IMPL( \
    _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
    _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
    _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
    _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
    _61,_62,_63,_64,N,...) N

//=============================================================================
// TYPE ENUMERATION EXPANSION MACROS
//=============================================================================

/**
 * Expand arguments to type, value pairs
 * For each argument, generate: PTK_BUF_TYPE_OF(arg), arg
 */

// 1 argument
#define PTK_BUF_EXPAND_1(a) \
    PTK_BUF_TYPE_OF(a), (a)

// 2 arguments
#define PTK_BUF_EXPAND_2(a, b) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b)

// 3 arguments
#define PTK_BUF_EXPAND_3(a, b, c) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b), \
    PTK_BUF_TYPE_OF(c), (c)

// 4 arguments
#define PTK_BUF_EXPAND_4(a, b, c, d) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b), \
    PTK_BUF_TYPE_OF(c), (c), \
    PTK_BUF_TYPE_OF(d), (d)

// 5 arguments
#define PTK_BUF_EXPAND_5(a, b, c, d, e) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b), \
    PTK_BUF_TYPE_OF(c), (c), \
    PTK_BUF_TYPE_OF(d), (d), \
    PTK_BUF_TYPE_OF(e), (e)

// 6 arguments (EIP header size)
#define PTK_BUF_EXPAND_6(a, b, c, d, e, f) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b), \
    PTK_BUF_TYPE_OF(c), (c), \
    PTK_BUF_TYPE_OF(d), (d), \
    PTK_BUF_TYPE_OF(e), (e), \
    PTK_BUF_TYPE_OF(f), (f)

// 7 arguments
#define PTK_BUF_EXPAND_7(a, b, c, d, e, f, g) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b), \
    PTK_BUF_TYPE_OF(c), (c), \
    PTK_BUF_TYPE_OF(d), (d), \
    PTK_BUF_TYPE_OF(e), (e), \
    PTK_BUF_TYPE_OF(f), (f), \
    PTK_BUF_TYPE_OF(g), (g)

// 8 arguments
#define PTK_BUF_EXPAND_8(a, b, c, d, e, f, g, h) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b), \
    PTK_BUF_TYPE_OF(c), (c), \
    PTK_BUF_TYPE_OF(d), (d), \
    PTK_BUF_TYPE_OF(e), (e), \
    PTK_BUF_TYPE_OF(f), (f), \
    PTK_BUF_TYPE_OF(g), (g), \
    PTK_BUF_TYPE_OF(h), (h)

// 10 arguments
#define PTK_BUF_EXPAND_10(a, b, c, d, e, f, g, h, i, j) \
    PTK_BUF_TYPE_OF(a), (a), \
    PTK_BUF_TYPE_OF(b), (b), \
    PTK_BUF_TYPE_OF(c), (c), \
    PTK_BUF_TYPE_OF(d), (d), \
    PTK_BUF_TYPE_OF(e), (e), \
    PTK_BUF_TYPE_OF(f), (f), \
    PTK_BUF_TYPE_OF(g), (g), \
    PTK_BUF_TYPE_OF(h), (h), \
    PTK_BUF_TYPE_OF(i), (i), \
    PTK_BUF_TYPE_OF(j), (j)

// Add more as needed up to 32 arguments...

/**
 * Select appropriate expansion macro based on argument count
 */
#define PTK_BUF_EXPAND_SELECT(N) PTK_BUF_EXPAND_##N
#define PTK_BUF_EXPAND_DISPATCH(N) PTK_BUF_EXPAND_SELECT(N)
#define PTK_BUF_EXPAND(...) PTK_BUF_EXPAND_DISPATCH(PTK_BUF_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)

//=============================================================================
// POINTER TYPE ENUMERATION FOR DESERIALIZATION
//=============================================================================

/**
 * Expand pointer arguments to type, pointer pairs
 * For each pointer argument, generate: PTK_BUF_TYPE_OF_PTR(ptr), ptr
 */

// 1 pointer
#define PTK_BUF_EXPAND_PTR_1(a) \
    PTK_BUF_TYPE_OF_PTR(a), (a)

// 2 pointers
#define PTK_BUF_EXPAND_PTR_2(a, b) \
    PTK_BUF_TYPE_OF_PTR(a), (a), \
    PTK_BUF_TYPE_OF_PTR(b), (b)

// 3 pointers
#define PTK_BUF_EXPAND_PTR_3(a, b, c) \
    PTK_BUF_TYPE_OF_PTR(a), (a), \
    PTK_BUF_TYPE_OF_PTR(b), (b), \
    PTK_BUF_TYPE_OF_PTR(c), (c)

// 4 pointers
#define PTK_BUF_EXPAND_PTR_4(a, b, c, d) \
    PTK_BUF_TYPE_OF_PTR(a), (a), \
    PTK_BUF_TYPE_OF_PTR(b), (b), \
    PTK_BUF_TYPE_OF_PTR(c), (c), \
    PTK_BUF_TYPE_OF_PTR(d), (d)

// 5 pointers
#define PTK_BUF_EXPAND_PTR_5(a, b, c, d, e) \
    PTK_BUF_TYPE_OF_PTR(a), (a), \
    PTK_BUF_TYPE_OF_PTR(b), (b), \
    PTK_BUF_TYPE_OF_PTR(c), (c), \
    PTK_BUF_TYPE_OF_PTR(d), (d), \
    PTK_BUF_TYPE_OF_PTR(e), (e)

// 6 pointers (EIP header size)
#define PTK_BUF_EXPAND_PTR_6(a, b, c, d, e, f) \
    PTK_BUF_TYPE_OF_PTR(a), (a), \
    PTK_BUF_TYPE_OF_PTR(b), (b), \
    PTK_BUF_TYPE_OF_PTR(c), (c), \
    PTK_BUF_TYPE_OF_PTR(d), (d), \
    PTK_BUF_TYPE_OF_PTR(e), (e), \
    PTK_BUF_TYPE_OF_PTR(f), (f)

// 7 pointers
#define PTK_BUF_EXPAND_PTR_7(a, b, c, d, e, f, g) \
    PTK_BUF_TYPE_OF_PTR(a), (a), \
    PTK_BUF_TYPE_OF_PTR(b), (b), \
    PTK_BUF_TYPE_OF_PTR(c), (c), \
    PTK_BUF_TYPE_OF_PTR(d), (d), \
    PTK_BUF_TYPE_OF_PTR(e), (e), \
    PTK_BUF_TYPE_OF_PTR(f), (f), \
    PTK_BUF_TYPE_OF_PTR(g), (g)

// 8 pointers
#define PTK_BUF_EXPAND_PTR_8(a, b, c, d, e, f, g, h) \
    PTK_BUF_TYPE_OF_PTR(a), (a), \
    PTK_BUF_TYPE_OF_PTR(b), (b), \
    PTK_BUF_TYPE_OF_PTR(c), (c), \
    PTK_BUF_TYPE_OF_PTR(d), (d), \
    PTK_BUF_TYPE_OF_PTR(e), (e), \
    PTK_BUF_TYPE_OF_PTR(f), (f), \
    PTK_BUF_TYPE_OF_PTR(g), (g), \
    PTK_BUF_TYPE_OF_PTR(h), (h)

// Add more as needed...

/**
 * Select appropriate pointer expansion macro
 */
#define PTK_BUF_EXPAND_PTR_SELECT(N) PTK_BUF_EXPAND_PTR_##N
#define PTK_BUF_EXPAND_PTR_DISPATCH(N) PTK_BUF_EXPAND_PTR_SELECT(N)
#define PTK_BUF_EXPAND_PTR(...) PTK_BUF_EXPAND_PTR_DISPATCH(PTK_BUF_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)

//=============================================================================
// USER-FACING MACROS
//=============================================================================

/**
 * Serialize variables to buffer with automatic type detection and counting
 *
 * Usage: ptk_buf_serialize(buffer, PTK_BUF_LITTLE_ENDIAN, var1, var2, var3, ...)
 */
#define ptk_buf_serialize(buf, endian, ...) \
    ptk_buf_serialize_impl((buf), (endian), PTK_BUF_ARG_COUNT(__VA_ARGS__), PTK_BUF_EXPAND(__VA_ARGS__))

/**
 * Deserialize from buffer to variables with automatic type detection and counting
 *
 * Usage: ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &var1, &var2, &var3, ...)
 */
#define ptk_buf_deserialize(buf, peek, endian, ...) \
    ptk_buf_deserialize_impl((buf), (peek), (endian), PTK_BUF_ARG_COUNT(__VA_ARGS__), PTK_BUF_EXPAND_PTR(__VA_ARGS__))

//=============================================================================
// USAGE EXAMPLES
//=============================================================================

#if 0  // Example usage (commented out)

typedef struct {
    uint16_t command;          // EIP command type
    uint16_t length;           // Length of data following header
    uint32_t session_handle;   // Session identifier
    uint32_t status;           // Status/error code
    uint64_t sender_context;   // Client context data (8 bytes)
    uint32_t options;          // Command options
} eip_header_t;

void example_usage(ptk_buf *buffer) {
    eip_header_t header = {0x0065, 4, 0, 0, 0x123456789ABCDEF0, 0};

    // Explicit serialization with endianness specification
    ptk_err err = ptk_buf_serialize(buffer, PTK_BUF_LITTLE_ENDIAN,
        header.command, header.length, header.session_handle,
        header.status, header.sender_context, header.options);

    // Deserialization with explicit endianness and peek control
    eip_header_t received = {0};
    err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN,
        &received.command, &received.length, &received.session_handle,
        &received.status, &received.sender_context, &received.options);
}

#endif
