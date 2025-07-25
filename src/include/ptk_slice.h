#ifndef PTK_SLICE_H
#define PTK_SLICE_H

#include "ptk_types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Type-Safe Slice System
 * 
 * Slices provide bounds-checked array views without hidden allocations.
 * They consist of a pointer and length, offering safety and composability.
 */

/**
 * Slice declaration macro - creates typed slice with operations
 * 
 * Usage: PTK_DECLARE_SLICE_TYPE(mytype, MyStruct)
 * Creates: ptk_slice_mytype_t and associated functions
 */
#define PTK_DECLARE_SLICE_TYPE(slice_name, T) \
    typedef struct { \
        T* data; \
        size_t len; \
    } ptk_slice_##slice_name##_t; \
    \
    static inline ptk_slice_##slice_name##_t ptk_slice_##slice_name##_make(T* data, size_t len) { \
        return (ptk_slice_##slice_name##_t){.data = data, .len = len}; \
    } \
    \
    static inline ptk_slice_##slice_name##_t ptk_slice_##slice_name##_advance(ptk_slice_##slice_name##_t slice, size_t count) { \
        if (count >= slice.len) { \
            return (ptk_slice_##slice_name##_t){.data = slice.data + slice.len, .len = 0}; \
        } \
        return (ptk_slice_##slice_name##_t){.data = slice.data + count, .len = slice.len - count}; \
    } \
    \
    static inline bool ptk_slice_##slice_name##_is_empty(ptk_slice_##slice_name##_t slice) { \
        return slice.len == 0; \
    } \
    \
    static inline ptk_slice_##slice_name##_t ptk_slice_##slice_name##_truncate(ptk_slice_##slice_name##_t slice, size_t len) { \
        if (len >= slice.len) return slice; \
        return (ptk_slice_##slice_name##_t){.data = slice.data, .len = len}; \
    } \
    \
    static const ptk_type_info_t ptk_type_info_##slice_name = { \
        .size = sizeof(T), \
        .alignment = _Alignof(T) \
    }

/**
 * Built-in byte slice type with serialization support
 * This is the "special" slice type that gets serialization functions
 */
typedef struct {
    uint8_t* data;
    size_t len;
} ptk_slice_bytes_t;

/* Standard slice operations for byte slices */
static inline ptk_slice_bytes_t ptk_slice_bytes_make(uint8_t* data, size_t len) {
    return (ptk_slice_bytes_t){.data = data, .len = len};
}

static inline ptk_slice_bytes_t ptk_slice_bytes_advance(ptk_slice_bytes_t slice, size_t count) {
    if (count >= slice.len) {
        return (ptk_slice_bytes_t){.data = slice.data + slice.len, .len = 0};
    }
    return (ptk_slice_bytes_t){.data = slice.data + count, .len = slice.len - count};
}

static inline bool ptk_slice_bytes_is_empty(ptk_slice_bytes_t slice) {
    return slice.len == 0;
}

static inline ptk_slice_bytes_t ptk_slice_bytes_truncate(ptk_slice_bytes_t slice, size_t len) {
    if (len >= slice.len) return slice;
    return (ptk_slice_bytes_t){.data = slice.data, .len = len};
}

/* Type information for byte slices */
static const ptk_type_info_t ptk_type_info_bytes = {
    .size = sizeof(uint8_t),
    .alignment = _Alignof(uint8_t)
};

/**
 * Common slice types - automatically declared
 */
PTK_DECLARE_SLICE_TYPE(u16, uint16_t);       /* ptk_slice_u16_t */
PTK_DECLARE_SLICE_TYPE(u32, uint32_t);       /* ptk_slice_u32_t */
PTK_DECLARE_SLICE_TYPE(u64, uint64_t);       /* ptk_slice_u64_t */
PTK_DECLARE_SLICE_TYPE(chars, char);         /* ptk_slice_chars_t */
PTK_DECLARE_SLICE_TYPE(str_ptrs, char*);     /* ptk_slice_str_ptrs_t */

/**
 * Backward compatibility - byte slice is the default
 */
typedef ptk_slice_bytes_t ptk_slice_t;

/* Backward compatible functions */
static inline ptk_slice_t ptk_slice_make(uint8_t* data, size_t len) {
    return ptk_slice_bytes_make(data, len);
}

static inline bool ptk_slice_is_empty(ptk_slice_t slice) {
    return ptk_slice_bytes_is_empty(slice);
}

static inline ptk_slice_t ptk_slice_advance(ptk_slice_t slice, size_t count) {
    return ptk_slice_bytes_advance(slice, count);
}

static inline ptk_slice_t ptk_slice_truncate(ptk_slice_t slice, size_t len) {
    return ptk_slice_bytes_truncate(slice, len);
}

#ifdef __cplusplus
}
#endif

#endif /* PTK_SLICE_H */