
#pragma once

/**
 * @file ptk_os_thread.h
 * @brief Threading and synchronization primitives for Protocol Toolkit
 *
 * This header provides cross-platform abstractions for threads, mutexes, and condition variables.
 * Use these APIs to create and manage threads, synchronize access to shared resources, and coordinate
 * between threads using condition variables. All types are opaque and managed by the library.
 *
 * Supported features:
 *   - Thread creation and joining
 *   - Recursive mutexes (lock/unlock)
 *   - Condition variables (wait/signal)
 *   - Thread-local storage macro
 *
 * Usage example:
 *   ptk_mutex *m = ptk_mutex_create();
 *   ptk_thread *t = ptk_thread_create(my_func, my_data);
 *   ptk_thread_join(t);
 *   ptk_mutex_wait_lock(m, PTK_TIME_WAIT_FOREVER);
 *   ptk_mutex_unlock(m);
 *
 * See function documentation for details.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
// Forward declarations to break circular dependencies
typedef enum ptk_err ptk_err;
typedef struct ptk_shared_handle ptk_shared_handle_t;
typedef int64_t ptk_time_ms;

// Timeout constants (from ptk_utils.h)
#define PTK_TIME_WAIT_FOREVER (INT64_MAX)
#define PTK_TIME_NO_WAIT (INT64_MIN)

#if defined(_WIN32) && defined(_MSC_VER)
#define ptk_thread_local __declspec(thread)
#else
#define ptk_thread_local __thread
#endif


/* Mutex and condition variable types removed from public API.
 * Use shared memory handles for synchronization instead:
 * - Data protection: use_shared() macro provides automatic synchronization
 * - Thread communication: ptk_thread_signal() and ptk_thread_wait()
 * - OS-native mutexes used internally by shared memory implementation only
 */

/**
 * @brief Thread handle type (uses shared memory for safe cross-thread access)
 */
typedef ptk_shared_handle_t ptk_thread_handle_t;

/**
 * @brief No parent constant for root threads
 */
#define PTK_THREAD_NO_PARENT PTK_SHARED_INVALID_HANDLE

/**
 * @brief Thread signal types for unified signaling API
 */
typedef enum {
    PTK_THREAD_SIGNAL_WAKEUP,      // General wake-up signal
    PTK_THREAD_SIGNAL_ABORT,       // Request graceful shutdown  
    PTK_THREAD_SIGNAL_TERMINATE,   // Force immediate termination
    PTK_THREAD_SIGNAL_CHILD_DIED   // Child death notification (automatic)
} ptk_thread_signal_t;

/* Public mutex and condition variable APIs removed.
 * Synchronization handled by shared memory system:
 * 
 * Instead of mutexes, use shared memory access patterns:
 *   use_shared(data_handle, timeout, my_data_t *data) {
 *       // Automatically synchronized access to data
 *   } on_shared_fail {
 *       // Handle timeout or failure
 *   }
 *
 * Instead of condition variables, use thread signals:
 *   ptk_thread_signal(worker_thread);  // Wake up waiting thread
 *   ptk_thread_wait(handle, timeout);  // Wait for signal
 */

//============================
// Thread Functions
//============================

/**
 * @brief Function type for thread entry points.
 *
 * @param data Shared memory handle containing user data for the thread.
 */
typedef void (*ptk_thread_func)(ptk_shared_handle_t data);

/**
 * @brief Creates and starts a new thread with parent-child relationship.
 *
 * Child threads automatically register with parent and signal parent when they die.
 * Use PTK_THREAD_NO_PARENT for root threads.
 *
 * @param parent Parent thread handle, or PTK_THREAD_NO_PARENT for root threads
 * @param func Entry point for the thread function
 * @param data User data to pass to the thread function (shared memory handle)
 * @return Thread handle for the new thread, or PTK_SHARED_INVALID_HANDLE on failure
 */
extern ptk_thread_handle_t ptk_thread_create(ptk_thread_handle_t parent,
                                             ptk_thread_func func, 
                                             ptk_shared_handle_t data);

/**
 * @brief Get the current thread's handle.
 *
 * @return Handle for the calling thread
 */
extern ptk_thread_handle_t ptk_thread_self(void);

/**
 * @brief Wait for signals or timeout (calling thread waits for itself).
 *
 * The calling thread waits until signaled or timeout occurs. Socket operations
 * (like ptk_tcp_accept) also wake up on any signal for responsive server loops.
 *
 * @param timeout_ms Timeout in milliseconds 
 * @return PTK_OK on timeout, PTK_SIGNAL if signaled (check ptk_thread_get_last_signal())
 */
extern ptk_err ptk_thread_wait(ptk_time_ms timeout_ms);

/**
 * @brief Send a signal to a thread.
 *
 * Signals cause the target thread to wake up from ptk_thread_wait() or socket
 * operations like ptk_tcp_accept(). The thread can check the signal type.
 *
 * @param handle Thread handle to signal
 * @param signal_type Type of signal to send
 * @return Error code
 */
extern ptk_err ptk_thread_signal(ptk_thread_handle_t handle, ptk_thread_signal_t signal_type);

/**
 * @brief Get the last signal received by the calling thread.
 *
 * Call this after ptk_thread_wait() returns PTK_SIGNAL to determine what signal was received.
 *
 * @return The last signal type received, or PTK_THREAD_SIGNAL_WAKEUP if none
 */
extern ptk_thread_signal_t ptk_thread_get_last_signal(void);

//============================
// Parent-Child Thread Management
//============================

/**
 * @brief Get the parent thread handle.
 *
 * @param thread Thread handle to query (use ptk_thread_self() for current thread)
 * @return Parent thread handle, or PTK_THREAD_NO_PARENT if no parent
 */
extern ptk_thread_handle_t ptk_thread_get_parent(ptk_thread_handle_t thread);

/**
 * @brief Count the number of child threads.
 *
 * Child threads are automatically tracked. This returns the current count.
 *
 * @param parent Parent thread handle
 * @return Number of active child threads
 */
extern int ptk_thread_count_children(ptk_thread_handle_t parent);

/**
 * @brief Signal all child threads.
 *
 * Sends the same signal to all children of the specified parent thread.
 * Useful for coordinated shutdown or other broadcast operations.
 *
 * @param parent Parent thread handle
 * @param signal_type Signal to send to all children
 * @return Error code
 */
extern ptk_err ptk_thread_signal_all_children(ptk_thread_handle_t parent, ptk_thread_signal_t signal_type);

/**
 * @brief Clean up dead child threads.
 *
 * Attempts to release handles for child threads that have died. Uses timeout
 * to avoid blocking. Call with PTK_TIME_NO_WAIT for immediate cleanup only.
 *
 * @param parent Parent thread handle
 * @param timeout_ms Maximum time to wait for cleanup
 * @return Error code
 */
extern ptk_err ptk_thread_cleanup_dead_children(ptk_thread_handle_t parent, ptk_time_ms timeout_ms);

