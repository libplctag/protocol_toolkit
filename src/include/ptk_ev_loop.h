#pragma once

/**
 * @file ptk_loop.h
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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <ptk_buf.h>
#include <ptk_err.h>


// Forward declarations
typedef struct ptk_loop ptk_loop;
typedef struct ptk_sock ptk_sock;


/**
 * Event types delivered to callbacks
 */
typedef enum {
    PTK_EVENT_ACCEPT,           // New client connected (server sockets)
    PTK_EVENT_CONNECT,          // Connection established (client sockets)
    PTK_EVENT_READ,             // Data received, for both TCP client and UDP sockets
    PTK_EVENT_WRITE_DONE,       // Write operation completed, for both TCP client and UDP socket.
    PTK_EVENT_CLOSE,            // Connection closed, for all socket types.
    PTK_EVENT_ERROR,            // Error occurred, for all socket types.
    PTK_EVENT_TICK,             // Timer tick, for all socket types.
} ptk_event_type;

/**
 * Socket connection states maintained by the library
 */
typedef enum {
    PTK_SOCK_STATE_CREATED,     // Socket created but not connected/listening
    PTK_SOCK_STATE_CONNECTING,  // TCP client connecting
    PTK_SOCK_STATE_LISTENING,   // TCP server listening
    PTK_SOCK_STATE_CONNECTED,   // TCP connection established
    PTK_SOCK_STATE_UDP_BOUND,   // UDP socket bound and ready
    PTK_SOCK_STATE_CLOSING,     // Socket closing
    PTK_SOCK_STATE_CLOSED,      // Socket closed
    PTK_SOCK_STATE_ERROR,       // Socket in error state
} ptk_sock_state;


/**
 * Socket types
 */
typedef enum {
    PTK_SOCK_TCP_SERVER,        // TCP listening socket
    PTK_SOCK_TCP_CLIENT,        // TCP client socket
    PTK_SOCK_UDP,               // UDP socket
    PTK_SOCK_TIMER,             // Timer object
} ptk_sock_type;

/**
 * Event structure passed to callbacks
 *
 * The library owns the resources of the event struct
 * itself.  The event callback must not free the event.
 */
typedef struct {
    ptk_event_type type;        // Type of event
    ptk_sock *sock;             // Socket that generated the event
    buf **data;                // Data buffer, callee ownership (for read events, NULL otherwise)
    const char *remote_host;   // Remote host (for accept/connect/UDP events)
    int64_t event_time_ms;     // Event time in milliseconds since epoch
    int remote_port;           // Remote port (for accept/connect/UDP events)
    ptk_err error;              // Error code (for error events)
    ptk_sock_state sock_state;  // Current socket state
    void *user_data;           // User data passed during socket creation
} ptk_event;

/**
 * Event callback function type
 *
 * @param event The event that occurred
 */
typedef void (*ptk_callback)(const ptk_event *event);

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
} ptk_loop_opts;

/**
 * Create a new event loop with background threads
 *
 * The event loop automatically starts one or more background threads to handle
 * I/O operations. Callbacks are invoked on these background threads.
 *
 * @param loop Pointer to store the created loop
 * @param opts Loop configuration (NULL for defaults)
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_loop_create(ptk_loop **loop, const ptk_loop_opts *opts);

/**
 * Wait for the event loop to finish
 *
 * This function blocks until ptk_loop_stop() is called from a callback
 * or another thread. Use this instead of ptk_loop_run() for background operation.
 *
 * @param loop The event loop to wait for
 * @return PTK_OK when loop exits normally, error code on failure
 */
ptk_err ptk_loop_wait(ptk_loop *loop);

/**
 * Wait for the event loop to finish with timeout
 *
 * @param loop The event loop to wait for
 * @param timeout_ms Maximum time to wait in milliseconds (0 for no timeout)
 * @return PTK_OK when loop exits, PTK_ERR_TIMEOUT on timeout, error code on failure
 */
ptk_err ptk_loop_wait_timeout(ptk_loop *loop, uint64_t timeout_ms);

/**
 * Stop a running event loop
 *
 * This can be called from within a callback or from another thread.
 * The loop will stop after processing current events and ptk_loop_wait() will return.
 *
 * @param loop The event loop to stop
 */
void ptk_loop_stop(ptk_loop *loop);

/**
 * Check if an event loop is running
 *
 * @param loop The event loop to check
 * @return true if running, false if stopped
 */
bool ptk_loop_is_running(ptk_loop *loop);

/**
 * Destroy an event loop and free its resources
 *
 * This automatically stops the loop and waits for background threads to exit.
 * All sockets associated with the loop are automatically closed.
 *
 * @param loop The event loop to destroy
 */
void ptk_loop_destroy(ptk_loop *loop);

/**
 * Post a callback to run on the next event loop iteration
 *
 * @param loop The event loop
 * @param callback The callback to run
 * @param user_data Data to pass to the callback
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_loop_post(ptk_loop *loop, void (*callback)(void *user_data), void *user_data);


//=============================================================================
// TCP CLIENT
//=============================================================================

/**
 * Configuration for TCP client creation
 */
typedef struct {
    const char *host;          // Remote host to connect to
    int port;                  // Remote port to connect to
    ptk_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    uint32_t connect_timeout_ms; // Connection timeout (default: 30000 if 0)
    bool keep_alive;           // Enable TCP keep-alive (default: false)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ptk_tcp_client_opts;

/**
 * Connect to a TCP server
 *
 * Connection is performed asynchronously. An PTK_EVENT_CONNECT event
 * is delivered when the connection is established or fails.
 *
 * @param loop The event loop
 * @param client Pointer to a pointer at which to store the created client socket
 * @param opts Client configuration
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_tcp_connect(ptk_loop *loop, ptk_sock **client, const ptk_tcp_client_opts *opts);

/**
 * Write data to a TCP socket
 *
 * The operation is performed asynchronously. An PTK_EVENT_WRITE_DONE
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
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_tcp_write(ptk_sock *sock, buf **data);

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
    ptk_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    bool reuse_addr;           // Enable SO_REUSEADDR (default: true)
    bool keep_alive;           // Enable TCP keep-alive (default: false)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ptk_tcp_server_opts;

/**
 * Start a TCP server
 *
 * The server will immediately start listening and accepting connections.
 * For each new connection, an PTK_EVENT_ACCEPT event is delivered.
 *
 * @param loop The event loop
 * @param server Pointer to store the created server socket
 * @param server_opts Server configuration
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_tcp_server_start(ptk_loop *loop, ptk_sock **server, const ptk_tcp_server_opts *server_opts);

//=============================================================================
// UDP SOCKETS
//=============================================================================

/**
 * Configuration for UDP socket creation
 */
typedef struct {
    const char *bind_host;     // Host to bind to (NULL for client-only)
    int bind_port;             // Port to bind to (0 for client-only)
    ptk_callback callback;      // Event callback function
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
} ptk_udp_opts;

/**
 * Create a UDP socket
 *
 * Can be used for both client and server operations.
 * If bind_host/bind_port are specified, the socket is bound for receiving.
 *
 * @param loop The event loop
 * @param udp_sock Pointer to store the created UDP socket
 * @param opts UDP socket configuration
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_udp_create(ptk_loop *loop, ptk_sock **udp_sock, const ptk_udp_opts *opts);

/**
 * Send UDP data to a specific address
 *
 * The operation is performed asynchronously. An PTK_EVENT_UDP_WRITE_DONE
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
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_udp_send(ptk_sock *sock, buf **data, const char *host, int port);

//=============================================================================
// TIMERS
//=============================================================================

/**
 * Configuration for timer creation
 */
typedef struct {
    uint64_t timeout_ms;       // Timeout in milliseconds
    bool repeat;               // Whether to repeat the timer (default true)
    ptk_callback callback;      // Event callback function
    void *user_data;           // User data passed to callback
} ptk_timer_opts;

/**
 * Start a timer
 *
 * An PTK_EVENT_TICK event is delivered when the timer expires.
 * If repeat is true, the timer will continue firing.
 *
 * @param loop The event loop
 * @param timer Pointer to store the created timer
 * @param opts Timer configuration
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_timer_start(ptk_loop *loop, ptk_sock **timer, const ptk_timer_opts *opts);

/**
 * Stop a timer
 *
 * Stops the timer and releases all internal resources used by it.
 *
 * @param timer The timer to stop
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_timer_stop(ptk_sock *timer);

//=============================================================================
// SOCKET OPERATIONS
//=============================================================================

/**
 * Close a socket
 *
 * An PTK_EVENT_CLOSE event will be delivered after the socket is closed.
 * No further events will be delivered for this socket after the close event.
 *
 * @param sock The socket to close
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_close(ptk_sock *sock);

/**
 * Get the socket type
 *
 * @param sock The socket
 * @return The socket type
 */
ptk_sock_type ptk_sock_get_type(ptk_sock *sock);

/**
 * Get the local address of a socket
 *
 * @param sock The socket
 * @param host Buffer to store host (must be at least 46 bytes for IPv6)
 * @param host_len Size of host buffer
 * @param port Pointer to store port number
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_sock_get_local_addr(ptk_sock *sock, char *host, size_t host_len, int *port);

/**
 * Get the remote address of a connected socket
 *
 * @param sock The socket
 * @param host Buffer to store host (must be at least 46 bytes for IPv6)
 * @param host_len Size of host buffer
 * @param port Pointer to store port number
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_sock_get_remote_addr(ptk_sock *sock, char *host, size_t host_len, int *port);

/**
 * Wake up a socket from another thread
 *
 * This causes the socket's callback to be invoked with an artificial event.
 * Useful for cross-thread communication.
 *
 * @param sock The socket to wake
 * @param user_data Data to pass in the wake event
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_sock_wake(ptk_sock *sock, void *user_data);


//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * Convert error code to human-readable string
 *
 * @param err The error code
 * @return String description of the error
 */
const char* ptk_err_to_string(ptk_err err);

/**
 * Convert event type to human-readable string
 *
 * @param type The event type
 * @return String description of the event type
 */
const char* ptk_event_string(ptk_event_type type);


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
} ptk_network_info;

/**
 * Find all network interfaces and their broadcast addresses
 *
 * This function discovers all active network interfaces on the system
 * and returns their IP addresses, netmasks, and calculated broadcast addresses.
 *
 * @param network_info Pointer to store allocated array of network info structures
 * @param num_networks Pointer to store the number of networks found
 * @param loop The event loop (may be NULL, used for future platform-specific needs)
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_loop_find_networks(ptk_network_info **network_info, size_t *num_networks, ptk_loop *loop);

/**
 * Free network information array allocated by ptk_loop_find_networks
 *
 * @param network_info The network information array to free
 * @param num_networks The number of network entries in the array
 */
void ptk_loop_network_info_dispose(ptk_network_info *network_info, size_t num_networks);

//=============================================================================
// ADVANCED FEATURES
//=============================================================================

/**
 * Set socket options
 */
typedef enum {
    PTK_SOCKOPT_KEEP_ALIVE,     // TCP keep-alive (bool)
    PTK_SOCKOPT_NO_DELAY,       // TCP_NODELAY (bool)
    PTK_SOCKOPT_REUSE_ADDR,     // SO_REUSEADDR (bool)
    PTK_SOCKOPT_RECV_BUFFER,    // SO_RCVBUF (int)
    PTK_SOCKOPT_SEND_BUFFER,    // SO_SNDBUF (int)
} ptk_sockopt;

/**
 * Set a socket option
 *
 * @param sock The socket
 * @param opt The option to set
 * @param value Pointer to the option value
 * @param value_len Size of the option value
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_sock_set_option(ptk_sock *sock, ptk_sockopt opt, const void *value, size_t value_len);

/**
 * Get a socket option
 *
 * @param sock The socket
 * @param opt The option to get
 * @param value Buffer to store the option value
 * @param value_len Pointer to size of value buffer (updated with actual size)
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_sock_get_option(ptk_sock *sock, ptk_sockopt opt, void *value, size_t *value_len);

//=============================================================================
// EVENT STRUCT ACCESSORS
//=============================================================================

/**
 * Get the event type from an event
 *
 * @param event The event
 * @return The event type
 */
static inline ptk_event_type ptk_event_get_type(const ptk_event *event) {
    return event ? event->type : PTK_EVENT_ERROR;
}

/**
 * Get the socket from an event
 *
 * @param event The event
 * @return The socket that generated the event
 */
static inline ptk_sock *ptk_event_get_socket(const ptk_event *event) {
    return event ? event->sock : NULL;
}

/**
 * Get the socket state from an event
 *
 * @param event The event
 * @return The current socket state
 */
static inline ptk_sock_state ptk_event_get_sock_state(const ptk_event *event) {
    return event ? event->sock_state : PTK_SOCK_STATE_ERROR;
}

/**
 * Get the data buffer from an event
 *
 * @param event The event
 * @return Pointer to the data buffer pointer (for read events, NULL otherwise)
 */
static inline buf **ptk_event_get_data(const ptk_event *event) {
    return event ? event->data : NULL;
}

/**
 * Get the remote host from an event
 *
 * @param event The event
 * @return The remote host string (for accept/connect/UDP events)
 */
static inline const char *ptk_event_get_remote_host(const ptk_event *event) {
    return event ? event->remote_host : NULL;
}

/**
 * Get the remote port from an event
 *
 * @param event The event
 * @return The remote port (for accept/connect/UDP events)
 */
static inline int ptk_event_get_remote_port(const ptk_event *event) {
    return event ? event->remote_port : 0;
}

/**
 * Get the event timestamp from an event
 *
 * @param event The event
 * @return Event time in milliseconds since epoch
 */
static inline int64_t ptk_event_get_time(const ptk_event *event) {
    return event ? event->event_time_ms : 0;
}

/**
 * Get the error code from an event
 *
 * @param event The event
 * @return The error code (for error events)
 */
static inline ptk_err ptk_event_get_error(const ptk_event *event) {
    return event ? event->error : PTK_ERR_NULL_PTR;
}

/**
 * Get the user data from an event
 *
 * @param event The event
 * @return The user data pointer passed during socket creation
 */
static inline void *ptk_event_get_user_data(const ptk_event *event) {
    return event ? event->user_data : NULL;
}
