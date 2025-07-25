# PTK Platform Abstraction Layer (PAL)

This document describes the Platform Abstraction Layer (PAL) that enables PTK to work across multiple embedded operating systems and networking stacks.

## Overview

To support multiple embedded OSes, PTK uses a thin Platform Abstraction Layer that maps to different networking models. The PAL provides a unified interface while allowing platform-specific optimizations underneath.

## Core PAL Interface

```c
/* Platform detection - simplified, no capability enumeration */
#if defined(CONFIG_NET_SOCKETS) && defined(CONFIG_POSIX_API)
    /* Zephyr with POSIX API */
    #include "ptk_pal_zephyr.h"
#elif defined(configUSE_FREERTOS_PLUS_TCP)
    /* FreeRTOS+TCP */
    #include "ptk_pal_freertos.h"
#elif defined(__unix__) || defined(__APPLE__)
    /* Standard POSIX */
    #include "ptk_pal_posix.h"
#elif defined(_WIN32)
    /* Windows */
    #include "ptk_pal_windows.h"
#else
    /* Default fallback to POSIX */
    #include "ptk_pal_posix.h"
#endif

/* Universal socket operations */
ptk_status_t ptk_pal_socket_create(ptk_pal_socket_t* sock, int domain, int type, int protocol);
ptk_status_t ptk_pal_socket_connect(ptk_pal_socket_t* sock, const struct sockaddr* addr, socklen_t len);
ptk_status_t ptk_pal_socket_bind(ptk_pal_socket_t* sock, const struct sockaddr* addr, socklen_t len);
ptk_status_t ptk_pal_socket_listen(ptk_pal_socket_t* sock, int backlog);
ptk_status_t ptk_pal_socket_accept(ptk_pal_socket_t* server, ptk_pal_socket_t* client);
ssize_t ptk_pal_socket_send(ptk_pal_socket_t* sock, const void* buf, size_t len, uint32_t timeout_ms);
ssize_t ptk_pal_socket_recv(ptk_pal_socket_t* sock, void* buf, size_t len, uint32_t timeout_ms);
ptk_status_t ptk_pal_socket_close(ptk_pal_socket_t* sock);

/* Universal event waiting - the key abstraction */
int ptk_pal_wait_for_multiple(ptk_pal_socket_t** sockets, 
                             size_t count, 
                             uint32_t timeout_ms,
                             ptk_connection_state_t* states);

/* Thread abstraction - leverages RTOS task/thread features */
typedef struct ptk_pal_thread ptk_pal_thread_t;
typedef void (*ptk_thread_func_t)(void* arg);

ptk_status_t ptk_pal_thread_create(ptk_pal_thread_t* thread, 
                                  ptk_thread_func_t func, 
                                  void* arg,
                                  size_t stack_size,
                                  int priority);
ptk_status_t ptk_pal_thread_join(ptk_pal_thread_t* thread);
void ptk_pal_thread_yield(void);
void ptk_pal_thread_sleep_ms(uint32_t ms);

/* Atomic operations - for lock-free coordination */
typedef volatile uint8_t ptk_atomic8_t;
typedef volatile uint16_t ptk_atomic16_t;
typedef volatile uint32_t ptk_atomic32_t;
typedef volatile uint64_t ptk_atomic64_t;

/* 8-bit atomic operations */
uint8_t ptk_pal_atomic8_load(ptk_atomic8_t* atomic);
void ptk_pal_atomic8_store(ptk_atomic8_t* atomic, uint8_t value);
uint8_t ptk_pal_atomic8_exchange(ptk_atomic8_t* atomic, uint8_t value);
bool ptk_pal_atomic8_compare_exchange(ptk_atomic8_t* atomic, uint8_t expected, uint8_t desired);

/* 16-bit atomic operations */
uint16_t ptk_pal_atomic16_load(ptk_atomic16_t* atomic);
void ptk_pal_atomic16_store(ptk_atomic16_t* atomic, uint16_t value);
uint16_t ptk_pal_atomic16_exchange(ptk_atomic16_t* atomic, uint16_t value);
bool ptk_pal_atomic16_compare_exchange(ptk_atomic16_t* atomic, uint16_t expected, uint16_t desired);

/* 32-bit atomic operations */
uint32_t ptk_pal_atomic32_load(ptk_atomic32_t* atomic);
void ptk_pal_atomic32_store(ptk_atomic32_t* atomic, uint32_t value);
uint32_t ptk_pal_atomic32_exchange(ptk_atomic32_t* atomic, uint32_t value);
bool ptk_pal_atomic32_compare_exchange(ptk_atomic32_t* atomic, uint32_t expected, uint32_t desired);

/* 64-bit atomic operations */
uint64_t ptk_pal_atomic64_load(ptk_atomic64_t* atomic);
void ptk_pal_atomic64_store(ptk_atomic64_t* atomic, uint64_t value);
uint64_t ptk_pal_atomic64_exchange(ptk_atomic64_t* atomic, uint64_t value);
bool ptk_pal_atomic64_compare_exchange(ptk_atomic64_t* atomic, uint64_t expected, uint64_t desired);

/* Note: Thread synchronization uses existing ptk_event_connection_t 
 * No separate semaphore/mutex primitives needed - event connections
 * already provide thread-safe message passing and signaling
 */
```

## Zephyr Implementation

```c
/* ptk_pal_zephyr.c - Zephyr with POSIX sockets */
#ifdef PTK_PLATFORM_ZEPHYR_POSIX

#include <zephyr/net/socket.h>
#include <zephyr/posix/sys/select.h>

typedef struct ptk_pal_socket {
    int fd;
    bool is_valid;
} ptk_pal_socket_t;

ptk_status_t ptk_pal_socket_create(ptk_pal_socket_t* sock, int domain, int type, int protocol) {
    sock->fd = zsock_socket(domain, type, protocol);
    if (sock->fd < 0) {
        return PTK_ERROR_SOCKET_CREATE;
    }
    sock->is_valid = true;
    return PTK_OK;
}

ptk_status_t ptk_pal_socket_connect(ptk_pal_socket_t* sock, const struct sockaddr* addr, socklen_t len) {
    if (zsock_connect(sock->fd, addr, len) < 0) {
        return PTK_ERROR_CONNECT;
    }
    return PTK_OK;
}

ssize_t ptk_pal_socket_send(ptk_pal_socket_t* sock, const void* buf, size_t len, uint32_t timeout_ms) {
    /* Set socket timeout */
    struct zsock_timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };
    zsock_setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    return zsock_send(sock->fd, buf, len, 0);
}

int ptk_pal_wait_for_multiple(ptk_pal_socket_t** sockets, 
                             size_t count, 
                             uint32_t timeout_ms,
                             ptk_connection_state_t* states) {
    zsock_fd_set read_fds, error_fds;
    int max_fd = -1;
    struct zsock_timeval timeout;
    
    ZSOCK_FD_ZERO(&read_fds);
    ZSOCK_FD_ZERO(&error_fds);
    
    /* Build fd_set */
    for (size_t i = 0; i < count; i++) {
        if (sockets[i]->is_valid) {
            ZSOCK_FD_SET(sockets[i]->fd, &read_fds);
            ZSOCK_FD_SET(sockets[i]->fd, &error_fds);
            if (sockets[i]->fd > max_fd) max_fd = sockets[i]->fd;
        }
        states[i] = 0; /* Clear state */
    }
    
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = zsock_select(max_fd + 1, &read_fds, NULL, &error_fds, &timeout);
    
    /* Update states */
    for (size_t i = 0; i < count; i++) {
        if (sockets[i]->is_valid) {
            if (ZSOCK_FD_ISSET(sockets[i]->fd, &read_fds)) {
                states[i] |= PTK_CONN_DATA_READY;
            }
            if (ZSOCK_FD_ISSET(sockets[i]->fd, &error_fds)) {
                states[i] |= PTK_CONN_ERROR;
            }
        }
    }
    
    return result;
}

#endif /* PTK_PLATFORM_ZEPHYR_POSIX */
```

## FreeRTOS+TCP Implementation

```c
/* ptk_pal_freertos.c - FreeRTOS+TCP with task notifications */
#ifdef PTK_PLATFORM_FREERTOS_TCP

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "semphr.h"

typedef struct ptk_pal_socket {
    Socket_t socket;
    TaskHandle_t waiting_task;
    bool is_valid;
} ptk_pal_socket_t;

/* Thread implementation using FreeRTOS tasks */
typedef struct ptk_pal_thread {
    TaskHandle_t handle;
    bool joinable;
} ptk_pal_thread_t;

ptk_status_t ptk_pal_thread_create(ptk_pal_thread_t* thread, 
                                  ptk_thread_func_t func, 
                                  void* arg,
                                  size_t stack_size,
                                  int priority) {
    /* Convert stack size from bytes to words */
    uint16_t stack_words = (stack_size + sizeof(StackType_t) - 1) / sizeof(StackType_t);
    
    /* FreeRTOS priority: higher numbers = higher priority */
    UBaseType_t freertos_priority = (UBaseType_t)priority;
    
    BaseType_t result = xTaskCreate(
        (TaskFunction_t)func,
        "ptk_thread",
        stack_words,
        arg,
        freertos_priority,
        &thread->handle
    );
    
    if (result != pdPASS) {
        return PTK_ERROR_THREAD_CREATE;
    }
    
    thread->joinable = true;
    return PTK_OK;
}

ptk_status_t ptk_pal_thread_join(ptk_pal_thread_t* thread) {
    if (!thread->joinable) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Simple spin-wait until thread completes */
    while (eTaskGetState(thread->handle) != eDeleted) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    thread->joinable = false;
    return PTK_OK;
}

void ptk_pal_thread_yield(void) {
    vTaskDelay(0); /* Yield to other tasks of same or higher priority */
}

void ptk_pal_thread_sleep_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* Atomic operations using FreeRTOS critical sections */

/* 8-bit atomic operations */
uint8_t ptk_pal_atomic8_load(ptk_atomic8_t* atomic) {
    taskENTER_CRITICAL();
    uint8_t value = *atomic;
    taskEXIT_CRITICAL();
    return value;
}

void ptk_pal_atomic8_store(ptk_atomic8_t* atomic, uint8_t value) {
    taskENTER_CRITICAL();
    *atomic = value;
    taskEXIT_CRITICAL();
}

uint8_t ptk_pal_atomic8_exchange(ptk_atomic8_t* atomic, uint8_t value) {
    taskENTER_CRITICAL();
    uint8_t old_value = *atomic;
    *atomic = value;
    taskEXIT_CRITICAL();
    return old_value;
}

bool ptk_pal_atomic8_compare_exchange(ptk_atomic8_t* atomic, uint8_t expected, uint8_t desired) {
    bool success = false;
    taskENTER_CRITICAL();
    if (*atomic == expected) {
        *atomic = desired;
        success = true;
    }
    taskEXIT_CRITICAL();
    return success;
}

/* 16-bit atomic operations */
uint16_t ptk_pal_atomic16_load(ptk_atomic16_t* atomic) {
    taskENTER_CRITICAL();
    uint16_t value = *atomic;
    taskEXIT_CRITICAL();
    return value;
}

void ptk_pal_atomic16_store(ptk_atomic16_t* atomic, uint16_t value) {
    taskENTER_CRITICAL();
    *atomic = value;
    taskEXIT_CRITICAL();
}

uint16_t ptk_pal_atomic16_exchange(ptk_atomic16_t* atomic, uint16_t value) {
    taskENTER_CRITICAL();
    uint16_t old_value = *atomic;
    *atomic = value;
    taskEXIT_CRITICAL();
    return old_value;
}

bool ptk_pal_atomic16_compare_exchange(ptk_atomic16_t* atomic, uint16_t expected, uint16_t desired) {
    bool success = false;
    taskENTER_CRITICAL();
    if (*atomic == expected) {
        *atomic = desired;
        success = true;
    }
    taskEXIT_CRITICAL();
    return success;
}

/* 32-bit atomic operations */
uint32_t ptk_pal_atomic32_load(ptk_atomic32_t* atomic) {
    taskENTER_CRITICAL();
    uint32_t value = *atomic;
    taskEXIT_CRITICAL();
    return value;
}

void ptk_pal_atomic32_store(ptk_atomic32_t* atomic, uint32_t value) {
    taskENTER_CRITICAL();
    *atomic = value;
    taskEXIT_CRITICAL();
}

uint32_t ptk_pal_atomic32_exchange(ptk_atomic32_t* atomic, uint32_t value) {
    taskENTER_CRITICAL();
    uint32_t old_value = *atomic;
    *atomic = value;
    taskEXIT_CRITICAL();
    return old_value;
}

bool ptk_pal_atomic32_compare_exchange(ptk_atomic32_t* atomic, uint32_t expected, uint32_t desired) {
    bool success = false;
    taskENTER_CRITICAL();
    if (*atomic == expected) {
        *atomic = desired;
        success = true;
    }
    taskEXIT_CRITICAL();
    return success;
}

/* 64-bit atomic operations - may not be truly atomic on 32-bit systems */
uint64_t ptk_pal_atomic64_load(ptk_atomic64_t* atomic) {
    taskENTER_CRITICAL();
    uint64_t value = *atomic;
    taskEXIT_CRITICAL();
    return value;
}

void ptk_pal_atomic64_store(ptk_atomic64_t* atomic, uint64_t value) {
    taskENTER_CRITICAL();
    *atomic = value;
    taskEXIT_CRITICAL();
}

uint64_t ptk_pal_atomic64_exchange(ptk_atomic64_t* atomic, uint64_t value) {
    taskENTER_CRITICAL();
    uint64_t old_value = *atomic;
    *atomic = value;
    taskEXIT_CRITICAL();
    return old_value;
}

bool ptk_pal_atomic64_compare_exchange(ptk_atomic64_t* atomic, uint64_t expected, uint64_t desired) {
    bool success = false;
    taskENTER_CRITICAL();
    if (*atomic == expected) {
        *atomic = desired;
        success = true;
    }
    taskEXIT_CRITICAL();
    return success;
}

ptk_status_t ptk_pal_socket_create(ptk_pal_socket_t* sock, int domain, int type, int protocol) {
    BaseType_t freertos_domain = (domain == AF_INET) ? FREERTOS_AF_INET : FREERTOS_AF_INET6;
    BaseType_t freertos_type = (type == SOCK_STREAM) ? FREERTOS_SOCK_STREAM : FREERTOS_SOCK_DGRAM;
    
    sock->socket = FreeRTOS_socket(freertos_domain, freertos_type, 0);
    if (sock->socket == FREERTOS_INVALID_SOCKET) {
        return PTK_ERROR_SOCKET_CREATE;
    }
    
    sock->is_valid = true;
    sock->waiting_task = NULL;
    return PTK_OK;
}

ssize_t ptk_pal_socket_send(ptk_pal_socket_t* sock, const void* buf, size_t len, uint32_t timeout_ms) {
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    /* Set send timeout */
    FreeRTOS_setsockopt(sock->socket, 0, FREERTOS_SO_SNDTIMEO, &timeout_ticks, sizeof(timeout_ticks));
    
    return FreeRTOS_send(sock->socket, buf, len, 0);
}

/* FreeRTOS doesn't have select(), so we implement using task notifications */
int ptk_pal_wait_for_multiple(ptk_pal_socket_t** sockets, 
                             size_t count, 
                             uint32_t timeout_ms,
                             ptk_connection_state_t* states) {
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    int ready_count = 0;
    
    /* Clear states */
    for (size_t i = 0; i < count; i++) {
        states[i] = 0;
    }
    
    /* For FreeRTOS, we need to poll each socket individually */
    /* This is less efficient than select() but works with FreeRTOS+TCP */
    
    TickType_t start_time = xTaskGetTickCount();
    TickType_t remaining_time = timeout_ticks;
    
    while (remaining_time > 0) {
        /* Check each socket for data */
        for (size_t i = 0; i < count; i++) {
            if (!sockets[i]->is_valid) continue;
            
            /* Non-blocking check for data availability */
            TickType_t zero_timeout = 0;
            FreeRTOS_setsockopt(sockets[i]->socket, 0, FREERTOS_SO_RCVTIMEO, &zero_timeout, sizeof(zero_timeout));
            
            /* Try to peek at data without consuming it */
            char peek_buf[1];
            BaseType_t result = FreeRTOS_recv(sockets[i]->socket, peek_buf, sizeof(peek_buf), FREERTOS_MSG_PEEK);
            
            if (result > 0) {
                states[i] |= PTK_CONN_DATA_READY;
                ready_count++;
            } else if (result == -pdFREERTOS_ERRNO_ENOTCONN) {
                states[i] |= PTK_CONN_CLOSED;
                ready_count++;
            } else if (result < 0 && result != -pdFREERTOS_ERRNO_EWOULDBLOCK) {
                states[i] |= PTK_CONN_ERROR;
                ready_count++;
            }
        }
        
        if (ready_count > 0) {
            break;
        }
        
        /* Sleep for a short time to avoid busy waiting */
        vTaskDelay(pdMS_TO_TICKS(10)); /* 10ms polling interval */
        
        TickType_t elapsed = xTaskGetTickCount() - start_time;
        if (elapsed >= timeout_ticks) {
            break;
        }
        remaining_time = timeout_ticks - elapsed;
    }
    
    return ready_count;
}

#endif /* PTK_PLATFORM_FREERTOS_TCP */
```

## lwIP Implementation (for bare metal or other RTOS)

```c
/* ptk_pal_lwip.c - lwIP with either raw API or sockets */
#ifdef PTK_PLATFORM_LWIP

#include "lwip/sockets.h"
#include "lwip/sys.h"

typedef struct ptk_pal_socket {
    int fd;
    bool is_valid;
} ptk_pal_socket_t;

/* Implementation similar to Zephyr but using lwIP functions */
ptk_status_t ptk_pal_socket_create(ptk_pal_socket_t* sock, int domain, int type, int protocol) {
    sock->fd = lwip_socket(domain, type, protocol);
    if (sock->fd < 0) {
        return PTK_ERROR_SOCKET_CREATE;
    }
    sock->is_valid = true;
    return PTK_OK;
}

int ptk_pal_wait_for_multiple(ptk_pal_socket_t** sockets, 
                             size_t count, 
                             uint32_t timeout_ms,
                             ptk_connection_state_t* states) {
    fd_set read_fds, error_fds;
    int max_fd = -1;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    
    for (size_t i = 0; i < count; i++) {
        if (sockets[i]->is_valid) {
            FD_SET(sockets[i]->fd, &read_fds);
            FD_SET(sockets[i]->fd, &error_fds);
            if (sockets[i]->fd > max_fd) max_fd = sockets[i]->fd;
        }
        states[i] = 0;
    }
    
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = lwip_select(max_fd + 1, &read_fds, NULL, &error_fds, &timeout);
    
    for (size_t i = 0; i < count; i++) {
        if (sockets[i]->is_valid) {
            if (FD_ISSET(sockets[i]->fd, &read_fds)) {
                states[i] |= PTK_CONN_DATA_READY;
            }
            if (FD_ISSET(sockets[i]->fd, &error_fds)) {
                states[i] |= PTK_CONN_ERROR;
            }
        }
    }
    
    return result;
}

#endif /* PTK_PLATFORM_LWIP */
```

## Updated Connection Implementation

```c
/* ptk_connection.c - Platform-agnostic connection implementation */

/* Modified TCP connection to use PAL */
typedef struct {
    ptk_pal_socket_t pal_socket;    /* Platform-specific socket */
    struct sockaddr_in addr;
    uint32_t connect_timeout_ms;
    ptk_connection_state_t state;
} ptk_tcp_connection_t;

/* Declare common slices for connection management */
PTK_DECLARE_SLICE_TYPE(tcp_conns, ptk_tcp_connection_t);
PTK_DECLARE_SLICE_TYPE(pal_sockets, ptk_pal_socket_t*);

ptk_status_t ptk_init_tcp_connection(ptk_tcp_connection_t* conn, const char* host, uint16_t port) {
    memset(conn, 0, sizeof(*conn));
    
    /* Create socket through PAL */
    ptk_status_t status = ptk_pal_socket_create(&conn->pal_socket, AF_INET, SOCK_STREAM, 0);
    if (status != PTK_OK) {
        return status;
    }
    
    /* Set up address */
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    
    /* Convert hostname to IP - platform agnostic */
    if (inet_pton(AF_INET, host, &conn->addr.sin_addr) <= 0) {
        /* Try DNS resolution if direct IP conversion fails */
        struct hostent* he = gethostbyname(host);
        if (!he) {
            ptk_pal_socket_close(&conn->pal_socket);
            return PTK_ERROR_DNS_RESOLVE;
        }
        memcpy(&conn->addr.sin_addr, he->h_addr, he->h_length);
    }
    
    /* Connect through PAL */
    status = ptk_pal_socket_connect(&conn->pal_socket, 
                                   (struct sockaddr*)&conn->addr, 
                                   sizeof(conn->addr));
    return status;
}

/* Universal wait function that works on all platforms */
int ptk_wait_for_multiple_tcp(ptk_slice_tcp_conns_t connections, 
                             uint32_t timeout_ms,
                             ptk_scratch_t* scratch) {
    /* Type-safe allocation of PAL socket array */
    ptk_slice_pal_sockets_t pal_sockets = ptk_scratch_alloc_slice(scratch, pal_sockets, connections.len);
    ptk_slice_bytes_t states_buffer = ptk_scratch_alloc_slice(scratch, bytes, connections.len * sizeof(ptk_connection_state_t));
    ptk_connection_state_t* states = (ptk_connection_state_t*)states_buffer.data;
    
    /* Extract PAL sockets */
    for (size_t i = 0; i < connections.len; i++) {
        pal_sockets.data[i] = &connections.data[i].pal_socket;
    }
    
    /* Use PAL wait function */
    int result = ptk_pal_wait_for_multiple(pal_sockets.data, pal_sockets.len, timeout_ms, states);
    
    /* Update connection states */
    for (size_t i = 0; i < connections.len; i++) {
        connections.data[i].state = states[i];
    }
    
    return result;
}
```

## Build System Integration

```c
/* ptk_config.h - Compile-time platform detection and configuration */

#ifndef PTK_CONFIG_H
#define PTK_CONFIG_H

/* Auto-detect platform */
#include "ptk_platform_detect.h"

/* Platform-specific includes */
#ifdef PTK_PLATFORM_ZEPHYR_POSIX
    #include "ptk_pal_zephyr.h"
#elif defined(PTK_PLATFORM_FREERTOS_TCP)
    #include "ptk_pal_freertos.h"
#elif defined(PTK_PLATFORM_LWIP)
    #include "ptk_pal_lwip.h"
#elif defined(PTK_PLATFORM_POSIX)
    #include "ptk_pal_posix.h"
#elif defined(PTK_PLATFORM_WINDOWS)
    #include "ptk_pal_windows.h"
#endif

/* Feature configuration based on platform capabilities */
#if (PTK_PLATFORM_CAPS & PTK_CAP_SELECT)
    #define PTK_HAS_EFFICIENT_MULTIPLEXING 1
#else
    #define PTK_HAS_EFFICIENT_MULTIPLEXING 0
    #define PTK_USE_POLLING_FALLBACK 1
#endif

/* Memory configuration for embedded systems */
#ifdef PTK_EMBEDDED_TARGET
    #define PTK_DEFAULT_SCRATCH_SIZE    2048   /* 2KB */
    #define PTK_MAX_CONNECTIONS        8      /* Conservative for embedded */
    #define PTK_DEFAULT_TIMEOUT_MS     1000   /* 1 second */
    #define PTK_POLLING_INTERVAL_MS    10     /* For platforms without select */
#else
    #define PTK_DEFAULT_SCRATCH_SIZE    65536  /* 64KB */
    #define PTK_MAX_CONNECTIONS        100    /* Higher for servers */
    #define PTK_DEFAULT_TIMEOUT_MS     5000   /* 5 seconds */
#endif

#endif /* PTK_CONFIG_H */
```

## Platform Support Matrix

| Platform | Event Mechanism | API Style | Memory Model | Status |
|----------|----------------|-----------|--------------|--------|
| **Zephyr** | `select()`/`poll()` | POSIX BSD Sockets | Stack + Arena | ✅ Native Support |
| **FreeRTOS+TCP** | Task Notifications | FreeRTOS Sockets | Stack + Arena | ✅ Polling Implementation |
| **lwIP** | `select()` | BSD Sockets | Stack + Arena | ✅ Native Support |
| **ThreadX NetX** | Event Flags | ThreadX API | Stack + Arena | 🔄 Planned |
| **VxWorks** | `select()` | POSIX | Stack + Arena | ✅ Native Support |
| **QNX Neutrino** | `select()`/`poll()` | POSIX | Stack + Arena | ✅ Native Support |
| **Bare Metal** | Polling | Custom | Stack + Arena | ✅ Polling Implementation |

## Implementation Strategy

### Phase 1: Core PAL
1. **Platform detection** - Compile-time platform identification
2. **Socket abstraction** - Unified socket operations
3. **Event multiplexing** - Abstract select()/polling/task notifications
4. **Memory management** - Platform-aware arena allocators

### Phase 2: Protocol Support
1. **EtherNet/IP** - Industrial Ethernet protocol
2. **Modbus TCP** - Popular industrial protocol  
3. **OPC UA** - Modern industrial communication
4. **Custom protocols** - Framework for adding new protocols

### Phase 3: Advanced Features
1. **TLS/SSL support** - Secure communications where available
2. **Multicast** - Industrial protocols often use multicast
3. **Real-time scheduling** - Integration with RTOS schedulers
4. **Diagnostics** - Network health monitoring and debugging

## Key Design Principles for Embedded

### 1. **Zero Hidden Allocations**
```c
/* All data structures are stack-allocated or in scratch arena */
ptk_tcp_connection_t tcp_conn;           /* Stack allocated */
ptk_eip_connection_t eip_conn;           /* Stack allocated */
ptk_scratch_t* scratch = ptk_scratch_create(2048); /* Only explicit allocation */

/* Type-safe slices use same arena - no hidden mallocs */
ptk_slice_u16_t registers = ptk_scratch_alloc_slice(scratch, u16, 100);  /* Explicit */
ptk_slice_tcp_conns_t clients = ptk_scratch_alloc_slice(scratch, tcp_conns, 8);  /* Explicit */
```

### 2. **Predictable Resource Usage**
```c
/* Known limits at compile time */
#define MAX_CLIENTS 8
ptk_tcp_connection_t clients[MAX_CLIENTS];  /* Fixed array, no malloc */

/* Type-safe slices with known limits */
PTK_DECLARE_SLICE_TYPE(clients, ptk_tcp_connection_t);
ptk_slice_clients_t active_clients = ptk_slice_clients_make(clients, MAX_CLIENTS);
/* Compiler knows exact memory usage: MAX_CLIENTS * sizeof(ptk_tcp_connection_t) */
```

### 3. **Graceful Degradation**
```c
/* Works efficiently with select() where available */
#if PTK_HAS_EFFICIENT_MULTIPLEXING
    int ready = ptk_wait_for_multiple_tcp(connections, count, timeout);
#else
    /* Falls back to polling on platforms without select() */
    int ready = ptk_poll_connections(connections, count, timeout);
#endif
```

### 4. **Platform-Specific Optimizations**
```c
/* Use platform strengths where available */
#ifdef PTK_PLATFORM_FREERTOS_TCP
    /* Use FreeRTOS task notifications for efficiency */
    xTaskNotifyWait(0, ULONG_MAX, &notification_value, timeout_ticks);
#elif defined(PTK_PLATFORM_ZEPHYR_POSIX)
    /* Use native select() for best performance */
    zsock_select(max_fd + 1, &read_fds, NULL, &error_fds, &timeout);
#endif
```

### 5. **Resource Monitoring**
```c
/* Built-in resource monitoring for embedded debugging */
void ptk_get_resource_usage(ptk_resource_info_t* info) {
    info->scratch_used = ptk_scratch_used(global_scratch);
    info->scratch_peak = ptk_scratch_peak_usage(global_scratch);
    info->active_connections = get_active_connection_count();
    info->total_packets_sent = get_packet_stats()->sent;
    info->total_packets_received = get_packet_stats()->received;
}
```

## Benefits of the PAL Approach

### **Cross-Platform Compatibility**
- Same application code works across all supported embedded OSes
- Platform differences hidden behind thin PAL layer
- Compile-time platform detection and optimization

### **Embedded-Friendly**
- No hidden memory allocations
- Predictable resource usage
- Graceful degradation on limited platforms
- Configurable limits for different target classes

### **Industrial Protocol Support**
- Native support for common industrial protocols
- Extensible framework for custom protocols
- Real-time considerations built-in

### **Development Efficiency**
- Write once, deploy everywhere
- Consistent debugging and monitoring across platforms
- Easy to test on desktop, deploy on embedded target

### **Performance**
- Zero-copy where possible
- Platform-optimized event handling
- Minimal overhead abstraction layer

This design allows PTK to work efficiently across the entire spectrum of embedded systems, from bare metal microcontrollers to full-featured embedded Linux systems, while maintaining the same clean, safe API.
