#include "ptk_threadlet_posix.h"
#include <ucontext.h>
#include <string.h>
#include "../../include/ptk_os_thread.h"
#include "../../include/ptk_alloc.h"
#include "../../include/ptk_err.h"
#include "../../include/ptk_log.h"

/**
 * @brief Threadlet object for cooperative scheduling.
 */
typedef struct threadlet_t {
    ucontext_t ctx;                /* POSIX context for the threadlet. */
    threadlet_run_func_t entry;    /* Entry function for the threadlet. */
    void *param;                   /* Parameter to pass to the entry function. */
    bool finished;                 /* True if the threadlet has finished execution. */
    char *stack;                   /* Pointer to the threadlet's stack. */
    size_t stack_size;             /* Size of the stack. */
} threadlet_t;

/**
 * @brief Scheduler object for per-thread cooperative scheduling.
 */
typedef struct scheduler_t {
    ucontext_t main_ctx;           /* Main context for the scheduler. */
    threadlet_t *current_threadlet;/* Currently running threadlet. */
} scheduler_t;

/**
 * @brief Thread-local pointer to scheduler for each event thread.
 */
ptk_thread_local scheduler_t *ptk_scheduler = NULL;

/**
 * @brief Destructor for threadlet_t.
 *
 * Frees the stack memory associated with the threadlet.
 *
 * @param ptr Pointer to the threadlet_t to destroy.
 * @return void
 */
static void threadlet_destructor(void *ptr)
{
    threadlet_t *t = (threadlet_t *)ptr;
    if (!t) return;
    if (t->stack) ptk_free(t->stack);
}


/**
 * @brief Trampoline function for threadlet execution.
 *
 * Invokes the entry function of the current threadlet and marks it as finished.
 * Yields control back to the scheduler after completion.
 *
 * @return void
 */
static void threadlet_trampoline(void)
{
    if (ptk_scheduler && ptk_scheduler->current_threadlet) {
        ptk_scheduler->current_threadlet->entry(ptk_scheduler->current_threadlet->param);
        ptk_scheduler->current_threadlet->finished = true;
        threadlet_posix_yield();
    }
}

/**
 * @brief Create a new POSIX threadlet.
 *
 * Allocates and initializes a threadlet with its own stack and context.
 *
 * @param func Entry function for the threadlet.
 * @param param Parameter to pass to the entry function.
 * @return Pointer to the created threadlet, or NULL on failure.
 * @retval NULL Allocation failure or scheduler not initialized.
 */
threadlet_t *ptk_threadlet_posix_create(threadlet_run_func_t func, void *param)
{
    if (!ptk_scheduler) {
        warn("scheduler not initialized");
        return NULL;
    }
    size_t stack_size = 64 * 1024; // Default stack size
    threadlet_t *t = ptk_alloc(sizeof(threadlet_t), threadlet_destructor);
    if (!t) {
        warn("failed to allocate threadlet_t");
        return NULL;
    }
    memset(t, 0, sizeof(threadlet_t));
    t->entry = func;
    t->param = param;
    t->stack_size = stack_size;
    t->stack = ptk_alloc(stack_size, NULL);
    if (!t->stack) {
        warn("failed to allocate stack");
        ptk_free(t);
        return NULL;
    }
    getcontext(&t->ctx);
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = stack_size;
    t->ctx.uc_link = &ptk_scheduler->main_ctx;
    makecontext(&t->ctx, threadlet_trampoline, 0);
    t->finished = false;
    debug("created threadlet %p with stack %zu", t, stack_size);
    return t;
}


/**
 * @brief Resume execution of a threadlet.
 *
 * Switches context from the scheduler to the specified threadlet.
 *
 * @param t Pointer to the threadlet to resume.
 * @return ptk_err PTK_OK on success, error code otherwise.
 * @retval PTK_ERR_INVALID_ARG t is NULL.
 * @retval PTK_ERR_INVALID_STATE Scheduler not initialized.
 */
ptk_err threadlet_posix_resume(threadlet_t *t)
{
    if (!t) {
        warn("null threadlet");
        return PTK_ERR_INVALID_ARG;
    }
    if (!ptk_scheduler) {
        warn("scheduler not initialized");
        return PTK_ERR_INVALID_STATE;
    }
    debug("resuming threadlet %p", t);
    ptk_scheduler->current_threadlet = t;
    swapcontext(&ptk_scheduler->main_ctx, &t->ctx);
    return PTK_OK;
}


/**
 * @brief Yield execution from the current threadlet back to the scheduler.
 *
 * Switches context from the current threadlet to the scheduler's main context.
 *
 * @return ptk_err PTK_OK on success, error code otherwise.
 * @retval PTK_ERR_INVALID_STATE No current threadlet or scheduler not initialized.
 */
ptk_err threadlet_posix_yield(void)
{
    if (!ptk_scheduler || !ptk_scheduler->current_threadlet) {
        warn("no current threadlet or scheduler");
        return PTK_ERR_INVALID_STATE;
    }
    debug("yielding threadlet %p", ptk_scheduler->current_threadlet);
    swapcontext(&ptk_scheduler->current_threadlet->ctx, &ptk_scheduler->main_ctx);
    return PTK_OK;
}

/**
 * @brief Join a threadlet, waiting for it to finish execution.
 *
 * Repeatedly resumes the threadlet until it is finished, then frees its resources.
 *
 * @param t Pointer to the threadlet to join.
 * @param timeout_ms Timeout in milliseconds (currently unused).
 * @return ptk_err PTK_OK on success, error code otherwise.
 * @retval PTK_ERR_INVALID_ARG t is NULL.
 */
ptk_err threadlet_posix_join(threadlet_t *t, ptk_duration_ms timeout_ms)
{
    if (!t) {
        warn("null threadlet");
        return PTK_ERR_INVALID_ARG;
    }
    debug("joining threadlet %p", t);
    while (!t->finished) {
        threadlet_posix_resume(t);
    }
    ptk_free(t);
    debug("threadlet %p cleaned up", t);
    return PTK_OK;
}

/**
 * @brief Create and initialize a scheduler for the current thread.
 *
 * This must be called at the start of each event thread before creating threadlets.
 *
 * @return ptk_err PTK_OK on success, error code otherwise.
 * @retval PTK_ERR_INVALID_STATE Scheduler already exists for this thread.
 * @retval PTK_ERR_NO_MEMORY Allocation failure.
 */
ptk_err ptk_threadlet_posix_scheduler_create(void)
{
    if (ptk_scheduler) {
        warn("scheduler already exists for this thread");
        return PTK_ERR_INVALID_STATE;
    }
    ptk_scheduler = ptk_alloc(sizeof(scheduler_t), NULL);
    if (!ptk_scheduler) {
        warn("failed to allocate scheduler");
        return PTK_ERR_NO_MEMORY;
    }
    memset(&ptk_scheduler->main_ctx, 0, sizeof(ucontext_t));
    ptk_scheduler->current_threadlet = NULL;
    debug("scheduler created for thread");
    return PTK_OK;
}

