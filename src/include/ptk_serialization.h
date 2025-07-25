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
 * NOTE: Individual read/write functions have been removed in favor of
 * the new type-safe multi-field serialization API below.
 * 
 * Use ptk_serialize() and ptk_deserialize() macros instead.
 */

/**
 * Utility functions for endianness conversion
 */
uint16_t ptk_bswap16(uint16_t value);
uint32_t ptk_bswap32(uint32_t value);
uint64_t ptk_bswap64(uint64_t value);

bool ptk_is_little_endian(void);
bool ptk_is_big_endian(void);

//=============================================================================
// SERIALIZABLE INTERFACE
//=============================================================================

/**
 * @brief Interface for objects that can serialize/deserialize themselves
 */
struct ptk_serializable {
    ptk_status_t (*serialize)(ptk_slice_bytes_t *slice, struct ptk_serializable *obj);
    ptk_status_t (*deserialize)(ptk_slice_bytes_t *slice, struct ptk_serializable *obj);
};

typedef struct ptk_serializable ptk_serializable_t;


//=============================================================================
// TYPE-SAFE SERIALIZATION API
//=============================================================================

// Forward declarations for type-safe serialization
typedef enum {
    PTK_SERIALIZE_TYPE_U8 = 1,        // uint8_t
    PTK_SERIALIZE_TYPE_U16,           // uint16_t
    PTK_SERIALIZE_TYPE_U32,           // uint32_t
    PTK_SERIALIZE_TYPE_U64,           // uint64_t
    PTK_SERIALIZE_TYPE_S8,            // int8_t
    PTK_SERIALIZE_TYPE_S16,           // int16_t
    PTK_SERIALIZE_TYPE_S32,           // int32_t
    PTK_SERIALIZE_TYPE_S64,           // int64_t
    PTK_SERIALIZE_TYPE_FLOAT,         // float
    PTK_SERIALIZE_TYPE_DOUBLE,        // double
    PTK_SERIALIZE_TYPE_SERIALIZABLE,  // struct ptk_serializable*
} ptk_serialize_type_t;

// Use existing ptk_endian_t from ptk_types.h

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
ptk_status_t ptk_serialize_impl(ptk_slice_bytes_t *slice, ptk_endian_t endian, size_t count, ...);

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
ptk_status_t ptk_deserialize_impl(ptk_slice_bytes_t *slice, bool peek, ptk_endian_t endian, size_t count, ...);

/* byte swap helpers */

/**
 * @brief swap the bytes in each 16-bit word
 *
 * @param value
 * @return ptk_u32_t
 */
static inline uint32_t ptk_serialize_bswap32(uint32_t value) {
    return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8)
           | ((value & 0xFF000000) >> 24);
}

static inline uint64_t ptk_serialize_bswap64(uint64_t value) {
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
#define PTK_SERIALIZE_TYPE_OF(x)                               \
    _Generic((x),                                        \
        uint8_t: PTK_SERIALIZE_TYPE_U8,                        \
        uint16_t: PTK_SERIALIZE_TYPE_U16,                      \
        uint32_t: PTK_SERIALIZE_TYPE_U32,                      \
        uint64_t: PTK_SERIALIZE_TYPE_U64,                      \
        int8_t: PTK_SERIALIZE_TYPE_S8,                         \
        int16_t: PTK_SERIALIZE_TYPE_S16,                       \
        int32_t: PTK_SERIALIZE_TYPE_S32,                       \
        int64_t: PTK_SERIALIZE_TYPE_S64,                       \
        float: PTK_SERIALIZE_TYPE_FLOAT,                       \
        double: PTK_SERIALIZE_TYPE_DOUBLE,                     \
        ptk_serializable_t *: PTK_SERIALIZE_TYPE_SERIALIZABLE, \
        ptk_serializable_t: PTK_SERIALIZE_TYPE_SERIALIZABLE,   \
        default: 0)

/**
 * For pointers, handle serializable objects specially, dereference others
 */
#define PTK_SERIALIZE_TYPE_OF_PTR(ptr) \
    _Generic((ptr), ptk_serializable_t * *: PTK_SERIALIZE_TYPE_SERIALIZABLE, default: PTK_SERIALIZE_TYPE_OF(*(ptr)))

//=============================================================================
// ARGUMENT COUNTING MACROS
//=============================================================================

/**
 * Count arguments (up to 64 arguments, so 32 fields max)
 */
#define PTK_SERIALIZE_ARG_COUNT(...)                                                                                                  \
    PTK_SERIALIZE_ARG_COUNT_IMPL(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, \
                           42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18,  \
                           17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define PTK_SERIALIZE_ARG_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, \
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
#define PTK_SERIALIZE_EXPAND_1(a) PTK_SERIALIZE_TYPE_OF(a), (a)

// 2 arguments
#define PTK_SERIALIZE_EXPAND_2(a, b) PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b)

// 3 arguments
#define PTK_SERIALIZE_EXPAND_3(a, b, c) PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b), PTK_SERIALIZE_TYPE_OF(c), (c)

// 4 arguments
#define PTK_SERIALIZE_EXPAND_4(a, b, c, d) \
    PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b), PTK_SERIALIZE_TYPE_OF(c), (c), PTK_SERIALIZE_TYPE_OF(d), (d)

// 5 arguments
#define PTK_SERIALIZE_EXPAND_5(a, b, c, d, e) \
    PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b), PTK_SERIALIZE_TYPE_OF(c), (c), PTK_SERIALIZE_TYPE_OF(d), (d), PTK_SERIALIZE_TYPE_OF(e), (e)

// 6 arguments (EIP header size)
#define PTK_SERIALIZE_EXPAND_6(a, b, c, d, e, f)                                                                                       \
    PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b), PTK_SERIALIZE_TYPE_OF(c), (c), PTK_SERIALIZE_TYPE_OF(d), (d), PTK_SERIALIZE_TYPE_OF(e), (e), \
        PTK_SERIALIZE_TYPE_OF(f), (f)

// 7 arguments
#define PTK_SERIALIZE_EXPAND_7(a, b, c, d, e, f, g)                                                                                    \
    PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b), PTK_SERIALIZE_TYPE_OF(c), (c), PTK_SERIALIZE_TYPE_OF(d), (d), PTK_SERIALIZE_TYPE_OF(e), (e), \
        PTK_SERIALIZE_TYPE_OF(f), (f), PTK_SERIALIZE_TYPE_OF(g), (g)

// 8 arguments
#define PTK_SERIALIZE_EXPAND_8(a, b, c, d, e, f, g, h)                                                                                 \
    PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b), PTK_SERIALIZE_TYPE_OF(c), (c), PTK_SERIALIZE_TYPE_OF(d), (d), PTK_SERIALIZE_TYPE_OF(e), (e), \
        PTK_SERIALIZE_TYPE_OF(f), (f), PTK_SERIALIZE_TYPE_OF(g), (g), PTK_SERIALIZE_TYPE_OF(h), (h)

// 10 arguments
#define PTK_SERIALIZE_EXPAND_10(a, b, c, d, e, f, g, h, i, j)                                                                          \
    PTK_SERIALIZE_TYPE_OF(a), (a), PTK_SERIALIZE_TYPE_OF(b), (b), PTK_SERIALIZE_TYPE_OF(c), (c), PTK_SERIALIZE_TYPE_OF(d), (d), PTK_SERIALIZE_TYPE_OF(e), (e), \
        PTK_SERIALIZE_TYPE_OF(f), (f), PTK_SERIALIZE_TYPE_OF(g), (g), PTK_SERIALIZE_TYPE_OF(h), (h), PTK_SERIALIZE_TYPE_OF(i), (i), PTK_SERIALIZE_TYPE_OF(j),  \
        (j)

// Add more as needed up to 32 arguments...

/**
 * Select appropriate expansion macro based on argument count
 */
#define PTK_SERIALIZE_EXPAND_SELECT(N) PTK_SERIALIZE_EXPAND_##N
#define PTK_SERIALIZE_EXPAND_DISPATCH(N) PTK_SERIALIZE_EXPAND_SELECT(N)
#define PTK_SERIALIZE_EXPAND(...) PTK_SERIALIZE_EXPAND_DISPATCH(PTK_SERIALIZE_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)

//=============================================================================
// POINTER TYPE ENUMERATION FOR DESERIALIZATION
//=============================================================================

/**
 * Expand pointer arguments to type, pointer pairs
 * For each pointer argument, generate: PTK_BUF_TYPE_OF_PTR(ptr), ptr
 */

// 1 pointer
#define PTK_SERIALIZE_EXPAND_PTR_1(a) PTK_SERIALIZE_TYPE_OF_PTR(a), (a)

// 2 pointers
#define PTK_SERIALIZE_EXPAND_PTR_2(a, b) PTK_SERIALIZE_TYPE_OF_PTR(a), (a), PTK_SERIALIZE_TYPE_OF_PTR(b), (b)

// 3 pointers
#define PTK_SERIALIZE_EXPAND_PTR_3(a, b, c) PTK_SERIALIZE_TYPE_OF_PTR(a), (a), PTK_SERIALIZE_TYPE_OF_PTR(b), (b), PTK_SERIALIZE_TYPE_OF_PTR(c), (c)

// 4 pointers
#define PTK_SERIALIZE_EXPAND_PTR_4(a, b, c, d) \
    PTK_SERIALIZE_TYPE_OF_PTR(a), (a), PTK_SERIALIZE_TYPE_OF_PTR(b), (b), PTK_SERIALIZE_TYPE_OF_PTR(c), (c), PTK_SERIALIZE_TYPE_OF_PTR(d), (d)

// 5 pointers
#define PTK_SERIALIZE_EXPAND_PTR_5(a, b, c, d, e)                                                                             \
    PTK_SERIALIZE_TYPE_OF_PTR(a), (a), PTK_SERIALIZE_TYPE_OF_PTR(b), (b), PTK_SERIALIZE_TYPE_OF_PTR(c), (c), PTK_SERIALIZE_TYPE_OF_PTR(d), (d), \
        PTK_SERIALIZE_TYPE_OF_PTR(e), (e)

// 6 pointers (EIP header size)
#define PTK_SERIALIZE_EXPAND_PTR_6(a, b, c, d, e, f)                                                                          \
    PTK_SERIALIZE_TYPE_OF_PTR(a), (a), PTK_SERIALIZE_TYPE_OF_PTR(b), (b), PTK_SERIALIZE_TYPE_OF_PTR(c), (c), PTK_SERIALIZE_TYPE_OF_PTR(d), (d), \
        PTK_SERIALIZE_TYPE_OF_PTR(e), (e), PTK_SERIALIZE_TYPE_OF_PTR(f), (f)

// 7 pointers
#define PTK_SERIALIZE_EXPAND_PTR_7(a, b, c, d, e, f, g)                                                                       \
    PTK_SERIALIZE_TYPE_OF_PTR(a), (a), PTK_SERIALIZE_TYPE_OF_PTR(b), (b), PTK_SERIALIZE_TYPE_OF_PTR(c), (c), PTK_SERIALIZE_TYPE_OF_PTR(d), (d), \
        PTK_SERIALIZE_TYPE_OF_PTR(e), (e), PTK_SERIALIZE_TYPE_OF_PTR(f), (f), PTK_SERIALIZE_TYPE_OF_PTR(g), (g)

// 8 pointers
#define PTK_SERIALIZE_EXPAND_PTR_8(a, b, c, d, e, f, g, h)                                                                    \
    PTK_SERIALIZE_TYPE_OF_PTR(a), (a), PTK_SERIALIZE_TYPE_OF_PTR(b), (b), PTK_SERIALIZE_TYPE_OF_PTR(c), (c), PTK_SERIALIZE_TYPE_OF_PTR(d), (d), \
        PTK_SERIALIZE_TYPE_OF_PTR(e), (e), PTK_SERIALIZE_TYPE_OF_PTR(f), (f), PTK_SERIALIZE_TYPE_OF_PTR(g), (g), PTK_SERIALIZE_TYPE_OF_PTR(h), (h)

// Add more as needed...

/**
 * Select appropriate pointer expansion macro
 */
#define PTK_SERIALIZE_EXPAND_PTR_SELECT(N) PTK_SERIALIZE_EXPAND_PTR_##N
#define PTK_SERIALIZE_EXPAND_PTR_DISPATCH(N) PTK_SERIALIZE_EXPAND_PTR_SELECT(N)
#define PTK_SERIALIZE_EXPAND_PTR(...) PTK_SERIALIZE_EXPAND_PTR_DISPATCH(PTK_SERIALIZE_ARG_COUNT(__VA_ARGS__))(__VA_ARGS__)

//=============================================================================
// USER-FACING MACROS
//=============================================================================

/**
 * Serialize variables to slice with automatic type detection and counting
 *
 * Usage: ptk_serialize(slice, PTK_ENDIAN_LITTLE, var1, var2, var3, ...)
 */
#define ptk_serialize(slice, endian, ...) \
    ptk_serialize_impl((slice), (endian), PTK_SERIALIZE_ARG_COUNT(__VA_ARGS__), PTK_SERIALIZE_EXPAND(__VA_ARGS__))

/**
 * Deserialize from slice to variables with automatic type detection and counting
 *
 * Usage: ptk_deserialize(slice, false, PTK_ENDIAN_LITTLE, &var1, &var2, &var3, ...)
 */
#define ptk_deserialize(slice, peek, endian, ...) \
    ptk_deserialize_impl((slice), (peek), (endian), PTK_SERIALIZE_ARG_COUNT(__VA_ARGS__), PTK_SERIALIZE_EXPAND_PTR(__VA_ARGS__))


#ifdef __cplusplus
}
#endif

#endif /* PTK_SERIALIZATION_H */