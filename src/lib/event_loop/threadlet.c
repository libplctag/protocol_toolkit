// src/lib/event_loop/threadlet.c
// Public threadlet API implementation
#include <ptk_threadlet.h>
#include "threadlet_integration.h"
#include "event_loop.h"
#include <ptk_log.h>

// Forward declaration
extern threadlet_t *threadlet_create_internal(threadlet_run_func_t func, void *data);

threadlet_t *ptk_threadlet_create(threadlet_run_func_t func, void *data) {
    trace("ptk_threadlet_create called");
    return threadlet_create_internal(func, data);
}

ptk_err ptk_threadlet_resume(threadlet_t *threadlet) {
    if (!threadlet) {
        warn("Cannot resume NULL threadlet");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    trace("ptk_threadlet_resume called");
    
    // Get current thread's event loop
    event_loop_t *loop = get_thread_local_event_loop();
    if (!loop) {
        warn("No event loop initialized for current thread");
        return PTK_ERR_INVALID_STATE;
    }
    
    // Add threadlet to ready queue
    threadlet_set_status(threadlet, THREADLET_READY);
    ptk_err err = threadlet_queue_enqueue(&loop->ready_queue, threadlet);
    if (err != PTK_OK) {
        warn("Failed to enqueue threadlet");
        return err;
    }
    
    return PTK_OK;
}

ptk_err ptk_threadlet_yield(void) {
    trace("ptk_threadlet_yield called");
    
    threadlet_t *current = threadlet_get_current();
    if (!current) {
        warn("ptk_threadlet_yield called outside threadlet context");
        return PTK_ERR_INVALID_STATE;
    }
    
    // Set status and yield back to scheduler
    threadlet_set_status(current, THREADLET_READY);
    threadlet_yield_to_scheduler(current);
    
    return PTK_OK;
}

ptk_err ptk_threadlet_join(threadlet_t *threadlet, ptk_duration_ms timeout_ms) {
    if (!threadlet) {
        warn("Cannot join NULL threadlet");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    trace("ptk_threadlet_join called with timeout=%u", timeout_ms);
    
    // For now, simple polling implementation
    // TODO: Implement proper waiting mechanism
    ptk_time_ms deadline = (timeout_ms == 0) ? 0 : ptk_now_ms() + timeout_ms;
    
    while (threadlet_get_status(threadlet) != THREADLET_FINISHED) {
        if (deadline > 0 && ptk_now_ms() >= deadline) {
            return PTK_ERR_TIMEOUT;
        }
        
        // Yield to allow other threadlets to run
        ptk_threadlet_yield();
    }
    
    return PTK_OK;
}
