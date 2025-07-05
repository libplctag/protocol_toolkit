#pragma once

/**
 * @file ev_loop_common.h
 * @brief Common utilities shared across platform implementations
 */

#include "ev_loop.h"
#include "log.h"
#include <string.h>
#include <errno.h>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    typedef SOCKET socket_t;
    typedef HANDLE thread_handle_t;
    typedef DWORD thread_id_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define GET_LAST_ERROR() WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <pthread.h>
    #include <netdb.h>
    typedef int socket_t;
    typedef pthread_t thread_handle_t;
    typedef pthread_t thread_id_t;
    #define INVALID_SOCKET_VALUE -1
    #define GET_LAST_ERROR() errno
#endif

//=============================================================================
// COMMON THREAD DATA STRUCTURE
//=============================================================================

/**
 * Platform-agnostic thread data structure
 */
typedef struct {
    thread_handle_t thread;
    thread_id_t thread_id;
    ev_loop *loop;
    bool should_stop;
    void *platform_specific_data;  // For any platform-specific thread data
} common_thread_data_t;

//=============================================================================
// ERROR HANDLING
//=============================================================================

/**
 * Convert platform-specific error to ev_err
 */
static inline ev_err system_error_to_ev_err(int err) {
#ifdef _WIN32
    switch (err) {
        case ERROR_NOT_ENOUGH_MEMORY: 
        case WSA_NOT_ENOUGH_MEMORY: return EV_ERR_NO_RESOURCES;
        case ERROR_INVALID_PARAMETER:
        case WSAEINVAL: return EV_ERR_INVALID_PARAM;
        case WSAEADDRINUSE: return EV_ERR_ADDRESS_IN_USE;
        case WSAECONNREFUSED: return EV_ERR_CONNECTION_REFUSED;
        case WSAEHOSTUNREACH: return EV_ERR_HOST_UNREACHABLE;
        case WSAEWOULDBLOCK: return EV_ERR_WOULD_BLOCK;
        case WSAETIMEDOUT: return EV_ERR_TIMEOUT;
        case WSAENOTCONN:
        case WSAECONNRESET:
        case WSAECONNABORTED: return EV_ERR_CLOSED;
        default: return EV_ERR_NETWORK_ERROR;
    }
#else
    switch (err) {
        case ENOMEM: return EV_ERR_NO_RESOURCES;
        case EINVAL: return EV_ERR_INVALID_PARAM;
        case EADDRINUSE: return EV_ERR_ADDRESS_IN_USE;
        case ECONNREFUSED: return EV_ERR_CONNECTION_REFUSED;
        case EHOSTUNREACH: return EV_ERR_HOST_UNREACHABLE;
#if EAGAIN != EWOULDBLOCK
        case EAGAIN:
        case EWOULDBLOCK: return EV_ERR_WOULD_BLOCK;
#else
        case EAGAIN: return EV_ERR_WOULD_BLOCK;
#endif
        case ETIMEDOUT: return EV_ERR_TIMEOUT;
        case ENOENT:
        case ECONNRESET:
        case EPIPE: return EV_ERR_CLOSED;
        default: return EV_ERR_NETWORK_ERROR;
    }
#endif
}

//=============================================================================
// SOCKET UTILITIES
//=============================================================================

/**
 * Set socket to non-blocking mode
 */
static inline ev_err set_socket_nonblocking(socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        error("ioctlsocket failed: %d", WSAGetLastError());
        return system_error_to_ev_err(WSAGetLastError());
    }
    return EV_OK;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        error("fcntl F_GETFL failed: %s", strerror(errno));
        return system_error_to_ev_err(errno);
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        error("fcntl F_SETFL failed: %s", strerror(errno));
        return system_error_to_ev_err(errno);
    }
    return EV_OK;
#endif
}

/**
 * Close a socket properly
 */
static inline void close_socket(socket_t sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

/**
 * Create a TCP socket
 */
static inline socket_t create_tcp_socket(void) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VALUE) {
        error("Failed to create TCP socket: %d", GET_LAST_ERROR());
        return INVALID_SOCKET_VALUE;
    }
    return sock;
}

/**
 * Create a UDP socket
 */
static inline socket_t create_udp_socket(void) {
    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET_VALUE) {
        error("Failed to create UDP socket: %d", GET_LAST_ERROR());
        return INVALID_SOCKET_VALUE;
    }
    return sock;
}

/**
 * Set socket reuse address option
 */
static inline ev_err set_socket_reuse_addr(socket_t sock, bool enable) {
#ifdef _WIN32
    BOOL opt = enable ? TRUE : FALSE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0) {
        error("setsockopt SO_REUSEADDR failed: %d", WSAGetLastError());
        return system_error_to_ev_err(WSAGetLastError());
    }
#else
    int opt = enable ? 1 : 0;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        error("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        return system_error_to_ev_err(errno);
    }
#endif
    return EV_OK;
}

/**
 * Bind socket to address
 */
static inline ev_err bind_socket(socket_t sock, const char *host, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (!host || strcmp(host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
#ifdef _WIN32
        if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
#else
        if (inet_aton(host, &addr.sin_addr) == 0) {
#endif
            error("Invalid IP address: %s", host);
            return EV_ERR_INVALID_PARAM;
        }
    }
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        error("bind failed for %s:%d: %d", host ? host : "0.0.0.0", port, GET_LAST_ERROR());
        return system_error_to_ev_err(GET_LAST_ERROR());
    }
    
    return EV_OK;
}

/**
 * Start listening on a socket
 */
static inline ev_err listen_socket(socket_t sock, int backlog) {
    if (listen(sock, backlog) != 0) {
        error("listen failed: %d", GET_LAST_ERROR());
        return system_error_to_ev_err(GET_LAST_ERROR());
    }
    return EV_OK;
}

//=============================================================================
// THREAD UTILITIES
//=============================================================================

/**
 * Create a platform thread
 */
static inline ev_err create_platform_thread(common_thread_data_t *thread_data, 
                                           void* (*thread_func)(void*)) {
#ifdef _WIN32
    thread_data->thread = CreateThread(
        NULL,                               // Security attributes
        0,                                  // Stack size
        (LPTHREAD_START_ROUTINE)thread_func, // Thread function
        thread_data,                        // Parameter
        0,                                  // Creation flags
        &thread_data->thread_id             // Thread ID
    );
    
    if (thread_data->thread == NULL) {
        error("CreateThread failed: %lu", GetLastError());
        return system_error_to_ev_err(GetLastError());
    }
#else
    if (pthread_create(&thread_data->thread, NULL, thread_func, thread_data) != 0) {
        error("pthread_create failed: %s", strerror(errno));
        return system_error_to_ev_err(errno);
    }
    thread_data->thread_id = thread_data->thread;
#endif
    return EV_OK;
}

/**
 * Wait for a platform thread to finish
 */
static inline ev_err join_platform_thread(common_thread_data_t *thread_data) {
#ifdef _WIN32
    if (WaitForSingleObject(thread_data->thread, INFINITE) != WAIT_OBJECT_0) {
        error("WaitForSingleObject failed: %lu", GetLastError());
        return system_error_to_ev_err(GetLastError());
    }
    CloseHandle(thread_data->thread);
#else
    if (pthread_join(thread_data->thread, NULL) != 0) {
        error("pthread_join failed: %s", strerror(errno));
        return system_error_to_ev_err(errno);
    }
#endif
    return EV_OK;
}

/**
 * Signal multiple threads to stop and wait for them
 */
static inline ev_err stop_and_join_threads(common_thread_data_t *threads, size_t count) {
    // Signal all threads to stop
    for (size_t i = 0; i < count; i++) {
        threads[i].should_stop = true;
    }
    
    // Wait for all threads to join
    for (size_t i = 0; i < count; i++) {
        ev_err result = join_platform_thread(&threads[i]);
        if (result != EV_OK) {
            error("Failed to join thread %zu", i);
            return result;
        }
    }
    
    return EV_OK;
}

#ifdef _WIN32
//=============================================================================
// WINDOWS-SPECIFIC UTILITIES (for IOCP)
//=============================================================================

/**
 * Create a Windows thread handle (for IOCP)
 */
static inline HANDLE create_platform_thread_handle(DWORD (WINAPI *start_routine)(LPVOID), 
                                                   void *arg) {
    DWORD thread_id;
    return CreateThread(NULL, 0, start_routine, arg, 0, &thread_id);
}

/**
 * Close a Windows thread handle
 */
static inline void close_platform_thread_handle(HANDLE thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

/**
 * Map Windows error to ev_err (Windows version of system_error_to_ev_err)
 */
static inline ev_err system_error_to_ev_err(DWORD err) {
    switch (err) {
        case ERROR_NOT_ENOUGH_MEMORY: return EV_ERR_NO_RESOURCES;
        case ERROR_INVALID_PARAMETER: return EV_ERR_INVALID_PARAM;
        case WSAEADDRINUSE: return EV_ERR_ADDRESS_IN_USE;
        case WSAECONNREFUSED: return EV_ERR_CONNECTION_REFUSED;
        case WSAEHOSTUNREACH: return EV_ERR_HOST_UNREACHABLE;
        case WSAEWOULDBLOCK: return EV_ERR_WOULD_BLOCK;
        case WSAETIMEDOUT: return EV_ERR_TIMEOUT;
        case WSAENOTCONN: return EV_ERR_CLOSED;
        case WSAECONNRESET: return EV_ERR_CLOSED;
        case WSAECONNABORTED: return EV_ERR_CLOSED;
        default: return EV_ERR_NETWORK_ERROR;
    }
}
#endif /* _WIN32 */