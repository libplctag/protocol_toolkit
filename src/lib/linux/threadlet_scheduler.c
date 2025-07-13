#include "threadlet_scheduler.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_os_thread.h>
#include <ptk_shared.h>
#include <string.h>

#define INITIAL_REGISTRATIONS_CAPACITY 64
#define INITIAL_QUEUE_CAPACITY 32

static ptk_thread_local event_loop_t *thread_event_loop = NULL;

PTK_ARRAY_IMPLEMENT(threadlet_ptr, threadlet_t*);

static void threadlet_queue_destructor(void *ptr) {
    threadlet_queue_t *queue = (threadlet_queue_t *)ptr;
    if (!queue) return;
    
    if (queue->threadlets) {
        ptk_free(&queue->threadlets);
    }
}

static void event_registration_destructor(void *ptr) {
    // Individual registration cleanup if needed
    // Currently no special cleanup required for event_registration_t
}

static void registrations_array_destructor(void *ptr) {
    registrations_array_t *reg_array = (registrations_array_t *)ptr;
    if (reg_array && reg_array->registration_handles) {
        // Release all individual registration handles
        for (size_t i = 0; i < reg_array->registrations_count; i++) {
            if (PTK_SHARED_IS_VALID(reg_array->registration_handles[i])) {
                ptk_shared_release(reg_array->registration_handles[i]);
            }
        }
        ptk_free(&reg_array->registration_handles);
    }
}

static threadlet_queue_t *threadlet_queue_create(void) {
    threadlet_queue_t *queue = ptk_alloc(sizeof(threadlet_queue_t), threadlet_queue_destructor);
    if (!queue) {
        error("Failed to allocate threadlet queue");
        return NULL;
    }
    
    queue->threadlets = threadlet_ptr_array_create();
    if (!queue->threadlets) {
        error("Failed to allocate threadlet array");
        ptk_free(&queue);
        return NULL;
    }
    
    queue->head = 0;
    queue->tail = 0;
    
    return queue;
}

static ptk_err threadlet_queue_enqueue(threadlet_queue_t *queue, threadlet_t *threadlet) {
    if (!queue || !threadlet) {
        warn("Invalid arguments to threadlet_queue_enqueue");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    ptk_err err = threadlet_ptr_array_append(queue->threadlets, threadlet);
    if (err != PTK_OK) {
        error("Failed to append threadlet to queue");
        return err;
    }
    
    queue->tail++;
    return PTK_OK;
}

static threadlet_t *threadlet_queue_dequeue(threadlet_queue_t *queue) {
    if (!queue || queue->head >= queue->tail) {
        return NULL;
    }
    
    threadlet_t *threadlet = threadlet_ptr_array_get(queue->threadlets, queue->head);
    queue->head++;
    
    if (queue->head == queue->tail) {
        queue->head = 0;
        queue->tail = 0;
        threadlet_ptr_array_clear(queue->threadlets);
    }
    
    return threadlet;
}

static bool threadlet_queue_empty(threadlet_queue_t *queue) {
    return !queue || queue->head >= queue->tail;
}

static void event_loop_destructor(void *ptr) {
    event_loop_t *loop = (event_loop_t *)ptr;
    if (!loop) return;
    
    info("Destroying event loop");
    
    if (loop->platform) {
        ptk_free(&loop->platform);
    }
    if (loop->ready_queue) {
        ptk_free(&loop->ready_queue);
    }
    if (loop->waiting_queue) {
        ptk_free(&loop->waiting_queue);
    }
    if (PTK_SHARED_IS_VALID(loop->registrations_handle)) {
        ptk_shared_release(loop->registrations_handle);
    }
}

event_loop_t *event_loop_create(void) {
    info("Creating event loop for thread");
    
    if (thread_event_loop) {
        warn("Event loop already exists for this thread");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return NULL;
    }
    
    event_loop_t *loop = ptk_alloc(sizeof(event_loop_t), event_loop_destructor);
    if (!loop) {
        error("Failed to allocate event loop");
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    loop->platform = platform_event_loop_create(0);
    if (!loop->platform) {
        error("Failed to create platform event loop");
        ptk_free(&loop);
        return NULL;
    }
    
    loop->ready_queue = threadlet_queue_create();
    if (!loop->ready_queue) {
        error("Failed to create ready queue");
        ptk_free(&loop);
        return NULL;
    }
    
    loop->waiting_queue = threadlet_queue_create();
    if (!loop->waiting_queue) {
        error("Failed to create waiting queue");
        ptk_free(&loop);
        return NULL;
    }
    
    // Initialize shared memory subsystem if not already done
    ptk_shared_init();
    
    // Create shared registrations array
    registrations_array_t *reg_array = ptk_alloc(sizeof(registrations_array_t), registrations_array_destructor);
    if (!reg_array) {
        error("Failed to allocate registrations array structure");
        ptk_free(&loop);
        return NULL;
    }
    
    reg_array->registration_handles = ptk_alloc(INITIAL_REGISTRATIONS_CAPACITY * sizeof(ptk_shared_handle_t), NULL);
    if (!reg_array->registration_handles) {
        error("Failed to allocate registration handles array");
        ptk_free(&reg_array);
        ptk_free(&loop);
        return NULL;
    }
    
    // Initialize all handles to invalid
    for (size_t i = 0; i < INITIAL_REGISTRATIONS_CAPACITY; i++) {
        reg_array->registration_handles[i] = PTK_SHARED_INVALID_HANDLE;
    }
    
    reg_array->registrations_capacity = INITIAL_REGISTRATIONS_CAPACITY;
    reg_array->registrations_count = 0;
    
    // Wrap in shared handle
    loop->registrations_handle = ptk_shared_wrap(reg_array);
    if (!PTK_SHARED_IS_VALID(loop->registrations_handle)) {
        error("Failed to create shared handle for registrations");
        ptk_free(&reg_array);
        ptk_free(&loop);
        return NULL;
    }
    loop->running = false;
    loop->current_time_ms = ptk_now_ms();
    
    thread_event_loop = loop;
    
    info("Event loop created successfully");
    return loop;
}

event_loop_t *get_thread_local_event_loop(void) {
    return thread_event_loop;
}

static ptk_shared_handle_t find_registration_handle(event_loop_t *loop, int fd) {
    ptk_shared_handle_t result = PTK_SHARED_INVALID_HANDLE;
    
    use_shared(loop->registrations_handle, registrations_array_t *reg_array) {
        for (size_t i = 0; i < reg_array->registrations_count; i++) {
            if (PTK_SHARED_IS_VALID(reg_array->registration_handles[i])) {
                use_shared(reg_array->registration_handles[i], event_registration_t *reg) {
                    if (reg->fd == fd) {
                        result = reg_array->registration_handles[i];
                        break;
                    }
                } on_shared_fail {
                    warn("Failed to acquire registration %zu for find", i);
                }
                if (PTK_SHARED_IS_VALID(result)) break;
            }
        }
    } on_shared_fail {
        error("Failed to acquire registrations for find_registration");
    }
    
    return result;
}

static ptk_err remove_registration(event_loop_t *loop, int fd) {
    ptk_err result = PTK_ERR_NOT_FOUND;
    
    use_shared(loop->registrations_handle, registrations_array_t *reg_array) {
        for (size_t i = 0; i < reg_array->registrations_count; i++) {
            if (PTK_SHARED_IS_VALID(reg_array->registration_handles[i])) {
                bool found = false;
                use_shared(reg_array->registration_handles[i], event_registration_t *reg) {
                    if (reg->fd == fd) {
                        found = true;
                    }
                } on_shared_fail {
                    warn("Failed to acquire registration %zu for remove check", i);
                }
                
                if (found) {
                    // Release the handle
                    ptk_shared_release(reg_array->registration_handles[i]);
                    
                    // Shift remaining handles down
                    memmove(&reg_array->registration_handles[i], &reg_array->registration_handles[i + 1],
                           (reg_array->registrations_count - i - 1) * sizeof(ptk_shared_handle_t));
                    
                    reg_array->registrations_count--;
                    result = PTK_OK;
                    break;
                }
            }
        }
    } on_shared_fail {
        error("Failed to acquire registrations for remove_registration");
        result = PTK_ERR_VALIDATION;
    }
    
    return result;
}

ptk_err event_loop_register_io(event_loop_t *loop, int fd, uint32_t events, 
                              threadlet_t *threadlet, ptk_duration_ms timeout_ms) {
    debug("Registering fd=%d for events=0x%x with timeout=%ld", fd, events, timeout_ms);
    
    if (!loop || fd < 0 || !threadlet) {
        warn("Invalid arguments to event_loop_register_io");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    ptk_err result = PTK_OK;
    
    use_shared(loop->registrations_handle, registrations_array_t *reg_array) {
        if (reg_array->registrations_count >= reg_array->registrations_capacity) {
            size_t new_capacity = reg_array->registrations_capacity * 2;
            void *new_handles = ptk_realloc(reg_array->registration_handles, 
                                          new_capacity * sizeof(ptk_shared_handle_t));
            if (!new_handles) {
                error("Failed to expand registrations handles array");
                ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
                result = PTK_ERR_OUT_OF_MEMORY;
                break;
            }
            reg_array->registration_handles = new_handles;
            
            // Initialize new handles to invalid
            for (size_t i = reg_array->registrations_capacity; i < new_capacity; i++) {
                reg_array->registration_handles[i] = PTK_SHARED_INVALID_HANDLE;
            }
            reg_array->registrations_capacity = new_capacity;
        }
        
        ptk_err err = platform_add_fd(loop->platform, fd, events);
        if (err != PTK_OK) {
            error("Failed to add fd to platform event loop");
            result = err;
            break;
        }
        
        // Create new registration
        event_registration_t *new_reg = ptk_alloc(sizeof(event_registration_t), event_registration_destructor);
        if (!new_reg) {
            error("Failed to allocate new registration");
            result = PTK_ERR_OUT_OF_MEMORY;
            break;
        }
        
        // Initialize registration fields
        new_reg->fd = fd;
        new_reg->waiting_threadlet = threadlet;
        new_reg->events = events;
        new_reg->deadline = (timeout_ms > 0) ? ptk_now_ms() + timeout_ms : 0;
        
        // Wrap in shared handle and add to array
        ptk_shared_handle_t reg_handle = ptk_shared_wrap(new_reg);
        if (!PTK_SHARED_IS_VALID(reg_handle)) {
            error("Failed to create shared handle for registration");
            ptk_free(&new_reg);
            result = PTK_ERR_VALIDATION;
            break;
        }
        
        reg_array->registration_handles[reg_array->registrations_count++] = reg_handle;
        
        // Update threadlet status within the use_shared block
        threadlet->waiting_fd = fd;
        threadlet->waiting_events = events;
        threadlet->deadline = new_reg->deadline;
        threadlet_set_status(threadlet, THREADLET_WAITING);
        
    } on_shared_fail {
        error("Failed to acquire registrations for register_io");
        result = PTK_ERR_VALIDATION;
    }
    
    if (result == PTK_OK) {
        debug("I/O registration complete for fd=%d", fd);
    }
    return result;
}

ptk_err event_loop_unregister_io(event_loop_t *loop, int fd) {
    debug("Unregistering fd=%d", fd);
    
    if (!loop || fd < 0) {
        warn("Invalid arguments to event_loop_unregister_io");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    ptk_err err = platform_remove_fd(loop->platform, fd);
    if (err != PTK_OK) {
        warn("Failed to remove fd from platform event loop");
    }
    
    remove_registration(loop, fd);
    
    debug("I/O unregistration complete for fd=%d", fd);
    return PTK_OK;
}

ptk_err event_loop_enqueue_ready(event_loop_t *loop, threadlet_t *threadlet) {
    if (!loop || !threadlet) {
        warn("Invalid arguments to event_loop_enqueue_ready");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    threadlet_set_status(threadlet, THREADLET_READY);
    return threadlet_queue_enqueue(loop->ready_queue, threadlet);
}

static void process_ready_events(event_loop_t *loop, platform_event_list_t *events) {
    for (int i = 0; i < events->count; i++) {
        platform_event_t *event = &events->events[i];
        event_registration_t *reg = find_registration(loop, event->fd);
        
        if (reg && reg->waiting_threadlet) {
            debug("Moving threadlet to ready queue for fd=%d", event->fd);
            
            threadlet_queue_enqueue(loop->ready_queue, reg->waiting_threadlet);
            reg->waiting_threadlet->waiting_fd = -1;
            reg->waiting_threadlet->waiting_events = 0;
            
            event_loop_unregister_io(loop, event->fd);
        }
    }
}

static void process_timeouts(event_loop_t *loop) {
    ptk_time_ms current_time = ptk_now_ms();
    loop->current_time_ms = current_time;
    int timeout_fd = -1;
    
    use_shared(loop->registrations_handle, registrations_array_t *reg_array) {
        for (size_t i = 0; i < reg_array->registrations_count; i++) {
            if (PTK_SHARED_IS_VALID(reg_array->registration_handles[i])) {
                bool timed_out = false;
                threadlet_t *timeout_threadlet = NULL;
                
                use_shared(reg_array->registration_handles[i], event_registration_t *reg) {
                    if (reg->deadline > 0 && current_time >= reg->deadline) {
                        timed_out = true;
                        timeout_fd = reg->fd;
                        timeout_threadlet = reg->waiting_threadlet;
                    }
                } on_shared_fail {
                    warn("Failed to acquire registration %zu for timeout check", i);
                }
                
                if (timed_out) {
                    warn("Timeout occurred for fd=%d", timeout_fd);
                    
                    ptk_set_err(PTK_ERR_TIMEOUT);
                    threadlet_queue_enqueue(loop->ready_queue, timeout_threadlet);
                    timeout_threadlet->waiting_fd = -1;
                    timeout_threadlet->waiting_events = 0;
                    
                    // Remove the registration - must be done outside the inner use_shared block
                    // but we'll defer it until after we finish the array iteration
                    // For now, mark as processed and continue
                    break; // Break and handle unregistration outside
                }
            }
        }
    } on_shared_fail {
        error("Failed to acquire registrations for timeout processing");
    }
    
    // If we found a timeout, unregister it and recursively check for more
    if (timeout_fd >= 0) {
        event_loop_unregister_io(loop, timeout_fd);
        process_timeouts(loop); // Recursive call to handle remaining timeouts
    }
}

static void execute_ready_threadlets(event_loop_t *loop) {
    while (!threadlet_queue_empty(loop->ready_queue)) {
        threadlet_t *threadlet = threadlet_queue_dequeue(loop->ready_queue);
        if (!threadlet) break;
        
        if (threadlet_get_status(threadlet) == THREADLET_FINISHED) {
            debug("Cleaning up finished threadlet");
            ptk_free(&threadlet);
            continue;
        }
        
        trace("Executing threadlet");
        threadlet_resume_execution(threadlet);
        
        if (threadlet_get_status(threadlet) == THREADLET_FINISHED) {
            debug("Threadlet completed execution");
            ptk_free(&threadlet);
        } else if (threadlet_get_status(threadlet) == THREADLET_READY) {
            threadlet_queue_enqueue(loop->ready_queue, threadlet);
        }
    }
}

ptk_err event_loop_run(event_loop_t *loop) {
    info("Starting event loop");
    
    if (!loop) {
        warn("NULL event loop in event_loop_run");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    loop->running = true;
    
    platform_event_list_t event_list;
    platform_event_t events_buffer[64];
    event_list.events = events_buffer;
    event_list.capacity = 64;
    event_list.count = 0;
    
    while (loop->running) {
        int timeout_ms = threadlet_queue_empty(loop->ready_queue) ? 100 : 0;
        
        int result = platform_poll_events(loop->platform, &event_list, timeout_ms);
        if (result < 0) {
            error("Platform event polling failed");
            break;
        }
        
        if (result > 0) {
            process_ready_events(loop, &event_list);
        }
        
        process_timeouts(loop);
        execute_ready_threadlets(loop);
    }
    
    info("Event loop stopped");
    return PTK_OK;
}

ptk_err event_loop_stop(event_loop_t *loop) {
    info("Stopping event loop");
    
    if (!loop) {
        warn("NULL event loop in event_loop_stop");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    loop->running = false;
    return platform_event_loop_wake(loop->platform);
}