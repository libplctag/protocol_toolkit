/***************************************************************************
 *   @file ptk_shared.c
 *   @brief Thread-safe shared memory implementation for Protocol Toolkit
 *
 *   This implementation provides thread-safe sharing of ptk_alloc'd memory
 *   using reference counting and generation counters for safety.
 ***************************************************************************/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <ptk_shared.h>
#include <ptk_alloc.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <ptk_os_thread.h>
#include <ptk_utils.h>

#define HANDLE_INDEX_MASK       0xFFFFFFFF
#define HANDLE_GENERATION_SHIFT 32
#define INITIAL_TABLE_SIZE      1024

typedef struct {
    uint64_t handle_value;      // Combined generation + index
    void *data_ptr;             // Points to ptk_alloc'd memory (NULL = free slot)
    uint32_t ref_count;         // Reference counter (protected by mutex)
    ptk_mutex *mutex;           // Per-entry mutex
    const char *file;           // Debug info from ptk_shared_wrap macro
    int line;                   // Debug info from ptk_shared_wrap macro
} shared_entry_t;

typedef struct {
    shared_entry_t *entries;    // Array of entries (NOT pointers)
    size_t capacity;            // Total table size
    size_t count;               // Entries in use
    uint32_t next_generation;   // Global generation counter
    ptk_mutex *table_mutex;     // Global table protection
} shared_table_t;

static shared_table_t shared_table = {0};
static bool shared_table_initialized = false;

static ptk_err init_shared_table(void) {
    if (shared_table_initialized) {
        return PTK_OK;
    }

    shared_table.table_mutex = ptk_mutex_create();
    if (!shared_table.table_mutex) {
        error("Failed to create shared table mutex");
        return PTK_ERR_NO_RESOURCES;
    }

    shared_table.entries = ptk_alloc(INITIAL_TABLE_SIZE * sizeof(shared_entry_t), NULL);
    if (!shared_table.entries) {
        error("Failed to allocate shared table entries");
        return PTK_ERR_NO_RESOURCES;
    }

    shared_table.capacity = INITIAL_TABLE_SIZE;
    shared_table.count = 0;
    shared_table.next_generation = 1;

    // Initialize all entry mutexes
    for (size_t i = 0; i < INITIAL_TABLE_SIZE; i++) {
        shared_table.entries[i].mutex = ptk_mutex_create();
        if (!shared_table.entries[i].mutex) {
            error("Failed to create entry mutex at index %zu", i);
            return PTK_ERR_NO_RESOURCES;
        }
        shared_table.entries[i].data_ptr = NULL;
        shared_table.entries[i].handle_value = 0;
        shared_table.entries[i].ref_count = 0;
        shared_table.entries[i].file = NULL;
        shared_table.entries[i].line = 0;
    }

    shared_table_initialized = true;
    return PTK_OK;
}

static ptk_err expand_shared_table(void) {
    size_t old_capacity = shared_table.capacity;
    size_t new_capacity = old_capacity * 2;

    shared_entry_t *new_entries = ptk_realloc(shared_table.entries, 
                                             new_capacity * sizeof(shared_entry_t));
    if (!new_entries) {
        error("Failed to expand shared table from %zu to %zu entries", old_capacity, new_capacity);
        return PTK_ERR_NO_RESOURCES;
    }

    // Initialize mutexes for new entries
    for (size_t i = old_capacity; i < new_capacity; i++) {
        new_entries[i].mutex = ptk_mutex_create();
        if (!new_entries[i].mutex) {
            error("Failed to create mutex for new entry at index %zu", i);
            return PTK_ERR_NO_RESOURCES;
        }
        new_entries[i].data_ptr = NULL;
        new_entries[i].handle_value = 0;
        new_entries[i].ref_count = 0;
        new_entries[i].file = NULL;
        new_entries[i].line = 0;
    }

    shared_table.entries = new_entries;
    shared_table.capacity = new_capacity;
    debug("Expanded shared table from %zu to %zu entries", old_capacity, new_capacity);
    return PTK_OK;
}

static shared_entry_t *find_free_slot_or_expand(void) {
    // Linear search for free slot
    for (size_t i = 0; i < shared_table.capacity; i++) {
        if (shared_table.entries[i].data_ptr == NULL) {
            return &shared_table.entries[i];
        }
    }

    // No free slots, need to expand
    if (expand_shared_table() != PTK_OK) {
        return NULL;
    }

    // Return first slot in the newly expanded area
    return &shared_table.entries[shared_table.capacity / 2];
}

static uint64_t create_new_handle(size_t index) {
    uint64_t generation = shared_table.next_generation++;
    if (shared_table.next_generation == 0) {
        shared_table.next_generation = 1; // Skip 0 to avoid invalid handles
    }
    
    return (generation << HANDLE_GENERATION_SHIFT) | (index & HANDLE_INDEX_MASK);
}

static shared_entry_t *lookup_entry_unsafe(ptk_shared_handle_t handle) {
    uint32_t index = handle.value & HANDLE_INDEX_MASK;
    if (index >= shared_table.capacity) {
        return NULL;
    }
    
    shared_entry_t *entry = &shared_table.entries[index];
    if (entry->handle_value != handle.value || entry->data_ptr == NULL) {
        return NULL;  // Invalid generation or cleared entry
    }
    return entry;
}

ptk_shared_handle_t ptk_shared_wrap_impl(const char *file, int line, void *ptr) {
    if (!ptr) {
        error("Cannot wrap NULL pointer at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_SHARED_INVALID_HANDLE;
    }

    // Initialize shared table if needed
    if (!shared_table_initialized) {
        if (init_shared_table() != PTK_OK) {
            error("Failed to initialize shared table");
            ptk_set_err(PTK_ERR_NO_RESOURCES);
            return PTK_SHARED_INVALID_HANDLE;
        }
    }

    ptk_err lock_result = ptk_mutex_wait_lock(shared_table.table_mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        error("Failed to lock shared table mutex");
        ptk_set_err(lock_result);
        return PTK_SHARED_INVALID_HANDLE;
    }

    // Check for double-wrapping (error condition)
    for (size_t i = 0; i < shared_table.capacity; i++) {
        if (shared_table.entries[i].data_ptr == ptr) {
            ptk_mutex_unlock(shared_table.table_mutex);
            error("Pointer %p already wrapped at %s:%d", ptr, file, line);
            ptk_set_err(PTK_ERR_INVALID_PARAM);
            return PTK_SHARED_INVALID_HANDLE;
        }
    }

    // Find free slot or expand table
    shared_entry_t *entry = find_free_slot_or_expand();
    if (!entry) {
        ptk_mutex_unlock(shared_table.table_mutex);
        error("Failed to find free slot for wrapping pointer at %s:%d", file, line);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_SHARED_INVALID_HANDLE;
    }

    // Calculate index for handle creation
    size_t entry_index = entry - shared_table.entries;

    // Initialize entry
    entry->handle_value = create_new_handle(entry_index);
    entry->data_ptr = ptr;
    entry->ref_count = 1;  // Initial reference
    entry->file = file;
    entry->line = line;
    
    shared_table.count++;

    ptk_mutex_unlock(shared_table.table_mutex);
    
    debug("Wrapped memory %p with handle 0x%016" PRIx64 " at index %zu from %s:%d", 
          ptr, entry->handle_value, entry_index, file, line);
    
    ptk_set_err(PTK_OK);
    return (ptk_shared_handle_t){entry->handle_value};
}

void *ptk_shared_acquire(ptk_shared_handle_t handle) {
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("Attempt to acquire invalid handle");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    if (!shared_table_initialized) {
        error("Shared table not initialized");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return NULL;
    }

    ptk_err lock_result = ptk_mutex_wait_lock(shared_table.table_mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        error("Failed to lock shared table mutex");
        ptk_set_err(lock_result);
        return NULL;
    }

    shared_entry_t *entry = lookup_entry_unsafe(handle);
    if (!entry) {
        ptk_mutex_unlock(shared_table.table_mutex);
        error("Invalid handle 0x%016" PRIx64, handle.value);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    lock_result = ptk_mutex_wait_lock(entry->mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        ptk_mutex_unlock(shared_table.table_mutex);
        error("Failed to lock entry mutex");
        ptk_set_err(lock_result);
        return NULL;
    }
    
    ptk_mutex_unlock(shared_table.table_mutex);  // Release table, keep entry locked

    // Check for zombie entry (ref_count is 0 but entry not cleaned up)
    if (entry->ref_count == 0) {
        ptk_mutex_unlock(entry->mutex);
        error("Found zombie entry with ref_count=0 at %s:%d", entry->file, entry->line);
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return NULL;
    }

    // Check for overflow
    if (entry->ref_count == UINT32_MAX) {
        ptk_mutex_unlock(entry->mutex);
        error("Reference count overflow at %s:%d", entry->file, entry->line);
        ptk_set_err(PTK_ERR_OVERFLOW);
        return NULL;
    }

    entry->ref_count++;
    
    trace("Acquired shared memory at %s:%d, ref_count=%u", 
          entry->file, entry->line, entry->ref_count);
    
    ptk_set_err(PTK_OK);
    return entry->data_ptr;  // Keep entry mutex locked for caller
}

ptk_err ptk_shared_release(ptk_shared_handle_t handle) {
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("Attempt to release invalid handle");
        return PTK_ERR_INVALID_PARAM;
    }

    if (!shared_table_initialized) {
        error("Shared table not initialized");
        return PTK_ERR_INVALID_STATE;
    }

    ptk_err lock_result = ptk_mutex_wait_lock(shared_table.table_mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        error("Failed to lock shared table mutex");
        return lock_result;
    }

    shared_entry_t *entry = lookup_entry_unsafe(handle);
    if (!entry) {
        ptk_mutex_unlock(shared_table.table_mutex);
        error("Invalid handle 0x%016" PRIx64 " during release", handle.value);
        return PTK_ERR_INVALID_PARAM;
    }

    // Entry mutex should be locked from acquire, but let's be safe
    lock_result = ptk_mutex_wait_lock(entry->mutex, PTK_TIME_NO_WAIT);
    if (lock_result != PTK_OK) {
        // If we can't acquire immediately, try with timeout
        lock_result = ptk_mutex_wait_lock(entry->mutex, PTK_TIME_WAIT_FOREVER);
        if (lock_result != PTK_OK) {
            ptk_mutex_unlock(shared_table.table_mutex);
            error("Failed to lock entry mutex during release");
            return lock_result;
        }
    }

    // Check for underflow (double release)
    if (entry->ref_count == 0) {
        error("Double release detected at %s:%d", entry->file, entry->line);
        entry->data_ptr = NULL;  // Mark slot as free
        entry->handle_value = 0;
        shared_table.count--;
        ptk_mutex_unlock(shared_table.table_mutex);
        ptk_mutex_unlock(entry->mutex);
        return PTK_ERR_INVALID_STATE;
    }

    entry->ref_count--;
    
    trace("Released shared memory at %s:%d, ref_count=%u", 
          entry->file, entry->line, entry->ref_count);

    if (entry->ref_count == 0) {
        void *temp_ptr = entry->data_ptr;
        const char *debug_file = entry->file;
        int debug_line = entry->line;
        
        entry->data_ptr = NULL;  // Mark slot as free
        entry->handle_value = 0;
        entry->file = NULL;
        entry->line = 0;
        shared_table.count--;

        ptk_mutex_unlock(shared_table.table_mutex);
        ptk_mutex_unlock(entry->mutex);

        debug("Freeing shared memory from %s:%d", debug_file, debug_line);
        ptk_free(&temp_ptr);  // Calls original destructor
    } else {
        ptk_mutex_unlock(shared_table.table_mutex);
        ptk_mutex_unlock(entry->mutex);
    }

    return PTK_OK;
}

ptk_err ptk_shared_realloc(ptk_shared_handle_t handle, size_t new_size) {
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("Attempt to realloc invalid handle");
        return PTK_ERR_INVALID_PARAM;
    }

    if (new_size == 0) {
        error("Invalid new_size 0 for shared memory realloc");
        return PTK_ERR_INVALID_PARAM;
    }

    // Use existing functions for safety
    void *ptr = ptk_shared_acquire(handle);
    if (!ptr) {
        error("Failed to acquire handle for realloc");
        return ptk_get_err();
    }

    ptk_err lock_result = ptk_mutex_wait_lock(shared_table.table_mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        ptk_shared_release(handle);
        error("Failed to lock shared table mutex for realloc");
        return lock_result;
    }

    // Get entry to update it (we know it's valid since acquire succeeded)
    shared_entry_t *entry = lookup_entry_unsafe(handle);
    if (!entry) {
        ptk_mutex_unlock(shared_table.table_mutex);
        ptk_shared_release(handle);
        error("Entry disappeared during realloc");
        return PTK_ERR_INVALID_STATE;
    }

    ptk_mutex_unlock(shared_table.table_mutex);

    // Realloc using existing ptk_realloc (handles temp pointer safely)
    void *temp = ptk_realloc(entry->data_ptr, new_size);
    if (!temp) {
        ptk_shared_release(handle);
        error("Failed to realloc shared memory at %s:%d to %zu bytes", 
              entry->file, entry->line, new_size);
        return PTK_ERR_NO_RESOURCES;
    }

    entry->data_ptr = temp;  // Update table entry
    
    debug("Reallocated shared memory at %s:%d to %zu bytes", 
          entry->file, entry->line, new_size);

    ptk_shared_release(handle);
    return PTK_OK;
}

ptk_err ptk_shared_init(void) {
    if (shared_table_initialized) {
        debug("Shared table already initialized");
        return PTK_OK;
    }
    
    ptk_err result = init_shared_table();
    if (result == PTK_OK) {
        info("Shared memory system initialized");
    } else {
        error("Failed to initialize shared memory system");
    }
    return result;
}

ptk_err ptk_shared_shutdown(void) {
    if (!shared_table_initialized) {
        debug("Shared table not initialized, nothing to shutdown");
        return PTK_OK;
    }

    info("Shutting down shared memory system");
    
    // Force cleanup of all remaining entries
    for (size_t i = 0; i < shared_table.capacity; i++) {
        shared_entry_t *entry = &shared_table.entries[i];
        if (entry->data_ptr) {
            error("Leaked shared memory at %s:%d during shutdown", 
                  entry->file, entry->line);
            ptk_free(&entry->data_ptr);
        }
        if (entry->mutex) {
            // Note: ptk_mutex_destroy not available, assuming cleanup handled elsewhere
        }
    }
    
    ptk_free(&shared_table.entries);
    shared_table.entries = NULL;
    shared_table.capacity = 0;
    shared_table.count = 0;
    shared_table.next_generation = 1;
    
    // Note: ptk_mutex_destroy not available, assuming cleanup handled elsewhere
    shared_table.table_mutex = NULL;
    shared_table_initialized = false;
    
    info("Shared memory system shutdown complete");
    return PTK_OK;
}