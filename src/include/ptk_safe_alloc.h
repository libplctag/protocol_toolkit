#pragma once

/**
 * @file ptk_safe_alloc.h
 * @brief Type-safe memory allocation using static inline functions
 * 
 * This provides a clean, type-safe way to allocate memory that's as easy to use
 * as the PTK array system but uses static inline functions instead of macros
 * for better type safety, debugging, and error messages.
 */

#include <ptk_defs.h>
#include <ptk_mem.h>
#include <ptk_err.h>

//=============================================================================
// CORE TYPE-SAFE ALLOCATION FUNCTIONS
//=============================================================================

/**
 * @brief Type-safe local allocation template
 * 
 * This function template approach uses the sizeof trick to get type safety
 * while maintaining the simplicity of a function call.
 */
#define PTK_ALLOC_FUNC(type_name, type) \
    static inline type* type_name##_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        type* ptr = (type*)ptk_local_alloc(sizeof(type) * count, NULL); \
        if (!ptr) { \
            ptk_set_err(PTK_ERR_NO_RESOURCES); \
        } \
        return ptr; \
    } \
    \
    static inline type* type_name##_new(void) { \
        return type_name##_alloc(1); \
    } \
    \
    static inline void type_name##_free(type **ptr) { \
        if (ptr && *ptr) { \
            ptk_local_free((void**)ptr); \
        } \
    } \
    \
    static inline ptk_shared_handle_t type_name##_shared_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return PTK_SHARED_INVALID_HANDLE; \
        } \
        return ptk_shared_alloc(sizeof(type) * count, NULL); \
    } \
    \
    static inline ptk_shared_handle_t type_name##_shared_new(void) { \
        return type_name##_shared_alloc(1); \
    } \
    \
    static inline type* type_name##_shared_get(ptk_shared_handle_t handle) { \
        if (!PTK_SHARED_IS_VALID(handle)) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        type* ptr = NULL; \
        use_shared(handle, type*, ptr, PTK_TIME_NO_WAIT) { \
            return ptr; \
        } on_shared_fail { \
            ptk_set_err(PTK_ERR_INVALID_STATE); \
            return NULL; \
        } \
    } \
    \
    static inline void type_name##_shared_free(ptk_shared_handle_t *handle) { \
        if (handle && PTK_SHARED_IS_VALID(*handle)) { \
            ptk_shared_free(handle); \
        } \
    }

//=============================================================================
// PREDEFINED TYPE-SAFE ALLOCATORS
//=============================================================================

// Basic C types
PTK_ALLOC_FUNC(ptk_char, char)
PTK_ALLOC_FUNC(ptk_int, int)
PTK_ALLOC_FUNC(ptk_float, float)
PTK_ALLOC_FUNC(ptk_double, double)

// Protocol Toolkit types
PTK_ALLOC_FUNC(ptk_u8, ptk_u8_t)
PTK_ALLOC_FUNC(ptk_u16, ptk_u16_t)
PTK_ALLOC_FUNC(ptk_u32, ptk_u32_t)
PTK_ALLOC_FUNC(ptk_u64, ptk_u64_t)
PTK_ALLOC_FUNC(ptk_i8, ptk_i8_t)
PTK_ALLOC_FUNC(ptk_i16, ptk_i16_t)
PTK_ALLOC_FUNC(ptk_i32, ptk_i32_t)
PTK_ALLOC_FUNC(ptk_i64, ptk_i64_t)

//=============================================================================
// ENHANCED ALLOCATORS WITH DESTRUCTORS
//=============================================================================

/**
 * @brief Type-safe allocation with destructor support
 */
#define PTK_ALLOC_FUNC_WITH_DESTRUCTOR(type_name, type, destructor_func) \
    static inline type* type_name##_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        type* ptr = (type*)ptk_local_alloc(sizeof(type) * count, destructor_func); \
        if (!ptr) { \
            ptk_set_err(PTK_ERR_NO_RESOURCES); \
        } \
        return ptr; \
    } \
    \
    static inline type* type_name##_new(void) { \
        return type_name##_alloc(1); \
    } \
    \
    static inline void type_name##_free(type **ptr) { \
        if (ptr && *ptr) { \
            ptk_local_free((void**)ptr); \
        } \
    } \
    \
    static inline ptk_shared_handle_t type_name##_shared_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return PTK_SHARED_INVALID_HANDLE; \
        } \
        return ptk_shared_alloc(sizeof(type) * count, destructor_func); \
    } \
    \
    static inline ptk_shared_handle_t type_name##_shared_new(void) { \
        return type_name##_shared_alloc(1); \
    } \
    \
    static inline type* type_name##_shared_get(ptk_shared_handle_t handle) { \
        if (!PTK_SHARED_IS_VALID(handle)) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        type* ptr = NULL; \
        use_shared(handle, type*, ptr, PTK_TIME_NO_WAIT) { \
            return ptr; \
        } on_shared_fail { \
            ptk_set_err(PTK_ERR_INVALID_STATE); \
            return NULL; \
        } \
    } \
    \
    static inline void type_name##_shared_free(ptk_shared_handle_t *handle) { \
        if (handle && PTK_SHARED_IS_VALID(*handle)) { \
            ptk_shared_free(handle); \
        } \
    }

//=============================================================================
// CONVENIENCE WRAPPER FUNCTIONS
//=============================================================================

/**
 * @brief String allocation convenience functions
 * These are just aliases to ptk_char_* functions with better names
 */
static inline char* ptk_string_alloc(size_t length) {
    return ptk_char_alloc(length);
}

static inline char* ptk_string_new(size_t length) {
    return ptk_char_alloc(length);
}

static inline void ptk_string_free(char **str) {
    ptk_char_free(str);
}

static inline ptk_shared_handle_t ptk_string_shared_alloc(size_t length) {
    return ptk_char_shared_alloc(length);
}

static inline char* ptk_string_shared_get(ptk_shared_handle_t handle) {
    return ptk_char_shared_get(handle);
}

static inline void ptk_string_shared_free(ptk_shared_handle_t *handle) {
    ptk_char_shared_free(handle);
}

//=============================================================================
// BUFFER-SPECIFIC ALLOCATORS
//=============================================================================

/**
 * @brief Allocate a buffer of bytes with zero initialization
 */
static inline ptk_u8_t* ptk_buffer_alloc(size_t size) {
    ptk_u8_t *buffer = ptk_u8_alloc(size);
    if (buffer) {
        // Zero-initialize the buffer
        for (size_t i = 0; i < size; i++) {
            buffer[i] = 0;
        }
    }
    return buffer;
}

static inline void ptk_buffer_free(ptk_u8_t **buffer) {
    ptk_u8_free(buffer);
}

//=============================================================================
// GENERIC ALLOCATION HELPERS
//=============================================================================

/**
 * @brief Generic allocation function that can be used with sizeof
 * 
 * This provides a type-safe way to allocate arbitrary types:
 * my_type_t *ptr = ptk_alloc_generic(sizeof(my_type_t), count);
 */
static inline void* ptk_alloc_generic(size_t element_size, size_t count) {
    if (element_size == 0 || count == 0) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    void *ptr = ptk_local_alloc(element_size * count, NULL);
    if (!ptr) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
    }
    return ptr;
}

/**
 * @brief Type-safe allocation using sizeof trick
 * 
 * Usage: my_type_t *ptr = ptk_alloc_sizeof(*ptr, count);
 */
#define ptk_alloc_sizeof(example_var, count) \
    ((typeof(example_var)*)ptk_alloc_generic(sizeof(example_var), count))

/**
 * @brief Simple type-safe allocation macro
 * 
 * Usage: my_type_t *ptr = ptk_alloc_type(my_type_t, count);
 */
#define ptk_alloc_type(type, count) \
    ((type*)ptk_alloc_generic(sizeof(type), count))

//=============================================================================
// USAGE EXAMPLES
//=============================================================================

/*
Example usage:

// 1. Use predefined allocators (all static inline functions):
char *string = ptk_string_alloc(256);
ptk_u32_t *numbers = ptk_u32_alloc(100);
int *single_int = ptk_int_new();

// 2. Shared memory versions:
ptk_shared_handle_t shared_numbers = ptk_u32_shared_alloc(50);
ptk_u32_t *shared_ptr = ptk_u32_shared_get(shared_numbers);

// 3. Custom types using PTK_ALLOC_FUNC:
typedef struct { int x, y; } point_t;
PTK_ALLOC_FUNC(point, point_t)

point_t *points = point_alloc(10);
point_t *single_point = point_new();

// 4. Generic allocation for any type:
my_custom_type_t *data = ptk_alloc_type(my_custom_type_t, 5);

// 5. Cleanup (all static inline functions):
ptk_string_free(&string);
ptk_u32_free(&numbers);
ptk_int_free(&single_int);
point_free(&points);
ptk_u32_shared_free(&shared_numbers);

// All functions provide:
// - Compile-time type safety
// - Proper error handling via ptk_set_err()
// - Automatic null checking
// - Both local and shared memory variants
// - Clean, debuggable static inline implementation
*/