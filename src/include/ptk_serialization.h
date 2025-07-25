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



//=============================================================================
// TYPE-SAFE SERIALIZATION API
//=============================================================================

// Forward declarations for type-safe serialization
typedef enum {
    PTK_BUF_TYPE_U8 = 1,        // uint8_t
    PTK_BUF_TYPE_U16,           // uint16_t
    PTK_BUF_TYPE_U32,           // uint32_t
    PTK_BUF_TYPE_U64,           // uint64_t
    PTK_BUF_TYPE_S8,            // int8_t
    PTK_BUF_TYPE_S16,           // int16_t
    PTK_BUF_TYPE_S32,           // int32_t
    PTK_BUF_TYPE_S64,           // int64_t
    PTK_BUF_TYPE_FLOAT,         // float
    PTK_BUF_TYPE_DOUBLE,        // double
    PTK_BUF_TYPE_SERIALIZABLE,  // struct ptk_serializable*
} ptk_buf_type_t;

typedef enum { PTK_BUF_LITTLE_ENDIAN = 0, PTK_BUF_BIG_ENDIAN = 1 } ptk_buf_endian_t;

/**
 * Implementation function for type-safe serialization
 *
 * Moves the cursor forward by the size of the serialized data.
 *
 * @param buf Buffer to write to
 * @param endian Endianness for multi-byte values
 * @param count Number of fields to serialize
 * @param ... Alternating type_enum, value pairs
 * @return Error code
 */
PTK_API extern ptk_err_t ptk_buf_serialize_impl(ptk_buf *buf, ptk_buf_endian_t endian, ptk_buf_size_t count, ...);

/**
 * Implementation function for type-safe deserialization
 *
 * Moves the cursor forward by the size of the deserialized data.
 *
 * Leaves the cursor unchanged if peek is true.
 *
 * Leaves the cursor unchanged if there is an error.
 *
 * @param buf Buffer to read from
 * @param peek Whether to peek (don't advance buffer position)
 * @param endian Endianness for multi-byte values
 * @param count Number of fields to deserialize
 * @param ... Alternating type_enum, pointer pairs
 * @return Error code
 */
PTK_API extern ptk_err_t ptk_buf_deserialize_impl(ptk_buf *buf, bool peek, ptk_buf_endian_t endian, ptk_buf_size_t count, ...);

/* byte swap helpers */

/**
 * @brief swap the bytes in each 16-bit word
 *
 * @param value
 * @return ptk_u32_t
 */
static inline ptk_u32_t ptk_buf_byte_swap_u32(ptk_u32_t value) {
    return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8)
           | ((value & 0xFF000000) >> 24);
}

static inline ptk_u64_t ptk_buf_byte_swap_u64(ptk_u64_t value) {
    return ((value & 0x00000000000000FFULL) << 56) | ((value & 0x000000000000FF00ULL) << 40)
           | ((value & 0x0000000000FF0000ULL) << 24) | ((value & 0x00000000FF000000ULL) << 8)
           | ((value & 0x000000FF00000000ULL) >> 8) | ((value & 0x0000FF0000000000ULL) >> 24)
           | ((value & 0x00FF000000000000ULL) >> 40) | ((value & 0xFF00000000000000ULL) >> 56);
}

//=============================================================================
// TYPE-SAFE SERIALIZATION MACROS
//=============================================================================

/**
 * Map C types to type enumeration using _Generic
 */
#define PTK_BUF_TYPE_OF(x)                               \
    _Generic((x),                                        \
        uint8_t: PTK_BUF_TYPE_U8,                        \
        uint16_t: PTK_BUF_TYPE_U16,                      \
        uint32_t: PTK_BUF_TYPE_U32,                      \
        uint64_t: PTK_BUF_TYPE_U64,                      \
        int8_t: PTK_BUF_TYPE_S8,                         \
        int16_t: PTK_BUF_TYPE_S16,                       \
        int32_t: PTK_BUF_TYPE_S32,                       \
        int64_t: PTK_BUF_TYPE_S64,                       \
        float: PTK_BUF_TYPE_FLOAT,                       \
        double: PTK_BUF_TYPE_DOUBLE,                     \
        ptk_serializable_t *: PTK_BUF_TYPE_SERIALIZABLE, \
        ptk_serializable_t: PTK_BUF_TYPE_SERIALIZABLE,   \
        default: 0)

/**
 * For pointers, handle serializable objects specially, dereference others
 */
#define PTK_BUF_TYPE_OF_PTR(ptr) \
    _Generic((ptr), ptk_serializable_t * *: PTK_BUF_TYPE_SERIALIZABLE, default: PTK_BUF_TYPE_OF(*(ptr)))

//=============================================================================
// ARGUMENT COUNTING MACROS
//=============================================================================

/**
 * Count arguments (up to 64 arguments, so 32 fields max)
 */
#define PTK_BUF_ARG_COUNT(...)                                                                                                  \
    PTK_BUF_ARG_COUNT_IMPL(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, \
                           42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18,  \
                           17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define PTK_BUF_ARG_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, \
                               _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40,  \
                               _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59,  \
                               _60, _61, _62, _63, _64, N, ...)                                                                \
    N

//=============================================================================
// TYPE ENUMERATION EXPANSION MACROS
//=============================================================================

/**
 * Expand arguments to type, value pairs
 * For each argument, generate: PTK_BUF_TYPE_OF(arg), arg
 */

// 1 argument
#define PTK_BUF_EXPAND_1(a) PTK_BUF_TYPE_OF(a), (a)

// 2 arguments
#define PTK_BUF_EXPAND_2(a, b) PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b)

// 3 arguments
#define PTK_BUF_EXPAND_3(a, b, c) PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b), PTK_BUF_TYPE_OF(c), (c)

// 4 arguments
#define PTK_BUF_EXPAND_4(a, b, c, d) \
    PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b), PTK_BUF_TYPE_OF(c), (c), PTK_BUF_TYPE_OF(d), (d)

// 5 arguments
#define PTK_BUF_EXPAND_5(a, b, c, d, e) \
    PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b), PTK_BUF_TYPE_OF(c), (c), PTK_BUF_TYPE_OF(d), (d), PTK_BUF_TYPE_OF(e), (e)

// 6 arguments (EIP header size)
#define PTK_BUF_EXPAND_6(a, b, c, d, e, f)                                                                                       \
    PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b), PTK_BUF_TYPE_OF(c), (c), PTK_BUF_TYPE_OF(d), (d), PTK_BUF_TYPE_OF(e), (e), \
        PTK_BUF_TYPE_OF(f), (f)

// 7 arguments
#define PTK_BUF_EXPAND_7(a, b, c, d, e, f, g)                                                                                    \
    PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b), PTK_BUF_TYPE_OF(c), (c), PTK_BUF_TYPE_OF(d), (d), PTK_BUF_TYPE_OF(e), (e), \
        PTK_BUF_TYPE_OF(f), (f), PTK_BUF_TYPE_OF(g), (g)

// 8 arguments
#define PTK_BUF_EXPAND_8(a, b, c, d, e, f, g, h)                                                                                 \
    PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b), PTK_BUF_TYPE_OF(c), (c), PTK_BUF_TYPE_OF(d), (d), PTK_BUF_TYPE_OF(e), (e), \
        PTK_BUF_TYPE_OF(f), (f), PTK_BUF_TYPE_OF(g), (g), PTK_BUF_TYPE_OF(h), (h)

// 10 arguments
#define PTK_BUF_EXPAND_10(a, b, c, d, e, f, g, h, i, j)                                                                          \
    PTK_BUF_TYPE_OF(a), (a), PTK_BUF_TYPE_OF(b), (b), PTK_BUF_TYPE_OF(c), (c), PTK_BUF_TYPE_OF(d), (d), PTK_BUF_TYPE_OF(e), (e), \
        PTK_BUF_TYPE_OF(f), (f), PTK_BUF_TYPE_OF(g), (g), PTK_BUF_TYPE_OF(h), (h), PTK_BUF_TYPE_OF(i), (i), PTK_BUF_TYPE_OF(j),  \
        (j)

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
#define PTK_BUF_EXPAND_PTR_1(a) PTK_BUF_TYPE_OF_PTR(a), (a)

// 2 pointers
#define PTK_BUF_EXPAND_PTR_2(a, b) PTK_BUF_TYPE_OF_PTR(a), (a), PTK_BUF_TYPE_OF_PTR(b), (b)

// 3 pointers
#define PTK_BUF_EXPAND_PTR_3(a, b, c) PTK_BUF_TYPE_OF_PTR(a), (a), PTK_BUF_TYPE_OF_PTR(b), (b), PTK_BUF_TYPE_OF_PTR(c), (c)

// 4 pointers
#define PTK_BUF_EXPAND_PTR_4(a, b, c, d) \
    PTK_BUF_TYPE_OF_PTR(a), (a), PTK_BUF_TYPE_OF_PTR(b), (b), PTK_BUF_TYPE_OF_PTR(c), (c), PTK_BUF_TYPE_OF_PTR(d), (d)

// 5 pointers
#define PTK_BUF_EXPAND_PTR_5(a, b, c, d, e)                                                                             \
    PTK_BUF_TYPE_OF_PTR(a), (a), PTK_BUF_TYPE_OF_PTR(b), (b), PTK_BUF_TYPE_OF_PTR(c), (c), PTK_BUF_TYPE_OF_PTR(d), (d), \
        PTK_BUF_TYPE_OF_PTR(e), (e)

// 6 pointers (EIP header size)
#define PTK_BUF_EXPAND_PTR_6(a, b, c, d, e, f)                                                                          \
    PTK_BUF_TYPE_OF_PTR(a), (a), PTK_BUF_TYPE_OF_PTR(b), (b), PTK_BUF_TYPE_OF_PTR(c), (c), PTK_BUF_TYPE_OF_PTR(d), (d), \
        PTK_BUF_TYPE_OF_PTR(e), (e), PTK_BUF_TYPE_OF_PTR(f), (f)

// 7 pointers
#define PTK_BUF_EXPAND_PTR_7(a, b, c, d, e, f, g)                                                                       \
    PTK_BUF_TYPE_OF_PTR(a), (a), PTK_BUF_TYPE_OF_PTR(b), (b), PTK_BUF_TYPE_OF_PTR(c), (c), PTK_BUF_TYPE_OF_PTR(d), (d), \
        PTK_BUF_TYPE_OF_PTR(e), (e), PTK_BUF_TYPE_OF_PTR(f), (f), PTK_BUF_TYPE_OF_PTR(g), (g)

// 8 pointers
#define PTK_BUF_EXPAND_PTR_8(a, b, c, d, e, f, g, h)                                                                    \
    PTK_BUF_TYPE_OF_PTR(a), (a), PTK_BUF_TYPE_OF_PTR(b), (b), PTK_BUF_TYPE_OF_PTR(c), (c), PTK_BUF_TYPE_OF_PTR(d), (d), \
        PTK_BUF_TYPE_OF_PTR(e), (e), PTK_BUF_TYPE_OF_PTR(f), (f), PTK_BUF_TYPE_OF_PTR(g), (g), PTK_BUF_TYPE_OF_PTR(h), (h)

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


#ifdef __cplusplus
}
#endif

#endif /* PTK_SERIALIZATION_H */