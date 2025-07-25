/**
 * PTK - Linux epoll Implementation
 * 
 * Linux epoll-based event multiplexing for optimal performance.
 */

#include "ptk_connection.h"
#include "ptk_event.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* Forward declarations for time functions */
extern uint64_t ptk_get_time_ms(void);

/* Forward declarations for error handling */
extern void ptk_clear_error(void);
extern void ptk_set_error_internal_internal(ptk_status_t error);


/**
 * Get file descriptor from event source
 */
static int get_event_source_fd(ptk_event_source_t* source) {
    if (!source) {
        return -1;
    }
    
    switch (source->type) {
        case PTK_EVENT_SOURCE_TCP:
            return ((ptk_tcp_connection_t*)source)->fd;
        case PTK_EVENT_SOURCE_UDP:
            return ((ptk_udp_connection_t*)source)->fd;
        case PTK_EVENT_SOURCE_SERIAL:
            return ((ptk_serial_connection_t*)source)->fd;
        case PTK_EVENT_SOURCE_TIMER:
        case PTK_EVENT_SOURCE_EVENT:
            /* These don't have file descriptors */
            return -1;
        default:
            return -1;
    }
}

/**
 * Update connection state based on epoll events
 */
static void update_connection_state(ptk_event_source_t* source, uint32_t events) {
    /* Clear previous state */
    source->state = 0;
    
    if (events & EPOLLIN) {
        source->state |= PTK_CONN_DATA_READY;
    }
    
    if (events & EPOLLOUT) {
        source->state |= PTK_CONN_WRITE_READY;
    }
    
    if (events & (EPOLLERR | EPOLLHUP)) {
        source->state |= PTK_CONN_ERROR;
    }
    
    if (events & EPOLLRDHUP) {
        source->state |= PTK_CONN_CLOSED;
    }
}

/**
 * Check and update timer states
 */
static void update_timer_states(ptk_event_source_t** sources, size_t count) {
    uint64_t current_time = ptk_get_time_ms();
    
    for (size_t i = 0; i < count; i++) {
        if (sources[i]->type == PTK_EVENT_SOURCE_TIMER) {
            ptk_timer_event_source_t* timer = (ptk_timer_event_source_t*)sources[i];
            
            if (timer->active && current_time >= timer->next_fire_time) {
                /* Timer has expired */
                timer->base.state |= PTK_CONN_DATA_READY;
                
                if (timer->repeating) {
                    /* Schedule next firing */
                    timer->next_fire_time = current_time + timer->interval_ms;
                } else {
                    /* One-shot timer, deactivate */
                    timer->active = false;
                }
            }
        }
    }
}

/**
 * Get next timer expiration time
 */
static uint32_t get_next_timer_timeout(ptk_event_source_t** sources, size_t count, uint32_t max_timeout) {
    uint64_t current_time = ptk_get_time_ms();
    uint64_t next_expiry = current_time + max_timeout;
    bool found_timer = false;
    
    for (size_t i = 0; i < count; i++) {
        if (sources[i]->type == PTK_EVENT_SOURCE_TIMER) {
            ptk_timer_event_source_t* timer = (ptk_timer_event_source_t*)sources[i];
            
            if (timer->active && timer->next_fire_time < next_expiry) {
                next_expiry = timer->next_fire_time;
                found_timer = true;
            }
        }
    }
    
    if (!found_timer) {
        return max_timeout;
    }
    
    if (next_expiry <= current_time) {
        return 0;  /* Timer already expired */
    }
    
    uint64_t timeout_ms = next_expiry - current_time;
    return (uint32_t)(timeout_ms > max_timeout ? max_timeout : timeout_ms);
}

/**
 * Multi-connection event waiting using epoll
 * Core abstraction that works across all platforms
 */
extern int ptk_wait_for_multiple(ptk_event_source_t** sources, size_t count, uint32_t timeout_ms) {
    if (!sources || count == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return -1;
    }
    
    /* Create epoll instance */
    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        ptk_set_error_internal(PTK_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    
    /* Add file descriptors to epoll */
    size_t fd_count = 0;
    for (size_t i = 0; i < count; i++) {
        int fd = get_event_source_fd(sources[i]);
        if (fd != -1) {
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET;
            ev.data.u64 = i;  /* Store source index */
            
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                close(epoll_fd);
                ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
                return -1;
            }
            fd_count++;
        }
        
        /* Clear previous state */
        sources[i]->state = 0;
    }
    
    /* Adjust timeout for timers */
    uint32_t actual_timeout = get_next_timer_timeout(sources, count, timeout_ms);
    
    /* Wait for events */
    struct epoll_event events[16];  /* Handle up to 16 events at once */
    int max_events = (fd_count < 16) ? (int)fd_count : 16;
    
    int ready = epoll_wait(epoll_fd, events, max_events, (int)actual_timeout);
    
    if (ready == -1) {
        close(epoll_fd);
        if (errno == EINTR) {
            ptk_set_error_internal(PTK_ERROR_INTERRUPTED);
        } else {
            ptk_set_error_internal(PTK_ERROR_TIMEOUT);
        }
        return -1;
    }
    
    /* Process epoll events */
    for (int i = 0; i < ready; i++) {
        size_t source_index = (size_t)events[i].data.u64;
        if (source_index < count) {
            update_connection_state(sources[source_index], events[i].events);
        }
    }
    
    /* Update timer states */
    update_timer_states(sources, count);
    
    /* Count total ready sources */
    int total_ready = 0;
    for (size_t i = 0; i < count; i++) {
        if (sources[i]->state != 0) {
            total_ready++;
        }
    }
    
    close(epoll_fd);
    ptk_clear_error();
    return total_ready;
}

/**
 * Type-safe multi-connection waiting using epoll
 */
extern int ptk_wait_for_multiple_tcp(ptk_slice_tcp_conns_t connections, uint32_t timeout_ms, ptk_scratch_t* scratch) {
    if (ptk_slice_tcp_conns_is_empty(connections) || !scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return -1;
    }
    
    /* Allocate array of event source pointers using scratch allocator */
    ptk_slice_event_sources_t sources = ptk_scratch_alloc_slice(scratch, event_sources, connections.len);
    if (ptk_slice_event_sources_is_empty(sources)) {
        ptk_set_error_internal(PTK_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    
    /* Convert TCP connections to event sources */
    for (size_t i = 0; i < connections.len; i++) {
        sources.data[i] = (ptk_event_source_t*)&connections.data[i];
    }
    
    /* Use the main event waiting function */
    return ptk_wait_for_multiple(sources.data, sources.len, timeout_ms);
}