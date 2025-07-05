#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ev_err.h"
#include "ev_utils.h"

/**
 * @file ev_threading.h
 * @brief Threading and synchronization primitives including threads, mutexes, and condition variables.
 */


/**
 * @brief Forward declaration of a mutex type.
 */
typedef struct ev_mutex ev_mutex;

/**
 * @brief Forward declaration of a condition variable type.
 */
typedef struct ev_cond_var ev_cond_var;

/**
 * @brief Forward declaration of a thread type.
 */
typedef struct ev_thread ev_thread;

//============================
// Mutex Functions
//============================

/**
 * @brief Creates a new recursive mutex.
 *
 * @param[out] mutex Pointer to the newly created mutex.
 * @return Error code indicating success or failure.
 */
extern ev_err ev_mutex_create(ev_mutex **mutex);

/**
 * @brief Destroys a mutex.
 *
 * @param mutex Pointer to the mutex to destroy.
 * @return Error code.
 */
extern ev_err ev_mutex_destroy(ev_mutex *mutex);

/**
 * @brief Attempts to lock the mutex, optionally waiting for a timeout.
 *
 * @param mutex Pointer to the mutex.
 * @param timeout_ms Timeout in milliseconds. Use EV_TIME_NO_WAIT or EV_TIME_WAIT_FOREVER for special cases.
 * @return Error code.
 */
extern ev_err ev_mutex_wait_lock(ev_mutex *mutex, ev_time_ms timeout_ms);

/**
 * @brief Unlocks the previously locked mutex.
 *
 * @param mutex Pointer to the mutex.
 * @return Error code.
 */
extern ev_err ev_mutex_unlock(ev_mutex *mutex);

//============================
// Condition Variable Functions
//============================

/**
 * @brief Creates a condition variable.
 *
 * @param[out] cond_var Pointer to the created condition variable.
 * @return Error code.
 */
extern ev_err ev_cond_var_create(ev_cond_var **cond_var);

/**
 * @brief Destroys a condition variable.
 *
 * @param cond_var Pointer to the condition variable.
 * @return Error code.
 */
extern ev_err ev_cond_var_destroy(ev_cond_var *cond_var);

/**
 * @brief Signals the condition variable, waking one waiting thread.
 *
 * @param cond_var Pointer to the condition variable.
 * @return Error code.
 */
extern ev_err ev_cond_var_signal(ev_cond_var *cond_var);

/**
 * @brief Waits for the condition variable to be signaled.
 *
 * @param cond_var Pointer to the condition variable.
 * @param mutex Pointer to an already locked mutex that will be released during the wait.
 * @param timeout_ms Timeout in milliseconds. Use THREAD_NO_WAIT or THREAD_WAIT_FOREVER for special cases.
 * @return Error code.
 */
extern ev_err ev_cond_var_wait(ev_cond_var *cond_var, ev_mutex *mutex, ev_time_ms timeout_ms);

//============================
// Thread Functions
//============================

/**
 * @brief Function type for thread entry points.
 *
 * @param data Pointer to user data passed to the thread.
 */
typedef void (*ev_thread_func)(void *data);

/**
 * @brief Creates and starts a new thread.
 *
 * @param[out] thread Pointer to the created thread handle.
 * @param func Entry point for the thread function.
 * @param data User data to pass to the thread function.
 * @return Error code.
 */
extern ev_err ev_thread_create(ev_thread **thread, ev_thread_func func, void *data);

/**
 * @brief Waits for the specified thread to complete.
 *
 * @param thread Thread to join.
 * @return Error code.
 */
extern ev_err ev_thread_join(ev_thread *thread);

/**
 * @brief Destroys a thread object.
 *
 * @param thread Thread to destroy.
 * @return Error code.
 */
extern ev_err ev_thread_destroy(ev_thread *thread);
