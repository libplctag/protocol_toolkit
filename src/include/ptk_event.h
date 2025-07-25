#ifndef PTK_EVENT_H
#define PTK_EVENT_H

#include "ptk_types.h"
#include "ptk_slice.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PTK_APP_EVENT_DATA_SIZE 256

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
} ptk_timer_connection_t;


/**
 * Timer operations
 */
ptk_status_t ptk_init_timer(ptk_timer_connection_t* timer, 
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
} ptk_tcp_client_connection_t;

typedef struct {
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in addr;
    uint32_t connect_timeout_ms;
} ptk_tcp_server_connection_t;

typedef struct {
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    uint32_t bind_timeout_ms;
} ptk_udp_connection_t;




typedef struct {
    ptk_connection_t base;      /* Must be first - enables polymorphism */
    int fd; /* might not be used */
    ptk_slice_bytes_t buffer;   /* User-provided buffer slice */
    size_t data_len;            /* Amount of valid data */
    volatile int8_t data_ready; /* 0 = empty, 1 = full */
    // For POSIX: pthread_mutex_t mutex; pthread_cond_t cond;
} ptk_app_event_connection_t;

/**
 * Stack-based connection initialization
 * No memory allocation - all data structures are provided by caller
 */
ptk_status_t ptk_init_tcp_client_connection(ptk_tcp_client_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_tcp_server_connection(ptk_tcp_server_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_udp_connection(ptk_udp_connection_t* conn, const char* host, uint16_t port);
/**
 * Initialize an app event connection with a user-provided buffer slice.
 * The buffer must remain valid for the lifetime of the connection.
 * @param conn Pointer to the connection struct
 * @param buffer_slice User-provided byte slice
 * @return PTK_OK on success, error code otherwise
 */
ptk_status_t ptk_init_app_event_connection(ptk_app_event_connection_t* conn, ptk_slice_bytes_t buffer_slice);

/**
 * Connection I/O operations
 * All operations use slices for bounds safety
 */
ptk_slice_t ptk_connection_read(ptk_connection_t* conn, ptk_slice_t *buffer, uint32_t timeout_ms);
ptk_status_t ptk_connection_write(ptk_connection_t* conn, ptk_slice_t *data, uint32_t timeout_ms);
ptk_status_t ptk_connection_close(ptk_connection_t* conn);

/**
 * @brief  * TCP server accept operation
 */
ptk_status_t ptk_tcp_server_accept(ptk_tcp_server_connection_t* server, ptk_tcp_client_connection_t* client_conn, uint32_t timeout_ms);


/* Universal wait function - polymorphic, works with any event source including timers */
int ptk_wait_for_multiple(ptk_connection_t** event_sources, 
                         size_t count,
                         uint32_t timeout_ms);



/**
 * Declare slice types for event management
 */
PTK_DECLARE_SLICE_TYPE(timers, ptk_timer_connection_t);
PTK_DECLARE_SLICE_TYPE(app_events, ptk_app_event_connection_t);

#ifdef __cplusplus
}
#endif

#endif /* PTK_EVENT_H */