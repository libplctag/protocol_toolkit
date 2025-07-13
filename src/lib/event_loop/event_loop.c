#include "event_loop.h"
#include "threadlet_scheduler.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>
#include <ptk_err.h>



// Thread-local event loop instance
__thread event_loop_t *thread_local_event_loop = NULL;

// Forward declarations for threadlet integration
extern threadlet_t *threadlet_get_current(void);
extern void threadlet_yield_to_scheduler(threadlet_t *threadlet);
extern void threadlet_run_until_yield(event_loop_t *loop, threadlet_t *threadlet);

// Helper to calculate timeout for epoll_wait
static int calculate_next_timeout(event_loop_t *loop) {
    if (timeout_heap_is_empty(loop->timeouts)) {
        return -1; // No timeouts, block indefinitely
    }
    
    ptk_time_ms next_deadline = timeout_heap_next_deadline(loop->timeouts);
    ptk_time_ms now = ptk_now_ms();
    
    if (next_deadline <= now) {
        return 0; // Already expired
    }
    
    ptk_time_ms diff = next_deadline - now;
    return (int)(diff > INT32_MAX ? INT32_MAX : diff);
}

// Process expired timeouts
static void process_timeouts(event_loop_t *loop) {
    ptk_time_ms now = loop->current_time_ms;
    
    while (!timeout_heap_is_empty(loop->timeouts)) {
        timeout_entry_t *entry = timeout_heap_peek(loop->timeouts);
        if (entry->deadline > now) {
            break; // No more expired timeouts
        }
        
        // Timeout expired - wake waiting threadlet
        event_registration_t *reg = event_registration_lookup(loop->registrations, entry->fd);
        if (reg && reg->waiting_threadlet) {
            trace("Timeout expired for fd=%d", entry->fd);
            threadlet_wake(reg->waiting_threadlet, THREADLET_READY);
            threadlet_queue_enqueue(&loop->ready_queue, reg->waiting_threadlet);
            event_registration_remove(loop->registrations, entry->fd);
            platform_remove_fd(loop->platform, entry->fd);
        }
        
        timeout_heap_pop(loop->timeouts);
    }
}

// Destructor for event loop
static void event_loop_destructor(void *ptr) {
    event_loop_t *loop = (event_loop_t *)ptr;
    if (!loop) return;
    
    debug("Destroying event loop");
    
    if (loop->platform) {
        platform_event_loop_destroy(loop->platform);
    }
    
    if (loop->registrations) {
        event_registration_table_destroy(loop->registrations);
    }
    
    if (loop->timeouts) {
        timeout_heap_destroy(loop->timeouts);
    }
    
    threadlet_queue_cleanup(&loop->ready_queue);
    threadlet_queue_cleanup(&loop->waiting_queue);
}

event_loop_t *event_loop_create(int max_events) {
    debug("Creating event loop with max_events=%d", max_events);
    
    event_loop_t *loop = ptk_alloc(sizeof(event_loop_t), event_loop_destructor);
    if (!loop) {
        warn("Failed to allocate event loop");
        return NULL;
    }
    
    memset(loop, 0, sizeof(event_loop_t));
    
    // Initialize platform-specific event polling
    loop->platform = platform_event_loop_create(max_events);
    if (!loop->platform) {
        warn("Failed to create platform event loop");
        ptk_free(loop);
        return NULL;
    }
    
    // Initialize threadlet queues
    if (threadlet_queue_init(&loop->ready_queue, 32) != PTK_OK) {
        warn("Failed to initialize ready queue");
        ptk_free(loop);
        return NULL;
    }
    
    if (threadlet_queue_init(&loop->waiting_queue, 32) != PTK_OK) {
        warn("Failed to initialize waiting queue");
        ptk_free(loop);
        return NULL;
    }
    
    // Initialize event tracking
    loop->registrations = event_registration_table_create(64);
    if (!loop->registrations) {
        warn("Failed to create event registration table");
        ptk_free(loop);
        return NULL;
    }
    
    loop->timeouts = timeout_heap_create(32);
    if (!loop->timeouts) {
        warn("Failed to create timeout heap");
        ptk_free(loop);
        return NULL;
    }
    
    loop->running = false;
    loop->current_time_ms = ptk_now_ms();
    
    debug("Event loop created successfully");
    return loop;
}

void event_loop_destroy(event_loop_t *loop) {
    if (loop) {
        debug("Destroying event loop");
        ptk_free(loop);
    }
}

void event_loop_run(event_loop_t *loop) {
    if (!loop) {
        warn("Cannot run NULL event loop");
        return;
    }
    
    info("Starting event loop");
    loop->running = true;
    
    while (loop->running) {
        // Update current time
        event_loop_update_time(loop);
        
        // 1. Calculate next timeout
        int timeout_ms = calculate_next_timeout(loop);
        
        // 2. Poll for I/O events
        platform_event_list_t ready_events;
        ready_events.events = ptk_alloc(loop->platform->max_events * sizeof(platform_event_t), NULL);
        if (!ready_events.events) {
            warn("Failed to allocate event buffer");
            break;
        }
        
        int nfds = platform_poll_events(loop->platform, &ready_events, timeout_ms);
        
        // 3. Process I/O events - wake waiting threadlets
        for (int i = 0; i < nfds; i++) {
            int fd = ready_events.events[i].fd;
            event_registration_t *reg = event_registration_lookup(loop->registrations, fd);
            if (reg && reg->waiting_threadlet) {
                trace("I/O ready for fd=%d", fd);
                threadlet_wake(reg->waiting_threadlet, THREADLET_READY);
                threadlet_queue_enqueue(&loop->ready_queue, reg->waiting_threadlet);
                event_registration_remove(loop->registrations, fd);
                platform_remove_fd(loop->platform, fd);
            }
        }
        
        ptk_free(ready_events.events);
        
        // 4. Process timeouts
        process_timeouts(loop);
        
        // 5. Run ready threadlets until they yield/block
        while (!threadlet_queue_is_empty(&loop->ready_queue)) {
            threadlet_t *t = threadlet_queue_dequeue(&loop->ready_queue);
            threadlet_run_until_yield(loop, t);
        }
    }
    
    info("Event loop stopped");
}

void event_loop_stop(event_loop_t *loop) {
    if (loop) {
        info("Stopping event loop");
        loop->running = false;
    }
}

ptk_err event_loop_wait_fd(event_loop_t *loop, int fd, uint32_t events, ptk_duration_ms timeout_ms) {
    if (!loop || fd < 0) {
        warn("Invalid arguments to event_loop_wait_fd");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    threadlet_t *current = loop->current_threadlet;
    if (!current) {
        warn("event_loop_wait_fd called outside threadlet context");
        return PTK_ERR_INVALID_STATE;
    }
    
    trace("Threadlet waiting on fd=%d, events=0x%x, timeout=%u", fd, events, timeout_ms);
    
    // Add to platform event polling
    if (platform_add_fd(loop->platform, fd, events) != PTK_OK) {
        warn("Failed to add fd to platform event loop");
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Calculate deadline
    ptk_time_ms deadline = (timeout_ms == 0) ? 0 : loop->current_time_ms + timeout_ms;
    
    // Register threadlet as waiting
    ptk_err err = event_registration_add(loop->registrations, fd, current, events, deadline);
    if (err != PTK_OK) {
        platform_remove_fd(loop->platform, fd);
        return err;
    }
    
    // Add timeout if specified
    if (deadline > 0) {
        if (timeout_heap_add(loop->timeouts, fd, deadline) != PTK_OK) {
            warn("Failed to add timeout for fd=%d", fd);
            // Continue anyway - worst case is no timeout
        }
    }
    
    // Move to waiting queue for tracking
    threadlet_queue_enqueue(&loop->waiting_queue, current);
    
    // Suspend threadlet - yield back to event loop
    threadlet_set_status(current, THREADLET_WAITING);
    threadlet_yield_to_scheduler(current);
    
    // When we resume, check why we woke up
    threadlet_status_t status = threadlet_get_status(current);
    if (status == THREADLET_READY) {
        return PTK_OK;  // I/O ready
    } else if (status == THREADLET_ABORTED) {
        return PTK_ERR_ABORTED;
    } else {
        return PTK_ERR_TIMEOUT;
    }
}

ptk_err event_loop_signal_fd(event_loop_t *loop, int fd) {
    if (!loop || fd < 0) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Wake waiting threadlet manually
    event_registration_t *reg = event_registration_lookup(loop->registrations, fd);
    if (reg && reg->waiting_threadlet) {
        trace("Manually signaling fd=%d", fd);
        threadlet_wake(reg->waiting_threadlet, THREADLET_READY);
        threadlet_queue_enqueue(&loop->ready_queue, reg->waiting_threadlet);
        event_registration_remove(loop->registrations, fd);
        platform_remove_fd(loop->platform, fd);
        return PTK_OK;
    }
    
    return PTK_ERR_NOT_FOUND;
}

event_loop_t *get_thread_local_event_loop(void) {
    return thread_local_event_loop;
}

event_loop_t *init_thread_event_loop(int max_events) {
    if (thread_local_event_loop) {
        return thread_local_event_loop;
    }
    
    thread_local_event_loop = event_loop_create(max_events);
    return thread_local_event_loop;
}

ptk_time_ms event_loop_get_current_time(event_loop_t *loop) {
    return loop ? loop->current_time_ms : ptk_now_ms();
}

void event_loop_update_time(event_loop_t *loop) {
    if (loop) {
        loop->current_time_ms = ptk_now_ms();
    }
}