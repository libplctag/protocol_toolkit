# Protocol Toolkit - Linux Implementation

## Overview

The Linux implementation provides the exact same API as the macOS version but uses Linux-specific system calls for optimal performance and compatibility.

## Architecture

### Core Components

| Component | Linux Implementation | Purpose |
|-----------|---------------------|---------|
| **Event Loop** | `epoll()` | Efficient I/O event multiplexing |
| **Timers** | `timerfd_create()` | High-resolution timer management |
| **User Events** | `eventfd()` | Thread-safe custom event sources |
| **Sockets** | BSD sockets + epoll | Non-blocking network I/O |
| **Thread Safety** | `pthread_mutex` | Resource protection |

### Key Differences from macOS

| Feature | macOS (GCD) | Linux (epoll) |
|---------|-------------|---------------|
| Event Multiplexing | `dispatch_source_t` | `epoll_wait()` |
| Timer Implementation | GCD timers | `timerfd_create()` |
| User Events | `dispatch_queue_t` | `eventfd()` |
| Thread Model | GCD queues | pthread mutexes |
| File Descriptors | Managed by GCD | Direct epoll management |

## Implementation Details

### Event Loop Structure

```c
typedef struct ptk_event_loop_slot {
    ptk_handle_t handle;
    ptk_event_loop_resources_t *resources;
    ptk_err_t last_error;

    /* Linux-specific fields */
    int epoll_fd;                /**< epoll file descriptor */
    struct epoll_event *events;  /**< Event buffer for epoll_wait */
    size_t max_events;           /**< Maximum events per epoll_wait call */
    bool is_running;
    pthread_mutex_t loop_mutex;  /**< Mutex for thread-safe operations */
    uint32_t generation_counter;
} ptk_event_loop_slot_t;
```

### Timer Implementation

```c
typedef struct ptk_timer_internal {
    ptk_resource_base_t base;
    int timer_fd;                          /**< Linux timerfd file descriptor */
    uint64_t interval_ms;
    bool is_repeating;
    bool is_running;
    uint32_t generation_counter;
    ptk_event_handler_t event_handlers[4];
} ptk_timer_internal_t;
```

### Socket Implementation

```c
typedef struct ptk_socket_internal {
    ptk_resource_base_t base;
    int sockfd;                            /**< BSD socket file descriptor */
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;
    socklen_t local_addr_len;
    socklen_t remote_addr_len;
    bool is_connected;
    bool is_listening;
    int socket_type;
    uint32_t epoll_events;                 /**< Current epoll event mask */
    uint32_t generation_counter;
    ptk_event_handler_t event_handlers[8];
} ptk_socket_internal_t;
```

### User Event Source Implementation

```c
typedef struct ptk_user_event_source_internal {
    ptk_resource_base_t base;
    int event_fd;                           /**< Linux eventfd file descriptor */
    pthread_mutex_t event_mutex;            /**< Mutex for thread-safe event raising */
    uint32_t generation_counter;
    ptk_event_handler_t event_handlers[16];
} ptk_user_event_source_internal_t;
```

## Performance Characteristics

### epoll Advantages

- **O(1) Performance**: Event notification scales with active connections, not total
- **Edge-Triggered Mode**: Minimal system calls for high-throughput scenarios
- **Low Memory Overhead**: Kernel maintains connection state efficiently
- **Wide Compatibility**: Available on all modern Linux kernels (2.6+)

### timerfd Benefits

- **High Resolution**: Nanosecond precision timers
- **Integrated with epoll**: Timers appear as regular file descriptor events
- **No Signal Overhead**: Avoids traditional signal-based timer limitations
- **Thread Safe**: Multiple threads can wait on same timer safely

### eventfd Efficiency

- **Minimal Overhead**: Simple counter-based event mechanism
- **Thread Safe**: Atomic operations for cross-thread signaling
- **epoll Compatible**: Can be monitored alongside other file descriptors
- **No Pipe Overhead**: More efficient than traditional pipe-based IPC

## Error Handling

The Linux implementation maps standard errno values to PTK error codes:

```c
static ptk_err_t ptk_errno_to_error(int err) {
    switch (err) {
        case EAGAIN:
        case EWOULDBLOCK:    return PTK_ERR_WOULD_BLOCK;
        case ECONNREFUSED:   return PTK_ERR_CONNECTION_REFUSED;
        case ECONNRESET:     return PTK_ERR_CONNECTION_RESET;
        case ENOTCONN:       return PTK_ERR_NOT_CONNECTED;
        case EISCONN:        return PTK_ERR_ALREADY_CONNECTED;
        case EADDRINUSE:     return PTK_ERR_ADDRESS_IN_USE;
        // ... more mappings
    }
}
```

## Building for Linux

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# Red Hat/CentOS/Fedora
sudo yum install gcc make
```

### Compilation

```bash
# Compile with Linux implementation
gcc -o my_app my_app.c src/lib/linux/protocol_toolkit_linux.c \
    -I src/lib/linux -lpthread

# Enable optimizations
gcc -O2 -DNDEBUG -o my_app my_app.c src/lib/linux/protocol_toolkit_linux.c \
    -I src/lib/linux -lpthread
```

### CMake Integration

```cmake
if(UNIX AND NOT APPLE)
    # Linux-specific build
    target_sources(protocol_toolkit PRIVATE
        src/lib/linux/protocol_toolkit_linux.c)
    target_include_directories(protocol_toolkit PUBLIC
        src/lib/linux)
    target_link_libraries(protocol_toolkit pthread)
endif()
```

## Cross-Platform Compatibility

The same application code works on both platforms:

```c
// This code is identical on macOS and Linux!
typedef struct {
    ptk_pt_t pt;        // Must be first field
    ptk_handle_t socket;
    ptk_buffer_t buffer;
    uint8_t data[1024];
} app_context_t;

void my_protothread(ptk_pt_t *pt) {
    app_context_t *app = (app_context_t *)pt;

    PT_BEGIN(pt);
    PTK_PT_TCP_CONNECT(pt, app->socket, "server.com", 80);
    PTK_PT_TCP_SEND(pt, app->socket, &app->buffer);
    PTK_PT_TCP_RECEIVE(pt, app->socket, &app->buffer);
    PT_END(pt);
}
```

## Debugging and Monitoring

### System Tools

```bash
# Monitor epoll usage
strace -e epoll_wait,epoll_ctl ./my_app

# Check file descriptor usage
ls -la /proc/$(pidof my_app)/fd/

# Monitor network connections
ss -tulpn | grep my_app

# Check timer usage
cat /proc/$(pidof my_app)/timerslack_ns
```

### Performance Profiling

```bash
# Profile system calls
perf trace -p $(pidof my_app)

# Monitor epoll events
perf record -e syscalls:sys_enter_epoll_wait ./my_app
perf report
```

## Best Practices

1. **Resource Limits**: Set appropriate `RLIMIT_NOFILE` for high connection counts
2. **Event Buffer Sizing**: Size epoll event buffer based on expected concurrent events
3. **Thread Safety**: Use the provided mutexes when accessing resources from multiple threads
4. **Error Handling**: Always check return values and handle `PTK_ERR_WOULD_BLOCK` appropriately
5. **Memory Management**: Pre-allocate all buffers to avoid runtime allocation

## Future Enhancements

- **io_uring Support**: Optional io_uring backend for Linux 5.1+ (when stability improves)
- **NUMA Awareness**: Event loop affinity for multi-socket systems
- **Container Integration**: Enhanced support for containerized environments
- **BPF Integration**: Optional eBPF filters for advanced packet processing
