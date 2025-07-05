#include "ptk_atomic.h"
#include "ptk_err.h"
#include "ptk_thread.h"

#include "ptk_shared_mem.h"


/**
 * @brief This is the implementation for a handle-based system of managing shared memory
 *
 * Handles are looked up in a lookup table.  The handle value is composed of:
 *
 * bits 0-7 - the handle generation,
 * bits 8-30 - the handle index in the lookup table.
 *
 * bit 31 is not used so that the value can be cast to a signed 32-bit int.
 *
 * When there are no empty slots, the number of slots is increased and the various internal tables
 * are expanded.
 *
 * The in_use_flags array is a bit set.  If the specific bit is zero, then the corresponding slot
 * is not in use.  If the bit is set, then the slot is in use.
 *
 */

typedef struct shared_memory_entry {
    void *data;
    void (*dtor)(void *);
    ptk_mutex *entry_mutex;
    ptk_atomic uint32_t ref_count;
    ptk_atomic uint8_t generation;
} shared_memory_entry;


typedef uint64_t bit_string_uint;

typedef struct ptk_shared_memory_manager {
    ptk_mutex *manager_mutex;
    size_t num_slots;
    bit_string_uint *in_use_flags;
    shared_memory_entry *entries;
} ptk_shared_memory_manager;


ptk_shared_memory_manager ptk_atomic *shared_memory_manager = NULL;

/**
 * @brief Sets up global structures for shared memory handling.
 *
 * @param manager
 * @param initial_slots
 * @return ptk_err
 */
ptk_err ptk_shared_memory_manager_startup(size_t initial_slots) {}

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
ptk_err ptk_shared_memory_manager_shutdown(void) {}


/**
 * @brief These declarations are for use via the macro.
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
ptk_err ptk_shared_make_impl(const char *func, int line, ptk_shared_handle *out, void *ptr, void (*dtor)(void *)) {}

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
void *ptk_shared_acquire_impl(const char *func, int line, ptk_shared_handle h) {}

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
void ptk_shared_release_impl(const char *func, int line, ptk_shared_handle h) {}
