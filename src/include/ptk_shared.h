#pragma once


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
 *      ptk_shared_handle_t handle = ptk_shared_wrap(obj);
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
 * - ptk_shared_wrap(ptr): wrap a pointer in a shared handle
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
 *   ptk_shared_handle_t handle = ptk_shared_wrap(obj);
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
    uint64_t value;  // Opaque handle value
} ptk_shared_handle_t;

#define PTK_SHARED_INVALID_HANDLE ((ptk_shared_handle_t){0})
#define PTK_SHARED_IS_VALID(h) ((h).value != 0)
#define PTK_SHARED_HANDLE_EQUAL(a,b) ((a).value == (b).value)

#define ptk_shared_wrap(ptr) ptk_shared_wrap_impl(__FILE__, __LINE__, ptr)
extern ptk_shared_handle_t ptk_shared_wrap_impl(const char *file, int line, void *ptr);

extern ptk_err ptk_shared_init(void);
extern ptk_err ptk_shared_shutdown(void);

extern void *ptk_shared_acquire(ptk_shared_handle_t handle);
extern ptk_err ptk_shared_realloc(ptk_shared_handle_t handle, size_t new_size);  // reuses the existing handle.
extern ptk_err ptk_shared_release(ptk_shared_handle_t handle);

/**
 * @brief Macro for safely using shared memory with automatic acquire/release
 * 
 * This macro provides a convenient way to work with shared memory by automatically
 * handling acquire/release operations and providing error handling.
 * 
 * Usage:
 * use_shared(handle, my_struct_t *obj) {
 *     // Code that uses obj - obj is automatically cast to the correct type
 *     obj->field = value;
 * } on_shared_fail {
 *     // Error handling code - runs if acquire fails
 *     error("Failed to acquire shared memory");
 * }
 * 
 * @param handle The shared memory handle to acquire
 * @param declaration The typed pointer declaration (e.g., "my_struct_t *obj")
 */
#define use_shared(handle, declaration) \
    for (int _ptk_shared_once = 0; _ptk_shared_once < 1; _ptk_shared_once++) \
        for (void *_ptk_shared_raw_ptr = ptk_shared_acquire(handle); \
             _ptk_shared_once < 1; \
             _ptk_shared_once++, (_ptk_shared_raw_ptr ? ptk_shared_release(handle) : (void)0)) \
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


