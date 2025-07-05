/**
 * @file ev_loop_iocp.c
 * @brief Windows IOCP-based event loop implementation
 */

#include "ev_loop_platform.h"
#include "ev_loop_common.h"
#include "log.h"
#include <string.h>
#include <time.h>
#include <assert.h>

#if defined(_WIN32)

// Platform-specific includes
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <iphlpapi.h>
#include <stdio.h>

// Link against required libraries
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

/**
 * Windows-specific platform data using IOCP
 */
typedef struct {
    HANDLE completion_port;     // I/O completion port handle
    size_t max_events;          // Maximum events per iteration
} iocp_platform_data_t;

/**
 * Thread data for IOCP
 */
typedef struct {
    HANDLE thread;
    ev_loop *loop;
    bool should_stop;
    DWORD thread_id;
} iocp_thread_data_t;

/**
 * Overlapped structure for async I/O operations
 */
typedef struct {
    OVERLAPPED overlapped;
    ev_sock *sock;
    WSABUF wsabuf;
    DWORD bytes_transferred;
    DWORD flags;
    enum {
        IOCP_OP_READ,
        IOCP_OP_WRITE,
        IOCP_OP_ACCEPT,
        IOCP_OP_CONNECT
    } operation;
} iocp_overlapped_t;

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * Convert Windows error to ev_err
 */
static ev_err winsock_error_to_ev_err(DWORD err) {
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


//=============================================================================
// WORKER THREAD IMPLEMENTATION
//=============================================================================

/**
 * Worker thread function for Windows using IOCP
 */
static DWORD WINAPI iocp_worker_thread(LPVOID lpParam) {
    iocp_thread_data_t *thread_data = (iocp_thread_data_t*)lpParam;
    ev_loop *loop = thread_data->loop;
    iocp_platform_data_t *platform = (iocp_platform_data_t*)loop->platform_data;
    
    trace("IOCP worker thread started");
    
    while (!thread_data->should_stop && loop->running) {
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        OVERLAPPED *overlapped = NULL;
        
        // Wait for I/O completion
        BOOL result = GetQueuedCompletionStatus(
            platform->completion_port,
            &bytes_transferred,
            &completion_key,
            &overlapped,
            100  // 100ms timeout
        );
        
        if (!result) {
            DWORD error = GetLastError();
            if (error == WAIT_TIMEOUT) {
                continue; // Timeout, check should_stop
            }
            if (error == ERROR_ABANDONED_WAIT_0) {
                break; // Completion port closed
            }
            if (overlapped == NULL) {
                error("GetQueuedCompletionStatus failed: %lu", error);
                continue;
            }
            // Handle specific I/O errors below
        }
        
        // Handle shutdown signal
        if (completion_key == 0 && overlapped == NULL) {
            break; // Shutdown signal
        }
        
        if (overlapped) {
            iocp_overlapped_t *iocp_ov = CONTAINING_RECORD(overlapped, iocp_overlapped_t, overlapped);
            ev_sock *sock = iocp_ov->sock;
            
            if (!sock) {
                free(iocp_ov);
                continue;
            }
            
            if (!result) {
                // I/O operation failed
                DWORD error = GetLastError();
                sock->sock_state = EV_SOCK_STATE_ERROR;
                
                if (sock->callback) {
                    ev_event event = {
                        .type = EV_EVENT_ERROR,
                        .sock = sock,
                        .data = NULL,
                        .remote_host = sock->remote_host,
                        .remote_port = sock->remote_port,
                        .error = winsock_error_to_ev_err(error),
                        .sock_state = sock->sock_state,
                        .event_time_ms = ev_now_ms(),
                        .user_data = sock->user_data
                    };
                    sock->callback(&event);
                }
                
                free(iocp_ov);
                continue;
            }
            
            // Process successful I/O completion
            switch (iocp_ov->operation) {
                case IOCP_OP_READ:
                    if (bytes_transferred > 0) {
                        if (sock->read_buffer) {
                            sock->read_buffer->cursor = bytes_transferred;
                            
                            if (sock->callback) {
                                ev_event event = {
                                    .type = EV_EVENT_READ,
                                    .sock = sock,
                                    .data = &sock->read_buffer,
                                    .remote_host = sock->remote_host,
                                    .remote_port = sock->remote_port,
                                    .error = EV_OK,
                                    .sock_state = sock->sock_state,
                                    .event_time_ms = ev_now_ms(),
                                    .user_data = sock->user_data
                                };
                                sock->callback(&event);
                                sock->read_buffer = NULL; // Transfer ownership
                            }
                        }
                    } else {
                        // Connection closed
                        sock->sock_state = EV_SOCK_STATE_CLOSED;
                        if (sock->callback) {
                            ev_event event = {
                                .type = EV_EVENT_CLOSE,
                                .sock = sock,
                                .data = NULL,
                                .remote_host = sock->remote_host,
                                .remote_port = sock->remote_port,
                                .error = EV_OK,
                                .sock_state = sock->sock_state,
                                .event_time_ms = ev_now_ms(),
                                .user_data = sock->user_data
                            };
                            sock->callback(&event);
                        }
                    }
                    break;
                    
                case IOCP_OP_WRITE:
                    if (sock->callback) {
                        ev_event event = {
                            .type = EV_EVENT_WRITE_DONE,
                            .sock = sock,
                            .data = NULL,
                            .remote_host = sock->remote_host,
                            .remote_port = sock->remote_port,
                            .error = EV_OK,
                            .sock_state = sock->sock_state,
                            .event_time_ms = ev_now_ms(),
                            .user_data = sock->user_data
                        };
                        sock->callback(&event);
                    }
                    break;
                    
                case IOCP_OP_ACCEPT:
                    // Handle accept completion (more complex, simplified here)
                    if (sock->callback) {
                        ev_event event = {
                            .type = EV_EVENT_ACCEPT,
                            .sock = sock,
                            .data = NULL,
                            .remote_host = "",  // Would need to extract from accept buffer
                            .remote_port = 0,   // Would need to extract from accept buffer
                            .error = EV_OK,
                            .sock_state = EV_SOCK_STATE_LISTENING,
                            .event_time_ms = ev_now_ms(),
                            .user_data = sock->user_data
                        };
                        sock->callback(&event);
                    }
                    break;
                    
                case IOCP_OP_CONNECT:
                    sock->sock_state = EV_SOCK_STATE_CONNECTED;
                    if (sock->callback) {
                        ev_event event = {
                            .type = EV_EVENT_CONNECT,
                            .sock = sock,
                            .data = NULL,
                            .remote_host = sock->remote_host,
                            .remote_port = sock->remote_port,
                            .error = EV_OK,
                            .sock_state = sock->sock_state,
                            .event_time_ms = ev_now_ms(),
                            .user_data = sock->user_data
                        };
                        sock->callback(&event);
                    }
                    break;
            }
            
            free(iocp_ov);
        }
    }
    
    trace("IOCP worker thread stopped");
    return 0;
}

//=============================================================================
// PLATFORM INTERFACE IMPLEMENTATION
//=============================================================================

ev_err platform_init(ev_loop *loop) {
    trace("Initializing IOCP platform data");
    
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        error("WSAStartup failed: %d", result);
        return EV_ERR_NETWORK_ERROR;
    }
    
    // Allocate platform data
    iocp_platform_data_t *platform = calloc(1, sizeof(iocp_platform_data_t));
    if (!platform) {
        error("Failed to allocate IOCP platform data");
        WSACleanup();
        return EV_ERR_NO_RESOURCES;
    }
    
    // Create I/O completion port
    platform->completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (platform->completion_port == NULL) {
        error("CreateIoCompletionPort failed: %lu", GetLastError());
        free(platform);
        WSACleanup();
        return system_error_to_ev_err(GetLastError());
    }
    
    platform->max_events = loop->max_events;
    loop->platform_data = platform;
    
    info("IOCP platform data initialized");
    return EV_OK;
}

ev_err platform_start_threads(ev_loop *loop) {
    trace("Starting IOCP worker threads");
    
    if (loop->worker_threads == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        loop->worker_threads = si.dwNumberOfProcessors;
    }
    
    // Allocate thread data array
    iocp_thread_data_t *threads = calloc(loop->worker_threads, sizeof(iocp_thread_data_t));
    if (!threads) {
        error("Failed to allocate thread data array");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Create threads
    for (size_t i = 0; i < loop->worker_threads; i++) {
        threads[i].loop = loop;
        threads[i].should_stop = false;
        
        threads[i].thread = create_platform_thread_handle(
            iocp_worker_thread,      // Thread function
            &threads[i]             // Parameter
        );
        
        if (threads[i].thread == NULL) {
            error("Failed to create worker thread %zu: %lu", i, GetLastError());
            
            // Stop already created threads
            for (size_t j = 0; j < i; j++) {
                threads[j].should_stop = true;
                close_platform_thread_handle(threads[j].thread);
            }
            
            free(threads);
            return EV_ERR_NO_RESOURCES;
        }
    }
    
    loop->threads = threads;
    
    info("Started %zu IOCP worker threads", loop->worker_threads);
    return EV_OK;
}

ev_err platform_stop_threads(ev_loop *loop) {
    trace("Stopping IOCP worker threads");
    
    if (!loop->threads) {
        return EV_OK;
    }
    
    iocp_thread_data_t *threads = (iocp_thread_data_t*)loop->threads;
    
    // Signal all threads to stop
    for (size_t i = 0; i < loop->worker_threads; i++) {
        threads[i].should_stop = true;
    }
    
    // Post completion packets to wake up threads
    iocp_platform_data_t *platform = (iocp_platform_data_t*)loop->platform_data;
    for (size_t i = 0; i < loop->worker_threads; i++) {
        PostQueuedCompletionStatus(platform->completion_port, 0, 0, NULL);
    }
    
    // Wait for all threads to finish
    HANDLE *handles = malloc(loop->worker_threads * sizeof(HANDLE));
    for (size_t i = 0; i < loop->worker_threads; i++) {
        handles[i] = threads[i].thread;
    }
    
    WaitForMultipleObjects(loop->worker_threads, handles, TRUE, INFINITE);
    
    // Close thread handles
    for (size_t i = 0; i < loop->worker_threads; i++) {
        close_platform_thread_handle(threads[i].thread);
    }
    
    free(handles);
    
    info("Stopped %zu IOCP worker threads", loop->worker_threads);
    return EV_OK;
}

void platform_cleanup(ev_loop *loop) {
    trace("Cleaning up IOCP platform data");
    
    if (loop->platform_data) {
        iocp_platform_data_t *platform = (iocp_platform_data_t*)loop->platform_data;
        
        if (platform->completion_port != NULL) {
            CloseHandle(platform->completion_port);
        }
        
        free(platform);
        loop->platform_data = NULL;
    }
    
    if (loop->threads) {
        free(loop->threads);
        loop->threads = NULL;
    }
    
    // Cleanup Winsock
    WSACleanup();
    
    info("IOCP platform data cleaned up");
}

ev_err platform_add_socket(ev_loop *loop, ev_sock *sock, uint32_t events) {
    iocp_platform_data_t *platform = (iocp_platform_data_t*)loop->platform_data;
    
    // Define event flags for platform interface
    #define EV_READ  0x01
    #define EV_WRITE 0x02
    
    // Associate socket with completion port
    HANDLE result = CreateIoCompletionPort((HANDLE)sock->handle, platform->completion_port, 
                                         (ULONG_PTR)sock, 0);
    if (result == NULL) {
        error("CreateIoCompletionPort failed for socket: %lu", GetLastError());
        return system_error_to_ev_err(GetLastError());
    }
    
    // For IOCP, we don't pre-register events like epoll/kqueue
    // Instead, we initiate async operations when needed
    return EV_OK;
}

ev_err platform_remove_socket(ev_loop *loop, ev_sock *sock) {
    // For IOCP, sockets are automatically removed when closed
    // No explicit removal needed
    return EV_OK;
}

ev_err platform_modify_socket(ev_loop *loop, ev_sock *sock, uint32_t events) {
    // For IOCP, event modification is done by initiating different async operations
    // No explicit modification of registered events
    return EV_OK;
}

ev_err platform_wait_threads(ev_loop *loop, uint64_t timeout_ms) {
    if (!loop->threads) {
        return EV_OK;
    }
    
    iocp_thread_data_t *threads = (iocp_thread_data_t*)loop->threads;
    
    // Wait for all threads to finish
    HANDLE *handles = malloc(loop->worker_threads * sizeof(HANDLE));
    for (size_t i = 0; i < loop->worker_threads; i++) {
        handles[i] = threads[i].thread;
    }
    
    DWORD wait_timeout = timeout_ms == 0 ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForMultipleObjects(loop->worker_threads, handles, TRUE, wait_timeout);
    
    free(handles);
    
    if (result == WAIT_TIMEOUT) {
        return EV_ERR_TIMEOUT;
    } else if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + loop->worker_threads) {
        return EV_OK;
    } else {
        error("WaitForMultipleObjects failed: %lu", GetLastError());
        return EV_ERR_NETWORK_ERROR;
    }
}

//=============================================================================
// NETWORK DISCOVERY IMPLEMENTATION
//=============================================================================

ev_err platform_find_networks(ev_network_info **network_info, size_t *num_networks) {
    trace("Finding network interfaces on Windows");
    
    if (!network_info || !num_networks) {
        return EV_ERR_NULL_PTR;
    }
    
    *network_info = NULL;
    *num_networks = 0;
    
    // Get adapter information using GetAdaptersInfo
    PIP_ADAPTER_INFO adapter_info = NULL;
    ULONG buffer_size = 0;
    
    // First call to get required buffer size
    DWORD result = GetAdaptersInfo(adapter_info, &buffer_size);
    if (result != ERROR_BUFFER_OVERFLOW) {
        error("GetAdaptersInfo failed to get buffer size: %lu", result);
        return EV_ERR_NETWORK_ERROR;
    }
    
    // Allocate buffer
    adapter_info = (PIP_ADAPTER_INFO)malloc(buffer_size);
    if (!adapter_info) {
        error("Failed to allocate adapter info buffer");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Second call to get actual data
    result = GetAdaptersInfo(adapter_info, &buffer_size);
    if (result != NO_ERROR) {
        error("GetAdaptersInfo failed: %lu", result);
        free(adapter_info);
        return EV_ERR_NETWORK_ERROR;
    }
    
    // First pass: count suitable interfaces
    size_t count = 0;
    PIP_ADAPTER_INFO current_adapter = adapter_info;
    while (current_adapter) {
        // Skip non-Ethernet adapters and down interfaces
        if (current_adapter->Type == MIB_IF_TYPE_ETHERNET ||
            current_adapter->Type == MIB_IF_TYPE_IEEE80211) {  // Include WiFi
            
            PIP_ADDR_STRING addr_string = &current_adapter->IpAddressList;
            while (addr_string) {
                // Skip 0.0.0.0 addresses (not configured)
                if (strcmp(addr_string->IpAddress.String, "0.0.0.0") != 0) {
                    count++;
                }
                addr_string = addr_string->Next;
            }
        }
        current_adapter = current_adapter->Next;
    }
    
    if (count == 0) {
        info("No suitable network interfaces found on Windows");
        free(adapter_info);
        return EV_OK;
    }
    
    // Allocate array for network info
    ev_network_info *networks = calloc(count, sizeof(ev_network_info));
    if (!networks) {
        error("Failed to allocate network info array");
        free(adapter_info);
        return EV_ERR_NO_RESOURCES;
    }
    
    // Second pass: populate network info
    size_t index = 0;
    current_adapter = adapter_info;
    while (current_adapter && index < count) {
        if (current_adapter->Type == MIB_IF_TYPE_ETHERNET ||
            current_adapter->Type == MIB_IF_TYPE_IEEE80211) {
            
            PIP_ADDR_STRING addr_string = &current_adapter->IpAddressList;
            while (addr_string && index < count) {
                if (strcmp(addr_string->IpAddress.String, "0.0.0.0") != 0) {
                    // Allocate and copy IP address
                    networks[index].network_ip = malloc(strlen(addr_string->IpAddress.String) + 1);
                    if (!networks[index].network_ip) {
                        error("Failed to allocate IP address string");
                        goto cleanup_error;
                    }
                    strcpy(networks[index].network_ip, addr_string->IpAddress.String);
                    
                    // Allocate and copy netmask
                    networks[index].netmask = malloc(strlen(addr_string->IpMask.String) + 1);
                    if (!networks[index].netmask) {
                        error("Failed to allocate netmask string");
                        goto cleanup_error;
                    }
                    strcpy(networks[index].netmask, addr_string->IpMask.String);
                    
                    // Calculate broadcast address
                    struct in_addr ip_addr, net_mask, broadcast_addr;
                    if (inet_pton(AF_INET, addr_string->IpAddress.String, &ip_addr) != 1 ||
                        inet_pton(AF_INET, addr_string->IpMask.String, &net_mask) != 1) {
                        error("Failed to parse IP address or netmask");
                        goto cleanup_error;
                    }
                    
                    uint32_t ip = ntohl(ip_addr.s_addr);
                    uint32_t mask = ntohl(net_mask.s_addr);
                    uint32_t broadcast = ip | (~mask);
                    
                    broadcast_addr.s_addr = htonl(broadcast);
                    
                    networks[index].broadcast = malloc(INET_ADDRSTRLEN);
                    if (!networks[index].broadcast) {
                        error("Failed to allocate broadcast address string");
                        goto cleanup_error;
                    }
                    
                    if (inet_ntop(AF_INET, &broadcast_addr, networks[index].broadcast, INET_ADDRSTRLEN) == NULL) {
                        error("Failed to convert broadcast address to string");
                        goto cleanup_error;
                    }
                    
                    trace("Found network interface: IP=%s, Netmask=%s, Broadcast=%s",
                          networks[index].network_ip, networks[index].netmask, networks[index].broadcast);
                    
                    index++;
                }
                addr_string = addr_string->Next;
            }
        }
        current_adapter = current_adapter->Next;
    }
    
    free(adapter_info);
    
    *network_info = networks;
    *num_networks = index;  // Use actual count in case we found fewer than expected
    
    info("Successfully found %zu network interfaces on Windows", index);
    return EV_OK;
    
cleanup_error:
    // Clean up partially allocated networks
    for (size_t i = 0; i <= index; i++) {
        if (networks[i].network_ip) free(networks[i].network_ip);
        if (networks[i].netmask) free(networks[i].netmask);
        if (networks[i].broadcast) free(networks[i].broadcast);
    }
    free(networks);
    free(adapter_info);
    return EV_ERR_NO_RESOURCES;
}

#endif // Windows