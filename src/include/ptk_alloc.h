#pragma once

/**
 * @file ptk_alloc.h
 * @brief Pluggable allocator interface for protocol toolkit
 *
 * Provides a v-table based allocator interface enabling custom memory management
 * strategies including arena allocation, pool allocation, and debug tracking.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ptk_err.h"

// Forward declarations
typedef struct ptk_allocator ptk_allocator_t;
typedef struct ptk_alloc_stats ptk_alloc_stats_t;

/**
 * @brief Destructor function type for automatic cleanup
 */
typedef void (*ptk_destructor_fn)(void *ptr);

/**
 * @brief Allocator function types
 */
typedef void* (*ptk_alloc_fn)(struct ptk_allocator *allocator, size_t size, ptk_destructor_fn destructor);
typedef void* (*ptk_realloc_fn)(struct ptk_allocator *allocator, void *ptr, size_t new_size);
typedef void (*ptk_free_fn)(struct ptk_allocator *allocator, void *ptr);
typedef void (*ptk_reset_fn)(struct ptk_allocator *allocator);
typedef void (*ptk_get_stats_fn)(struct ptk_allocator *allocator, ptk_alloc_stats_t *stats);
typedef void (*ptk_destroy_fn)(struct ptk_allocator *allocator);

/**
 * @brief Allocator statistics structure
 */
struct ptk_alloc_stats {
    size_t total_allocated;      ///< Total bytes currently allocated
    size_t peak_allocated;       ///< High water mark of allocated bytes
    size_t total_allocations;    ///< Total number of allocations made
    size_t total_frees;          ///< Total number of frees made
    size_t active_allocations;   ///< Number of currently active allocations
    size_t total_bytes_allocated; ///< Cumulative bytes allocated (lifetime)
    size_t total_bytes_freed;    ///< Cumulative bytes freed (lifetime)
};

/**
 * @brief Generic allocator interface
 *
 * All allocators use this same structure. Allocator-specific data
 * follows this structure in memory.
 */
struct ptk_allocator {
    ptk_alloc_fn alloc;        ///< Allocate memory with optional destructor
    ptk_realloc_fn realloc;    ///< Reallocate memory
    ptk_free_fn free;          ///< Free memory
    ptk_reset_fn reset;        ///< Reset/free all allocations (may be NULL)
    ptk_get_stats_fn get_stats; ///< Get allocator statistics (may be NULL)
    ptk_destroy_fn destroy;    ///< Destroy allocator and free all resources
    size_t default_alignment;  ///< Default alignment for allocations
};

/**
 * @brief Create default system allocator
 *
 * Creates a system allocator that uses malloc/realloc/free with no debug tracking.
 * Reset function is a no-op for system allocator.
 *
 * @param default_alignment Default alignment for allocations
 * @return Pointer to allocator or NULL on failure
 */
ptk_allocator_t *allocator_default_create(size_t default_alignment);

/**
 * @brief Create debug allocator
 *
 * Creates a debug allocator that tracks all allocations and can detect leaks.
 *
 * @param default_alignment Default alignment for allocations
 * @return Pointer to allocator or NULL on failure
 */
ptk_allocator_t *allocator_debug_create(size_t default_alignment);

/**
 * @brief Create arena allocator
 *
 * Creates an arena allocator from a pre-allocated memory pool.
 * No individual frees - all memory is reclaimed when the arena is reset.
 *
 * @param pool_size Size of memory pool to allocate
 * @param default_alignment Default alignment for allocations
 * @return Pointer to allocator or NULL on failure
 */
ptk_allocator_t *allocator_arena_create(size_t pool_size, size_t default_alignment);


/**
 * @brief Print debug allocator report
 *
 * Prints detailed information about current allocations and statistics.
 * Useful for debugging memory leaks. Only works with debug allocators.
 *
 * @param allocator Debug allocator
 */
void ptk_debug_allocator_report(const ptk_allocator_t *allocator);

/**
 * @brief Check for memory leaks
 *
 * @param allocator Debug allocator
 * @return true if leaks detected (active allocations > 0), false otherwise
 */
bool ptk_debug_allocator_has_leaks(const ptk_allocator_t *allocator);

/**
 * @brief Simple convenience macros for allocator operations
 */
#define ptk_alloc(allocator, size) (allocator)->alloc(allocator, size, NULL)
#define ptk_alloc_with_destructor(allocator, size, destructor) (allocator)->alloc(allocator, size, destructor)
#define ptk_realloc(allocator, ptr, new_size) (allocator)->realloc(allocator, ptr, new_size)
#define ptk_free(allocator, ptr) (allocator)->free(allocator, ptr)
#define ptk_reset(allocator) do { if ((allocator)->reset) (allocator)->reset(allocator); } while(0)
#define ptk_get_stats(allocator, stats) do { if ((allocator)->get_stats) (allocator)->get_stats(allocator, stats); } while(0)
#define ptk_allocator_destroy(allocator) do { if (allocator) { (allocator)->destroy(allocator); allocator = NULL; } } while(0)

/**
 * @brief Convenience macro for allocating structures with destructors
 */
#define ptk_alloc_auto(allocator, type, destructor) \
    (type*)ptk_alloc_with_destructor(allocator, sizeof(type), (ptk_destructor_fn)(destructor))

/**
 * @brief Allocation alignment helpers
 */
#define PTK_ALIGN_SIZE(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))
#define PTK_ALIGN_PTR(ptr, alignment) ((void*)PTK_ALIGN_SIZE((uintptr_t)(ptr), alignment))

/**
 * @brief Example usage patterns
 */
#if 0
// Example 1: Using system allocator
void example_system_alloc() {
    ptk_allocator_t *alloc = allocator_default_create(8);
    void *ptr = ptk_alloc(alloc, 1024);
    // ... use ptr ...
    ptk_free(alloc, ptr);
    ptk_allocator_destroy(alloc);
}

// Example 2: Using debug allocator
void example_debug_alloc() {
    ptk_allocator_t *alloc = allocator_debug_create(8);

    void *ptr1 = ptk_alloc(alloc, 1024);
    void *ptr2 = ptk_alloc(alloc, 512);

    ptk_free(alloc, ptr1);
    // ptr2 intentionally not freed to test leak detection

    if (ptk_debug_allocator_has_leaks(alloc)) {
        ptk_debug_allocator_report(alloc);  // Will show ptr2 leak
    }

    ptk_allocator_destroy(alloc);  // Clean up remaining allocations
}

// Example 3: Using arena allocator for Modbus
void example_arena_modbus() {
    // Allocate arena for up to 2000 coils + overhead
    ptk_allocator_t *alloc = allocator_arena_create(8192, 8);

    // Decode Modbus message - all arrays allocated from arena
    modbus_tcp_request_t request;
    ptk_err err = modbus_tcp_request_decode(alloc, &request, buffer, context);

    if (err == PTK_OK) {
        // Process the request - arrays are in arena memory
        process_modbus_request(&request);
    }

    // Simple cleanup - reset arena (no individual frees needed)
    ptk_reset(alloc);
    ptk_allocator_destroy(alloc);
}
#endif