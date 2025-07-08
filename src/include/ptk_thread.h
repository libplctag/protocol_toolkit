#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ptk_err.h"
#include "ptk_utils.h"
#include "ptk_alloc.h"

/**
 * @file ptk_threading.h
 * @brief Threading and synchronization primitives including threads, mutexes, and condition variables.
 */

 #if defined(_WIN32) && defined(_MSC_VER)
#define ptk_thread_local __declspec(thread)
#else
#define ptk_thread_local __thread
#endif


/**
 * @brief Forward declaration of a mutex type.
 */
typedef struct ptk_mutex ptk_mutex;

/**
 * @brief Forward declaration of a condition variable type.
 */
typedef struct ptk_cond_var ptk_cond_var;

/**
 * @brief Forward declaration of a thread type.
 */
typedef struct ptk_thread ptk_thread;

//============================
// Mutex Functions
//============================

/**
 * @brief Creates a new recursive mutex.
 *
 * @param allocator The allocator to use for creating the mutex
 * @return Pointer to the newly created mutex, or NULL on failure
 */
extern ptk_mutex *ptk_mutex_create(ptk_allocator_t *allocator);

/**
 * @brief Destroys a mutex.
 *
 * @param mutex Pointer to the mutex to destroy.
 * @return Error code.
 */
extern ptk_err ptk_mutex_destroy(ptk_mutex *mutex);

/**
 * @brief Attempts to lock the mutex, optionally waiting for a timeout.
 *
 * @param mutex Pointer to the mutex.
 * @param timeout_ms Timeout in milliseconds. Use PTK_TIME_NO_WAIT or PTK_TIME_WAIT_FOREVER for special cases.
 * @return Error code.
 */
extern ptk_err ptk_mutex_wait_lock(ptk_mutex *mutex, ptk_time_ms timeout_ms);

/**
 * @brief Unlocks the previously locked mutex.
 *
 * @param mutex Pointer to the mutex.
 * @return Error code.
 */
extern ptk_err ptk_mutex_unlock(ptk_mutex *mutex);

//============================
// Condition Variable Functions
//============================

/**
 * @brief Creates a condition variable.
 *
 * @param allocator The allocator to use for creating the condition variable
 * @return Pointer to the created condition variable, or NULL on failure
 */
extern ptk_cond_var *ptk_cond_var_create(ptk_allocator_t *allocator);

/**
 * @brief Destroys a condition variable.
 *
 * @param cond_var Pointer to the condition variable.
 * @return Error code.
 */
extern ptk_err ptk_cond_var_destroy(ptk_cond_var *cond_var);

/**
 * @brief Signals the condition variable, waking one waiting thread.
 *
 * @param cond_var Pointer to the condition variable.
 * @return Error code.
 */
extern ptk_err ptk_cond_var_signal(ptk_cond_var *cond_var);

/**
 * @brief Waits for the condition variable to be signaled.
 *
 * @param cond_var Pointer to the condition variable.
 * @param mutex Pointer to an already locked mutex that will be released during the wait.
 * @param timeout_ms Timeout in milliseconds. Use THREAD_NO_WAIT or THREAD_WAIT_FOREVER for special cases.
 * @return Error code.
 */
extern ptk_err ptk_cond_var_wait(ptk_cond_var *cond_var, ptk_mutex *mutex, ptk_time_ms timeout_ms);

//============================
// Thread Functions
//============================

/**
 * @brief Function type for thread entry points.
 *
 * @param data Pointer to user data passed to the thread.
 */
typedef void (*ptk_thread_func)(void *data);

/**
 * @brief Creates and starts a new thread.
 *
 * @param allocator The allocator to use for creating the thread
 * @param func Entry point for the thread function.
 * @param data User data to pass to the thread function.
 * @return Pointer to the created thread handle, or NULL on failure
 */
extern ptk_thread *ptk_thread_create(ptk_allocator_t *allocator, ptk_thread_func func, void *data);

/**
 * @brief Waits for the specified thread to complete.
 *
 * @param thread Thread to join.
 * @return Error code.
 */
extern ptk_err ptk_thread_join(ptk_thread *thread);

/**
 * @brief Destroys a thread object.
 *
 * @param thread Thread to destroy.
 * @return Error code.
 */
extern ptk_err ptk_thread_destroy(ptk_thread *thread);
