# Cross-Platform Socket and Utils Restructuring Proposal

## Overview

This document proposes a restructuring of `src/lib/ptk_socket.c` and `src/lib/ptk_utils.c` to support macOS, BSD, Linux, and Windows with minimal platform-specific code separation. The goal is to keep common BSD socket operations unified using `#ifdef` conditionals while isolating truly platform-specific code (like kqueue/epoll/IOCP) into separate files.

## Current State Analysis

### ptk_socket.c Issues
- **macOS/BSD-only**: Currently uses kqueue exclusively for event handling
- **No Windows support**: No IOCP or select/poll fallback implementation
- **No Linux support**: No epoll implementation for optimal Linux performance
- **Monolithic structure**: ~1020 lines with all platform code intermixed

### ptk_utils.c Issues
- **Partial cross-platform**: Has Windows/Unix `#ifdef` but limited functionality
- **Missing platform optimizations**: Could use platform-specific high-resolution timers
- **Signal handling**: Unix-only signal handling with basic Windows compatibility

## Proposed Structure

### 1. Socket Implementation Restructuring

#### Core Socket File: `src/lib/ptk_socket.c`
Contains all platform-independent socket operations:
- Socket creation, binding, listening
- Basic TCP connect/accept logic
- UDP send/recv operations
- Address resolution
- Socket option setting
- Buffer management integration
- Common error mapping
- Socket structure management

#### Platform Event Files:

**`src/lib/ptk_socket_kqueue.c`** (macOS, FreeBSD, OpenBSD, NetBSD)
- kqueue-specific event handling
- Timer implementation using EVFILT_TIMER
- User events for interrupts/abort (EVFILT_USER)
- Efficient handling of multiple sockets

**`src/lib/ptk_socket_epoll.c`** (Linux)
- epoll-specific event handling
- Timer implementation using timerfd
- eventfd for interrupts/abort signaling
- Optimized for high-connection-count scenarios

**`src/lib/ptk_socket_iocp.c`** (Windows)
- IOCP (I/O Completion Ports) implementation
- Asynchronous I/O operations
- Timer implementation using Windows timer APIs
- Event signaling using Windows events

**`src/lib/ptk_socket_select.c`** (Fallback for all platforms)
- select()-based implementation for maximum compatibility
- Less efficient but universally supported
- Timer implementation using timeouts in select()
- Used when platform-specific implementations unavailable

#### Platform Selection Logic:
```c
// In ptk_socket.c
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #include "ptk_socket_kqueue.h"
    #define PTK_SOCKET_BACKEND_KQUEUE
#elif defined(__linux__)
    #include "ptk_socket_epoll.h"
    #define PTK_SOCKET_BACKEND_EPOLL
#elif defined(_WIN32)
    #include "ptk_socket_iocp.h"
    #define PTK_SOCKET_BACKEND_IOCP
#else
    #include "ptk_socket_select.h"
    #define PTK_SOCKET_BACKEND_SELECT
#endif
```

### 2. Socket Backend Interface

Each platform file implements a common interface:

```c
// Platform backend interface (internal)
typedef struct ptk_socket_backend {
    ptk_err (*setup_event_system)(ptk_sock *sock);
    ptk_err (*wait_for_event)(ptk_sock *sock, int16_t filter);
    ptk_err (*add_event)(ptk_sock *sock, int16_t filter);
    ptk_err (*start_timer)(ptk_sock *sock, ptk_duration_ms period_ms);
    ptk_err (*stop_timer)(ptk_sock *sock);
    ptk_err (*send_interrupt)(ptk_sock *sock);
    ptk_err (*send_abort)(ptk_sock *sock);
    ptk_err (*cleanup)(ptk_sock *sock);
} ptk_socket_backend;

// Each platform file provides:
extern const ptk_socket_backend ptk_socket_backend_impl;
```

### 3. Updated Socket Structure

```c
typedef struct ptk_sock {
    uint32_t magic;                     // Magic number for validation
    ptk_sock_type type;                 // Socket type
    int fd;                             // OS socket file descriptor

    // Platform-specific event system data
    union {
        struct {                        // kqueue (macOS/BSD)
            int kqueue_fd;
            int timer_ident;
            int abort_ident;
        } kqueue;
        struct {                        // epoll (Linux)
            int epoll_fd;
            int timer_fd;
            int abort_fd;
        } epoll;
        struct {                        // IOCP (Windows)
            HANDLE iocp_handle;
            HANDLE timer_handle;
            HANDLE abort_event;
        } iocp;
        struct {                        // select (fallback)
            bool has_timer;
            ptk_time_ms timer_start;
            ptk_duration_ms timer_period;
            int wake_sock_read_fd;
            int wake_sock_write_fd;
        } select;
    } event_data;

    // Common socket data
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;
    socklen_t local_addr_len;
    socklen_t remote_addr_len;
    bool connected;
    bool listening;
    bool timer_active;
    ptk_duration_ms timer_period_ms;
    void (*interrupt_handler)(ptk_sock *sock, ptk_time_ms time_ms, void *user_data);
    void *interrupt_user_data;
    bool aborted;
} ptk_sock;
```

### 4. Utils Implementation Restructuring

#### Core Utils File: `src/lib/ptk_utils.c`
Contains cross-platform utility functions with platform selection:
- Unified time API with platform-specific implementations
- Cross-platform signal/interrupt handling
- Memory utilities
- String utilities
- Platform detection and capability queries

#### Platform-Specific Implementations:

**Time Functions:**
```c
ptk_time_ms ptk_now_ms(void) {
#ifdef _WIN32
    // Windows: Use QueryPerformanceCounter for high precision
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }

    QueryPerformanceCounter(&counter);
    return (ptk_time_ms)((counter.QuadPart * 1000) / frequency.QuadPart);

#elif defined(__APPLE__)
    // macOS: Use mach_absolute_time with mach_timebase_info
    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }

    uint64_t abs_time = mach_absolute_time();
    return (ptk_time_ms)((abs_time * timebase.numer) / (timebase.denom * 1000000));

#else
    // Linux/BSD: Use clock_gettime with CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (ptk_time_ms)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }

    // Fallback to gettimeofday
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (ptk_time_ms)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}
```

**Signal/Interrupt Handling:**
```c
#ifdef _WIN32
    // Windows: Use SetConsoleCtrlHandler
    static BOOL WINAPI windows_ctrl_handler(DWORD ctrl_type);
#else
    // Unix: Use sigaction with proper signal masks
    static void unix_signal_handler(int sig);
    static void setup_unix_signals(void);
#endif
```

## Implementation Plan

### Phase 1: Socket Backend Abstraction
1. **Extract platform-specific code** from current `ptk_socket.c`
2. **Create backend interface** header `ptk_socket_backend.h`
3. **Implement kqueue backend** `ptk_socket_kqueue.c` (migrate existing code)
4. **Update socket structure** to use union for platform data
5. **Modify core socket functions** to use backend interface

### Phase 2: Additional Platform Backends
1. **Implement epoll backend** `ptk_socket_epoll.c` for Linux
2. **Implement IOCP backend** `ptk_socket_iocp.c` for Windows
3. **Implement select fallback** `ptk_socket_select.c` for compatibility
4. **Add platform detection** and automatic backend selection

### Phase 3: Utils Enhancement
1. **Enhance time functions** with platform-specific high-resolution timers
2. **Improve signal handling** for cross-platform compatibility
3. **Add platform capability detection** functions
4. **Add utility functions** for network interface enumeration per platform

### Phase 4: Build System Updates
1. **Update CMakeLists.txt** to compile appropriate platform files
2. **Add compiler flags** for platform-specific features
3. **Add feature detection** for optional APIs (epoll, kqueue, etc.)
4. **Create platform-specific test suites**

## File Organization

```
src/lib/
├── ptk_socket.c              # Core socket implementation
├── ptk_socket_backend.h      # Backend interface definition
├── ptk_socket_kqueue.c       # macOS/BSD backend
├── ptk_socket_epoll.c        # Linux backend
├── ptk_socket_iocp.c         # Windows backend
├── ptk_socket_select.c       # Fallback backend
├── ptk_utils.c               # Enhanced cross-platform utilities
└── platform/                 # Platform-specific helpers
    ├── ptk_platform_detect.c # Platform detection utilities
    ├── ptk_time_win32.c      # Windows-specific time functions
    ├── ptk_time_unix.c       # Unix-specific time functions
    └── ptk_signal_handlers.c  # Cross-platform signal handling
```

## Benefits

### Performance
- **Platform-optimized I/O**: kqueue on BSD, epoll on Linux, IOCP on Windows
- **High-resolution timers**: Platform-specific timer implementations
- **Reduced overhead**: Direct platform API usage where appropriate

### Maintainability
- **Clear separation**: Platform-specific code isolated but following common interface
- **Shared logic**: Common socket operations remain unified
- **Easy testing**: Each backend can be tested independently
- **Future expansion**: Easy to add new platforms or backends

### Compatibility
- **Broad platform support**: Windows, macOS, Linux, BSDs
- **Graceful fallback**: select() backend for unsupported platforms
- **Compile-time selection**: No runtime overhead for unused backends
- **Feature detection**: Runtime capability checking where needed

## Migration Considerations

### Backward Compatibility
- **API preserved**: No changes to public socket API in `ptk_socket.h`
- **Behavior maintained**: Same interrupt, abort, and timer semantics
- **Error codes unchanged**: Existing error mappings preserved

### Testing Strategy
- **Platform-specific test suites**: Validate each backend independently
- **Cross-platform integration tests**: Ensure consistent behavior
- **Performance benchmarks**: Verify platform optimizations effective
- **Stress testing**: High-connection scenarios on each platform

### Documentation Updates
- **Platform support matrix**: Document supported features per platform
- **Build instructions**: Platform-specific build requirements
- **Performance characteristics**: Expected performance per platform
- **Troubleshooting guide**: Platform-specific debugging information

## Conclusion

This restructuring provides a solid foundation for cross-platform socket operations while maintaining high performance through platform-specific optimizations. The modular design allows for easy maintenance and future expansion while preserving the existing API and behavior.

The separation of concerns between core socket logic and platform-specific event handling creates a maintainable codebase that can evolve with platform capabilities while ensuring consistent behavior across all supported systems.
