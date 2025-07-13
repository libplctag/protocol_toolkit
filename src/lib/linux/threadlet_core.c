#include "threadlet_core.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_os_thread.h>
#include <signal.h>
#include <sys/mman.h>

static ptk_thread_local threadlet_t *current_threadlet = NULL;
static ptk_thread_local ucontext_t scheduler_context;

static void threadlet_destructor(void *ptr) {
    threadlet_t *threadlet = (threadlet_t *)ptr;
    if (!threadlet) return;
    
    info("Destroying threadlet");
    
    if (threadlet->stack) {
        if (munmap(threadlet->stack, THREADLET_STACK_SIZE) != 0) {
            warn("Failed to unmap threadlet stack: %s", strerror(errno));
        }
    }
}

static void threadlet_entry_wrapper(void) {
    threadlet_t *threadlet = threadlet_get_current();
    if (!threadlet || !threadlet->entry_func) {
        error("Invalid threadlet in entry wrapper");
        return;
    }
    
    info("Starting threadlet execution");
    
    threadlet_set_status(threadlet, THREADLET_RUNNING);
    threadlet->entry_func(threadlet->user_data);
    
    info("Threadlet execution completed");
    threadlet_set_status(threadlet, THREADLET_FINISHED);
    threadlet->finished = true;
    
    threadlet_yield_to_scheduler(threadlet);
}

threadlet_t *threadlet_create_internal(threadlet_run_func_t func, void *data) {
    info("Creating threadlet with 64KiB stack");
    
    if (!func) {
        warn("Invalid function pointer");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    threadlet_t *threadlet = ptk_alloc(sizeof(threadlet_t), threadlet_destructor);
    if (!threadlet) {
        error("Failed to allocate threadlet");
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    threadlet->stack = mmap(NULL, THREADLET_STACK_SIZE, 
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                           -1, 0);
    if (threadlet->stack == MAP_FAILED) {
        error("Failed to allocate threadlet stack: %s", strerror(errno));
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        ptk_free(&threadlet);
        return NULL;
    }
    
    if (getcontext(&threadlet->context) != 0) {
        error("getcontext failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_DEVICE_FAILURE);
        ptk_free(&threadlet);
        return NULL;
    }
    
    threadlet->context.uc_stack.ss_sp = threadlet->stack;
    threadlet->context.uc_stack.ss_size = THREADLET_STACK_SIZE;
    threadlet->context.uc_link = &scheduler_context;
    
    makecontext(&threadlet->context, threadlet_entry_wrapper, 0);
    
    threadlet->entry_func = func;
    threadlet->user_data = data;
    threadlet->status = THREADLET_CREATED;
    threadlet->waiting_fd = -1;
    threadlet->waiting_events = 0;
    threadlet->deadline = 0;
    threadlet->finished = false;
    
    debug("Threadlet created successfully");
    return threadlet;
}

threadlet_t *threadlet_get_current(void) {
    return current_threadlet;
}

void threadlet_set_current(threadlet_t *threadlet) {
    trace("Setting current threadlet to %p", threadlet);
    current_threadlet = threadlet;
}

threadlet_status_t threadlet_get_status(const threadlet_t *threadlet) {
    if (!threadlet) {
        warn("NULL threadlet in get_status");
        return THREADLET_FINISHED;
    }
    return threadlet->status;
}

void threadlet_set_status(threadlet_t *threadlet, threadlet_status_t status) {
    if (!threadlet) {
        warn("NULL threadlet in set_status");
        return;
    }
    
    trace("Threadlet status: %d -> %d", threadlet->status, status);
    threadlet->status = status;
}

void threadlet_yield_to_scheduler(threadlet_t *threadlet) {
    if (!threadlet) {
        warn("NULL threadlet in yield_to_scheduler");
        return;
    }
    
    trace("Threadlet yielding to scheduler");
    
    threadlet_t *old_current = current_threadlet;
    current_threadlet = NULL;
    
    if (swapcontext(&threadlet->context, &scheduler_context) != 0) {
        error("swapcontext failed: %s", strerror(errno));
        current_threadlet = old_current;
    }
}

void threadlet_resume_execution(threadlet_t *threadlet) {
    if (!threadlet) {
        warn("NULL threadlet in resume_execution");
        return;
    }
    
    trace("Resuming threadlet execution");
    
    threadlet_t *old_current = current_threadlet;
    current_threadlet = threadlet;
    threadlet_set_status(threadlet, THREADLET_RUNNING);
    
    if (swapcontext(&scheduler_context, &threadlet->context) != 0) {
        error("swapcontext failed: %s", strerror(errno));
        current_threadlet = old_current;
        return;
    }
    
    current_threadlet = old_current;
}