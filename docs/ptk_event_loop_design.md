# PTK Event Loop, Waitable, and Socket Interface Architecture

This document describes the architecture, design patterns, and implementation details for the platform event loop abstraction (`ptk_event_loop_platform`), waitable interface (`ptk_waitable`), and socket abstraction (`ptk_sock`) in the `protocol_toolkit`. It also provides guidance on file organization and discusses key implementation challenges and decisions.

---

## Overview

The goal of the PTK event loop architecture is to provide a unified, cross-platform foundation for synchronous looking I/O, timers, and other event sources. This enables portable, scalable network protocol implementations across Linux, macOS, BSD, Windows, and RTOS targets.

### Key Abstractions

- **ptk_event_loop_platform:** Platform-specific event multiplexing (epoll, kqueue, IOCP, etc.).
- **ptk_waitable:** Abstracts any object that can be waited on (sockets, timers, signals).
- **ptk_sock:** Unified socket API, wrapping OS networking primitives.

---

## Architecture and Design Patterns

### Layered Abstraction

1. **Platform Layer** (`src/lib/platform/`):
    - Implements platform event loop (e.g., `ptk_event_loop_platform_posix.c`, `ptk_event_loop_platform_linux.c`, `ptk_event_loop_platform_macos.c`, etc.).
    - Uses native APIs (epoll, kqueue, select, IOCP) to multiplex events.

2. **Core Layer** (`src/lib/`):
    - `ptk_event_loop.c`: Core event loop logic, delegates to platform-specific functions via `ptk_event_loop_platform.h`.
    - `ptk_waitable.c`/`.h`: Provides unified waitable API. Registers waitables with event loop and maps platform handles.
    - `ptk_sock.c`/`.h`: Portable socket interface, wraps system calls and integrates with waitable/event loop.

3. **Protocol Layer** (`src/lib/protocols/`):
    - Implements concrete protocol logic (e.g., EtherNet/IP, Modbus, etc.), using waitables and sockets for I/O.

### Design Patterns Used

- **Adapter Pattern:** Platform modules adapt OS APIs to the common event loop interface.
- **Facade Pattern:** `ptk_waitable` and `ptk_sock` expose simplified interfaces to higher-level code.
- **Strategy Pattern:** Event loop selects appropriate platform strategy at compile/run time.
- **Opaque Handles:** Internal data structures are opaque to users, promoting encapsulation and portability.

---

## Interface Details

### `ptk_event_loop_platform`

- **Purpose:** Abstract the OS-specific event multiplexing system.
- **API:** Create/destroy loop, register/unregister sources, wait for events.
- **Implementation:** Each OS/RTOS gets its own file in `src/lib/platform/`.
    - Example: `ptk_event_loop_platform_linux.c` uses `epoll`.
    - Example: `ptk_event_loop_platform_macos.c` uses `kqueue/kevent`.
    - Example: `ptk_event_loop_platform_win32.c` (future) uses IOCP.

### `ptk_waitable`

- **Purpose:** Abstract any object that can be waited on by the event loop.
- **API:** Create/destroy waitable, register with loop, set callbacks.
- **Implementation:** Waitables wrap platform handles (FDs, sockets, timers, etc.) and map these to event sources.
    - File: `src/lib/ptk_waitable.h`/`.c`.

### `ptk_sock`

- **Purpose:** Provide cross-platform socket API.
- **API:** Open/close sockets, read/write, set options, wait for events.
- **Implementation:** Wraps BSD sockets, Winsock, or RTOS networking APIs. Internally uses waitable/event loop for readiness and I/O.
    - File: `src/lib/ptk_sock.h`/`.c`.

---

## Implementation Issues & Challenges

### Platform Differences

- **Event APIs:** Linux supports `epoll`, BSD/macOS supports `kqueue`, Windows has IOCP; RTOSes often lack a native event API.
- **Timeout Handling:** Different APIs specify timeouts differently (ms, struct timeval, struct timespec).
- **Error Codes:** Need to normalize error handling across platforms.

### Resource Management

- **FD/Handle Mapping:** Must track mapping from platform handles (FDs, sockets) to event sources for efficient lookup.
- **Lifetime Management:** Opaque structs and reference counting may be needed to ensure safe destruction.

### Thread Safety

- **Concurrency:** If event loop is shared between threads, synchronization primitives (mutexes, atomics) may be needed.
- **Callbacks:** Must ensure callbacks from event loop are executed in a safe context.

### Extensibility

- **Timers, Signals:** Design allows for future extension to timers (POSIX timerfd, EVFILT_TIMER, Windows timers) and signals.
- **Custom Waitables:** Users can implement custom waitables for protocol-specific needs.

---

## File Organization

- `src/lib/ptk_event_loop_platform.h`  
    - Public platform event loop interface.

- `src/lib/platform/ptk_event_loop_platform_<platform>.c`  
    - Platform-specific implementation (Linux, macOS, Windows, RTOS).

- `src/lib/ptk_event_source.h`  
    - Event source abstraction (type, platform handle, callback).

- `src/lib/ptk_event_loop.c`  
    - Core event loop logic, delegates to platform.

- `src/lib/ptk_waitable.h` / `src/lib/ptk_waitable.c`  
    - Waitable abstraction and logic.

- `src/lib/ptk_sock.h` / `src/lib/ptk_sock.c`  
    - Cross-platform socket API.

---

## Example Flow

1. Protocol code creates a `ptk_sock` (socket).
2. Socket is wrapped in a `ptk_waitable` and registered with the event loop.
3. Event loop dispatches readiness events (read, write) to waitable callbacks.
4. Protocol code performs I/O; event loop ensures efficient, non-blocking operation.

---

## Future Directions

- **Asynchronous I/O:** Integrate with async APIs (io_uring, IOCP) for higher performance.
- **RTOS Support:** Add minimal event loop implementations for FreeRTOS, Zephyr, etc.
- **Advanced Features:** Support for timers, signals, and custom event sources.

---

## Summary

The PTK event loop, waitable, and socket interfaces provide a solid foundation for portable, scalable protocol implementations. By separating platform-specific logic and using well-known design patterns, the architecture is maintainable, extensible, and easy to adapt to new platforms and requirements.
