# Protocol Toolkit Tests

This directory contains comprehensive tests for the protocol toolkit functionality.

## Socket Tests

### Test 1: TCP Echo Server/Client with Abort (`test_tcp_echo_abort`)

**Test Description:**
- Starts a TCP server thread that listens on port 12345
- Server accepts connections and spawns client handler threads
- Each client handler echoes back any data received
- When server receives abort, it aborts all client sockets
- Starts a client thread that connects to server
- Client sets 500ms timer, waits for interrupt, sends message, reads response
- Test runs for 5 seconds then cleanly shuts down

**Test Features:**
- ✅ Multi-threaded TCP server with client handlers
- ✅ Timer-based interrupt handling
- ✅ Socket abort functionality
- ✅ Clean shutdown handling

### Test 2: UDP Echo Server/Client with Abort (`test_udp_echo_abort`)

**Test Description:**
- Starts a UDP server thread that listens on port 12346
- Server echoes back any UDP packets received
- When server receives abort, it stops listening
- Starts a client thread that connects to server
- Client sets timer, waits for interrupt, sends message, reads response
- Test runs for 5 seconds then cleanly shuts down

**Test Features:**
- ✅ UDP socket handling
- ✅ Timer-based interrupt handling
- ✅ Socket abort functionality
- ✅ Clean shutdown handling

## Type-Safe Serialization Tests

### Test 3: Type-Safe Serialization Test (`test_type_safe_serialize`)

**Test Description:**
- Comprehensive test suite for the type-safe buffer serialization system
- Tests basic serialization/deserialization of various data types
- Tests struct-based field serialization
- Tests endianness conversion (little-endian vs big-endian)
- Tests peek functionality (non-destructive reads)
- Tests error handling (buffer overflow/underflow protection)

**Test Features:**
- ✅ Automatic type detection using C11 _Generic
- ✅ Automatic argument counting
- ✅ EtherNet/IP header serialization
- ✅ Endianness conversion verification
- ✅ Peek functionality validation
- ✅ Buffer safety and error handling

### Demo 4: Type-Safe Serialization Demo (`demo_type_safe_serialize`)

**Demo Description:**
- Interactive demonstration of the type-safe serialization system
- Shows practical usage examples for protocol development
- Demonstrates EtherNet/IP header serialization
- Shows endianness handling for different protocols
- Demonstrates safety features and error protection
- Shows peek functionality for protocol parsing

**Demo Features:**
- ✅ Real-world protocol examples (EtherNet/IP)
- ✅ Interactive demonstrations
- ✅ Safety feature validation
- ✅ Performance characteristics
- ✅ Best practices examples

## Building and Running Tests

All tests are built automatically with the main project:

```bash
make -C build
```

Run individual tests:

```bash
./build/bin/test_tcp_echo_abort
./build/bin/test_udp_echo_abort
./build/bin/test_type_safe_serialize
./build/bin/demo_type_safe_serialize
```

## Test Coverage

The test suite covers:
- ✅ Socket functionality (TCP/UDP)
- ✅ Abort handling and clean shutdown
- ✅ Type-safe buffer serialization
- ✅ Multi-threading and concurrency
- ✅ Error handling and safety features
- ✅ Real-world protocol examples
- ✅ Performance and reliability testing