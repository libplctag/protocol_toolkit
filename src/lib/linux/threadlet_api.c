#include <ptk_threadlet.h>
#include "threadlet_core.h"
#include "threadlet_scheduler.h"
#include <ptk_log.h>
#include <ptk_os_thread.h>

static ptk_thread_local size_t thread_assignment_counter = 0;
static ptk_mutex *global_thread_mutex = NULL;
static ptk_thread **thread_pool = NULL;
static size_t thread_pool_size = 0;
static size_t current_thread_index = 0;

static void init_thread_pool(void) {
    if (global_thread_mutex) return;
    
    global_thread_mutex = ptk_mutex_create();
    if (!global_thread_mutex) {
        error("Failed to create global thread mutex");
        return;
    }
    
    thread_pool_size = 4;
    thread_pool = ptk_alloc(thread_pool_size * sizeof(ptk_thread*), NULL);
    if (!thread_pool) {
        error("Failed to allocate thread pool");
        return;
    }
    
    for (size_t i = 0; i < thread_pool_size; i++) {
        thread_pool[i] = NULL;
    }
}

static ptk_thread *get_next_thread_round_robin(void) {
    if (!global_thread_mutex) {
        init_thread_pool();
        if (!global_thread_mutex) return NULL;
    }
    
    ptk_mutex_wait_lock(global_thread_mutex, PTK_TIME_WAIT_FOREVER);
    
    ptk_thread *result = NULL;
    if (thread_pool_size > 0) {
        current_thread_index = (current_thread_index + 1) % thread_pool_size;
        result = thread_pool[current_thread_index];
    }
    
    ptk_mutex_unlock(global_thread_mutex);
    return result;
}

threadlet_t *ptk_threadlet_create(threadlet_run_func_t func, void *data) {
    info("Creating new threadlet");
    
    if (!func) {
        warn("Invalid function pointer");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    return threadlet_create_internal(func, data);
}

ptk_err ptk_threadlet_resume(threadlet_t *threadlet) {
    info("Resuming threadlet with round-robin distribution");
    
    if (!threadlet) {
        warn("Cannot resume NULL threadlet");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    event_loop_t *loop = get_thread_local_event_loop();
    if (!loop) {
        warn("No event loop initialized for current thread");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return PTK_ERR_INVALID_STATE;
    }
    
    ptk_err err = event_loop_enqueue_ready(loop, threadlet);
    if (err != PTK_OK) {
        warn("Failed to enqueue threadlet: %d", err);
        return err;
    }
    
    debug("Threadlet resumed successfully");
    return PTK_OK;
}

ptk_err ptk_threadlet_yield(void) {
    trace("Threadlet yielding");
    
    threadlet_t *current = threadlet_get_current();
    if (!current) {
        warn("ptk_threadlet_yield called outside threadlet context");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return PTK_ERR_INVALID_STATE;
    }
    
    threadlet_set_status(current, THREADLET_READY);
    threadlet_yield_to_scheduler(current);
    
    return PTK_OK;
}

ptk_err ptk_threadlet_join(threadlet_t *threadlet, ptk_duration_ms timeout_ms) {
    info("Joining threadlet with timeout=%ld ms", timeout_ms);
    
    if (!threadlet) {
        warn("Cannot join NULL threadlet");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    ptk_time_ms deadline = (timeout_ms == 0) ? 0 : ptk_now_ms() + timeout_ms;
    
    while (threadlet_get_status(threadlet) != THREADLET_FINISHED) {
        if (deadline > 0 && ptk_now_ms() >= deadline) {
            warn("Threadlet join timed out");
            ptk_set_err(PTK_ERR_TIMEOUT);
            return PTK_ERR_TIMEOUT;
        }
        
        ptk_err err = ptk_threadlet_yield();
        if (err != PTK_OK) {
            return err;
        }
    }
    
    info("Threadlet join completed successfully");
    return PTK_OK;
}