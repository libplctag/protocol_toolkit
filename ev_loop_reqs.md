# Event Loop Library Requirements Specification

This specification describes the requirements for the Event Loop Library (`ptk_loop.h`) - a simplified, background-threaded event loop API for network programming that replaces libuv with more streamlined usage.

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
- No blocking `ptk_loop_run()` function - threads start immediately on creation
- Main thread uses `ptk_loop_wait()` to wait for completion
- All callbacks execute on background threads
- Thread-safe operations for cross-thread communication

### Thread Configuration
```c
typedef struct {
    size_t worker_threads;     // Number of background threads (default: CPU count if 0)
    size_t max_events;         // Max events per loop iteration (default: 1024 if 0)
    bool auto_start;           // Start background threads immediately (default: true)
} ptk_loop_opts;
```

## Error Handling

### Error Code Definitions
```c
typedef enum {
    PTK_OK = 0,                  // Success
    PTK_ERR_OUT_OF_MEMORY,      // Memory allocation failed
    PTK_ERR_INVALID_PARAM,      // Invalid parameter passed
    PTK_ERR_NETWORK_ERROR,      // Network operation failed
    PTK_ERR_CLOSED,             // Socket is closed
    PTK_ERR_TIMEOUT,            // Operation timed out
    PTK_ERR_WOULD_BLOCK,        // Operation would block
    PTK_ERR_ADDRESS_IN_USE,     // Address already in use
    PTK_ERR_CONNECTION_REFUSED, // Connection refused by remote
    PTK_ERR_HOST_UNREACHABLE,   // Host unreachable
} ptk_err;
```

### Error Reporting
- All functions return `ptk_err` codes
- Error events delivered via callback with `PTK_EVENT_ERROR` type
- Utility function `ptk_err_string()` provides human-readable error descriptions
- Network errors automatically mapped to appropriate error codes

## Event System

### Event Types
```c
typedef enum {
    PTK_EVENT_ACCEPT,           // New client connected (server sockets)
    PTK_EVENT_CONNECT,          // Connection established (client sockets)
    PTK_EVENT_READ,         // Data received
    PTK_EVENT_WRITE_DONE,   // Write operation completed
    PTK_EVENT_CLOSE,            // Connection closed
    PTK_EVENT_ERROR,            // Error occurred
    PTK_EVENT_TICK,             // Timer tick
} ptk_event_type;
```

### Event Structure
```c
typedef struct {
    ptk_event_type type;        // Type of event
    ptk_sock *sock;             // Socket that generated the event
    buf *data;                 // Data buffer (for read events, NULL otherwise)
    ptk_buf_ownership ownership;// Who owns the data buffer
    const char *remote_host;   // Remote host (for accept/connect/UDP events)
    int remote_port;           // Remote port (for accept/connect/UDP events)
    ptk_err error;              // Error code (for error events)
    void *user_data;           // User data passed during socket creation
} ptk_event;
```

### Callback Function
```c
typedef void (*ptk_callback)(const ptk_event *event);
```

## Buffer Management

### Buffer Ownership Model
```c
typedef enum {
    PTK_BUF_BORROWED,           // Don't free - library retains ownership
    PTK_BUF_TRANSFER,           // Take ownership - you must call buf_free()
} ptk_buf_ownership;
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
    PTK_SOCK_TCP_SERVER,        // TCP listening socket
    PTK_SOCK_TCP_CLIENT,        // TCP client socket
    PTK_SOCK_UDP,               // UDP socket
    PTK_SOCK_TIMER,             // Timer object
} ptk_sock_type;
```

### Socket Operations
- `ptk_write()` - Asynchronous write with ownership transfer
- `ptk_close()` - Close socket (generates PTK_EVENT_CLOSE)
- `ptk_sock_get_type()` - Get socket type
- `ptk_sock_get_local_addr()` - Get local address
- `ptk_sock_get_remote_addr()` - Get remote address

## Event Loop Management

### Loop Creation and Destruction
```c
ptk_err ptk_loop_create(ptk_loop **loop, const ptk_loop_opts *opts);
void ptk_loop_destroy(ptk_loop *loop);
```

### Loop Control
```c
ptk_err ptk_loop_wait(ptk_loop *loop);
ptk_err ptk_loop_wait_timeout(ptk_loop *loop, uint64_t timeout_ms);
void ptk_loop_stop(ptk_loop *loop);
bool ptk_loop_is_running(ptk_loop *loop);
```

### Loop Requirements
- `ptk_loop_create()` starts background threads if `auto_start` is true
- `ptk_loop_wait()` blocks until `ptk_loop_stop()` is called
- `ptk_loop_destroy()` automatically stops threads and cleans up resources
- Loop supports multiple worker threads for scalability

## TCP Server Operations

### Server Configuration
```c
typedef struct {
    const char *bind_host;     // Host to bind to ("0.0.0.0" for all interfaces)
    int port;                  // Port to listen on
    int backlog;               // Listen backlog (default: 128 if 0)
    ptk_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    bool reuse_addr;           // Enable SO_REUSEADDR (default: true)
    bool keep_alive;           // Enable TCP keep-alive (default: false)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ptk_tcp_server_opts;
```

### Server Operations
```c
ptk_err ptk_tcp_server_start(ptk_loop *loop, ptk_sock **server, const ptk_tcp_server_opts *opts);
```

### Server Requirements
- Immediately starts listening on successful creation
- Generates `PTK_EVENT_ACCEPT` for new connections
- Supports multiple simultaneous connections
- Configurable backlog and buffer sizes

## TCP Client Operations

### Client Configuration
```c
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
```

### Client Operations
```c
ptk_err ptk_tcp_connect(ptk_loop *loop, ptk_sock **client, const ptk_tcp_client_opts *opts);
```

### Client Requirements
- Asynchronous connection establishment
- Generates `PTK_EVENT_CONNECT` on success/failure
- Configurable connection timeout
- Support for keep-alive and custom buffer sizes

## UDP Operations

### UDP Configuration
```c
typedef struct {
    const char *bind_host;     // Host to bind to (NULL for client-only)
    int bind_port;             // Port to bind to (0 for client-only)
    ptk_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks

    // Optional settings
    bool broadcast;            // Enable broadcast (default: false)
    bool reuse_addr;           // Enable SO_REUSEADDR (default: true)
    size_t read_buffer_size;   // Read buffer size (default: 8192 if 0)
} ptk_udp_opts;
```

### UDP Operations
```c
ptk_err ptk_udp_create(ptk_loop *loop, ptk_sock **udp_sock, const ptk_udp_opts *opts);
ptk_err ptk_udp_send(ptk_sock *sock, buf *data, const char *host, int port);
```

### UDP Requirements
- Support both client and server (bound) operation
- Generates `PTK_EVENT_UDP_READ` for received packets
- Generates `PTK_EVENT_UDP_WRITE_DONE` for completed sends
- Support for broadcast and unicast
- Destination address specified per send operation

## Timer Operations

### Timer Configuration
```c
typedef struct {
    uint64_t timeout_ms;       // Timeout in milliseconds
    bool repeat;               // Whether to repeat the timer
    ptk_callback callback;      // Event callback function
    void *user_data;           // User data passed to callbacks
} ptk_timer_opts;
```

### Timer Operations
```c
ptk_err ptk_timer_start(ptk_loop *loop, ptk_sock **timer, const ptk_timer_opts *opts);
ptk_err ptk_timer_stop(ptk_sock *timer);
```

### Timer Requirements
- Generates `PTK_EVENT_TICK` when timer expires
- Support for one-shot and repeating timers
- Millisecond precision timing
- Timer can be stopped before expiration

## Advanced Features

### Socket Options
```c
typedef enum {
    PTK_SOCKOPT_KEEP_ALIVE,     // TCP keep-alive (bool)
    PTK_SOCKOPT_NO_DELAY,       // TCP_NODELAY (bool)
    PTK_SOCKOPT_REUSE_ADDR,     // SO_REUSEADDR (bool)
    PTK_SOCKOPT_RECV_BUFFER,    // SO_RCVBUF (int)
    PTK_SOCKOPT_SEND_BUFFER,    // SO_SNDBUF (int)
} ptk_sockopt;
```

### Advanced Operations
```c
ptk_err ptk_sock_set_option(ptk_sock *sock, ptk_sockopt opt, const void *value, size_t value_len);
ptk_err ptk_sock_get_option(ptk_sock *sock, ptk_sockopt opt, void *value, size_t *value_len);
ptk_err ptk_sock_wake(ptk_sock *sock, void *user_data);
ptk_err ptk_loop_post(ptk_loop *loop, void (*callback)(void *user_data), void *user_data);
```

### Advanced Requirements
- Socket options can be set/get at runtime
- Cross-thread wake capability for inter-thread communication
- Deferred callback execution via `ptk_loop_post()`

## Utility Functions

### Utility Operations
```c
const char* ptk_err_string(ptk_err err);
const char* ptk_event_string(ptk_event_type type);
uint64_t ptk_now_ms(void);
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
void server_callback(const ptk_event *event) {
    switch (event->type) {
        case PTK_EVENT_ACCEPT:
            // Handle new client connection
            break;
        case PTK_EVENT_READ:
            // Process received data using buf API
            // Handle buffer ownership correctly
            break;
        case PTK_EVENT_WRITE_DONE:
            // Handle completion of write operation
            break;
        case PTK_EVENT_ERROR:
            // Handle error conditions
            break;
    }
}

int main() {
    ptk_loop *loop;
    ptk_loop_create(&loop, NULL);  // Use defaults, auto-start threads

    ptk_sock *server;
    ptk_tcp_server_opts opts = {
        .bind_host = "0.0.0.0",
        .port = 8080,
        .callback = server_callback,
        .user_data = NULL
    };

    if (ptk_tcp_server_start(loop, &server, &opts) == PTK_OK) {
        ptk_loop_wait(loop);  // Wait for background threads
    }

    ptk_loop_destroy(loop);
    return 0;
}
```

This specification defines a complete event loop library that provides simplified, background-threaded network programming with clear ownership semantics and unified error handling.
