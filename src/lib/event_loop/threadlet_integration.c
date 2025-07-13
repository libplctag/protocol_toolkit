#include "threadlet_integration.h"
#include "event_loop.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <string.h>

// Thread-local current threadlet
__thread threadlet_t *current_threadlet = NULL;

// Forward declaration for trampoline
static void threadlet_trampoline(void);

// Destructor for threadlet
static void threadlet_destructor(void *ptr) {
    threadlet_t *t = (threadlet_t *)ptr;
    if (!t) return;
    
    trace("Destroying threadlet");
    if (t->stack) {
        ptk_free(t->stack);
    }
}

// Internal function called by ptk_threadlet_create (from threadlet.c)
threadlet_t *threadlet_create_internal(threadlet_run_func_t func, void *data) {
    if (!func) {
        warn("Cannot create threadlet with NULL function");
        return NULL;
    }
    
    threadlet_t *t = ptk_alloc(sizeof(threadlet_t), threadlet_destructor);
    if (!t) {
        warn("Failed to allocate threadlet");
        return NULL;
    }
    
    memset(t, 0, sizeof(threadlet_t));
    
    // Allocate stack
    t->stack_size = 64 * 1024; // 64KB default
    t->stack = ptk_alloc(t->stack_size, NULL);
    if (!t->stack) {
        warn("Failed to allocate threadlet stack");
        ptk_free(t);
        return NULL;
    }
    
    // Initialize context
    if (getcontext(&t->context) < 0) {
        warn("Failed to get threadlet context");
        ptk_free(t);
        return NULL;
    }
    
    t->context.uc_stack.ss_sp = t->stack;
    t->context.uc_stack.ss_size = t->stack_size;
    t->context.uc_link = NULL; // Don't return anywhere when done
    
    // Set up entry point
    makecontext(&t->context, threadlet_trampoline, 0);
    
    // Initialize state
    t->entry_func = func;
    t->user_data = data;
    t->status = THREADLET_READY;
    t->waiting_fd = -1;
    t->finished = false;
    
    trace("Created threadlet");
    return t;
}

// Trampoline function that runs the user's entry function
static void threadlet_trampoline(void) {
    threadlet_t *t = current_threadlet;
    if (!t || !t->entry_func) {
        warn("Invalid threadlet in trampoline");
        return;
    }
    
    trace("Starting threadlet execution");
    
    // Run user function
    t->entry_func(t->user_data);
    
    // Mark as finished
    t->finished = true;
    t->status = THREADLET_FINISHED;
    
    trace("Threadlet execution finished");
    
    // Yield back to scheduler (this threadlet will be cleaned up)
    threadlet_yield_to_scheduler(t);
}

threadlet_t *threadlet_get_current(void) {
    return current_threadlet;
}

void threadlet_yield_to_scheduler(threadlet_t *threadlet) {
    if (!threadlet || !threadlet->event_loop) {
        warn("Cannot yield from invalid threadlet");
        return;
    }
    
    trace("Threadlet yielding to scheduler");
    current_threadlet = NULL;
    
    // Switch back to event loop scheduler context
    swapcontext(&threadlet->context, &threadlet->event_loop->scheduler_context);
}

void threadlet_run_until_yield(event_loop_t *loop, threadlet_t *threadlet) {
    if (!loop || !threadlet) {
        warn("Invalid arguments to threadlet_run_until_yield");
        return;
    }
    
    trace("Running threadlet until yield");
    
    // Set up context
    current_threadlet = threadlet;
    loop->current_threadlet = threadlet;
    threadlet->event_loop = loop;
    threadlet->status = THREADLET_RUNNING;
    
    // Switch to threadlet
    swapcontext(&loop->scheduler_context, &threadlet->context);
    
    // Threadlet has yielded back
    loop->current_threadlet = NULL;
    
    if (threadlet->status == THREADLET_FINISHED) {
        trace("Threadlet finished, will be cleaned up");
        ptk_free(threadlet);
    }
}

threadlet_status_t threadlet_get_status(const threadlet_t *threadlet) {
    return threadlet ? threadlet->status : THREADLET_ABORTED;
}

void threadlet_set_status(threadlet_t *threadlet, threadlet_status_t status) {
    if (threadlet) {
        threadlet->status = status;
    }
}
