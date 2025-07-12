#pragma once

#include <stdint.h>
#include <stdbool.h>

// Opaque threadlet type
typedef struct threadlet_t threadlet_t;

// Threadlet queue
typedef struct threadlet_queue_t threadlet_queue_t;

threadlet_queue_t *threadlet_queue_create(void);
void threadlet_queue_destroy(threadlet_queue_t *queue);
bool threadlet_queue_is_empty(threadlet_queue_t *queue);
void threadlet_queue_enqueue(threadlet_queue_t *queue, threadlet_t *threadlet);
threadlet_t *threadlet_queue_dequeue(threadlet_queue_t *queue);

// Load balancing
int threadlet_queue_get_load(threadlet_queue_t *queue);
