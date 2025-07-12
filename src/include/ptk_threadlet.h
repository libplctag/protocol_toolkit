#pragma once

/**
 * @file ptk_threadlet.h
 * @brief Green thread (threadlet) API for cooperative multitasking
 *
 * This module provides an implementation of green threads (asymmetric coroutines)
 * that work cooperatively with the PTK socket API to provide the illusion of 
 * blocking socket calls without blocking OS threads.
 *
 * Architecture:
 * - Multiple OS threads each run their own scheduler
 * - Each threadlet is bound to one OS thread and cannot migrate
 * - Newly created threadlets are distributed across threads
 * - Schedulers run on top of platform-specific event loops (epoll, kqueue, IOCP)
 */

#include <ptk_err.h>
#include <ptk_utils.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// TYPES
//=============================================================================

/**
 * @brief Opaque threadlet handle
 */
typedef struct threadlet_t threadlet_t;

/**
 * @brief Threadlet entry point function
 * @param param User-provided parameter passed to ptk_threadlet_create()
 */
typedef void (*threadlet_run_func_t)(void *param);

//=============================================================================
// THREADLET LIFECYCLE
//=============================================================================

/**
 * @brief Create a new threadlet
 * 
 * Creates a new threadlet but does not schedule it for execution.
 * The threadlet must be started with ptk_threadlet_resume().
 *
 * @param func Entry point function for the threadlet
 * @param data User parameter passed to the entry function
 * @return New threadlet handle, or NULL on failure
 */
threadlet_t *ptk_threadlet_create(threadlet_run_func_t func, void *data);

/**
 * @brief Wait for a threadlet to complete and clean up its resources
 * 
 * Blocks the calling threadlet until the target threadlet exits.
 * This function cleans up all resources associated with the threadlet.
 *
 * @param threadlet The threadlet to wait for
 * @param timeout_ms Maximum time to wait in milliseconds (0 for infinite)
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, other error codes on failure
 */
ptk_err ptk_threadlet_join(threadlet_t *threadlet, ptk_duration_ms timeout_ms);

//=============================================================================
// THREADLET SCHEDULING
//=============================================================================

/**
 * @brief Suspend the current threadlet
 * 
 * Suspends the currently running threadlet and removes it from the scheduler.
 * The threadlet can be resumed later with ptk_threadlet_resume().
 * This function must be called from within a threadlet context.
 *
 * @return PTK_OK on successful yield, error code on failure
 */
ptk_err ptk_threadlet_yield(void);

/**
 * @brief Schedule a threadlet for execution
 * 
 * Adds the threadlet to a scheduler queue. The scheduler will pick an
 * OS thread with the least load and assign the threadlet to that thread.
 * Once assigned, the threadlet cannot migrate to another OS thread.
 *
 * @param threadlet The threadlet to schedule
 * @return PTK_OK on successful scheduling, error code on failure
 */
ptk_err ptk_threadlet_resume(threadlet_t *threadlet);

#ifdef __cplusplus
}
#endif

