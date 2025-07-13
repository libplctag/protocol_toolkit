#pragma once

/**
 * @file ptk_shared.h
 * @brief Shared memory definitions for the Protocol Toolkit library
 * 
 * Shared memory is represented by a handle.  The handle is an opaque struct.
 * 
 * To create a new shared memory segment, use ptk_shared_create().  To use a shared memory segment
 * you need to get the underlying ponter with ptk_shared_acquire(), and release it with ptk_shared_release().
 * 
 * Shared memroy segments are reference counted.  When the last reference is released, the memory segment is freed.
 * 
 * the handle is an integer value (64-bit) that includes two parts, a unique index in a look up table, and a generation counter.
 * This allows handles to be safely reused after a segment is freed. And attempts to use a stale handle will be detected.
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


