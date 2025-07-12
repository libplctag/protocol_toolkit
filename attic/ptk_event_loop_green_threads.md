# PTK Event Loop, Waitable, and Socket Architecture with Green Threads

This document describes the architecture, API, and design changes for `ptk_waitable` and `ptk_sock` when blocking is managed via green threads/coroutines (such as libdill or libtask), and multiple OS threads each run their own event loop. The green thread scheduler runs on top of the event loop in each thread.

---

## Overview

- Each OS thread runs a separate platform event loop.
- Green threads (coroutines) are scheduled on top of the event loop within each thread.
- Blocking I/O is simulated in user space: when a green thread waits for I/O, it yields until the event loop signals readiness.
- APIs must support coroutine-friendly blocking and event notification.

---

## Architecture Changes

### Event Loop per Thread

- Instead of a single global event loop, each OS thread creates its own event loop instance (`ptk_event_loop_platform_t`).
- Each thread runs its own green thread scheduler, multiplexing coroutines on top of the event loop.
- No shared event loop state; resources registered with an event loop belong to the thread that owns it.

### Green Thread Integration

- When a coroutine performs a blocking operation (like socket read), it registers interest with the event loop and suspends itself.
- The event loop wakes the coroutine when the event is ready.
- Coroutine APIs provide natural blocking semantics without actual OS thread blocking.

---

## API Design: Coroutine-Friendly

### ptk_waitable.h

Key changes:
- Remove traditional callback or poll APIs; replace with coroutine/blocking APIs.
- Waitable operations are designed to be called from within a green thread context and will yield if blocking.

```c name=src/ptk_waitable.h
#pragma once

#include <stdint.h>
#include <ptk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ptk_waitable ptk_waitable_t;
typedef struct ptk_event_loop_platform ptk_event_loop_platform_t;

typedef enum {
    PTK_WAITABLE_SOCKET,
    PTK_WAITABLE_TIMER,
    PTK_WAITABLE_SIGNAL,
    PTK_WAITABLE_USER,
} ptk_waitable_type_t;

/**
 * Create a waitable and register it with an event loop (thread local).
 */
ptk_waitable_t *ptk_waitable_create(ptk_event_loop_platform_t *loop, ptk_waitable_type_t type, void *platform_handle);

/**
 * Destroy the waitable (must not be registered with event loop).
 */
void ptk_waitable_destroy(ptk_waitable_t *waitable);

/**
 * Wait for readiness (blocking from coroutine context).
 * This call yields the coroutine until the waitable is ready for the specified event.
 * Returns PTK_OK on success, error code otherwise.
 */
ptk_err ptk_waitable_wait(ptk_waitable_t *waitable, uint32_t events, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
```

#### Notes

- `ptk_waitable_wait` is coroutine/blocking-friendly. It suspends the calling green thread until the waitable is ready, then resumes it.
- Events encode readiness (read/write/error), similar to epoll/kqueue masks.

---

### ptk_sock.h

Key changes:
- Socket operations (connect, send, recv, accept) are coroutine/blocking-friendly.
- All socket operations block by yielding the calling green thread until the operation can proceed.

```c name=src/ptk_sock.h
#pragma once

#include <ptk_waitable.h>
#include <stdint.h>
#include <ptk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ptk_sock ptk_sock_t;

/**
 * Open a socket and associate it with a thread-local event loop.
 */
ptk_sock_t *ptk_sock_open(ptk_event_loop_platform_t *loop, int af, int type, int proto);

/**
 * Close a socket.
 */
void ptk_sock_close(ptk_sock_t *sock);

/**
 * Coroutine-friendly blocking connect.
 */
ptk_err ptk_sock_connect(ptk_sock_t *sock, const struct sockaddr *addr, socklen_t addrlen, uint32_t timeout_ms);

/**
 * Coroutine-friendly blocking send.
 */
ptk_err ptk_sock_send(ptk_sock_t *sock, const void *buf, size_t len, uint32_t timeout_ms);

/**
 * Coroutine-friendly blocking recv.
 */
ptk_err ptk_sock_recv(ptk_sock_t *sock, void *buf, size_t len, uint32_t timeout_ms);

/**
 * Coroutine-friendly blocking accept.
 */
ptk_sock_t *ptk_sock_accept(ptk_sock_t *sock, struct sockaddr *addr, socklen_t *addrlen, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
```

#### Notes

- All blocking operations are implemented via coroutine yield/resume, not OS thread blocking.
- The socket is always associated with a specific event loop (thread).
- Each coroutine must be scheduled on the same thread as its socket/event loop.

---

## Design Considerations

### Thread Affinity

- Waitables and sockets must not migrate between threads/event loops; they remain bound to the event loop of their creating thread.
- Green threads must run on the same thread as their associated waitables.

### Coroutine Scheduler Integration

- The coroutine library (e.g., libdill, libtask) is responsible for managing suspension/resumption.
- The event loop must provide hooks for waking coroutines when waitables become ready.

### Resource Ownership

- Waitable and socket lifetimes are managed by the owning thread.
- Destruction must ensure that no coroutine is awaiting the waitable.

### Platform Event Loop

- Implementation files remain in `src/platform/`, but each thread creates and owns its own instance.
- No shared data structures between event loops.

---

## Implementation Issues

- **Deadlock:** Care must be taken to avoid deadlocks if coroutines block on resources that will never become ready.
- **Starvation:** Coroutine scheduler must fairly wake blocked coroutines.
- **Timeouts:** All blocking APIs accept timeouts; event loop must handle these correctly per coroutine.
- **Cleanup:** Coroutine and resource destruction must be coordinated to avoid use-after-free.

---

## File Organization

- `src/ptk_event_loop_platform.h` / `src/platform/ptk_event_loop_platform_<platform>.c`  
    Platform event loop per thread.

- `src/ptk_waitable.h` / `src/ptk_waitable.c`  
    Coroutine-friendly waitable API.

- `src/ptk_sock.h` / `src/ptk_sock.c`  
    Coroutine-friendly socket API.

- `src/ptk_event_source.h`  
    Internal event source abstraction.

---

## Example Usage Pattern

```c
// In thread main:
ptk_event_loop_platform_t *loop = ptk_event_loop_platform_create();

// Start coroutine scheduler in this thread
ptk_scheduler_run(loop);

// In coroutine:
ptk_sock_t *sock = ptk_sock_open(loop, AF_INET, SOCK_STREAM, 0);
ptk_sock_connect(sock, ...); // blocks coroutine, not thread
ptk_sock_send(sock, ...);    // blocks coroutine, not thread
ptk_sock_recv(sock, ...);    // blocks coroutine, not thread
ptk_sock_close(sock);
```

---

## Summary

By using a per-thread event loop and coroutine-friendly APIs, `protocol_toolkit` can efficiently support scalable I/O with natural blocking semantics, without blocking OS threads. This design is suitable for high concurrency network protocols on all major platforms.

