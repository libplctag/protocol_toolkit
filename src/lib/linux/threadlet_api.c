#include <ptk_threadlet.h>
#include "threadlet_core.h"
#include "threadlet_scheduler.h"
#include <ptk_log.h>
#include <ptk_os_thread.h>
#include <ptk_shared.h>

static ptk_thread_local size_t thread_assignment_counter = 0;
static ptk_mutex *global_thread_mutex = NULL;
static ptk_shared_handle_t thread_pool_handle = PTK_SHARED_INVALID_HANDLE;
static size_t thread_pool_size = 0;
static size_t current_thread_index = 0;

typedef struct {
    ptk_thread **threads;
    size_t size;
} thread_pool_t;

static void thread_pool_destructor(void *ptr) {
    thread_pool_t *pool = (thread_pool_t *)ptr;
    if (pool && pool->threads) {
        // Note: Individual threads should be cleaned up elsewhere
        ptk_free(&pool->threads);
    }
}

static void init_thread_pool(void) {
    if (PTK_SHARED_IS_VALID(thread_pool_handle)) return;
    
    global_thread_mutex = ptk_mutex_create();
    if (!global_thread_mutex) {
        error("Failed to create global thread mutex");
        return;
    }
    
    // Initialize shared memory subsystem if not already done
    ptk_shared_init();
    
    thread_pool_size = 4;
    
    // Allocate the thread pool structure
    thread_pool_t *pool = ptk_alloc(sizeof(thread_pool_t), thread_pool_destructor);
    if (!pool) {
        error("Failed to allocate thread pool structure");
        return;
    }
    
    // Allocate the thread array
    pool->threads = ptk_alloc(thread_pool_size * sizeof(ptk_thread*), NULL);
    if (!pool->threads) {
        error("Failed to allocate thread pool array");
        ptk_free(&pool);
        return;
    }
    
    pool->size = thread_pool_size;
    
    for (size_t i = 0; i < thread_pool_size; i++) {
        pool->threads[i] = NULL;
    }
    
    // Wrap in shared handle
    thread_pool_handle = ptk_shared_wrap(pool);
    if (!PTK_SHARED_IS_VALID(thread_pool_handle)) {
        error("Failed to create shared handle for thread pool");
        ptk_free(&pool);
        return;
    }
}

static ptk_thread *get_next_thread_round_robin(void) {
    if (!PTK_SHARED_IS_VALID(thread_pool_handle)) {
        init_thread_pool();
        if (!PTK_SHARED_IS_VALID(thread_pool_handle)) return NULL;
    }
    
    ptk_mutex_wait_lock(global_thread_mutex, PTK_TIME_WAIT_FOREVER);
    
    ptk_thread *result = NULL;
    
    use_shared(thread_pool_handle, thread_pool_t *pool) {
        if (pool->size > 0) {
            current_thread_index = (current_thread_index + 1) % pool->size;
            result = pool->threads[current_thread_index];
        }
    } on_shared_fail {
        error("Failed to acquire thread pool for round-robin selection");
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