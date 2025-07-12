// src/lib/event_loop/platform/linux_event_loop.h

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <ptk_err.h>

// Forward declaration for platform event loop
typedef struct linux_event_loop_t linux_event_loop_t;

// Opaque handle for platform event loop
typedef struct platform_event_loop_t {
    linux_event_loop_t *impl;
} platform_event_loop_t;

// Event types
#define PTK_EVENT_READ   (1 << 0)
#define PTK_EVENT_WRITE  (1 << 1)
#define PTK_EVENT_ERROR  (1 << 2)

// Event structure returned by poll
typedef struct {
    int fd;
    uint32_t events; // PTK_EVENT_READ | PTK_EVENT_WRITE | PTK_EVENT_ERROR
} platform_event_t;

typedef struct {
    platform_event_t *events;
    int count;
} platform_event_list_t;

// Create a new epoll-based event loop
platform_event_loop_t *platform_event_loop_create(int max_events);

// Destroy the event loop and free resources
void platform_event_loop_destroy(platform_event_loop_t *loop);

// Register a file descriptor for read events
ptk_err platform_add_fd_read(platform_event_loop_t *loop, int fd);

// Register a file descriptor for write events
ptk_err platform_add_fd_write(platform_event_loop_t *loop, int fd);

// Remove a file descriptor from the event loop
ptk_err platform_remove_fd(platform_event_loop_t *loop, int fd);

// Poll for events, returns number of ready events
// timeout_ms: -1 for infinite, 0 for non-blocking, >0 for timeout
int platform_poll_events(platform_event_loop_t *loop, platform_event_list_t *out_events, int timeout_ms);

// Wake up the event loop (for cross-thread signaling)
ptk_err platform_event_loop_wake(platform_event_loop_t *loop);
