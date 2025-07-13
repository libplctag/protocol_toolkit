#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ptk_utils.h>
#include <ptk_err.h>


/**
 * @brief Timeout entry for deadline tracking
 */
typedef struct timeout_entry_t {
    int fd;                    // File descriptor this timeout applies to
    ptk_time_ms deadline;      // When this timeout expires
} timeout_entry_t;

/**
 * @brief Min-heap for efficient timeout processing
 * 
 * Heap property: parent deadline <= child deadline
 * Root (index 0) always has the earliest deadline
 */
typedef struct timeout_heap_t {
    timeout_entry_t *entries;  // Array of timeout entries
    size_t capacity;           // Total slots available
    size_t count;              // Number of active timeouts
} timeout_heap_t;

// Heap management
timeout_heap_t *timeout_heap_create(size_t initial_capacity);
void timeout_heap_destroy(timeout_heap_t *heap);

// Timeout operations
ptk_err timeout_heap_add(timeout_heap_t *heap, int fd, ptk_time_ms deadline);
timeout_entry_t *timeout_heap_peek(timeout_heap_t *heap);
timeout_entry_t *timeout_heap_pop(timeout_heap_t *heap);
void timeout_heap_remove(timeout_heap_t *heap, int fd);

// Utilities
bool timeout_heap_is_empty(const timeout_heap_t *heap);
size_t timeout_heap_count(const timeout_heap_t *heap);
ptk_time_ms timeout_heap_next_deadline(const timeout_heap_t *heap);
