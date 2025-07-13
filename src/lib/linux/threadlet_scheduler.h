#pragma once

#include "threadlet_core.h"
#include "linux_event_loop.h"
#include <ptk_array.h>
#include <ptk_shared.h>

typedef struct threadlet_queue_t threadlet_queue_t;

typedef struct {
    int fd;
    threadlet_t *waiting_threadlet;
    uint32_t events;
    ptk_time_ms deadline;
} event_registration_t;

typedef struct {
    ptk_shared_handle_t *registration_handles;  // Array of handles to individual registrations
    size_t registrations_capacity;
    size_t registrations_count;
} registrations_array_t;

typedef struct event_loop_t {
    platform_event_loop_t *platform;
    threadlet_queue_t *ready_queue;
    threadlet_queue_t *waiting_queue;
    ptk_shared_handle_t registrations_handle;
    bool running;
    ptk_time_ms current_time_ms;
} event_loop_t;

/**
 * @brief Create a new event loop for the current thread
 * @return Event loop handle or NULL on failure. Use ptk_get_err() for error details.
 */
event_loop_t *event_loop_create(void);

/**
 * @brief Get the thread-local event loop
 * @return Current thread's event loop or NULL if none initialized
 */
event_loop_t *get_thread_local_event_loop(void);

/**
 * @brief Run the event loop until stopped
 * @param loop Event loop to run
 * @return PTK_OK on success, error code on failure
 */
ptk_err event_loop_run(event_loop_t *loop);

/**
 * @brief Stop the event loop
 * @param loop Event loop to stop
 * @return PTK_OK on success, error code on failure
 */
ptk_err event_loop_stop(event_loop_t *loop);

/**
 * @brief Register a threadlet to wait for I/O events
 * @param loop Event loop
 * @param fd File descriptor to wait on
 * @param events Events to wait for
 * @param threadlet Threadlet to resume when ready
 * @param timeout_ms Timeout in milliseconds
 * @return PTK_OK on success, error code on failure
 */
ptk_err event_loop_register_io(event_loop_t *loop, int fd, uint32_t events, 
                              threadlet_t *threadlet, ptk_duration_ms timeout_ms);

/**
 * @brief Unregister a file descriptor from the event loop
 * @param loop Event loop
 * @param fd File descriptor to unregister
 * @return PTK_OK on success, error code on failure
 */
ptk_err event_loop_unregister_io(event_loop_t *loop, int fd);

/**
 * @brief Add a threadlet to the ready queue
 * @param loop Event loop
 * @param threadlet Threadlet to enqueue
 * @return PTK_OK on success, error code on failure
 */
ptk_err event_loop_enqueue_ready(event_loop_t *loop, threadlet_t *threadlet);

PTK_ARRAY_DECLARE(threadlet_ptr, threadlet_t*);

struct threadlet_queue_t {
    threadlet_ptr_array_t *threadlets;
    size_t head;
    size_t tail;
};