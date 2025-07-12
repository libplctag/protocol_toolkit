# PTK Event Loop and Threadlet Integration Architecture

This document describes how the PTK event loop integrates with the threadlet (green thread) system to provide cooperative multitasking with blocking I/O semantics.

---

## Overview

The PTK event loop provides the foundation for threadlet scheduling by:
- Running one event loop instance per OS thread
- Multiplexing I/O events using platform-specific APIs
- Coordinating threadlet suspension and resumption based on I/O readiness
- Maintaining the illusion of blocking socket operations without blocking OS threads

---

## Architecture Components

### Per-Thread Event Loop Design

Each OS thread creates and owns its own event loop instance:

```
OS Thread 1          OS Thread 2          OS Thread N
+-----------+        +-----------+        +-----------+
| Event     |        | Event     |        | Event     |
| Loop 1    |        | Loop 2    |        | Loop N    |
|           |        |           |        |           |
| Threadlet |        | Threadlet |        | Threadlet |
| Scheduler |        | Scheduler |        | Scheduler |
+-----------+        +-----------+        +-----------+
```

### Key Data Structures

```c
typedef struct {
    int fd;                           // Socket file descriptor
    threadlet_t *waiting_threadlet;   // Threadlet blocked on this fd
    uint32_t events;                  // Events being waited for (READ/WRITE)
    ptk_duration_ms timeout;          // Timeout for this operation
    uint64_t deadline;                // Absolute deadline for timeout
} event_registration_t;

typedef struct {
    platform_event_loop_t *platform; // Platform-specific event loop
    event_registration_t *registrations; // Hash table: fd -> registration
    threadlet_queue_t ready_queue;    // Threadlets ready to run
    threadlet_queue_t waiting_queue;  // Threadlets waiting on I/O
    bool running;                     // Event loop running state
    uint64_t current_time_ms;         // Cached current time
} ptk_event_loop_t;
```

---

## Event Loop Main Cycle

The event loop runs continuously, alternating between I/O polling and threadlet execution:

### Phase 1: I/O Event Processing

```c
// Poll for ready I/O events (non-blocking initially)
int timeout_ms = calculate_next_timeout();
ready_events = platform_poll_events(event_loop->platform, timeout_ms);

// Process each ready event
for (int i = 0; i < ready_events.count; i++) {
    event_t *event = &ready_events.events[i];
    event_registration_t *reg = lookup_registration(event->fd);
    
    if (reg && reg->waiting_threadlet) {
        // Remove from waiting queue and add to ready queue
        remove_from_waiting_queue(reg->waiting_threadlet);
        enqueue_ready_threadlet(reg->waiting_threadlet);
        
        // Clear the registration
        unregister_fd_events(event->fd);
    }
}
```

### Phase 2: Timeout Processing

```c
// Check for timed-out operations
current_time = get_current_time_ms();
event_loop->current_time_ms = current_time;

for (each registration in waiting_queue) {
    if (current_time >= registration->deadline) {
        // Timeout occurred
        threadlet_t *threadlet = registration->waiting_threadlet;
        
        // Set timeout error for the threadlet
        threadlet_set_error(threadlet, PTK_ERR_TIMEOUT);
        
        // Move to ready queue
        remove_from_waiting_queue(threadlet);
        enqueue_ready_threadlet(threadlet);
        
        // Clean up registration
        unregister_fd_events(registration->fd);
    }
}
```

### Phase 3: Threadlet Execution

```c
// Execute ready threadlets cooperatively
while (has_ready_threadlets()) {
    threadlet_t *threadlet = dequeue_ready_threadlet();
    
    // Resume threadlet execution
    threadlet_execution_result_t result = threadlet_resume_execution(threadlet);
    
    switch (result.status) {
        case THREADLET_YIELDED:
            // Threadlet yielded voluntarily, keep in ready queue
            enqueue_ready_threadlet(threadlet);
            break;
            
        case THREADLET_BLOCKED_ON_IO:
            // Threadlet blocked on I/O, register with event loop
            register_io_wait(result.fd, result.events, threadlet);
            break;
            
        case THREADLET_COMPLETED:
            // Threadlet finished, clean up resources
            threadlet_destroy(threadlet);
            break;
            
        case THREADLET_ERROR:
            // Handle threadlet error
            handle_threadlet_error(threadlet, result.error);
            break;
    }
}
```

---

## Socket Operation Integration

### Blocking Socket Read Example

When a threadlet calls `ptk_tcp_socket_recv()` (returns `ptk_err`):

1. **Attempt Non-blocking Read**:
   ```c
   ssize_t bytes_read = recv(sock->fd, buffer, length, MSG_DONTWAIT);
   if (bytes_read > 0) {
       // Data available, return immediately
       return PTK_OK;
   }
   ```

2. **Handle EAGAIN/EWOULDBLOCK**:
   ```c
   if (errno == EAGAIN || errno == EWOULDBLOCK) {
       // Register interest in read events
       ptk_event_loop_register_read(sock->fd, current_threadlet, timeout_ms);
       
       // Yield current threadlet
       ptk_threadlet_yield();
       
       // When resumed, check for timeout or error
       if (threadlet_has_error()) {
           ptk_err err = ptk_get_err();
           return err; // PTK_ERR_TIMEOUT or other error
       }
       
       // Try read again (should succeed now)
       bytes_read = recv(sock->fd, buffer, length, MSG_DONTWAIT);
   }
   ```

3. **Event Loop Wakes Threadlet**:
   - Socket becomes readable
   - Event loop moves threadlet from waiting to ready queue
   - Threadlet resumes execution after `ptk_threadlet_yield()`

### Socket Registration Process

```c
ptk_err ptk_event_loop_register_read(int fd, threadlet_t *threadlet, 
                                    ptk_duration_ms timeout_ms) {
    event_registration_t *reg = create_registration();
    reg->fd = fd;
    reg->waiting_threadlet = threadlet;
    reg->events = PTK_EVENT_READ;
    reg->timeout = timeout_ms;
    reg->deadline = current_time_ms + timeout_ms;
    
    // Add to platform event loop
    platform_add_fd_read(event_loop->platform, fd);
    
    // Store registration for lookup
    hash_table_insert(event_loop->registrations, fd, reg);
    
    // Move threadlet to waiting queue
    move_to_waiting_queue(threadlet);
    
    return PTK_OK;
}
```

---

## Platform-Specific Implementation

### Linux (epoll)

```c
typedef struct {
    int epoll_fd;
    struct epoll_event *events;
    int max_events;
} linux_event_loop_t;

int platform_poll_events(platform_event_loop_t *platform, int timeout_ms) {
    linux_event_loop_t *linux_loop = (linux_event_loop_t *)platform;
    
    int ready_count = epoll_wait(linux_loop->epoll_fd, 
                                linux_loop->events,
                                linux_loop->max_events,
                                timeout_ms);
    
    return ready_count;
}
```

### macOS/BSD (kqueue)

```c
typedef struct {
    int kqueue_fd;
    struct kevent *events;
    int max_events;
} bsd_event_loop_t;

int platform_poll_events(platform_event_loop_t *platform, int timeout_ms) {
    bsd_event_loop_t *bsd_loop = (bsd_event_loop_t *)platform;
    
    struct timespec timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_nsec = (timeout_ms % 1000) * 1000000
    };
    
    int ready_count = kevent(bsd_loop->kqueue_fd,
                            NULL, 0,  // No changes
                            bsd_loop->events, bsd_loop->max_events,
                            timeout_ms >= 0 ? &timeout : NULL);
    
    return ready_count;
}
```

---

## Threading and Synchronization

### Thread Affinity Rules

1. **Threadlet Binding**: Once a threadlet is assigned to an OS thread, it cannot migrate
2. **Socket Binding**: Sockets are bound to the event loop of their creating thread
3. **No Cross-Thread Operations**: All socket operations must occur on the owning thread

### Threadlet Distribution

New threadlets are distributed across OS threads using a simple load-balancing algorithm:

```c
ptk_err ptk_threadlet_resume(threadlet_t *threadlet) {
    // Find thread with least load
    ptk_thread_t *target_thread = find_least_loaded_thread();
    
    // Assign threadlet to that thread's event loop
    ptk_event_loop_t *target_loop = target_thread->event_loop;
    
    // Add to ready queue (thread-safe)
    thread_safe_enqueue_threadlet(target_loop, threadlet);
    
    // Wake up the target thread's event loop
    platform_wake_event_loop(target_loop->platform);
    
    return PTK_OK;
}
```

---

## Signal Handling and Interruption

### Socket Signaling

The `pkt_socket_signal()` function allows external threads to interrupt waiting threadlets:

```c
ptk_err pkt_socket_signal(ptk_sock *sock) {
    event_registration_t *reg = lookup_registration(sock->fd);
    if (reg && reg->waiting_threadlet) {
        // Set signal error
        threadlet_set_error(reg->waiting_threadlet, PTK_ERR_SIGNAL);
        
        // Move to ready queue
        move_to_ready_queue(reg->waiting_threadlet);
        
        // Wake event loop
        platform_wake_event_loop(sock->event_loop->platform);
    }
    return PTK_OK;
}
```

### Event Loop Interruption

Each platform provides a mechanism to wake a sleeping event loop:

- **Linux**: Use `eventfd()` or `pipe()` with epoll
- **macOS/BSD**: Use `kqueue` user events
- **Windows**: Use `PostQueuedCompletionStatus()`

---

## Performance Considerations

### Optimization Strategies

1. **Batch Processing**: Process multiple ready events before running threadlets
2. **Timeout Optimization**: Use efficient data structures (min-heap) for timeout tracking
3. **Memory Pool**: Reuse event registration structures to reduce allocation overhead
4. **Lock-Free Queues**: Use lock-free data structures for threadlet queues where possible

### Scaling Characteristics

- **Threadlets per Thread**: Each OS thread can efficiently handle thousands of threadlets
- **I/O Scalability**: Limited by platform event loop capacity (epoll: ~65K FDs, kqueue: similar)
- **Memory Usage**: Each blocked threadlet requires minimal memory (registration + stack)

---

## Error Handling

### Error Propagation

Errors flow from the event loop to threadlets through several mechanisms:

1. **I/O Errors**: Platform events indicate error conditions
2. **Timeouts**: Event loop detects expired deadlines
3. **Interrupts**: External interruption via `pkt_socket_signal()`
4. **System Errors**: Platform-specific error handling

### Cleanup Procedures

When errors occur:

1. Remove event registrations
2. Set appropriate error codes in threadlet context
3. Move threadlet to ready queue for error handling
4. Clean up platform-specific resources

---

## Summary

The PTK event loop and threadlet integration provides:

- **Cooperative Multitasking**: Thousands of concurrent "threads" per OS thread
- **Blocking Semantics**: Natural programming model with `ptk_tcp_socket_recv()`, etc.
- **High Performance**: Non-blocking I/O with efficient event multiplexing
- **Cross-Platform**: Unified API across Linux, macOS, Windows, and RTOS platforms
- **Scalable Architecture**: Per-thread event loops eliminate global synchronization

This design enables writing straightforward protocol implementations that scale to handle thousands of concurrent connections efficiently.