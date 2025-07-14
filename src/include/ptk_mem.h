#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ptk_err.h>

/**
 * @brief Allocate local memory with optional destructor
 *
 * Creates a new memory allocation. If a destructor is provided, it will be called
 * when the allocation is freed.
 *
 * @param size Size of memory to allocate in bytes
 * @param destructor Optional cleanup function called during deallocation
 * @return Pointer to allocated memory, or NULL on failure
 *
 * @note This is a macro that automatically captures file/line information
 *
 * @example
 * ```c
 * // Create an allocation
 * void *obj = ptk_local_alloc(1024, NULL);
 *
 * // free it later, calls the destructor if provided
 * ptk_free(obj);
 * ```
 */
#define ptk_local_alloc(size, destructor) \
    ptk_local_alloc_impl(__FILE__, __LINE__, size, destructor)
extern void *ptk_local_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr));

/**
 * @brief Resize an allocated memory block
 *
 * Changes the size of an existing allocation.
 *
 * This fails if called on a NULL pointer or if the new size is 0 or if the memory is
 * already marked as free.  Failure to reallocate will return NULL and log an error.
 *
 * If the new size is larger than the current size, the contents are preserved up to the minimum of old and new sizes.
 * The new memory is zeroed out if the new size is larger than the old size.
 *
 * @param ptr Pointer to memory block to resize
 * @param new_size New size in bytes
 * @return Pointer to resized memory block, or NULL on failure
 *
 * @note This is a macro that automatically captures file/line information
 * @note Contents are preserved up to the minimum of old and new sizes
 * @note Reallocation may change the memory address
 *
 * @example
 * ```c
 * void *ptr = ptk_local_alloc(1024, NULL);
 * ptr = ptk_local_realloc(ptr, 2048);  // Resize to 2048 bytes
 * ```
 */
#define ptk_local_realloc(ptr, new_size) \
    ptk_local_realloc_impl(__FILE__, __LINE__, ptr, new_size)
extern void *ptk_local_realloc_impl(const char *file, int line, void *ptr, size_t new_size);


/**
 * @brief Free a locally allocated memory block after calling its destructor
 *
 * Calls the destructor if provided, then frees the memory and sets the pointer to NULL.
 *
 * @param ptr_ref Pointer to pointer to memory block to free
 *
 * @note This is a macro that automatically captures file/line information
 * @note Destructors are called before freeing each allocation
 * @note It is safe to call ptk_free(NULL) or ptk_free(&null_ptr)
 * @note The pointer will be set to NULL after freeing to prevent use-after-free bugs
 *
 * @example
 * ```c
 * void *ptr = ptk_local_alloc(1024, NULL);
 * ptk_free(&ptr);  // ptr is now NULL
 * ```
 */
#define ptk_local_free(ptr_ref) \
    ptk_local_free_impl(__FILE__, __LINE__, (void **)(ptr_ref))
extern void ptk_local_free_impl(const char *file, int line, void **ptr_ref);





//=============================================================================
// SHARED MEMORY API
//=============================================================================


/**
 * @file ptk_shared.h
 * @brief Type-safe, reference-counted shared memory API for Protocol Toolkit
 *
 * This module provides a robust abstraction for managing shared memory segments in C.
 * Shared memory is accessed via opaque handles (`ptk_shared_handle_t`), which encapsulate
 * a unique index and generation counter for safety against stale or reused handles.
 *
 * Key Features:
 * - Type-safe handle-based access to shared memory
 * - Automatic reference counting: memory is freed when the last reference is released
 * - Safe handle reuse: attempts to use stale handles are detected and rejected
 * - Convenient macros for safe acquire/release and error handling
 *
 * Usage Overview:
 * 1. Initialize the shared memory subsystem:
 *      ptk_shared_init();
 *
 * 2. Create a shared memory segment:
 *      my_struct_t *obj = ptk_alloc(sizeof(my_struct_t), my_destructor_fn);
 *      ptk_shared_handle_t handle = ptk_shared_create(obj);
 *
 * 3. Acquire and use the memory safely:
 *      use_shared(handle, my_struct_t *ptr) {
 *          // Use ptr as a normal pointer
 *          ptr->field = value;
 *      } on_shared_fail {
 *          // Handle acquisition failure
 *          error("Failed to acquire shared memory");
 *      }
 *
 * 4. Release the memory when done:
 *      ptk_shared_release(handle);

 * 5. Optionally resize the segment:
 *      ptk_shared_realloc(handle, new_size);

 * 6. Shutdown the subsystem:
 *      ptk_shared_shutdown();

 * Memory Management:
 * - All allocations should use ptk_alloc() from ptk_alloc.h, not malloc/free.
 * - When the reference count for a shared segment reaches zero, ptk_free() is called
 *   and the registered destructor (if any) will be invoked automatically.
 *
 * API Summary:
 * - ptk_shared_init(), ptk_shared_shutdown(): global setup/teardown
 * - ptk_shared_create(ptr): wrap a pointer in a shared handle
 * - ptk_shared_acquire(handle): get a pointer from a handle
 * - ptk_shared_release(handle): release a reference
 * - ptk_shared_realloc(handle, new_size): resize segment
 * - Macros: PTK_SHARED_INVALID_HANDLE, PTK_SHARED_IS_VALID, PTK_SHARED_HANDLE_EQUAL
 * - use_shared(handle, declaration) { ... } on_shared_fail { ... }: safe usage pattern
 *
 * Design Notes:
 * - Handles are 64-bit values, combining a table index and generation counter
 * - All memory segments are reference counted; freeing is automatic
 * - Macros provide ergonomic, error-safe access for typical use cases
 * - Intended for use in multi-threaded and event-driven applications
 *
 * Example:
 *   my_struct_t *obj = ptk_alloc(sizeof(my_struct_t), my_destructor_fn);
 *   ptk_shared_handle_t handle = ptk_shared_create(obj);
 *   use_shared(handle, my_struct_t *obj_ptr) {
 *       obj_ptr->field = 42;
 *       // WARNING: Do NOT use 'return' inside this block. Only 'break' is allowed.
 *   } on_shared_fail {
 *       error("Could not acquire shared memory");
 *   }
 *   ptk_shared_release(handle);

 * WARNING:
 *   You cannot use 'return' within a use_shared block. Only 'break' is allowed to exit early.
 *   Using 'return' will result in undefined behavior and may leak resources.

 * See documentation for details on error codes and advanced usage.
 */


typedef struct ptk_shared_handle {
    uintptr_t value;  // Opaque handle value
} ptk_shared_handle_t;

#define PTK_SHARED_INVALID_HANDLE ((ptk_shared_handle_t){0})
#define PTK_SHARED_IS_VALID(h) ((h).value != 0)
#define PTK_SHARED_HANDLE_EQUAL(a,b) ((a).value == (b).value)

/**
 * @brief Initialize the shared memory subsystem.
 *
 * @return ptk_err status code
 * @retval PTK_OK on success
 */
extern ptk_err ptk_shared_init(void);

/**
 * @brief Shut down the shared memory subsystem.
 *
 * @return ptk_err status code
 * @retval PTK_OK on success
 */
extern ptk_err ptk_shared_shutdown(void);


/**
 * @brief Allocate a shared memory block.
 *
 * @param size Size of the memory block to allocate
 * @param destructor Optional destructor function for the memory block
 * @return ptk_shared_handle_t Handle to the allocated memory, or PTK_SHARED_INVALID_HANDLE on failure
 */
#define ptk_shared_alloc(size, destructor) ptk_shared_alloc_impl(__FILE__, __LINE__, size, destructor)
extern ptk_shared_handle_t ptk_shared_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr));

#define ptk_shared_realloc(handle, new_size) ptk_shared_realloc_impl(__FILE__, __LINE__, handle, new_size)
extern ptk_err ptk_shared_realloc_impl(const char *file, int line, ptk_shared_handle_t handle, size_t new_size);  // reuses the existing handle.

#define ptk_shared_acquire(handle, timeout) ptk_shared_acquire_impl(__FILE__, __LINE__, handle, timeout)
extern void *ptk_shared_acquire_impl(const char *file, int line, ptk_shared_handle_t handle, ptk_time_ms timeout);

#define ptk_shared_release(handle) ptk_shared_release_impl(__FILE__, __LINE__, handle)
extern ptk_err ptk_shared_release_impl(const char *file, int line, ptk_shared_handle_t handle);

/**
 * @brief Macro for safely using shared memory with automatic acquire/release
 * 
 * This macro provides a convenient way to work with shared memory by automatically
 * handling acquire/release operations and providing error handling.
 * 
 * Usage:
 * use_shared(handle, timeout_ms, my_struct_t *obj) {
 *     // Code that uses obj - obj is automatically cast to the correct type
 *     obj->field = value;
 * } on_shared_fail {
 *     // Error handling code - runs if acquire fails or times out
 *     error("Failed to acquire shared memory");
 * }
 * 
 * @param handle The shared memory handle to acquire
 * @param timeout_ms Timeout in milliseconds (use PTK_TIME_WAIT_FOREVER or PTK_TIME_NO_WAIT)
 * @param declaration The typed pointer declaration (e.g., "my_struct_t *obj")
 * 
 * NOTE: ptk_shared_release_impl must be called even if the return value is NULL, to avoid leaks.
 *       The macro ensures this happens automatically.
 */
#define use_shared(handle, timeout_ms, declaration) \
    for (int _ptk_shared_once = 0; _ptk_shared_once < 1; _ptk_shared_once++) \
        for (void *_ptk_shared_raw_ptr = ptk_shared_acquire_impl(__FILE__, __LINE__, handle, timeout_ms); \
             _ptk_shared_once < 1; \
             _ptk_shared_once++, ptk_shared_release_impl(__FILE__, __LINE__, handle)) \
            if (_ptk_shared_raw_ptr != NULL) \
                for (declaration = (typeof(declaration))_ptk_shared_raw_ptr; \
                     _ptk_shared_raw_ptr != NULL; \
                     _ptk_shared_raw_ptr = NULL)

/**
 * @brief Error handling clause for use_shared macro
 * 
 * This must immediately follow a use_shared block to handle acquisition failures.
 */
#define on_shared_fail else



/**
 * @brief Free a shared allocated memory block after calling its destructor
 *
 * Decrements the reference count for the shared memory handle. If the reference count
 * reaches zero, the memory is freed and its destructor (if any) is called.
 *
 * @param ptr_ref Pointer to pointer to memory block to free
 *
 * @note This is a macro that automatically captures file/line information
 * @note Destructors are called before freeing each allocation
 * @note It is safe to call ptk_free(NULL) or ptk_free(&null_ptr)
 * @note The pointer will be set to NULL after freeing to prevent use-after-free bugs
 *
 * @example
 * ```c
 * void *ptr = ptk_shared_alloc(1024, NULL);
 * ptk_free(&ptr);  // ptr is now NULL
 * ```
 */
#define ptk_shared_free(ptr_ref) \
    ptk_shared_free_impl(__FILE__, __LINE__, (void **)(ptr_ref))
extern void ptk_shared_free_impl(const char *file, int line, void **ptr_ref);


