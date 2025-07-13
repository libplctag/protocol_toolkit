#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <ptk_utils.h>

// Forward declaration from ptk_threadlet.h
typedef struct threadlet_t threadlet_t;

/**
 * @brief Threadlet status for scheduling
 */
typedef enum {
    THREADLET_READY,      // Can be scheduled for execution
    THREADLET_RUNNING,    // Currently executing
    THREADLET_WAITING,    // Blocked waiting for I/O or timeout
    THREADLET_FINISHED,   // Completed execution
    THREADLET_ABORTED     // Aborted due to error or cancellation
} threadlet_status_t;

/**
 * @brief Queue for managing threadlets in various states
 */
typedef struct threadlet_queue_t {
    threadlet_t **items;   // Array of threadlet pointers
    size_t capacity;       // Total queue capacity
    size_t count;          // Current number of items
    size_t head;           // Index of first item
    size_t tail;           // Index of next insertion point
} threadlet_queue_t;

// Queue management
ptk_err threadlet_queue_init(threadlet_queue_t *queue, size_t initial_capacity);
void threadlet_queue_cleanup(threadlet_queue_t *queue);

// Queue operations
ptk_err threadlet_queue_enqueue(threadlet_queue_t *queue, threadlet_t *threadlet);
threadlet_t *threadlet_queue_dequeue(threadlet_queue_t *queue);
bool threadlet_queue_is_empty(const threadlet_queue_t *queue);
bool threadlet_queue_is_full(const threadlet_queue_t *queue);
size_t threadlet_queue_count(const threadlet_queue_t *queue);

// Threadlet state management
void threadlet_wake(threadlet_t *threadlet, threadlet_status_t new_status);
threadlet_status_t threadlet_get_status(const threadlet_t *threadlet);
void threadlet_set_status(threadlet_t *threadlet, threadlet_status_t status);
