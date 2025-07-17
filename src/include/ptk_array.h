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
#include <ptk_mem.h>
#include <ptk_err.h>
#include <ptk_log.h>

/*
 Invariants that must be maintained:
    * - The array must always have a valid length (>= 0).
    * - The elements pointer must always point to a valid memory block.
    * - The element destructor must be called for each element before freeing the array.
*/

/**
 * @brief Declare a type-safe array type with custom name prefix and element type
 *
 * @param PREFIX Name prefix for the array type and functions
 * @param T Element type for the array
 *
 * Creates public definitions:
 * - PREFIX_array_t struct type
 * - PREFIX_array_create() function
 * - PREFIX_array_resize() function
 * - PREFIX_array_append() function
 * - PREFIX_array_get() function
 * - PREFIX_array_set() function
 * - PREFIX_array_copy() function
 */
#define PTK_ARRAY_DECLARE(PREFIX, T) \
    typedef struct PREFIX##_array { \
        T *elements; \
        void (*element_destructor)(T *element); \
        size_t len; \
    } PREFIX##_array_t; \
    \
    static void PREFIX##_PRIVATE_array_destructor(void *ptr) { \
        PREFIX##_array_t *arr = (PREFIX##_array_t *)ptr; \
        if (!arr) return; \
        if (arr->element_destructor && arr->elements) { \
            for (size_t i = 0; i < arr->len; i++) { \
                arr->element_destructor(&arr->elements[i]); \
            } \
        } \
        if (arr->elements) { \
            ptk_local_free(&arr->elements); \
        } \
    } \
    \
    static inline PREFIX##_array_t * PREFIX##_array_create(size_t initial_size, void (*element_destructor)(T *element)) { \
        if (initial_size == 0) return NULL; \
        \
        PREFIX##_array_t *arr = ptk_local_alloc(sizeof(PREFIX##_array_t), PREFIX##_PRIVATE_array_destructor); \
        if (!arr) return NULL; \
        \
        arr->len = initial_size; \
        arr->element_destructor = element_destructor; \
        arr->elements = ptk_local_alloc(initial_size * sizeof(T), NULL); \
        if (!arr->elements) { \
            ptk_local_free(&arr); \
            return NULL; \
        } \
        \
        memset(arr->elements, 0, initial_size * sizeof(T)); \
        \
        trace("Created " #PREFIX "_array with %zu initial elements", initial_size); \
        return arr; \
    } \
    \
    static inline ptk_err_t PREFIX##_array_resize(PREFIX##_array_t *arr, size_t new_len) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        if (new_len == 0) return PTK_ERR_INVALID_PARAM; \
        \
        T *new_elements = ptk_local_realloc(arr->elements, new_len * sizeof(T)); \
        if (!new_elements) { \
            error("Failed to allocate %zu bytes for " #PREFIX "_array", new_len * sizeof(T)); \
            return PTK_ERR_NO_RESOURCES; \
        } \
        \
        if(new_len > arr->len) { \
            memset(new_elements + arr->len, 0, (new_len - arr->len) * sizeof(T)); \
        } \
        \
        arr->elements = new_elements; \
        arr->len = new_len; \
        trace("Resized " #PREFIX "_array to %zu elements", new_len); \
        return PTK_OK; \
    } \
    \
    static inline ptk_err_t PREFIX##_array_append(PREFIX##_array_t *arr, T element) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        \
        ptk_err_t err = PREFIX##_array_resize(arr, arr->len + 1); \
        if (err != PTK_OK) return err; \
        \
        arr->elements[arr->len - 1] = element; \
        trace("Appended element to " #PREFIX "_array, new length: %zu", arr->len); \
        return PTK_OK; \
    } \
    \
    static inline T * PREFIX##_array_get(const PREFIX##_array_t *arr, size_t index) { \
        if (!arr) { ptk_set_err(PTK_ERR_NULL_PTR); return NULL; } \
        if (index >= arr->len) { \
            error("Index %zu out of bounds for " #PREFIX "_array (len=%zu)", index, arr->len); \
            ptk_set_err(PTK_ERR_OUT_OF_BOUNDS); \
            return NULL; \
        } \
        return &(arr->elements[index]); \
    } \
    \
    static inline ptk_err_t PREFIX##_array_set(PREFIX##_array_t *arr, size_t index, T element) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        if (index >= arr->len) { \
            error("Index %zu out of bounds for " #PREFIX "_array (len=%zu)", index, arr->len); \
            return PTK_ERR_OUT_OF_BOUNDS; \
        } \
        arr->elements[index] = element; \
        return PTK_OK; \
    } \
    \
    static inline PREFIX##_array_t *PREFIX##_array_copy(const PREFIX##_array_t *src) { \
        if (!src || src->len == 0) return NULL; \
        \
        PREFIX##_array_t *dst = PREFIX##_array_create(src->len, src->element_destructor); \
        if (!dst) return NULL; \
        \
        memcpy(dst->elements, src->elements, src->len * sizeof(T)); \
        debug("Copied " #PREFIX "_array: %zu elements", src->len); \
        return dst; \
    } \
    \
    static inline bool PREFIX##_array_is_valid(const PREFIX##_array_t *arr) { \
        if (!arr) return false; \
        return (arr->len > 0 && arr->elements != NULL); \
    } \
    \
    static inline PREFIX##_array_t *PREFIX##_array_from_raw(const T *raw_data, size_t count, void (*element_destructor)(T *element)) { \
        if (!raw_data || count == 0) return NULL; \
        \
        PREFIX##_array_t *arr = PREFIX##_array_create(count, element_destructor); \
        if (!arr) return NULL; \
        \
        memcpy(arr->elements, raw_data, count * sizeof(T)); \
        \
        return arr; \
    } \
    \
    \
    static inline size_t PREFIX##_array_len(const PREFIX##_array_t *arr) { \
        return (arr && arr->elements && arr->len > 0) ? arr->len : 0; \
    }

