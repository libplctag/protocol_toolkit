#include "threadlet_scheduler.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <string.h>

// We need to access threadlet_t internal structure
// This assumes threadlet_t has a status field
extern threadlet_status_t threadlet_get_status(const threadlet_t *threadlet);
extern void threadlet_set_status(threadlet_t *threadlet, threadlet_status_t status);

ptk_err threadlet_queue_init(threadlet_queue_t *queue, size_t initial_capacity) {
    if (!queue) {
        warn("Invalid queue pointer");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    if (initial_capacity == 0) {
        initial_capacity = 16; // Default capacity
    }
    
    queue->items = ptk_alloc(initial_capacity * sizeof(threadlet_t *), NULL);
    if (!queue->items) {
        warn("Failed to allocate threadlet queue items");
        return PTK_ERR_NO_MEMORY;
    }
    
    queue->capacity = initial_capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    
    trace("Initialized threadlet queue with capacity %zu", initial_capacity);
    return PTK_OK;
}

void threadlet_queue_cleanup(threadlet_queue_t *queue) {
    if (queue && queue->items) {
        trace("Cleaning up threadlet queue (count=%zu)", queue->count);
        ptk_free(queue->items);
        memset(queue, 0, sizeof(threadlet_queue_t));
    }
}

ptk_err threadlet_queue_enqueue(threadlet_queue_t *queue, threadlet_t *threadlet) {
    if (!queue || !threadlet) {
        warn("Invalid arguments to threadlet_queue_enqueue");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    if (threadlet_queue_is_full(queue)) {
        warn("Threadlet queue is full (%zu/%zu)", queue->count, queue->capacity);
        return PTK_ERR_NO_MEMORY;
    }
    
    queue->items[queue->tail] = threadlet;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    trace("Enqueued threadlet (count now %zu)", queue->count);
    return PTK_OK;
}

threadlet_t *threadlet_queue_dequeue(threadlet_queue_t *queue) {
    if (!queue || threadlet_queue_is_empty(queue)) {
        return NULL;
    }
    
    threadlet_t *threadlet = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    trace("Dequeued threadlet (count now %zu)", queue->count);
    return threadlet;
}

bool threadlet_queue_is_empty(const threadlet_queue_t *queue) {
    return !queue || queue->count == 0;
}

bool threadlet_queue_is_full(const threadlet_queue_t *queue) {
    return !queue || queue->count >= queue->capacity;
}

size_t threadlet_queue_count(const threadlet_queue_t *queue) {
    return queue ? queue->count : 0;
}

void threadlet_wake(threadlet_t *threadlet, threadlet_status_t new_status) {
    if (!threadlet) {
        warn("Cannot wake NULL threadlet");
        return;
    }
    
    threadlet_status_t old_status = threadlet_get_status(threadlet);
    threadlet_set_status(threadlet, new_status);
    
    trace("Threadlet status changed: %d -> %d", old_status, new_status);
}
