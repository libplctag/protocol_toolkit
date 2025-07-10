# Timer API Integration Guide

## Overview

The minimal timer API provides a simple, global timer system that integrates seamlessly with the Protocol Toolkit's event framework. It's designed for single-protocol applications where simplicity is key.

## Key Features

- **Global Management**: Single, internal timer manager - no manager objects to create
- **Simple API**: Just 7 functions for complete timer functionality
- **Event Integration**: Designed to work with select/poll-based event loops
- **Minimal Overhead**: Fixed-size timer pool, no dynamic allocation in hot paths
- **Protocol Agnostic**: Works with any protocol implementation

## Integration with Event Framework

The timer API integrates with the existing socket event framework through two key functions:

### 1. Getting Poll Timeout

```c
// In your event loop
int timeout = ptk_timer_get_next_timeout();
if (timeout == -1) {
    // No timers active, use default timeout or block indefinitely
    timeout = -1;  // Block indefinitely in poll/select
}

// Use timeout in your select/poll call
struct timeval tv;
tv.tv_sec = timeout / 1000;
tv.tv_usec = (timeout % 1000) * 1000;
select(max_fd + 1, &read_fds, &write_fds, NULL, &tv);
```

### 2. Processing Expired Timers

```c
// After select/poll returns, process expired timers
int expired_count = ptk_timer_process_expired();
if (expired_count > 0) {
    // Timers were processed, continue event loop
}
```

## Protocol Usage Patterns

### 1. Request Timeouts

```c
// Create timeout when sending request
ptk_timer_t timeout = ptk_timer_create_oneshot(5000, request_timeout_handler, &request_id);

// Cancel timeout when response received
if (response_received) {
    ptk_timer_cancel(timeout);
}
```

### 2. Keepalive/Heartbeat

```c
// Start keepalive timer
ptk_timer_t keepalive = ptk_timer_create_repeating(30000, send_keepalive, connection);

// Stop when connection closes
ptk_timer_cancel(keepalive);
```

### 3. Periodic Tasks

```c
// Stats, cleanup, etc.
ptk_timer_t stats = ptk_timer_create_repeating(10000, update_stats, NULL);
```

## Memory Management

- **No Dynamic Allocation**: Uses fixed-size pool of 64 timers
- **Automatic Cleanup**: Timer system cleans up on shutdown
- **No Memory Leaks**: Timers automatically freed when canceled or expired

## Error Handling

- **Simple Return Values**: 0 for success, negative for errors
- **NULL Checks**: API handles NULL pointers gracefully
- **State Validation**: Checks for proper initialization

## Example Event Loop Integration

```c
int main_event_loop() {
    // Initialize timer system
    if (ptk_timer_init() != 0) {
        return -1;
    }

    // Main event loop
    while (running) {
        // Prepare file descriptors
        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        // Add protocol sockets to fd_set
        add_protocol_sockets(&read_fds, &write_fds);

        // Get timeout for next timer
        int timeout = ptk_timer_get_next_timeout();
        struct timeval tv;
        struct timeval *tv_ptr = NULL;

        if (timeout >= 0) {
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;
            tv_ptr = &tv;
        }

        // Wait for events
        int result = select(max_fd + 1, &read_fds, &write_fds, NULL, tv_ptr);

        if (result < 0) {
            if (errno == EINTR) continue;
            break;  // Error
        }

        // Process expired timers first
        ptk_timer_process_expired();

        // Process socket events
        if (result > 0) {
            handle_socket_events(&read_fds, &write_fds);
        }
    }

    // Cleanup
    ptk_timer_cleanup();
    return 0;
}
```

## Performance Characteristics

- **O(n) timer operations**: Linear scan of timer pool
- **Acceptable for n <= 64**: Suitable for typical protocol applications
- **No heap allocations**: All memory pre-allocated
- **Minimal overhead**: Simple data structures

## Thread Safety

- **Single-threaded**: Designed for single-threaded event loops
- **No locking**: No mutex/lock overhead
- **Simple**: Easy to understand and debug

## Debugging Support

- **Debug Logging**: Compile with -DDEBUG for detailed logs
- **Timer IDs**: Each timer has unique ID for tracking
- **State Validation**: Checks for common errors

This timer API provides exactly what most protocol implementations need: simple, reliable timers that integrate cleanly with event loops without unnecessary complexity.
