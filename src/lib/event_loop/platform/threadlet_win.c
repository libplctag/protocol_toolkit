// src/lib/event_loop/platform/ptk_threadlet_win.c
#include "ptk_threadlet_win.h"
#include <Windows.h>
#include <stdlib.h>

struct threadlet_t {
    LPVOID fiber;
    threadlet_run_func_t entry;
    void *param;
    bool finished;
};

static threadlet_t *current_threadlet = NULL;
static LPVOID main_fiber = NULL;

static void __stdcall threadlet_trampoline(void *param) {
    threadlet_t *t = (threadlet_t *)param;
    t->entry(t->param);
    t->finished = true;
    ptk_threadlet_win_yield();
}

threadlet_t *ptk_threadlet_win_create(threadlet_run_func_t func, void *param) {
    if (!main_fiber) main_fiber = ConvertThreadToFiber(NULL);
    threadlet_t *t = calloc(1, sizeof(threadlet_t));
    t->entry = func;
    t->param = param;
    t->fiber = CreateFiber(64 * 1024, threadlet_trampoline, t);
    return t;
}

ptk_err ptk_threadlet_win_resume(threadlet_t *t) {
    current_threadlet = t;
    SwitchToFiber(t->fiber);
    return 0;
}

ptk_err ptk_threadlet_win_yield(void) {
    SwitchToFiber(main_fiber);
    return 0;
}

ptk_err ptk_threadlet_win_join(threadlet_t *t, ptk_duration_ms timeout_ms) {
    while (!t->finished) {
        ptk_threadlet_win_resume(t);
    }
    DeleteFiber(t->fiber);
    free(t);
    return 0;
}
