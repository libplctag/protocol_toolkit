#ifndef PTK_CONNECTION_H
#define PTK_CONNECTION_H

#include "ptk_types.h"
#include "ptk_slice.h"
#include "ptk_scratch.h"
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in addr;
    uint32_t connect_timeout_ms;
} ptk_tcp_connection_t;

typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in addr;
    uint32_t bind_timeout_ms;
} ptk_udp_connection_t;

typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    int fd;
    char device_path[256];
    int baud_rate;
    uint32_t read_timeout_ms;
} ptk_serial_connection_t;

/**
 * Stack-based connection initialization
 * No memory allocation - all data structures are provided by caller
 */
ptk_status_t ptk_init_tcp_connection(ptk_tcp_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_udp_connection(ptk_udp_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_serial_connection(ptk_serial_connection_t* conn, const char* device, int baud);

/**
 * Connection I/O operations
 * All operations use slices for bounds safety
 */
ptk_slice_t ptk_connection_read(ptk_event_source_t* conn, ptk_slice_t buffer, uint32_t timeout_ms);
ptk_status_t ptk_connection_write(ptk_event_source_t* conn, ptk_slice_t data, uint32_t timeout_ms);
ptk_status_t ptk_connection_close(ptk_event_source_t* conn);

/**
 * Multi-connection event waiting
 * Core abstraction that works across all platforms
 */
int ptk_wait_for_multiple(ptk_event_source_t** sources, 
                         size_t count, 
                         uint32_t timeout_ms);

/**
 * Declare common slice types for connection management
 */
PTK_DECLARE_SLICE_TYPE(tcp_conns, ptk_tcp_connection_t);
PTK_DECLARE_SLICE_TYPE(udp_conns, ptk_udp_connection_t);
PTK_DECLARE_SLICE_TYPE(serial_conns, ptk_serial_connection_t);
PTK_DECLARE_SLICE_TYPE(event_sources, ptk_event_source_t*);

/**
 * Type-safe multi-connection waiting
 */
int ptk_wait_for_multiple_tcp(ptk_slice_tcp_conns_t connections, 
                             uint32_t timeout_ms,
                             ptk_scratch_t* scratch);

#ifdef __cplusplus
}
#endif

#endif /* PTK_CONNECTION_H */