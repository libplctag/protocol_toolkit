# How to Use Protocol Toolkit

Protocol Toolkit (PTK) is a comprehensive C library for building robust network protocols and distributed systems. This guide provides an overview of the toolkit's features and demonstrates how to use them effectively.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Core Concepts](#core-concepts)
3. [Memory Management](#memory-management)
4. [Buffer Operations](#buffer-operations)
5. [Networking](#networking)
6. [Green Threads (Threadlets)](#green-threads-threadlets)
7. [Shared Memory](#shared-memory)
8. [Configuration Management](#configuration-management)
9. [Logging](#logging)
10. [Error Handling](#error-handling)
11. [Complete Example](#complete-example)
12. [Best Practices](#best-practices)

## Getting Started

### Library Initialization

Every PTK application must initialize and shutdown the library:

```c
#include <ptk.h>

int main() {
    // Initialize PTK
    ptk_err err = ptk_startup();
    if (err != PTK_OK) {
        fprintf(stderr, "Failed to initialize PTK: %d\n", err);
        return 1;
    }
    
    // Initialize shared memory subsystem (if using shared memory)
    err = ptk_shared_init();
    if (err != PTK_OK) {
        error("Failed to initialize shared memory: %d", err);
        ptk_shutdown();
        return 1;
    }
    
    // Your application code here...
    
    // Cleanup
    ptk_shared_shutdown();
    ptk_shutdown();
    return 0;
}
```

### Basic Project Structure

```
my_protocol/
├── src/
│   ├── protocol_impl.c
│   ├── protocol_impl.h
│   ├── server.c
│   └── client.c
├── CMakeLists.txt
└── README.md
```

## Core Concepts

### Design Philosophy

PTK follows several key principles:

1. **Memory Safety**: All allocations include destructors and automatic cleanup
2. **Type Safety**: Compile-time type checking for serialization operations
3. **Thread Safety**: Safe sharing of data between threads and threadlets
4. **Error Transparency**: Clear error propagation and reporting
5. **Platform Abstraction**: Works across Linux, macOS, Windows, and RTOS

### API Patterns

Most PTK functions follow consistent patterns:

- **Return Values**: Functions return `ptk_err` for status, actual data via output parameters
- **Memory Ownership**: Functions that return pointers transfer ownership to caller
- **Error Context**: Thread-local error codes provide detailed failure information
- **Resource Management**: RAII-style cleanup using destructors

## Memory Management

PTK provides a sophisticated memory management system that prevents common C programming errors.

### Basic Allocation

```c
#include <ptk_alloc.h>

// Simple allocation
void *buffer = ptk_alloc(1024, NULL);
if (!buffer) {
    error("Allocation failed");
    return PTK_ERR_NO_RESOURCES;
}

// Use the buffer...

// Clean up (sets buffer to NULL)
ptk_free(&buffer);
```

### Allocation with Destructors

```c
typedef struct {
    char *name;
    int *data_array;
    size_t array_size;
} my_object_t;

// Destructor function
static void my_object_destructor(void *ptr) {
    my_object_t *obj = (my_object_t*)ptr;
    if (obj) {
        debug("Cleaning up object: %s", obj->name ? obj->name : "unnamed");
        ptk_free(&obj->name);
        ptk_free(&obj->data_array);
    }
}

// Create object with automatic cleanup
my_object_t *create_object(const char *name, size_t array_size) {
    my_object_t *obj = ptk_alloc(sizeof(my_object_t), my_object_destructor);
    if (!obj) return NULL;
    
    obj->name = ptk_alloc(strlen(name) + 1, NULL);
    if (obj->name) {
        strcpy(obj->name, name);
    }
    
    obj->data_array = ptk_alloc(array_size * sizeof(int), NULL);
    obj->array_size = array_size;
    
    return obj;
}

// Usage
my_object_t *obj = create_object("test", 100);
// ... use obj ...
ptk_free(&obj);  // Automatically calls destructor
```

### Memory Reallocation

```c
// Resize existing allocation
void *buffer = ptk_alloc(1024, NULL);
buffer = ptk_realloc(buffer, 2048);  // Preserves existing data
if (!buffer) {
    error("Reallocation failed");
}
```

## Buffer Operations

PTK provides type-safe buffer operations for protocol implementation.

### Buffer Creation and Basic Operations

```c
#include <ptk_buf.h>

// Create a buffer
ptk_buf *buf = ptk_buf_alloc(256);
if (!buf) {
    error("Buffer allocation failed");
    return PTK_ERR_NO_RESOURCES;
}

// Get buffer properties
buf_size_t capacity = ptk_buf_get_capacity(buf);  // 256
buf_size_t length = ptk_buf_get_len(buf);         // 0 initially

// Simple byte operations
ptk_buf_set_u8(buf, 0xAA);  // Write single byte
u8 value = ptk_buf_get_u8(buf);  // Read single byte

// Clean up
ptk_free(&buf);
```

### Type-Safe Serialization

```c
// Define protocol structure
typedef struct {
    u8 command;
    u16 sequence;
    f32 timestamp;
    u64 payload_length;
} packet_header_t;

// Serialize data
ptk_buf *serialize_header(const packet_header_t *header) {
    ptk_buf *buf = ptk_buf_alloc(64);
    if (!buf) return NULL;
    
    // Type-safe serialization with automatic type detection
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN,
                                   header->command,
                                   header->sequence, 
                                   header->timestamp,
                                   header->payload_length);
    
    if (err != PTK_OK) {
        error("Serialization failed: %d", err);
        ptk_free(&buf);
        return NULL;
    }
    
    return buf;
}

// Deserialize data
ptk_err deserialize_header(ptk_buf *buf, packet_header_t *header) {
    // Type-safe deserialization
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN,
                                     &header->command,
                                     &header->sequence,
                                     &header->timestamp, 
                                     &header->payload_length);
    
    if (err != PTK_OK) {
        error("Deserialization failed: %d", err);
        return err;
    }
    
    return PTK_OK;
}
```

### Custom Serializable Objects

```c
typedef struct {
    ptk_serializable_t base;  // Must be first member
    u16 message_type;
    u32 message_id;
    char *payload;
    u16 payload_length;
} my_message_t;

// Serialization method
static ptk_err my_message_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    my_message_t *msg = (my_message_t*)obj;
    
    // Serialize header fields
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                                   msg->message_type,
                                   msg->message_id,
                                   msg->payload_length);
    if (err != PTK_OK) return err;
    
    // Serialize payload if present
    if (msg->payload && msg->payload_length > 0) {
        for (u16 i = 0; i < msg->payload_length; i++) {
            err = ptk_buf_set_u8(buf, (u8)msg->payload[i]);
            if (err != PTK_OK) return err;
        }
    }
    
    return PTK_OK;
}

// Deserialization method  
static ptk_err my_message_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    my_message_t *msg = (my_message_t*)obj;
    
    // Deserialize header
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN,
                                     &msg->message_type,
                                     &msg->message_id,
                                     &msg->payload_length);
    if (err != PTK_OK) return err;
    
    // Allocate and deserialize payload
    if (msg->payload_length > 0) {
        msg->payload = ptk_alloc(msg->payload_length + 1, NULL);  // +1 for null terminator
        if (!msg->payload) return PTK_ERR_NO_RESOURCES;
        
        for (u16 i = 0; i < msg->payload_length; i++) {
            msg->payload[i] = (char)ptk_buf_get_u8(buf);
        }
        msg->payload[msg->payload_length] = '\0';
    }
    
    return PTK_OK;
}

// Object creation
my_message_t *my_message_create(u16 type, u32 id, const char *payload) {
    my_message_t *msg = ptk_alloc(sizeof(my_message_t), my_message_destructor);
    if (!msg) return NULL;
    
    // Set up vtable
    msg->base.serialize = my_message_serialize;
    msg->base.deserialize = my_message_deserialize;
    
    msg->message_type = type;
    msg->message_id = id;
    
    if (payload) {
        msg->payload_length = strlen(payload);
        msg->payload = ptk_alloc(msg->payload_length + 1, NULL);
        if (msg->payload) {
            strcpy(msg->payload, payload);
        }
    } else {
        msg->payload = NULL;
        msg->payload_length = 0;
    }
    
    return msg;
}

// Usage with type-safe serialization
my_message_t *msg = my_message_create(1, 12345, "Hello World");
ptk_buf *buf = ptk_buf_alloc(256);

// Serialize the entire object
ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, (ptk_serializable_t*)msg);
```

## Networking

PTK provides a unified networking API that works across all supported platforms.

### TCP Server

```c
#include <ptk_sock.h>

void run_tcp_server(uint16_t port) {
    // Create server address
    ptk_address_t server_addr;
    ptk_err err = ptk_address_init_any(&server_addr, port);
    if (err != PTK_OK) {
        error("Failed to initialize address: %d", err);
        return;
    }
    
    // Create listening socket
    ptk_sock *server_sock = ptk_tcp_socket_listen(&server_addr, 10);
    if (!server_sock) {
        error("Failed to create server socket: %d", ptk_get_err());
        return;
    }
    
    info("Server listening on port %d", port);
    
    while (true) {
        // Accept client connection
        ptk_sock *client_sock = ptk_tcp_socket_accept(server_sock, 0);  // 0 = infinite timeout
        if (!client_sock) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_ABORT) {
                info("Server shutting down");
                break;
            }
            warn("Accept failed: %d", err);
            continue;
        }
        
        info("Client connected");
        
        // Handle client in separate threadlet (see threadlet section)
        handle_client_connection(client_sock);
    }
    
    ptk_free(&server_sock);
}
```

### TCP Client

```c
void run_tcp_client(const char *server_ip, uint16_t port) {
    // Create server address
    ptk_address_t server_addr;
    ptk_err err = ptk_address_init(&server_addr, server_ip, port);
    if (err != PTK_OK) {
        error("Failed to initialize address: %d", err);
        return;
    }
    
    // Connect to server
    ptk_sock *client_sock = ptk_tcp_socket_connect(&server_addr, 5000);  // 5 second timeout
    if (!client_sock) {
        error("Failed to connect: %d", ptk_get_err());
        return;
    }
    
    info("Connected to server");
    
    // Send data
    ptk_buf *send_buf = ptk_buf_alloc_from_data((u8*)"Hello Server", 12);
    ptk_buf_array_t *send_array = ptk_buf_array_create();
    ptk_buf_array_append(send_array, send_buf);
    
    err = ptk_tcp_socket_send(client_sock, send_array, 5000);
    ptk_free(&send_array);  // This also frees send_buf
    
    if (err != PTK_OK) {
        error("Send failed: %d", err);
        ptk_free(&client_sock);
        return;
    }
    
    // Receive response
    ptk_buf *recv_buf = ptk_tcp_socket_recv(client_sock, false, 5000);
    if (recv_buf) {
        info("Received %d bytes", ptk_buf_get_len(recv_buf));
        debug_buf(recv_buf);  // Log hex dump
        ptk_free(&recv_buf);
    } else {
        warn("Receive failed: %d", ptk_get_err());
    }
    
    ptk_free(&client_sock);
}
```

### UDP Communication

```c
// UDP Server
void run_udp_server(uint16_t port) {
    ptk_address_t bind_addr;
    ptk_address_init_any(&bind_addr, port);
    
    ptk_sock *udp_sock = ptk_udp_socket_create(&bind_addr, false);
    if (!udp_sock) {
        error("Failed to create UDP socket: %d", ptk_get_err());
        return;
    }
    
    while (true) {
        ptk_address_t sender_addr;
        ptk_buf *data = ptk_udp_socket_recv_from(udp_sock, &sender_addr, 0);
        
        if (data) {
            char *sender_ip = ptk_address_to_string(&sender_addr);
            info("Received UDP packet from %s:%d", 
                 sender_ip ? sender_ip : "unknown", 
                 ptk_address_get_port(&sender_addr));
            if (sender_ip) ptk_free(&sender_ip);
            
            // Echo back to sender
            ptk_udp_socket_send_to(udp_sock, data, &sender_addr, false, 1000);
            ptk_free(&data);
        }
    }
    
    ptk_free(&udp_sock);
}

// UDP Client
void run_udp_client(const char *server_ip, uint16_t port) {
    ptk_sock *udp_sock = ptk_udp_socket_create(NULL, false);  // Client socket
    if (!udp_sock) {
        error("Failed to create UDP socket: %d", ptk_get_err());
        return;
    }
    
    ptk_address_t server_addr;
    ptk_address_init(&server_addr, server_ip, port);
    
    ptk_buf *message = ptk_buf_alloc_from_data((u8*)"UDP Hello", 9);
    ptk_err err = ptk_udp_socket_send_to(udp_sock, message, &server_addr, false, 1000);
    ptk_free(&message);
    
    if (err == PTK_OK) {
        ptk_address_t response_addr;
        ptk_buf *response = ptk_udp_socket_recv_from(udp_sock, &response_addr, 5000);
        if (response) {
            info("Received UDP response");
            ptk_free(&response);
        }
    }
    
    ptk_free(&udp_sock);
}
```

## Green Threads (Threadlets)

Threadlets provide cooperative multitasking that scales to thousands of concurrent "threads" per OS thread.

### Basic Threadlet Usage

```c
#include <ptk_threadlet.h>

// Threadlet function
void worker_threadlet(void *param) {
    int worker_id = *(int*)param;
    
    info("Worker %d started", worker_id);
    
    for (int i = 0; i < 10; i++) {
        debug("Worker %d: iteration %d", worker_id, i);
        
        // Cooperative yield (optional - socket operations yield automatically)
        ptk_threadlet_yield();
    }
    
    info("Worker %d completed", worker_id);
}

// Create and run threadlets
void run_workers() {
    const int num_workers = 5;
    threadlet_t *workers[num_workers];
    int worker_ids[num_workers];
    
    // Create worker threadlets
    for (int i = 0; i < num_workers; i++) {
        worker_ids[i] = i + 1;
        workers[i] = ptk_threadlet_create(worker_threadlet, &worker_ids[i]);
        if (!workers[i]) {
            error("Failed to create worker %d", i);
            continue;
        }
        
        // Start the threadlet
        ptk_err err = ptk_threadlet_resume(workers[i]);
        if (err != PTK_OK) {
            error("Failed to start worker %d: %d", i, err);
            ptk_free(&workers[i]);
        }
    }
    
    // Wait for all workers to complete
    for (int i = 0; i < num_workers; i++) {
        if (workers[i]) {
            ptk_threadlet_join(workers[i], 0);  // Wait indefinitely
        }
    }
    
    info("All workers completed");
}
```

### Threadlets with Networking

```c
typedef struct {
    ptk_sock *client_sock;
    int client_id;
} client_context_t;

static void client_context_destructor(void *ptr) {
    client_context_t *ctx = (client_context_t*)ptr;
    if (ctx && ctx->client_sock) {
        debug("Cleaning up client %d", ctx->client_id);
        ptk_free(&ctx->client_sock);
    }
}

void client_handler_threadlet(void *param) {
    client_context_t *ctx = (client_context_t*)param;
    
    info("Handling client %d", ctx->client_id);
    
    while (true) {
        // This appears to block but actually yields the threadlet
        ptk_buf *request = ptk_tcp_socket_recv(ctx->client_sock, false, 30000);  // 30 second timeout
        
        if (!request) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_TIMEOUT) {
                debug("Client %d timeout", ctx->client_id);
                continue;
            } else if (err == PTK_ERR_CLOSED) {
                info("Client %d disconnected", ctx->client_id);
                break;
            } else {
                warn("Error receiving from client %d: %d", ctx->client_id, err);
                break;
            }
        }
        
        // Process request
        debug("Client %d sent %d bytes", ctx->client_id, ptk_buf_get_len(request));
        
        // Echo response
        ptk_buf_array_t *response_array = ptk_buf_array_create();
        ptk_buf_array_append(response_array, request);  // Transfer ownership
        
        ptk_err err = ptk_tcp_socket_send(ctx->client_sock, response_array, 5000);
        ptk_free(&response_array);
        
        if (err != PTK_OK) {
            error("Failed to send to client %d: %d", ctx->client_id, err);
            break;
        }
    }
    
    info("Client %d handler exiting", ctx->client_id);
}

// Multi-client server using threadlets
void multi_client_server(uint16_t port) {
    ptk_address_t server_addr;
    ptk_address_init_any(&server_addr, port);
    
    ptk_sock *server_sock = ptk_tcp_socket_listen(&server_addr, 100);
    if (!server_sock) {
        error("Failed to start server: %d", ptk_get_err());
        return;
    }
    
    info("Multi-client server listening on port %d", port);
    
    int client_counter = 0;
    
    while (true) {
        ptk_sock *client_sock = ptk_tcp_socket_accept(server_sock, 0);
        if (!client_sock) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_ABORT) break;
            warn("Accept failed: %d", err);
            continue;
        }
        
        // Create client context
        client_context_t *ctx = ptk_alloc(sizeof(client_context_t), client_context_destructor);
        if (!ctx) {
            error("Failed to allocate client context");
            ptk_free(&client_sock);
            continue;
        }
        
        ctx->client_sock = client_sock;
        ctx->client_id = ++client_counter;
        
        // Create threadlet for this client
        threadlet_t *client_threadlet = ptk_threadlet_create(client_handler_threadlet, ctx);
        if (!client_threadlet) {
            error("Failed to create client threadlet");
            ptk_free(&ctx);
            continue;
        }
        
        ptk_err err = ptk_threadlet_resume(client_threadlet);
        if (err != PTK_OK) {
            error("Failed to start client threadlet: %d", err);
            ptk_free(&client_threadlet);
            ptk_free(&ctx);
            continue;
        }
        
        info("Created threadlet for client %d", client_counter);
        
        // Note: We don't wait for the client threadlet here - it runs independently
        // The threadlet will clean up when the client disconnects
    }
    
    ptk_free(&server_sock);
}
```

## Shared Memory

PTK's shared memory system provides safe sharing of data between threads and threadlets.

### Basic Shared Memory Usage

```c
#include <ptk_shared.h>

typedef struct {
    int counter;
    char name[64];
    double timestamp;
} shared_data_t;

static void shared_data_destructor(void *ptr) {
    shared_data_t *data = (shared_data_t*)ptr;
    if (data) {
        debug("Destroying shared data: %s", data->name);
    }
}

void shared_memory_example() {
    // Create shared data
    shared_data_t *data = ptk_alloc(sizeof(shared_data_t), shared_data_destructor);
    if (!data) {
        error("Failed to allocate shared data");
        return;
    }
    
    data->counter = 0;
    strcpy(data->name, "test_data");
    data->timestamp = 123.456;
    
    // Wrap in shared memory handle
    ptk_shared_handle_t handle = ptk_shared_wrap(data);
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("Failed to wrap data in shared memory");
        ptk_free(&data);
        return;
    }
    
    // Safe access pattern
    use_shared(handle, shared_data_t *shared_ptr) {
        shared_ptr->counter++;
        info("Counter is now: %d", shared_ptr->counter);
        info("Name: %s", shared_ptr->name);
    } on_shared_fail {
        error("Failed to acquire shared memory");
    }
    
    // Another access
    use_shared(handle, shared_data_t *shared_ptr) {
        shared_ptr->counter += 10;
        shared_ptr->timestamp = 789.012;
        info("Updated counter: %d, timestamp: %f", shared_ptr->counter, shared_ptr->timestamp);
    } on_shared_fail {
        error("Failed to acquire shared memory");
    }
    
    // Release our reference (memory freed when last reference is released)
    ptk_shared_release(handle);
}
```

### Sharing Between Threadlets

```c
typedef struct {
    ptk_shared_handle_t shared_counter_handle;
    int threadlet_id;
} threadlet_param_t;

typedef struct {
    int value;
    int access_count;
} shared_counter_t;

void counter_threadlet(void *param) {
    threadlet_param_t *tp = (threadlet_param_t*)param;
    
    info("Counter threadlet %d started", tp->threadlet_id);
    
    for (int i = 0; i < 100; i++) {
        use_shared(tp->shared_counter_handle, shared_counter_t *counter) {
            counter->value++;
            counter->access_count++;
            
            if (i % 10 == 0) {
                debug("Threadlet %d: counter=%d, accesses=%d", 
                      tp->threadlet_id, counter->value, counter->access_count);
            }
        } on_shared_fail {
            warn("Threadlet %d failed to access shared counter", tp->threadlet_id);
            break;
        }
        
        // Yield to allow other threadlets to run
        ptk_threadlet_yield();
    }
    
    // Release reference
    ptk_shared_release(tp->shared_counter_handle);
    
    info("Counter threadlet %d completed", tp->threadlet_id);
}

void shared_counter_example() {
    // Create shared counter
    shared_counter_t *counter = ptk_alloc(sizeof(shared_counter_t), NULL);
    if (!counter) return;
    
    counter->value = 0;
    counter->access_count = 0;
    
    ptk_shared_handle_t counter_handle = ptk_shared_wrap(counter);
    if (!PTK_SHARED_IS_VALID(counter_handle)) {
        ptk_free(&counter);
        return;
    }
    
    // Create multiple threadlets sharing the same counter
    const int num_threadlets = 5;
    threadlet_t *threadlets[num_threadlets];
    threadlet_param_t params[num_threadlets];
    
    for (int i = 0; i < num_threadlets; i++) {
        params[i].shared_counter_handle = counter_handle;
        params[i].threadlet_id = i + 1;
        
        threadlets[i] = ptk_threadlet_create(counter_threadlet, &params[i]);
        if (threadlets[i]) {
            ptk_threadlet_resume(threadlets[i]);
        }
    }
    
    // Wait for all threadlets to complete
    for (int i = 0; i < num_threadlets; i++) {
        if (threadlets[i]) {
            ptk_threadlet_join(threadlets[i], 0);
        }
    }
    
    // Check final value
    use_shared(counter_handle, shared_counter_t *final_counter) {
        info("Final counter value: %d, total accesses: %d", 
             final_counter->value, final_counter->access_count);
    } on_shared_fail {
        error("Failed to read final counter value");
    }
    
    // Release our reference
    ptk_shared_release(counter_handle);
}
```

## Configuration Management

PTK provides a simple yet powerful configuration system for command-line arguments.

### Basic Configuration

```c
#include <ptk_config.h>

int main(int argc, char *argv[]) {
    // Configuration variables
    char *server_address = NULL;
    uint16_t port = 8080;
    int max_connections = 10;
    bool verbose = false;
    bool help = false;
    
    // Configuration field definitions
    ptk_config_field_t config_fields[] = {
        {"address", 'a', PTK_CONFIG_STRING, &server_address, "Server address to bind to", "0.0.0.0"},
        {"port", 'p', PTK_CONFIG_UINT16, &port, "Port number to listen on", "8080"},
        {"max-conn", 'm', PTK_CONFIG_INT, &max_connections, "Maximum concurrent connections", "10"},
        {"verbose", 'v', PTK_CONFIG_BOOL, &verbose, "Enable verbose logging", NULL},
        {"help", 'h', PTK_CONFIG_HELP, &help, "Show this help message", NULL},
        PTK_CONFIG_END
    };
    
    // Parse command line
    ptk_err err = ptk_config_parse(argc, argv, config_fields, "myserver");
    if (err == 1) {
        // Help was shown
        return 0;
    } else if (err != PTK_OK) {
        // Parse error
        return 1;
    }
    
    // Set log level based on verbose flag
    if (verbose) {
        ptk_log_level_set(PTK_LOG_LEVEL_DEBUG);
    } else {
        ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    }
    
    // Use configuration
    info("Server configuration:");
    info("  Address: %s", server_address ? server_address : "any");
    info("  Port: %d", port);
    info("  Max connections: %d", max_connections);
    
    // Start server with configuration...
    
    // Clean up allocated strings
    if (server_address) {
        ptk_free(&server_address);
    }
    
    return 0;
}
```

## Logging

PTK provides comprehensive logging with multiple levels and binary data support.

### Basic Logging

```c
#include <ptk_log.h>

void logging_examples() {
    // Set log level
    ptk_log_level_set(PTK_LOG_LEVEL_DEBUG);
    
    // Different log levels
    trace("This is a trace message (very detailed)");
    debug("Debug information: value=%d", 42);
    info("Application started successfully");
    warn("This is a warning about %s", "something");
    error("An error occurred: code=%d", -1);
    
    // Log current level
    ptk_log_level current = ptk_log_level_get();
    info("Current log level: %d", current);
}

void binary_logging_example() {
    // Create some binary data
    ptk_buf *data = ptk_buf_alloc(16);
    for (int i = 0; i < 16; i++) {
        ptk_buf_set_u8(data, (u8)i);
    }
    
    // Log binary data as hex dump
    info("Binary data contents:");
    debug_buf(data);  // Shows hex dump with ASCII representation
    
    ptk_free(&data);
}

// Custom log formatting in application code
void protocol_message_log(const char *direction, const void *data, size_t length) {
    info("Protocol %s: %zu bytes", direction, length);
    
    if (ptk_log_level_get() >= PTK_LOG_LEVEL_DEBUG) {
        ptk_buf *temp_buf = ptk_buf_alloc_from_data((const u8*)data, (buf_size_t)length);
        if (temp_buf) {
            debug_buf(temp_buf);
            ptk_free(&temp_buf);
        }
    }
}
```

## Error Handling

PTK uses thread-local error codes for detailed error reporting.

### Basic Error Handling

```c
#include <ptk_err.h>

ptk_err example_function_that_might_fail() {
    // Simulate various failure conditions
    static int call_count = 0;
    call_count++;
    
    if (call_count == 1) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_ERR_NO_RESOURCES;
    } else if (call_count == 2) {
        ptk_set_err(PTK_ERR_TIMEOUT);
        return PTK_ERR_TIMEOUT;
    }
    
    return PTK_OK;
}

void error_handling_example() {
    ptk_err err = example_function_that_might_fail();
    if (err != PTK_OK) {
        // Get more detailed error information
        ptk_err detailed_err = ptk_get_err();
        
        switch (detailed_err) {
            case PTK_ERR_NO_RESOURCES:
                error("Resource exhaustion: %s", ptk_err_to_string(detailed_err));
                // Handle resource limitation
                break;
                
            case PTK_ERR_TIMEOUT:
                warn("Operation timed out: %s", ptk_err_to_string(detailed_err));
                // Handle timeout
                break;
                
            case PTK_ERR_NETWORK_ERROR:
                error("Network error: %s", ptk_err_to_string(detailed_err));
                // Handle network issues
                break;
                
            default:
                error("Unexpected error: %s", ptk_err_to_string(detailed_err));
                break;
        }
    }
}

// Error handling in network operations
void network_error_handling_example() {
    ptk_address_t addr;
    ptk_address_init(&addr, "192.168.1.100", 8080);
    
    ptk_sock *sock = ptk_tcp_socket_connect(&addr, 5000);
    if (!sock) {
        ptk_err err = ptk_get_err();
        
        switch (err) {
            case PTK_ERR_TIMEOUT:
                warn("Connection timed out");
                break;
                
            case PTK_ERR_CONNECTION_REFUSED:
                warn("Connection refused by server");
                break;
                
            case PTK_ERR_HOST_UNREACHABLE:
                error("Host unreachable");
                break;
                
            default:
                error("Connection failed: %s", ptk_err_to_string(err));
                break;
        }
        return;
    }
    
    // Use socket...
    ptk_free(&sock);
}
```

## Complete Example

Here's a complete example that demonstrates most PTK features:

```c
// complete_example.c
#include <ptk.h>
#include <ptk_sock.h>
#include <ptk_buf.h>
#include <ptk_alloc.h>
#include <ptk_shared.h>
#include <ptk_threadlet.h>
#include <ptk_log.h>
#include <ptk_config.h>
#include <ptk_err.h>

// Protocol message structure
typedef struct {
    ptk_serializable_t base;
    u32 message_id;
    u16 message_type;
    char *payload;
    u16 payload_length;
} protocol_message_t;

// Message destructor
static void protocol_message_destructor(void *ptr) {
    protocol_message_t *msg = (protocol_message_t*)ptr;
    if (msg && msg->payload) {
        debug("Destroying message %u", msg->message_id);
        ptk_free(&msg->payload);
    }
}

// Serialization implementation
static ptk_err protocol_message_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    protocol_message_t *msg = (protocol_message_t*)obj;
    
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN,
                                   msg->message_id, msg->message_type, msg->payload_length);
    if (err != PTK_OK) return err;
    
    // Serialize payload
    if (msg->payload && msg->payload_length > 0) {
        for (u16 i = 0; i < msg->payload_length; i++) {
            err = ptk_buf_set_u8(buf, (u8)msg->payload[i]);
            if (err != PTK_OK) return err;
        }
    }
    
    return PTK_OK;
}

// Deserialization implementation
static ptk_err protocol_message_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    protocol_message_t *msg = (protocol_message_t*)obj;
    
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_BIG_ENDIAN,
                                     &msg->message_id, &msg->message_type, &msg->payload_length);
    if (err != PTK_OK) return err;
    
    // Deserialize payload
    if (msg->payload_length > 0) {
        msg->payload = ptk_alloc(msg->payload_length + 1, NULL);
        if (!msg->payload) return PTK_ERR_NO_RESOURCES;
        
        for (u16 i = 0; i < msg->payload_length; i++) {
            msg->payload[i] = (char)ptk_buf_get_u8(buf);
        }
        msg->payload[msg->payload_length] = '\0';
    }
    
    return PTK_OK;
}

// Create protocol message
protocol_message_t *protocol_message_create(u32 id, u16 type, const char *payload) {
    protocol_message_t *msg = ptk_alloc(sizeof(protocol_message_t), protocol_message_destructor);
    if (!msg) return NULL;
    
    msg->base.serialize = protocol_message_serialize;
    msg->base.deserialize = protocol_message_deserialize;
    msg->message_id = id;
    msg->message_type = type;
    
    if (payload) {
        msg->payload_length = strlen(payload);
        msg->payload = ptk_alloc(msg->payload_length + 1, NULL);
        if (msg->payload) {
            strcpy(msg->payload, payload);
        }
    } else {
        msg->payload = NULL;
        msg->payload_length = 0;
    }
    
    return msg;
}

// Shared server state
typedef struct {
    u32 message_counter;
    u32 client_counter;
    bool running;
} server_state_t;

// Client connection context
typedef struct {
    ptk_sock *socket;
    u32 client_id;
    ptk_shared_handle_t server_state_handle;
} client_context_t;

static void client_context_destructor(void *ptr) {
    client_context_t *ctx = (client_context_t*)ptr;
    if (ctx) {
        debug("Cleaning up client %u", ctx->client_id);
        if (ctx->socket) {
            ptk_free(&ctx->socket);
        }
        if (PTK_SHARED_IS_VALID(ctx->server_state_handle)) {
            ptk_shared_release(ctx->server_state_handle);
        }
    }
}

// Client handler threadlet
void client_handler_threadlet(void *param) {
    client_context_t *ctx = (client_context_t*)param;
    
    info("Client %u handler started", ctx->client_id);
    
    while (true) {
        // Receive message
        ptk_buf *recv_buf = ptk_tcp_socket_recv(ctx->socket, false, 30000);
        if (!recv_buf) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_TIMEOUT) {
                debug("Client %u receive timeout", ctx->client_id);
                continue;
            } else if (err == PTK_ERR_CLOSED) {
                info("Client %u disconnected", ctx->client_id);
                break;
            } else {
                warn("Client %u receive error: %d", ctx->client_id, err);
                break;
            }
        }
        
        debug("Client %u sent %d bytes", ctx->client_id, ptk_buf_get_len(recv_buf));
        
        // Deserialize message
        protocol_message_t *msg = protocol_message_create(0, 0, NULL);
        if (!msg) {
            error("Failed to create message object");
            ptk_free(&recv_buf);
            break;
        }
        
        ptk_err err = protocol_message_deserialize(recv_buf, (ptk_serializable_t*)msg);
        ptk_free(&recv_buf);
        
        if (err != PTK_OK) {
            error("Failed to deserialize message: %d", err);
            ptk_free(&msg);
            continue;
        }
        
        info("Client %u message: id=%u, type=%u, payload='%s'",
             ctx->client_id, msg->message_id, msg->message_type,
             msg->payload ? msg->payload : "");
        
        // Update server state
        use_shared(ctx->server_state_handle, server_state_t *state) {
            state->message_counter++;
            info("Total messages processed: %u", state->message_counter);
        } on_shared_fail {
            warn("Failed to update server state");
        }
        
        // Create response
        char response_payload[256];
        snprintf(response_payload, sizeof(response_payload),
                "Echo: %s (from client %u)", 
                msg->payload ? msg->payload : "", ctx->client_id);
        
        protocol_message_t *response = protocol_message_create(
            msg->message_id + 1000, msg->message_type + 100, response_payload);
        
        ptk_free(&msg);
        
        if (!response) {
            error("Failed to create response");
            break;
        }
        
        // Serialize and send response
        ptk_buf *send_buf = ptk_buf_alloc(512);
        if (!send_buf) {
            error("Failed to allocate send buffer");
            ptk_free(&response);
            break;
        }
        
        err = protocol_message_serialize(send_buf, (ptk_serializable_t*)response);
        ptk_free(&response);
        
        if (err != PTK_OK) {
            error("Failed to serialize response: %d", err);
            ptk_free(&send_buf);
            break;
        }
        
        ptk_buf_array_t *send_array = ptk_buf_array_create();
        if (send_array) {
            ptk_buf_array_append(send_array, send_buf);
            err = ptk_tcp_socket_send(ctx->socket, send_array, 5000);
            ptk_free(&send_array);
            
            if (err != PTK_OK) {
                error("Failed to send response to client %u: %d", ctx->client_id, err);
                break;
            }
            
            debug("Sent response to client %u", ctx->client_id);
        } else {
            ptk_free(&send_buf);
            break;
        }
    }
    
    info("Client %u handler exiting", ctx->client_id);
}

// Server threadlet
void server_threadlet(void *param) {
    uint16_t port = *(uint16_t*)param;
    
    info("Server threadlet started on port %u", port);
    
    // Create server state
    server_state_t *state = ptk_alloc(sizeof(server_state_t), NULL);
    if (!state) {
        error("Failed to allocate server state");
        return;
    }
    
    state->message_counter = 0;
    state->client_counter = 0;
    state->running = true;
    
    ptk_shared_handle_t state_handle = ptk_shared_wrap(state);
    if (!PTK_SHARED_IS_VALID(state_handle)) {
        error("Failed to wrap server state");
        ptk_free(&state);
        return;
    }
    
    // Create listening socket
    ptk_address_t server_addr;
    ptk_err err = ptk_address_init_any(&server_addr, port);
    if (err != PTK_OK) {
        error("Failed to initialize server address: %d", err);
        ptk_shared_release(state_handle);
        return;
    }
    
    ptk_sock *server_sock = ptk_tcp_socket_listen(&server_addr, 50);
    if (!server_sock) {
        error("Failed to create server socket: %d", ptk_get_err());
        ptk_shared_release(state_handle);
        return;
    }
    
    info("Server listening for connections");
    
    // Accept connections
    while (true) {
        ptk_sock *client_sock = ptk_tcp_socket_accept(server_sock, 0);
        if (!client_sock) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_ABORT) {
                info("Server shutting down");
                break;
            }
            warn("Accept failed: %d", err);
            continue;
        }
        
        // Update client counter
        u32 client_id;
        use_shared(state_handle, server_state_t *shared_state) {
            shared_state->client_counter++;
            client_id = shared_state->client_counter;
        } on_shared_fail {
            warn("Failed to update client counter");
            client_id = 0;
        }
        
        info("Client %u connected", client_id);
        
        // Create client context
        client_context_t *ctx = ptk_alloc(sizeof(client_context_t), client_context_destructor);
        if (!ctx) {
            error("Failed to allocate client context");
            ptk_free(&client_sock);
            continue;
        }
        
        ctx->socket = client_sock;
        ctx->client_id = client_id;
        ctx->server_state_handle = state_handle;  // Share reference
        
        // Create client handler threadlet
        threadlet_t *client_threadlet = ptk_threadlet_create(client_handler_threadlet, ctx);
        if (!client_threadlet) {
            error("Failed to create client threadlet");
            ptk_free(&ctx);
            continue;
        }
        
        err = ptk_threadlet_resume(client_threadlet);
        if (err != PTK_OK) {
            error("Failed to start client threadlet: %d", err);
            ptk_free(&client_threadlet);
            ptk_free(&ctx);
            continue;
        }
        
        debug("Created threadlet for client %u", client_id);
    }
    
    // Cleanup
    ptk_free(&server_sock);
    ptk_shared_release(state_handle);
    
    info("Server threadlet exiting");
}

// Client function
void run_client(const char *server_ip, uint16_t port) {
    info("Connecting to server %s:%u", server_ip, port);
    
    ptk_address_t server_addr;
    ptk_err err = ptk_address_init(&server_addr, server_ip, port);
    if (err != PTK_OK) {
        error("Failed to initialize server address: %d", err);
        return;
    }
    
    ptk_sock *client_sock = ptk_tcp_socket_connect(&server_addr, 5000);
    if (!client_sock) {
        error("Failed to connect to server: %d", ptk_get_err());
        return;
    }
    
    info("Connected to server");
    
    // Send test messages
    for (int i = 1; i <= 3; i++) {
        char payload[128];
        snprintf(payload, sizeof(payload), "Test message %d from client", i);
        
        protocol_message_t *msg = protocol_message_create(i, 1, payload);
        if (!msg) {
            error("Failed to create message %d", i);
            break;
        }
        
        ptk_buf *send_buf = ptk_buf_alloc(256);
        if (!send_buf) {
            error("Failed to allocate send buffer");
            ptk_free(&msg);
            break;
        }
        
        err = protocol_message_serialize(send_buf, (ptk_serializable_t*)msg);
        ptk_free(&msg);
        
        if (err != PTK_OK) {
            error("Failed to serialize message %d: %d", i, err);
            ptk_free(&send_buf);
            break;
        }
        
        ptk_buf_array_t *send_array = ptk_buf_array_create();
        if (!send_array) {
            error("Failed to create send array");
            ptk_free(&send_buf);
            break;
        }
        
        ptk_buf_array_append(send_array, send_buf);
        
        err = ptk_tcp_socket_send(client_sock, send_array, 5000);
        ptk_free(&send_array);
        
        if (err != PTK_OK) {
            error("Failed to send message %d: %d", i, err);
            break;
        }
        
        info("Sent message %d", i);
        
        // Receive response
        ptk_buf *recv_buf = ptk_tcp_socket_recv(client_sock, false, 5000);
        if (!recv_buf) {
            error("Failed to receive response %d: %d", i, ptk_get_err());
            break;
        }
        
        protocol_message_t *response = protocol_message_create(0, 0, NULL);
        if (!response) {
            error("Failed to create response object");
            ptk_free(&recv_buf);
            break;
        }
        
        err = protocol_message_deserialize(recv_buf, (ptk_serializable_t*)response);
        ptk_free(&recv_buf);
        
        if (err != PTK_OK) {
            error("Failed to deserialize response %d: %d", i, err);
            ptk_free(&response);
            break;
        }
        
        info("Response %d: id=%u, type=%u, payload='%s'",
             i, response->message_id, response->message_type,
             response->payload ? response->payload : "");
        
        ptk_free(&response);
    }
    
    info("Closing connection");
    ptk_free(&client_sock);
}

// Main function
int main(int argc, char *argv[]) {
    // Configuration
    char *mode = NULL;
    char *server_ip = NULL;
    uint16_t port = 8080;
    bool verbose = false;
    bool help = false;
    
    ptk_config_field_t config_fields[] = {
        {"mode", 'm', PTK_CONFIG_STRING, &mode, "Mode: 'server' or 'client'", "server"},
        {"server", 's', PTK_CONFIG_STRING, &server_ip, "Server IP (client mode)", "127.0.0.1"},
        {"port", 'p', PTK_CONFIG_UINT16, &port, "Port number", "8080"},
        {"verbose", 'v', PTK_CONFIG_BOOL, &verbose, "Verbose logging", NULL},
        {"help", 'h', PTK_CONFIG_HELP, &help, "Show help", NULL},
        PTK_CONFIG_END
    };
    
    ptk_err err = ptk_config_parse(argc, argv, config_fields, "ptk_example");
    if (err == 1) {
        return 0;  // Help shown
    } else if (err != PTK_OK) {
        return 1;  // Parse error
    }
    
    // Set up logging
    ptk_log_level_set(verbose ? PTK_LOG_LEVEL_DEBUG : PTK_LOG_LEVEL_INFO);
    
    info("Protocol Toolkit Complete Example");
    
    // Initialize PTK
    err = ptk_startup();
    if (err != PTK_OK) {
        error("Failed to initialize PTK: %d", err);
        return 1;
    }
    
    err = ptk_shared_init();
    if (err != PTK_OK) {
        error("Failed to initialize shared memory: %d", err);
        ptk_shutdown();
        return 1;
    }
    
    // Default mode to server if not specified
    if (!mode) {
        mode = ptk_alloc(8, NULL);
        if (mode) strcpy(mode, "server");
    }
    
    // Default server IP if not specified
    if (!server_ip) {
        server_ip = ptk_alloc(16, NULL);
        if (server_ip) strcpy(server_ip, "127.0.0.1");
    }
    
    // Run in specified mode
    if (mode && strcmp(mode, "server") == 0) {
        info("Running in server mode on port %u", port);
        
        threadlet_t *server = ptk_threadlet_create(server_threadlet, &port);
        if (server) {
            ptk_threadlet_resume(server);
            ptk_threadlet_join(server, 0);  // Wait indefinitely
        } else {
            error("Failed to create server threadlet");
        }
        
    } else if (mode && strcmp(mode, "client") == 0) {
        info("Running in client mode, connecting to %s:%u", server_ip, port);
        run_client(server_ip, port);
        
    } else {
        error("Invalid mode: %s (use 'server' or 'client')", mode ? mode : "null");
    }
    
    // Cleanup
    if (mode) ptk_free(&mode);
    if (server_ip) ptk_free(&server_ip);
    
    info("Shutting down");
    ptk_shared_shutdown();
    ptk_shutdown();
    
    return 0;
}
```

Build this example with:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(ptk_complete_example)

find_library(PTK_LIB ptk REQUIRED)
include_directories(${PTK_INCLUDE_DIR})

add_executable(ptk_complete_example complete_example.c)
target_link_libraries(ptk_complete_example ${PTK_LIB} pthread m)
```

Run the example:

```bash
# Terminal 1 - Server
./ptk_complete_example --mode server --port 8080 --verbose

# Terminal 2 - Client  
./ptk_complete_example --mode client --server 127.0.0.1 --port 8080 --verbose
```

## Best Practices

### 1. Memory Management
- Always use PTK allocation functions (`ptk_alloc`, `ptk_free`, `ptk_realloc`)
- Provide destructors for complex objects
- Use shared memory for data accessed by multiple threadlets
- Check return values and handle allocation failures

### 2. Error Handling
- Always check return values from PTK functions
- Use `ptk_get_err()` for detailed error information
- Log errors appropriately using PTK logging functions
- Provide meaningful error messages

### 3. Networking
- Use timeouts for all blocking operations
- Handle connection failures gracefully
- Use threadlets for concurrent client handling
- Validate received data before processing

### 4. Threading
- Use threadlets instead of OS threads for scalability
- Share data safely using PTK shared memory system
- Avoid blocking operations in threadlets (use PTK APIs instead)
- Handle threadlet lifecycle properly with join operations

### 5. Serialization
- Use type-safe serialization for protocol data
- Implement proper validation in deserialization
- Handle endianness correctly
- Add checksums or CRC for data integrity

### 6. Configuration
- Use PTK configuration system for command-line arguments
- Provide sensible defaults
- Include help text for all options
- Validate configuration values

### 7. Logging
- Set appropriate log levels for different build types
- Use structured logging with meaningful messages
- Include context information in log messages
- Use binary logging for protocol debugging

This comprehensive guide covers the major features of Protocol Toolkit and provides practical examples for building robust network applications.