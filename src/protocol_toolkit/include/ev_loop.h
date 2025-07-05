#pragma once

/**
 * @file ev_loop.h
 * @brief Simplified event loop API for network programming
 *
 * A clean, unified API that replaces libuv with simpler semantics:
 * - Single callback type for all events
 * - Clear buffer ownership model.
 * - Configuration-based object creation
 * - Event-driven design with minimal boilerplate
 *
 *
 * Buffer ownership: The callee owns the buffer.  When a pointer to a pointer
 * to a buffer is passed to a function, the function owns the buffer and needs
 * to release its resources.
 */

#include "buf.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ev_buf.h"
#include "ev_err.h"


// Forward declarations
typedef struct ev_loop ev_loop;
typedef struct ev_sock ev_sock;

/**
 * Error codes for event loop operations
 */
typedef enum {
    EV_OK = BUF_OK,
    EV_ERR_OUT_OF_BOUNDS = BUF_ERR_OUT_OF_BOUNDS,
    EV_ERR_NULL_PTR = BUF_ERR_NULL_PTR,
    EV_ERR_NO_RESOURCES = BUF_ERR_NO_RESOURCES,
    EV_ERR_BAD_FORMAT = BUF_ERR_BAD_FORMAT,
    EV_ERR_INVALID_PARAM,      // Invalid parameter passed
    EV_ERR_NETWORK_ERROR,      // Network operation failed
    EV_ERR_CLOSED,             // Socket is closed
    EV_ERR_TIMEOUT,            // Operation timed out
    EV_ERR_WOULD_BLOCK,        // Operation would block
    EV_ERR_ADDRESS_IN_USE,     // Address already in use
    EV_ERR_CONNECTION_REFUSED, // Connection refused by remote
    EV_ERR_HOST_UNREACHABLE,   // Host unreachable
} ev_err;

/**
 * Event types delivered to callbacks
 */
typedef enum {
    EV_EVENT_ACCEPT,           // New client connected (server sockets)
    EV_EVENT_CONNECT,          // Connection established (client sockets)
    EV_EVENT_READ,             // Data received, for both TCP client and UDP sockets
    EV_EVENT_WRITE_DONE,       // Write operation completed, for both TCP client and UDP socket.
    EV_EVENT_CLOSE,            // Connection closed, for all socket types.
    EV_EVENT_ERROR,            // Error occurred, for all socket types.
    EV_EVENT_TICK,             // Timer tick, for all socket types.
} ev_event_type;

/**
 * Socket connection states maintained by the library
 */
typedef enum {
    EV_SOCK_STATE_CREATED,     // Socket created but not connected/listening
    EV_SOCK_STATE_CONNECTING,  // TCP client connecting
    EV_SOCK_STATE_LISTENING,   // TCP server listening
    EV_SOCK_STATE_CONNECTED,   // TCP connection established
    EV_SOCK_STATE_UDP_BOUND,   // UDP socket bound and ready
    EV_SOCK_STATE_CLOSING,     // Socket closing
    EV_SOCK_STATE_CLOSED,      // Socket closed
    EV_SOCK_STATE_ERROR,       // Socket in error state
} ev_sock_state;


/**
 * Socket types
 */
typedef enum {
    EV_SOCK_TCP_SERVER,        // TCP listening socket
    EV_SOCK_TCP_CLIENT,        // TCP client socket
    EV_SOCK_UDP,               // UDP socket
    EV_SOCK_TIMER,             // Timer object
} ev_sock_type;

/**
 * Event structure passed to callbacks
 *
 * The library owns the resources of the event struct
 * itself.  The event callback must not free the event.
 */
typedef struct {
    ev_event_type type;        // Type of event
    ev_sock *sock;             // Socket that generated the event
    buf **data;                // Data buffer, callee ownership (for read events, NULL otherwise)
    const char *remote_host;   // Remote host (for accept/connect/UDP events)
    int64_t event_time_ms;     // Event time in milliseconds since epoch
    int remote_port;           // Remote port (for accept/connect/UDP events)
    ev_err error;              // Error code (for error events)
    ev_sock_state sock_state;  // Current socket state
    void *user_data;           // User data passed during socket creation
} ev_event;

/**
 * Event callback function type
 *
 * @param event The event that occurred
 */
typedef void (*ev_callback)(const ev_event *event);

//=============================================================================
// EVENT LOOP MANAGEMENT
//=============================================================================

/**
 * Configuration for event loop creation
 */
typedef struct {
    size_t worker_threads;     // Number of background threads (default: CPU count if 0)
    size_t max_events;         // Max events per loop iteration (default: 1024 if 0)
    bool auto_start;           // Start background threads immediately (default: true)
} ev_loop_opts;

/**
 * Create a new event loop with background threads
 *
 * The event loop automatically starts one or more background threads to handle
 * I/O operations. Callbacks are invoked on these background threads.
 *
 * @param loop Pointer to store the created loop
 * @param opts Loop configuration (NULL for defaults)
 * @return EV_OK on success, error code on failure
 */
ev_err ev_loop_create(ev_loop **loop, const ev_loop_opts *opts);

/**
 * Wait for the event loop to finish
 *
 * This function blocks until ev_loop_stop() is called from a callback
 * or another thread. Use this instead of ev_loop_run() for background operation.
 *
 * @param loop The event loop to wait for
 * @return EV_OK when loop exits normally, error code on failure
 */
ev_err ev_loop_wait(ev_loop *loop);

/**
 * Wait for the event loop to finish with timeout
 *
 * @param loop The event loop to wait for
 * @param timeout_ms Maximum time to wait in milliseconds (0 for no timeout)
 * @return EV_OK when loop exits, EV_ERR_TIMEOUT on timeout, error code on failure
 */
ev_err ev_loop_wait_timeout(ev_loop *loop, uint64_t timeout_ms);

/**
 * Stop a running event loop
 *
 * This can be called from within a callback or from another thread.
 * The loop will stop after processing current events and ev_loop_wait() will return.
 *
 * @param loop The event loop to stop
 */
void ev_loop_stop(ev_loop *loop);

/**
 * Check if an event loop is running
 *
 * @param loop The event loop to check
 * @return true if running, false if stopped
 */
bool ev_loop_is_running(ev_loop *loop);

/**
 * Destroy an event loop and free its resources
 *
 * This automatically stops the loop and waits for background threads to exit.
 * All sockets associated with the loop are automatically closed.
 *
 * @param loop The event loop to destroy
 */
void ev_loop_destroy(ev_loop *loop);

/**
 * Post a callback to run on the next event loop iteration
 *
 * @param loop The event loop
 * @param callback The callback to run
 * @param user_data Data to pass to the callback
 * @return EV_OK on success, error code on failure
 */
ev_err ev_loop_post(ev_loop *loop, void (*callback)(void *user_data), void *user_data);


//=============================================================================
// TCP CLIENT
//=============================================================================

/**
 * Configuration for TCP client creation
 */
typedef struct {
    const char *host;          // Remote host to connect to
    int port;                  // Remote port to connect to
    ev_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    uint32_t connect_timeout_ms; // Connection timeout (default: 30000 if 0)
    bool keep_alive;           // Enable TCP keep-alive (default: false)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ev_tcp_client_opts;

/**
 * Connect to a TCP server
 *
 * Connection is performed asynchronously. An EV_EVENT_CONNECT event
 * is delivered when the connection is established or fails.
 *
 * @param loop The event loop
 * @param client Pointer to a pointer at which to store the created client socket
 * @param opts Client configuration
 * @return EV_OK on success, error code on failure
 */
ev_err ev_tcp_connect(ev_loop *loop, ev_sock **client, const ev_tcp_client_opts *opts);

/**
 * Write data to a TCP socket
 *
 * The operation is performed asynchronously. An EV_EVENT_WRITE_DONE
 * event is delivered when the write completes. The library takes
 * ownership of the buffer and will free it after writing.
 *
 * The library owns the data buffer after this call.  The buffer is
 * passed as a pointer to a pointer.  The library nulls out the
 * caller's pointer to the buffer when this is called.  The library
 * takes owernship of the buffer data and frees it when done.
 *
 * @param sock The TCP client socket to write to
 * @param data Data to write (ownership transferred)
 * @return EV_OK on success, error code on failure
 */
ev_err ev_tcp_write(ev_sock *sock, buf **data);

//=============================================================================
// TCP Server
//=============================================================================

/**
 * Configuration for TCP server creation
 */
typedef struct {
    const char *bind_host;     // Host to bind to ("0.0.0.0" for all interfaces)
    int bind_port;             // Port to listen on
    int backlog;               // Listen backlog (default: 128 if 0)
    ev_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    bool reuse_addr;           // Enable SO_REUSEADDR (default: true)
    bool keep_alive;           // Enable TCP keep-alive (default: false)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ev_tcp_server_opts;

/**
 * Start a TCP server
 *
 * The server will immediately start listening and accepting connections.
 * For each new connection, an EV_EVENT_ACCEPT event is delivered.
 *
 * @param loop The event loop
 * @param server Pointer to store the created server socket
 * @param server_opts Server configuration
 * @return EV_OK on success, error code on failure
 */
ev_err ev_tcp_server_start(ev_loop *loop, ev_sock **server, const ev_tcp_server_opts *server_opts);

//=============================================================================
// UDP SOCKETS
//=============================================================================

/**
 * Configuration for UDP socket creation
 */
typedef struct {
    const char *bind_host;     // Host to bind to (NULL for client-only)
    int bind_port;             // Port to bind to (0 for client-only)
    ev_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    bool broadcast;            // Enable broadcast (default: false)
    bool reuse_addr;           // Enable SO_REUSEADDR (default: true)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)

    // Multicast settings
    const char *multicast_group; // Multicast group to join (e.g., "224.0.0.1")
    const char *multicast_interface; // Interface for multicast (NULL for default)
    uint8_t multicast_ttl;     // Multicast TTL (default: 1 if 0)
    bool multicast_loop;       // Enable multicast loopback (default: false)
} ev_udp_opts;

/**
 * Create a UDP socket
 *
 * Can be used for both client and server operations.
 * If bind_host/bind_port are specified, the socket is bound for receiving.
 *
 * @param loop The event loop
 * @param udp_sock Pointer to store the created UDP socket
 * @param opts UDP socket configuration
 * @return EV_OK on success, error code on failure
 */
ev_err ev_udp_create(ev_loop *loop, ev_sock **udp_sock, const ev_udp_opts *opts);

/**
 * Send UDP data to a specific address
 *
 * The operation is performed asynchronously. An EV_EVENT_UDP_WRITE_DONE
 * event is delivered when the send completes.
 *
 * The buffer is passed as a pointer to a pointer.  The library takes
 * owenership of the buffer and nulls out the caller's pointer to the
 * data when this is called.
 *
 * @param sock The UDP socket
 * @param data Data to send (ownership transferred to the library)
 * @param host Destination host
 * @param port Destination port
 * @return EV_OK on success, error code on failure
 */
ev_err ev_udp_send(ev_sock *sock, buf **data, const char *host, int port);

//=============================================================================
// TIMERS
//=============================================================================

/**
 * Configuration for timer creation
 */
typedef struct {
    uint64_t timeout_ms;       // Timeout in milliseconds
    bool repeat;               // Whether to repeat the timer (default true)
    ev_callback callback;      // Event callback function
    void *user_data;           // User data passed to callback
} ev_timer_opts;

/**
 * Start a timer
 *
 * An EV_EVENT_TICK event is delivered when the timer expires.
 * If repeat is true, the timer will continue firing.
 *
 * @param loop The event loop
 * @param timer Pointer to store the created timer
 * @param opts Timer configuration
 * @return EV_OK on success, error code on failure
 */
ev_err ev_timer_start(ev_loop *loop, ev_sock **timer, const ev_timer_opts *opts);

/**
 * Stop a timer
 *
 * Stops the timer and releases all internal resources used by it.
 *
 * @param timer The timer to stop
 * @return EV_OK on success, error code on failure
 */
ev_err ev_timer_stop(ev_sock *timer);

//=============================================================================
// SOCKET OPERATIONS
//=============================================================================

/**
 * Close a socket
 *
 * An EV_EVENT_CLOSE event will be delivered after the socket is closed.
 * No further events will be delivered for this socket after the close event.
 *
 * @param sock The socket to close
 * @return EV_OK on success, error code on failure
 */
ev_err ev_close(ev_sock *sock);

/**
 * Get the socket type
 *
 * @param sock The socket
 * @return The socket type
 */
ev_sock_type ev_sock_get_type(ev_sock *sock);

/**
 * Get the local address of a socket
 *
 * @param sock The socket
 * @param host Buffer to store host (must be at least 46 bytes for IPv6)
 * @param host_len Size of host buffer
 * @param port Pointer to store port number
 * @return EV_OK on success, error code on failure
 */
ev_err ev_sock_get_local_addr(ev_sock *sock, char *host, size_t host_len, int *port);

/**
 * Get the remote address of a connected socket
 *
 * @param sock The socket
 * @param host Buffer to store host (must be at least 46 bytes for IPv6)
 * @param host_len Size of host buffer
 * @param port Pointer to store port number
 * @return EV_OK on success, error code on failure
 */
ev_err ev_sock_get_remote_addr(ev_sock *sock, char *host, size_t host_len, int *port);

/**
 * Wake up a socket from another thread
 *
 * This causes the socket's callback to be invoked with an artificial event.
 * Useful for cross-thread communication.
 *
 * @param sock The socket to wake
 * @param user_data Data to pass in the wake event
 * @return EV_OK on success, error code on failure
 */
ev_err ev_sock_wake(ev_sock *sock, void *user_data);


//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * Convert error code to human-readable string
 *
 * @param err The error code
 * @return String description of the error
 */
const char* ev_err_to_string(ev_err err);

/**
 * Convert event type to human-readable string
 *
 * @param type The event type
 * @return String description of the event type
 */
const char* ev_event_string(ev_event_type type);


//=============================================================================
// NETWORK DISCOVERY
//=============================================================================

/**
 * Network interface information
 */
typedef struct {
    char *network_ip;          // Network interface IP address (allocated)
    char *netmask;             // Network mask (allocated)
    char *broadcast;           // Broadcast address (allocated)
} ev_network_info;

/**
 * Find all network interfaces and their broadcast addresses
 *
 * This function discovers all active network interfaces on the system
 * and returns their IP addresses, netmasks, and calculated broadcast addresses.
 *
 * @param network_info Pointer to store allocated array of network info structures
 * @param num_networks Pointer to store the number of networks found
 * @param loop The event loop (may be NULL, used for future platform-specific needs)
 * @return EV_OK on success, error code on failure
 */
ev_err ev_loop_find_networks(ev_network_info **network_info, size_t *num_networks, ev_loop *loop);

/**
 * Free network information array allocated by ev_loop_find_networks
 *
 * @param network_info The network information array to free
 * @param num_networks The number of network entries in the array
 */
void ev_loop_network_info_dispose(ev_network_info *network_info, size_t num_networks);

//=============================================================================
// ADVANCED FEATURES
//=============================================================================

/**
 * Set socket options
 */
typedef enum {
    EV_SOCKOPT_KEEP_ALIVE,     // TCP keep-alive (bool)
    EV_SOCKOPT_NO_DELAY,       // TCP_NODELAY (bool)
    EV_SOCKOPT_REUSE_ADDR,     // SO_REUSEADDR (bool)
    EV_SOCKOPT_RECV_BUFFER,    // SO_RCVBUF (int)
    EV_SOCKOPT_SEND_BUFFER,    // SO_SNDBUF (int)
} ev_sockopt;

/**
 * Set a socket option
 *
 * @param sock The socket
 * @param opt The option to set
 * @param value Pointer to the option value
 * @param value_len Size of the option value
 * @return EV_OK on success, error code on failure
 */
ev_err ev_sock_set_option(ev_sock *sock, ev_sockopt opt, const void *value, size_t value_len);

/**
 * Get a socket option
 *
 * @param sock The socket
 * @param opt The option to get
 * @param value Buffer to store the option value
 * @param value_len Pointer to size of value buffer (updated with actual size)
 * @return EV_OK on success, error code on failure
 */
ev_err ev_sock_get_option(ev_sock *sock, ev_sockopt opt, void *value, size_t *value_len);

//=============================================================================
// EVENT STRUCT ACCESSORS
//=============================================================================

/**
 * Get the event type from an event
 *
 * @param event The event
 * @return The event type
 */
static inline ev_event_type ev_event_get_type(const ev_event *event) {
    return event ? event->type : EV_EVENT_ERROR;
}

/**
 * Get the socket from an event
 *
 * @param event The event
 * @return The socket that generated the event
 */
static inline ev_sock *ev_event_get_socket(const ev_event *event) {
    return event ? event->sock : NULL;
}

/**
 * Get the socket state from an event
 *
 * @param event The event
 * @return The current socket state
 */
static inline ev_sock_state ev_event_get_sock_state(const ev_event *event) {
    return event ? event->sock_state : EV_SOCK_STATE_ERROR;
}

/**
 * Get the data buffer from an event
 *
 * @param event The event
 * @return Pointer to the data buffer pointer (for read events, NULL otherwise)
 */
static inline buf **ev_event_get_data(const ev_event *event) {
    return event ? event->data : NULL;
}

/**
 * Get the remote host from an event
 *
 * @param event The event
 * @return The remote host string (for accept/connect/UDP events)
 */
static inline const char *ev_event_get_remote_host(const ev_event *event) {
    return event ? event->remote_host : NULL;
}

/**
 * Get the remote port from an event
 *
 * @param event The event
 * @return The remote port (for accept/connect/UDP events)
 */
static inline int ev_event_get_remote_port(const ev_event *event) {
    return event ? event->remote_port : 0;
}

/**
 * Get the event timestamp from an event
 *
 * @param event The event
 * @return Event time in milliseconds since epoch
 */
static inline int64_t ev_event_get_time(const ev_event *event) {
    return event ? event->event_time_ms : 0;
}

/**
 * Get the error code from an event
 *
 * @param event The event
 * @return The error code (for error events)
 */
static inline ev_err ev_event_get_error(const ev_event *event) {
    return event ? event->error : EV_ERR_NULL_PTR;
}

/**
 * Get the user data from an event
 *
 * @param event The event
 * @return The user data pointer passed during socket creation
 */
static inline void *ev_event_get_user_data(const ev_event *event) {
    return event ? event->user_data : NULL;
}
