#ifndef PTK_TYPES_H
#define PTK_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Core PTK status codes
 */
typedef enum {
    PTK_OK = 0,                    /* Success */
    PTK_ERROR_INVALID_PARAM = 1,   /* Invalid parameter */
    PTK_ERROR_OUT_OF_MEMORY = 2,   /* Out of memory */
    PTK_ERROR_BUFFER_TOO_SMALL = 3,/* Buffer too small */
    PTK_ERROR_SOCKET_CREATE = 4,   /* Socket creation failed */
    PTK_ERROR_CONNECT = 5,         /* Connection failed */
    PTK_ERROR_TIMEOUT = 6,         /* Operation timed out */
    PTK_ERROR_THREAD_CREATE = 7,   /* Thread creation failed */
    PTK_ERROR_DNS_RESOLVE = 8,     /* DNS resolution failed */
    PTK_ERROR_PROTOCOL = 9,        /* Protocol error */
    PTK_ERROR_INVALID_DATA = 10,   /* Invalid data format */
    PTK_ERROR_NOT_CONNECTED = 11,  /* Not connected */
    PTK_ERROR_ALREADY_CONNECTED = 12, /* Already connected */
    PTK_ERROR_INTERRUPTED = 13     /* Operation interrupted */
} ptk_status_t;

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
    PTK_EVENT_SOURCE_EVENT = 4,    /* Application event */
    PTK_EVENT_SOURCE_TIMER = 5     /* Timer event source */
} ptk_event_source_type_t;

/**
 * Base event source - all connection types include this as first element
 * This enables polymorphism through casting
 */
typedef struct {
    ptk_event_source_type_t type;     /* Type of event source */
    ptk_connection_state_t state;     /* Current state */
} ptk_event_source_t;

/**
 * Endianness specification for serialization
 */
typedef enum {
    PTK_ENDIAN_LITTLE = 0,
    PTK_ENDIAN_BIG = 1,
    PTK_ENDIAN_HOST = 2    /* Use host byte order */
} ptk_endian_t;

/**
 * Type information for allocation and type safety
 */
typedef struct {
    size_t size;
    size_t alignment;
} ptk_type_info_t;

/**
 * Atomic types for cross-platform synchronization
 */
typedef volatile uint8_t ptk_atomic8_t;
typedef volatile uint16_t ptk_atomic16_t;
typedef volatile uint32_t ptk_atomic32_t;
typedef volatile uint64_t ptk_atomic64_t;

/* Backward compatibility */
typedef ptk_atomic32_t ptk_atomic_t;

#ifdef __cplusplus
}
#endif

#endif /* PTK_TYPES_H */