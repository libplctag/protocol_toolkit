
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
#include <ptk_defs.h>



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
 * @brief Thread signal types for unified signaling API (bitflags)
 */
typedef enum {
    // Bottom 8 signals are all variations on ABORT (0x00 - 0xFF)
    PTK_THREAD_SIGNAL_ABORT = (1 << 0),        // Request graceful shutdown  
    PTK_THREAD_SIGNAL_TERMINATE = (1 << 1),    // Force immediate termination
    PTK_THREAD_SIGNAL_ABORT_MASK = 0xFF,       // Mask for all abort-type signals
    
    // Non-aborting signals (0x100 and above)
    PTK_THREAD_SIGNAL_WAKE = (1 << 8),         // General wake-up signal
    PTK_THREAD_SIGNAL_CHILD_DIED = (1 << 9),   // Child death notification (automatic)

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
 * Thread functions receive no arguments. Use ptk_thread_get_arg_*() functions
 * to retrieve arguments that were added before the thread was started.
 * Use ptk_thread_self() to get the current thread's handle if needed.
 */
typedef void (*ptk_thread_func)(void);

/**
 * @brief Create a thread handle (does not start execution).
 *
 * After creating a thread, use ptk_thread_add_*_arg() functions to add arguments,
 * ptk_thread_set_run_function() to set the entry point, and ptk_thread_start()
 * to begin execution.
 *
 * @return Thread handle for the new thread, or PTK_SHARED_INVALID_HANDLE on failure
 */
PTK_API extern ptk_thread_handle_t ptk_thread_create(void);

/**
 * @brief Set the thread's run function.
 *
 * @param thread Thread handle
 * @param func Entry point for the thread function
 * @return PTK_OK on success, error code on failure
 */
PTK_API extern ptk_err_t ptk_thread_set_run_function(ptk_thread_handle_t thread, ptk_thread_func func);

/**
 * @brief Start the thread (executes the run function with the provided arguments).
 *
 * @param thread Thread handle
 * @return PTK_OK on success, error code on failure
 */
PTK_API extern ptk_err_t ptk_thread_start(ptk_thread_handle_t thread);

//============================
// Thread Argument Functions
//============================

/**
 * @brief Add a pointer argument (user provides type value).
 *
 * The function takes the address of the pointer and nulls it after adding.
 * This is an ownership transfer mechanism.
 *
 * @param thread Thread handle
 * @param user_type User-defined type identifier for this argument
 * @param ptr Pointer to the pointer to add (will be nulled after adding)
 * @return PTK_OK on success, error code on failure
 */
PTK_API extern ptk_err_t ptk_thread_add_ptr_arg(ptk_thread_handle_t thread, int user_type, void **ptr);

/**
 * @brief Add an unsigned integer argument.
 *
 * @param thread Thread handle
 * @param user_type User-defined type identifier for this argument
 * @param val Value to add
 * @return PTK_OK on success, error code on failure
 */
PTK_API extern ptk_err_t ptk_thread_add_uint_arg(ptk_thread_handle_t thread, int user_type, uint64_t val);

/**
 * @brief Add a signed integer argument.
 *
 * @param thread Thread handle
 * @param user_type User-defined type identifier for this argument
 * @param val Value to add
 * @return PTK_OK on success, error code on failure
 */
PTK_API extern ptk_err_t ptk_thread_add_int_arg(ptk_thread_handle_t thread, int user_type, int64_t val);

/**
 * @brief Add a floating-point argument (double precision).
 *
 * @param thread Thread handle
 * @param user_type User-defined type identifier for this argument
 * @param val Value to add
 * @return PTK_OK on success, error code on failure
 */
PTK_API extern ptk_err_t ptk_thread_add_float_arg(ptk_thread_handle_t thread, int user_type, double val);

/**
 * @brief Add a shared handle argument.
 *
 * The function takes the address of the handle and nulls it after adding.
 * This is an ownership transfer mechanism.
 *
 * @param thread Thread handle
 * @param user_type User-defined type identifier for this argument
 * @param handle Pointer to the handle to add (will be nulled after adding)
 * @return PTK_OK on success, error code on failure
 */
PTK_API extern ptk_err_t ptk_thread_add_handle_arg(ptk_thread_handle_t thread, int user_type, ptk_shared_handle_t *handle);

//============================
// Thread Argument Retrieval Functions (only valid in running thread)
//============================

/**
 * @brief Get the number of arguments passed to the thread.
 *
 * Only valid when called from within the running thread.
 *
 * @return Number of arguments, or 0 if called from outside the thread
 */
PTK_API extern size_t ptk_thread_get_arg_count(void);

/**
 * @brief Get the user-provided type value for the argument at index.
 *
 * Only valid when called from within the running thread.
 *
 * @param index Argument index (0-based)
 * @return User-defined type value, or 0 if invalid index or called from outside the thread
 */
PTK_API extern int ptk_thread_get_arg_type(size_t index);

/**
 * @brief Get a pointer argument by index.
 *
 * Only valid when called from within the running thread.
 *
 * @param index Argument index (0-based)
 * @return Pointer value, or NULL if invalid index or called from outside the thread
 */
PTK_API extern void *ptk_thread_get_ptr_arg(size_t index);

/**
 * @brief Get an unsigned integer argument by index.
 *
 * Only valid when called from within the running thread.
 *
 * @param index Argument index (0-based)
 * @return Value, or 0 if invalid index or called from outside the thread
 */
PTK_API extern uint64_t ptk_thread_get_uint_arg(size_t index);

/**
 * @brief Get a signed integer argument by index.
 *
 * Only valid when called from within the running thread.
 *
 * @param index Argument index (0-based)
 * @return Value, or 0 if invalid index or called from outside the thread
 */
PTK_API extern int64_t ptk_thread_get_int_arg(size_t index);

/**
 * @brief Get a floating-point argument by index.
 *
 * Only valid when called from within the running thread.
 *
 * @param index Argument index (0-based)
 * @return Value, or 0.0 if invalid index or called from outside the thread
 */
PTK_API extern double ptk_thread_get_float_arg(size_t index);

/**
 * @brief Get a shared handle argument by index.
 *
 * Only valid when called from within the running thread.
 *
 * @param index Argument index (0-based)
 * @return Handle value, or PTK_SHARED_INVALID_HANDLE if invalid index or called from outside the thread
 */
PTK_API extern ptk_shared_handle_t ptk_thread_get_handle_arg(size_t index);

/**
 * @brief Get the current thread's handle.
 *
 * @return Handle for the calling thread
 */
PTK_API extern ptk_thread_handle_t ptk_thread_self(void);

/**
 * @brief Wait for signals or timeout (calling thread waits for itself).
 *
 * The calling thread waits until signaled or timeout occurs. Socket operations
 * (like ptk_tcp_accept) also wake up on any signal for responsive server loops.
 * This function must be interruptible by thread signals.
 *
 * @param timeout_ms Timeout in milliseconds 
 * @return PTK_OK on timeout, PTK_ERR_SIGNAL if signaled (check ptk_thread_get_pending_signals())
 */
PTK_API extern ptk_err_t ptk_thread_wait(ptk_time_ms timeout_ms);

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
PTK_API extern ptk_err_t ptk_thread_signal(ptk_thread_handle_t handle, ptk_thread_signal_t signal_type);

/**
 * @brief Get all signals currently pending for the calling thread.
 *
 * Call this after ptk_thread_wait() returns PTK_SIGNAL to determine what signals were received.
 *
 * @return Bitflags of all pending signals, or 0 if none
 */
PTK_API extern uint64_t ptk_thread_get_pending_signals(void);

/**
 * @brief Check if a specific signal is pending for the calling thread.
 *
 * @param signal_bit The signal bit to check (e.g., PTK_THREAD_SIGNAL_ABORT)
 * @return true if the signal is pending, false otherwise
 */
PTK_API extern bool ptk_thread_has_signal(ptk_thread_signal_t signal_bit);

/**
 * @brief Clear specific signals for the calling thread.
 *
 * @param signal_mask Bitflags of signals to clear
 */
PTK_API extern void ptk_thread_clear_signals(uint64_t signal_mask);

//============================
// Internal Functions (for socket system)
//============================

/**
 * @brief Get current thread's epoll file descriptor (internal use only).
 *
 * @return Epoll file descriptor, or -1 if not available
 */
PTK_API extern int ptk_thread_get_epoll_fd(void);

/**
 * @brief Get current thread's signal file descriptor (internal use only).
 *
 * @return Signal file descriptor, or -1 if not available
 */
PTK_API extern int ptk_thread_get_signal_fd(void);

//============================
// Parent-Child Thread Management
//============================

/**
 * @brief Get the parent thread handle.
 *
 * @param thread Thread handle to query (use ptk_thread_self() for current thread)
 * @return Parent thread handle, or PTK_THREAD_NO_PARENT if no parent
 */
PTK_API extern ptk_thread_handle_t ptk_thread_get_parent(ptk_thread_handle_t thread);

/**
 * @brief Count the number of child threads.
 *
 * Child threads are automatically tracked. This returns the current count.
 *
 * @param parent Parent thread handle
 * @return Number of active child threads
 */
PTK_API extern int ptk_thread_count_children(ptk_thread_handle_t parent);

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
PTK_API extern ptk_err_t ptk_thread_signal_all_children(ptk_thread_handle_t parent, ptk_thread_signal_t signal_type);

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
PTK_API extern ptk_err_t ptk_thread_cleanup_dead_children(ptk_thread_handle_t parent, ptk_time_ms timeout_ms);

