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
// CONSUME/PRODUCE data in buffers
//=============================================================================

/*

This is much like printf()/scanf or Python's pack/unpack.  There are a set of format
characters and modifiers.

The general format of a field is '%'<modifiers><type>

type  Meaning

b       An 8-bit value
w       A 16-bit value
d       A 32-bit value
q       A 64-bit value

Modifier    Meaning
*           An array of the value.  Producing this takes two arguments, a size_t
            number of elements and a pointer to a C array of those elements.
            Consuming takes two arguments: a size_t number of elements to consume,
            and a pointer to a C array in which to copy those elements.

Spaces are ignored.  All values are taken to be unsigned integers.  If you need to
produce or consume other types, cast them.

Special Format Characters

<   All fields after this, until overridden, are little endian in the encoded data.
>   All fields after this, until overridden, are big endian in the encoded data.

 */

extern ptk_err ptk_buf_produce(ptk_buf *dest, const char *fmt, ...);

extern ptk_err ptk_buf_consume(ptk_buf *src, bool peek, const char *fmt, ...);

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
