#include "../ptk_event_loop_platform.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

/*
 * Linux (epoll) implementation for platform event loop abstraction.
 * Supports registering/unregistering file descriptors and waiting for events.
 *
 * Note: This implementation assumes that ptk_event_source->platform_handle points to an int containing the FD.
 * It also assumes all events are for read (EPOLLIN). Extend as needed for write, timer, etc.
 */

#define PTK_EPOLL_MAX_EVENTS 64

struct ptk_event_loop_platform {
    int epoll_fd;
    // Optionally: you could add a map from FD to ptk_event_source_t* for fast lookup if needed.
};

ptk_event_loop_platform_t* ptk_event_loop_platform_create(void) {
    ptk_event_loop_platform_t* loop = calloc(1, sizeof(ptk_event_loop_platform_t));
    if (!loop) return NULL;
    loop->epoll_fd = epoll_create1(0);
    if (loop->epoll_fd < 0) {
        free(loop);
        return NULL;
    }
    return loop;
}

void ptk_event_loop_platform_destroy(ptk_event_loop_platform_t* loop) {
    if (!loop) return;
    if (loop->epoll_fd >= 0) close(loop->epoll_fd);
    free(loop);
}

ptk_err ptk_event_loop_platform_register(ptk_event_loop_platform_t* loop, ptk_event_source_t* source) {
    if (!loop || !source || !source->platform_handle) return PTK_ERR_INVALID;
    int fd = *(int*)(source->platform_handle);

    struct epoll_event event;
    event.events = EPOLLIN; // For now: monitor for input. Extend for other types as needed.
    event.data.ptr = source; // Store pointer to source for event lookup.

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
        return PTK_ERR_SYSTEM;
    }
    return PTK_OK;
}

ptk_err ptk_event_loop_platform_unregister(ptk_event_loop_platform_t* loop, ptk_event_source_t* source) {
    if (!loop || !source || !source->platform_handle) return PTK_ERR_INVALID;
    int fd = *(int*)(source->platform_handle);

    // epoll_ctl: remove the FD. event parameter can be NULL for EPOLL_CTL_DEL.
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        return PTK_ERR_SYSTEM;
    }
    return PTK_OK;
}

int ptk_event_loop_platform_wait(ptk_event_loop_platform_t* loop, ptk_event_source_t** sources_out, int max_sources, uint32_t timeout_ms) {
    if (!loop || !sources_out || max_sources <= 0) return -1;
    struct epoll_event events[PTK_EPOLL_MAX_EVENTS];
    int n = epoll_wait(loop->epoll_fd, events, (max_sources < PTK_EPOLL_MAX_EVENTS) ? max_sources : PTK_EPOLL_MAX_EVENTS, timeout_ms);
    if (n < 0) {
        // Interrupted by signal? Could handle EINTR here if needed.
        if (errno == EINTR) return 0;
        return -1;
    }
    // Fill output array with event sources that have events.
    for (int i = 0; i < n; ++i) {
        sources_out[i] = (ptk_event_source_t*)events[i].data.ptr;
    }
    return n;
}

