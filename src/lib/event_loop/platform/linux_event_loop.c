// src/lib/event_loop/platform/linux_event_loop.c

#include "linux_event_loop.h"
#include "../../include/ptk_alloc.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/eventfd.h>


#define MAX_EVENTS 1024

struct linux_event_loop_t {
    int epoll_fd;
    int max_events;
    struct epoll_event *events;
    int wake_fd; // eventfd for cross-thread wakeup
};

static void linux_event_loop_destructor(void *ptr) {
    linux_event_loop_t *loop = (linux_event_loop_t *)ptr;
    if (!loop) return;
    close(loop->epoll_fd);
    close(loop->wake_fd);
    if (loop->events) ptk_free(loop->events);
}

static void set_epoll_event(struct epoll_event *ev, int fd, uint32_t events) {
    ev->data.fd = fd;
    ev->events = 0;
    if (events & PTK_EVENT_READ)  ev->events |= EPOLLIN;
    if (events & PTK_EVENT_WRITE) ev->events |= EPOLLOUT;
    if (events & PTK_EVENT_ERROR) ev->events |= EPOLLERR;
}

// Create a new platform event loop with a specified maximum number of events.
/**
 * @brief Create and initialize a new Linux epoll-based event loop.
 *
 * This function allocates and sets up the internal epoll instance and eventfd for wakeup.
 * The event buffer size is fixed at MAX_EVENTS events.
 *
 * @param max_events Ignored; buffer size is fixed at MAX_EVENTS.
 * @return Pointer to a new platform_event_loop_t handle, or NULL on failure.
 */
platform_event_loop_t *platform_event_loop_create(int max_events) {
    debug("entry");
    linux_event_loop_t *loop = ptk_alloc(sizeof(linux_event_loop_t), linux_event_loop_destructor);
    if (!loop) {
        warn("Failed to allocate event loop");
        return NULL;
    }
    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0) {
        warn("epoll_create1 failed: %s", strerror(errno));
        ptk_free(loop);
        return NULL;
    }
    loop->max_events = MAX_EVENTS; // Fixed size as requested
    loop->events = ptk_alloc(loop->max_events * sizeof(struct epoll_event), NULL);
    if (!loop->events) {
        warn("Failed to allocate epoll events buffer");
        close(loop->epoll_fd);
        ptk_free(loop);
        return NULL;
    }
    // Create eventfd for wakeup
    loop->wake_fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
    if (loop->wake_fd < 0) {
        warn("eventfd failed: %s", strerror(errno));
        close(loop->epoll_fd);
        ptk_free(loop->events);
        ptk_free(loop);
        return NULL;
    }
    struct epoll_event ev;
    set_epoll_event(&ev, loop->wake_fd, EPOLLIN);
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->wake_fd, &ev) < 0) {
        warn("epoll_ctl add wake_fd failed: %s", strerror(errno));
        close(loop->wake_fd);
        close(loop->epoll_fd);
        ptk_free(loop->events);
        ptk_free(loop);
        return NULL;
    }
    platform_event_loop_t *handle = ptk_alloc(sizeof(platform_event_loop_t), NULL);
    handle->impl = loop;
    handle->max_events = loop->max_events;
    debug("exit");
    return handle;
}

// Destroy the platform event loop and free associated resources.
/**
 * @brief Destroy the platform event loop and free associated resources.
 *
 * This function releases all memory and closes all file descriptors associated with the event loop.
 *
 * @param handle Pointer to the platform_event_loop_t to destroy.
 */
void platform_event_loop_destroy(platform_event_loop_t *handle) {
    debug("entry");
    if (!handle) return;
    linux_event_loop_t *loop = handle->impl;
    if (loop) {
        ptk_free(loop);
    }
    ptk_free(handle);
    debug("exit");
}

// Add a file descriptor for specific events to the event loop.
/**
 * @brief Add a file descriptor for specific events to the event loop.
 *
 * Registers the given fd for the specified events. If the fd is already present, modifies its event mask.
 *
 * @param handle Event loop handle.
 * @param fd File descriptor to monitor.
 * @param events Event mask (PTK_EVENT_READ | PTK_EVENT_WRITE | PTK_EVENT_ERROR).
 * @return PTK_OK on success, error code otherwise.
 */
ptk_err platform_add_fd(platform_event_loop_t *handle, int fd, uint32_t events) {
    debug("fd=%d, events=0x%x", fd, events);
    
    if (!handle || !handle->impl || fd < 0) {
        warn("Invalid arguments");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    linux_event_loop_t *loop = handle->impl;
    struct epoll_event ev;
    set_epoll_event(&ev, fd, events);
    
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        if (errno == EEXIST) {
            debug("fd %d already exists, modifying", fd);
            if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                warn("epoll_ctl mod failed for fd %d: %s", fd, strerror(errno));
                return PTK_ERR_NETWORK_ERROR;
            }
        } else {
            warn("epoll_ctl add failed for fd %d: %s", fd, strerror(errno));
            return PTK_ERR_NETWORK_ERROR;
        }
    }
    
    debug("fd %d added/modified with events 0x%x", fd, events);
    return PTK_OK;
}

// Add a file descriptor for read events to the event loop.
/**
 * @brief Add a file descriptor for read events to the event loop.
 *
 * Convenience function that calls platform_add_fd with PTK_EVENT_READ.
 *
 * @param handle Event loop handle.
 * @param fd File descriptor to monitor for read events.
 * @return PTK_OK on success, error code otherwise.
 */

// Add a file descriptor for write events to the event loop.
/**
 * @brief Add a file descriptor for write events to the event loop.
 *
 * Registers the given fd for EPOLLOUT events. If the fd is already present, modifies its event mask.
 *
 * @param handle Event loop handle.
 * @param fd File descriptor to monitor for write events.
 * @return PTK_OK on success, error code otherwise.
 */
// Add a file descriptor for write events to the event loop.
/**
 * @brief Add a file descriptor for write events to the event loop.
 *
 * Convenience function that calls platform_add_fd with PTK_EVENT_WRITE.
 *
 * @param handle Event loop handle.
 * @param fd File descriptor to monitor for write events.
 * @return PTK_OK on success, error code otherwise.
 */

// Remove a file descriptor from the event loop.
/**
 * @brief Remove a file descriptor from the event loop.
 *
 * Unregisters the given fd from the epoll instance.
 *
 * @param handle Event loop handle.
 * @param fd File descriptor to remove.
 * @return PTK_OK on success, error code otherwise.
 */
ptk_err platform_remove_fd(platform_event_loop_t *handle, int fd) {
    debug("fd=%d", fd);
    linux_event_loop_t *loop = handle->impl;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        warn("epoll_ctl del failed for fd %d: %s", fd, strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    debug("fd %d removed", fd);
    return PTK_OK;
}

// Poll for events on the file descriptors in the event loop.
/**
 * @brief Poll for events on the file descriptors in the event loop.
 *
 * Waits for events on registered file descriptors. Fills out_events with ready fds and their event masks.
 * Handles wakeup via eventfd.
 *
 * @param handle Event loop handle.
 * @param out_events Pointer to event list to fill.
 * @param timeout_ms Timeout in milliseconds for polling.
 * @return Number of events, or negative error code.
 */
int platform_poll_events(platform_event_loop_t *handle, platform_event_list_t *out_events, int timeout_ms) {
    debug("entry");
    linux_event_loop_t *loop = handle->impl;
    int n = epoll_wait(loop->epoll_fd, loop->events, loop->max_events, timeout_ms);
    if (n < 0) {
        warn("epoll_wait failed: %s", strerror(errno));
        return n;
    }
    int out_count = 0;
    for (int i = 0; i < n; ++i) {
        int fd = loop->events[i].data.fd;
        uint32_t events = 0;
        if (loop->events[i].events & EPOLLIN)  events |= PTK_EVENT_READ;
        if (loop->events[i].events & EPOLLOUT) events |= PTK_EVENT_WRITE;
        if (loop->events[i].events & EPOLLERR) events |= PTK_EVENT_ERROR;
        // Ignore wake fd
        if (fd == loop->wake_fd) {
            uint64_t val;
            read(loop->wake_fd, &val, sizeof(val));
            debug("wake_fd triggered");
            continue;
        }
        out_events->events[out_count].fd = fd;
        out_events->events[out_count].events = events;
        out_count++;
    }
    out_events->count = out_count;
    debug("exit, %d events", out_count);
    return out_count;
}

// Wake the event loop to process events.
/**
 * @brief Wake the event loop to process events.
 *
 * Signals the eventfd to interrupt epoll_wait and wake the event loop thread.
 *
 * @param handle Event loop handle.
 * @return PTK_OK on success, error code otherwise.
 */
ptk_err platform_event_loop_wake(platform_event_loop_t *handle) {
    debug("entry");
    linux_event_loop_t *loop = handle->impl;
    uint64_t val = 1;
    if (write(loop->wake_fd, &val, sizeof(val)) < 0) {
        warn("write to wake_fd failed: %s", strerror(errno));
        return PTK_ERR_DEVICE_FAILURE;
    }
    debug("exit");
    return PTK_OK;
}
