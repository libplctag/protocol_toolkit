#ifndef MODBUS_ARRAY_H
#define MODBUS_ARRAY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ptk_alloc.h>
#include <ptk_err.h>
#include <ptk_log.h>

/**
 * @brief Declare a type-safe array type compatible with ptk_alloc parent-child system
 *
 * @param PREFIX Name prefix for the array type and functions
 * @param T Element type for the array
 *
 * Creates:
 * - PREFIX_array_t struct type
 * - PREFIX_array_create() function
 * - PREFIX_array_len() function
 * - PREFIX_array_resize() function
 * - PREFIX_array_append() function
 * - PREFIX_array_get() function
 * - PREFIX_array_set() function
 * - PREFIX_array_clear() function
 */
#define MODBUS_ARRAY_DECLARE(PREFIX, T) \
    typedef struct PREFIX##_array { \
        void *parent;     /* Parent for allocation hierarchy */ \
        size_t len;       /* Current number of elements */ \
        size_t capacity;  /* Allocated capacity */ \
        T *elements;      /* Array elements (child of parent) */ \
    } PREFIX##_array_t; \
    \
    static inline ptk_err PREFIX##_array_create(void *parent, PREFIX##_array_t *arr) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        arr->parent = parent; \
        arr->len = 0; \
        arr->capacity = 0; \
        arr->elements = NULL; \
        return PTK_OK; \
    } \
    \
    static inline size_t PREFIX##_array_len(const PREFIX##_array_t *arr) { \
        return arr ? arr->len : 0; \
    } \
    \
    static inline ptk_err PREFIX##_array_resize(PREFIX##_array_t *arr, size_t new_size) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        \
        if (new_size == 0) { \
            if (arr->elements) { \
                ptk_free(arr->elements); \
                arr->elements = NULL; \
            } \
            arr->len = 0; \
            arr->capacity = 0; \
            return PTK_OK; \
        } \
        \
        if (new_size > arr->capacity) { \
            size_t new_capacity = new_size * 2; /* Grow by 2x */ \
            T *new_elements = ptk_alloc(arr->parent, new_capacity * sizeof(T), NULL); \
            if (!new_elements) { \
                error("Failed to allocate memory for " #PREFIX "_array resize"); \
                return PTK_ERR_NO_RESOURCES; \
            } \
            \
            if (arr->elements && arr->len > 0) { \
                memcpy(new_elements, arr->elements, arr->len * sizeof(T)); \
                ptk_free(arr->elements); \
            } \
            \
            arr->elements = new_elements; \
            arr->capacity = new_capacity; \
        } \
        \
        arr->len = new_size; \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_append(PREFIX##_array_t *arr, T value) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        \
        ptk_err err = PREFIX##_array_resize(arr, arr->len + 1); \
        if (err != PTK_OK) return err; \
        \
        arr->elements[arr->len - 1] = value; \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_get(const PREFIX##_array_t *arr, size_t index, T *value) { \
        if (!arr || !value) return PTK_ERR_NULL_PTR; \
        if (index >= arr->len) return PTK_ERR_INVALID_PARAM; \
        \
        *value = arr->elements[index]; \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_set(PREFIX##_array_t *arr, size_t index, T value) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        if (index >= arr->len) return PTK_ERR_INVALID_PARAM; \
        \
        arr->elements[index] = value; \
        return PTK_OK; \
    } \
    \
    static inline ptk_err PREFIX##_array_clear(PREFIX##_array_t *arr) { \
        if (!arr) return PTK_ERR_NULL_PTR; \
        \
        if (arr->elements) { \
            ptk_free(arr->elements); \
            arr->elements = NULL; \
        } \
        arr->len = 0; \
        arr->capacity = 0; \
        return PTK_OK; \
    }

#endif // MODBUS_ARRAY_H
