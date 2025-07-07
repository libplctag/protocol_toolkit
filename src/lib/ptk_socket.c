#include "ptk_socket.h"
#include "ptk_atomic.h"
#include "ptk_utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

//=============================================================================
// CONSTANTS
//=============================================================================

#define PTK_SOCKET_MAX_EVENTS 64
#define PTK_SOCKET_INVALID_FD -1
#define PTK_SOCKET_MAGIC 0x534F434B  // "SOCK"
#define PTK_SOCKET_TIMER_MIN_MS 50   // Minimum timer resolution

//=============================================================================
// SOCKET STRUCTURE
//=============================================================================

typedef struct ptk_sock {
    uint32_t magic;                                                                   // Magic number for validation
    ptk_sock_type type;                                                               // Socket type
    int fd;                                                                           // OS socket file descriptor
    int kqueue_fd;                                                                    // kqueue file descriptor
    struct sockaddr_storage local_addr;                                               // Local address
    struct sockaddr_storage remote_addr;                                              // Remote address
    socklen_t local_addr_len;                                                         // Local address length
    socklen_t remote_addr_len;                                                        // Remote address length
    bool connected;                                                                   // Connection state for TCP
    bool listening;                                                                   // Listening state for TCP server
    bool timer_active;                                                                // Timer active flag
    ptk_duration_ms timer_period_ms;                                                  // Timer period
    int timer_ident;                                                                  // Timer identifier for kqueue
    void (*interrupt_handler)(ptk_sock *sock, ptk_time_ms time_ms, void *user_data);  // Interrupt callback
    void *interrupt_user_data;                                                        // User data for interrupt callback
    bool aborted;                                                                     // Socket operations aborted flag
} ptk_sock;

//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Validate socket magic number
 */
static bool ptk_socket_is_valid(ptk_sock *sock) { return sock && sock->magic == PTK_SOCKET_MAGIC; }

/**
 * @brief Set socket to non-blocking mode
 */
static ptk_err ptk_socket_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) { return PTK_ERR_NETWORK_ERROR; }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_OK;
}

/**
 * @brief Set SO_REUSEADDR on socket
 */
static ptk_err ptk_socket_set_reuseaddr(int fd) {
    int reuse = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_OK;
}

/**
 * @brief Set socket options for small packets (disable Nagle, etc.)
 */
static ptk_err ptk_socket_set_small_packet_opts(int fd, ptk_sock_type type) {
    if(type == PTK_SOCK_TCP_CLIENT || type == PTK_SOCK_TCP_SERVER) {
        // Disable Nagle algorithm for small packets
        int nodelay = 1;
        if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) == -1) { return PTK_ERR_NETWORK_ERROR; }
    }

    return PTK_OK;
}

/**
 * @brief Map errno to ptk_err
 */
static ptk_err ptk_socket_map_errno(int error) {
    switch(error) {
        case EAGAIN: return PTK_ERR_WOULD_BLOCK;
        case ECONNRESET:
        case EPIPE: return PTK_ERR_CLOSED;
        case ECONNREFUSED: return PTK_ERR_CONNECTION_REFUSED;
        case EHOSTUNREACH:
        case ENETUNREACH: return PTK_ERR_HOST_UNREACHABLE;
        case EADDRINUSE: return PTK_ERR_ADDRESS_IN_USE;
        case EMFILE:
        case ENFILE: return PTK_ERR_NO_RESOURCES;
        case EACCES: return PTK_ERR_AUTHORIZATION_FAILED;
        case ETIMEDOUT: return PTK_ERR_TIMEOUT;
        default: return PTK_ERR_NETWORK_ERROR;
    }
}

/**
 * @brief Create kqueue and register socket for events
 */
static ptk_err ptk_socket_setup_kqueue(ptk_sock *sock) {
    sock->kqueue_fd = kqueue();
    if(sock->kqueue_fd == -1) { return PTK_ERR_NETWORK_ERROR; }

    // Add interrupt and abort events (user events)
    struct kevent ev[2];
    EV_SET(&ev[0], 1, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);  // Interrupt event
    EV_SET(&ev[1], 2, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);  // Abort event

    if(kevent(sock->kqueue_fd, ev, 2, NULL, 0, NULL) == -1) {
        close(sock->kqueue_fd);
        sock->kqueue_fd = PTK_SOCKET_INVALID_FD;
        return PTK_ERR_NETWORK_ERROR;
    }

    return PTK_OK;
}

/**
 * @brief Add socket event to kqueue
 */
static ptk_err ptk_socket_add_event(ptk_sock *sock, int16_t filter) {
    struct kevent ev;
    EV_SET(&ev, sock->fd, filter, EV_ADD | EV_ENABLE, 0, 0, NULL);

    if(kevent(sock->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_OK;
}

/**
 * @brief Wait for socket events using kqueue
 */
static ptk_err ptk_socket_wait_for_event(ptk_sock *sock, int16_t filter) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    // Check abort flag before starting
    if(sock->aborted) { return PTK_ERR_ABORT; }

    // Add the event we're waiting for
    ptk_err err = ptk_socket_add_event(sock, filter);
    if(err != PTK_OK) { return err; }

    struct kevent events[PTK_SOCKET_MAX_EVENTS];

    while(true) {
        int nevents = kevent(sock->kqueue_fd, NULL, 0, events, PTK_SOCKET_MAX_EVENTS, NULL);

        if(nevents == -1) {
            if(errno == EINTR) {
                continue;  // Signal interrupted, retry
            }
            return ptk_socket_map_errno(errno);
        }

        // Check for events
        for(int i = 0; i < nevents; i++) {
            if(events[i].filter == EVFILT_USER && events[i].ident == 2) {
                // Abort event
                return PTK_ERR_ABORT;
            } else if(events[i].filter == EVFILT_USER && events[i].ident == 1) {
                // Interrupt event - call handler if set
                if(sock->interrupt_handler) {
                    ptk_time_ms current_time = ptk_now_ms();
                    sock->interrupt_handler(sock, current_time, sock->interrupt_user_data);
                }
                return PTK_ERR_INTERRUPT;
            } else if(events[i].filter == EVFILT_TIMER && events[i].ident == (uintptr_t)sock->timer_ident) {
                // Timer event - call handler if set
                if(sock->interrupt_handler) {
                    ptk_time_ms current_time = ptk_now_ms();
                    sock->interrupt_handler(sock, current_time, sock->interrupt_user_data);
                }
                sock->timer_active = false;
                return PTK_ERR_INTERRUPT;
            } else if(events[i].ident == (uintptr_t)sock->fd && events[i].filter == filter) {
                // Socket event ready
                if(events[i].flags & EV_ERROR) { return ptk_socket_map_errno(events[i].data); }
                return PTK_OK;
            }
        }
    }
}

/**
 * @brief Resolve hostname to sockaddr
 */
static ptk_err ptk_socket_resolve_addr(const char *host, int port, struct sockaddr_storage *addr, socklen_t *addr_len) {
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4 only
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int ret = getaddrinfo(host, port_str, &hints, &result);
    if(ret != 0) { return PTK_ERR_HOST_UNREACHABLE; }

    memcpy(addr, result->ai_addr, result->ai_addrlen);
    *addr_len = result->ai_addrlen;

    freeaddrinfo(result);
    return PTK_OK;
}

//=============================================================================
// SOCKET MANAGEMENT
//=============================================================================

ptk_err ptk_socket_type(ptk_sock *sock, ptk_sock_type *type) {
    if(!ptk_socket_is_valid(sock) || !type) { return PTK_ERR_NULL_PTR; }

    *type = sock->type;
    return PTK_OK;
}

ptk_err ptk_socket_start_repeat_interrupt(ptk_sock *sock, ptk_duration_ms timer_period_ms) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    // Round up to minimum resolution
    ptk_duration_ms period =
        ((timer_period_ms + PTK_SOCKET_TIMER_MIN_MS - 1) / PTK_SOCKET_TIMER_MIN_MS) * PTK_SOCKET_TIMER_MIN_MS;

    sock->timer_period_ms = period;
    sock->timer_ident = (int)(uintptr_t)sock;  // Use socket pointer as unique identifier
    sock->timer_active = true;

    // Add timer event to kqueue
    struct kevent ev;
    EV_SET(&ev, sock->timer_ident, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, period, NULL);

    if(kevent(sock->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) {
        sock->timer_active = false;
        return PTK_ERR_NETWORK_ERROR;
    }

    return PTK_OK;
}

ptk_err ptk_socket_stop_repeat_interrupt(ptk_sock *sock) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    if(sock->timer_active) {
        // Remove timer from kqueue
        struct kevent ev;
        EV_SET(&ev, sock->timer_ident, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
        kevent(sock->kqueue_fd, &ev, 1, NULL, 0, NULL);

        sock->timer_active = false;
    }

    return PTK_OK;
}

ptk_err ptk_socket_close(ptk_sock *sock) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    ptk_socket_stop_repeat_interrupt(sock);

    if(sock->fd != PTK_SOCKET_INVALID_FD) {
        close(sock->fd);
        sock->fd = PTK_SOCKET_INVALID_FD;
    }

    if(sock->kqueue_fd != PTK_SOCKET_INVALID_FD) {
        close(sock->kqueue_fd);
        sock->kqueue_fd = PTK_SOCKET_INVALID_FD;
    }

    sock->magic = 0;
    free(sock);

    return PTK_OK;
}

ptk_err ptk_socket_wait(ptk_sock *sock, ptk_time_ms timeout_ms) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    // Check abort flag before starting
    if(sock->aborted) { return PTK_ERR_ABORT; }

    struct timespec timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1000000;

    struct kevent events[PTK_SOCKET_MAX_EVENTS];
    int nevents = kevent(sock->kqueue_fd, NULL, 0, events, PTK_SOCKET_MAX_EVENTS, &timeout);

    if(nevents == -1) { return ptk_socket_map_errno(errno); }

    if(nevents == 0) { return PTK_ERR_TIMEOUT; }

    // Check for abort event in returned events
    for(int i = 0; i < nevents; i++) {
        if(events[i].filter == EVFILT_USER && events[i].ident == 2) { return PTK_ERR_ABORT; }
    }

    return PTK_OK;
}

ptk_err ptk_socket_set_interrupt_handler(ptk_sock *sock,
                                         void (*interrupt_handler)(ptk_sock *sock, ptk_time_ms time_ms, void *user_data),
                                         void *user_data) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    sock->interrupt_handler = interrupt_handler;
    sock->interrupt_user_data = user_data;

    return PTK_OK;
}

ptk_err ptk_socket_wait_for_interrupt(ptk_sock *sock) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    // Check abort flag before starting
    if(sock->aborted) { return PTK_ERR_ABORT; }

    struct kevent events[PTK_SOCKET_MAX_EVENTS];
    struct timespec timeout;

    // Use timer period as timeout if timer is active, otherwise wait indefinitely
    if(sock->timer_active) {
        timeout.tv_sec = sock->timer_period_ms / 1000;
        timeout.tv_nsec = (sock->timer_period_ms % 1000) * 1000000;
    }

    while(true) {
        int nevents = kevent(sock->kqueue_fd, NULL, 0, events, PTK_SOCKET_MAX_EVENTS, sock->timer_active ? &timeout : NULL);

        if(nevents == -1) {
            if(errno == EINTR) {
                continue;  // Signal interrupted, retry
            }
            return ptk_socket_map_errno(errno);
        }

        if(nevents == 0) {
            // Timeout occurred, call interrupt handler if set
            if(sock->interrupt_handler) {
                ptk_time_ms current_time = ptk_now_ms();
                sock->interrupt_handler(sock, current_time, sock->interrupt_user_data);
            }
            continue;
        }

        // Check for events
        for(int i = 0; i < nevents; i++) {
            if(events[i].filter == EVFILT_USER && events[i].ident == 2) {
                // Abort event
                return PTK_ERR_ABORT;
            } else if(events[i].filter == EVFILT_USER && events[i].ident == 1) {
                // Interrupt event - call handler if set
                if(sock->interrupt_handler) {
                    ptk_time_ms current_time = ptk_now_ms();
                    sock->interrupt_handler(sock, current_time, sock->interrupt_user_data);
                }
                return PTK_OK;
            } else if(events[i].filter == EVFILT_TIMER && events[i].ident == (uintptr_t)sock->timer_ident) {
                // Timer event - call handler if set
                if(sock->interrupt_handler) {
                    ptk_time_ms current_time = ptk_now_ms();
                    sock->interrupt_handler(sock, current_time, sock->interrupt_user_data);
                }
                sock->timer_active = false;
                return PTK_OK;
            }
        }
    }
}

ptk_err ptk_socket_interrupt_once(ptk_sock *sock) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    // Trigger user event immediately
    struct kevent ev;
    EV_SET(&ev, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);

    if(kevent(sock->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_OK;
}

ptk_err ptk_socket_abort(ptk_sock *sock) {
    if(!ptk_socket_is_valid(sock)) { return PTK_ERR_NULL_PTR; }

    // Set abort flag
    sock->aborted = true;

    // Trigger user event to wake up any waiting operations
    struct kevent ev;
    EV_SET(&ev, 2, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);
    kevent(sock->kqueue_fd, &ev, 1, NULL, 0, NULL);

    EV_SET(&ev, 2, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    if(kevent(sock->kqueue_fd, &ev, 1, NULL, 0, NULL) == -1) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_OK;
}

//=============================================================================
// TCP CLIENT SOCKETS
//=============================================================================

ptk_err ptk_tcp_socket_connect(ptk_sock **client, const char *remote_ip, int remote_port) {
    if(!client || !remote_ip) { return PTK_ERR_NULL_PTR; }

    // Allocate socket structure
    ptk_sock *sock = calloc(1, sizeof(ptk_sock));
    if(!sock) { return PTK_ERR_NO_RESOURCES; }

    sock->magic = PTK_SOCKET_MAGIC;
    sock->type = PTK_SOCK_TCP_CLIENT;
    sock->fd = PTK_SOCKET_INVALID_FD;
    sock->kqueue_fd = PTK_SOCKET_INVALID_FD;

    ptk_err err = PTK_OK;

    // Setup kqueue first
    err = ptk_socket_setup_kqueue(sock);
    if(err != PTK_OK) { goto cleanup; }

    // Resolve remote address
    err = ptk_socket_resolve_addr(remote_ip, remote_port, &sock->remote_addr, &sock->remote_addr_len);
    if(err != PTK_OK) { goto cleanup; }

    // Create socket
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock->fd == -1) {
        err = ptk_socket_map_errno(errno);
        goto cleanup;
    }

    // Set socket options
    err = ptk_socket_set_nonblocking(sock->fd);
    if(err != PTK_OK) { goto cleanup; }

    err = ptk_socket_set_reuseaddr(sock->fd);
    if(err != PTK_OK) { goto cleanup; }

    err = ptk_socket_set_small_packet_opts(sock->fd, PTK_SOCK_TCP_CLIENT);
    if(err != PTK_OK) { goto cleanup; }

    // Attempt connection
    int ret = connect(sock->fd, (struct sockaddr *)&sock->remote_addr, sock->remote_addr_len);
    if(ret == -1) {
        if(errno == EINPROGRESS) {
            // Connection in progress, wait for completion
            err = ptk_socket_wait_for_event(sock, EVFILT_WRITE);
            if(err != PTK_OK) { goto cleanup; }

            // Check if connection succeeded
            int error;
            socklen_t len = sizeof(error);
            if(getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
                err = PTK_ERR_NETWORK_ERROR;
                goto cleanup;
            }

            if(error != 0) {
                err = ptk_socket_map_errno(error);
                goto cleanup;
            }
        } else {
            err = ptk_socket_map_errno(errno);
            goto cleanup;
        }
    }

    sock->connected = true;

    // Get local address
    sock->local_addr_len = sizeof(sock->local_addr);
    getsockname(sock->fd, (struct sockaddr *)&sock->local_addr, &sock->local_addr_len);

    *client = sock;
    return PTK_OK;

cleanup:
    if(sock->fd != PTK_SOCKET_INVALID_FD) { close(sock->fd); }
    if(sock->kqueue_fd != PTK_SOCKET_INVALID_FD) { close(sock->kqueue_fd); }
    free(sock);
    return err;
}

ptk_err ptk_tcp_socket_write(ptk_sock *sock, ptk_buf_t *data) {
    if(!ptk_socket_is_valid(sock) || !data) { return PTK_ERR_NULL_PTR; }

    if(sock->type != PTK_SOCK_TCP_CLIENT || !sock->connected) { return PTK_ERR_CLOSED; }

    // Get data to send from start to end
    size_t data_len;
    ptk_err err = ptk_buf_len(&data_len, data);
    if(err != PTK_OK || data_len == 0) {
        return PTK_OK;  // Nothing to send
    }

    uint8_t *send_data;
    err = ptk_buf_get_start_ptr(&send_data, data);
    if(err != PTK_OK) { return err; }

    size_t total_sent = 0;

    while(total_sent < data_len) {
        ssize_t sent = send(sock->fd, send_data + total_sent, data_len - total_sent, 0);

        if(sent == -1) {
            if(errno == EAGAIN) {
                // Socket would block, wait for write readiness
                ptk_err sock_err = ptk_socket_wait_for_event(sock, EVFILT_WRITE);
                if(sock_err != PTK_OK) { return sock_err; }
                continue;
            } else {
                return ptk_socket_map_errno(errno);
            }
        }

        total_sent += sent;
        // Update start position to reflect sent data
        size_t new_start;
        ptk_buf_get_start(&new_start, data);
        ptk_buf_set_start(data, new_start + sent);
    }

    return PTK_OK;
}

ptk_err ptk_tcp_socket_read(ptk_sock *sock, ptk_buf_t *data) {
    if(!ptk_socket_is_valid(sock) || !data) { return PTK_ERR_NULL_PTR; }

    if(sock->type != PTK_SOCK_TCP_CLIENT || !sock->connected) { return PTK_ERR_CLOSED; }

    // Read as much as possible into buffer starting at end
    size_t space_available;
    ptk_err err = ptk_buf_get_remaining(&space_available, data);
    if(err != PTK_OK || space_available == 0) { return PTK_ERR_BUFFER_TOO_SMALL; }

    uint8_t *recv_data;
    err = ptk_buf_get_end_ptr(&recv_data, data);
    if(err != PTK_OK) { return err; }

    size_t total_received = 0;

    while(space_available > 0) {
        ssize_t received = recv(sock->fd, recv_data + total_received, space_available, 0);

        if(received == -1) {
            if(errno == EAGAIN) {
                if(total_received > 0) {
                    // We got some data, update buffer end and return
                    size_t current_end;
                    ptk_buf_get_end(&current_end, data);
                    ptk_buf_set_end(data, current_end + total_received);
                    return PTK_OK;
                }
                // No data yet, wait for read readiness
                ptk_err sock_err = ptk_socket_wait_for_event(sock, EVFILT_READ);
                if(sock_err != PTK_OK) { return sock_err; }
                continue;
            } else {
                return ptk_socket_map_errno(errno);
            }
        }

        if(received == 0) {
            // Connection closed by peer
            if(total_received > 0) {
                size_t current_end;
                ptk_buf_get_end(&current_end, data);
                ptk_buf_set_end(data, current_end + total_received);
                return PTK_OK;
            }
            return PTK_ERR_CLOSED;
        }

        total_received += received;
        space_available -= received;

        // Try to read more if there's space and data might be available
        // This will loop back and either get more data or EAGAIN
    }

    // Update buffer end position
    size_t current_end;
    ptk_buf_get_end(&current_end, data);
    ptk_buf_set_end(data, current_end + total_received);
    return PTK_OK;
}

//=============================================================================
// TCP SERVER SOCKETS
//=============================================================================

ptk_err ptk_tcp_socket_listen(ptk_sock **server, const char *local_ip, int local_port, int backlog) {
    if(!server) { return PTK_ERR_NULL_PTR; }

    // Allocate socket structure
    ptk_sock *sock = calloc(1, sizeof(ptk_sock));
    if(!sock) { return PTK_ERR_NO_RESOURCES; }

    sock->magic = PTK_SOCKET_MAGIC;
    sock->type = PTK_SOCK_TCP_SERVER;
    sock->fd = PTK_SOCKET_INVALID_FD;
    sock->kqueue_fd = PTK_SOCKET_INVALID_FD;

    ptk_err err = PTK_OK;

    // Setup kqueue first
    err = ptk_socket_setup_kqueue(sock);
    if(err != PTK_OK) { goto cleanup; }

    // Create socket
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock->fd == -1) {
        err = ptk_socket_map_errno(errno);
        goto cleanup;
    }

    // Set socket options
    err = ptk_socket_set_nonblocking(sock->fd);
    if(err != PTK_OK) { goto cleanup; }

    err = ptk_socket_set_reuseaddr(sock->fd);
    if(err != PTK_OK) { goto cleanup; }

    err = ptk_socket_set_small_packet_opts(sock->fd, PTK_SOCK_TCP_SERVER);
    if(err != PTK_OK) { goto cleanup; }

    // Setup local address
    struct sockaddr_in *addr = (struct sockaddr_in *)&sock->local_addr;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(local_port);

    if(local_ip && strcmp(local_ip, "0.0.0.0") != 0) {
        if(inet_pton(AF_INET, local_ip, &addr->sin_addr) != 1) {
            err = PTK_ERR_INVALID_PARAM;
            goto cleanup;
        }
    } else {
        addr->sin_addr.s_addr = INADDR_ANY;
    }

    sock->local_addr_len = sizeof(struct sockaddr_in);

    // Bind socket
    if(bind(sock->fd, (struct sockaddr *)&sock->local_addr, sock->local_addr_len) == -1) {
        err = ptk_socket_map_errno(errno);
        goto cleanup;
    }

    // Listen
    if(listen(sock->fd, backlog) == -1) {
        err = ptk_socket_map_errno(errno);
        goto cleanup;
    }

    sock->listening = true;

    *server = sock;
    return PTK_OK;

cleanup:
    if(sock->fd != PTK_SOCKET_INVALID_FD) { close(sock->fd); }
    if(sock->kqueue_fd != PTK_SOCKET_INVALID_FD) { close(sock->kqueue_fd); }
    free(sock);
    return err;
}

ptk_err ptk_tcp_socket_accept(ptk_sock *server, ptk_sock **client) {
    if(!ptk_socket_is_valid(server) || !client) { return PTK_ERR_NULL_PTR; }

    if(server->type != PTK_SOCK_TCP_SERVER || !server->listening) { return PTK_ERR_INVALID_PARAM; }

    while(true) {
        // Try to accept connection
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(server->fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if(client_fd == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                // No connections available, wait for read readiness
                ptk_err err = ptk_socket_wait_for_event(server, EVFILT_READ);
                if(err != PTK_OK) { return err; }
                continue;
            } else {
                return ptk_socket_map_errno(errno);
            }
        }

        // Create client socket structure
        ptk_sock *client_sock = calloc(1, sizeof(ptk_sock));
        if(!client_sock) {
            close(client_fd);
            return PTK_ERR_NO_RESOURCES;
        }

        client_sock->magic = PTK_SOCKET_MAGIC;
        client_sock->type = PTK_SOCK_TCP_CLIENT;
        client_sock->fd = client_fd;
        client_sock->connected = true;

        // Setup kqueue for client
        ptk_err err = ptk_socket_setup_kqueue(client_sock);
        if(err != PTK_OK) {
            close(client_fd);
            free(client_sock);
            return err;
        }

        // Copy addresses
        memcpy(&client_sock->remote_addr, &client_addr, client_addr_len);
        client_sock->remote_addr_len = client_addr_len;

        client_sock->local_addr_len = sizeof(client_sock->local_addr);
        getsockname(client_fd, (struct sockaddr *)&client_sock->local_addr, &client_sock->local_addr_len);

        // Set client socket options
        err = ptk_socket_set_nonblocking(client_fd);
        if(err != PTK_OK) {
            close(client_fd);
            ptk_socket_close(client_sock);
            return err;
        }

        err = ptk_socket_set_small_packet_opts(client_fd, PTK_SOCK_TCP_CLIENT);
        if(err != PTK_OK) {
            close(client_fd);
            ptk_socket_close(client_sock);
            return err;
        }

        *client = client_sock;
        return PTK_OK;
    }
}

//=============================================================================
// UDP SOCKETS
//=============================================================================

ptk_err ptk_udp_socket_create(ptk_sock **udp_sock, const char *local_host, int local_port) {
    if(!udp_sock) { return PTK_ERR_NULL_PTR; }

    // Allocate socket structure
    ptk_sock *sock = calloc(1, sizeof(ptk_sock));
    if(!sock) { return PTK_ERR_NO_RESOURCES; }

    sock->magic = PTK_SOCKET_MAGIC;
    sock->type = PTK_SOCK_UDP;
    sock->fd = PTK_SOCKET_INVALID_FD;
    sock->kqueue_fd = PTK_SOCKET_INVALID_FD;

    ptk_err err = PTK_OK;

    // Setup kqueue first
    err = ptk_socket_setup_kqueue(sock);
    if(err != PTK_OK) { goto cleanup; }

    // Create socket
    sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock->fd == -1) {
        err = ptk_socket_map_errno(errno);
        goto cleanup;
    }

    // Set socket options
    err = ptk_socket_set_nonblocking(sock->fd);
    if(err != PTK_OK) { goto cleanup; }

    err = ptk_socket_set_reuseaddr(sock->fd);
    if(err != PTK_OK) { goto cleanup; }

    // Bind if local address specified
    if(local_host && local_port > 0) {
        struct sockaddr_in *addr = (struct sockaddr_in *)&sock->local_addr;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(local_port);

        if(strcmp(local_host, "0.0.0.0") != 0) {
            if(inet_pton(AF_INET, local_host, &addr->sin_addr) != 1) {
                err = PTK_ERR_INVALID_PARAM;
                goto cleanup;
            }
        } else {
            addr->sin_addr.s_addr = INADDR_ANY;
        }

        sock->local_addr_len = sizeof(struct sockaddr_in);

        if(bind(sock->fd, (struct sockaddr *)&sock->local_addr, sock->local_addr_len) == -1) {
            err = ptk_socket_map_errno(errno);
            goto cleanup;
        }
    }

    *udp_sock = sock;
    return PTK_OK;

cleanup:
    if(sock->fd != PTK_SOCKET_INVALID_FD) { close(sock->fd); }
    if(sock->kqueue_fd != PTK_SOCKET_INVALID_FD) { close(sock->kqueue_fd); }
    free(sock);
    return err;
}

ptk_err ptk_udp_socket_send(ptk_sock *sock, ptk_buf_t *data, const char *host, int port, bool broadcast) {
    if(!ptk_socket_is_valid(sock) || !data || !host) { return PTK_ERR_NULL_PTR; }

    if(sock->type != PTK_SOCK_UDP) { return PTK_ERR_INVALID_PARAM; }

    // Set broadcast option if needed
    if(broadcast) {
        int broadcast_flag = 1;
        if(setsockopt(sock->fd, SOL_SOCKET, SO_BROADCAST, &broadcast_flag, sizeof(broadcast_flag)) == -1) {
            return PTK_ERR_NETWORK_ERROR;
        }
    }

    // Resolve destination address
    struct sockaddr_storage dest_addr;
    socklen_t dest_addr_len;
    ptk_err err = ptk_socket_resolve_addr(host, port, &dest_addr, &dest_addr_len);
    if(err != PTK_OK) { return err; }

    // Send data from start to end
    size_t send_len;
    err = ptk_buf_len(&send_len, data);
    if(err != PTK_OK || send_len == 0) {
        return PTK_OK;  // Nothing to send
    }

    uint8_t *send_data;
    err = ptk_buf_get_start_ptr(&send_data, data);
    if(err != PTK_OK) { return err; }

    while(true) {
        ssize_t sent = sendto(sock->fd, send_data, send_len, 0, (struct sockaddr *)&dest_addr, dest_addr_len);

        if(sent == -1) {
            if(errno == EAGAIN) {
                // Socket would block, wait for write readiness
                err = ptk_socket_wait_for_event(sock, EVFILT_WRITE);
                if(err != PTK_OK) { return err; }
                continue;
            } else {
                return ptk_socket_map_errno(errno);
            }
        }

        // For UDP, should send all at once or fail
        // Update start position to reflect sent data
        size_t start;
        ptk_buf_get_start(&start, data);
        ptk_buf_set_start(data, start + sent);
        return PTK_OK;
    }
}

ptk_err ptk_udp_socket_recv(ptk_sock *sock, ptk_buf_t *data, char *host, int *port) {
    if(!ptk_socket_is_valid(sock) || !data) { return PTK_ERR_NULL_PTR; }

    if(sock->type != PTK_SOCK_UDP) { return PTK_ERR_INVALID_PARAM; }

    // Check abort flag before starting
    if(sock->aborted) { return PTK_ERR_ABORT; }

    // Get buffer space for receiving
    size_t space_available;
    ptk_err err = ptk_buf_get_remaining(&space_available, data);
    if(err != PTK_OK || space_available == 0) { return PTK_ERR_BUFFER_TOO_SMALL; }

    uint8_t *recv_data;
    err = ptk_buf_get_end_ptr(&recv_data, data);
    if(err != PTK_OK) { return err; }

    while(true) {
        struct sockaddr_storage sender_addr;
        socklen_t sender_addr_len = sizeof(sender_addr);

        ssize_t received = recvfrom(sock->fd, recv_data, space_available, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);

        if(received == -1) {
            if(errno == EAGAIN) {
                // Socket would block, wait for read readiness
                err = ptk_socket_wait_for_event(sock, EVFILT_READ);
                if(err != PTK_OK) { return err; }
                continue;
            } else {
                return ptk_socket_map_errno(errno);
            }
        }

        if(received == 0) {
            // No data available
            continue;
        }

        // Update buffer end position
        size_t current_end;
        ptk_buf_get_end(&current_end, data);
        ptk_buf_set_end(data, current_end + received);

        // Extract sender address information if requested
        if(host && port) {
            if(sender_addr.ss_family == AF_INET) {
                struct sockaddr_in *addr_in = (struct sockaddr_in *)&sender_addr;
                inet_ntop(AF_INET, &addr_in->sin_addr, host, INET_ADDRSTRLEN);
                *port = ntohs(addr_in->sin_port);
            } else {
                strcpy(host, "unknown");
                *port = 0;
            }
        }

        return PTK_OK;
    }
}

//=============================================================================
// NETWORK DISCOVERY
//=============================================================================

ptk_err ptk_socket_find_networks(ptk_network_info **network_info, size_t *num_networks) {
    if(!network_info || !num_networks) { return PTK_ERR_NULL_PTR; }

    struct ifaddrs *ifaddrs_list;
    if(getifaddrs(&ifaddrs_list) == -1) { return PTK_ERR_NETWORK_ERROR; }

    // Count interfaces
    size_t count = 0;
    for(struct ifaddrs *ifa = ifaddrs_list; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {  // Skip loopback
            count++;
        }
    }

    if(count == 0) {
        freeifaddrs(ifaddrs_list);
        *network_info = NULL;
        *num_networks = 0;
        return PTK_OK;
    }

    // Allocate result array
    ptk_network_info *info = calloc(count, sizeof(ptk_network_info));
    if(!info) {
        freeifaddrs(ifaddrs_list);
        return PTK_ERR_NO_RESOURCES;
    }

    // Fill in network information
    size_t index = 0;
    for(struct ifaddrs *ifa = ifaddrs_list; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {

            struct sockaddr_in *addr_in = (struct sockaddr_in *)ifa->ifa_addr;
            struct sockaddr_in *netmask_in = (struct sockaddr_in *)ifa->ifa_netmask;

            // Allocate and copy IP address
            info[index].network_ip = malloc(INET_ADDRSTRLEN);
            if(info[index].network_ip) { inet_ntop(AF_INET, &addr_in->sin_addr, info[index].network_ip, INET_ADDRSTRLEN); }

            // Allocate and copy netmask
            info[index].netmask = malloc(INET_ADDRSTRLEN);
            if(info[index].netmask) { inet_ntop(AF_INET, &netmask_in->sin_addr, info[index].netmask, INET_ADDRSTRLEN); }

            // Calculate and copy broadcast address
            info[index].broadcast = malloc(INET_ADDRSTRLEN);
            if(info[index].broadcast) {
                uint32_t ip = ntohl(addr_in->sin_addr.s_addr);
                uint32_t mask = ntohl(netmask_in->sin_addr.s_addr);
                uint32_t broadcast = ip | (~mask);

                struct in_addr broadcast_addr;
                broadcast_addr.s_addr = htonl(broadcast);
                inet_ntop(AF_INET, &broadcast_addr, info[index].broadcast, INET_ADDRSTRLEN);
            }

            index++;
        }
    }

    freeifaddrs(ifaddrs_list);

    *network_info = info;
    *num_networks = index;

    return PTK_OK;
}

void ptk_socket_network_info_dispose(ptk_network_info *network_info, size_t num_networks) {
    if(!network_info) { return; }

    for(size_t i = 0; i < num_networks; i++) {
        free(network_info[i].network_ip);
        free(network_info[i].netmask);
        free(network_info[i].broadcast);
    }

    free(network_info);
}