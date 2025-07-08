#include "ptk_thread.h"
#include "ptk_alloc.h"
#include <errno.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <process.h>
    #include <windows.h>
#else
    #include <pthread.h>
    #include <sys/time.h>
    #include <time.h>
    #include <unistd.h>
#endif

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

#ifdef _WIN32
struct ptk_mutex {
    ptk_allocator_t *allocator;
    CRITICAL_SECTION cs;
};

struct ptk_cond_var {
    ptk_allocator_t *allocator;
    CONDITION_VARIABLE cv;
};

struct ptk_thread {
    ptk_allocator_t *allocator;
    HANDLE handle;
    ptk_thread_func func;
    void *data;
};

static unsigned __stdcall thread_wrapper(void *arg) {
    ptk_thread *thread = (ptk_thread *)arg;
    thread->func(thread->data);
    return 0;
}

#else
struct ptk_mutex {
    ptk_allocator_t *allocator;
    pthread_mutex_t mutex;
};

struct ptk_cond_var {
    ptk_allocator_t *allocator;
    pthread_cond_t cond;
};

struct ptk_thread {
    ptk_allocator_t *allocator;
    pthread_t thread;
    ptk_thread_func func;
    void *data;
};

static void *thread_wrapper(void *arg) {
    ptk_thread *thread = (ptk_thread *)arg;
    thread->func(thread->data);
    return NULL;
}
#endif

//=============================================================================
// MUTEX FUNCTIONS
//=============================================================================

ptk_mutex *ptk_mutex_create(ptk_allocator_t *allocator) {
    if(!allocator) { return NULL; }

    ptk_mutex *m = ptk_alloc(allocator, sizeof(ptk_mutex));
    if(!m) { return NULL; }

    m->allocator = allocator;

#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
#else
    pthread_mutexattr_t attr;
    if(pthread_mutexattr_init(&attr) != 0) {
        ptk_free(allocator, m);
        return NULL;
    }

    // Make mutex recursive
    if(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        pthread_mutexattr_destroy(&attr);
        ptk_free(allocator, m);
        return NULL;
    }

    if(pthread_mutex_init(&m->mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        ptk_free(allocator, m);
        return NULL;
    }

    pthread_mutexattr_destroy(&attr);
#endif

    return m;
}

ptk_err ptk_mutex_destroy(ptk_mutex *mutex) {
    if(!mutex) { return PTK_ERR_NULL_PTR; }

    ptk_allocator_t *allocator = mutex->allocator;

#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif

    ptk_free(allocator, mutex);
    return PTK_OK;
}

ptk_err ptk_mutex_wait_lock(ptk_mutex *mutex, ptk_time_ms timeout_ms) {
    if(!mutex) { return PTK_ERR_NULL_PTR; }

#ifdef _WIN32
    if(timeout_ms == PTK_TIME_NO_WAIT) {
        if(TryEnterCriticalSection(&mutex->cs)) {
            return PTK_OK;
        } else {
            return PTK_ERR_WOULD_BLOCK;
        }
    } else {
        // Windows critical sections don't support timeouts
        // For simplicity, we'll just block indefinitely
        EnterCriticalSection(&mutex->cs);
        return PTK_OK;
    }
#else
    if(timeout_ms == PTK_TIME_NO_WAIT) {
        int result = pthread_mutex_trylock(&mutex->mutex);
        if(result == 0) {
            return PTK_OK;
        } else if(result == EBUSY) {
            return PTK_ERR_WOULD_BLOCK;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    } else if(timeout_ms == PTK_TIME_WAIT_FOREVER) {
        if(pthread_mutex_lock(&mutex->mutex) == 0) {
            return PTK_OK;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    } else {
        // Timed lock - fallback implementation for macOS
        // Try lock first
        int result = pthread_mutex_trylock(&mutex->mutex);
        if(result == 0) {
            return PTK_OK;
        } else if(result != EBUSY) {
            return PTK_ERR_CONFIGURATION_ERROR;
        }

        // If busy, sleep and retry until timeout
        struct timeval start, now;
        gettimeofday(&start, NULL);

        while(true) {
            usleep(1000);  // Sleep 1ms

            result = pthread_mutex_trylock(&mutex->mutex);
            if(result == 0) {
                return PTK_OK;
            } else if(result != EBUSY) {
                return PTK_ERR_CONFIGURATION_ERROR;
            }

            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
            if(elapsed >= timeout_ms) { return PTK_ERR_TIMEOUT; }
        }
    }
#endif
}

ptk_err ptk_mutex_unlock(ptk_mutex *mutex) {
    if(!mutex) { return PTK_ERR_NULL_PTR; }

#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    if(pthread_mutex_unlock(&mutex->mutex) != 0) { return PTK_ERR_CONFIGURATION_ERROR; }
#endif

    return PTK_OK;
}

//=============================================================================
// CONDITION VARIABLE FUNCTIONS
//=============================================================================

ptk_cond_var *ptk_cond_var_create(ptk_allocator_t *allocator) {
    if(!allocator) { return NULL; }

    ptk_cond_var *cv = ptk_alloc(allocator, sizeof(ptk_cond_var));
    if(!cv) { return NULL; }

    cv->allocator = allocator;

#ifdef _WIN32
    InitializeConditionVariable(&cv->cv);
#else
    if(pthread_cond_init(&cv->cond, NULL) != 0) {
        ptk_free(allocator, cv);
        return NULL;
    }
#endif

    return cv;
}

ptk_err ptk_cond_var_destroy(ptk_cond_var *cond_var) {
    if(!cond_var) { return PTK_ERR_NULL_PTR; }

    ptk_allocator_t *allocator = cond_var->allocator;

#ifdef _WIN32
    // Windows condition variables don't need explicit cleanup
#else
    pthread_cond_destroy(&cond_var->cond);
#endif

    ptk_free(allocator, cond_var);
    return PTK_OK;
}

ptk_err ptk_cond_var_signal(ptk_cond_var *cond_var) {
    if(!cond_var) { return PTK_ERR_NULL_PTR; }

#ifdef _WIN32
    WakeConditionVariable(&cond_var->cv);
#else
    if(pthread_cond_signal(&cond_var->cond) != 0) { return PTK_ERR_CONFIGURATION_ERROR; }
#endif

    return PTK_OK;
}

ptk_err ptk_cond_var_wait(ptk_cond_var *cond_var, ptk_mutex *mutex, ptk_time_ms timeout_ms) {
    if(!cond_var || !mutex) { return PTK_ERR_NULL_PTR; }

#ifdef _WIN32
    DWORD wait_time = (timeout_ms == PTK_TIME_WAIT_FOREVER) ? INFINITE : (DWORD)timeout_ms;
    if(SleepConditionVariableCS(&cond_var->cv, &mutex->cs, wait_time)) {
        return PTK_OK;
    } else {
        DWORD error = GetLastError();
        if(error == ERROR_TIMEOUT) {
            return PTK_ERR_TIMEOUT;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    }
#else
    if(timeout_ms == PTK_TIME_WAIT_FOREVER) {
        if(pthread_cond_wait(&cond_var->cond, &mutex->mutex) == 0) {
            return PTK_OK;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    } else {
        struct timespec abs_timeout;
        struct timeval now;
        gettimeofday(&now, NULL);

        abs_timeout.tv_sec = now.tv_sec + (timeout_ms / 1000);
        abs_timeout.tv_nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;

        // Handle nanosecond overflow
        if(abs_timeout.tv_nsec >= 1000000000) {
            abs_timeout.tv_sec++;
            abs_timeout.tv_nsec -= 1000000000;
        }

        int result = pthread_cond_timedwait(&cond_var->cond, &mutex->mutex, &abs_timeout);
        if(result == 0) {
            return PTK_OK;
        } else if(result == ETIMEDOUT) {
            return PTK_ERR_TIMEOUT;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    }
#endif
}

//=============================================================================
// THREAD FUNCTIONS
//=============================================================================

ptk_thread *ptk_thread_create(ptk_allocator_t *allocator, ptk_thread_func func, void *data) {
    if(!allocator || !func) { return NULL; }

    ptk_thread *t = ptk_alloc(allocator, sizeof(ptk_thread));
    if(!t) { return NULL; }

    t->allocator = allocator;
    t->func = func;
    t->data = data;

#ifdef _WIN32
    t->handle = (HANDLE)_beginthreadex(NULL, 0, thread_wrapper, t, 0, NULL);
    if(t->handle == 0) {
        ptk_free(allocator, t);
        return NULL;
    }
#else
    if(pthread_create(&t->thread, NULL, thread_wrapper, t) != 0) {
        ptk_free(allocator, t);
        return NULL;
    }
#endif

    return t;
}

ptk_err ptk_thread_join(ptk_thread *thread) {
    if(!thread) { return PTK_ERR_NULL_PTR; }

#ifdef _WIN32
    if(WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0) { return PTK_ERR_CONFIGURATION_ERROR; }
#else
    if(pthread_join(thread->thread, NULL) != 0) { return PTK_ERR_CONFIGURATION_ERROR; }
#endif

    return PTK_OK;
}

ptk_err ptk_thread_destroy(ptk_thread *thread) {
    if(!thread) { return PTK_ERR_NULL_PTR; }

    ptk_allocator_t *allocator = thread->allocator;

#ifdef _WIN32
    CloseHandle(thread->handle);
#else
    // pthread_t doesn't need explicit cleanup after join
#endif

    ptk_free(allocator, thread);
    return PTK_OK;
}