/**
 * @file ptk_mem.c
 * @brief Implementation of unified memory management for Protocol Toolkit
 *
 * See code_creation_guidelines.md for style and documentation rules.
 */

#include <ptk_mem.h>
#include <ptk_alloc.h>
#include <ptk_shared.h>
#include <ptk_utils.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <stdlib.h>
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
    
    // Check if this looks like it could be a valid PTK allocation
    // First check: is the header canary correct?
    if(header->header_canary != PTK_ALLOC_HEADER_CANARY) {
        error("Invalid header canary detected at %p - expected 0x%llx, got 0x%llx", 
              user_ptr, (unsigned long long)PTK_ALLOC_HEADER_CANARY, 
              (unsigned long long)header->header_canary);
        error("This pointer was likely allocated with malloc() instead of ptk_local_alloc()");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return false;
    }
    
    // Check footer canary
    ptk_local_alloc_footer_t *footer = ptk_get_footer(header);
    if(footer->footer_canary != PTK_ALLOC_FOOTER_CANARY) {
        error("Invalid footer canary detected at %p - expected 0x%llx, got 0x%llx",
              user_ptr, (unsigned long long)PTK_ALLOC_FOOTER_CANARY,
              (unsigned long long)footer->footer_canary);
        error("Memory corruption detected or invalid pointer from %s:%d", 
              header->file, header->line);
        ptk_set_err(PTK_ERR_VALIDATION);
        return false;
    }
    
    return true;
}

void *ptk_local_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr)) {
    if(size == 0) {
        warn("called with zero size at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    size_t user_size = ptk_round_up_16(size);
    size_t total_size = sizeof(ptk_local_alloc_header_t) + user_size + sizeof(ptk_local_alloc_footer_t);

    void *raw = NULL;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    raw = aligned_alloc(PTK_ALLOC_ALIGNMENT, ptk_round_up_16(total_size));
#else
    raw = malloc(ptk_round_up_16(total_size));
#endif
    if(!raw) {
        warn("failed to allocate %zu bytes at %s:%d", total_size, file, line);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    // Initialize header with canary
    ptk_local_alloc_header_t *hdr = (ptk_local_alloc_header_t *)raw;
    hdr->header_canary = PTK_ALLOC_HEADER_CANARY;
    hdr->destructor = destructor;
    hdr->size = user_size;
    hdr->file = file;
    hdr->line = line;
    
    // Initialize user data area
    void *user_ptr = (uint8_t *)raw + sizeof(ptk_local_alloc_header_t);
    memset(user_ptr, 0, user_size);
    
    // Initialize footer with canary
    ptk_local_alloc_footer_t *footer = ptk_get_footer(hdr);
    footer->footer_canary = PTK_ALLOC_FOOTER_CANARY;
    
    debug("allocated %zu bytes at %p (user: %p) from %s:%d", total_size, raw, user_ptr, file, line);
    ptk_set_err(PTK_OK);
    return user_ptr;
}

void *ptk_local_realloc_impl(const char *file, int line, void *ptr, size_t new_size) {
    if(!ptr || new_size == 0) {
        warn("called with null pointer or zero size at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Validate canaries before proceeding
    if(!ptk_validate_canaries(ptr)) {
        error("canary validation failed for pointer %p at %s:%d", ptr, file, line);
        return NULL;
    }
    
    ptk_local_alloc_header_t *old_hdr = ptk_get_header(ptr);
    size_t old_size = old_hdr->size;
    size_t new_user_size = ptk_round_up_16(new_size);
    size_t new_total_size = sizeof(ptk_local_alloc_header_t) + new_user_size + sizeof(ptk_local_alloc_footer_t);

    void *raw = realloc(old_hdr, ptk_round_up_16(new_total_size));
    if(!raw) {
        warn("failed to reallocate to %zu bytes at %s:%d", new_total_size, file, line);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    ptk_local_alloc_header_t *new_hdr = (ptk_local_alloc_header_t *)raw;
    void *new_user_ptr = (uint8_t *)raw + sizeof(ptk_local_alloc_header_t);
    
    // Zero new memory if size increased
    if(new_user_size > old_size) { 
        memset((uint8_t *)new_user_ptr + old_size, 0, new_user_size - old_size); 
    }
    
    // Update header (canary should still be intact)
    new_hdr->size = new_user_size;
    new_hdr->file = file;
    new_hdr->line = line;
    
    // Update footer canary
    ptk_local_alloc_footer_t *new_footer = ptk_get_footer(new_hdr);
    new_footer->footer_canary = PTK_ALLOC_FOOTER_CANARY;
    
    debug("reallocated to %zu bytes at %p (user: %p) from %s:%d", new_total_size, raw, new_user_ptr, file, line);
    ptk_set_err(PTK_OK);
    return new_user_ptr;
}

void ptk_local_free_impl(const char *file, int line, void **ptr_ref) {
    if(!ptr_ref) {
        warn("called with null pointer reference at %s:%d", file, line);
        return;
    }
    void *ptr = *ptr_ref;
    if(!ptr) {
        debug("called with null pointer at %s:%d", file, line);
        return;
    }
    
    // Validate canaries before proceeding with free
    if(!ptk_validate_canaries(ptr)) {
        error("canary validation failed for pointer %p at %s:%d", ptr, file, line);
        error("refusing to free potentially invalid pointer");
        // Do not set ptk_set_err here as ptk_validate_canaries already did
        return;
    }
    
    ptk_local_alloc_header_t *hdr = ptk_get_header(ptr);
    debug("freeing memory at %p (user: %p) allocated from %s:%d", 
          hdr, ptr, hdr->file, hdr->line);
    
    // Call destructor if present
    if(hdr->destructor) {
        debug("calling destructor for memory at %p", ptr);
        hdr->destructor(ptr);
    }
    
    // Clear canaries to detect double-free
    hdr->header_canary = 0xDEADDEADDEADDEADULL;  // Invalid canary
    ptk_local_alloc_footer_t *footer = ptk_get_footer(hdr);
    footer->footer_canary = 0xDEADDEADDEADDEADULL;  // Invalid canary
    
    // Free the raw allocation
    free(hdr);
    *ptr_ref = NULL;
    ptk_set_err(PTK_OK);
}



//==============================================================================
// SHARED MEMORY API
//==============================================================================

#define HANDLE_INDEX_MASK       0xFFFFFFFF
#define HANDLE_GENERATION_SHIFT 32
#define INITIAL_TABLE_SIZE      1024

typedef struct {
    uint64_t handle_value;      // Combined generation + index
    void *data_ptr;             // Points to ptk_alloc'd memory (NULL = free slot)
    uint32_t ref_count;         // Reference counter (protected by mutex)
    ptk_mutex *mutex;           // Per-entry mutex
    const char *file;           // Debug info from ptk_shared_create macro
    int line;                   // Debug info from ptk_shared_create macro
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

ptk_shared_handle_t ptk_shared_create_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr)) {
    if (size == 0) {
        error("Cannot create shared memory segment of size 0 at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_SHARED_INVALID_HANDLE;
    }

    void *ptr = ptk_alloc(size, destructor);
    if (!ptr) {
        error("Failed to allocate shared memory at %s:%d", file, line);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_SHARED_INVALID_HANDLE;
    }

    ptk_err lock_result = ptk_mutex_wait_lock(shared_table.table_mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        error("Failed to lock shared table mutex");
        ptk_set_err(lock_result);
        return PTK_SHARED_INVALID_HANDLE;
    }

    // Find free slot or expand table
    shared_entry_t *entry = find_free_slot_or_expand();
    if (!entry) {
        ptk_mutex_unlock(shared_table.table_mutex);
        error("Failed to find free slot for data at %s:%d", file, line);
        ptk_free(&ptr);  // Clean up allocated memory on failure
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
    
    debug("Shared memory %p with handle 0x%016" PRIx64 " at index %zu from %s:%d", 
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
        ptk_set_err(PTK_ERR_BAD_INTERNAL_STATE);
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
        ptk_set_err(PTK_ERR_BAD_INTERNAL_STATE);
        return NULL;
    }

    // Check for overflow
    if (entry->ref_count == UINT32_MAX) {
        ptk_mutex_unlock(entry->mutex);
        error("Reference count overflow at %s:%d", entry->file, entry->line);
        ptk_set_err(PTK_ERR_BAD_INTERNAL_STATE);
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
        return PTK_ERR_BAD_INTERNAL_STATE;
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
        return PTK_ERR_BAD_INTERNAL_STATE;
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
        return PTK_ERR_BAD_INTERNAL_STATE;
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

void ptk_shared_free_impl(const char *file, int line, ptk_shared_handle_t *handle) {
    if (!handle || !PTK_SHARED_IS_VALID(*handle)) {
        warn("ptk_shared_free: called with null or invalid handle at %s:%d", file, line);
        return;
    }

    if (!shared_table_initialized) {
        error("ptk_shared_free: shared table not initialized");
        return;
    }

    ptk_err lock_result = ptk_mutex_wait_lock(shared_table.table_mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        error("ptk_shared_free: failed to lock shared table mutex");
        return;
    }

    shared_entry_t *entry = lookup_entry_unsafe(*handle);
    if (!entry) {
        ptk_mutex_unlock(shared_table.table_mutex);
        warn("ptk_shared_free: invalid handle 0x%016" PRIx64 " at %s:%d", handle->value, file, line);
        return;
    }

    lock_result = ptk_mutex_wait_lock(entry->mutex, PTK_TIME_WAIT_FOREVER);
    if (lock_result != PTK_OK) {
        ptk_mutex_unlock(shared_table.table_mutex);
        error("ptk_shared_free: failed to lock entry mutex");
        return;
    }

    if (entry->ref_count == 0) {
        // Already freed or double free
        ptk_mutex_unlock(entry->mutex);
        ptk_mutex_unlock(shared_table.table_mutex);
        warn("ptk_shared_free: double free detected for handle 0x%016" PRIx64 " at %s:%d", handle->value, file, line);
        handle->value = 0;
        return;
    }

    entry->ref_count--;
    trace("ptk_shared_free: decremented ref count for handle 0x%016" PRIx64 " to %u at %s:%d", handle->value, entry->ref_count, file, line);

    if (entry->ref_count == 0) {
        void *ptr = entry->data_ptr;
        entry->data_ptr = NULL;
        entry->handle_value = 0;
        entry->file = NULL;
        entry->line = 0;
        shared_table.count--;
        handle->value = 0;

        ptk_mutex_unlock(entry->mutex);

        ptk_local_free_impl(file, line, &ptr);  // Call destructor if needed

        ptk_mutex_unlock(shared_table.table_mutex);

        ptk_set_err(PTK_OK);
        return;
    }

    ptk_mutex_unlock(entry->mutex);
    ptk_mutex_unlock(shared_table.table_mutex);
    ptk_set_err(PTK_OK);
}


//==============================================================================
// MODULE FUNCTIONS
//==============================================================================

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

