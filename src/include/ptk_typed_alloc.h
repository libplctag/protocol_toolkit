#pragma once

/**
 * @file ptk_typed_alloc.h
 * @brief Type-safe allocation system for Protocol Toolkit
 * 
 * This header provides type-safe allocation functions that return typed handles
 * instead of void pointers, preventing common type confusion bugs at compile time.
 */

#include <ptk_defs.h>

//=============================================================================
// TYPE REGISTRY SYSTEM
//=============================================================================

/**
 * @brief Type identifiers for type-safe allocation
 */
typedef enum {
    PTK_TYPE_INVALID = 0,
    PTK_TYPE_BUFFER,
    PTK_TYPE_SOCKET,
    PTK_TYPE_STRING,
    PTK_TYPE_THREAD_ARG,
    PTK_TYPE_CONFIG,
    PTK_TYPE_NETWORK_INTERFACE,
    PTK_TYPE_ADDRESS,
    PTK_TYPE_CUSTOM_BASE = 1000  // User-defined types start here
} ptk_type_id_t;

/**
 * @brief Type-safe handle structure
 * Contains both the allocation handle and type information
 */
typedef struct {
    ptk_shared_handle_t handle;
    ptk_type_id_t type_id;
    size_t element_size;
    size_t element_count;
} ptk_typed_handle_t;

#define PTK_TYPED_INVALID_HANDLE ((ptk_typed_handle_t){PTK_SHARED_INVALID_HANDLE, PTK_TYPE_INVALID, 0, 0})

//=============================================================================
// TYPED HANDLE DECLARATIONS
//=============================================================================

// Specific typed handles for common types
typedef struct { ptk_typed_handle_t base; } ptk_buffer_handle_t;
typedef struct { ptk_typed_handle_t base; } ptk_socket_handle_t;  
typedef struct { ptk_typed_handle_t base; } ptk_string_handle_t;
typedef struct { ptk_typed_handle_t base; } ptk_config_handle_t;
typedef struct { ptk_typed_handle_t base; } ptk_address_handle_t;

//=============================================================================
// TYPE-SAFE ALLOCATION MACROS
//=============================================================================

/**
 * @brief Allocate typed memory using _Generic dispatch
 * 
 * This macro provides compile-time type safety by dispatching to the correct
 * allocation function based on the pointer type.
 */
#define ptk_typed_alloc(ptr_type, count) _Generic((ptr_type*)0, \
    ptk_buf*: ptk_buffer_alloc_impl, \
    ptk_sock*: ptk_socket_alloc_impl, \
    char*: ptk_string_alloc_impl, \
    ptk_address_t*: ptk_address_alloc_impl, \
    default: ptk_generic_typed_alloc_impl \
)(count, sizeof(*((ptr_type*)0)), #ptr_type)

/**
 * @brief Get typed pointer from handle with compile-time type checking
 */
#define ptk_typed_get(handle_var, ptr_type) \
    (ptr_type*)ptk_typed_get_impl((handle_var).base, PTK_TYPE_##ptr_type, sizeof(ptr_type))

/**
 * @brief Type-safe handle validation
 */
#define ptk_typed_is_valid(handle_var, expected_type) \
    (PTK_SHARED_IS_VALID((handle_var).base.handle) && \
     (handle_var).base.type_id == PTK_TYPE_##expected_type)

//=============================================================================
// IMPLEMENTATION FUNCTIONS
//=============================================================================

/**
 * @brief Generic typed allocation implementation
 */
PTK_API ptk_typed_handle_t ptk_generic_typed_alloc_impl(size_t count, size_t element_size, const char *type_name);

/**
 * @brief Specific allocation functions for common types
 */
PTK_API ptk_buffer_handle_t ptk_buffer_alloc_impl(size_t count, size_t element_size, const char *type_name);
PTK_API ptk_socket_handle_t ptk_socket_alloc_impl(size_t count, size_t element_size, const char *type_name);
PTK_API ptk_string_handle_t ptk_string_alloc_impl(size_t count, size_t element_size, const char *type_name);
PTK_API ptk_address_handle_t ptk_address_alloc_impl(size_t count, size_t element_size, const char *type_name);

/**
 * @brief Get pointer from typed handle with type validation
 */
PTK_API void* ptk_typed_get_impl(ptk_typed_handle_t handle, ptk_type_id_t expected_type, size_t expected_size);

/**
 * @brief Free typed allocation
 */
PTK_API ptk_err_t ptk_typed_free(ptk_typed_handle_t *handle);

//=============================================================================
// CONVENIENCE MACROS FOR COMMON PATTERNS
//=============================================================================

/**
 * @brief Allocate and get pointer in one operation
 */
#define ptk_new(type, count) \
    ptk_typed_get(ptk_typed_alloc(type, count), type)

/**
 * @brief Allocate single object
 */
#define ptk_new_single(type) ptk_new(type, 1)

/**
 * @brief Safe cast with runtime type checking in debug builds
 */
#ifdef DEBUG
#define ptk_typed_cast(handle_var, target_type) \
    (ptk_typed_is_valid(handle_var, target_type) ? \
     ptk_typed_get(handle_var, target_type) : \
     (ptk_set_err(PTK_ERR_INVALID_PARAM), (target_type*)NULL))
#else
#define ptk_typed_cast(handle_var, target_type) \
    ptk_typed_get(handle_var, target_type)
#endif

//=============================================================================
// USAGE EXAMPLES
//=============================================================================

/*
Example usage:

// Type-safe allocation
ptk_buffer_handle_t buf_handle = ptk_typed_alloc(ptk_buf, 1);
ptk_string_handle_t str_handle = ptk_typed_alloc(char, 256);

// Get typed pointers
ptk_buf *buffer = ptk_typed_get(buf_handle, ptk_buf);
char *string = ptk_typed_get(str_handle, char);

// Convenience macros
ptk_buf *new_buffer = ptk_new_single(ptk_buf);
int *numbers = ptk_new(int, 100);

// Type validation
if (ptk_typed_is_valid(buf_handle, BUFFER)) {
    // Safe to use
}

// Cleanup
ptk_typed_free(&buf_handle);
ptk_typed_free(&str_handle);
*/