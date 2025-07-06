#include "ptk_thread.h"
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#endif

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

#ifdef _WIN32
struct ptk_mutex {
    CRITICAL_SECTION cs;
};

struct ptk_cond_var {
    CONDITION_VARIABLE cv;
};

struct ptk_thread {
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
    pthread_mutex_t mutex;
};

struct ptk_cond_var {
    pthread_cond_t cond;
};

struct ptk_thread {
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

ptk_err ptk_mutex_create(ptk_mutex **mutex) {
    if (!mutex) {
        return PTK_ERR_NULL_PTR;
    }
    
    ptk_mutex *m = malloc(sizeof(ptk_mutex));
    if (!m) {
        return PTK_ERR_NO_RESOURCES;
    }
    
#ifdef _WIN32
    InitializeCriticalSection(&m->cs);
#else
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        free(m);
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    
    // Make mutex recursive
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(m);
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    
    if (pthread_mutex_init(&m->mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(m);
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    
    pthread_mutexattr_destroy(&attr);
#endif
    
    *mutex = m;
    return PTK_OK;
}

ptk_err ptk_mutex_destroy(ptk_mutex *mutex) {
    if (!mutex) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
#endif
    
    free(mutex);
    return PTK_OK;
}

ptk_err ptk_mutex_wait_lock(ptk_mutex *mutex, ptk_time_ms timeout_ms) {
    if (!mutex) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    if (timeout_ms == PTK_TIME_NO_WAIT) {
        if (TryEnterCriticalSection(&mutex->cs)) {
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
    if (timeout_ms == PTK_TIME_NO_WAIT) {
        int result = pthread_mutex_trylock(&mutex->mutex);
        if (result == 0) {
            return PTK_OK;
        } else if (result == EBUSY) {
            return PTK_ERR_WOULD_BLOCK;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    } else if (timeout_ms == PTK_TIME_WAIT_FOREVER) {
        if (pthread_mutex_lock(&mutex->mutex) == 0) {
            return PTK_OK;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    } else {
        // Timed lock - fallback implementation for macOS
        // Try lock first
        int result = pthread_mutex_trylock(&mutex->mutex);
        if (result == 0) {
            return PTK_OK;
        } else if (result != EBUSY) {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
        
        // If busy, sleep and retry until timeout
        struct timeval start, now;
        gettimeofday(&start, NULL);
        
        while (true) {
            usleep(1000); // Sleep 1ms
            
            result = pthread_mutex_trylock(&mutex->mutex);
            if (result == 0) {
                return PTK_OK;
            } else if (result != EBUSY) {
                return PTK_ERR_CONFIGURATION_ERROR;
            }
            
            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 + 
                          (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms) {
                return PTK_ERR_TIMEOUT;
            }
        }
    }
#endif
}

ptk_err ptk_mutex_unlock(ptk_mutex *mutex) {
    if (!mutex) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    LeaveCriticalSection(&mutex->cs);
#else
    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PTK_ERR_CONFIGURATION_ERROR;
    }
#endif
    
    return PTK_OK;
}

//=============================================================================
// CONDITION VARIABLE FUNCTIONS
//=============================================================================

ptk_err ptk_cond_var_create(ptk_cond_var **cond_var) {
    if (!cond_var) {
        return PTK_ERR_NULL_PTR;
    }
    
    ptk_cond_var *cv = malloc(sizeof(ptk_cond_var));
    if (!cv) {
        return PTK_ERR_NO_RESOURCES;
    }
    
#ifdef _WIN32
    InitializeConditionVariable(&cv->cv);
#else
    if (pthread_cond_init(&cv->cond, NULL) != 0) {
        free(cv);
        return PTK_ERR_CONFIGURATION_ERROR;
    }
#endif
    
    *cond_var = cv;
    return PTK_OK;
}

ptk_err ptk_cond_var_destroy(ptk_cond_var *cond_var) {
    if (!cond_var) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    // Windows condition variables don't need explicit cleanup
#else
    pthread_cond_destroy(&cond_var->cond);
#endif
    
    free(cond_var);
    return PTK_OK;
}

ptk_err ptk_cond_var_signal(ptk_cond_var *cond_var) {
    if (!cond_var) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    WakeConditionVariable(&cond_var->cv);
#else
    if (pthread_cond_signal(&cond_var->cond) != 0) {
        return PTK_ERR_CONFIGURATION_ERROR;
    }
#endif
    
    return PTK_OK;
}

ptk_err ptk_cond_var_wait(ptk_cond_var *cond_var, ptk_mutex *mutex, ptk_time_ms timeout_ms) {
    if (!cond_var || !mutex) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    DWORD wait_time = (timeout_ms == PTK_TIME_WAIT_FOREVER) ? INFINITE : (DWORD)timeout_ms;
    if (SleepConditionVariableCS(&cond_var->cv, &mutex->cs, wait_time)) {
        return PTK_OK;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_TIMEOUT) {
            return PTK_ERR_TIMEOUT;
        } else {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
    }
#else
    if (timeout_ms == PTK_TIME_WAIT_FOREVER) {
        if (pthread_cond_wait(&cond_var->cond, &mutex->mutex) == 0) {
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
        if (abs_timeout.tv_nsec >= 1000000000) {
            abs_timeout.tv_sec++;
            abs_timeout.tv_nsec -= 1000000000;
        }
        
        int result = pthread_cond_timedwait(&cond_var->cond, &mutex->mutex, &abs_timeout);
        if (result == 0) {
            return PTK_OK;
        } else if (result == ETIMEDOUT) {
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

ptk_err ptk_thread_create(ptk_thread **thread, ptk_thread_func func, void *data) {
    if (!thread || !func) {
        return PTK_ERR_NULL_PTR;
    }
    
    ptk_thread *t = malloc(sizeof(ptk_thread));
    if (!t) {
        return PTK_ERR_NO_RESOURCES;
    }
    
    t->func = func;
    t->data = data;
    
#ifdef _WIN32
    t->handle = (HANDLE)_beginthreadex(NULL, 0, thread_wrapper, t, 0, NULL);
    if (t->handle == 0) {
        free(t);
        return PTK_ERR_CONFIGURATION_ERROR;
    }
#else
    if (pthread_create(&t->thread, NULL, thread_wrapper, t) != 0) {
        free(t);
        return PTK_ERR_CONFIGURATION_ERROR;
    }
#endif
    
    *thread = t;
    return PTK_OK;
}

ptk_err ptk_thread_join(ptk_thread *thread) {
    if (!thread) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    if (WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0) {
        return PTK_ERR_CONFIGURATION_ERROR;
    }
#else
    if (pthread_join(thread->thread, NULL) != 0) {
        return PTK_ERR_CONFIGURATION_ERROR;
    }
#endif
    
    return PTK_OK;
}

ptk_err ptk_thread_destroy(ptk_thread *thread) {
    if (!thread) {
        return PTK_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    CloseHandle(thread->handle);
#else
    // pthread_t doesn't need explicit cleanup after join
#endif
    
    free(thread);
    return PTK_OK;
}