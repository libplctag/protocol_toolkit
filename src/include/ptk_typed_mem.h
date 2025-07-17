#pragma once

/**
 * @file ptk_typed_mem.h
 * @brief Type-safe memory allocation system using declaration macros
 * 
 * This provides a simple, scalable way to create type-safe allocation functions
 * similar to the PTK_ARRAY_DECLARE system. Users can easily declare typed
 * allocators for any type, including their own custom types.
 */

#include <ptk_defs.h>
#include <ptk_mem.h>
#include <ptk_err.h>

//=============================================================================
// TYPE-SAFE ALLOCATOR DECLARATION MACRO
//=============================================================================

/**
 * @brief Declare type-safe allocation functions for a specific type
 * 
 * This macro generates type-safe allocation functions similar to how
 * PTK_ARRAY_DECLARE works. It creates functions that return properly
 * typed pointers instead of void*.
 * 
 * For each type T with prefix PREFIX, this generates:
 * - PREFIX_alloc(count) -> T* (local allocation)
 * - PREFIX_shared_alloc(count) -> ptk_shared_handle_t (shared allocation)
 * - PREFIX_shared_get(handle) -> T* (get from shared handle)
 * - PREFIX_free(ptr) -> void (free local allocation)
 * - PREFIX_shared_free(handle) -> void (free shared allocation)
 * 
 * @param PREFIX Function name prefix (e.g., "buffer" creates buffer_alloc)
 * @param T The type to allocate (e.g., ptk_buf)
 * 
 * Example:
 *   PTK_TYPED_MEM_DECLARE(buffer, ptk_buf);
 *   
 *   // Creates these functions:
 *   ptk_buf* buffer_alloc(size_t count);
 *   ptk_shared_handle_t buffer_shared_alloc(size_t count);
 *   ptk_buf* buffer_shared_get(ptk_shared_handle_t handle);
 *   void buffer_free(ptk_buf **ptr);
 *   void buffer_shared_free(ptk_shared_handle_t *handle);
 */
#define PTK_TYPED_MEM_DECLARE(PREFIX, T) \
    static inline T* PREFIX##_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        T* ptr = (T*)ptk_local_alloc(sizeof(T) * count, NULL); \
        if (!ptr) { \
            ptk_set_err(PTK_ERR_NO_RESOURCES); \
        } \
        return ptr; \
    } \
    \
    static inline ptk_shared_handle_t PREFIX##_shared_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return PTK_SHARED_INVALID_HANDLE; \
        } \
        return ptk_shared_alloc(sizeof(T) * count, NULL); \
    } \
    \
    static inline T* PREFIX##_shared_get(ptk_shared_handle_t handle) { \
        if (!PTK_SHARED_IS_VALID(handle)) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        T* ptr = NULL; \
        use_shared(handle, T*, ptr, PTK_TIME_NO_WAIT) { \
            return ptr; \
        } on_shared_fail { \
            ptk_set_err(PTK_ERR_INVALID_STATE); \
            return NULL; \
        } \
    } \
    \
    static inline void PREFIX##_free(T **ptr) { \
        if (ptr) { \
            ptk_local_free((void**)ptr); \
        } \
    } \
    \
    static inline void PREFIX##_shared_free(ptk_shared_handle_t *handle) { \
        if (handle) { \
            ptk_shared_free(handle); \
        } \
    } \
    \
    static inline T* PREFIX##_new(void) { \
        return PREFIX##_alloc(1); \
    } \
    \
    static inline ptk_shared_handle_t PREFIX##_shared_new(void) { \
        return PREFIX##_shared_alloc(1); \
    }

//=============================================================================
// ENHANCED ALLOCATOR WITH DESTRUCTOR SUPPORT
//=============================================================================

/**
 * @brief Declare type-safe allocation functions with destructor support
 * 
 * Similar to PTK_TYPED_MEM_DECLARE but allows specifying a destructor
 * function that will be called when the memory is freed.
 * 
 * @param PREFIX Function name prefix
 * @param T The type to allocate  
 * @param DESTRUCTOR Destructor function name (or NULL)
 */
#define PTK_TYPED_MEM_DECLARE_WITH_DESTRUCTOR(PREFIX, T, DESTRUCTOR) \
    static inline T* PREFIX##_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        T* ptr = (T*)ptk_local_alloc(sizeof(T) * count, DESTRUCTOR); \
        if (!ptr) { \
            ptk_set_err(PTK_ERR_NO_RESOURCES); \
        } \
        return ptr; \
    } \
    \
    static inline ptk_shared_handle_t PREFIX##_shared_alloc(size_t count) { \
        if (count == 0) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return PTK_SHARED_INVALID_HANDLE; \
        } \
        return ptk_shared_alloc(sizeof(T) * count, DESTRUCTOR); \
    } \
    \
    static inline T* PREFIX##_shared_get(ptk_shared_handle_t handle) { \
        if (!PTK_SHARED_IS_VALID(handle)) { \
            ptk_set_err(PTK_ERR_INVALID_PARAM); \
            return NULL; \
        } \
        T* ptr = NULL; \
        use_shared(handle, T*, ptr, PTK_TIME_NO_WAIT) { \
            return ptr; \
        } on_shared_fail { \
            ptk_set_err(PTK_ERR_INVALID_STATE); \
            return NULL; \
        } \
    } \
    \
    static inline void PREFIX##_free(T **ptr) { \
        if (ptr) { \
            ptk_local_free((void**)ptr); \
        } \
    } \
    \
    static inline void PREFIX##_shared_free(ptk_shared_handle_t *handle) { \
        if (handle) { \
            ptk_shared_free(handle); \
        } \
    } \
    \
    static inline T* PREFIX##_new(void) { \
        return PREFIX##_alloc(1); \
    } \
    \
    static inline ptk_shared_handle_t PREFIX##_shared_new(void) { \
        return PREFIX##_shared_alloc(1); \
    }

//=============================================================================
// PREDEFINED COMMON TYPE ALLOCATORS
//=============================================================================

// Declare allocators for common Protocol Toolkit types
PTK_TYPED_MEM_DECLARE(ptk_string, char);
PTK_TYPED_MEM_DECLARE(ptk_u8, ptk_u8_t);
PTK_TYPED_MEM_DECLARE(ptk_u16, ptk_u16_t);
PTK_TYPED_MEM_DECLARE(ptk_u32, ptk_u32_t);
PTK_TYPED_MEM_DECLARE(ptk_u64, ptk_u64_t);
PTK_TYPED_MEM_DECLARE(ptk_i8, ptk_i8_t);
PTK_TYPED_MEM_DECLARE(ptk_i16, ptk_i16_t);
PTK_TYPED_MEM_DECLARE(ptk_i32, ptk_i32_t);
PTK_TYPED_MEM_DECLARE(ptk_i64, ptk_i64_t);
PTK_TYPED_MEM_DECLARE(ptk_int, int);
PTK_TYPED_MEM_DECLARE(ptk_float, float);
PTK_TYPED_MEM_DECLARE(ptk_double, double);

// Address allocator will be declared in ptk_sock.h since that's where ptk_address_t is defined
// PTK_TYPED_MEM_DECLARE(ptk_address, ptk_address_t);

//=============================================================================
// USAGE EXAMPLES AND PATTERNS
//=============================================================================

/*
Example usage:

// 1. Use predefined allocators:
char *my_string = ptk_string_alloc(256);
ptk_u32_t *numbers = ptk_u32_alloc(100);
ptk_address_t *addr = ptk_address_new();  // Single allocation

// 2. Create custom type allocators:
typedef struct {
    int id;
    char name[64];
    float value;
} my_data_t;

PTK_TYPED_MEM_DECLARE(my_data, my_data_t);

// Now you can use:
my_data_t *data = my_data_alloc(10);
my_data_t *single = my_data_new();

// 3. Shared memory allocations:
ptk_shared_handle_t handle = ptk_u32_shared_alloc(1000);
ptk_u32_t *shared_numbers = ptk_u32_shared_get(handle);

// 4. With destructors:
void cleanup_my_data(void *ptr) {
    my_data_t *data = (my_data_t*)ptr;
    // Custom cleanup logic here
}

PTK_TYPED_MEM_DECLARE_WITH_DESTRUCTOR(my_managed_data, my_data_t, cleanup_my_data);

// 5. Cleanup:
ptk_string_free(&my_string);
ptk_u32_free(&numbers);
ptk_address_free(&addr);
my_data_free(&data);
ptk_u32_shared_free(&handle);
*/