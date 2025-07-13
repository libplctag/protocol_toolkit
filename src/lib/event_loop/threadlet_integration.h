#pragma once

#include <ucontext.h>
#include <ptk_utils.h>
#include "threadlet_scheduler.h"

// Forward declaration
typedef struct event_loop_t event_loop_t;

/**
 * @brief Enhanced threadlet structure with scheduler integration
 */
typedef struct threadlet_t {
    // Execution context
    ucontext_t context;                  // POSIX execution context
    void *stack;                         // Stack memory
    size_t stack_size;                   // Stack size
    
    // Entry point
    threadlet_run_func_t entry_func;     // Entry function
    void *user_data;                     // User parameter
    
    // Scheduler state
    threadlet_status_t status;           // Current status
    event_loop_t *event_loop;            // Owning event loop
    
    // Waiting state (when status == THREADLET_WAITING)
    int waiting_fd;                      // FD being waited on (-1 if not waiting)
    uint32_t waiting_events;             // Events being waited for
    ptk_time_ms deadline;                // Timeout deadline
    
    // Lifecycle
    bool finished;                       // True when execution complete
} threadlet_t;

// Functions needed by event loop
threadlet_t *threadlet_get_current(void);
void threadlet_yield_to_scheduler(threadlet_t *threadlet);
void threadlet_run_until_yield(event_loop_t *loop, threadlet_t *threadlet);

// Functions needed by scheduler
threadlet_status_t threadlet_get_status(const threadlet_t *threadlet);
void threadlet_set_status(threadlet_t *threadlet, threadlet_status_t status);
