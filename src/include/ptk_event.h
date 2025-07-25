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
 * Connection state flags - can be combined
 */
typedef enum {
    PTK_CONN_DATA_READY = 1,    /* Data available to read */
    PTK_CONN_WRITE_READY = 2,   /* Ready for write */
    PTK_CONN_ERROR = 4,         /* Error condition */
    PTK_CONN_CLOSED = 8,        /* Connection closed */
    PTK_CONN_TIMEOUT = 16       /* Timeout occurred */
} ptk_connection_state_t;

/**
 * Event source types for polymorphic event handling
 */
typedef enum {
    PTK_EVENT_SOURCE_TCP = 1,      /* TCP socket */
    PTK_EVENT_SOURCE_UDP = 2,      /* UDP socket */
    PTK_EVENT_SOURCE_SERIAL = 3,   /* Serial port */
    PTK_EVENT_SOURCE_APP_EVENT = 4,    /* Application event */
    PTK_EVENT_SOURCE_TIMER = 5     /* Timer event source */
} ptk_connection_type_t;

/**
 * Base event source - all connection types include this as first element
 * This enables polymorphism through casting
 */
typedef struct {
    ptk_connection_type_t type;     /* Type of event source */
    ptk_connection_state_t state;     /* Current state */
} ptk_connection_t;


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
    ptk_connection_t base;      /* Must be first - enables polymorphism */
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
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    ptk_atomic32_t signal_count;  /* Atomic signal counter */
    uint32_t id;                  /* User-defined event ID */
} ptk_app_event_source_t;


/**
 * Timer operations
 */
ptk_status_t ptk_init_timer(ptk_timer_event_source_t* timer, 
                           uint32_t interval_ms, 
                           uint32_t id, 
                           bool repeating);



/**
 * Connection Abstractions
 * 
 * Unified connection API that treats all I/O sources as event sources.
 * All connections are stack-allocated with no hidden allocations.
 */

/**
 * Transport connections - all are event sources directly
 */
typedef struct {
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in addr;
    uint32_t connect_timeout_ms;
} ptk_tcp_connection_t;

typedef struct {
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in addr;
    uint32_t bind_timeout_ms;
} ptk_udp_connection_t;

typedef struct {
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    int fd;
    char device_path[256];
    int baud_rate;
    uint32_t read_timeout_ms;
} ptk_serial_connection_t;

typedef struct {
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    int fd; /* might not be used */
    uint8_t data[PTK_APP_EVENT_DATA_SIZE]
} ptk_app_event_connection_t;

/**
 * Stack-based connection initialization
 * No memory allocation - all data structures are provided by caller
 */
ptk_status_t ptk_init_tcp_connection(ptk_tcp_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_udp_connection(ptk_udp_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_serial_connection(ptk_serial_connection_t* conn, const char* device, int baud);
ptk_status_t ptk_init_app_event_connection(ptk_app_event_connection_t* conn);

/**
 * Connection I/O operations
 * All operations use slices for bounds safety
 */
ptk_slice_t ptk_connection_read(ptk_connection_t* conn, ptk_slice_t *buffer, uint32_t timeout_ms);
ptk_status_t ptk_connection_write(ptk_connection_t* conn, ptk_slice_t *data, uint32_t timeout_ms);
ptk_status_t ptk_connection_close(ptk_connection_t* conn);


/**
 * Get current time in milliseconds (platform abstracted)
 * Used internally by timer system
 */


/* Universal wait function - polymorphic, works with any event source including timers */
int ptk_wait_for_multiple(ptk_connection_t** event_sources, 
                         size_t count,
                         uint32_t timeout_ms);



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