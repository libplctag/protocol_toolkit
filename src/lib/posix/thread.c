/**
 * @file thread.c
 * @brief POSIX implementation of thread system with signal-driven event system
 *
 * This implementation provides thread argument passing, signal handling,
 * and per-thread event queues for socket ownership management.
 */

#include <ptk_os_thread.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

// Thread argument types
typedef enum {
    PTK_THREAD_ARG_PTR,
    PTK_THREAD_ARG_UINT,
    PTK_THREAD_ARG_INT,
    PTK_THREAD_ARG_FLOAT,
    PTK_THREAD_ARG_HANDLE
} ptk_thread_arg_type_t;

// Thread argument structure
typedef struct {
    ptk_thread_arg_type_t type;
    int user_type;
    union {
        void *ptr_val;
        uint64_t uint_val;
        int64_t int_val;
        double float_val;
        ptk_shared_handle_t handle_val;
    } value;
} ptk_thread_arg_t;

// Thread state
typedef struct {
    pthread_t pthread;
    ptk_thread_func func;
    ptk_thread_arg_t *args;
    size_t arg_count;
    size_t arg_capacity;
    int epoll_fd;
    int signal_fd;
    uint64_t pending_signals;
    bool started;
    bool finished;
    ptk_thread_handle_t self_handle;  // Store our own handle
    ptk_thread_handle_t parent_handle; // Parent thread handle
    ptk_thread_handle_t *children;     // Array of child thread handles
    size_t child_count;                // Number of active children
    size_t child_capacity;             // Capacity of children array
} ptk_thread_state_t;

// Thread-local storage for current thread handle
static ptk_thread_local ptk_thread_handle_t tls_current_thread_handle = {0};
static ptk_thread_local ptk_thread_state_t *tls_current_thread = NULL;

// Thread state destructor
static void thread_state_destructor(void *ptr) {
    ptk_thread_state_t *state = (ptk_thread_state_t *)ptr;
    if (!state) return;
    
    debug("Destroying thread state");
    
    // Clean up arguments
    if (state->args) {
        ptk_local_free(&state->args);
    }
    
    // Clean up children array
    if (state->children) {
        ptk_local_free(&state->children);
    }
    
    // Clean up event system
    if (state->epoll_fd >= 0) {
        close(state->epoll_fd);
    }
    if (state->signal_fd >= 0) {
        close(state->signal_fd);
    }
    
    // Join thread if it was started and not yet finished
    if (state->started && !state->finished) {
        debug("Joining thread in destructor");
        pthread_join(state->pthread, NULL);
        state->finished = true;
    }
}

// Initialize thread event system
static ptk_err_t init_thread_event_system(ptk_thread_state_t *state) {
    // Create epoll instance
    state->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (state->epoll_fd == -1) {
        error("epoll_create1 failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Create signal eventfd
    state->signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (state->signal_fd == -1) {
        error("eventfd for signals failed: %s", strerror(errno));
        close(state->epoll_fd);
        state->epoll_fd = -1;
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Add signal eventfd to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = state->signal_fd;
    if (epoll_ctl(state->epoll_fd, EPOLL_CTL_ADD, state->signal_fd, &ev) == -1) {
        error("epoll_ctl ADD signal_fd failed: %s", strerror(errno));
        close(state->signal_fd);
        close(state->epoll_fd);
        state->signal_fd = -1;
        state->epoll_fd = -1;
        return PTK_ERR_NETWORK_ERROR;
    }
    
    return PTK_OK;
}

// Thread wrapper function
static void *thread_wrapper(void *arg) {
    ptk_thread_handle_t *handle_ptr = (ptk_thread_handle_t *)arg;
    ptk_thread_handle_t handle = *handle_ptr;
    info("Thread wrapper starting with handle value: 0x%lx", handle.value);
    
    // Access the thread state using shared memory
    use_shared(handle, ptk_thread_state_t*, state, PTK_TIME_WAIT_FOREVER) {
        // Set thread-local storage
        tls_current_thread_handle = handle;
        tls_current_thread = state;
        
        // Initialize event system for this thread
        if (init_thread_event_system(state) != PTK_OK) {
            error("Failed to initialize thread event system");
            state->finished = true;
            tls_current_thread_handle = PTK_SHARED_INVALID_HANDLE;
        tls_current_thread = NULL;
            return NULL;
        }
        
        info("Thread started with %zu arguments", state->arg_count);
        
        // Call user function
        state->func();
        
        info("Thread finished");
        
        state->finished = true;
        
        // Notify parent thread of child death
        if (ptk_shared_is_valid(state->parent_handle)) {
            info("Notifying parent thread of child death");
            ptk_err_t signal_result = ptk_thread_signal(state->parent_handle, PTK_THREAD_SIGNAL_CHILD_DIED);
            info("Parent notification result: %d", signal_result);
        } else {
            info("No valid parent to notify of child death");
        }
    } on_shared_fail {
        error("Failed to access thread state in wrapper");
    }
    
    info("Thread wrapper exiting");
    tls_current_thread_handle = PTK_SHARED_INVALID_HANDLE;
    tls_current_thread = NULL;
    return NULL;
}

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

ptk_thread_handle_t ptk_thread_create(void) {
    ptk_thread_handle_t handle = ptk_shared_alloc(sizeof(ptk_thread_state_t), thread_state_destructor);
    if (!ptk_shared_is_valid(handle)) {
        error("Failed to allocate thread state");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    // Initialize state using shared memory
    use_shared(handle, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        state->func = NULL;
        state->args = NULL;
        state->arg_count = 0;
        state->arg_capacity = 0;
        state->epoll_fd = -1;
        state->signal_fd = -1;
        state->pending_signals = 0;
        state->started = false;
        state->finished = false;
        state->self_handle = handle;
        state->parent_handle = ptk_thread_self(); // Set current thread as parent
        state->children = NULL;
        state->child_count = 0;
        state->child_capacity = 0;
    } on_shared_fail {
        error("Failed to access thread state during initialization");
        ptk_shared_free(&handle);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    // Add this thread to parent's children list
    ptk_thread_handle_t parent_handle = ptk_thread_self();
    if (ptk_shared_is_valid(parent_handle)) {
        use_shared(parent_handle, ptk_thread_state_t*, parent_state, PTK_TIME_NO_WAIT) {
            // Expand children array if needed
            if (parent_state->child_count >= parent_state->child_capacity) {
                size_t new_capacity = parent_state->child_capacity == 0 ? 4 : parent_state->child_capacity * 2;
                ptk_thread_handle_t *new_children = ptk_local_realloc(parent_state->children, 
                    new_capacity * sizeof(ptk_thread_handle_t));
                if (new_children) {
                    parent_state->children = new_children;
                    parent_state->child_capacity = new_capacity;
                }
            }
            
            // Add child to parent's list
            if (parent_state->child_count < parent_state->child_capacity) {
                parent_state->children[parent_state->child_count++] = handle;
            }
        } on_shared_fail {
            // Parent inaccessible, but not critical
            debug("Could not add child to parent's children list");
        }
    }
    
    return handle;
}

ptk_err_t ptk_thread_set_run_function(ptk_thread_handle_t thread, ptk_thread_func func) {
    if (!ptk_shared_is_valid(thread) || !func) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    ptk_err_t result = PTK_ERR_INVALID_PARAM;
    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (state->started) {
            result = PTK_ERR_INVALID_STATE;
        } else {
            state->func = func;
            result = PTK_OK;
        }
    } on_shared_fail {
        result = PTK_ERR_INVALID_PARAM;
    }
    return result;
}

ptk_err_t ptk_thread_start(ptk_thread_handle_t thread) {
    if (!ptk_shared_is_valid(thread)) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    ptk_err_t result = PTK_ERR_INVALID_PARAM;
    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (!state->func) {
            result = PTK_ERR_INVALID_PARAM;
        } else if (state->started) {
            result = PTK_ERR_INVALID_STATE;
        } else {
            // Create pthread - pass the handle stored in the state
            int pthread_result = pthread_create(&state->pthread, NULL, thread_wrapper, &state->self_handle);
            if (pthread_result != 0) {
                error("pthread_create failed: %s", strerror(pthread_result));
                result = PTK_ERR_NETWORK_ERROR;
            } else {
                state->started = true;
                result = PTK_OK;
            }
        }
    } on_shared_fail {
        result = PTK_ERR_INVALID_PARAM;
    }
    return result;
}

ptk_err_t ptk_thread_add_ptr_arg(ptk_thread_handle_t thread, int user_type, void **ptr) {
    if (!ptk_shared_is_valid(thread) || !ptr) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    ptk_err_t result = PTK_OK;

    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (state->started) {
            result = PTK_ERR_INVALID_STATE;
            break;
        } else {
            // Expand args array if needed
            if (state->arg_count >= state->arg_capacity) {
                size_t new_capacity = state->arg_capacity == 0 ? 4 : state->arg_capacity + 4;
                ptk_thread_arg_t *new_args = ptk_local_realloc(state->args, new_capacity * sizeof(ptk_thread_arg_t));
                if (!new_args) {
                    result = PTK_ERR_NO_RESOURCES;
                    break;
                } else {
                    state->args = new_args;
                    state->arg_capacity = new_capacity;
                }
            }
            
            // Add argument
            ptk_thread_arg_t *arg = &state->args[state->arg_count++];
            arg->type = PTK_THREAD_ARG_PTR;
            arg->user_type = user_type;
            arg->value.ptr_val = *ptr;
            
            // Null the original pointer (ownership transfer)
            *ptr = NULL;
        }
    } on_shared_fail {
        result = PTK_ERR_INVALID_PARAM;
    }
    return result;
}

ptk_err_t ptk_thread_add_uint_arg(ptk_thread_handle_t thread, int user_type, uint64_t val) {
    if (!ptk_shared_is_valid(thread)) {
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_err_t result = PTK_OK;
    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (state->started) {
            result = PTK_ERR_INVALID_STATE;
            break;
        }
        // Expand args array if needed
        if (state->arg_count >= state->arg_capacity) {
            size_t new_capacity = state->arg_capacity == 0 ? 4 : state->arg_capacity + 4;
            ptk_thread_arg_t *new_args = ptk_local_realloc(state->args, new_capacity * sizeof(ptk_thread_arg_t));
            if (!new_args) {
                result = PTK_ERR_NO_RESOURCES;
                break;
            }
            state->args = new_args;
            state->arg_capacity = new_capacity;
        }
        // Add argument
        ptk_thread_arg_t *arg = &state->args[state->arg_count++];
        arg->type = PTK_THREAD_ARG_UINT;
        arg->user_type = user_type;
        arg->value.uint_val = val;
    } on_shared_fail {
        result = PTK_ERR_INVALID_PARAM;
    }
    return result;
}

ptk_err_t ptk_thread_add_int_arg(ptk_thread_handle_t thread, int user_type, int64_t val) {
    if (!ptk_shared_is_valid(thread)) {
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_err_t result = PTK_OK;
    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (state->started) {
            result = PTK_ERR_INVALID_STATE;
            break;
        }
        // Expand args array if needed
        if (state->arg_count >= state->arg_capacity) {
            size_t new_capacity = state->arg_capacity == 0 ? 4 : state->arg_capacity + 4;
            ptk_thread_arg_t *new_args = ptk_local_realloc(state->args, new_capacity * sizeof(ptk_thread_arg_t));
            if (!new_args) {
                result = PTK_ERR_NO_RESOURCES;
                break;
            }
            state->args = new_args;
            state->arg_capacity = new_capacity;
        }
        // Add argument
        ptk_thread_arg_t *arg = &state->args[state->arg_count++];
        arg->type = PTK_THREAD_ARG_INT;
        arg->user_type = user_type;
        arg->value.int_val = val;
    } on_shared_fail {
        result = PTK_ERR_INVALID_PARAM;
    }
    return result;
}

ptk_err_t ptk_thread_add_float_arg(ptk_thread_handle_t thread, int user_type, double val) {
    if (!ptk_shared_is_valid(thread)) {
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_err_t result = PTK_OK;
    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (state->started) {
            result = PTK_ERR_INVALID_STATE;
            break;
        }
        // Expand args array if needed
        if (state->arg_count >= state->arg_capacity) {
            size_t new_capacity = state->arg_capacity == 0 ? 4 : state->arg_capacity + 4;
            ptk_thread_arg_t *new_args = ptk_local_realloc(state->args, new_capacity * sizeof(ptk_thread_arg_t));
            if (!new_args) {
                result = PTK_ERR_NO_RESOURCES;
                break;
            }
            state->args = new_args;
            state->arg_capacity = new_capacity;
        }
        // Add argument
        ptk_thread_arg_t *arg = &state->args[state->arg_count++];
        arg->type = PTK_THREAD_ARG_FLOAT;
        arg->user_type = user_type;
        arg->value.float_val = val;
    } on_shared_fail {
        result = PTK_ERR_INVALID_PARAM;
    }
    return result;
}

ptk_err_t ptk_thread_add_handle_arg(ptk_thread_handle_t thread, int user_type, ptk_shared_handle_t *handle) {
    if (!ptk_shared_is_valid(thread) || !handle) {
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_err_t result = PTK_OK;
    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (state->started) {
            result = PTK_ERR_INVALID_STATE;
            break;
        }
        // Expand args array if needed
        if (state->arg_count >= state->arg_capacity) {
            size_t new_capacity = state->arg_capacity == 0 ? 4 : state->arg_capacity + 4;
            ptk_thread_arg_t *new_args = ptk_local_realloc(state->args, new_capacity * sizeof(ptk_thread_arg_t));
            if (!new_args) {
                result = PTK_ERR_NO_RESOURCES;
                break;
            }
            state->args = new_args;
            state->arg_capacity = new_capacity;
        }
        // Add argument
        ptk_thread_arg_t *arg = &state->args[state->arg_count++];
        arg->type = PTK_THREAD_ARG_HANDLE;
        arg->user_type = user_type;
        arg->value.handle_val = *handle;
        // Null the original handle (ownership transfer)
        *handle = PTK_SHARED_INVALID_HANDLE;
    } on_shared_fail {
        result = PTK_ERR_INVALID_PARAM;
    }
    return result;
}

size_t ptk_thread_get_arg_count(void) {
    if (!tls_current_thread) {
        return 0;
    }
    return tls_current_thread->arg_count;
}

int ptk_thread_get_arg_type(size_t index) {
    if (!tls_current_thread || index >= tls_current_thread->arg_count) {
        return 0;
    }
    return tls_current_thread->args[index].user_type;
}

void *ptk_thread_get_ptr_arg(size_t index) {
    if (!tls_current_thread || index >= tls_current_thread->arg_count) {
        return NULL;
    }
    
    ptk_thread_arg_t *arg = &tls_current_thread->args[index];
    if (arg->type != PTK_THREAD_ARG_PTR) {
        return NULL;
    }
    
    return arg->value.ptr_val;
}

uint64_t ptk_thread_get_uint_arg(size_t index) {
    if (!tls_current_thread || index >= tls_current_thread->arg_count) {
        return 0;
    }
    
    ptk_thread_arg_t *arg = &tls_current_thread->args[index];
    if (arg->type != PTK_THREAD_ARG_UINT) {
        return 0;
    }
    
    return arg->value.uint_val;
}

int64_t ptk_thread_get_int_arg(size_t index) {
    if (!tls_current_thread || index >= tls_current_thread->arg_count) {
        return 0;
    }
    
    ptk_thread_arg_t *arg = &tls_current_thread->args[index];
    if (arg->type != PTK_THREAD_ARG_INT) {
        return 0;
    }
    
    return arg->value.int_val;
}

double ptk_thread_get_float_arg(size_t index) {
    if (!tls_current_thread || index >= tls_current_thread->arg_count) {
        return 0.0;
    }
    
    ptk_thread_arg_t *arg = &tls_current_thread->args[index];
    if (arg->type != PTK_THREAD_ARG_FLOAT) {
        return 0.0;
    }
    
    return arg->value.float_val;
}

ptk_shared_handle_t ptk_thread_get_handle_arg(size_t index) {
    if (!tls_current_thread || index >= tls_current_thread->arg_count) {
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    ptk_thread_arg_t *arg = &tls_current_thread->args[index];
    if (arg->type != PTK_THREAD_ARG_HANDLE) {
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    return arg->value.handle_val;
}

ptk_thread_handle_t ptk_thread_self(void) {
    // If this is the main thread and it hasn't been initialized, create a minimal state
    if (!ptk_shared_is_valid(tls_current_thread_handle) && !tls_current_thread) {
        // Initialize main thread state
        static ptk_thread_state_t main_thread_state = {0};
        static bool main_thread_initialized = false;
        
        if (!main_thread_initialized) {
            main_thread_state.func = NULL;
            main_thread_state.args = NULL;
            main_thread_state.arg_count = 0;
            main_thread_state.arg_capacity = 0;
            main_thread_state.started = true;  // Main thread is always "started"
            main_thread_state.finished = false;
            main_thread_state.self_handle = PTK_SHARED_INVALID_HANDLE;
            main_thread_state.parent_handle = PTK_SHARED_INVALID_HANDLE;
            
            // Initialize event system for main thread
            if (init_thread_event_system(&main_thread_state) == PTK_OK) {
                tls_current_thread = &main_thread_state;
                main_thread_initialized = true;
                debug("Main thread event system initialized");
            }
        }
        
        tls_current_thread_handle = PTK_SHARED_INVALID_HANDLE; // Main thread doesn't have a valid shared handle
    }
    
    return tls_current_thread_handle;
}

ptk_err_t ptk_thread_wait(ptk_time_ms timeout_ms) {
    if (!tls_current_thread) {
        return PTK_ERR_INVALID_STATE;
    }
    
    debug("Thread waiting for signals with timeout %ld ms", timeout_ms);
    
    struct epoll_event events[1];
    int timeout = (timeout_ms == 0) ? -1 : (int)timeout_ms;
    
    int nfds = epoll_wait(tls_current_thread->epoll_fd, events, 1, timeout);
    if (nfds == -1) {
        if (errno == EINTR) {
            return PTK_ERR_INTERRUPT;
        }
        error("epoll_wait failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    if (nfds == 0) {
        return PTK_OK; // Timeout
    }
    
    // Check if signal was received
    if (events[0].data.fd == tls_current_thread->signal_fd) {
        // Drain the eventfd
        uint64_t val;
        (void)read(tls_current_thread->signal_fd, &val, sizeof(val));
        
        // Check if we have abort signals
        if (tls_current_thread->pending_signals & PTK_THREAD_SIGNAL_ABORT_MASK) {
            return PTK_ERR_SIGNAL;
        }
        
        // Non-abort signal
        return PTK_ERR_SIGNAL;
    }
    
    return PTK_OK;
}

ptk_err_t ptk_thread_signal(ptk_thread_handle_t handle, ptk_thread_signal_t signal_type) {
    if (!ptk_shared_is_valid(handle)) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    ptk_err_t local_result = PTK_OK;
    use_shared(handle, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        if (!state->started || state->finished) {
            local_result = PTK_ERR_INVALID_STATE;
            break;
        }
        // Set the signal bit
        state->pending_signals |= signal_type;
        // Wake up the thread
        uint64_t val = 1;
        if (write(state->signal_fd, &val, sizeof(val)) == -1) {
            error("Failed to signal thread: %s", strerror(errno));
            local_result = PTK_ERR_NETWORK_ERROR;
            break;
        }
        local_result = PTK_OK;
    } on_shared_fail {
        local_result = PTK_ERR_INVALID_PARAM;
    }
    return local_result;
}

uint64_t ptk_thread_get_pending_signals(void) {
    if (!tls_current_thread) {
        return 0;
    }
    return tls_current_thread->pending_signals;
}

bool ptk_thread_has_signal(ptk_thread_signal_t signal_bit) {
    if (!tls_current_thread) {
        return false;
    }
    return (tls_current_thread->pending_signals & signal_bit) != 0;
}

void ptk_thread_clear_signals(uint64_t signal_mask) {
    if (!tls_current_thread) {
        return;
    }
    tls_current_thread->pending_signals &= ~signal_mask;
}

// Internal function to get current thread's epoll fd (for socket system)
int ptk_thread_get_epoll_fd(void) {
    if (!tls_current_thread) {
        return -1;
    }
    return tls_current_thread->epoll_fd;
}

// Internal function to get current thread's signal fd (for socket system)
int ptk_thread_get_signal_fd(void) {
    if (!tls_current_thread) {
        return -1;
    }
    return tls_current_thread->signal_fd;
}

//=============================================================================
// PARENT-CHILD THREAD MANAGEMENT (STUBS)
//=============================================================================

ptk_thread_handle_t ptk_thread_get_parent(ptk_thread_handle_t thread) {
    if (!ptk_shared_is_valid(thread)) {
        return PTK_SHARED_INVALID_HANDLE;
    }
    
    ptk_thread_handle_t parent = PTK_SHARED_INVALID_HANDLE;
    use_shared(thread, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        parent = state->parent_handle;
    } on_shared_fail {
        parent = PTK_SHARED_INVALID_HANDLE;
    }
    return parent;
}

int ptk_thread_count_children(ptk_thread_handle_t parent) {
    if (!ptk_shared_is_valid(parent)) {
        return 0;
    }
    
    int count = 0;
    use_shared(parent, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        count = (int)state->child_count;
    } on_shared_fail {
        count = 0;
    }
    return count;
}

ptk_err_t ptk_thread_signal_all_children(ptk_thread_handle_t parent, ptk_thread_signal_t signal_type) {
    if (!ptk_shared_is_valid(parent)) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    ptk_err_t result = PTK_OK;
    use_shared(parent, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        // Signal all children
        for (size_t i = 0; i < state->child_count; i++) {
            ptk_err_t child_result = ptk_thread_signal(state->children[i], signal_type);
            if (child_result != PTK_OK) {
                result = child_result; // Keep last error
            }
        }
    } on_shared_fail {
        result = PTK_ERR_INVALID_STATE;
    }
    
    return result;
}

ptk_err_t ptk_thread_cleanup_dead_children(ptk_thread_handle_t parent, ptk_time_ms timeout_ms) {
    if (!ptk_shared_is_valid(parent)) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    ptk_err_t result = PTK_OK;
    use_shared(parent, ptk_thread_state_t*, state, PTK_TIME_NO_WAIT) {
        size_t write_index = 0;
        
        // Go through all children and compact the array by removing dead ones
        for (size_t read_index = 0; read_index < state->child_count; read_index++) {
            ptk_thread_handle_t child_handle = state->children[read_index];
            bool is_dead = false;
            
            // Check if child is finished
            use_shared(child_handle, ptk_thread_state_t*, child_state, PTK_TIME_NO_WAIT) {
                is_dead = child_state->finished;
            } on_shared_fail {
                // If we can't access the child, consider it dead
                is_dead = true;
            }
            
            if (is_dead) {
                // Release the child handle
                ptk_shared_free(&child_handle);
                debug("Cleaned up dead child thread");
            } else {
                // Keep this child in the array
                state->children[write_index++] = child_handle;
            }
        }
        
        // Update the count to reflect removed children
        state->child_count = write_index;
    } on_shared_fail {
        result = PTK_ERR_INVALID_STATE;
    }
    
    return result;
}

