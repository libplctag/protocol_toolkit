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

/**
 * @brief Allocator function types
 */
typedef void* (*ptk_alloc_fn)(size_t size, void *alloc_ctx);
typedef void* (*ptk_realloc_fn)(void *ptr, size_t new_size, void *alloc_ctx);
typedef void (*ptk_free_fn)(void *ptr, void *alloc_ctx);
typedef void (*ptk_reset_fn)(void *alloc_ctx);

/**
 * @brief Allocator statistics structure
 */
typedef struct ptk_alloc_stats {
    size_t total_allocated;      ///< Total bytes currently allocated
    size_t peak_allocated;       ///< High water mark of allocated bytes
    size_t total_allocations;    ///< Total number of allocations made
    size_t total_frees;          ///< Total number of frees made
    size_t active_allocations;   ///< Number of currently active allocations
    size_t total_bytes_allocated; ///< Cumulative bytes allocated (lifetime)
    size_t total_bytes_freed;    ///< Cumulative bytes freed (lifetime)
} ptk_alloc_stats_t;

typedef void (*ptk_get_stats_fn)(void *alloc_ctx, ptk_alloc_stats_t *stats);

/**
 * @brief Allocator v-table
 * 
 * Function pointer table for allocator implementations.
 */
typedef struct ptk_allocator_vtable {
    ptk_alloc_fn alloc;      ///< Allocate memory
    ptk_realloc_fn realloc;  ///< Reallocate memory
    ptk_free_fn free;        ///< Free memory
    ptk_reset_fn reset;      ///< Reset/free all allocations (may be NULL)
    ptk_get_stats_fn get_stats; ///< Get allocator statistics (may be NULL)
} ptk_allocator_vtable_t;

/**
 * @brief Generic allocator interface
 * 
 * All allocators use this same structure. Allocator-specific data
 * is hidden behind the context pointer.
 */
typedef struct ptk_allocator {
    const ptk_allocator_vtable_t *vtable;  ///< Function pointer table
    void *context;                         ///< Allocator-specific context
} ptk_allocator_t;

/**
 * @brief Standard system allocator (default)
 * 
 * Uses malloc/realloc/free with no debug tracking.
 * Reset function is a no-op for system allocator.
 */
extern const ptk_allocator_t PTK_SYSTEM_ALLOCATOR;

/**
 * @brief Debug allocator initialization
 * 
 * Creates a debug allocator that tracks all allocations and can detect leaks.
 * The allocator context is hidden behind the returned generic allocator interface.
 * 
 * @param allocator Allocator structure to initialize (out)
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_debug_allocator_init(ptk_allocator_t *allocator);

/**
 * @brief Destroy debug allocator and free its resources
 * 
 * This will free any remaining tracked allocations and clean up the
 * debug allocator's internal state.
 * 
 * @param allocator Debug allocator to destroy
 */
void ptk_debug_allocator_destroy(ptk_allocator_t *allocator);

/**
 * @brief Print debug allocator report
 * 
 * Prints detailed information about current allocations and statistics.
 * Useful for debugging memory leaks.
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
 * @brief Arena allocator initialization
 * 
 * Creates an arena allocator from a pre-allocated memory block.
 * No individual frees - all memory is reclaimed when the arena is reset.
 * The arena context is hidden behind the returned generic allocator interface.
 * 
 * @param allocator Allocator structure to initialize (out)
 * @param memory Pre-allocated memory block
 * @param size Size of memory block
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_arena_allocator_init(ptk_allocator_t *allocator, void *memory, size_t size);

/**
 * @brief Destroy arena allocator
 * 
 * Cleans up the arena allocator's internal state. The memory block
 * itself is not freed (it was provided by the caller).
 * 
 * @param allocator Arena allocator to destroy
 */
void ptk_arena_allocator_destroy(ptk_allocator_t *allocator);

/**
 * @brief Simple convenience macros for allocator operations
 */
#define ptk_alloc(allocator, size) (allocator)->vtable->alloc(size, (allocator)->context)
#define ptk_realloc(allocator, ptr, new_size) (allocator)->vtable->realloc(ptr, new_size, (allocator)->context)
#define ptk_free(allocator, ptr) (allocator)->vtable->free(ptr, (allocator)->context)
#define ptk_reset(allocator) do { if ((allocator)->vtable->reset) (allocator)->vtable->reset((allocator)->context); } while(0)
#define ptk_get_stats(allocator, stats) do { if ((allocator)->vtable->get_stats) (allocator)->vtable->get_stats((allocator)->context, stats); } while(0)

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
    const ptk_allocator_t *alloc = &PTK_SYSTEM_ALLOCATOR;
    void *ptr = ptk_alloc(alloc, 1024);
    // ... use ptr ...
    ptk_free(alloc, ptr);
}

// Example 2: Using debug allocator
void example_debug_alloc() {
    ptk_allocator_t alloc;
    ptk_debug_allocator_init(&alloc);
    
    void *ptr1 = ptk_alloc(&alloc, 1024);
    void *ptr2 = ptk_alloc(&alloc, 512);
    
    ptk_free(&alloc, ptr1);
    // ptr2 intentionally not freed to test leak detection
    
    if (ptk_debug_allocator_has_leaks(&alloc)) {
        ptk_debug_allocator_report(&alloc);  // Will show ptr2 leak
    }
    
    ptk_reset(&alloc);  // Clean up remaining allocations
    ptk_debug_allocator_destroy(&alloc);
}

// Example 3: Using arena allocator for Modbus
void example_arena_modbus() {
    // Allocate arena for up to 2000 coils + overhead
    static uint8_t arena_memory[8192];
    ptk_allocator_t alloc;
    ptk_arena_allocator_init(&alloc, arena_memory, sizeof(arena_memory));
    
    // Decode Modbus message - all arrays allocated from arena
    modbus_tcp_request_t request;
    ptk_err err = modbus_tcp_request_decode(&alloc, &request, buffer, context);
    
    if (err == PTK_OK) {
        // Process the request - arrays are in arena memory
        process_modbus_request(&request);
    }
    
    // Simple cleanup - reset arena (no individual frees needed)
    ptk_reset(&alloc);
    ptk_arena_allocator_destroy(&alloc);
}
#endif