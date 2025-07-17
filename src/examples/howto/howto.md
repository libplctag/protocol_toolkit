# Protocol Toolkit Arithmetic Example

This example demonstrates how to create a robust client-server protocol using the Protocol Toolkit library. The example implements a multi-threaded arithmetic service where clients can send mathematical operations to a server and receive results.

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
    ptk_u8_t operation;         // Operation code
    ptk_f32_t operand1;         // First operand  
    ptk_f32_t operand2;         // Second operand
    ptk_u16_t crc;              // CRC checksum
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
2. **Creates dedicated threads** for each client connection using `ptk_thread_create()`
3. **Manages client threads** with parent-child relationships and automatic cleanup
4. **Processes arithmetic requests** concurrently across multiple clients
5. **Handles graceful shutdown** via platform-independent interrupt handling

#### Key Server Features

**Multi-Threading Architecture:**
```
Main Thread
├── Sets up ptk_set_interrupt_handler()
├── Creates Server Thread
└── Waits for signals and manages cleanup

Server Thread (Parent)
├── Listens for connections with ptk_tcp_accept()
├── Creates Client Thread for each connection
├── Periodically cleans up dead client threads
└── Responds to abort signals

Client Threads (Children)
├── Handle individual client connections
├── Process arithmetic requests/responses  
└── Respond to abort signals for graceful shutdown
```

**Thread-Based Concurrency:**
```c
// Create dedicated thread for each client
ptk_thread_handle_t client_thread = ptk_thread_create();
ptk_thread_add_handle_arg(client_thread, 1, &conn_handle);
ptk_thread_set_run_function(client_thread, handle_client_connection);
ptk_thread_start(client_thread);
```

**Safe Memory Sharing:**
```c
// Share connection data safely between threads
use_shared(conn_handle, client_connection_t*, conn, PTK_TIME_WAIT_FOREVER) {
    // Safe access to shared connection data
    char *client_ip = ptk_address_to_string(&conn->client_addr);
    info("Handling client connection from %s:%d", client_ip, 
         ptk_address_get_port(&conn->client_addr));
} on_shared_fail {
    error("Failed to acquire shared memory");
}
```

**Platform-Independent Signal Handling:**
```c
// Use PTK's cross-platform interrupt handler
ptk_set_interrupt_handler(interrupt_handler);

static void interrupt_handler(void) {
    info("Received interrupt signal, shutting down...");
    shutdown_requested = true;
    
    // Signal the server thread to abort
    if (ptk_shared_is_valid(g_server_thread)) {
        ptk_thread_signal(g_server_thread, PTK_THREAD_SIGNAL_ABORT);
    }
}
```

**Thread Lifecycle Management:**
```c
// Parent thread manages children
ptk_thread_cleanup_dead_children(ptk_thread_self(), PTK_TIME_NO_WAIT);

// Graceful shutdown of all children
ptk_thread_signal_all_children(self, PTK_THREAD_SIGNAL_ABORT);
ptk_thread_cleanup_dead_children(self, 5000);
```

**Signal-Aware Socket Operations:**
```c
// Threads check for abort signals during I/O
while (!shutdown_requested) {
    if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
        info("Client handler received abort signal");
        break;
    }
    
    ptk_buf *request_buf = ptk_tcp_socket_recv(conn->client_sock, 1000);
    // ... process request
}
```

### 4. Client Implementation (`client.c`)

Threaded TCP client that:

1. **Connects to server** using `ptk_tcp_connect()`
2. **Runs in dedicated thread** for demonstration of threading APIs
3. **Sends arithmetic request** with configurable operation and operands
4. **Receives and displays result**
5. **Handles command-line arguments** using PTK configuration system

#### Key Client Features

**Thread-Based Execution:**
```c
// Client runs in its own thread
ptk_thread_handle_t client_thread = ptk_thread_create();
ptk_thread_add_handle_arg(client_thread, CLIENT_THREAD_PTR_ARG_TYPE, &config_handle);
ptk_thread_set_run_function(client_thread, client_thread_run_func);
ptk_thread_start(client_thread);
```

**Command-Line Configuration:**
```c
ptk_config_field_t config_fields[] = {
    {"server", 's', PTK_CONFIG_STRING, &config->server_ip, "Server IP address", "127.0.0.1"},
    {"port", 'p', PTK_CONFIG_UINT16, &config->server_port, "Server port", "12345"},
    {"help", 'h', PTK_CONFIG_HELP, &help, "Show this help message", NULL},
    PTK_CONFIG_END
};
```

**Robust Error Handling:**
```c
if (!response_buf) {
    ptk_err_t err = ptk_get_err();
    error("Failed to receive response: %d", err);
    return err;
}
```

## Protocol Toolkit APIs Used

### Memory Management (ptk_mem.h)
- `ptk_local_alloc()`: Allocate memory with destructors
- `ptk_local_free()`: Safe deallocation with pointer nulling
- `ptk_local_realloc()`: Resize allocations while preserving data
- `ptk_shared_alloc()`: Create shared memory segments with reference counting

Example:
```c
client_connection_t *conn = ptk_local_alloc(sizeof(client_connection_t), client_connection_destructor);
// ... use conn ...
ptk_local_free(&conn);  // conn is now NULL
```

### Address Management (ptk_sock.h)
- `ptk_address_create()`: Create address from IP string and port
- `ptk_address_create_any()`: Create address for any interface (INADDR_ANY)
- `ptk_address_to_string()`: Convert address to IP string
- `ptk_address_get_port()`: Extract port from address

Example:
```c
ptk_address_t *server_addr = ptk_address_create("192.168.1.100", 12345);
char *ip_str = ptk_address_to_string(server_addr);
uint16_t port = ptk_address_get_port(server_addr);
```

### Networking (ptk_sock.h)
- `ptk_tcp_server_create()`: Create listening server socket
- `ptk_tcp_accept()`: Accept client connections with timeout
- `ptk_tcp_connect()`: Connect to remote server with timeout
- `ptk_tcp_socket_send()`: Send data with timeout
- `ptk_tcp_socket_recv()`: Receive data with timeout

Example:
```c
ptk_sock *server_sock = ptk_tcp_server_create(server_addr);
ptk_sock *client_sock = ptk_tcp_accept(server_sock, &client_addr, 1000);
```

### Threading (ptk_os_thread.h)
- `ptk_thread_create()`: Create thread handle
- `ptk_thread_add_handle_arg()`: Add shared handle argument
- `ptk_thread_set_run_function()`: Set thread entry point
- `ptk_thread_start()`: Start thread execution
- `ptk_thread_signal()`: Send signals to threads
- `ptk_thread_cleanup_dead_children()`: Clean up terminated child threads

Example:
```c
ptk_thread_handle_t thread = ptk_thread_create();
ptk_thread_add_handle_arg(thread, 1, &data_handle);
ptk_thread_set_run_function(thread, worker_function);
ptk_thread_start(thread);
```

### Shared Memory (ptk_mem.h)
- `ptk_shared_alloc()`: Allocate shared memory with reference counting
- `ptk_shared_acquire()`: Get pointer with timeout
- `ptk_shared_release()`: Release reference
- `use_shared()` macro: Safe acquire/release pattern

Example:
```c
ptk_shared_handle_t handle = ptk_shared_alloc(sizeof(my_data_t), destructor);
use_shared(handle, my_data_t*, ptr, PTK_TIME_WAIT_FOREVER) {
    ptr->field = value;  // Safe access
} on_shared_fail {
    error("Acquisition failed");
}
```

### Interrupt Handling (ptk_utils.h)
- `ptk_set_interrupt_handler()`: Platform-independent Ctrl+C handling
- `ptk_thread_signal()`: Signal threads from interrupt handler
- `ptk_thread_has_signal()`: Check for pending signals
- `ptk_thread_clear_signals()`: Clear processed signals

Example:
```c
ptk_set_interrupt_handler(interrupt_handler);

static void interrupt_handler(void) {
    info("Received interrupt, shutting down...");
    ptk_thread_signal(worker_thread, PTK_THREAD_SIGNAL_ABORT);
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
- `ptk_err_set()`: Set thread-local error code
- Comprehensive error codes for all operations

Example:
```c
if (result == NULL) {
    ptk_err_t err = ptk_get_err();
    if (err == PTK_ERR_TIMEOUT) {
        warn("Operation timed out");
    }
}
```

## Building and Running

### Build Steps

1. **Navigate to the project root:**
   ```bash
   cd /path/to/protocol_toolkit
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
   make arithmetic_server arithmetic_client
   ```

### Running the Example

1. **Start the server:**
   ```bash
   ./bin/arithmetic_server --port 12345
   ```
   
   Server output:
   ```
   [2025-07-16 18:34:23.615] [INFO] main:324: Starting Protocol Toolkit arithmetic server
   [2025-07-16 18:34:23.615] [INFO] main:418: Server thread started, waiting for completion
   [2025-07-16 18:34:23.615] [INFO] server_thread_func:220: Server thread started on port 12345
   ```

2. **Run the client** (in another terminal):
   ```bash
   ./bin/arithmetic_client --server 127.0.0.1 --port 12345
   ```

3. **Expected output:**
   ```
   Result: 5.000000 + 3.000000 = 8.000000
   ```

4. **Test multiple concurrent clients:**
   ```bash
   # Terminal 2
   ./bin/arithmetic_client --server 127.0.0.1 --port 12345 &
   
   # Terminal 3  
   ./bin/arithmetic_client --server 127.0.0.1 --port 12345 &
   
   # Terminal 4
   ./bin/arithmetic_client --server 127.0.0.1 --port 12345 &
   ```

5. **Graceful shutdown:**
   Press `Ctrl+C` in the server terminal to trigger graceful shutdown of all threads.

### Command-Line Options

**Server:**
- `--port, -p`: Port to listen on (default: 12345)
- `--help, -h`: Show help message

**Client:**
- `--server, -s`: Server IP address (default: 127.0.0.1)
- `--port, -p`: Server port (default: 12345)
- `--help, -h`: Show help message

Note: The client currently uses hardcoded operation (addition) and operands (5.0 + 3.0). This demonstrates the core protocol functionality.

## Key Learning Points

### 1. Multi-Threaded Server Architecture
The server demonstrates proper thread management:
```c
// Each client gets its own thread
// Parent thread manages all children
// Graceful shutdown coordinates all threads
```

### 2. Platform-Independent Signal Handling
Uses PTK's cross-platform interrupt handling:
```c
// Works on both POSIX and Windows
ptk_set_interrupt_handler(interrupt_handler);
```

### 3. Thread-Safe Shared Memory
Safe data sharing between threads:
```c
// Reference counting prevents use-after-free
// Timeout-based acquisition prevents deadlocks
use_shared(handle, data_t*, ptr, timeout) { /* safe access */ }
```

### 4. Signal-Aware I/O Operations
Socket operations can be interrupted by thread signals:
```c
// Threads check for abort signals during I/O
// Enables responsive shutdown
ptk_tcp_socket_recv(sock, 1000);  // Short timeout for responsiveness
```

### 5. Robust Resource Management
All resources are properly cleaned up:
```c
// Automatic cleanup of dead threads
// Memory freed when reference count reaches 0
// Sockets closed on thread termination
```

### 6. Type-Safe Serialization
The PTK buffer system provides compile-time type safety:
```c
// Compiler ensures types match the values
ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN, u8_val, f32_val, u64_val);
```

This example demonstrates the power of the Protocol Toolkit for building robust, multi-threaded network services with minimal code and maximum safety. The threading model scales to handle many concurrent clients while maintaining clean shutdown semantics and proper resource management.