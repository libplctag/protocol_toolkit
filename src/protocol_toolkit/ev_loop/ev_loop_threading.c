/**
 * @file ev_loop_threading.c
 * @brief Cross-platform threading, synchronization and atomic operations
 */

// Enable POSIX features for pthread_mutex_timedlock
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "ev_loop.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

// Check for C11 atomics availability
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #include <stdatomic.h>
    #define HAS_C11_ATOMICS 1
#else
    #define HAS_C11_ATOMICS 0
#endif

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #include <signal.h>
#else
    #include <pthread.h>
    #include <signal.h>
    #include <sys/time.h>
    #include <errno.h>
    #include <time.h>
    #if defined(__APPLE__) || defined(__MACH__)
        #include <mach/mach_time.h>
    #endif
#endif

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

#ifdef _WIN32
struct ev_mutex {
    CRITICAL_SECTION cs;
    DWORD owner_thread;
    int lock_count;
};

struct ev_cond_var {
    CONDITION_VARIABLE cv;
};

struct ev_thread {
    HANDLE handle;
    DWORD thread_id;
    ev_thread_func func;
    void *data;
};

#else // POSIX (Linux, macOS, BSD)
struct ev_mutex {
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
};

struct ev_cond_var {
    pthread_cond_t cond;
};

struct ev_thread {
    pthread_t thread;
    ev_thread_func func;
    void *data;
};
#endif

//=============================================================================
// CONSTANT DEFINITIONS
//=============================================================================

const size_t THREAD_WAIT_FOREVER = SIZE_MAX;
const size_t THREAD_NO_WAIT = 0;

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

#ifdef _WIN32
static uint64_t get_time_ms(void) {
    return GetTickCount64();
}
#else
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
#endif

//=============================================================================
// MUTEX IMPLEMENTATION
//=============================================================================

ev_err ev_mutex_create(ev_mutex **mutex) {
    trace("Creating mutex");
    
    if (!mutex) {
        return EV_ERR_NULL_PTR;
    }
    
    *mutex = calloc(1, sizeof(ev_mutex));
    if (!*mutex) {
        error("Failed to allocate mutex");
        return EV_ERR_NO_RESOURCES;
    }
    
#ifdef _WIN32
    InitializeCriticalSection(&(*mutex)->cs);
    (*mutex)->owner_thread = 0;
    (*mutex)->lock_count = 0;
#else
    // Create recursive mutex attributes
    if (pthread_mutexattr_init(&(*mutex)->attr) != 0) {
        error("Failed to initialize mutex attributes");
        free(*mutex);
        *mutex = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
    
    if (pthread_mutexattr_settype(&(*mutex)->attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        error("Failed to set mutex type to recursive");
        pthread_mutexattr_destroy(&(*mutex)->attr);
        free(*mutex);
        *mutex = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
    
    if (pthread_mutex_init(&(*mutex)->mutex, &(*mutex)->attr) != 0) {
        error("Failed to initialize mutex");
        pthread_mutexattr_destroy(&(*mutex)->attr);
        free(*mutex);
        *mutex = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
#endif
    
    trace("Mutex created successfully");
    return EV_OK;
}

ev_err ev_mutex_destroy(ev_mutex *mutex) {
    trace("Destroying mutex");
    
    if (!mutex) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    DeleteCriticalSection(&mutex->cs);
#else
    pthread_mutex_destroy(&mutex->mutex);
    pthread_mutexattr_destroy(&mutex->attr);
#endif
    
    free(mutex);
    trace("Mutex destroyed");
    return EV_OK;
}

ev_err ev_mutex_wait_lock(ev_mutex *mutex, size_t timeout_ms) {
    trace("Waiting to lock mutex with timeout %zu ms", timeout_ms);
    
    if (!mutex) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    if (timeout_ms == THREAD_NO_WAIT) {
        // Try to lock immediately
        if (TryEnterCriticalSection(&mutex->cs)) {
            mutex->owner_thread = GetCurrentThreadId();
            mutex->lock_count++;
            trace("Mutex locked immediately");
            return EV_OK;
        } else {
            trace("Mutex try lock failed");
            return EV_ERR_WOULD_BLOCK;
        }
    } else if (timeout_ms == THREAD_WAIT_FOREVER) {
        // Wait forever
        EnterCriticalSection(&mutex->cs);
        mutex->owner_thread = GetCurrentThreadId();
        mutex->lock_count++;
        trace("Mutex locked (waited forever)");
        return EV_OK;
    } else {
        // Windows doesn't have a direct timeout for critical sections
        // We'll implement a polling approach
        uint64_t start_time = get_time_ms();
        uint64_t end_time = start_time + timeout_ms;
        
        while (get_time_ms() < end_time) {
            if (TryEnterCriticalSection(&mutex->cs)) {
                mutex->owner_thread = GetCurrentThreadId();
                mutex->lock_count++;
                trace("Mutex locked within timeout");
                return EV_OK;
            }
            Sleep(1); // Sleep for 1ms
        }
        
        trace("Mutex lock timeout");
        return EV_ERR_TIMEOUT;
    }
#else
    if (timeout_ms == THREAD_NO_WAIT) {
        // Try to lock immediately
        int result = pthread_mutex_trylock(&mutex->mutex);
        if (result == 0) {
            trace("Mutex locked immediately");
            return EV_OK;
        } else if (result == EBUSY) {
            trace("Mutex try lock failed");
            return EV_ERR_WOULD_BLOCK;
        } else {
            error("pthread_mutex_trylock failed: %s", strerror(result));
            return EV_ERR_NETWORK_ERROR;
        }
    } else if (timeout_ms == THREAD_WAIT_FOREVER) {
        // Wait forever
        int result = pthread_mutex_lock(&mutex->mutex);
        if (result == 0) {
            trace("Mutex locked (waited forever)");
            return EV_OK;
        } else {
            error("pthread_mutex_lock failed: %s", strerror(result));
            return EV_ERR_NETWORK_ERROR;
        }
    } else {
        // Timed lock - use fallback for macOS
#if defined(__APPLE__) || defined(__MACH__)
        // macOS doesn't always have pthread_mutex_timedlock, use polling approach
        uint64_t start_time = get_time_ms();
        uint64_t end_time = start_time + timeout_ms;
        
        while (get_time_ms() < end_time) {
            int result = pthread_mutex_trylock(&mutex->mutex);
            if (result == 0) {
                trace("Mutex locked within timeout (macOS fallback)");
                return EV_OK;
            } else if (result != EBUSY) {
                error("pthread_mutex_trylock failed: %s", strerror(result));
                return EV_ERR_NETWORK_ERROR;
            }
            
            // Sleep for 1ms
            struct timespec sleep_time = {0, 1000000}; // 1ms
            nanosleep(&sleep_time, NULL);
        }
        
        trace("Mutex lock timeout (macOS fallback)");
        return EV_ERR_TIMEOUT;
#else
        // Linux/BSD with pthread_mutex_timedlock
        struct timespec abs_timeout;
        uint64_t current_time_ms = get_time_ms();
        uint64_t target_time_ms = current_time_ms + timeout_ms;
        
        abs_timeout.tv_sec = target_time_ms / 1000;
        abs_timeout.tv_nsec = (target_time_ms % 1000) * 1000000;
        
        int result = pthread_mutex_timedlock(&mutex->mutex, &abs_timeout);
        if (result == 0) {
            trace("Mutex locked within timeout");
            return EV_OK;
        } else if (result == ETIMEDOUT) {
            trace("Mutex lock timeout");
            return EV_ERR_TIMEOUT;
        } else {
            error("pthread_mutex_timedlock failed: %s", strerror(result));
            return EV_ERR_NETWORK_ERROR;
        }
#endif
    }
#endif
}

ev_err ev_mutex_unlock(ev_mutex *mutex) {
    trace("Unlocking mutex");
    
    if (!mutex) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    if (mutex->lock_count > 0) {
        mutex->lock_count--;
        if (mutex->lock_count == 0) {
            mutex->owner_thread = 0;
        }
    }
    LeaveCriticalSection(&mutex->cs);
#else
    int result = pthread_mutex_unlock(&mutex->mutex);
    if (result != 0) {
        error("pthread_mutex_unlock failed: %s", strerror(result));
        return EV_ERR_NETWORK_ERROR;
    }
#endif
    
    trace("Mutex unlocked");
    return EV_OK;
}

//=============================================================================
// CONDITIONAL VARIABLE IMPLEMENTATION
//=============================================================================

ev_err ev_cond_var_create(ev_cond_var **cond_var) {
    trace("Creating conditional variable");
    
    if (!cond_var) {
        return EV_ERR_NULL_PTR;
    }
    
    *cond_var = calloc(1, sizeof(ev_cond_var));
    if (!*cond_var) {
        error("Failed to allocate conditional variable");
        return EV_ERR_NO_RESOURCES;
    }
    
#ifdef _WIN32
    InitializeConditionVariable(&(*cond_var)->cv);
#else
    if (pthread_cond_init(&(*cond_var)->cond, NULL) != 0) {
        error("Failed to initialize conditional variable");
        free(*cond_var);
        *cond_var = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
#endif
    
    trace("Conditional variable created successfully");
    return EV_OK;
}

ev_err ev_cond_var_destroy(ev_cond_var *cond_var) {
    trace("Destroying conditional variable");
    
    if (!cond_var) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    // Windows condition variables don't need explicit cleanup
#else
    pthread_cond_destroy(&cond_var->cond);
#endif
    
    free(cond_var);
    trace("Conditional variable destroyed");
    return EV_OK;
}

ev_err ev_cond_var_signal(ev_cond_var *cond_var) {
    trace("Signaling conditional variable");
    
    if (!cond_var) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    WakeConditionVariable(&cond_var->cv);
#else
    int result = pthread_cond_signal(&cond_var->cond);
    if (result != 0) {
        error("pthread_cond_signal failed: %s", strerror(result));
        return EV_ERR_NETWORK_ERROR;
    }
#endif
    
    trace("Conditional variable signaled");
    return EV_OK;
}

ev_err ev_cond_var_wait(ev_cond_var *cond_var, ev_mutex *mutex, size_t timeout_ms) {
    trace("Waiting on conditional variable with timeout %zu ms", timeout_ms);
    
    if (!cond_var || !mutex) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    DWORD timeout = (timeout_ms == THREAD_WAIT_FOREVER) ? INFINITE : (DWORD)timeout_ms;
    
    if (timeout_ms == THREAD_NO_WAIT) {
        // For no-wait, we just check if we can immediately acquire and signal
        return EV_ERR_WOULD_BLOCK;
    }
    
    BOOL result = SleepConditionVariableCS(&cond_var->cv, &mutex->cs, timeout);
    if (result) {
        trace("Conditional variable wait completed");
        return EV_OK;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_TIMEOUT) {
            trace("Conditional variable wait timeout");
            return EV_ERR_TIMEOUT;
        } else {
            error("SleepConditionVariableCS failed: %lu", error);
            return EV_ERR_NETWORK_ERROR;
        }
    }
#else
    if (timeout_ms == THREAD_NO_WAIT) {
        // For no-wait, we just return would block
        return EV_ERR_WOULD_BLOCK;
    } else if (timeout_ms == THREAD_WAIT_FOREVER) {
        // Wait forever
        int result = pthread_cond_wait(&cond_var->cond, &mutex->mutex);
        if (result == 0) {
            trace("Conditional variable wait completed");
            return EV_OK;
        } else {
            error("pthread_cond_wait failed: %s", strerror(result));
            return EV_ERR_NETWORK_ERROR;
        }
    } else {
        // Timed wait
        struct timespec abs_timeout;
        uint64_t current_time_ms = get_time_ms();
        uint64_t target_time_ms = current_time_ms + timeout_ms;
        
        abs_timeout.tv_sec = target_time_ms / 1000;
        abs_timeout.tv_nsec = (target_time_ms % 1000) * 1000000;
        
        int result = pthread_cond_timedwait(&cond_var->cond, &mutex->mutex, &abs_timeout);
        if (result == 0) {
            trace("Conditional variable wait completed");
            return EV_OK;
        } else if (result == ETIMEDOUT) {
            trace("Conditional variable wait timeout");
            return EV_ERR_TIMEOUT;
        } else {
            error("pthread_cond_timedwait failed: %s", strerror(result));
            return EV_ERR_NETWORK_ERROR;
        }
    }
#endif
}

//=============================================================================
// THREAD IMPLEMENTATION
//=============================================================================

#ifdef _WIN32
static DWORD WINAPI thread_wrapper(LPVOID arg) {
    ev_thread *thread = (ev_thread *)arg;
    thread->func(thread->data);
    return 0;
}
#else
static void *thread_wrapper(void *arg) {
    ev_thread *thread = (ev_thread *)arg;
    thread->func(thread->data);
    return NULL;
}
#endif

ev_err ev_thread_create(ev_thread **thread, ev_thread_func func, void *data) {
    trace("Creating thread");
    
    if (!thread || !func) {
        return EV_ERR_NULL_PTR;
    }
    
    *thread = calloc(1, sizeof(ev_thread));
    if (!*thread) {
        error("Failed to allocate thread");
        return EV_ERR_NO_RESOURCES;
    }
    
    (*thread)->func = func;
    (*thread)->data = data;
    
#ifdef _WIN32
    (*thread)->handle = CreateThread(
        NULL,                   // Security attributes
        0,                      // Stack size
        thread_wrapper,         // Thread function
        *thread,                // Parameter
        0,                      // Creation flags
        &(*thread)->thread_id   // Thread ID
    );
    
    if ((*thread)->handle == NULL) {
        error("CreateThread failed: %lu", GetLastError());
        free(*thread);
        *thread = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
#else
    if (pthread_create(&(*thread)->thread, NULL, thread_wrapper, *thread) != 0) {
        error("pthread_create failed: %s", strerror(errno));
        free(*thread);
        *thread = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
#endif
    
    trace("Thread created successfully");
    return EV_OK;
}

ev_err ev_thread_join(ev_thread *thread) {
    trace("Joining thread");
    
    if (!thread) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    if (WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0) {
        error("WaitForSingleObject failed: %lu", GetLastError());
        return EV_ERR_NETWORK_ERROR;
    }
#else
    if (pthread_join(thread->thread, NULL) != 0) {
        error("pthread_join failed: %s", strerror(errno));
        return EV_ERR_NETWORK_ERROR;
    }
#endif
    
    trace("Thread joined successfully");
    return EV_OK;
}

ev_err ev_thread_destroy(ev_thread *thread) {
    trace("Destroying thread");
    
    if (!thread) {
        return EV_ERR_NULL_PTR;
    }
    
#ifdef _WIN32
    if (thread->handle) {
        CloseHandle(thread->handle);
    }
#endif
    
    free(thread);
    trace("Thread destroyed");
    return EV_OK;
}

//=============================================================================
// SIGNAL HANDLING
//=============================================================================

static void (*g_interrupt_handler)(void) = NULL;

static void signal_handler_wrapper(int sig) {
    if (g_interrupt_handler) {
        g_interrupt_handler();
    }
}

ev_err ev_set_interrupt_handler(void (*handler)(void)) {
    trace("Setting interrupt handler");
    
    g_interrupt_handler = handler;
    
#ifdef _WIN32
    // Windows signal handling
    if (signal(SIGINT, signal_handler_wrapper) == SIG_ERR) {
        error("Failed to set SIGINT handler");
        return EV_ERR_NETWORK_ERROR;
    }
    if (signal(SIGTERM, signal_handler_wrapper) == SIG_ERR) {
        error("Failed to set SIGTERM handler");
        return EV_ERR_NETWORK_ERROR;
    }
#else
    // POSIX signal handling
    struct sigaction sa;
    sa.sa_handler = signal_handler_wrapper;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        error("Failed to set SIGINT handler: %s", strerror(errno));
        return EV_ERR_NETWORK_ERROR;
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        error("Failed to set SIGTERM handler: %s", strerror(errno));
        return EV_ERR_NETWORK_ERROR;
    }
#endif
    
    trace("Interrupt handler set successfully");
    return EV_OK;
}

//=============================================================================
// ATOMIC OPERATIONS IMPLEMENTATION
//=============================================================================

// Macros to reduce code duplication for atomic operations
#define IMPLEMENT_ATOMIC_OPS(type_suffix, type) \
    ev_err ev_atomic_load_##type_suffix(type *dest_value, ev_atomic type *src_value) { \
        if (!dest_value || !src_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_load_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_store_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_store_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_fetch_add_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_fetch_add_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_add_fetch_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_add_fetch_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_fetch_sub_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_fetch_sub_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_sub_fetch_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_sub_fetch_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_fetch_and_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_fetch_and_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_and_fetch_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_and_fetch_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_fetch_or_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_fetch_or_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_or_fetch_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_or_fetch_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_fetch_xor_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_fetch_xor_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    } \
    \
    ev_err ev_atomic_xor_fetch_##type_suffix(ev_atomic type *dest_value, type src_value) { \
        if (!dest_value) return EV_ERR_NULL_PTR; \
        _implement_atomic_xor_fetch_##type_suffix(dest_value, src_value); \
        return EV_OK; \
    }

// Platform-specific atomic implementations
#if HAS_C11_ATOMICS
    // C11 atomics implementation
    #define _implement_atomic_load_u8(dest, src) (*dest = atomic_load((_Atomic uint8_t *)src))
    #define _implement_atomic_store_u8(dest, src) atomic_store((_Atomic uint8_t *)dest, src)
    #define _implement_atomic_fetch_add_u8(dest, src) atomic_fetch_add((_Atomic uint8_t *)dest, src)
    #define _implement_atomic_add_fetch_u8(dest, src) (atomic_fetch_add((_Atomic uint8_t *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u8(dest, src) atomic_fetch_sub((_Atomic uint8_t *)dest, src)
    #define _implement_atomic_sub_fetch_u8(dest, src) (atomic_fetch_sub((_Atomic uint8_t *)dest, src) - src)
    #define _implement_atomic_fetch_and_u8(dest, src) atomic_fetch_and((_Atomic uint8_t *)dest, src)
    #define _implement_atomic_and_fetch_u8(dest, src) (atomic_fetch_and((_Atomic uint8_t *)dest, src) & src)
    #define _implement_atomic_fetch_or_u8(dest, src) atomic_fetch_or((_Atomic uint8_t *)dest, src)
    #define _implement_atomic_or_fetch_u8(dest, src) (atomic_fetch_or((_Atomic uint8_t *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u8(dest, src) atomic_fetch_xor((_Atomic uint8_t *)dest, src)
    #define _implement_atomic_xor_fetch_u8(dest, src) (atomic_fetch_xor((_Atomic uint8_t *)dest, src) ^ src)

    #define _implement_atomic_load_u16(dest, src) (*dest = atomic_load((_Atomic uint16_t *)src))
    #define _implement_atomic_store_u16(dest, src) atomic_store((_Atomic uint16_t *)dest, src)
    #define _implement_atomic_fetch_add_u16(dest, src) atomic_fetch_add((_Atomic uint16_t *)dest, src)
    #define _implement_atomic_add_fetch_u16(dest, src) (atomic_fetch_add((_Atomic uint16_t *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u16(dest, src) atomic_fetch_sub((_Atomic uint16_t *)dest, src)
    #define _implement_atomic_sub_fetch_u16(dest, src) (atomic_fetch_sub((_Atomic uint16_t *)dest, src) - src)
    #define _implement_atomic_fetch_and_u16(dest, src) atomic_fetch_and((_Atomic uint16_t *)dest, src)
    #define _implement_atomic_and_fetch_u16(dest, src) (atomic_fetch_and((_Atomic uint16_t *)dest, src) & src)
    #define _implement_atomic_fetch_or_u16(dest, src) atomic_fetch_or((_Atomic uint16_t *)dest, src)
    #define _implement_atomic_or_fetch_u16(dest, src) (atomic_fetch_or((_Atomic uint16_t *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u16(dest, src) atomic_fetch_xor((_Atomic uint16_t *)dest, src)
    #define _implement_atomic_xor_fetch_u16(dest, src) (atomic_fetch_xor((_Atomic uint16_t *)dest, src) ^ src)

    #define _implement_atomic_load_u32(dest, src) (*dest = atomic_load((_Atomic uint32_t *)src))
    #define _implement_atomic_store_u32(dest, src) atomic_store((_Atomic uint32_t *)dest, src)
    #define _implement_atomic_fetch_add_u32(dest, src) atomic_fetch_add((_Atomic uint32_t *)dest, src)
    #define _implement_atomic_add_fetch_u32(dest, src) (atomic_fetch_add((_Atomic uint32_t *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u32(dest, src) atomic_fetch_sub((_Atomic uint32_t *)dest, src)
    #define _implement_atomic_sub_fetch_u32(dest, src) (atomic_fetch_sub((_Atomic uint32_t *)dest, src) - src)
    #define _implement_atomic_fetch_and_u32(dest, src) atomic_fetch_and((_Atomic uint32_t *)dest, src)
    #define _implement_atomic_and_fetch_u32(dest, src) (atomic_fetch_and((_Atomic uint32_t *)dest, src) & src)
    #define _implement_atomic_fetch_or_u32(dest, src) atomic_fetch_or((_Atomic uint32_t *)dest, src)
    #define _implement_atomic_or_fetch_u32(dest, src) (atomic_fetch_or((_Atomic uint32_t *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u32(dest, src) atomic_fetch_xor((_Atomic uint32_t *)dest, src)
    #define _implement_atomic_xor_fetch_u32(dest, src) (atomic_fetch_xor((_Atomic uint32_t *)dest, src) ^ src)

    #define _implement_atomic_load_u64(dest, src) (*dest = atomic_load((_Atomic uint64_t *)src))
    #define _implement_atomic_store_u64(dest, src) atomic_store((_Atomic uint64_t *)dest, src)
    #define _implement_atomic_fetch_add_u64(dest, src) atomic_fetch_add((_Atomic uint64_t *)dest, src)
    #define _implement_atomic_add_fetch_u64(dest, src) (atomic_fetch_add((_Atomic uint64_t *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u64(dest, src) atomic_fetch_sub((_Atomic uint64_t *)dest, src)
    #define _implement_atomic_sub_fetch_u64(dest, src) (atomic_fetch_sub((_Atomic uint64_t *)dest, src) - src)
    #define _implement_atomic_fetch_and_u64(dest, src) atomic_fetch_and((_Atomic uint64_t *)dest, src)
    #define _implement_atomic_and_fetch_u64(dest, src) (atomic_fetch_and((_Atomic uint64_t *)dest, src) & src)
    #define _implement_atomic_fetch_or_u64(dest, src) atomic_fetch_or((_Atomic uint64_t *)dest, src)
    #define _implement_atomic_or_fetch_u64(dest, src) (atomic_fetch_or((_Atomic uint64_t *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u64(dest, src) atomic_fetch_xor((_Atomic uint64_t *)dest, src)
    #define _implement_atomic_xor_fetch_u64(dest, src) (atomic_fetch_xor((_Atomic uint64_t *)dest, src) ^ src)

#elif defined(_WIN32)
    // Windows InterlockedXxx functions
    #define _implement_atomic_load_u8(dest, src) (*dest = *(volatile uint8_t *)src)
    #define _implement_atomic_store_u8(dest, src) (*(volatile uint8_t *)dest = src)
    #define _implement_atomic_fetch_add_u8(dest, src) InterlockedExchangeAdd8((volatile CHAR *)dest, src)
    #define _implement_atomic_add_fetch_u8(dest, src) (InterlockedExchangeAdd8((volatile CHAR *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u8(dest, src) InterlockedExchangeAdd8((volatile CHAR *)dest, -(src))
    #define _implement_atomic_sub_fetch_u8(dest, src) (InterlockedExchangeAdd8((volatile CHAR *)dest, -(src)) - src)
    #define _implement_atomic_fetch_and_u8(dest, src) InterlockedAnd8((volatile CHAR *)dest, src)
    #define _implement_atomic_and_fetch_u8(dest, src) (InterlockedAnd8((volatile CHAR *)dest, src) & src)
    #define _implement_atomic_fetch_or_u8(dest, src) InterlockedOr8((volatile CHAR *)dest, src)
    #define _implement_atomic_or_fetch_u8(dest, src) (InterlockedOr8((volatile CHAR *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u8(dest, src) InterlockedXor8((volatile CHAR *)dest, src)
    #define _implement_atomic_xor_fetch_u8(dest, src) (InterlockedXor8((volatile CHAR *)dest, src) ^ src)

    #define _implement_atomic_load_u16(dest, src) (*dest = *(volatile uint16_t *)src)
    #define _implement_atomic_store_u16(dest, src) (*(volatile uint16_t *)dest = src)
    #define _implement_atomic_fetch_add_u16(dest, src) InterlockedExchangeAdd16((volatile SHORT *)dest, src)
    #define _implement_atomic_add_fetch_u16(dest, src) (InterlockedExchangeAdd16((volatile SHORT *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u16(dest, src) InterlockedExchangeAdd16((volatile SHORT *)dest, -(src))
    #define _implement_atomic_sub_fetch_u16(dest, src) (InterlockedExchangeAdd16((volatile SHORT *)dest, -(src)) - src)
    #define _implement_atomic_fetch_and_u16(dest, src) InterlockedAnd16((volatile SHORT *)dest, src)
    #define _implement_atomic_and_fetch_u16(dest, src) (InterlockedAnd16((volatile SHORT *)dest, src) & src)
    #define _implement_atomic_fetch_or_u16(dest, src) InterlockedOr16((volatile SHORT *)dest, src)
    #define _implement_atomic_or_fetch_u16(dest, src) (InterlockedOr16((volatile SHORT *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u16(dest, src) InterlockedXor16((volatile SHORT *)dest, src)
    #define _implement_atomic_xor_fetch_u16(dest, src) (InterlockedXor16((volatile SHORT *)dest, src) ^ src)

    #define _implement_atomic_load_u32(dest, src) (*dest = *(volatile uint32_t *)src)
    #define _implement_atomic_store_u32(dest, src) (*(volatile uint32_t *)dest = src)
    #define _implement_atomic_fetch_add_u32(dest, src) InterlockedExchangeAdd((volatile LONG *)dest, src)
    #define _implement_atomic_add_fetch_u32(dest, src) (InterlockedExchangeAdd((volatile LONG *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u32(dest, src) InterlockedExchangeAdd((volatile LONG *)dest, -(src))
    #define _implement_atomic_sub_fetch_u32(dest, src) (InterlockedExchangeAdd((volatile LONG *)dest, -(src)) - src)
    #define _implement_atomic_fetch_and_u32(dest, src) InterlockedAnd((volatile LONG *)dest, src)
    #define _implement_atomic_and_fetch_u32(dest, src) (InterlockedAnd((volatile LONG *)dest, src) & src)
    #define _implement_atomic_fetch_or_u32(dest, src) InterlockedOr((volatile LONG *)dest, src)
    #define _implement_atomic_or_fetch_u32(dest, src) (InterlockedOr((volatile LONG *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u32(dest, src) InterlockedXor((volatile LONG *)dest, src)
    #define _implement_atomic_xor_fetch_u32(dest, src) (InterlockedXor((volatile LONG *)dest, src) ^ src)

    #define _implement_atomic_load_u64(dest, src) (*dest = *(volatile uint64_t *)src)
    #define _implement_atomic_store_u64(dest, src) (*(volatile uint64_t *)dest = src)
    #define _implement_atomic_fetch_add_u64(dest, src) InterlockedExchangeAdd64((volatile LONGLONG *)dest, src)
    #define _implement_atomic_add_fetch_u64(dest, src) (InterlockedExchangeAdd64((volatile LONGLONG *)dest, src) + src)
    #define _implement_atomic_fetch_sub_u64(dest, src) InterlockedExchangeAdd64((volatile LONGLONG *)dest, -(src))
    #define _implement_atomic_sub_fetch_u64(dest, src) (InterlockedExchangeAdd64((volatile LONGLONG *)dest, -(src)) - src)
    #define _implement_atomic_fetch_and_u64(dest, src) InterlockedAnd64((volatile LONGLONG *)dest, src)
    #define _implement_atomic_and_fetch_u64(dest, src) (InterlockedAnd64((volatile LONGLONG *)dest, src) & src)
    #define _implement_atomic_fetch_or_u64(dest, src) InterlockedOr64((volatile LONGLONG *)dest, src)
    #define _implement_atomic_or_fetch_u64(dest, src) (InterlockedOr64((volatile LONGLONG *)dest, src) | src)
    #define _implement_atomic_fetch_xor_u64(dest, src) InterlockedXor64((volatile LONGLONG *)dest, src)
    #define _implement_atomic_xor_fetch_u64(dest, src) (InterlockedXor64((volatile LONGLONG *)dest, src) ^ src)

#else
    // GCC/Clang builtin atomics (POSIX)
    #define _implement_atomic_load_u8(dest, src) (*dest = __atomic_load_n(src, __ATOMIC_SEQ_CST))
    #define _implement_atomic_store_u8(dest, src) __atomic_store_n(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_add_u8(dest, src) __atomic_fetch_add(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_add_fetch_u8(dest, src) __atomic_add_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_sub_u8(dest, src) __atomic_fetch_sub(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_sub_fetch_u8(dest, src) __atomic_sub_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_and_u8(dest, src) __atomic_fetch_and(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_and_fetch_u8(dest, src) __atomic_and_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_or_u8(dest, src) __atomic_fetch_or(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_or_fetch_u8(dest, src) __atomic_or_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_xor_u8(dest, src) __atomic_fetch_xor(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_xor_fetch_u8(dest, src) __atomic_xor_fetch(dest, src, __ATOMIC_SEQ_CST)

    #define _implement_atomic_load_u16(dest, src) (*dest = __atomic_load_n(src, __ATOMIC_SEQ_CST))
    #define _implement_atomic_store_u16(dest, src) __atomic_store_n(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_add_u16(dest, src) __atomic_fetch_add(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_add_fetch_u16(dest, src) __atomic_add_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_sub_u16(dest, src) __atomic_fetch_sub(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_sub_fetch_u16(dest, src) __atomic_sub_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_and_u16(dest, src) __atomic_fetch_and(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_and_fetch_u16(dest, src) __atomic_and_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_or_u16(dest, src) __atomic_fetch_or(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_or_fetch_u16(dest, src) __atomic_or_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_xor_u16(dest, src) __atomic_fetch_xor(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_xor_fetch_u16(dest, src) __atomic_xor_fetch(dest, src, __ATOMIC_SEQ_CST)

    #define _implement_atomic_load_u32(dest, src) (*dest = __atomic_load_n(src, __ATOMIC_SEQ_CST))
    #define _implement_atomic_store_u32(dest, src) __atomic_store_n(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_add_u32(dest, src) __atomic_fetch_add(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_add_fetch_u32(dest, src) __atomic_add_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_sub_u32(dest, src) __atomic_fetch_sub(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_sub_fetch_u32(dest, src) __atomic_sub_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_and_u32(dest, src) __atomic_fetch_and(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_and_fetch_u32(dest, src) __atomic_and_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_or_u32(dest, src) __atomic_fetch_or(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_or_fetch_u32(dest, src) __atomic_or_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_xor_u32(dest, src) __atomic_fetch_xor(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_xor_fetch_u32(dest, src) __atomic_xor_fetch(dest, src, __ATOMIC_SEQ_CST)

    #define _implement_atomic_load_u64(dest, src) (*dest = __atomic_load_n(src, __ATOMIC_SEQ_CST))
    #define _implement_atomic_store_u64(dest, src) __atomic_store_n(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_add_u64(dest, src) __atomic_fetch_add(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_add_fetch_u64(dest, src) __atomic_add_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_sub_u64(dest, src) __atomic_fetch_sub(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_sub_fetch_u64(dest, src) __atomic_sub_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_and_u64(dest, src) __atomic_fetch_and(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_and_fetch_u64(dest, src) __atomic_and_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_or_u64(dest, src) __atomic_fetch_or(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_or_fetch_u64(dest, src) __atomic_or_fetch(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_fetch_xor_u64(dest, src) __atomic_fetch_xor(dest, src, __ATOMIC_SEQ_CST)
    #define _implement_atomic_xor_fetch_u64(dest, src) __atomic_xor_fetch(dest, src, __ATOMIC_SEQ_CST)

#endif

// Generate all atomic operations for each type
IMPLEMENT_ATOMIC_OPS(u8, uint8_t)
IMPLEMENT_ATOMIC_OPS(u16, uint16_t)
IMPLEMENT_ATOMIC_OPS(u32, uint32_t)
IMPLEMENT_ATOMIC_OPS(u64, uint64_t)