#include "threadlet_scheduler.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_os_thread.h>
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
    if (loop->registrations) {
        ptk_free(&loop->registrations);
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
    
    loop->registrations = ptk_alloc(INITIAL_REGISTRATIONS_CAPACITY * sizeof(event_registration_t), NULL);
    if (!loop->registrations) {
        error("Failed to allocate registrations array");
        ptk_free(&loop);
        return NULL;
    }
    
    loop->registrations_capacity = INITIAL_REGISTRATIONS_CAPACITY;
    loop->registrations_count = 0;
    loop->running = false;
    loop->current_time_ms = ptk_now_ms();
    
    thread_event_loop = loop;
    
    info("Event loop created successfully");
    return loop;
}

event_loop_t *get_thread_local_event_loop(void) {
    return thread_event_loop;
}

static event_registration_t *find_registration(event_loop_t *loop, int fd) {
    for (size_t i = 0; i < loop->registrations_count; i++) {
        if (loop->registrations[i].fd == fd) {
            return &loop->registrations[i];
        }
    }
    return NULL;
}

static ptk_err remove_registration(event_loop_t *loop, int fd) {
    for (size_t i = 0; i < loop->registrations_count; i++) {
        if (loop->registrations[i].fd == fd) {
            memmove(&loop->registrations[i], &loop->registrations[i + 1],
                   (loop->registrations_count - i - 1) * sizeof(event_registration_t));
            loop->registrations_count--;
            return PTK_OK;
        }
    }
    return PTK_ERR_NOT_FOUND;
}

ptk_err event_loop_register_io(event_loop_t *loop, int fd, uint32_t events, 
                              threadlet_t *threadlet, ptk_duration_ms timeout_ms) {
    debug("Registering fd=%d for events=0x%x with timeout=%ld", fd, events, timeout_ms);
    
    if (!loop || fd < 0 || !threadlet) {
        warn("Invalid arguments to event_loop_register_io");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    if (loop->registrations_count >= loop->registrations_capacity) {
        size_t new_capacity = loop->registrations_capacity * 2;
        void *new_registrations = ptk_realloc(loop->registrations, 
                                            new_capacity * sizeof(event_registration_t));
        if (!new_registrations) {
            error("Failed to expand registrations array");
            ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
            return PTK_ERR_OUT_OF_MEMORY;
        }
        loop->registrations = new_registrations;
        loop->registrations_capacity = new_capacity;
    }
    
    ptk_err err = platform_add_fd(loop->platform, fd, events);
    if (err != PTK_OK) {
        error("Failed to add fd to platform event loop");
        return err;
    }
    
    event_registration_t *reg = &loop->registrations[loop->registrations_count++];
    reg->fd = fd;
    reg->waiting_threadlet = threadlet;
    reg->events = events;
    reg->deadline = (timeout_ms > 0) ? ptk_now_ms() + timeout_ms : 0;
    
    threadlet->waiting_fd = fd;
    threadlet->waiting_events = events;
    threadlet->deadline = reg->deadline;
    threadlet_set_status(threadlet, THREADLET_WAITING);
    
    debug("I/O registration complete for fd=%d", fd);
    return PTK_OK;
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
    
    for (size_t i = 0; i < loop->registrations_count; i++) {
        event_registration_t *reg = &loop->registrations[i];
        if (reg->deadline > 0 && current_time >= reg->deadline) {
            warn("Timeout occurred for fd=%d", reg->fd);
            
            ptk_set_err(PTK_ERR_TIMEOUT);
            threadlet_queue_enqueue(loop->ready_queue, reg->waiting_threadlet);
            reg->waiting_threadlet->waiting_fd = -1;
            reg->waiting_threadlet->waiting_events = 0;
            
            event_loop_unregister_io(loop, reg->fd);
            i--;
        }
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