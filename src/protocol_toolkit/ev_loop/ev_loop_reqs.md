# Event Loop Library Requirements Specification

This specification describes the requirements for the Event Loop Library (`ev_loop.h`) - a simplified, background-threaded event loop API for network programming that replaces libuv with more streamlined usage.

## Core Design Principles

The Event Loop Library provides:
- Single callback type for all events
- Clear buffer ownership model
- Configuration-based object creation
- Event-driven design with minimal boilerplate
- Background threading with automatic lifecycle management
- Unified error handling

## Threading Model

### Background Thread Operation
- Event loops operate in background threads automatically
- No blocking `ev_loop_run()` function - threads start immediately on creation
- Main thread uses `ev_loop_wait()` to wait for completion
- All callbacks execute on background threads
- Thread-safe operations for cross-thread communication

### Thread Configuration
```c
typedef struct {
    size_t worker_threads;     // Number of background threads (default: CPU count if 0)
    size_t max_events;         // Max events per loop iteration (default: 1024 if 0)
    bool auto_start;           // Start background threads immediately (default: true)
} ev_loop_opts;
```

## Error Handling

### Error Code Definitions
```c
typedef enum {
    EV_OK = 0,                  // Success
    EV_ERR_OUT_OF_MEMORY,      // Memory allocation failed
    EV_ERR_INVALID_PARAM,      // Invalid parameter passed
    EV_ERR_NETWORK_ERROR,      // Network operation failed
    EV_ERR_CLOSED,             // Socket is closed
    EV_ERR_TIMEOUT,            // Operation timed out
    EV_ERR_WOULD_BLOCK,        // Operation would block
    EV_ERR_ADDRESS_IN_USE,     // Address already in use
    EV_ERR_CONNECTION_REFUSED, // Connection refused by remote
    EV_ERR_HOST_UNREACHABLE,   // Host unreachable
} ev_err;
```

### Error Reporting
- All functions return `ev_err` codes
- Error events delivered via callback with `EV_EVENT_ERROR` type
- Utility function `ev_err_string()` provides human-readable error descriptions
- Network errors automatically mapped to appropriate error codes

## Event System

### Event Types
```c
typedef enum {
    EV_EVENT_ACCEPT,           // New client connected (server sockets)
    EV_EVENT_CONNECT,          // Connection established (client sockets)
    EV_EVENT_READ,         // Data received
    EV_EVENT_WRITE_DONE,   // Write operation completed
    EV_EVENT_CLOSE,            // Connection closed
    EV_EVENT_ERROR,            // Error occurred
    EV_EVENT_TICK,             // Timer tick
} ev_event_type;
```

### Event Structure
```c
typedef struct {
    ev_event_type type;        // Type of event
    ev_sock *sock;             // Socket that generated the event
    buf *data;                 // Data buffer (for read events, NULL otherwise)
    ev_buf_ownership ownership;// Who owns the data buffer
    const char *remote_host;   // Remote host (for accept/connect/UDP events)
    int remote_port;           // Remote port (for accept/connect/UDP events)
    ev_err error;              // Error code (for error events)
    void *user_data;           // User data passed during socket creation
} ev_event;
```

### Callback Function
```c
typedef void (*ev_callback)(const ev_event *event);
```

## Buffer Management

### Buffer Ownership Model
```c
typedef enum {
    EV_BUF_BORROWED,           // Don't free - library retains ownership
    EV_BUF_TRANSFER,           // Take ownership - you must call buf_free()
} ev_buf_ownership;
```

### Buffer Integration
- All buffers use the unified `buf.h` API
- Event callbacks receive `buf *` with clear ownership semantics
- Read buffers may be borrowed (library-managed) or transferred (user-managed)
- Write buffers always transfer ownership to the library

## Socket Types and Operations

### Socket Types
```c
typedef enum {
    EV_SOCK_TCP_SERVER,        // TCP listening socket
    EV_SOCK_TCP_CLIENT,        // TCP client socket
    EV_SOCK_UDP,               // UDP socket
    EV_SOCK_TIMER,             // Timer object
} ev_sock_type;
```

### Socket Operations
- `ev_write()` - Asynchronous write with ownership transfer
- `ev_close()` - Close socket (generates EV_EVENT_CLOSE)
- `ev_sock_get_type()` - Get socket type
- `ev_sock_get_local_addr()` - Get local address
- `ev_sock_get_remote_addr()` - Get remote address

## Event Loop Management

### Loop Creation and Destruction
```c
ev_err ev_loop_create(ev_loop **loop, const ev_loop_opts *opts);
void ev_loop_destroy(ev_loop *loop);
```

### Loop Control
```c
ev_err ev_loop_wait(ev_loop *loop);
ev_err ev_loop_wait_timeout(ev_loop *loop, uint64_t timeout_ms);
void ev_loop_stop(ev_loop *loop);
bool ev_loop_is_running(ev_loop *loop);
```

### Loop Requirements
- `ev_loop_create()` starts background threads if `auto_start` is true
- `ev_loop_wait()` blocks until `ev_loop_stop()` is called
- `ev_loop_destroy()` automatically stops threads and cleans up resources
- Loop supports multiple worker threads for scalability

## TCP Server Operations

### Server Configuration
```c
typedef struct {
    const char *bind_host;     // Host to bind to ("0.0.0.0" for all interfaces)
    int port;                  // Port to listen on
    int backlog;               // Listen backlog (default: 128 if 0)
    ev_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    bool reuse_addr;           // Enable SO_REUSEADDR (default: true)
    bool keep_alive;           // Enable TCP keep-alive (default: false)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ev_tcp_server_opts;
```

### Server Operations
```c
ev_err ev_tcp_server_start(ev_loop *loop, ev_sock **server, const ev_tcp_server_opts *opts);
```

### Server Requirements
- Immediately starts listening on successful creation
- Generates `EV_EVENT_ACCEPT` for new connections
- Supports multiple simultaneous connections
- Configurable backlog and buffer sizes

## TCP Client Operations

### Client Configuration
```c
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
```

### Client Operations
```c
ev_err ev_tcp_connect(ev_loop *loop, ev_sock **client, const ev_tcp_client_opts *opts);
```

### Client Requirements
- Asynchronous connection establishment
- Generates `EV_EVENT_CONNECT` on success/failure
- Configurable connection timeout
- Support for keep-alive and custom buffer sizes

## UDP Operations

### UDP Configuration
```c
typedef struct {
    const char *bind_host;     // Host to bind to (NULL for client-only)
    int bind_port;             // Port to bind to (0 for client-only)
    ev_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    bool broadcast;            // Enable broadcast (default: false)
    bool reuse_addr;           // Enable SO_REUSEADDR (default: true)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ev_udp_opts;
```

### UDP Operations
```c
ev_err ev_udp_create(ev_loop *loop, ev_sock **udp_sock, const ev_udp_opts *opts);
ev_err ev_udp_send(ev_sock *sock, buf *data, const char *host, int port);
```

### UDP Requirements
- Support both client and server (bound) operation
- Generates `EV_EVENT_UDP_READ` for received packets
- Generates `EV_EVENT_UDP_WRITE_DONE` for completed sends
- Support for broadcast and unicast
- Destination address specified per send operation

## Timer Operations

### Timer Configuration
```c
typedef struct {
    uint64_t timeout_ms;       // Timeout in milliseconds
    bool repeat;               // Whether to repeat the timer
    ev_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks
} ev_timer_opts;
```

### Timer Operations
```c
ev_err ev_timer_start(ev_loop *loop, ev_sock **timer, const ev_timer_opts *opts);
ev_err ev_timer_stop(ev_sock *timer);
```

### Timer Requirements
- Generates `EV_EVENT_TICK` when timer expires
- Support for one-shot and repeating timers
- Millisecond precision timing
- Timer can be stopped before expiration

## Advanced Features

### Socket Options
```c
typedef enum {
    EV_SOCKOPT_KEEP_ALIVE,     // TCP keep-alive (bool)
    EV_SOCKOPT_NO_DELAY,       // TCP_NODELAY (bool)
    EV_SOCKOPT_REUSE_ADDR,     // SO_REUSEADDR (bool)
    EV_SOCKOPT_RECV_BUFFER,    // SO_RCVBUF (int)
    EV_SOCKOPT_SEND_BUFFER,    // SO_SNDBUF (int)
} ev_sockopt;
```

### Advanced Operations
```c
ev_err ev_sock_set_option(ev_sock *sock, ev_sockopt opt, const void *value, size_t value_len);
ev_err ev_sock_get_option(ev_sock *sock, ev_sockopt opt, void *value, size_t *value_len);
ev_err ev_sock_wake(ev_sock *sock, void *user_data);
ev_err ev_loop_post(ev_loop *loop, void (*callback)(void *user_data), void *user_data);
```

### Advanced Requirements
- Socket options can be set/get at runtime
- Cross-thread wake capability for inter-thread communication
- Deferred callback execution via `ev_loop_post()`

## Utility Functions

### Utility Operations
```c
const char* ev_err_string(ev_err err);
const char* ev_event_string(ev_event_type type);
uint64_t ev_now_ms(void);
```

### Utility Requirements
- Human-readable error and event type strings
- High-resolution timestamp function
- All utility functions are thread-safe

## Implementation Requirements

### Memory Management
- All objects properly allocated/deallocated
- No memory leaks on normal or error paths
- Automatic cleanup on event loop destruction
- Buffer ownership clearly managed

### Thread Safety
- All public functions thread-safe
- Callbacks may be invoked from multiple threads
- Internal data structures protected with appropriate synchronization
- No race conditions in event delivery

### Performance
- Minimal overhead for event processing
- Efficient I/O multiplexing (epoll/kqueue/IOCP)
- Scalable to thousands of concurrent connections
- Low latency for event delivery

### Error Handling
- All error conditions properly detected and reported
- Graceful degradation on resource exhaustion
- Network errors properly mapped to error codes
- No silent failures

### Integration
- Compatible with existing `buf.h` API
- No dependencies beyond standard libraries
- Clean separation from libuv or other event loop libraries
- Minimal external dependencies

## Example Usage Pattern

```c
void server_callback(const ev_event *event) {
    switch (event->type) {
        case EV_EVENT_ACCEPT:
            // Handle new client connection
            break;
        case EV_EVENT_READ:
            // Process received data using buf API
            // Handle buffer ownership correctly
            break;
        case EV_EVENT_WRITE_DONE:
            // Handle completion of write operation
            break;
        case EV_EVENT_ERROR:
            // Handle error conditions
            break;
    }
}

int main() {
    ev_loop *loop;
    ev_loop_create(&loop, NULL);  // Use defaults, auto-start threads

    ev_sock *server;
    ev_tcp_server_opts opts = {
        .bind_host = "0.0.0.0",
        .port = 8080,
        .callback = server_callback,
        .user_data = NULL
    };

    if (ev_tcp_server_start(loop, &server, &opts) == EV_OK) {
        ev_loop_wait(loop);  // Wait for background threads
    }

    ev_loop_destroy(loop);
    return 0;
}
```

This specification defines a complete event loop library that provides simplified, background-threaded network programming with clear ownership semantics and unified error handling.
