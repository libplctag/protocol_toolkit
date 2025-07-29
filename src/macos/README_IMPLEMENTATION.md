# Protocol Toolkit macOS Implementation using kevent

## Overview

This document describes the implementation sketch for the Protocol Toolkit API on macOS using the kevent system call for event handling. The implementation follows the critical goals of zero dynamic memory allocation while providing an efficient event-driven framework for industrial network protocol stacks.

## Key Design Principles

### 1. Zero Allocation
- All memory is managed by the application
- Platform-specific data is embedded directly in public structures
- Static arrays are used for event buffers and timer management
- No malloc/free calls in the implementation

### 2. kevent Integration
- Uses macOS kqueue/kevent for efficient event multiplexing
- Handles socket I/O, timers, and user events uniformly
- Non-blocking socket operations with event-driven callbacks

### 3. Embedded-Friendly
- Deterministic behavior with bounded resource usage
- Clear separation between platform-agnostic API and platform implementation
- Configurable limits via compile-time constants

## Architecture

### Platform-Specific Extensions

The implementation extends the public structures with platform-specific data:

```c
// Added to protocol_toolkit.h
#define PTK_MAX_KEVENTS 64
#define PTK_MAX_TIMERS_PER_LOOP 32

typedef struct ptk_event_source {
    // ... existing fields ...
    struct {
        enum { PTK_ES_TIMER, PTK_ES_SOCKET, PTK_ES_USER } type;
        uintptr_t ident;      // kevent identifier
        bool active;          // Registration state
        struct timespec next_fire;  // For timers
    } macos;
} ptk_event_source_t;
```

### Event Loop Implementation

The event loop uses kqueue for efficient event handling:

1. **kqueue Setup**: Creates kqueue file descriptor during initialization
2. **Event Registration**: Sockets and timers are registered with kevent
3. **Event Processing**: Main loop calls kevent() with appropriate timeout
4. **Timer Management**: Software timers using timespec for precise timing

### Socket Implementation

Sockets are configured as non-blocking and integrated with kevent:

1. **Creation**: Standard BSD socket calls with O_NONBLOCK
2. **Registration**: File descriptors registered for read/write events
3. **I/O Operations**: Non-blocking send/recv with EAGAIN handling
4. **Event Delivery**: Socket events trigger state machine transitions

### Timer Implementation

Timers are implemented in software with kevent timeout support:

1. **Static Allocation**: Fixed array of timer slots
2. **Precise Timing**: Uses CLOCK_MONOTONIC for consistent timing
3. **Periodic Support**: Automatic re-arming for periodic timers
4. **Integration**: Timer expiration triggers state machine events

## Key Implementation Files

### protocol_toolkit.h (Updated)
- Original API with platform-specific extensions
- Embedded macOS-specific data in public structures
- Maintains API compatibility while enabling zero-allocation

### protocol_toolkit_macos.c
- Complete implementation using kevent
- Helper functions for time management and socket setup
- Event loop with integrated timer and socket handling

### example_tcp_client.c
- Demonstrates usage patterns
- Shows static memory allocation approach
- Example state machine for TCP client protocol

## Usage Pattern

```c
// All memory allocated statically by application
static ptk_transition_t transitions[10];
static ptk_transition_table_t transition_table;
static ptk_event_source_t timer_source;
static ptk_state_machine_t state_machine;
static ptk_loop_t event_loop;

// Initialize components
ptk_tt_init(&transition_table, transitions, 10);
ptk_sm_init(&state_machine, ...);
ptk_loop_init(&event_loop, &state_machine);

// Attach event sources
ptk_es_init_timer(&timer_source, EVENT_TIMEOUT, 5000, true, NULL);
ptk_sm_attach_event_source(&state_machine, &timer_source);

// Run event loop
ptk_loop_run(&event_loop);
```

## Resource Limits

The implementation uses compile-time constants for resource limits:

- `PTK_MAX_KEVENTS`: Maximum events per kevent() call (64)
- `PTK_MAX_TIMERS_PER_LOOP`: Maximum concurrent timers (32)
- Application controls all other limits through static allocation

## Error Handling

- Comprehensive error codes for all failure modes
- Non-blocking socket operations handle EAGAIN gracefully
- Resource exhaustion detected at attach time, not runtime

## Performance Characteristics

### Event Loop Efficiency
- Single kevent() call handles all I/O events
- O(1) timer management with static array
- Minimal system calls in steady state

### Memory Footprint
- Fixed memory usage determined at compile time
- No heap fragmentation issues
- Predictable memory access patterns

### Latency
- Direct event delivery to state machines
- No intermediate buffering or queuing
- Bounded processing time per event

## Integration with Industrial Protocols

This implementation is well-suited for industrial network protocols:

### EtherNet/IP
- State machines for CIP connections
- Timer-based keepalive mechanisms
- UDP multicast for I/O messaging

### Modbus TCP
- Connection state management
- Transaction timeout handling
- Multiple concurrent client connections

### Custom Protocols
- Flexible state machine framework
- Timer-driven protocol timeouts
- Event-driven packet processing

## Compilation and Building

```bash
# Build the library and example
cd src/macos
make all

# Run the example
./example_tcp_client
```

## Testing Considerations

The implementation should be tested with:

1. **Stress Testing**: Maximum concurrent sockets and timers
2. **Timing Verification**: Timer accuracy under load
3. **Resource Exhaustion**: Graceful handling of limit conditions
4. **Protocol Compliance**: Integration with real protocol stacks

## Future Enhancements

Potential improvements to consider:

1. **Socket Event Sources**: Direct integration of socket events with state machines
2. **High-Resolution Timers**: Microsecond precision for real-time protocols
3. **Multi-Threading**: Support for multiple event loops in different threads
4. **Memory Pools**: Optional memory pool support for variable-size buffers

## Questions Addressed

> **Q**: How to achieve zero allocation?
> **A**: Embed platform-specific data in public structures and use static arrays for all internal bookkeeping.

> **Q**: How to integrate kevent efficiently?
> **A**: Use kqueue as the primary event multiplexer, with software timers managed alongside socket events.

> **Q**: How to maintain API portability?
> **A**: Keep platform-specific data in clearly marked sections of public structures, with the core API remaining identical across platforms.

This implementation provides a solid foundation for building industrial network protocol stacks on macOS while maintaining the embedded-friendly characteristics required for the target applications.
