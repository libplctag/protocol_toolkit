#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ucontext.h>
#include <ptk_utils.h>
#include "event_registration.h"
#include "threadlet_scheduler.h"
#include "timeout_heap.h"
#include "platform/linux_event_loop.h"
#include <ptk_err.h>

// Forward declarations
typedef struct threadlet_t threadlet_t;

/**
 * @brief Main event loop structure - one per OS thread
 */
typedef struct event_loop_t {
    // Platform-specific event polling
    platform_event_loop_t *platform;
    
    // Threadlet scheduling
    threadlet_queue_t ready_queue;      // Threadlets ready to run
    threadlet_queue_t waiting_queue;    // Threadlets blocked on I/O (for tracking)
    threadlet_t *current_threadlet;     // Currently executing threadlet
    ucontext_t scheduler_context;       // Context to return to when threadlets yield
    
    // Event tracking
    event_registration_table_t *registrations;  // fd -> threadlet mapping
    timeout_heap_t *timeouts;                   // Timeout management
    
    // Control
    bool running;                       // Whether event loop should continue
    ptk_time_ms current_time_ms;        // Cached current time
} event_loop_t;

// Event loop lifecycle
event_loop_t *event_loop_create(int max_events);
void event_loop_destroy(event_loop_t *loop);

// Main loop execution
void event_loop_run(event_loop_t *loop);
void event_loop_stop(event_loop_t *loop);

// Threadlet integration - called by socket API
ptk_err event_loop_wait_fd(event_loop_t *loop, int fd, uint32_t events, ptk_duration_ms timeout_ms);
ptk_err event_loop_signal_fd(event_loop_t *loop, int fd);

// Thread-local access
event_loop_t *get_thread_local_event_loop(void);
event_loop_t *init_thread_event_loop(int max_events);

// Time utilities
ptk_time_ms event_loop_get_current_time(event_loop_t *loop);
void event_loop_update_time(event_loop_t *loop);
