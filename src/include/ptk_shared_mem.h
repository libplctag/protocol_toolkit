#pragma once

#include <stdint.h>
#include <stddef.h>

#include "ptk_err.h"

/**
 * @brief Create the global structures needed to support shared memory handling.
 *
 * Not threadsafe.
 *
 * @param manager
 * @param initial_slots
 * @return ptk_err
 */
ptk_err ptk_shared_memory_manager_startup(size_t initial_slots);

/**
 * @brief Free up resources for a shared memory manager.
 *
 * This will call the dispose function on all existing handles.
 *
 * Not threadsafe.
 *
 * @param manager
 * @return ptk_err
 */
ptk_err ptk_shared_memory_manager_shutdown(void);




/**
 * @brief shared handle wrapper.
 */
typedef struct { uint32_t id; } ptk_shared_handle;




/**
 * @brief These declarations are for internal use, use the macro version.
 */

/**
 * @brief set up a handle to reference the shared memory block.
 *
 * The block will be protected by a mutex and a reference counter.
 *
 * If the reference counter hits zero, the destructor is called and the handle
 * is removed from the lookup table.
 *
 * Threadsafe.
 *
 * @param func - the name of the calling function.
 * @param line - the line number in the file where this is called.
 * @param out
 * @param ptr
 * @param dtor
 * @return ptk_err
 */
ptk_err ptk_shared_make_impl(const char *func, int line, ptk_shared_handle *out, void *ptr, void (*dtor)(void *));

/**
 * @brief Acquire a shared memory resource given its handle
 *
 * This looks up the handle information and if it is found:
 *  - atomically does a load to get the current reference count.
 *  - increments that value if it is greater than zero.
 *  - tries to do an atomic compare and swap to set the value.
 *  - if that succeeds, it tries to lock the shared resource mutex.
 *
 * The call is threadsafe.
 *
 * @param func - a string of the calling function name.
 * @param line - the line number in the source file where it was called.
 * @param h
 * @return void*
 */
void *ptk_shared_acquire_impl(const char *func, int line, ptk_shared_handle h);

/**
 * @brief Releases a shared memory resource as referenced by its handle.
 *
 * This looks up the handle information and if it is found:
 *  - if that succeeds, it tries to unlock the shared resource mutex.
 *  - atomically does a load to get the current reference count.
 *  - decrements that value if it is greater than zero.
 *  - tries to do an atomic compare and swap to set the value.
 *
 * Threadsafe
 *
 * @param func - the name of the calling function.
 * @param line - the line number in the file where this is called.
 * @param h
 */
void ptk_shared_release_impl(const char *func, int line, ptk_shared_handle h);


/**
 * @brief Define type-safe versions of shared handle and stored handle.
 *
 * @param TYPE Logical name prefix for user-defined type (e.g., `mytype`)
 */
#define PTK_DEFINE_SHARED_TYPE(TYPE) \
    typedef struct { ptk_shared_handle raw; } TYPE##_shared_handle; \
    typedef struct { ptk_shared_stored_handle raw; } TYPE##_shared_stored_handle;

/**
 * @brief Generate type-safe wrapper functions for a shared pointer-like API.
 *
 * @param TYPE The identifier name (e.g., `mytype`)
 * @param REALTYPE The underlying type being shared (e.g., `struct mytype`)
 */
#define PTK_DECLARE_SHARED_TYPE_API(TYPE, REALTYPE) \
    /** Create a new shared object of type REALTYPE. */ \
    static inline ptk_err TYPE##_shared_make(TYPE##_shared_handle *out, REALTYPE *ptr, void (*dtor)(REALTYPE *)) { \
        return ptk_shared_make_impl(__func__, __LINE__,&out->raw, (void *)ptr, (void (*)(void *))dtor); \
    } \
    \
    /** Acquire a reference to the shared object. */ \
    static inline REALTYPE *TYPE##_shared_acquire(TYPE##_shared_handle h) { \
        return (REALTYPE *)ptk_shared_acquire_impl(__func__, __LINE__, h.raw.id); \
    } \
    \
    /** Release a previously acquired reference. */ \
    static inline void TYPE##_shared_release(TYPE##_shared_handle h) { \
        ptk_shared_release_impl(__func__, __LINE__, h.raw.id); \
    }
