# Protocol Toolkit Arithmetic Example

This example demonstrates how to create a simple client-server protocol using the Protocol Toolkit library. The example implements an arithmetic service where clients can send mathematical operations to a server and receive results.

## Protocol Specification

### Request Packet (Client to Server)
- **Byte 0**: Operation code (1=+, 2=-, 3=*, 4=/)
- **Bytes 1-4**: First operand (32-bit float, big-endian)
- **Bytes 5-8**: Second operand (32-bit float, big-endian)
- **Bytes 9-10**: 16-bit CRC (big-endian)

Total size: 11 bytes

### Response Packet (Server to Client)
- **Byte 0**: Inverted operation code (bitwise NOT of original operation)
- **Bytes 1-8**: Result (64-bit double, little-endian)
- **Byte 9**: 8-bit CRC

Total size: 10 bytes

## Architecture Overview

The example consists of several components:

### 1. CRC Implementation (`crc.h`, `crc.c`)

Simple CRC implementations for packet integrity:

```c
uint16_t crc16_calculate(const uint8_t *data, size_t length);
uint8_t crc8_calculate(const uint8_t *data, size_t length);
```

- **CRC16**: Uses polynomial 0xA001 for client-to-server packets
- **CRC8**: Uses polynomial 0x07 for server-to-client packets

### 2. Protocol Structures (`arithmetic_protocol.h`, `arithmetic_protocol.c`)

Defines the PDU structures and serialization logic:

```c
typedef struct {
    ptk_serializable_t base;    // Enables automatic serialization
    u8 operation;               // Operation code
    f32 operand1;              // First operand  
    f32 operand2;              // Second operand
    u16 crc;                   // CRC checksum
} arithmetic_request_t;
```

Key features:
- **Inheritance from ptk_serializable_t**: Enables type-safe serialization
- **Automatic memory management**: Uses PTK allocation with destructors
- **Built-in CRC calculation**: Computed during serialization
- **Endianness handling**: Big-endian for requests, little-endian for responses

### 3. Server Implementation (`server.c`)

Multi-threaded TCP server that:

1. **Listens for connections** on a configurable port
2. **Creates threadlets** for each client connection using `ptk_threadlet_create()`
3. **Shares connection data** safely between threadlets using `ptk_shared_wrap()`
4. **Processes arithmetic requests** in parallel

#### Key Server Features

**Threadlet-Based Concurrency:**
```c
threadlet_t *client_threadlet = ptk_threadlet_create(handle_client_connection, handle_param);
ptk_threadlet_resume(client_threadlet);
```

**Safe Memory Sharing:**
```c
ptk_shared_handle_t conn_handle = ptk_shared_wrap(conn);
use_shared(conn_handle, client_connection_t *conn) {
    // Safe access to shared connection data
} on_shared_fail {
    error("Failed to acquire shared memory");
}
```

**Type-Safe Serialization:**
```c
ptk_err err = arithmetic_request_deserialize(request_buf, (ptk_serializable_t*)request);
if (err != PTK_OK) {
    error("Failed to deserialize request: %d", err);
    continue;
}
```

### 4. Client Implementation (`client.c`)

Simple TCP client that:

1. **Connects to server** using `ptk_tcp_socket_connect()`
2. **Sends arithmetic request** with user-specified operation and operands
3. **Receives and displays result**
4. **Handles command-line arguments** using PTK configuration system

#### Key Client Features

**Command-Line Configuration:**
```c
ptk_config_field_t config_fields[] = {
    {"server", 's', PTK_CONFIG_STRING, &server_ip, "Server IP address", "127.0.0.1"},
    {"port", 'p', PTK_CONFIG_UINT16, &server_port, "Server port", "12345"},
    {"operation", 'o', PTK_CONFIG_STRING, &operation_str, "Operation (+, -, *, /)", "+"},
    {"operand1", '1', PTK_CONFIG_STRING, &operand1_str, "First operand", "5.0"},
    {"operand2", '2', PTK_CONFIG_STRING, &operand2_str, "Second operand", "3.0"},
    PTK_CONFIG_END
};
```

**Robust Error Handling:**
```c
if (!response_buf) {
    ptk_err err = ptk_get_err();
    error("Failed to receive response: %d", err);
    return err;
}
```

## Protocol Toolkit APIs Used

### Memory Management (ptk_alloc.h)
- `ptk_alloc()`: Allocate memory with destructors
- `ptk_free()`: Safe deallocation with pointer nulling
- `ptk_realloc()`: Resize allocations while preserving data

Example:
```c
client_connection_t *conn = ptk_alloc(sizeof(client_connection_t), client_connection_destructor);
// ... use conn ...
ptk_free(&conn);  // conn is now NULL
```

### Buffer Management (ptk_buf.h)
- `ptk_buf_alloc()`: Create buffers for network I/O
- `ptk_buf_serialize()`: Type-safe serialization with endianness control
- `ptk_buf_deserialize()`: Type-safe deserialization with validation

Example:
```c
ptk_buf *request_buf = ptk_buf_alloc(64);
ptk_err err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, 
                               req->operation, req->operand1, req->operand2);
```

### Networking (ptk_sock.h)
- `ptk_tcp_socket_listen()`: Create listening server socket
- `ptk_tcp_socket_accept()`: Accept client connections
- `ptk_tcp_socket_connect()`: Connect to remote server
- `ptk_tcp_socket_send()`: Send data with timeout
- `ptk_tcp_socket_recv()`: Receive data with timeout

Example:
```c
ptk_sock *server_sock = ptk_tcp_socket_listen(&server_addr, 10);
ptk_sock *client_sock = ptk_tcp_socket_accept(server_sock, 0);
```

### Green Threads (ptk_threadlet.h)
- `ptk_threadlet_create()`: Create cooperative threads
- `ptk_threadlet_resume()`: Schedule threadlet execution
- `ptk_threadlet_join()`: Wait for threadlet completion
- `ptk_threadlet_yield()`: Cooperative yielding (used internally by socket operations)

Example:
```c
threadlet_t *threadlet = ptk_threadlet_create(worker_function, param);
ptk_threadlet_resume(threadlet);
ptk_threadlet_join(threadlet, 0);  // Wait indefinitely
```

### Shared Memory (ptk_shared.h)
- `ptk_shared_wrap()`: Wrap allocated memory for sharing
- `ptk_shared_acquire()`: Get pointer with reference counting
- `ptk_shared_release()`: Release reference
- `use_shared()` macro: Safe acquire/release pattern

Example:
```c
ptk_shared_handle_t handle = ptk_shared_wrap(my_data);
use_shared(handle, my_struct_t *ptr) {
    ptr->field = value;  // Safe access
} on_shared_fail {
    error("Acquisition failed");
}
```

### Logging (ptk_log.h)
- `info()`, `debug()`, `warn()`, `error()`: Leveled logging
- `debug_buf()`: Hex dump of binary data
- `ptk_log_level_set()`: Configure log verbosity

Example:
```c
ptk_log_level_set(PTK_LOG_LEVEL_DEBUG);
info("Server started on port %d", port);
debug_buf(packet_data);  // Shows hex dump
```

### Configuration (ptk_config.h)
- `ptk_config_parse()`: Parse command-line arguments
- `ptk_config_print_help()`: Generate help text
- Type-safe argument parsing with defaults

Example:
```c
ptk_config_field_t fields[] = {
    {"port", 'p', PTK_CONFIG_UINT16, &port, "Port number", "12345"},
    PTK_CONFIG_END
};
ptk_config_parse(argc, argv, fields, "myprogram");
```

### Error Handling (ptk_err.h)
- `ptk_get_err()`: Get thread-local error code
- `ptk_set_err()`: Set thread-local error code
- Comprehensive error codes for all operations

Example:
```c
if (result == NULL) {
    ptk_err err = ptk_get_err();
    if (err == PTK_ERR_TIMEOUT) {
        warn("Operation timed out");
    }
}
```

## Building and Running

### Build Steps

1. **Navigate to the example directory:**
   ```bash
   cd src/examples/howto
   ```

2. **Create build directory:**
   ```bash
   mkdir build && cd build
   ```

3. **Configure with CMake:**
   ```bash
   cmake ..
   ```

4. **Build the executables:**
   ```bash
   make
   ```

### Running the Example

1. **Start the server:**
   ```bash
   ./arithmetic_server --port 12345
   ```

2. **Run the client** (in another terminal):
   ```bash
   ./arithmetic_client --server 127.0.0.1 --port 12345 --operation + --operand1 10.5 --operand2 3.2
   ```

3. **Expected output:**
   ```
   Result: 10.500000 + 3.200000 = 13.700000
   ```

### Command-Line Options

**Server:**
- `--port, -p`: Port to listen on (default: 12345)
- `--help, -h`: Show help message

**Client:**
- `--server, -s`: Server IP address (default: 127.0.0.1)
- `--port, -p`: Server port (default: 12345)
- `--operation, -o`: Operation (+, -, *, /) (default: +)
- `--operand1, -1`: First operand (default: 5.0)
- `--operand2, -2`: Second operand (default: 3.0)
- `--help, -h`: Show help message

## Key Learning Points

### 1. Type-Safe Serialization
The PTK buffer system provides compile-time type safety:
```c
// Compiler ensures types match the values
ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, u8_val, f32_val, u64_val);
```

### 2. Automatic Memory Management
All allocations use PTK's system with automatic cleanup:
```c
// Destructor called automatically when reference count reaches 0
arithmetic_request_t *req = ptk_alloc(sizeof(arithmetic_request_t), destructor_func);
```

### 3. Cooperative Multitasking
Threadlets provide the illusion of blocking I/O without blocking OS threads:
```c
// This appears to block but actually yields the threadlet
ptk_buf *data = ptk_tcp_socket_recv(sock, false, 5000);
```

### 4. Safe Shared Memory
The shared memory system prevents use-after-free bugs:
```c
// Handle becomes invalid when memory is freed
use_shared(handle, my_data_t *ptr) {
    // Safe access guaranteed
}
```

### 5. Comprehensive Error Handling
Thread-local error codes provide detailed failure information:
```c
if (operation_failed()) {
    ptk_err err = ptk_get_err();
    error("Operation failed: %s", ptk_err_to_string(err));
}
```

This example demonstrates the power and ease of use of the Protocol Toolkit for building robust network protocols with minimal code and maximum safety.