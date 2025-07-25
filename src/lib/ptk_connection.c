/**
 * PTK - Connection Layer Implementation
 * 
 * Platform-independent connection abstractions that delegate to
 * platform-specific implementations.
 */

#include "ptk_connection.h"
#include <string.h>

/* Forward declarations for platform-specific functions */
extern ptk_status_t ptk_init_tcp_connection(ptk_tcp_connection_t* conn, const char* host, uint16_t port);
extern ptk_status_t ptk_init_udp_connection(ptk_udp_connection_t* conn, const char* host, uint16_t port);
extern ptk_status_t ptk_init_serial_connection(ptk_serial_connection_t* conn, const char* device, int baud);
extern ptk_slice_t ptk_connection_read(ptk_event_source_t* conn, ptk_slice_t buffer, uint32_t timeout_ms);
extern ptk_status_t ptk_connection_write(ptk_event_source_t* conn, ptk_slice_t data, uint32_t timeout_ms);
extern ptk_status_t ptk_connection_close(ptk_event_source_t* conn);
extern int ptk_wait_for_multiple(ptk_event_source_t** sources, size_t count, uint32_t timeout_ms);

/* Forward declarations for error handling */
extern void ptk_clear_error(void);
extern void ptk_set_error_internal_internal(ptk_status_t error);


/**
 * Connection initialization - these delegate to platform-specific implementations
 * The actual implementations are in src/lib/posix/ptk_socket_posix.c
 */

/* TCP connection initialization is already implemented in platform-specific code */
/* UDP connection initialization is already implemented in platform-specific code */
/* Serial connection initialization is already implemented in platform-specific code */

/**
 * Polymorphic I/O operations - these delegate to platform-specific implementations
 * The actual implementations are in src/lib/posix/ptk_socket_posix.c
 */

/* ptk_connection_read is already implemented in platform-specific code */
/* ptk_connection_write is already implemented in platform-specific code */
/* ptk_connection_close is already implemented in platform-specific code */

/**
 * Multi-connection event waiting - delegates to platform-specific implementation
 * The actual implementation for Linux is in src/lib/linux/ptk_epoll.c
 */

/* ptk_wait_for_multiple is already implemented in platform-specific code */

/**
 * Type-safe multi-connection waiting - delegates to platform-specific implementation
 * The actual implementation for Linux is in src/lib/linux/ptk_epoll.c
 */

/* ptk_wait_for_multiple_tcp is already implemented in platform-specific code */

/**
 * Additional helper functions for connection management
 */

/**
 * Get the file descriptor from an event source (if applicable)
 */
static int ptk_get_connection_fd(ptk_event_source_t* source) {
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
 * Check if an event source represents a network connection
 */
static bool ptk_is_network_connection(ptk_event_source_t* source) {
    if (!source) {
        return false;
    }
    
    return (source->type == PTK_EVENT_SOURCE_TCP ||
            source->type == PTK_EVENT_SOURCE_UDP ||
            source->type == PTK_EVENT_SOURCE_SERIAL);
}

/**
 * Validate event source array
 */
static ptk_status_t ptk_validate_event_sources(ptk_event_source_t** sources, size_t count) {
    if (!sources) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (!sources[i]) {
            return PTK_ERROR_INVALID_PARAM;
        }
        
        /* Verify event source type is valid */
        switch (sources[i]->type) {
            case PTK_EVENT_SOURCE_TCP:
            case PTK_EVENT_SOURCE_UDP:
            case PTK_EVENT_SOURCE_SERIAL:
            case PTK_EVENT_SOURCE_TIMER:
            case PTK_EVENT_SOURCE_EVENT:
                /* Valid types */
                break;
            default:
                return PTK_ERROR_INVALID_PARAM;
        }
        
        /* For network connections, verify they have valid file descriptors */
        if (ptk_is_network_connection(sources[i])) {
            int fd = ptk_get_connection_fd(sources[i]);
            if (fd == -1) {
                return PTK_ERROR_NOT_CONNECTED;
            }
        }
    }
    
    return PTK_OK;
}

/**
 * Helper function to initialize all connection states to zero
 */
static void ptk_clear_connection_states(ptk_event_source_t** sources, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (sources[i]) {
            sources[i]->state = 0;
        }
    }
}

/**
 * Count the number of ready event sources
 */
static int ptk_count_ready_sources(ptk_event_source_t** sources, size_t count) {
    int ready_count = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (sources[i] && sources[i]->state != 0) {
            ready_count++;
        }
    }
    
    return ready_count;
}

/**
 * Wrapper function for event waiting with validation
 * This provides additional error checking and validation before delegating
 * to the platform-specific implementation.
 */
int ptk_wait_for_multiple_validated(ptk_event_source_t** sources, size_t count, uint32_t timeout_ms) {
    /* Validate parameters */
    if (count == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return -1;
    }
    
    /* Validate event sources */
    ptk_status_t status = ptk_validate_event_sources(sources, count);
    if (status != PTK_OK) {
        ptk_set_error_internal(status);
        return -1;
    }
    
    /* Clear all connection states before waiting */
    ptk_clear_connection_states(sources, count);
    
    /* Delegate to platform-specific implementation */
    int result = ptk_wait_for_multiple(sources, count, timeout_ms);
    
    /* The platform-specific implementation sets the error if needed */
    return result;
}

/**
 * Helper function to convert slice of TCP connections to event sources
 */
static ptk_status_t ptk_tcp_slice_to_event_sources(ptk_slice_tcp_conns_t connections,
                                                  ptk_event_source_t*** sources_out,
                                                  ptk_scratch_t* scratch) {
    if (ptk_slice_tcp_conns_is_empty(connections) || !sources_out || !scratch) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Allocate array of event source pointers */
    ptk_slice_event_sources_t sources_slice = ptk_scratch_alloc_slice(scratch, event_sources, connections.len);
    if (ptk_slice_event_sources_is_empty(sources_slice)) {
        return PTK_ERROR_OUT_OF_MEMORY;
    }
    
    /* Convert TCP connections to event sources */
    for (size_t i = 0; i < connections.len; i++) {
        sources_slice.data[i] = (ptk_event_source_t*)&connections.data[i];
    }
    
    *sources_out = sources_slice.data;
    return PTK_OK;
}

/**
 * Enhanced type-safe multi-connection waiting with additional validation
 */
int ptk_wait_for_multiple_tcp_validated(ptk_slice_tcp_conns_t connections, uint32_t timeout_ms, ptk_scratch_t* scratch) {
    if (ptk_slice_tcp_conns_is_empty(connections) || !scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return -1;
    }
    
    /* Convert to event sources */
    ptk_event_source_t** sources;
    ptk_status_t status = ptk_tcp_slice_to_event_sources(connections, &sources, scratch);
    if (status != PTK_OK) {
        ptk_set_error_internal(status);
        return -1;
    }
    
    /* Use the validated event waiting function */
    return ptk_wait_for_multiple_validated(sources, connections.len, timeout_ms);
}