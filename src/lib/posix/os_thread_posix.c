// POSIX implementation of ptk_os_thread API
#include <ptk_os_thread.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

typedef struct ptk_thread_internal {
    pthread_t thread;
    ptk_thread_handle_t handle;
    ptk_thread_handle_t parent;
    ptk_shared_handle_t data;
    ptk_thread_func func;
    atomic_uint_fast64_t pending_signals;
    volatile bool running;
} ptk_thread_internal_t;

// Thread-local storage for current thread
static ptk_thread_local ptk_thread_internal_t *current_thread = NULL;
static ptk_thread_internal_t *main_thread = NULL;

//=============================================================================
// SIGNAL-DRIVEN THREADING IMPLEMENTATION
//=============================================================================

static void *ptk_thread_entry(void *arg) {
    ptk_thread_internal_t *thread_info = (ptk_thread_internal_t *)arg;
    current_thread = thread_info;
    
    debug("Thread starting with handle 0x%016lx", thread_info->handle.value);
    
    // Call the user function
    thread_info->func(thread_info->data);
    
    debug("Thread finishing with handle 0x%016lx", thread_info->handle.value);
    
    // Signal parent about child death (if we have a parent)
    if (PTK_SHARED_IS_VALID(thread_info->parent)) {
        ptk_thread_signal(thread_info->parent, PTK_THREAD_SIGNAL_CHILD_DIED);
    }
    
    thread_info->running = false;
    
    return NULL;
}

ptk_thread_handle_t ptk_thread_create(ptk_thread_handle_t parent,
                                     ptk_thread_func func, 
                                     ptk_shared_handle_t data) {
    if (!func) {
        error("Thread function cannot be NULL");
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    // Allocate thread info in shared memory
    ptk_shared_handle_t thread_handle = ptk_shared_alloc(sizeof(ptk_thread_internal_t), NULL);
    if (!PTK_SHARED_IS_VALID(thread_handle)) {
        error("Failed to allocate thread handle");
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    ptk_thread_internal_t *thread_info = ptk_shared_acquire(thread_handle, PTK_TIME_WAIT_FOREVER);
    if (!thread_info) {
        error("Failed to acquire thread handle");
        ptk_shared_release(thread_handle);
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    thread_info->handle = thread_handle;
    thread_info->parent = parent;
    thread_info->data = data;
    thread_info->func = func;
    atomic_store(&thread_info->pending_signals, 0);
    thread_info->running = true;
    
    int result = pthread_create(&thread_info->thread, NULL, ptk_thread_entry, thread_info);
    if (result != 0) {
        error("pthread_create failed: %s", strerror(result));
        ptk_shared_release(thread_handle);
        ptk_shared_release(thread_handle);
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    ptk_shared_release(thread_handle);
    
    debug("Created thread with handle 0x%016lx", thread_handle.value);
    
    return thread_handle;
}

ptk_thread_handle_t ptk_thread_self(void) {
    if (current_thread) {
        return current_thread->handle;
    }
    
    // Initialize main thread if not already done
    if (!main_thread) {
        ptk_shared_handle_t main_handle = ptk_shared_alloc(sizeof(ptk_thread_internal_t), NULL);
        if (PTK_SHARED_IS_VALID(main_handle)) {
            main_thread = ptk_shared_acquire(main_handle, PTK_TIME_WAIT_FOREVER);
            if (main_thread) {
                main_thread->handle = main_handle;
                main_thread->parent = PTK_THREAD_NO_PARENT;
                main_thread->data = PTK_SHARED_INVALID_HANDLE;
                main_thread->func = NULL;
                atomic_store(&main_thread->pending_signals, 0);
                main_thread->running = true;
                current_thread = main_thread;
                debug("Initialized main thread with handle 0x%016lx", main_handle.value);
            }
        }
    }
    
    return current_thread ? current_thread->handle : PTK_SHARED_INVALID_HANDLE;
}

ptk_err ptk_thread_wait(ptk_time_ms timeout_ms) {
    // Ensure we have a current thread
    if (!current_thread) {
        ptk_thread_self();  // Initialize main thread if needed
    }
    
    if (!current_thread) {
        return PTK_ERR_BAD_INTERNAL_STATE;
    }
    
    // Simple polling implementation for now
    // In a real implementation, this would use proper OS-level waiting
    ptk_time_ms elapsed = 0;
    const ptk_time_ms poll_interval = 1;  // 1ms polling
    
    while (elapsed < timeout_ms || timeout_ms == PTK_TIME_WAIT_FOREVER) {
        // Check for pending signals
        uint64_t signals = atomic_load(&current_thread->pending_signals);
        if (signals != 0) {
            return PTK_ERR_SIGNAL;
        }
        
        // Sleep for a short time
        usleep(poll_interval * 1000);  // Convert ms to microseconds
        
        if (timeout_ms != PTK_TIME_WAIT_FOREVER) {
            elapsed += poll_interval;
        }
    }
    
    return PTK_OK;  // Timeout
}

ptk_err ptk_thread_signal(ptk_thread_handle_t handle, ptk_thread_signal_t signal_type) {
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("Invalid thread handle");
        return PTK_ERR_INVALID_PARAM;
    }
    
    ptk_thread_internal_t *thread_info = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!thread_info) {
        error("Failed to acquire thread handle");
        return PTK_ERR_INVALID_PARAM;
    }
    
    // Set the signal bit atomically
    atomic_fetch_or(&thread_info->pending_signals, signal_type);
    
    // TODO: Actually signal the thread (e.g., using pthread_kill)
    
    ptk_shared_release(handle);
    
    debug("Signaled thread 0x%016lx with signal 0x%lx", handle.value, (uint64_t)signal_type);
    
    return PTK_OK;
}

uint64_t ptk_thread_get_pending_signals(void) {
    if (current_thread) {
        return atomic_load(&current_thread->pending_signals);
    }
    
    return 0;
}

bool ptk_thread_has_signal(ptk_thread_signal_t signal_bit) {
    if (current_thread) {
        return (atomic_load(&current_thread->pending_signals) & signal_bit) != 0;
    }
    
    return false;
}

void ptk_thread_clear_signals(uint64_t signal_mask) {
    if (current_thread) {
        atomic_fetch_and(&current_thread->pending_signals, ~signal_mask);
    }
}

ptk_thread_handle_t ptk_thread_get_parent(ptk_thread_handle_t thread) {
    if (!PTK_SHARED_IS_VALID(thread)) {
        return PTK_THREAD_NO_PARENT;
    }
    
    ptk_thread_internal_t *thread_info = ptk_shared_acquire(thread, PTK_TIME_WAIT_FOREVER);
    if (!thread_info) {
        return PTK_THREAD_NO_PARENT;
    }
    
    ptk_thread_handle_t parent = thread_info->parent;
    ptk_shared_release(thread);
    
    return parent;
}

int ptk_thread_count_children(ptk_thread_handle_t parent) {
    // TODO: Implement child counting
    (void)parent;
    return 0;
}

ptk_err ptk_thread_signal_all_children(ptk_thread_handle_t parent, ptk_thread_signal_t signal_type) {
    // TODO: Implement signaling all children
    (void)parent;
    (void)signal_type;
    return PTK_OK;
}

ptk_err ptk_thread_cleanup_dead_children(ptk_thread_handle_t parent, ptk_time_ms timeout_ms) {
    // TODO: Implement child cleanup
    (void)parent;
    (void)timeout_ms;
    return PTK_OK;
}