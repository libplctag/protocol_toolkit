#include "linux_event_loop.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_err.h>

#define MAX_EVENTS 1024

struct linux_event_loop_t {
    int epoll_fd;
    int max_events;
    struct epoll_event *events;
    int wake_fd;
};

static void linux_event_loop_destructor(void *ptr) {
    linux_event_loop_t *loop = (linux_event_loop_t *)ptr;
    if (!loop) return;
    
    info("Destroying Linux event loop");
    
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
    if (loop->wake_fd >= 0) {
        close(loop->wake_fd);
    }
    if (loop->events) {
        ptk_free(&loop->events);
    }
}

static void set_epoll_event(struct epoll_event *ev, int fd, uint32_t events) {
    ev->data.fd = fd;
    ev->events = 0;
    if (events & PTK_EVENT_READ)  ev->events |= EPOLLIN;
    if (events & PTK_EVENT_WRITE) ev->events |= EPOLLOUT;
    if (events & PTK_EVENT_ERROR) ev->events |= EPOLLERR;
}

platform_event_loop_t *platform_event_loop_create(int max_events) {
    info("Creating Linux epoll event loop with max_events=%d", max_events);
    
    linux_event_loop_t *loop = ptk_alloc(sizeof(linux_event_loop_t), linux_event_loop_destructor);
    if (!loop) {
        error("Failed to allocate event loop");
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        error("epoll_create1 failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_DEVICE_FAILURE);
        ptk_free(&loop);
        return NULL;
    }
    
    loop->max_events = (max_events > 0 && max_events < MAX_EVENTS) ? max_events : MAX_EVENTS;
    loop->events = ptk_alloc(loop->max_events * sizeof(struct epoll_event), NULL);
    if (!loop->events) {
        error("Failed to allocate epoll events buffer");
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        close(loop->epoll_fd);
        ptk_free(&loop);
        return NULL;
    }
    
    loop->wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (loop->wake_fd < 0) {
        error("eventfd failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_DEVICE_FAILURE);
        close(loop->epoll_fd);
        ptk_free(&loop->events);
        ptk_free(&loop);
        return NULL;
    }
    
    struct epoll_event ev;
    set_epoll_event(&ev, loop->wake_fd, PTK_EVENT_READ);
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, loop->wake_fd, &ev) < 0) {
        error("epoll_ctl add wake_fd failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_DEVICE_FAILURE);
        close(loop->wake_fd);
        close(loop->epoll_fd);
        ptk_free(&loop->events);
        ptk_free(&loop);
        return NULL;
    }
    
    platform_event_loop_t *handle = ptk_alloc(sizeof(platform_event_loop_t), NULL);
    if (!handle) {
        error("Failed to allocate event loop handle");
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        ptk_free(&loop);
        return NULL;
    }
    
    handle->impl = loop;
    handle->max_events = loop->max_events;
    
    info("Linux event loop created successfully");
    return handle;
}

ptk_err platform_add_fd(platform_event_loop_t *handle, int fd, uint32_t events) {
    debug("Adding fd=%d with events=0x%x", fd, events);
    
    if (!handle || !handle->impl || fd < 0) {
        warn("Invalid arguments to platform_add_fd");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    linux_event_loop_t *loop = handle->impl;
    struct epoll_event ev;
    set_epoll_event(&ev, fd, events);
    
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        if (errno == EEXIST) {
            debug("fd %d already exists, modifying", fd);
            if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                error("epoll_ctl mod failed for fd %d: %s", fd, strerror(errno));
                ptk_set_err(PTK_ERR_NETWORK_ERROR);
                return PTK_ERR_NETWORK_ERROR;
            }
        } else {
            error("epoll_ctl add failed for fd %d: %s", fd, strerror(errno));
            ptk_set_err(PTK_ERR_NETWORK_ERROR);
            return PTK_ERR_NETWORK_ERROR;
        }
    }
    
    debug("fd %d added/modified with events 0x%x", fd, events);
    return PTK_OK;
}

ptk_err platform_remove_fd(platform_event_loop_t *handle, int fd) {
    debug("Removing fd=%d", fd);
    
    if (!handle || !handle->impl || fd < 0) {
        warn("Invalid arguments to platform_remove_fd");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    linux_event_loop_t *loop = handle->impl;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        error("epoll_ctl del failed for fd %d: %s", fd, strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return PTK_ERR_NETWORK_ERROR;
    }
    
    debug("fd %d removed", fd);
    return PTK_OK;
}

int platform_poll_events(platform_event_loop_t *handle, platform_event_list_t *out_events, int timeout_ms) {
    trace("Polling with timeout=%d ms", timeout_ms);
    
    if (!handle || !handle->impl || !out_events) {
        warn("Invalid arguments to platform_poll_events");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return -1;
    }
    
    linux_event_loop_t *loop = handle->impl;
    int n = epoll_wait(loop->epoll_fd, loop->events, loop->max_events, timeout_ms);
    if (n < 0) {
        if (errno == EINTR) {
            debug("epoll_wait interrupted by signal");
            return 0;
        }
        error("epoll_wait failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_DEVICE_FAILURE);
        return -1;
    }
    
    int out_count = 0;
    for (int i = 0; i < n && out_count < out_events->capacity; ++i) {
        int fd = loop->events[i].data.fd;
        uint32_t events = 0;
        
        if (loop->events[i].events & EPOLLIN)  events |= PTK_EVENT_READ;
        if (loop->events[i].events & EPOLLOUT) events |= PTK_EVENT_WRITE;
        if (loop->events[i].events & EPOLLERR) events |= PTK_EVENT_ERROR;
        if (loop->events[i].events & EPOLLHUP) events |= PTK_EVENT_ERROR;
        
        if (fd == loop->wake_fd) {
            uint64_t val;
            ssize_t result = read(loop->wake_fd, &val, sizeof(val));
            (void)result;
            debug("Wake event received");
            continue;
        }
        
        out_events->events[out_count].fd = fd;
        out_events->events[out_count].events = events;
        out_count++;
    }
    
    out_events->count = out_count;
    trace("Poll returned %d events", out_count);
    return out_count;
}

ptk_err platform_event_loop_wake(platform_event_loop_t *handle) {
    debug("Waking event loop");
    
    if (!handle || !handle->impl) {
        warn("Invalid arguments to platform_event_loop_wake");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    linux_event_loop_t *loop = handle->impl;
    uint64_t val = 1;
    if (write(loop->wake_fd, &val, sizeof(val)) < 0) {
        error("write to wake_fd failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_DEVICE_FAILURE);
        return PTK_ERR_DEVICE_FAILURE;
    }
    
    debug("Event loop wake signal sent");
    return PTK_OK;
}