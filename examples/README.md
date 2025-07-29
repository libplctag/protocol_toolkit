# Protocol Toolkit Examples

This directory contains example programs demonstrating the Protocol Toolkit macOS API with buffer-based socket operations.

## Building Examples

From the project root directory:

```bash
# Build the library first
mkdir build && cd build
cmake ..
make

# Build an example
cd ..
gcc -std=c99 -I build/include -L build/lib -o examples/working_example examples/working_example.c -lprotocol_toolkit -framework Foundation
```

## Available Examples

### working_example.c

A comprehensive test and demonstration of the Protocol Toolkit API including:
- Handle creation and manipulation
- Event loop creation
- Timer management
- Socket management
- User event sources
- Handle validation
- Error handling

**Run with:**
```bash
./examples/working_example
```

### udp_buffer_example.c

Demonstrates the buffer-based UDP socket API including:
- Buffer creation and management using `ptk_buffer_t`
- UDP sendto/recvfrom with buffers
- Broadcast functionality
- Multicast operations (join/leave groups, TTL, loopback)
- Automatic buffer size tracking

**Run with:**
```bash
./examples/udp_buffer_example
```

**Key Features Demonstrated:**
- `ptk_buffer_create()` for buffer initialization
- `ptk_socket_sendto()` and `ptk_socket_recvfrom()` with buffer structures
- `ptk_socket_enable_broadcast()` and `ptk_socket_broadcast()`
- `ptk_socket_join_multicast_group()` and multicast configuration

### tcp_buffer_example.c

Demonstrates the buffer-based TCP socket API including:
- Buffer creation with different capacities
- TCP send/receive with automatic size tracking
- Buffer size management and validation
- Multiple buffer size testing

**Run with:**
```bash
./examples/tcp_buffer_example
```

**Key Features Demonstrated:**
- `ptk_socket_send()` and `ptk_socket_receive()` with buffer structures
- Automatic tracking of sent/received bytes in buffer size field
- Buffer capacity vs. size management
- Zero-copy buffer operations

## Buffer-Based API Design

The Protocol Toolkit uses a buffer-centric approach for all socket operations:

### Buffer Structure
```c
typedef struct ptk_buffer {
    uint8_t *data;     // Pointer to the buffer data
    size_t size;       // Current size of data in buffer
    size_t capacity;   // Total capacity of the buffer
} ptk_buffer_t;
```

### Key Benefits
- **Zero-copy operations**: Buffers reference user-provided memory
- **Automatic size tracking**: Functions update `buffer->size` with actual bytes transferred
- **Capacity management**: Functions respect `buffer->capacity` limits
- **Type safety**: Strong typing prevents buffer overruns

### Buffer Usage Patterns

**For sending:**
```c
uint8_t data[256];
strcpy((char*)data, "Hello, World!");
ptk_buffer_t buffer = ptk_buffer_create(data, sizeof(data));
buffer.size = strlen("Hello, World!");
ptk_socket_send(socket, &buffer);  // buffer.size updated with bytes sent
```

**For receiving:**
```c
uint8_t data[256];
ptk_buffer_t buffer = ptk_buffer_create(data, sizeof(data));
ptk_socket_receive(socket, &buffer);  // buffer.size set to bytes received
printf("Received %zu bytes: %.*s\n", buffer.size, (int)buffer.size, buffer.data);
```

## API Design Notes

The Protocol Toolkit uses:

- **Zero Global State**: All resources are managed by user-provided resource pools
- **Zero Runtime Allocation**: All memory is pre-allocated at compile time
- **Event-Loop-Centric**: Resources are managed through event loops using Grand Central Dispatch
- **Handle-Based Safety**: 64-bit handles with generation counters prevent use-after-free
- **Buffer-Based I/O**: All socket operations use structured buffers for type safety and automatic size tracking

### Key Macros

- `PTK_DECLARE_EVENT_LOOP_SLOTS(name, count)` - Declares event loop slots
- `PTK_DECLARE_EVENT_LOOP_RESOURCES(name, timers, sockets, user_events)` - Declares resource pools
- `PTK_MAKE_HANDLE(type, loop_id, generation, handle_id)` - Creates handles manually
- `PTK_HANDLE_TYPE(handle)` - Extracts resource type from handle

### Core Functions

- `ptk_event_loop_create()` - Create a new event loop
- `ptk_timer_create()` - Create a new timer
- `ptk_socket_create_tcp()` / `ptk_socket_create_udp()` - Create sockets
- `ptk_socket_send()` / `ptk_socket_receive()` - Buffer-based TCP I/O
- `ptk_socket_sendto()` / `ptk_socket_recvfrom()` - Buffer-based UDP I/O
- `ptk_socket_broadcast()` / `ptk_socket_multicast_send()` - Specialized UDP operations
- `ptk_buffer_create()` - Initialize buffers
- `ptk_handle_is_valid()` - Validate handles
- `ptk_error_string()` - Get human-readable error messages

This implementation provides a solid foundation for building network protocol implementations on macOS using modern GCD-based event handling with efficient, type-safe buffer management.