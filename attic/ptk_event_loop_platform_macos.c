#include "../ptk_event_loop_platform.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * Example macOS/BSD (kqueue/kevent) implementation for event loop platform abstraction.
 * This is a minimal stub; for real usage, track event sources, FDs, etc.
 */

struct ptk_event_loop_platform {
    int kq;
    // Optionally: array/list of sources, mapping FDs to event sources, etc.
};

ptk_event_loop_platform_t* ptk_event_loop_platform_create(void) {
    ptk_event_loop_platform_t* loop = calloc(1, sizeof(ptk_event_loop_platform_t));
    if (!loop) return NULL;
    loop->kq = kqueue();
    if (loop->kq < 0) {
        free(loop);
        return NULL;
    }
    return loop;
}

void ptk_event_loop_platform_destroy(ptk_event_loop_platform_t* loop) {
    if (!loop) return;
    if (loop->kq >= 0) close(loop->kq);
    free(loop);
}

ptk_err ptk_event_loop_platform_register(ptk_event_loop_platform_t* loop, ptk_event_source_t* source) {
    // Example: register a socket or timer fd for read/write events
    // For real code, you'll need to keep track of FDs/types/context.
    if (!loop || !source || !source->platform_handle) return PTK_ERR_INVALID;
    int fd = *(int*)(source->platform_handle);

    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, source);
    if (kevent(loop->kq, &kev, 1, NULL, 0, NULL) == -1) {
        return PTK_ERR_SYSTEM;
    }
    return PTK_OK;
}

ptk_err ptk_event_loop_platform_unregister(ptk_event_loop_platform_t* loop, ptk_event_source_t* source) {
    if (!loop || !source || !source->platform_handle) return PTK_ERR_INVALID;
    int fd = *(int*)(source->platform_handle);

    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_DELETE, 0, 0, source);
    if (kevent(loop->kq, &kev, 1, NULL, 0, NULL) == -1) {
        return PTK_ERR_SYSTEM;
    }
    return PTK_OK;
}

int ptk_event_loop_platform_wait(ptk_event_loop_platform_t* loop, ptk_event_source_t** sources_out, int max_sources, uint32_t timeout_ms) {
    if (!loop || !sources_out || max_sources <= 0) return -1;
    struct timespec ts;
    struct timespec* pts = NULL;
    if (timeout_ms > 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        pts = &ts;
    }

    struct kevent events[max_sources];
    int nev = kevent(loop->kq, NULL, 0, events, max_sources, pts);
    if (nev < 0) return -1; // error
    if (nev == 0) return 0; // timeout

    for (int i = 0; i < nev; ++i) {
        // Use udata field to get the event source pointer (see register/unregister)
        sources_out[i] = (ptk_event_source_t*)events[i].udata;
    }
    return nev;
}
