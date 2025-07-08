#pragma once

/**
 * @file ptk_array.h
 * @brief Type-safe dynamic array API for protocol toolkit
 *
 * Provides macros for creating type-safe dynamic arrays with automatic
 * length tracking, bounds checking, and memory management.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "ptk_alloc.h"
#include "ptk_err.h"
#include "ptk_log.h"

/**
 * @brief Declare a type-safe array type with custom name prefix and element type
 *
 * @param PREFIX Name prefix for the array type and functions
 * @param T Element type for the array
 *
 * Creates:
 * - PREFIX_array_t struct type
 * - PREFIX_array_create() function
 * - PREFIX_array_dispose() function
 * - PREFIX_array_resize() function
 * - PREFIX_array_append() function
 * - PREFIX_array_get() function
 * - PREFIX_array_set() function
 * - PREFIX_array_clear() function
 * - PREFIX_array_copy() function
 */
#define PTK_ARRAY_DECLARE(PREFIX, T) \
    typedef struct PREFIX##_array { \
        size_t len; \
        ptk_allocator_t *allocator; \
        T *elements; \
    } PREFIX##_array_t; \
    \
    static inline ptk_err PREFIX##_array_create(ptk_allocator_t *allocator, PREFIX##_array_t *arr) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        arr->len = 0; \
        arr->allocator = allocator; \
        arr->elements = NULL; \
        return PTK_OK; \
    } \
    \
    static inline void PREFIX##_array_dispose(PREFIX##_array_t *arr) { \
        if (arr && arr->elements) { \
            debug("Disposing " #PREFIX "_array with %zu elements", arr->len); \
            ptk_free(arr->allocator, arr->elements); \
            arr->elements = NULL; \
            arr->len = 0; \
        } \
    } \
    \
    static inline ptk_err PREFIX##_array_resize(PREFIX##_array_t *arr, size_t new_len) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        \
        if (new_len == 0) { \
            PREFIX##_array_dispose(arr); \
            return PTK_OK; \
        } \
        \
        T *new_elements = arr->allocator->realloc(arr->allocator, arr->elements, new_len * sizeof(T)); \
        if (!new_elements) { \
            error("Failed to allocate %zu bytes for " #PREFIX "_array", new_len * sizeof(T)); \
            return PTK_ERR_NO_RESOURCES; \
        } \
        \
        /* Initialize new elements to zero */ \
        if (new_len > arr->len) { \
            memset(new_elements + arr->len, 0, (new_len - arr->len) * sizeof(T)); \
        } \
        \
        arr->elements = new_elements; \
        arr->len = new_len; \
        trace("Resized " #PREFIX "_array to %zu elements", new_len); \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_append(PREFIX##_array_t *arr, T element) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        \
        ptk_err err = PREFIX##_array_resize(arr, arr->len + 1); \
        if (err != PTK_OK) return err; \
        \
        arr->elements[arr->len - 1] = element; \
        trace("Appended element to " #PREFIX "_array, new length: %zu", arr->len); \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_get(const PREFIX##_array_t *arr, size_t index, T *element) { \
        if (!arr || !element) return PTK_ERR_NULL_PTR; \
        if (index >= arr->len) { \
            error("Index %zu out of bounds for " #PREFIX "_array (len=%zu)", index, arr->len); \
            return PTK_ERR_OUT_OF_BOUNDS; \
        } \
        *element = arr->elements[index]; \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_set(PREFIX##_array_t *arr, size_t index, T element) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        if (index >= arr->len) { \
            error("Index %zu out of bounds for " #PREFIX "_array (len=%zu)", index, arr->len); \
            return PTK_ERR_OUT_OF_BOUNDS; \
        } \
        arr->elements[index] = element; \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_copy(PREFIX##_array_t *dst, const PREFIX##_array_t *src) { \
        if (!dst || !src) return PTK_ERR_NULL_PTR; \
        \
        PREFIX##_array_dispose(dst); \
        \
        if (src->len == 0) { \
            return PREFIX##_array_create(src->allocator, dst); \
        } \
        \
        ptk_err err = PREFIX##_array_resize(dst, src->len); \
        if (err != PTK_OK) return err; \
        \
        memcpy(dst->elements, src->elements, src->len * sizeof(T)); \
        debug("Copied " #PREFIX "_array: %zu elements", src->len); \
        return PTK_OK; \
    } \
    \
    static inline bool PREFIX##_array_is_valid(const PREFIX##_array_t *arr) { \
        if (!arr) return false; \
        if (arr->len == 0) return arr->elements == NULL; \
        return arr->elements != NULL; \
    } \
    \
    static inline ptk_err PREFIX##_array_from_raw(ptk_allocator_t *allocator, PREFIX##_array_t *arr, const T *raw_data, size_t count) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        if (count > 0 && !raw_data) return PTK_ERR_NULL_PTR; \
        \
        PREFIX##_array_dispose(arr); \
        \
        if (count == 0) { \
            return PREFIX##_array_create(allocator, arr); \
        } \
        \
        ptk_err err = PREFIX##_array_resize(arr, count); \
        if (err != PTK_OK) return err; \
        \
        memcpy(arr->elements, raw_data, count * sizeof(T)); \
        trace("Created " #PREFIX "_array from raw data: %zu elements", count); \
        return PTK_OK; \
    } \
    \
    \
    static inline size_t PREFIX##_array_len(const PREFIX##_array_t *arr) { \
        return (arr && arr->elements) ? arr->len : 0; \
    }
