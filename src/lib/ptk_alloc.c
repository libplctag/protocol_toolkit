#include "ptk_alloc.h"
#include "ptk_log.h"
#include <stdlib.h>
#include <string.h>

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static void *debug_alloc(ptk_allocator_t *allocator, size_t size, ptk_destructor_fn destructor);
static void debug_free(ptk_allocator_t *allocator, void *ptr);
static void *arena_alloc(ptk_allocator_t *allocator, size_t size, ptk_destructor_fn destructor);
static void arena_free(ptk_allocator_t *allocator, void *ptr);

//=============================================================================
// ALLOCATOR TYPE IDENTIFICATION
//=============================================================================

typedef enum { PTK_ALLOCATOR_DEFAULT, PTK_ALLOCATOR_DEBUG, PTK_ALLOCATOR_ARENA } ptk_allocator_type_t;

//=============================================================================
// SYSTEM/DEFAULT ALLOCATOR
//=============================================================================

typedef struct {
    ptk_allocator_t base;
    ptk_allocator_type_t type;
} default_allocator_t;

static void *default_alloc(ptk_allocator_t *allocator, size_t size, ptk_destructor_fn destructor) {
    (void)destructor;  // Default allocator doesn't support destructors
    size_t aligned_size = PTK_ALIGN_SIZE(size, allocator->default_alignment);
    void *ptr = malloc(aligned_size);

    if(!ptr) {
        error("Failed to allocate %zu bytes (aligned to %zu)", aligned_size, allocator->default_alignment);
    } else {
        trace("Allocated %zu bytes at %p", aligned_size, ptr);
    }

    return ptr;
}

static void *default_realloc(ptk_allocator_t *allocator, void *ptr, size_t new_size) {
    size_t aligned_size = PTK_ALIGN_SIZE(new_size, allocator->default_alignment);
    void *new_ptr = realloc(ptr, aligned_size);

    if(!new_ptr && aligned_size > 0) {
        error("Failed to reallocate %p to %zu bytes (aligned to %zu)", ptr, aligned_size, allocator->default_alignment);
    } else {
        trace("Reallocated %p to %p (%zu bytes)", ptr, new_ptr, aligned_size);
    }

    return new_ptr;
}

static void default_free(ptk_allocator_t *allocator, void *ptr) {
    (void)allocator;  // Unused
    if(ptr) {
        trace("Freeing %p", ptr);
        free(ptr);
    }
}

static void default_reset(ptk_allocator_t *allocator) {
    (void)allocator;  // No-op for system allocator
    debug("Reset called on default allocator (no-op)");
}

static void default_get_stats(ptk_allocator_t *allocator, ptk_alloc_stats_t *stats) {
    (void)allocator;  // Unused
    if(stats) { memset(stats, 0, sizeof(*stats)); }
}

static void default_destroy(ptk_allocator_t *allocator) {
    if(!allocator) { return; }

    debug("Destroying default allocator");
    free(allocator);
}

ptk_allocator_t *allocator_default_create(size_t default_alignment) {
    if(default_alignment == 0) { default_alignment = sizeof(void *); }

    default_allocator_t *alloc = malloc(sizeof(default_allocator_t));
    if(!alloc) {
        error("Failed to allocate default allocator");
        return NULL;
    }

    alloc->base.alloc = default_alloc;
    alloc->base.realloc = default_realloc;
    alloc->base.free = default_free;
    alloc->base.reset = default_reset;
    alloc->base.get_stats = default_get_stats;
    alloc->base.destroy = default_destroy;
    alloc->base.default_alignment = default_alignment;
    alloc->type = PTK_ALLOCATOR_DEFAULT;

    debug("Created default allocator with %zu-byte alignment", default_alignment);
    return &alloc->base;
}

//=============================================================================
// DEBUG ALLOCATOR (with memory tracking and deferred freeing)
//=============================================================================

typedef struct debug_alloc_entry {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    bool freed;
    struct debug_alloc_entry *next;
} debug_alloc_entry_t;

typedef struct {
    ptk_allocator_t base;
    ptk_allocator_type_t type;
    debug_alloc_entry_t *allocations;
    ptk_alloc_stats_t stats;
} debug_allocator_t;

static debug_alloc_entry_t *debug_find_allocation(debug_allocator_t *debug_alloc, void *ptr) {
    debug_alloc_entry_t *current = debug_alloc->allocations;
    while(current) {
        if(current->ptr == ptr) { return current; }
        current = current->next;
    }
    return NULL;
}

static void debug_add_allocation(debug_allocator_t *debug_alloc, void *ptr, size_t size) {
    debug_alloc_entry_t *entry = malloc(sizeof(debug_alloc_entry_t));
    if(!entry) {
        error("Failed to allocate debug tracking entry for %p", ptr);
        return;  // Can't track this allocation
    }

    entry->ptr = ptr;
    entry->size = size;
    entry->file = __FILE__;  // Could be enhanced with macro magic
    entry->line = __LINE__;
    entry->freed = false;
    entry->next = debug_alloc->allocations;
    debug_alloc->allocations = entry;

    debug_alloc->stats.total_allocated += size;
    debug_alloc->stats.total_allocations++;
    debug_alloc->stats.active_allocations++;
    debug_alloc->stats.total_bytes_allocated += size;

    if(debug_alloc->stats.total_allocated > debug_alloc->stats.peak_allocated) {
        debug_alloc->stats.peak_allocated = debug_alloc->stats.total_allocated;
    }

    trace("Tracked allocation %p (%zu bytes)", ptr, size);
}

static void debug_mark_freed(debug_allocator_t *debug_alloc, void *ptr) {
    debug_alloc_entry_t *entry = debug_find_allocation(debug_alloc, ptr);

    if(!entry) {
        warn("Attempt to free untracked pointer %p", ptr);
        return;
    }

    if(entry->freed) {
        error("Double free detected for pointer %p (originally allocated at %s:%d)", ptr, entry->file ? entry->file : "unknown",
              entry->line);
        return;
    }

    entry->freed = true;
    debug_alloc->stats.total_frees++;
    debug_alloc->stats.total_bytes_freed += entry->size;

    trace("Marked allocation %p as freed (%zu bytes)", ptr, entry->size);
}

static void *debug_alloc(ptk_allocator_t *allocator, size_t size, ptk_destructor_fn destructor) {
    (void)destructor;  // Debug allocator doesn't support destructors yet
    debug_allocator_t *debug_alloc = (debug_allocator_t *)allocator;
    size_t aligned_size = PTK_ALIGN_SIZE(size, allocator->default_alignment);

    void *ptr = malloc(aligned_size);
    if(!ptr) {
        error("Failed to allocate %zu bytes (aligned to %zu)", aligned_size, allocator->default_alignment);
        return NULL;
    }

    debug_add_allocation(debug_alloc, ptr, aligned_size);
    debug("Debug allocated %zu bytes at %p", aligned_size, ptr);
    return ptr;
}

static void *debug_realloc(ptk_allocator_t *allocator, void *ptr, size_t new_size) {
    debug_allocator_t *debug_allocator = (debug_allocator_t *)allocator;
    size_t aligned_size = PTK_ALIGN_SIZE(new_size, allocator->default_alignment);

    if(!ptr) { return debug_alloc(allocator, new_size, NULL); }

    if(new_size == 0) {
        debug_free(allocator, ptr);
        return NULL;
    }

    debug_alloc_entry_t *entry = debug_find_allocation(debug_allocator, ptr);
    if(!entry) {
        warn("Realloc called on untracked pointer %p", ptr);
    } else if(entry->freed) {
        error("Use-after-free detected: realloc on freed pointer %p", ptr);
        return NULL;
    }

    void *new_ptr = realloc(ptr, aligned_size);
    if(!new_ptr) {
        error("Failed to reallocate %p to %zu bytes", ptr, aligned_size);
        return NULL;
    }

    // Update tracking if pointer changed
    if(new_ptr != ptr && entry) {
        entry->ptr = new_ptr;
        debug_allocator->stats.total_allocated -= entry->size;
        debug_allocator->stats.total_allocated += aligned_size;
        debug_allocator->stats.total_bytes_allocated += aligned_size;
        entry->size = aligned_size;

        if(debug_allocator->stats.total_allocated > debug_allocator->stats.peak_allocated) {
            debug_allocator->stats.peak_allocated = debug_allocator->stats.total_allocated;
        }
    }

    debug("Debug reallocated %p to %p (%zu bytes)", ptr, new_ptr, aligned_size);
    return new_ptr;
}

static void debug_free(ptk_allocator_t *allocator, void *ptr) {
    debug_allocator_t *debug_alloc = (debug_allocator_t *)allocator;

    if(!ptr) { return; }

    debug_mark_freed(debug_alloc, ptr);
    // DON'T actually free the memory - just mark as freed for use-after-free detection
    debug("Debug free called on %p (memory not actually freed)", ptr);
}

static void debug_reset(ptk_allocator_t *allocator) {
    debug_allocator_t *debug_alloc = (debug_allocator_t *)allocator;

    debug("Debug allocator reset - freeing all tracked memory");

    // Actually free all tracked allocations
    debug_alloc_entry_t *current = debug_alloc->allocations;
    size_t freed_count = 0;
    size_t freed_bytes = 0;

    while(current) {
        debug_alloc_entry_t *next = current->next;

        if(!current->freed) { info("Cleaning up unfreed allocation %p (%zu bytes)", current->ptr, current->size); }

        free(current->ptr);
        freed_bytes += current->size;
        freed_count++;
        free(current);
        current = next;
    }

    debug_alloc->allocations = NULL;
    memset(&debug_alloc->stats, 0, sizeof(debug_alloc->stats));

    info("Debug allocator reset complete: freed %zu allocations totaling %zu bytes", freed_count, freed_bytes);
}

static void debug_get_stats(ptk_allocator_t *allocator, ptk_alloc_stats_t *stats) {
    debug_allocator_t *debug_alloc = (debug_allocator_t *)allocator;
    if(stats) { *stats = debug_alloc->stats; }
}

static void debug_destroy(ptk_allocator_t *allocator) {
    if(!allocator) { return; }

    debug("Destroying debug allocator");
    debug_allocator_t *debug_alloc = (debug_allocator_t *)allocator;
    debug_reset(allocator);  // Clean up all tracked allocations
    free(debug_alloc);
}

ptk_allocator_t *allocator_debug_create(size_t default_alignment) {
    if(default_alignment == 0) { default_alignment = sizeof(void *); }

    debug_allocator_t *alloc = malloc(sizeof(debug_allocator_t));
    if(!alloc) {
        error("Failed to allocate debug allocator");
        return NULL;
    }

    alloc->base.alloc = debug_alloc;
    alloc->base.realloc = debug_realloc;
    alloc->base.free = debug_free;
    alloc->base.reset = debug_reset;
    alloc->base.get_stats = debug_get_stats;
    alloc->base.destroy = debug_destroy;
    alloc->base.default_alignment = default_alignment;
    alloc->type = PTK_ALLOCATOR_DEBUG;
    alloc->allocations = NULL;
    memset(&alloc->stats, 0, sizeof(alloc->stats));

    debug("Created debug allocator with %zu-byte alignment", default_alignment);
    return &alloc->base;
}

void ptk_debug_allocator_report(const ptk_allocator_t *allocator) {
    if(!allocator) {
        warn("Debug allocator report called with NULL allocator");
        return;
    }

    debug_allocator_t *debug_alloc = (debug_allocator_t *)allocator;
    if(debug_alloc->type != PTK_ALLOCATOR_DEBUG) {
        warn("Debug allocator report called on non-debug allocator");
        return;
    }

    info("=== DEBUG ALLOCATOR REPORT ===");
    info("Total allocated: %zu bytes", debug_alloc->stats.total_allocated);
    info("Peak allocated: %zu bytes", debug_alloc->stats.peak_allocated);
    info("Total allocations: %zu", debug_alloc->stats.total_allocations);
    info("Total frees: %zu", debug_alloc->stats.total_frees);
    info("Active allocations: %zu", debug_alloc->stats.active_allocations);
    info("Total bytes allocated (lifetime): %zu", debug_alloc->stats.total_bytes_allocated);
    info("Total bytes freed (lifetime): %zu", debug_alloc->stats.total_bytes_freed);

    // Report leaks and use-after-free issues
    debug_alloc_entry_t *current = debug_alloc->allocations;
    size_t leaks = 0;
    size_t leak_bytes = 0;
    size_t freed_but_not_cleaned = 0;

    while(current) {
        if(!current->freed) {
            leaks++;
            leak_bytes += current->size;
            warn("LEAK: %p (%zu bytes) allocated at %s:%d", current->ptr, current->size,
                 current->file ? current->file : "unknown", current->line);
        } else {
            freed_but_not_cleaned++;
        }
        current = current->next;
    }

    if(leaks > 0) {
        error("%zu memory leaks detected totaling %zu bytes", leaks, leak_bytes);
    } else {
        info("No memory leaks detected");
    }

    if(freed_but_not_cleaned > 0) { info("%zu freed allocations pending cleanup", freed_but_not_cleaned); }

    info("==============================");
}

bool ptk_debug_allocator_has_leaks(const ptk_allocator_t *allocator) {
    if(!allocator) { return false; }

    debug_allocator_t *debug_alloc = (debug_allocator_t *)allocator;
    if(debug_alloc->type != PTK_ALLOCATOR_DEBUG) { return false; }

    // Count unfreed allocations
    debug_alloc_entry_t *current = debug_alloc->allocations;
    while(current) {
        if(!current->freed) { return true; }
        current = current->next;
    }

    return false;
}

//=============================================================================
// ARENA ALLOCATOR (simple bump allocator with destructor support)
//=============================================================================

typedef struct arena_destructor_entry {
    void *ptr;
    ptk_destructor_fn destructor;
    struct arena_destructor_entry *next;
} arena_destructor_entry_t;

typedef struct {
    ptk_allocator_t base;
    ptk_allocator_type_t type;
    uint8_t *memory;
    size_t size;
    size_t offset;
    ptk_alloc_stats_t stats;
    arena_destructor_entry_t *destructor_list;
} arena_allocator_t;

static void *arena_alloc(ptk_allocator_t *allocator, size_t size, ptk_destructor_fn destructor) {
    arena_allocator_t *arena = (arena_allocator_t *)allocator;
    size_t aligned_size = PTK_ALIGN_SIZE(size, allocator->default_alignment);

    if(arena->offset + aligned_size > arena->size) {
        error("Arena allocator out of memory: requested %zu bytes, %zu available", aligned_size, arena->size - arena->offset);
        return NULL;
    }

    void *ptr = arena->memory + arena->offset;
    arena->offset += aligned_size;

    arena->stats.total_allocated += aligned_size;
    arena->stats.total_allocations++;
    arena->stats.active_allocations++;
    arena->stats.total_bytes_allocated += aligned_size;

    if(arena->stats.total_allocated > arena->stats.peak_allocated) { arena->stats.peak_allocated = arena->stats.total_allocated; }

    // If destructor provided, add to destructor list
    if(destructor) {
        // Allocate destructor entry from arena itself
        if(arena->offset + sizeof(arena_destructor_entry_t) <= arena->size) {
            arena_destructor_entry_t *entry = (arena_destructor_entry_t *)(arena->memory + arena->offset);
            arena->offset += PTK_ALIGN_SIZE(sizeof(arena_destructor_entry_t), allocator->default_alignment);

            entry->ptr = ptr;
            entry->destructor = destructor;
            entry->next = arena->destructor_list;
            arena->destructor_list = entry;

            trace("Added destructor for %p", ptr);
        } else {
            // If we can't allocate destructor entry, we can't guarantee cleanup
            error("Failed to allocate destructor entry for %p", ptr);
            // Return the allocation anyway, but no automatic cleanup
        }
    }

    trace("Arena allocated %zu bytes at %p (offset now %zu)", aligned_size, ptr, arena->offset);
    return ptr;
}

static void *arena_realloc(ptk_allocator_t *allocator, void *ptr, size_t new_size) {
    arena_allocator_t *arena = (arena_allocator_t *)allocator;

    if(!ptr) { return arena_alloc(allocator, new_size, NULL); }

    if(new_size == 0) {
        arena_free(allocator, ptr);
        return NULL;
    }

    // Simple arena realloc: allocate new memory and copy
    // We can't know the original size easily, so this is a limitation
    warn("Arena realloc is inefficient - consider avoiding realloc with arena allocators");

    void *new_ptr = arena_alloc(allocator, new_size, NULL);
    if(new_ptr && ptr) {
        // Copy what we can - this is unsafe but simple
        // In a real implementation, you'd track allocation sizes
        size_t copy_size = new_size;  // Assume worst case
        memcpy(new_ptr, ptr, copy_size);
    }

    return new_ptr;
}

static void arena_free(ptk_allocator_t *allocator, void *ptr) {
    arena_allocator_t *arena = (arena_allocator_t *)allocator;
    (void)ptr;  // Arena doesn't support individual frees

    if(ptr && arena->stats.active_allocations > 0) {
        arena->stats.total_frees++;
        arena->stats.active_allocations--;
        trace("Arena free called on %p (no-op)", ptr);
    }
}

static void arena_reset(ptk_allocator_t *allocator) {
    arena_allocator_t *arena = (arena_allocator_t *)allocator;

    debug("Arena allocator reset - reclaiming %zu bytes", arena->offset);

    arena->offset = 0;
    arena->stats.total_allocated = 0;
    arena->stats.active_allocations = 0;

    info("Arena reset: reclaimed %zu bytes", arena->size);
}

static void arena_get_stats(ptk_allocator_t *allocator, ptk_alloc_stats_t *stats) {
    arena_allocator_t *arena = (arena_allocator_t *)allocator;
    if(stats) { *stats = arena->stats; }
}

static void arena_destroy(ptk_allocator_t *allocator) {
    if(!allocator) { return; }

    debug("Destroying arena allocator");
    arena_allocator_t *arena = (arena_allocator_t *)allocator;

    // Call all destructors in reverse order (LIFO)
    arena_destructor_entry_t *entry = arena->destructor_list;
    while(entry) {
        if(entry->destructor && entry->ptr) {
            trace("Calling destructor for %p", entry->ptr);
            entry->destructor(entry->ptr);
        }
        entry = entry->next;
    }

    free(arena->memory);  // Free the arena memory pool
    free(arena);
}

ptk_allocator_t *allocator_arena_create(size_t pool_size, size_t default_alignment) {
    if(default_alignment == 0) { default_alignment = sizeof(void *); }

    if(pool_size == 0) {
        error("Arena allocator requires non-zero pool size");
        return NULL;
    }

    arena_allocator_t *alloc = malloc(sizeof(arena_allocator_t));
    if(!alloc) {
        error("Failed to allocate arena allocator structure");
        return NULL;
    }

    alloc->memory = malloc(pool_size);
    if(!alloc->memory) {
        error("Failed to allocate arena memory pool of %zu bytes", pool_size);
        free(alloc);
        return NULL;
    }

    alloc->base.alloc = arena_alloc;
    alloc->base.realloc = arena_realloc;
    alloc->base.free = arena_free;
    alloc->base.reset = arena_reset;
    alloc->base.get_stats = arena_get_stats;
    alloc->base.destroy = arena_destroy;
    alloc->base.default_alignment = default_alignment;
    alloc->type = PTK_ALLOCATOR_ARENA;
    alloc->size = pool_size;
    alloc->offset = 0;
    alloc->destructor_list = NULL;
    memset(&alloc->stats, 0, sizeof(alloc->stats));

    debug("Created arena allocator with %zu bytes and %zu-byte alignment", pool_size, default_alignment);
    return &alloc->base;
}
