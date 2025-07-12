#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../include/ptk_utils.h"

// Timeout heap for efficient timeout tracking
typedef struct timeout_heap_t timeout_heap_t;

typedef struct timeout_entry_t {
    int fd;
    ptk_time_ms deadline;
} timeout_entry_t;

timeout_heap_t *timeout_heap_create(void);
void timeout_heap_destroy(timeout_heap_t *heap);
bool timeout_heap_is_empty(timeout_heap_t *heap);
void timeout_heap_insert(timeout_heap_t *heap, int fd, ptk_time_ms deadline);
void timeout_heap_remove(timeout_heap_t *heap, int fd);
timeout_entry_t *timeout_heap_peek(timeout_heap_t *heap);
ptk_time_ms timeout_heap_next_deadline(timeout_heap_t *heap);
