# PTK Shared Memory Design

This document describes the shared memory system for the Protocol Toolkit library, designed to allow safe sharing of ptk_alloc'd memory across multiple threads.

## Overview

The shared memory system provides:
- Thread-safe sharing of existing ptk_alloc'd memory
- Reference counting with automatic cleanup
- Protection against use-after-free via generation counters
- Integration with existing ptk_alloc destructor system

## Core Design Principles

1. **Wrap Existing Memory**: Only ptk_alloc'd memory can be shared
2. **Leverage Existing Destructors**: Use ptk_alloc's destructor system
3. **Two-Level Mutex Protection**: Global table mutex + per-entry mutex
4. **Simple Clearing**: Mark slots available by setting data_ptr to NULL
5. **Generation Counter Wraparound**: Allow 32-bit generation counter overflow

## Data Structures

### Handle Format
```c
typedef struct ptk_shared_handle {
    uint64_t value;  // [32-bit generation] [32-bit index]
} ptk_shared_handle_t;

#define HANDLE_INDEX_MASK    0xFFFFFFFF
#define HANDLE_GENERATION_SHIFT 32
```

### Table Entry
```c
typedef struct {
    uint64_t handle_value;      // Combined generation + index
    void *data_ptr;             // Points to ptk_alloc'd memory (NULL = free slot)
    uint32_t ref_count;         // Reference counter (protected by mutex)
    ptk_mutex *mutex;           // Per-entry mutex
    const char *file;           // Debug info from ptk_shared_wrap macro
    int line;                   // Debug info from ptk_shared_wrap macro
} shared_entry_t;
```

### Global Table
```c
typedef struct {
    shared_entry_t *entries;    // Array of entries (NOT pointers)
    size_t capacity;            // Total table size
    size_t count;               // Entries in use
    uint32_t next_generation;   // Global generation counter
    ptk_mutex *table_mutex;     // Global table protection
} shared_table_t;
```

## API Functions

### Core API
```c
// Macro captures file/line for debugging
#define ptk_shared_wrap(ptr) ptk_shared_wrap_impl(__FILE__, __LINE__, ptr)

// Implementation functions
ptk_shared_handle_t ptk_shared_wrap_impl(const char *file, int line, void *ptr);
void *ptk_shared_acquire(ptk_shared_handle_t handle);
ptk_err ptk_shared_release(ptk_shared_handle_t handle);
ptk_err ptk_shared_realloc(ptk_shared_handle_t handle, size_t new_size);
```

### Type-Safe Wrappers
```c
#define PTK_SHARED_DECLARE_TYPE(type_name, type) \
    typedef struct { \
        ptk_shared_handle_t handle; \
    } ptk_shared_##type_name##_t; \
    \
    static inline ptk_shared_##type_name##_t ptk_shared_##type_name##_wrap(type *ptr) { \
        ptk_shared_handle_t h = ptk_shared_wrap(ptr); \
        return (ptk_shared_##type_name##_t){h}; \
    }
```

## Implementation Details

### Mutex Ordering (Deadlock Prevention)
All functions follow strict lock ordering:
1. Always acquire global table mutex first
2. Then acquire entry mutex
3. Release table mutex quickly (hold only during lookup)
4. Keep entry mutex locked across acquire/release pairs

### Handle Lookup
```c
static shared_entry_t *lookup_entry_unsafe(ptk_shared_handle_t handle) {
    uint32_t index = handle.value & HANDLE_INDEX_MASK;
    if (index >= shared_table.capacity) return NULL;
    
    shared_entry_t *entry = &shared_table.entries[index];
    if (entry->handle_value != handle.value || entry->data_ptr == NULL) {
        return NULL;  // Invalid generation or cleared entry
    }
    return entry;
}
```

### Memory Wrapping
```c
ptk_shared_handle_t ptk_shared_wrap_impl(const char *file, int line, void *ptr) {
    ptk_mutex_wait_lock(global_table_mutex, PTK_TIME_WAIT_FOREVER);
    
    // Check for double-wrapping (error condition)
    for (size_t i = 0; i < shared_table.count; i++) {
        if (shared_table.entries[i].data_ptr == ptr) {
            ptk_mutex_unlock(global_table_mutex);
            error("Pointer %p already wrapped at %s:%d", ptr, file, line);
            ptk_set_err(PTK_ERR_ALREADY_EXISTS);
            return PTK_SHARED_INVALID_HANDLE;
        }
    }
    
    // Find free slot or expand table
    shared_entry_t *entry = find_free_slot_or_expand();
    
    // Initialize entry
    entry->handle_value = create_new_handle(entry_index);
    entry->data_ptr = ptr;
    entry->ref_count = 1;  // Initial reference
    entry->file = file;
    entry->line = line;
    
    ptk_mutex_unlock(global_table_mutex);
    return (ptk_shared_handle_t){entry->handle_value};
}
```

### Memory Acquisition
```c
void *ptk_shared_acquire(ptk_shared_handle_t handle) {
    ptk_mutex_wait_lock(global_table_mutex, PTK_TIME_WAIT_FOREVER);
    
    shared_entry_t *entry = lookup_entry_unsafe(handle);
    if (!entry) {
        ptk_mutex_unlock(global_table_mutex);
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    ptk_mutex_wait_lock(entry->mutex, PTK_TIME_WAIT_FOREVER);
    ptk_mutex_unlock(global_table_mutex);  // Release table, keep entry locked
    
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
    return entry->data_ptr;  // Keep entry mutex locked for caller
}
```

### Memory Release
```c
ptk_err ptk_shared_release(ptk_shared_handle_t handle) {
    ptk_mutex_wait_lock(global_table_mutex, PTK_TIME_WAIT_FOREVER);
    
    shared_entry_t *entry = lookup_entry_unsafe(handle);
    if (!entry) {
        ptk_mutex_unlock(global_table_mutex);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Entry mutex assumed locked from acquire
    
    // Check for underflow (double release)
    if (entry->ref_count == 0) {
        error("Double release detected at %s:%d", entry->file, entry->line);
        entry->data_ptr = NULL;  // Mark slot as free
        ptk_mutex_unlock(global_table_mutex);
        ptk_mutex_unlock(entry->mutex);
        return PTK_ERR_INVALID_STATE;
    }
    
    entry->ref_count--;
    
    if (entry->ref_count == 0) {
        void *temp_ptr = entry->data_ptr;
        entry->data_ptr = NULL;  // Mark slot as free
        
        ptk_mutex_unlock(global_table_mutex);
        ptk_mutex_unlock(entry->mutex);
        
        ptk_free(&temp_ptr);  // Calls original destructor
    } else {
        ptk_mutex_unlock(global_table_mutex);
        ptk_mutex_unlock(entry->mutex);
    }
    
    return PTK_OK;
}
```

### Safe Reallocation
```c
ptk_err ptk_shared_realloc(ptk_shared_handle_t handle, size_t new_size) {
    // Use existing functions for safety
    void *ptr = ptk_shared_acquire(handle);
    if (!ptr) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Get entry to update it
    shared_entry_t *entry = lookup_entry_unsafe(handle);
    if (!entry) {
        ptk_shared_release(handle);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Realloc using existing ptk_realloc (handles temp pointer safely)
    void *temp = ptk_realloc(entry->data_ptr, new_size);
    if (!temp) {
        ptk_shared_release(handle);
        return PTK_ERR_OUT_OF_MEMORY;
    }
    
    entry->data_ptr = temp;  // Update table entry
    
    ptk_shared_release(handle);
    return PTK_OK;
}
```

## Error Conditions

### Double Wrapping
- **Detection**: Check all entries during wrap operation
- **Response**: Return PTK_ERR_ALREADY_EXISTS
- **Logging**: Error level with file/line

### Reference Count Overflow
- **Detection**: Check ref_count == UINT32_MAX before increment
- **Response**: Return PTK_ERR_OVERFLOW, don't increment
- **Logging**: Error level with creation location

### Reference Count Underflow
- **Detection**: Check ref_count == 0 during release
- **Response**: Return PTK_ERR_INVALID_STATE, clear entry
- **Logging**: Error level indicating double release

### Zombie Entries
- **Detection**: Check ref_count == 0 during acquire
- **Response**: Return PTK_ERR_INVALID_STATE
- **Cause**: Entry decremented to 0 but not properly cleaned up

## Table Management

### Initial Size
- Start with 1024 entries
- All entry mutexes pre-initialized

### Growth Strategy
```c
static ptk_err expand_shared_table(void) {
    size_t old_capacity = shared_table.capacity;
    size_t new_capacity = old_capacity * 2;
    
    shared_entry_t *new_entries = ptk_realloc(shared_table.entries, 
                                             new_capacity * sizeof(shared_entry_t));
    if (!new_entries) return PTK_ERR_OUT_OF_MEMORY;
    
    // Initialize mutexes for new entries
    for (size_t i = old_capacity; i < new_capacity; i++) {
        new_entries[i].mutex = ptk_mutex_create();
        new_entries[i].data_ptr = NULL;  // Mark as free
        new_entries[i].handle_value = 0;
    }
    
    shared_table.entries = new_entries;
    shared_table.capacity = new_capacity;
    return PTK_OK;
}
```

### Free Slot Management
- **Free Indication**: entry->data_ptr == NULL
- **Reuse**: Linear search for free slots (acceptable for current scale)
- **No Shrinking**: Table only grows, never shrinks

## Integration with PTK Startup/Shutdown

### Startup (ptk_startup())
```c
ptk_err ptk_startup(void) {
    // Initialize shared memory system
    if (!shared_table_initialized) {
        shared_table.table_mutex = ptk_mutex_create();
        shared_table.entries = ptk_alloc(1024 * sizeof(shared_entry_t), NULL);
        shared_table.capacity = 1024;
        shared_table.count = 0;
        shared_table.next_generation = 1;
        
        // Initialize all entry mutexes
        for (size_t i = 0; i < 1024; i++) {
            shared_table.entries[i].mutex = ptk_mutex_create();
            shared_table.entries[i].data_ptr = NULL;
        }
        
        shared_table_initialized = true;
    }
    return PTK_OK;
}
```

### Shutdown (ptk_shutdown())
```c
ptk_err ptk_shutdown(void) {
    if (shared_table_initialized) {
        // Force cleanup of all remaining entries
        for (size_t i = 0; i < shared_table.capacity; i++) {
            shared_entry_t *entry = &shared_table.entries[i];
            if (entry->data_ptr) {
                error("Leaked shared memory at %s:%d during shutdown", 
                      entry->file, entry->line);
                ptk_free(&entry->data_ptr);
            }
            ptk_mutex_destroy(entry->mutex);
        }
        
        ptk_free(&shared_table.entries);
        ptk_mutex_destroy(shared_table.table_mutex);
        shared_table_initialized = false;
    }
    return PTK_OK;
}
```

## Usage Examples

### Basic Usage
```c
// Create and wrap memory
threadlet_t *t = ptk_alloc(sizeof(threadlet_t), threadlet_destructor);
ptk_shared_handle_t handle = ptk_shared_wrap(t);

// Thread 1: Use the memory
threadlet_t *ptr1 = ptk_shared_acquire(handle);  // ref_count = 2, mutex locked
// ... use ptr1 ...
ptk_shared_release(handle);  // ref_count = 1, mutex unlocked

// Thread 2: Use the memory
threadlet_t *ptr2 = ptk_shared_acquire(handle);  // ref_count = 2, mutex locked
// ... use ptr2 ...
ptk_shared_release(handle);  // ref_count = 1, mutex unlocked

// Final release
ptk_shared_release(handle);  // ref_count = 0, calls threadlet_destructor, frees memory
```

### Type-Safe Usage
```c
PTK_SHARED_DECLARE_TYPE(threadlet, threadlet_t);

threadlet_t *t = ptk_alloc(sizeof(threadlet_t), threadlet_destructor);
ptk_shared_threadlet_t shared_t = ptk_shared_threadlet_wrap(t);

threadlet_t *ptr = ptk_shared_threadlet_acquire(shared_t);
// ... use ptr ...
ptk_shared_threadlet_release(shared_t);
```

## Performance Characteristics

### Memory Usage
- **Entry Overhead**: ~32 bytes per shared pointer
- **Table Growth**: Doubles when full (1K → 2K → 4K → 8K...)
- **No Fragmentation**: Entries reused when freed

### Lock Contention
- **Table Mutex**: Held briefly during lookup only
- **Entry Mutex**: Held across acquire/release pairs
- **Scalability**: Per-entry locks reduce contention

### Generation Counter
- **32-bit Range**: 4.2 billion reuses per slot
- **Wraparound Safe**: Assumed no rapid reuse of same index
- **Stale Handle Protection**: Mismatched generation = invalid access

This design provides thread-safe sharing of ptk_alloc'd memory with strong safety guarantees and integration with the existing PTK infrastructure.