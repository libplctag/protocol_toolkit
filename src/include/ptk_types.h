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



/* common typedefs */
typedef int64_t ptk_time_ms;
typedef int64_t ptk_duration_ms;


#ifdef __cplusplus
}
#endif

#endif /* PTK_TYPES_H */