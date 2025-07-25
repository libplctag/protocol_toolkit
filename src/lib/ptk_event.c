/**
 * PTK - Event System Implementation
 * 
 * Provides timer event sources and application events that integrate
 * seamlessly with the connection event system.
 */

#include "ptk_event.h"
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>

/* Forward declarations for time functions */
extern uint64_t ptk_get_time_ms(void);

/* Forward declarations for error handling */
extern void ptk_clear_error(void);
extern void ptk_set_error_internal_internal(ptk_status_t error);


/**
 * Timer operations
 */

extern ptk_status_t ptk_init_timer(ptk_timer_event_source_t* timer, 
                                  uint32_t interval_ms, 
                                  uint32_t id, 
                                  bool repeating) {
    if (!timer || interval_ms == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Initialize timer structure */
    memset(timer, 0, sizeof(*timer));
    timer->base.type = PTK_EVENT_SOURCE_TIMER;
    timer->base.state = 0;
    timer->interval_ms = interval_ms;
    timer->id = id;
    timer->repeating = repeating;
    timer->next_fire_time = 0;
    timer->active = false;
    
    ptk_clear_error();
    return PTK_OK;
}

extern ptk_status_t ptk_timer_start(ptk_timer_event_source_t* timer) {
    if (!timer) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    if (timer->base.type != PTK_EVENT_SOURCE_TIMER) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Set next fire time */
    uint64_t current_time = ptk_get_time_ms();
    timer->next_fire_time = current_time + timer->interval_ms;
    timer->active = true;
    timer->base.state = 0;  /* Clear any previous state */
    
    ptk_clear_error();
    return PTK_OK;
}

extern ptk_status_t ptk_timer_stop(ptk_timer_event_source_t* timer) {
    if (!timer) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    if (timer->base.type != PTK_EVENT_SOURCE_TIMER) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    timer->active = false;
    timer->base.state = 0;
    
    ptk_clear_error();
    return PTK_OK;
}

extern ptk_status_t ptk_timer_reset(ptk_timer_event_source_t* timer) {
    if (!timer) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    if (timer->base.type != PTK_EVENT_SOURCE_TIMER) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    if (timer->active) {
        /* Reset timer to fire after interval from now */
        uint64_t current_time = ptk_get_time_ms();
        timer->next_fire_time = current_time + timer->interval_ms;
    }
    
    timer->base.state = 0;  /* Clear fired state */
    
    ptk_clear_error();
    return PTK_OK;
}

/**
 * Application event operations
 */

extern ptk_status_t ptk_init_app_event(ptk_app_event_source_t* event, uint32_t id) {
    if (!event) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Initialize event structure */
    memset(event, 0, sizeof(*event));
    event->base.type = PTK_EVENT_SOURCE_EVENT;
    event->base.state = 0;
    event->id = id;
    atomic_store(&event->signal_count, 0);
    
    ptk_clear_error();
    return PTK_OK;
}

extern ptk_status_t ptk_app_event_signal(ptk_app_event_source_t* event) {
    if (!event) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    if (event->base.type != PTK_EVENT_SOURCE_EVENT) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Atomically increment signal count */
    uint32_t prev_count = atomic_fetch_add(&event->signal_count, 1);
    
    /* Set state to indicate event is signaled */
    if (prev_count == 0) {
        event->base.state |= PTK_CONN_DATA_READY;
    }
    
    ptk_clear_error();
    return PTK_OK;
}

extern ptk_status_t ptk_app_event_clear(ptk_app_event_source_t* event) {
    if (!event) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    if (event->base.type != PTK_EVENT_SOURCE_EVENT) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Clear signal count and state */
    atomic_store(&event->signal_count, 0);
    event->base.state = 0;
    
    ptk_clear_error();
    return PTK_OK;
}

/**
 * Event connection operations (for thread synchronization)
 */

extern ptk_status_t ptk_init_event_connection(ptk_event_connection_t* conn, uint32_t id) {
    if (!conn) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Initialize connection structure */
    memset(conn, 0, sizeof(*conn));
    conn->id = id;
    atomic_store(&conn->message_count, 0);
    atomic_store(&conn->reader_waiting, 0);
    
    ptk_clear_error();
    return PTK_OK;
}

extern ptk_status_t ptk_event_connection_send(ptk_event_connection_t* conn) {
    if (!conn) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Atomically increment message count */
    atomic_fetch_add(&conn->message_count, 1);
    
    ptk_clear_error();
    return PTK_OK;
}

extern ptk_status_t ptk_event_connection_receive(ptk_event_connection_t* conn, uint32_t timeout_ms) {
    if (!conn) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Set reader waiting flag */
    atomic_store(&conn->reader_waiting, 1);
    
    uint64_t start_time = ptk_get_time_ms();
    uint64_t end_time = start_time + timeout_ms;
    
    /* Wait for message */
    while (atomic_load(&conn->message_count) == 0) {
        uint64_t current_time = ptk_get_time_ms();
        
        if (timeout_ms > 0 && current_time >= end_time) {
            /* Timeout expired */
            atomic_store(&conn->reader_waiting, 0);
            ptk_set_error_internal(PTK_ERROR_TIMEOUT);
            return PTK_ERROR_TIMEOUT;
        }
        
        /* Brief sleep to avoid busy waiting */
        /* In a real implementation, this would use proper synchronization primitives */
        usleep(1000);  /* Sleep for 1ms */
    }
    
    /* Message received, decrement count */
    uint32_t prev_count = atomic_fetch_sub(&conn->message_count, 1);
    if (prev_count == 0) {
        /* This shouldn't happen, but handle gracefully */
        atomic_store(&conn->message_count, 0);
    }
    
    atomic_store(&conn->reader_waiting, 0);
    ptk_clear_error();
    return PTK_OK;
}