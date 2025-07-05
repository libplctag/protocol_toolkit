/**
 * @file ev_loop_kqueue.c
 * @brief macOS/BSD kqueue-based event loop implementation
 */

#include "ev_loop_platform.h"
#include "ev_loop_common.h"
#include "log.h"
#include <string.h>
#include <time.h>
#include <assert.h>

#if defined(__APPLE__) || defined(__MACH__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

// Platform-specific includes
#include <sys/event.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

/**
 * macOS/BSD-specific platform data using kqueue
 */
typedef struct {
    int kqueue_fd;               // kqueue file descriptor
    struct kevent *events;       // Event buffer
    size_t max_events;          // Maximum events per iteration
} kqueue_platform_data_t;

//=============================================================================
// WORKER THREAD IMPLEMENTATION
//=============================================================================

/**
 * Worker thread function for kqueue
 */
static void* kqueue_worker_thread(void *arg) {
    common_thread_data_t *thread_data = (common_thread_data_t*)arg;
    ev_loop *loop = thread_data->loop;
    kqueue_platform_data_t *platform = (kqueue_platform_data_t*)loop->platform_data;
    
    trace("kqueue worker thread started");
    
    while (!thread_data->should_stop && loop->running) {
        // Wait for events with timeout
        struct timespec timeout = {.tv_sec = 0, .tv_nsec = 100000000}; // 100ms
        
        int nev = kevent(platform->kqueue_fd, NULL, 0, platform->events, 
                        platform->max_events, &timeout);
        
        if (nev == -1) {
            if (errno == EINTR) continue; // Interrupted by signal
            error("kevent failed: %s", strerror(errno));
            break;
        }
        
        // Process events
        for (int i = 0; i < nev; i++) {
            struct kevent *kev = &platform->events[i];
            ev_sock *sock = (ev_sock*)kev->udata;
            
            if (!sock) continue;
            
            if (kev->flags & EV_ERROR) {
                // Error event
                sock->sock_state = EV_SOCK_STATE_ERROR;
                if (sock->callback) {
                    ev_event event = {
                        .type = EV_EVENT_ERROR,
                        .sock = sock,
                        .data = NULL,
                        .remote_host = sock->remote_host,
                        .remote_port = sock->remote_port,
                        .error = system_error_to_ev_err(kev->data),
                        .sock_state = sock->sock_state,
                        .event_time_ms = ev_now_ms(),
                        .user_data = sock->user_data
                    };
                    sock->callback(&event);
                }
                continue;
            }
            
            if (kev->filter == EVFILT_READ) {
                // Read event
                if (sock->type == EV_SOCK_TCP_SERVER) {
                    // Accept new connection
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_fd = accept(sock->fd, (struct sockaddr*)&client_addr, &addr_len);
                    
                    if (client_fd >= 0) {
                        set_socket_nonblocking(client_fd);
                        
                        if (sock->callback) {
                            ev_event event = {
                                .type = EV_EVENT_ACCEPT,
                                .sock = sock,
                                .data = NULL,
                                .remote_host = inet_ntoa(client_addr.sin_addr),
                                .remote_port = ntohs(client_addr.sin_port),
                                .error = EV_OK,
                                .sock_state = EV_SOCK_STATE_LISTENING,
                                .event_time_ms = ev_now_ms(),
                                .user_data = sock->user_data
                            };
                            sock->callback(&event);
                        }
                    }
                } else {
                    // Read data
                    if (!sock->read_buffer) {
                        buf_alloc(&sock->read_buffer, sock->read_buffer_size);
                    }
                    
                    ssize_t bytes_read;
                    char remote_host[64] = "";
                    int remote_port = 0;
                    
                    if (sock->type == EV_SOCK_UDP) {
                        // UDP socket - use recvfrom to get peer information
                        struct sockaddr_in peer_addr;
                        socklen_t peer_addr_len = sizeof(peer_addr);
                        
                        bytes_read = recvfrom(sock->fd, sock->read_buffer->data, sock->read_buffer->len, 0,
                                             (struct sockaddr*)&peer_addr, &peer_addr_len);
                        
                        if (bytes_read > 0) {
                            // Extract peer address information
                            inet_ntop(AF_INET, &peer_addr.sin_addr, remote_host, sizeof(remote_host));
                            remote_port = ntohs(peer_addr.sin_port);
                        }
                    } else {
                        // TCP socket - use regular read
                        bytes_read = read(sock->fd, sock->read_buffer->data, sock->read_buffer->len);
                        strncpy(remote_host, sock->remote_host, sizeof(remote_host) - 1);
                        remote_port = sock->remote_port;
                    }
                    
                    if (bytes_read > 0) {
                        sock->read_buffer->cursor = bytes_read;
                        
                        if (sock->callback) {
                            ev_event event = {
                                .type = EV_EVENT_READ,
                                .sock = sock,
                                .data = &sock->read_buffer,
                                .remote_host = remote_host,
                                .remote_port = remote_port,
                                .error = EV_OK,
                                .sock_state = sock->sock_state,
                                .event_time_ms = ev_now_ms(),
                                .user_data = sock->user_data
                            };
                            sock->callback(&event);
                            sock->read_buffer = NULL; // Transfer ownership
                        }
                    } else if (bytes_read == 0 && sock->type != EV_SOCK_UDP) {
                        // Connection closed (TCP only, UDP doesn't close)
                        sock->sock_state = EV_SOCK_STATE_CLOSED;
                        if (sock->callback) {
                            ev_event event = {
                                .type = EV_EVENT_CLOSE,
                                .sock = sock,
                                .data = NULL,
                                .remote_host = remote_host,
                                .remote_port = remote_port,
                                .error = EV_OK,
                                .sock_state = sock->sock_state,
                                .event_time_ms = ev_now_ms(),
                                .user_data = sock->user_data
                            };
                            sock->callback(&event);
                        }
                    }
                }
            }
            
            if (kev->filter == EVFILT_WRITE) {
                // Write ready event
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
            }
            
            if (kev->filter == EVFILT_TIMER) {
                // Timer event
                trace("Timer event fired for timer ID %d", sock->fd);
                
                if (sock->callback) {
                    ev_event event = {
                        .type = EV_EVENT_TICK,
                        .sock = sock,
                        .data = NULL,
                        .remote_host = "",
                        .remote_port = 0,
                        .error = EV_OK,
                        .sock_state = sock->sock_state,
                        .event_time_ms = ev_now_ms(),
                        .user_data = sock->user_data
                    };
                    sock->callback(&event);
                } else {
                    // Timer with no callback - just mark as fired
                    trace("Timer with no callback fired");
                }
                
                // If not repeating, mark timer as stopped
                if (!sock->timer_repeat) {
                    sock->sock_state = EV_SOCK_STATE_CLOSED;
                    trace("One-shot timer completed, marked as closed");
                }
            }
        }
    }
    
    trace("kqueue worker thread stopped");
    return NULL;
}

//=============================================================================
// PLATFORM INTERFACE IMPLEMENTATION
//=============================================================================

ev_err platform_init(ev_loop *loop) {
    trace("Initializing kqueue platform data");
    
    // Allocate platform data
    kqueue_platform_data_t *platform = calloc(1, sizeof(kqueue_platform_data_t));
    if (!platform) {
        error("Failed to allocate kqueue platform data");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Create kqueue
    platform->kqueue_fd = kqueue();
    if (platform->kqueue_fd == -1) {
        error("kqueue() failed: %s", strerror(errno));
        free(platform);
        return system_error_to_ev_err(errno);
    }
    
    // Allocate event buffer
    platform->max_events = loop->max_events;
    platform->events = calloc(platform->max_events, sizeof(struct kevent));
    if (!platform->events) {
        error("Failed to allocate event buffer");
        close(platform->kqueue_fd);
        free(platform);
        return EV_ERR_NO_RESOURCES;
    }
    
    loop->platform_data = platform;
    
    info("kqueue platform data initialized with fd %d", platform->kqueue_fd);
    return EV_OK;
}

ev_err platform_start_threads(ev_loop *loop) {
    trace("Starting kqueue worker threads");
    
    if (loop->worker_threads == 0) {
        loop->worker_threads = 1; // Default to 1 thread
    }
    
    // Allocate thread data array
    common_thread_data_t *threads = calloc(loop->worker_threads, sizeof(common_thread_data_t));
    if (!threads) {
        error("Failed to allocate thread data array");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Create threads
    for (size_t i = 0; i < loop->worker_threads; i++) {
        threads[i].loop = loop;
        threads[i].should_stop = false;
        
        ev_err result = create_platform_thread(&threads[i], kqueue_worker_thread);
        if (result != EV_OK) {
            error("Failed to create worker thread %zu", i);
            
            // Stop already created threads
            for (size_t j = 0; j < i; j++) {
                threads[j].should_stop = true;
                join_platform_thread(&threads[j]);
            }
            
            free(threads);
            return result;
        }
    }
    
    loop->threads = threads;
    
    info("Started %zu kqueue worker threads", loop->worker_threads);
    return EV_OK;
}

ev_err platform_stop_threads(ev_loop *loop) {
    trace("Stopping kqueue worker threads");
    
    if (!loop->threads) {
        return EV_OK;
    }
    
    common_thread_data_t *threads = (common_thread_data_t*)loop->threads;
    ev_err result = stop_and_join_threads(threads, loop->worker_threads);
    
    info("Stopped %zu kqueue worker threads", loop->worker_threads);
    return result;
}

void platform_cleanup(ev_loop *loop) {
    trace("Cleaning up kqueue platform data");
    
    if (loop->platform_data) {
        kqueue_platform_data_t *platform = (kqueue_platform_data_t*)loop->platform_data;
        
        if (platform->events) {
            free(platform->events);
        }
        
        if (platform->kqueue_fd >= 0) {
            close(platform->kqueue_fd);
        }
        
        free(platform);
        loop->platform_data = NULL;
    }
    
    if (loop->threads) {
        free(loop->threads);
        loop->threads = NULL;
    }
    
    info("kqueue platform data cleaned up");
}

ev_err platform_add_socket(ev_loop *loop, ev_sock *sock, uint32_t events) {
    kqueue_platform_data_t *platform = (kqueue_platform_data_t*)loop->platform_data;
    struct kevent kev[2];
    int nchanges = 0;
    
    // Define event flags for platform interface
    #define EV_READ  0x01
    #define EV_WRITE 0x02
    
    if (sock->type == EV_SOCK_TIMER) {
        // Handle timer differently - use EVFILT_TIMER
        EV_SET(&kev[nchanges], sock->fd, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, sock->timeout_ms, sock);
        nchanges++;
    } else {
        // Handle regular sockets
        if (events & EV_READ) {
            EV_SET(&kev[nchanges], sock->fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, sock);
            nchanges++;
        }
        
        if (events & EV_WRITE) {
            EV_SET(&kev[nchanges], sock->fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, sock);
            nchanges++;
        }
    }
    
    if (nchanges > 0) {
        if (kevent(platform->kqueue_fd, kev, nchanges, NULL, 0, NULL) == -1) {
            error("kevent add failed: %s", strerror(errno));
            return system_error_to_ev_err(errno);
        }
    }
    
    return EV_OK;
}

ev_err platform_remove_socket(ev_loop *loop, ev_sock *sock) {
    kqueue_platform_data_t *platform = (kqueue_platform_data_t*)loop->platform_data;
    struct kevent kev[2];
    int nchanges = 0;
    
    EV_SET(&kev[nchanges], sock->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    nchanges++;
    EV_SET(&kev[nchanges], sock->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    nchanges++;
    
    if (kevent(platform->kqueue_fd, kev, nchanges, NULL, 0, NULL) == -1) {
        // Ignore errors since the socket might not be registered
        trace("kevent remove failed (ignored): %s", strerror(errno));
    }
    
    return EV_OK;
}

ev_err platform_modify_socket(ev_loop *loop, ev_sock *sock, uint32_t events) {
    // For kqueue, we remove and then add again
    platform_remove_socket(loop, sock);
    return platform_add_socket(loop, sock, events);
}

ev_err platform_wait_threads(ev_loop *loop, uint64_t timeout_ms) {
    if (!loop->threads) {
        return EV_OK;
    }
    
    // If timeout_ms is 0, wait indefinitely
    if (timeout_ms == 0) {
        common_thread_data_t *threads = (common_thread_data_t*)loop->threads;
        
        // Wait for all threads to join
        for (size_t i = 0; i < loop->worker_threads; i++) {
            ev_err result = join_platform_thread(&threads[i]);
            if (result != EV_OK) {
                return result;
            }
        }
        return EV_OK;
    }
    
    // For timeout > 0, we need to wait for the specified duration
    // This is a simplified implementation - just sleep for the timeout
    uint64_t start_time = ev_now_ms();
    uint64_t elapsed = 0;
    
    while (elapsed < timeout_ms && loop->running) {
        // Sleep for a short interval
        struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 10000000}; // 10ms
        nanosleep(&sleep_time, NULL);
        
        elapsed = ev_now_ms() - start_time;
    }
    
    if (elapsed >= timeout_ms) {
        return EV_ERR_TIMEOUT;
    }
    
    return EV_OK;
}

//=============================================================================
// NETWORK DISCOVERY IMPLEMENTATION
//=============================================================================

ev_err platform_find_networks(ev_network_info **network_info, size_t *num_networks) {
    trace("Finding network interfaces on macOS/BSD");
    
    if (!network_info || !num_networks) {
        return EV_ERR_NULL_PTR;
    }
    
    *network_info = NULL;
    *num_networks = 0;
    
    // Use getifaddrs to enumerate network interfaces
    struct ifaddrs *ifaddrs_ptr = NULL;
    if (getifaddrs(&ifaddrs_ptr) != 0) {
        error("getifaddrs() failed: %s", strerror(errno));
        return EV_ERR_NETWORK_ERROR;
    }
    
    // First pass: count IPv4 interfaces that are up and not loopback
    size_t count = 0;
    for (struct ifaddrs *ifa = ifaddrs_ptr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;  // IPv4 only
        if (!(ifa->ifa_flags & IFF_UP)) continue;           // Interface must be up
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;        // Skip loopback
        if (ifa->ifa_netmask == NULL) continue;             // Need netmask for broadcast calculation
        
        count++;
    }
    
    if (count == 0) {
        info("No suitable network interfaces found");
        freeifaddrs(ifaddrs_ptr);
        return EV_OK;
    }
    
    // Allocate array for network info
    ev_network_info *networks = calloc(count, sizeof(ev_network_info));
    if (!networks) {
        error("Failed to allocate network info array");
        freeifaddrs(ifaddrs_ptr);
        return EV_ERR_NO_RESOURCES;
    }
    
    // Second pass: populate network info
    size_t index = 0;
    for (struct ifaddrs *ifa = ifaddrs_ptr; ifa != NULL && index < count; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (ifa->ifa_netmask == NULL) continue;
        
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;
        
        // Allocate and copy IP address
        networks[index].network_ip = malloc(INET_ADDRSTRLEN);
        if (!networks[index].network_ip) {
            error("Failed to allocate IP address string");
            goto cleanup_error;
        }
        inet_ntop(AF_INET, &addr->sin_addr, networks[index].network_ip, INET_ADDRSTRLEN);
        
        // Allocate and copy netmask
        networks[index].netmask = malloc(INET_ADDRSTRLEN);
        if (!networks[index].netmask) {
            error("Failed to allocate netmask string");
            goto cleanup_error;
        }
        inet_ntop(AF_INET, &netmask->sin_addr, networks[index].netmask, INET_ADDRSTRLEN);
        
        // Calculate broadcast address: IP | (~netmask)
        uint32_t ip_addr = ntohl(addr->sin_addr.s_addr);
        uint32_t net_mask = ntohl(netmask->sin_addr.s_addr);
        uint32_t broadcast_addr = ip_addr | (~net_mask);
        
        struct in_addr broadcast_in_addr;
        broadcast_in_addr.s_addr = htonl(broadcast_addr);
        
        networks[index].broadcast = malloc(INET_ADDRSTRLEN);
        if (!networks[index].broadcast) {
            error("Failed to allocate broadcast address string");
            goto cleanup_error;
        }
        inet_ntop(AF_INET, &broadcast_in_addr, networks[index].broadcast, INET_ADDRSTRLEN);
        
        trace("Found network interface: IP=%s, Netmask=%s, Broadcast=%s",
              networks[index].network_ip, networks[index].netmask, networks[index].broadcast);
        
        index++;
    }
    
    freeifaddrs(ifaddrs_ptr);
    
    *network_info = networks;
    *num_networks = count;
    
    info("Successfully found %zu network interfaces", count);
    return EV_OK;
    
cleanup_error:
    // Clean up partially allocated networks
    for (size_t i = 0; i <= index; i++) {
        if (networks[i].network_ip) free(networks[i].network_ip);
        if (networks[i].netmask) free(networks[i].netmask);
        if (networks[i].broadcast) free(networks[i].broadcast);
    }
    free(networks);
    freeifaddrs(ifaddrs_ptr);
    return EV_ERR_NO_RESOURCES;
}

#endif // macOS/BSD