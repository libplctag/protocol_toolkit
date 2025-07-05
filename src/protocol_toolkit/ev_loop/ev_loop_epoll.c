/**
 * @file ev_loop_epoll.c
 * @brief Linux epoll-based event loop implementation
 */

#include "ev_loop_platform.h"
#include "ev_loop_common.h"
#include "log.h"
#include <string.h>
#include <time.h>
#include <assert.h>

#if defined(__linux__)

// Platform-specific includes
#include <sys/epoll.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

/**
 * Linux-specific platform data using epoll
 */
typedef struct {
    int epoll_fd;               // epoll file descriptor
    struct epoll_event *events; // Event buffer
    size_t max_events;          // Maximum events per iteration
} epoll_platform_data_t;

//=============================================================================
// WORKER THREAD IMPLEMENTATION
//=============================================================================

/**
 * Worker thread function for Linux using epoll
 */
static void* epoll_worker_thread(void *arg) {
    epoll_thread_data_t *thread_data = (epoll_thread_data_t*)arg;
    ev_loop *loop = thread_data->loop;
    epoll_platform_data_t *platform = (epoll_platform_data_t*)loop->platform_data;
    
    trace("epoll worker thread started");
    
    while (!thread_data->should_stop && loop->running) {
        // Wait for events with timeout
        int nev = epoll_wait(platform->epoll_fd, platform->events, 
                           platform->max_events, 100); // 100ms timeout
        
        if (nev == -1) {
            if (errno == EINTR) continue; // Interrupted by signal
            error("epoll_wait failed: %s", strerror(errno));
            break;
        }
        
        // Process events
        for (int i = 0; i < nev; i++) {
            struct epoll_event *epev = &platform->events[i];
            ev_sock *sock = (ev_sock*)epev->data.ptr;
            
            if (!sock) continue;
            
            // Update socket state
            if (epev->events & EPOLLERR) {
                sock->sock_state = EV_SOCK_STATE_ERROR;
            } else if (epev->events & EPOLLHUP) {
                sock->sock_state = EV_SOCK_STATE_CLOSED;
            }
            
            if (epev->events & (EPOLLERR | EPOLLHUP)) {
                // Error or hangup event
                if (sock->callback) {
                    ev_event event = {
                        .type = EV_EVENT_ERROR,
                        .sock = sock,
                        .data = NULL,
                        .remote_host = sock->remote_host,
                        .remote_port = sock->remote_port,
                        .error = EV_ERR_CLOSED,
                        .sock_state = sock->sock_state,
                        .event_time_ms = ev_now_ms(),
                        .user_data = sock->user_data
                    };
                    sock->callback(&event);
                }
                continue;
            }
            
            if (epev->events & EPOLLIN) {
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
                    
                    ssize_t bytes_read = read(sock->fd, sock->read_buffer->data, sock->read_buffer->len);
                    if (bytes_read > 0) {
                        sock->read_buffer->cursor = bytes_read;
                        
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
                    } else if (bytes_read == 0) {
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
                }
            }
            
            if (epev->events & EPOLLOUT) {
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
        }
    }
    
    trace("epoll worker thread stopped");
    return NULL;
}

//=============================================================================
// PLATFORM INTERFACE IMPLEMENTATION
//=============================================================================

ev_err platform_init(ev_loop *loop) {
    trace("Initializing epoll platform data");
    
    // Allocate platform data
    epoll_platform_data_t *platform = calloc(1, sizeof(epoll_platform_data_t));
    if (!platform) {
        error("Failed to allocate epoll platform data");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Create epoll instance
    platform->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (platform->epoll_fd == -1) {
        error("epoll_create1 failed: %s", strerror(errno));
        free(platform);
        return system_error_to_ev_err(errno);
    }
    
    // Allocate event buffer
    platform->max_events = loop->max_events;
    platform->events = calloc(platform->max_events, sizeof(struct epoll_event));
    if (!platform->events) {
        error("Failed to allocate event buffer");
        close(platform->epoll_fd);
        free(platform);
        return EV_ERR_NO_RESOURCES;
    }
    
    loop->platform_data = platform;
    
    info("epoll platform data initialized with fd %d", platform->epoll_fd);
    return EV_OK;
}

ev_err platform_start_threads(ev_loop *loop) {
    trace("Starting epoll worker threads");
    
    if (loop->worker_threads == 0) {
        loop->worker_threads = 1; // Default to 1 thread
    }
    
    // Allocate thread data array
    epoll_thread_data_t *threads = calloc(loop->worker_threads, sizeof(epoll_thread_data_t));
    if (!threads) {
        error("Failed to allocate thread data array");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Create threads
    for (size_t i = 0; i < loop->worker_threads; i++) {
        threads[i].loop = loop;
        threads[i].should_stop = false;
        
        if (create_platform_thread(&threads[i].thread, epoll_worker_thread, &threads[i]) != 0) {
            error("Failed to create worker thread %zu: %s", i, strerror(errno));
            
            // Stop already created threads
            for (size_t j = 0; j < i; j++) {
                threads[j].should_stop = true;
                join_platform_thread(threads[j].thread);
            }
            
            free(threads);
            return EV_ERR_NO_RESOURCES;
        }
    }
    
    loop->threads = threads;
    
    info("Started %zu epoll worker threads", loop->worker_threads);
    return EV_OK;
}

ev_err platform_stop_threads(ev_loop *loop) {
    trace("Stopping epoll worker threads");
    
    if (!loop->threads) {
        return EV_OK;
    }
    
    epoll_thread_data_t *threads = (epoll_thread_data_t*)loop->threads;
    
    // Signal all threads to stop
    for (size_t i = 0; i < loop->worker_threads; i++) {
        threads[i].should_stop = true;
    }
    
    // Wait for all threads to join
    for (size_t i = 0; i < loop->worker_threads; i++) {
        join_platform_thread(threads[i].thread);
    }
    
    info("Stopped %zu epoll worker threads", loop->worker_threads);
    return EV_OK;
}

void platform_cleanup(ev_loop *loop) {
    trace("Cleaning up epoll platform data");
    
    if (loop->platform_data) {
        epoll_platform_data_t *platform = (epoll_platform_data_t*)loop->platform_data;
        
        if (platform->events) {
            free(platform->events);
        }
        
        if (platform->epoll_fd >= 0) {
            close(platform->epoll_fd);
        }
        
        free(platform);
        loop->platform_data = NULL;
    }
    
    if (loop->threads) {
        free(loop->threads);
        loop->threads = NULL;
    }
    
    info("epoll platform data cleaned up");
}

ev_err platform_add_socket(ev_loop *loop, ev_sock *sock, uint32_t events) {
    epoll_platform_data_t *platform = (epoll_platform_data_t*)loop->platform_data;
    struct epoll_event ev;
    
    // Define event flags for platform interface
    #define EV_READ  0x01
    #define EV_WRITE 0x02
    
    ev.events = 0;
    if (events & EV_READ) {
        ev.events |= EPOLLIN;
    }
    if (events & EV_WRITE) {
        ev.events |= EPOLLOUT;
    }
    ev.events |= EPOLLRDHUP | EPOLLERR | EPOLLHUP; // Always monitor for close/error
    ev.data.ptr = sock;
    
    if (epoll_ctl(platform->epoll_fd, EPOLL_CTL_ADD, sock->fd, &ev) == -1) {
        error("epoll_ctl ADD failed: %s", strerror(errno));
        return system_error_to_ev_err(errno);
    }
    
    return EV_OK;
}

ev_err platform_remove_socket(ev_loop *loop, ev_sock *sock) {
    epoll_platform_data_t *platform = (epoll_platform_data_t*)loop->platform_data;
    
    if (epoll_ctl(platform->epoll_fd, EPOLL_CTL_DEL, sock->fd, NULL) == -1) {
        // Ignore errors since the socket might not be registered
        trace("epoll_ctl DEL failed (ignored): %s", strerror(errno));
    }
    
    return EV_OK;
}

ev_err platform_modify_socket(ev_loop *loop, ev_sock *sock, uint32_t events) {
    epoll_platform_data_t *platform = (epoll_platform_data_t*)loop->platform_data;
    struct epoll_event ev;
    
    ev.events = 0;
    if (events & EV_READ) {
        ev.events |= EPOLLIN;
    }
    if (events & EV_WRITE) {
        ev.events |= EPOLLOUT;
    }
    ev.events |= EPOLLRDHUP | EPOLLERR | EPOLLHUP; // Always monitor for close/error
    ev.data.ptr = sock;
    
    if (epoll_ctl(platform->epoll_fd, EPOLL_CTL_MOD, sock->fd, &ev) == -1) {
        error("epoll_ctl MOD failed: %s", strerror(errno));
        return system_error_to_ev_err(errno);
    }
    
    return EV_OK;
}

ev_err platform_wait_threads(ev_loop *loop, uint64_t timeout_ms) {
    if (!loop->threads) {
        return EV_OK;
    }
    
    epoll_thread_data_t *threads = (epoll_thread_data_t*)loop->threads;
    
    // Wait for all threads to join (no timeout support in pthread_join)
    for (size_t i = 0; i < loop->worker_threads; i++) {
        join_platform_thread(threads[i].thread);
    }
    
    return EV_OK;
}

//=============================================================================
// NETWORK DISCOVERY IMPLEMENTATION
//=============================================================================

ev_err platform_find_networks(ev_network_info **network_info, size_t *num_networks) {
    trace("Finding network interfaces on Linux");
    
    if (!network_info || !num_networks) {
        return EV_ERR_NULL_PTR;
    }
    
    *network_info = NULL;
    *num_networks = 0;
    
    // Use getifaddrs to enumerate network interfaces (same as macOS/BSD)
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
    
    info("Successfully found %zu network interfaces on Linux", count);
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

#endif // Linux