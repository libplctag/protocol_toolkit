#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ptk_utils.h"

typedef struct threadlet_t threadlet_t;

typedef void (*threadlet_run_func_t)(void *param);

// Threadlet status
typedef enum {
    THREADLET_READY,
    THREADLET_WAITING,
    THREADLET_TIMEOUT,
    THREADLET_FINISHED
} threadlet_status_t;

struct threadlet_t {
    threadlet_run_func_t entry;
    void *param;
    threadlet_status_t status;
    // ...platform-specific fields...
};

threadlet_t *threadlet_create(threadlet_run_func_t func, void *data);
void threadlet_resume(threadlet_t *threadlet);
void threadlet_yield(void);
void threadlet_destroy(threadlet_t *threadlet);
