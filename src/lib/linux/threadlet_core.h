#pragma once

#include <ucontext.h>
#include <ptk_threadlet.h>
#include <ptk_utils.h>
#include <ptk_err.h>
#include "ptk_platform.h"

typedef enum {
    THREADLET_CREATED,
    THREADLET_READY,
    THREADLET_RUNNING,
    THREADLET_WAITING,
    THREADLET_FINISHED
} threadlet_status_t;

struct threadlet_t {
    ucontext_t context;
    void *stack;
    threadlet_run_func_t entry_func;
    void *user_data;
    threadlet_status_t status;
    int waiting_fd;
    uint32_t waiting_events;
    ptk_time_ms deadline;
    bool finished;
};

/**
 * @brief Create a new threadlet with 64KiB stack
 * @param func Entry point function
 * @param data User parameter
 * @return New threadlet or NULL on failure. Use ptk_get_err() for error details.
 */
threadlet_t *threadlet_create_internal(threadlet_run_func_t func, void *data);

/**
 * @brief Get the currently running threadlet
 * @return Current threadlet or NULL if not in threadlet context
 */
threadlet_t *threadlet_get_current(void);

/**
 * @brief Set the current threadlet (for context switching)
 * @param threadlet Threadlet to set as current
 */
void threadlet_set_current(threadlet_t *threadlet);

/**
 * @brief Get threadlet status
 * @param threadlet Threadlet to query
 * @return Current status
 */
threadlet_status_t threadlet_get_status(const threadlet_t *threadlet);

/**
 * @brief Set threadlet status
 * @param threadlet Threadlet to modify
 * @param status New status
 */
void threadlet_set_status(threadlet_t *threadlet, threadlet_status_t status);

/**
 * @brief Context switch from current threadlet to scheduler
 * @param threadlet Current threadlet
 */
void threadlet_yield_to_scheduler(threadlet_t *threadlet);

/**
 * @brief Resume execution of a threadlet
 * @param threadlet Threadlet to resume
 */
void threadlet_resume_execution(threadlet_t *threadlet);