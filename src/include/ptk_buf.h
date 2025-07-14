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

#include <ptk_err.h>
#include <ptk_array.h>
#include <ptk_mem.h>


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
// SERIALIZABLE INTERFACE
//=============================================================================

/**
 * @brief Interface for objects that can serialize/deserialize themselves
 */
struct ptk_serializable {
    ptk_err (*serialize)(ptk_buf *buf, struct ptk_serializable *obj);
    ptk_err (*deserialize)(ptk_buf *buf, struct ptk_serializable *obj);
};

typedef struct ptk_serializable ptk_serializable_t;

//=============================================================================
// BUFFER STRUCTURE
//=============================================================================

typedef uint16_t buf_size_t;

/* note the small size! */

typedef struct ptk_buf {
    u8 *data;           // Pointer to start of buffer
    buf_size_t data_len;  // Total length of buffer
    buf_size_t start;
    buf_size_t end;
} ptk_buf;


//=============================================================================
// BUFFER OPERATIONS
//=============================================================================


/**
 * @brief Allocate a new buffer of the given size.
 * 
 * size must be greater than zero.
 * 
 * @return a new buffer or NULL if there was an error.  The thread error will be set.
 */
extern ptk_buf *ptk_buf_alloc(buf_size_t size);

/**
 * @brief as for ptk_buf_alloc() but adds the passed data in.
 */
extern ptk_buf *ptk_buf_alloc_from_data(const u8 *data, buf_size_t size);

/**
 * @brief Resizes the buffer.   
 * 
 * The size may not be zero.  The passed in buffer may not be NULL.
 * This does not act like POSIX realloc()!
 */
extern ptk_buf *ptk_buf_realloc(ptk_buf *buf, buf_size_t new_size);

/**
 * @brief returns the length of data in the buffer (end - start).
 * 
 * Returns zero if the buffer was NULL. Returns the amount of data
 * between start and end (exclusive for end).
 */
extern buf_size_t ptk_buf_get_len(const ptk_buf *buf);

/**
 * @brief returns the total capacity of the buffer.
 * 
 * Returns zero if the buffer was NULL. Returns the total allocated
 * size of the data block.
 */
extern buf_size_t ptk_buf_get_capacity(const ptk_buf *buf);

extern buf_size_t ptk_buf_get_start(const ptk_buf *buf);
extern ptk_err ptk_buf_set_start(ptk_buf *buf, buf_size_t start);

extern buf_size_t ptk_buf_get_end(const ptk_buf *buf);
extern ptk_err ptk_buf_set_end(ptk_buf *buf, buf_size_t end);

/**
 * @brief Move the block of memory (start to end) to the new position in the buffer.
 * 
 * @return OK or OUT_OF_BOUNDS if the block would go out of bounds.
 */
extern ptk_err ptk_buf_move_block(ptk_buf *buf, buf_size_t new_position);

/* Use ptk_local_free() to free buffers allocated with ptk_buf_alloc() */

//=============================================================================
// SIMPLE BYTE ACCESS API
//=============================================================================

/**
 * @brief Set a single byte at the current end position and advance end
 * 
 * @param buf The buffer to write to
 * @param val The byte value to set
 * @return PTK_OK on success, error code on failure
 */
extern ptk_err ptk_buf_set_u8(ptk_buf *buf, u8 val);

/**
 * @brief Get a single byte from the current start position and advance start
 * 
 * @param buf The buffer to read from
 * @return The byte value, or 0 if error (check ptk_get_err())
 */
extern u8 ptk_buf_get_u8(ptk_buf *buf);

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
extern ptk_err ptk_buf_serialize_impl(ptk_buf *buf, ptk_buf_endian_t endian, buf_size_t count, ...);

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
extern ptk_err ptk_buf_deserialize_impl(ptk_buf *buf, bool peek, ptk_buf_endian_t endian, buf_size_t count, ...);

/* byte swap helpers */

/**
 * @brief swap the bytes in each 16-bit word
 *
 * @param value
 * @return u32
 */
static inline u32 ptk_buf_byte_swap_u32(u32 value) {
    return ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8) | ((value & 0x00FF0000) >> 8)
           | ((value & 0xFF000000) >> 24);
}

static inline u64 ptk_buf_byte_swap_u64(u64 value) {
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

//=============================================================================
// USAGE EXAMPLES
//=============================================================================

#if 0  // Example usage (commented out)

// Example PDU that implements ptk_serializable
typedef struct {
    ptk_serializable_t base;  // Must be first member for casting
    uint16_t command;          // EIP command type
    uint16_t length;           // Length of data following header
    uint32_t session_handle;   // Session identifier
    uint32_t status;           // Status/error code
    uint64_t sender_context;   // Client context data (8 bytes)
    uint32_t options;          // Command options
} eip_header_t;

// Implementation of serialize method
static ptk_err eip_header_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    eip_header_t *header = (eip_header_t*)obj;
    return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
        header->command, header->length, header->session_handle,
        header->status, header->sender_context, header->options);
}

// Implementation of deserialize method
static ptk_err eip_header_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    eip_header_t *header = (eip_header_t*)obj;
    return ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN,
        &header->command, &header->length, &header->session_handle,
        &header->status, &header->sender_context, &header->options);
}

// Initialize the PDU with its vtable
void eip_header_init(eip_header_t *header) {
    header->base.serialize = eip_header_serialize;
    header->base.deserialize = eip_header_deserialize;
    // Initialize other fields as needed
}

void example_usage(ptk_buf *buffer) {
    // Traditional field-by-field approach
    eip_header_t header = {0};
    eip_header_init(&header);

    // Direct PDU serialization - just pass the PDU to ptk_buf_serialize
    ptk_err err = ptk_buf_serialize(buffer, PTK_BUF_LITTLE_ENDIAN,
        (ptk_serializable_t*)&header);

    // Direct PDU deserialization - just pass the PDU to ptk_buf_deserialize
    eip_header_t received = {0};
    eip_header_init(&received);
    err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN,
        (ptk_serializable_t*)&received);

    // Mixed usage - combine PDUs with primitive types
    uint8_t preamble = 0xAA;
    uint16_t checksum = 0x1234;
    err = ptk_buf_serialize(buffer, PTK_BUF_LITTLE_ENDIAN,
        preamble, (ptk_serializable_t*)&header, checksum);
}

#endif
