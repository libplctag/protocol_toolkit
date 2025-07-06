# Protocol Toolkit Socket Tests

This directory contains comprehensive tests for the socket functionality with abort handling.

## Tests

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
- ✅ Echo protocol implementation
- ✅ Graceful abort propagation (server → all clients)
- ✅ Clean shutdown after timeout

### Test 2: UDP Echo Server/Client with Abort (`test_udp_echo_abort`)

**Test Description:**
- Starts a UDP server thread that listens on port 12346
- Server receives UDP packets and echoes them back to sender
- When server receives abort, it stops the receive loop
- Starts a client thread that creates UDP socket
- Client sets 500ms timer, waits for interrupt, sends message, reads response
- Test runs for 5 seconds then cleanly shuts down

**Test Features:**
- ✅ UDP echo server implementation
- ✅ Timer-based interrupt handling
- ✅ UDP packet echo with sender identification
- ✅ Abort handling in UDP receive loop
- ✅ Clean shutdown after timeout

## Building and Running

```bash
# Build tests
cmake --build build --target test_tcp_echo_abort
cmake --build build --target test_udp_echo_abort

# Run tests
./build/bin/test_tcp_echo_abort
./build/bin/test_udp_echo_abort
```

## Expected Output

Both tests should:
1. Start server and client threads
2. Show timer interrupt firing at ~500ms
3. Display message exchange between client and server
4. Show clean abort and shutdown after 5 seconds
5. Complete with "Test completed" message

## Abort Functionality Tested

- **Immediate abort detection**: All socket operations check abort flag before starting
- **Event-driven abort**: kqueue user events wake up blocked operations instantly
- **Thread-safe abort**: External threads can abort socket operations safely
- **Abort propagation**: Server abort cascades to all client connections (TCP test)
- **Clean resource cleanup**: All sockets and threads shut down properly

## Error Handling

Tests demonstrate proper error handling for:
- Connection failures
- Read/write errors  
- Abort conditions
- Timeout scenarios
- Resource cleanup