#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "platform/linux_event_loop.h"
#include "event_registration.h"
#include "threadlet_scheduler.h"
#include "timeout_heap.h"

// Main event loop structure
typedef struct ptk_event_loop_t {
    platform_event_loop_t *platform;
    event_registration_table_t *registrations; // fd -> registration
    threadlet_queue_t ready_queue;
    threadlet_queue_t waiting_queue;
    bool running;
    uint64_t current_time_ms;
    timeout_heap_t *timeout_heap;
} ptk_event_loop_t;

// API
ptk_event_loop_t *ptk_event_loop_create(int max_events);
void ptk_event_loop_destroy(ptk_event_loop_t *loop);
void ptk_event_loop_run(ptk_event_loop_t *loop);
void ptk_event_loop_stop(ptk_event_loop_t *loop);

// Register interest in fd events
ptk_err ptk_event_loop_register_fd(ptk_event_loop_t *loop, int fd, uint32_t events, void *threadlet, uint64_t deadline);
ptk_err ptk_event_loop_unregister_fd(ptk_event_loop_t *loop, int fd);

// Used by socket API integration
ptk_err ptk_event_loop_signal_fd(ptk_event_loop_t *loop, int fd);

// Get current time (ms)
uint64_t ptk_event_loop_get_time(ptk_event_loop_t *loop);
