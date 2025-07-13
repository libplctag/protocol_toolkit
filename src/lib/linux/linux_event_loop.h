#pragma once

#include "ptk_platform.h"
#include <ptk_err.h>
#include <ptk_utils.h>

typedef struct linux_event_loop_t linux_event_loop_t;

typedef struct platform_event_loop_t {
    linux_event_loop_t *impl;
    int max_events;
} platform_event_loop_t;

/**
 * @brief Create a new epoll-based event loop
 * @param max_events Maximum number of events to handle per poll
 * @return Event loop handle or NULL on failure. Use ptk_get_err() for error details.
 */
platform_event_loop_t *platform_event_loop_create(int max_events);

/**
 * @brief Add a file descriptor for specific events
 * @param loop Event loop handle
 * @param fd File descriptor to monitor
 * @param events Event mask (PTK_EVENT_READ | PTK_EVENT_WRITE | PTK_EVENT_ERROR)
 * @return PTK_OK on success, error code on failure
 */
ptk_err platform_add_fd(platform_event_loop_t *loop, int fd, uint32_t events);

/**
 * @brief Remove a file descriptor from the event loop
 * @param loop Event loop handle
 * @param fd File descriptor to remove
 * @return PTK_OK on success, error code on failure
 */
ptk_err platform_remove_fd(platform_event_loop_t *loop, int fd);

/**
 * @brief Poll for events
 * @param loop Event loop handle
 * @param out_events Output event list to fill
 * @param timeout_ms Timeout in milliseconds (-1 for infinite, 0 for non-blocking)
 * @return Number of events on success, negative error code on failure
 */
int platform_poll_events(platform_event_loop_t *loop, platform_event_list_t *out_events, int timeout_ms);

/**
 * @brief Wake up the event loop from another thread
 * @param loop Event loop handle
 * @return PTK_OK on success, error code on failure
 */
ptk_err platform_event_loop_wake(platform_event_loop_t *loop);