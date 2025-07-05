#pragma once

/**
 * @file ev_loop_platform.h
 * @brief Platform-specific event loop interface
 *
 * This header defines the interface that platform-specific implementations
 * must provide. Each platform (kqueue, epoll, IOCP) implements these functions.
 */

#include "ev_loop.h"

//=============================================================================
// INTERNAL STRUCTURE DEFINITIONS (needed by platform implementations)
//=============================================================================

/**
 * Internal event loop structure (needed by platform implementations)
 */
struct ev_loop {
    // Generic fields
    bool running;
    bool auto_started;
    size_t worker_threads;
    size_t max_events;
    
    // Thread synchronization
    void *threads;           // Array of thread handles (platform-specific)
    void *mutex;             // Mutex for thread safety (platform-specific)
    void *stop_event;        // Event to signal thread stop (platform-specific)
    
    // Socket management
    struct ev_sock **sockets;
    size_t socket_count;
    size_t socket_capacity;
    
    // Platform-specific event handling
    void *platform_data;     // Platform-specific data (epoll/kqueue/IOCP)
};

/**
 * Internal socket structure (needed by platform implementations)
 */
struct ev_sock {
    ev_sock_type type;
    ev_sock_state sock_state;
    ev_callback callback;
    void *user_data;
    
    // Platform-specific socket data
    int fd;                  // File descriptor (Unix) or invalid on Windows
    void *handle;            // Windows socket handle
    
    // Socket state
    bool connected;
    bool listening;
    char remote_host[64];
    int remote_port;
    char local_host[64];
    int local_port;
    
    // Buffer management
    buf *read_buffer;
    size_t read_buffer_size;
    
    // Timer-specific data
    uint64_t timeout_ms;
    bool timer_repeat;
    uint64_t next_fire_time;
};

//=============================================================================
// PLATFORM-SPECIFIC INTERFACE
//=============================================================================

/**
 * Initialize platform-specific event loop data
 *
 * @param loop The event loop to initialize
 * @return EV_OK on success, error code on failure
 */
ev_err platform_init(ev_loop *loop);

/**
 * Start platform-specific worker threads
 *
 * @param loop The event loop
 * @return EV_OK on success, error code on failure
 */
ev_err platform_start_threads(ev_loop *loop);

/**
 * Stop platform-specific worker threads
 *
 * @param loop The event loop
 * @return EV_OK on success, error code on failure
 */
ev_err platform_stop_threads(ev_loop *loop);

/**
 * Cleanup platform-specific event loop data
 *
 * @param loop The event loop to cleanup
 */
void platform_cleanup(ev_loop *loop);

/**
 * Add a socket to the platform-specific event monitoring
 *
 * @param loop The event loop
 * @param sock The socket to add
 * @param events The events to monitor (platform-specific flags)
 * @return EV_OK on success, error code on failure
 */
ev_err platform_add_socket(ev_loop *loop, ev_sock *sock, uint32_t events);

/**
 * Remove a socket from platform-specific event monitoring
 *
 * @param loop The event loop
 * @param sock The socket to remove
 * @return EV_OK on success, error code on failure
 */
ev_err platform_remove_socket(ev_loop *loop, ev_sock *sock);

/**
 * Modify the events being monitored for a socket
 *
 * @param loop The event loop
 * @param sock The socket to modify
 * @param events The new events to monitor
 * @return EV_OK on success, error code on failure
 */
ev_err platform_modify_socket(ev_loop *loop, ev_sock *sock, uint32_t events);

/**
 * Wait for platform-specific worker threads to complete
 *
 * @param loop The event loop
 * @param timeout_ms Maximum time to wait in milliseconds (0 for infinite)
 * @return EV_OK when threads complete, EV_ERR_TIMEOUT on timeout, error code on failure
 */
ev_err platform_wait_threads(ev_loop *loop, uint64_t timeout_ms);

/**
 * Platform-specific network interface discovery
 *
 * @param network_info Pointer to store allocated array of network info structures
 * @param num_networks Pointer to store the number of networks found
 * @return EV_OK on success, error code on failure
 */
ev_err platform_find_networks(ev_network_info **network_info, size_t *num_networks);