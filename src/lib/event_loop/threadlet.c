// src/lib/event_loop/ptk_threadlet.c
// Platform-agnostic threadlet API
#include "ptk_threadlet.h"
#if defined(_WIN32)
#include "platform/ptk_threadlet_win.h"
#else
#include "platform/ptk_threadlet_posix.h"
#endif

threadlet_t *ptk_threadlet_create(threadlet_run_func_t func, void *data) {
#if defined(_WIN32)
    return ptk_threadlet_win_create(func, data);
#else
    return ptk_threadlet_posix_create(func, data);
#endif
}

ptk_err ptk_threadlet_resume(threadlet_t *threadlet) {
#if defined(_WIN32)
    return ptk_threadlet_win_resume(threadlet);
#else
    return ptk_threadlet_posix_resume(threadlet);
#endif
}

ptk_err ptk_threadlet_yield(void) {
#if defined(_WIN32)
    return ptk_threadlet_win_yield();
#else
    return ptk_threadlet_posix_yield();
#endif
}

ptk_err ptk_threadlet_join(threadlet_t *threadlet, ptk_duration_ms timeout_ms) {
#if defined(_WIN32)
    return ptk_threadlet_win_join(threadlet, timeout_ms);
#else
    return threadlet_posix_join(threadlet, timeout_ms);
#endif
}
