#ifndef PTK_EVENT_H
#define PTK_EVENT_H

#include "ptk_types.h"
#include "ptk_slice.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Event Sources and Timers
 * 
 * Provides timer event sources and application events that integrate
 * seamlessly with the connection event system.
 */

/**
 * Timer event source
 * Integrates with ptk_wait_for_multiple() alongside socket connections
 */
typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    uint32_t interval_ms;         /* Timer interval in milliseconds */
    uint32_t id;                  /* User-defined timer ID */
    bool repeating;               /* Whether timer repeats automatically */
    uint64_t next_fire_time;      /* Internal: next fire time */
    bool active;                  /* Internal: timer is active */
} ptk_timer_event_source_t;

/**
 * Application/user event source
 * Thread-safe signaling mechanism for inter-thread communication
 */
typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    ptk_atomic32_t signal_count;  /* Atomic signal counter */
    uint32_t id;                  /* User-defined event ID */
} ptk_app_event_source_t;

/**
 * Event connection for thread-safe messaging
 * Replaces traditional semaphores/mutexes with event-based coordination
 */
typedef struct {
    ptk_atomic32_t message_count; /* Number of pending messages */
    ptk_atomic32_t reader_waiting; /* Reader waiting flag */
    uint32_t id;                  /* Connection identifier */
} ptk_event_connection_t;

/**
 * Timer operations
 */
ptk_status_t ptk_init_timer(ptk_timer_event_source_t* timer, 
                           uint32_t interval_ms, 
                           uint32_t id, 
                           bool repeating);

ptk_status_t ptk_timer_start(ptk_timer_event_source_t* timer);
ptk_status_t ptk_timer_stop(ptk_timer_event_source_t* timer);
ptk_status_t ptk_timer_reset(ptk_timer_event_source_t* timer);

/**
 * Application event operations
 */
ptk_status_t ptk_init_app_event(ptk_app_event_source_t* event, uint32_t id);
ptk_status_t ptk_app_event_signal(ptk_app_event_source_t* event);
ptk_status_t ptk_app_event_clear(ptk_app_event_source_t* event);

/**
 * Event connection operations (for thread synchronization)
 */
ptk_status_t ptk_init_event_connection(ptk_event_connection_t* conn, uint32_t id);
ptk_status_t ptk_event_connection_send(ptk_event_connection_t* conn);
ptk_status_t ptk_event_connection_receive(ptk_event_connection_t* conn, uint32_t timeout_ms);

/**
 * Get current time in milliseconds (platform abstracted)
 * Used internally by timer system
 */
uint64_t ptk_get_time_ms(void);

/**
 * Declare slice types for event management
 */
PTK_DECLARE_SLICE_TYPE(timers, ptk_timer_event_source_t);
PTK_DECLARE_SLICE_TYPE(app_events, ptk_app_event_source_t);
PTK_DECLARE_SLICE_TYPE(event_conns, ptk_event_connection_t);

#ifdef __cplusplus
}
#endif

#endif /* PTK_EVENT_H */