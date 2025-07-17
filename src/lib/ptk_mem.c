/**
 * @file ptk_mem.c
 * @brief Implementation of unified memory management for Protocol Toolkit
 *
 * See code_creation_guidelines.md for style and documentation rules.
 */

#include <ptk_mem.h>
#include <ptk_utils.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define PTK_ALLOC_ALIGNMENT 16
#define PTK_ALLOC_HEADER_CANARY 0xDEADBEEFCAFEBABEULL
#define PTK_ALLOC_FOOTER_CANARY 0xFEEDFACEDEADC0DEULL

typedef struct ptk_local_alloc_header {
    uint64_t header_canary;         // Must be first for detection
    void (*destructor)(void *ptr);
    size_t size;
    const char *file;
    int line;
} ptk_local_alloc_header_t;

typedef struct ptk_local_alloc_footer {
    uint64_t footer_canary;
} ptk_local_alloc_footer_t;

static size_t ptk_round_up_16(size_t sz) { return (sz + PTK_ALLOC_ALIGNMENT - 1) & ~(PTK_ALLOC_ALIGNMENT - 1); }

static ptk_local_alloc_header_t *ptk_get_header(void *user_ptr) {
    if(!user_ptr) { return NULL; }
    return (ptk_local_alloc_header_t *)((uint8_t *)user_ptr - sizeof(ptk_local_alloc_header_t));
}

static ptk_local_alloc_footer_t *ptk_get_footer(ptk_local_alloc_header_t *header) {
    if(!header) { return NULL; }
    uint8_t *user_ptr = (uint8_t *)header + sizeof(ptk_local_alloc_header_t);
    return (ptk_local_alloc_footer_t *)(user_ptr + header->size);
}

static bool ptk_validate_canaries(void *user_ptr) {
    if(!user_ptr) {
        ptk_set_err(PTK_ERR_NULL_PTR);
        return false;
    }
    
    ptk_local_alloc_header_t *header = ptk_get_header(user_ptr);
    if(!header) {
        ptk_set_err(PTK_ERR_NULL_PTR);
        return false;
    }
    
    if(header->header_canary != PTK_ALLOC_HEADER_CANARY) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return false;
    }
    
    ptk_local_alloc_footer_t *footer = ptk_get_footer(header);
    if(!footer) {
        ptk_set_err(PTK_ERR_NULL_PTR);
        return false;
    }
    
    if(footer->footer_canary != PTK_ALLOC_FOOTER_CANARY) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return false;
    }
    
    return true;
}

void *ptk_local_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr)) {
    if(size == 0) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    size_t aligned_size = ptk_round_up_16(size);
    size_t total_size = sizeof(ptk_local_alloc_header_t) + aligned_size + sizeof(ptk_local_alloc_footer_t);
    
    ptk_local_alloc_header_t *header = (ptk_local_alloc_header_t *)malloc(total_size);
    if(!header) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    header->header_canary = PTK_ALLOC_HEADER_CANARY;
    header->destructor = destructor;
    header->size = aligned_size;
    header->file = file;
    header->line = line;
    
    ptk_local_alloc_footer_t *footer = ptk_get_footer(header);
    footer->footer_canary = PTK_ALLOC_FOOTER_CANARY;
    
    void *user_ptr = (uint8_t *)header + sizeof(ptk_local_alloc_header_t);
    memset(user_ptr, 0, aligned_size);
    
    debug("Allocated %zu bytes at %p from %s:%d", size, user_ptr, file, line);
    
    ptk_set_err(PTK_OK);
    return user_ptr;
}

void *ptk_local_realloc_impl(const char *file, int line, void *ptr, size_t new_size) {
    if(!ptr) {
        return ptk_local_alloc_impl(file, line, new_size, NULL);
    }
    
    if(new_size == 0) {
        ptk_local_free_impl(file, line, &ptr);
        return NULL;
    }
    
    if(!ptk_validate_canaries(ptr)) {
        error("Invalid canaries in realloc at %s:%d", file, line);
        return NULL;
    }
    
    ptk_local_alloc_header_t *old_header = ptk_get_header(ptr);
    void *new_ptr = ptk_local_alloc_impl(file, line, new_size, old_header->destructor);
    if(!new_ptr) {
        return NULL;
    }
    
    size_t copy_size = (old_header->size < new_size) ? old_header->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    
    ptk_local_free_impl(file, line, &ptr);
    return new_ptr;
}

void ptk_local_free_impl(const char *file, int line, void **ptr_ref) {
    if(!ptr_ref || !*ptr_ref) {
        return;
    }
    
    void *ptr = *ptr_ref;
    
    if(!ptk_validate_canaries(ptr)) {
        error("Invalid canaries in free at %s:%d", file, line);
        return;
    }
    
    ptk_local_alloc_header_t *header = ptk_get_header(ptr);
    
    if(header->destructor) {
        header->destructor(ptr);
    }
    
    debug("Freed %zu bytes at %p from %s:%d", header->size, ptr, file, line);
    
    memset(header, 0, sizeof(ptk_local_alloc_header_t) + header->size + sizeof(ptk_local_alloc_footer_t));
    free(header);
    
    *ptr_ref = NULL;
    ptk_set_err(PTK_OK);
}


/**
 * @brief Check if a pointer was allocated by ptk_local_alloc()
 *
 * Returns true if the pointer was allocated by ptk_local_alloc(), false otherwise.
 * This checks for the special header and footer canary values.
 *
 * @param ptr Pointer to check
 * @return true if allocated by ptk_local_alloc(), false otherwise
 */
bool ptk_local_is_allocated(void *ptr) {
    if (!ptr) return false;
    ptk_local_alloc_header_t *header = ptk_get_header(ptr);
    if (!header) return false;
    if (header->header_canary != PTK_ALLOC_HEADER_CANARY) return false;
    ptk_local_alloc_footer_t *footer = ptk_get_footer(header);
    if (!footer) return false;
    if (footer->footer_canary != PTK_ALLOC_FOOTER_CANARY) return false;
    return true;
}

//=============================================================================
// SHARED MEMORY IMPLEMENTATION (Simplified)
//=============================================================================

#define INITIAL_TABLE_SIZE      1024

// POSIX mutex for Linux
#include <pthread.h>
typedef struct {
    uintptr_t handle_value;      // Combined generation + index
    void *data_ptr;             // Points to ptk_local_alloc'd memory (NULL = free slot)
    uint32_t ref_count;         // Reference counter (atomic)
    const char *file;           // Debug info from ptk_shared_create macro
    int line;                   // Debug info from ptk_shared_create macro
    pthread_mutex_t mutex;      // Per-entry mutex
} shared_entry_t;

typedef struct {
    shared_entry_t *entries;    // Array of entries (NOT pointers)
    size_t capacity;            // Total table size
    size_t count;               // Entries in use
    uint32_t next_generation;   // Global generation counter
} shared_table_t;

static shared_table_t shared_table = {0};
static bool shared_table_initialized = false;

static ptk_err_t init_shared_table(void) {
    if (shared_table_initialized) {
        return PTK_OK;
    }
    
    shared_table.entries = ptk_local_alloc(INITIAL_TABLE_SIZE * sizeof(shared_entry_t), NULL);
    if (!shared_table.entries) {
        error("Failed to allocate shared table entries");
        return PTK_ERR_NO_RESOURCES;
    }
    
    shared_table.capacity = INITIAL_TABLE_SIZE;
    shared_table.count = 0;
    shared_table.next_generation = 1;
    
    shared_table_initialized = true;
    debug("Shared table initialized with capacity %zu", shared_table.capacity);
    
    return PTK_OK;
}

ptk_err_t ptk_shared_init(void) {
    return init_shared_table();
}

ptk_err_t ptk_shared_shutdown(void) {
    if (!shared_table_initialized) {
        return PTK_OK;
    }
    
    // Free all remaining entries
    for (size_t i = 0; i < shared_table.capacity; i++) {
        if (shared_table.entries[i].data_ptr) {
            ptk_local_free(&shared_table.entries[i].data_ptr);
        }
    }
    
    ptk_local_free(&shared_table.entries);
    shared_table_initialized = false;
    
    debug("Shared table shut down");
    return PTK_OK;
}

ptk_shared_handle_t ptk_shared_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr)) {
    if (!shared_table_initialized) {
        if (init_shared_table() != PTK_OK) {
            return PTK_SHARED_INVALID_HANDLE;
        }
    }
    
    void *ptr = ptk_local_alloc_impl(file, line, size, destructor);
    if (!ptr) {
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    // Find free slot
    size_t entry_index = 0;
    for (size_t i = 0; i < shared_table.capacity; i++) {
        if (!shared_table.entries[i].data_ptr) {
            entry_index = i;
            break;
        }
    }
    
    if (entry_index == shared_table.capacity) {
        error("Shared table full");
        ptk_local_free(&ptr);
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    shared_entry_t *entry = &shared_table.entries[entry_index];
    entry->handle_value = ((uintptr_t)shared_table.next_generation << 32) | entry_index;
    entry->data_ptr = ptr;
    entry->ref_count = 1;
    entry->file = file;
    entry->line = line;
    pthread_mutex_init(&entry->mutex, NULL);

    shared_table.next_generation++;
    shared_table.count++;

    debug("Shared memory %p with handle 0x%016" PRIx64 " at index %zu from %s:%d", 
          ptr, entry->handle_value, entry_index, file, line);

    return (ptk_shared_handle_t){entry->handle_value};
}

void *ptk_shared_acquire_impl(const char *file, int line, ptk_shared_handle_t handle, ptk_time_ms timeout) {
    if (!ptk_shared_is_valid(handle)) {
        error("Attempt to acquire invalid handle");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    if (!shared_table_initialized) {
        error("Shared table not initialized");
        ptk_set_err(PTK_ERR_BAD_INTERNAL_STATE);
        return NULL;
    }

    size_t index = handle.value & 0xFFFFFFFF;
    if (index >= shared_table.capacity) {
        error("Invalid handle index %zu", index);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    shared_entry_t *entry = &shared_table.entries[index];
    if (entry->handle_value != handle.value) {
        error("Stale handle 0x%016" PRIx64 " != 0x%016" PRIx64, handle.value, entry->handle_value);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    int lock_result = 0;
    if (timeout == PTK_TIME_NO_WAIT) {
        lock_result = pthread_mutex_trylock(&entry->mutex);
    } else if (timeout == PTK_TIME_WAIT_FOREVER) {
        lock_result = pthread_mutex_lock(&entry->mutex);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout / 1000;
        ts.tv_nsec += (timeout % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += ts.tv_nsec / 1000000000;
            ts.tv_nsec = ts.tv_nsec % 1000000000;
        }
        lock_result = pthread_mutex_timedlock(&entry->mutex, &ts);
    }
    if (lock_result != 0) {
        ptk_set_err(PTK_ERR_TIMEOUT);
        return NULL;
    }

    if (!entry->data_ptr) {
        pthread_mutex_unlock(&entry->mutex);
        ptk_set_err(PTK_ERR_TIMEOUT);
        return NULL;
    }

    if (entry->ref_count == UINT32_MAX) {
        pthread_mutex_unlock(&entry->mutex);
        error("Reference count overflow at %s:%d", file, line);
        ptk_set_err(PTK_ERR_OVERFLOW);
        return NULL;
    }

    entry->ref_count++;

    debug("Acquired shared memory %p (ref_count=%u) from %s:%d", 
          entry->data_ptr, entry->ref_count, file, line);

    pthread_mutex_unlock(&entry->mutex);
    ptk_set_err(PTK_OK);
    return entry->data_ptr;
}

ptk_err_t ptk_shared_release_impl(const char *file, int line, ptk_shared_handle_t handle) {
    if (!ptk_shared_is_valid(handle)) {
        error("Attempt to release invalid handle");
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (!shared_table_initialized) {
        error("Shared table not initialized");
        return PTK_ERR_BAD_INTERNAL_STATE;
    }
    
    size_t index = handle.value & 0xFFFFFFFF;
    if (index >= shared_table.capacity) {
        error("Invalid handle index %zu", index);
        return PTK_ERR_INVALID_PARAM;
    }
    
    shared_entry_t *entry = &shared_table.entries[index];
    if (entry->handle_value != handle.value) {
        error("Stale handle 0x%016" PRIx64 " != 0x%016" PRIx64, handle.value, entry->handle_value);
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (entry->ref_count == 0) {
        error("Attempt to release handle with zero ref count");
        return PTK_ERR_BAD_INTERNAL_STATE;
    }
    
    entry->ref_count--;
    
    debug("Released shared memory %p (ref_count=%u) from %s:%d", 
          entry->data_ptr, entry->ref_count, file, line);
    
    if (entry->ref_count == 0) {
        ptk_local_free(&entry->data_ptr);
        pthread_mutex_destroy(&entry->mutex);
        entry->handle_value = 0;
        entry->file = NULL;
        entry->line = 0;
        shared_table.count--;
        debug("Freed shared memory entry at index %zu", index);
    }
    
    return PTK_OK;
}

ptk_err_t ptk_shared_realloc_impl(const char *file, int line, ptk_shared_handle_t handle, size_t new_size) {
    if (!ptk_shared_is_valid(handle)) {
        error("Attempt to realloc invalid handle");
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (!shared_table_initialized) {
        error("Shared table not initialized");
        return PTK_ERR_BAD_INTERNAL_STATE;
    }
    
    size_t index = handle.value & 0xFFFFFFFF;
    if (index >= shared_table.capacity) {
        error("Invalid handle index %zu", index);
        return PTK_ERR_INVALID_PARAM;
    }
    
    shared_entry_t *entry = &shared_table.entries[index];
    if (entry->handle_value != handle.value) {
        error("Stale handle 0x%016" PRIx64 " != 0x%016" PRIx64, handle.value, entry->handle_value);
        return PTK_ERR_INVALID_PARAM;
    }
    
    void *new_ptr = ptk_local_realloc_impl(file, line, entry->data_ptr, new_size);
    if (!new_ptr) {
        return PTK_ERR_NO_RESOURCES;
    }
    
    entry->data_ptr = new_ptr;
    
    debug("Reallocated shared memory to %zu bytes at %p from %s:%d", 
          new_size, new_ptr, file, line);
    
    return PTK_OK;
}

void ptk_shared_free_impl(const char *file, int line, void **ptr_ref) {
    if (!ptr_ref || !*ptr_ref) {
        return;
    }
    void *ptr = *ptr_ref;
    if (!shared_table_initialized) {
        error("Shared table not initialized");
        return;
    }
    // Find entry in shared table
    for (size_t i = 0; i < shared_table.capacity; i++) {
        shared_entry_t *entry = &shared_table.entries[i];
        if (entry->data_ptr == ptr) {
            // Remove from table
            entry->data_ptr = NULL;
            pthread_mutex_destroy(&entry->mutex);
            entry->handle_value = 0;
            entry->ref_count = 0;
            entry->file = NULL;
            entry->line = 0;
            shared_table.count--;
            debug("Removed shared memory entry for %p at index %zu", ptr, i);
            break;
        }
    }
    ptk_local_free_impl(file, line, ptr_ref);
}