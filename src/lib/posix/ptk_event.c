#include <ptk_event.h>
#include <ptk_dgram_mem_socket_impl.h>
#include <ptk_log.h>
#include <ptk_time.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <time.h>

// Timer initialization
ptk_status_t ptk_init_timer(ptk_timer_connection_t* timer, uint32_t interval_ms, uint32_t id, bool repeating) {
    if (!timer || interval_ms == 0) return PTK_ERROR_INVALID_PARAM;
    memset(timer, 0, sizeof(*timer));
    timer->base.type = PTK_EVENT_SOURCE_TIMER;
    timer->base.state = 0;
    timer->interval_ms = interval_ms;
    timer->id = id;
    timer->repeating = repeating;
    timer->next_fire_time = ptk_get_time_ms() + interval_ms;
    timer->active = true;
    return PTK_OK;
}

// TCP client connection initialization
ptk_status_t ptk_init_tcp_client_connection(ptk_tcp_client_connection_t* conn, const char* host, uint16_t port) {
    if (!conn || !host) return PTK_ERROR_INVALID_PARAM;
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_TCP;
    conn->base.state = 0;
    conn->fd = -1;
    conn->connect_timeout_ms = 0;
    struct sockaddr_in* addr = &conn->addr;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr->sin_addr) <= 0) return PTK_ERROR_DNS_RESOLVE;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return PTK_ERROR_SOCKET_CREATE;
    set_socket_opts(fd);
    // Set non-blocking for connect
    fcntl(fd, F_SETFL, O_NONBLOCK);
    int res = connect(fd, (struct sockaddr*)addr, sizeof(*addr));
    if (res < 0 && errno != EINPROGRESS) {
        close(fd);
        return PTK_ERROR_CONNECT;
    }
    conn->fd = fd;
    return PTK_OK;
}

// TCP server connection initialization
ptk_status_t ptk_init_tcp_server_connection(ptk_tcp_server_connection_t* conn, const char* host, uint16_t port) {
    if (!conn || !host) return PTK_ERROR_INVALID_PARAM;
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_TCP;
    conn->base.state = 0;
    conn->fd = -1;
    conn->connect_timeout_ms = 0;
    struct sockaddr_in* addr = &conn->addr;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr->sin_addr) <= 0) return PTK_ERROR_DNS_RESOLVE;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return PTK_ERROR_SOCKET_CREATE;
    set_socket_opts(fd);
    if (bind(fd, (struct sockaddr*)addr, sizeof(*addr)) < 0) {
        close(fd);
        return PTK_ERROR_SOCKET_CREATE;
    }
    if (listen(fd, 8) < 0) {
        close(fd);
        return PTK_ERROR_SOCKET_CREATE;
    }
    conn->fd = fd;
    return PTK_OK;
}

// UDP connection initialization
ptk_status_t ptk_init_udp_connection(ptk_udp_connection_t* conn, const char* host, uint16_t port) {
    if (!conn || !host) return PTK_ERROR_INVALID_PARAM;
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_UDP;
    conn->base.state = 0;
    conn->fd = -1;
    conn->bind_timeout_ms = 0;
    struct sockaddr_in* laddr = &conn->local_addr;
    struct sockaddr_in* raddr = &conn->remote_addr;
    laddr->sin_family = AF_INET;
    laddr->sin_port = htons(port);
    laddr->sin_addr.s_addr = INADDR_ANY;
    raddr->sin_family = AF_INET;
    raddr->sin_port = htons(port);
    if (inet_pton(AF_INET, host, &raddr->sin_addr) <= 0) return PTK_ERROR_DNS_RESOLVE;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return PTK_ERROR_SOCKET_CREATE;
    set_socket_opts(fd);
    if (bind(fd, (struct sockaddr*)laddr, sizeof(*laddr)) < 0) {
        close(fd);
        return PTK_ERROR_SOCKET_CREATE;
    }
    conn->fd = fd;
    return PTK_OK;
}

// App event connection initialization (now uses in-memory dgram socket)
ptk_status_t ptk_init_app_event_connection(ptk_app_event_connection_t* conn, ptk_slice_bytes_t buffer_slice) {
    if (!conn) return PTK_ERROR_INVALID_PARAM;
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_APP_EVENT;
    conn->base.state = 0;
    // Initialize the in-memory dgram socket (platform-specific)
    ptk_status_t st = ptk_dgram_mem_socket_init(&conn->dgram_sock, buffer_slice);
    if (st != PTK_OK) return st;
    // Expose the read end of the notification pipe as fd for select()
    conn->fd = ptk_dgram_mem_socket_get_fd(&conn->dgram_sock);
    return PTK_OK;
}

// TCP server accept
ptk_status_t ptk_tcp_server_accept(ptk_tcp_server_connection_t* server, ptk_tcp_client_connection_t* client_conn, uint32_t timeout_ms) {
    if (!server || !client_conn) return PTK_ERROR_INVALID_PARAM;
    int fd = server->fd;
    if (fd < 0) return PTK_ERROR_NOT_CONNECTED;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int cfd = accept(fd, (struct sockaddr*)&addr, &addrlen);
    if (cfd < 0) return PTK_ERROR_CONNECT;
    set_socket_opts(cfd);
    memset(client_conn, 0, sizeof(*client_conn));
    client_conn->base.type = PTK_EVENT_SOURCE_TCP;
    client_conn->base.state = 0;
    client_conn->fd = cfd;
    memcpy(&client_conn->addr, &addr, sizeof(addr));
    return PTK_OK;
}

// Connection read (TCP/UDP)
ptk_slice_t ptk_connection_read(ptk_connection_t* conn, ptk_slice_t* buffer, uint32_t timeout_ms) {
    if (!conn || !buffer || !buffer->data || buffer->len == 0) return (ptk_slice_t){0};
    ssize_t n = 0;
    switch (conn->type) {
        case PTK_EVENT_SOURCE_TCP: {
            int fd = ((ptk_tcp_client_connection_t*)conn)->fd;
            n = read(fd, buffer->data, buffer->len);
            break;
        }
        case PTK_EVENT_SOURCE_UDP: {
            int fd = ((ptk_udp_connection_t*)conn)->fd;
            n = recvfrom(fd, buffer->data, buffer->len, 0, NULL, NULL);
            break;
        }
        case PTK_EVENT_SOURCE_APP_EVENT: {
            ptk_app_event_connection_t* app = (ptk_app_event_connection_t*)conn;
            // Use the dgram socket receive function
            n = ptk_dgram_mem_socket_recv(&app->dgram_sock, buffer->data, buffer->len, timeout_ms);
            break;
        }
        default: return (ptk_slice_t){0};
    }
    if (n <= 0) return (ptk_slice_t){0};
    return ptk_slice_make(buffer->data, (size_t)n);
}

// Connection write (TCP/UDP)
ptk_status_t ptk_connection_write(ptk_connection_t* conn, ptk_slice_t* data, uint32_t timeout_ms) {
    if (!conn || !data || !data->data || data->len == 0) return PTK_ERROR_INVALID_PARAM;
    ssize_t n = 0;
    switch (conn->type) {
        case PTK_EVENT_SOURCE_TCP: {
            int fd = ((ptk_tcp_client_connection_t*)conn)->fd;
            n = write(fd, data->data, data->len);
            break;
        }
        case PTK_EVENT_SOURCE_UDP: {
            int fd = ((ptk_udp_connection_t*)conn)->fd;
            struct sockaddr_in* raddr = &((ptk_udp_connection_t*)conn)->remote_addr;
            n = sendto(fd, data->data, data->len, 0, (struct sockaddr*)raddr, sizeof(*raddr));
            break;
        }
        case PTK_EVENT_SOURCE_APP_EVENT: {
            ptk_app_event_connection_t* app = (ptk_app_event_connection_t*)conn;
            // Use the dgram socket send function
            n = ptk_dgram_mem_socket_send(&app->dgram_sock, data->data, data->len, timeout_ms);
            break;
        }
        default: return PTK_ERROR_INVALID_PARAM;
    }
    return (n < 0) ? PTK_ERROR_INVALID_DATA : PTK_OK;
}

// Connection close
ptk_status_t ptk_connection_close(ptk_connection_t* conn) {
    if (!conn) return PTK_ERROR_INVALID_PARAM;
    switch (conn->type) {
        case PTK_EVENT_SOURCE_TCP:
            close(((ptk_tcp_client_connection_t*)conn)->fd);
            ((ptk_tcp_client_connection_t*)conn)->fd = -1;
            break;
        case PTK_EVENT_SOURCE_UDP:
            close(((ptk_udp_connection_t*)conn)->fd);
            ((ptk_udp_connection_t*)conn)->fd = -1;
            break;
        case PTK_EVENT_SOURCE_APP_EVENT:
            // No-op
            break;
        case PTK_EVENT_SOURCE_TIMER:
            ((ptk_timer_connection_t*)conn)->active = false;
            break;
        default:
            return PTK_ERROR_INVALID_PARAM;
    }
    conn->state = PTK_CONN_CLOSED;
    return PTK_OK;
}



// Helper: set socket options for reuse and no signals
static void set_socket_opts(int fd) {
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set));
#else
    signal(SIGPIPE, SIG_IGN);
#endif
}

// Main event loop: waits for any event source to become ready
int ptk_wait_for_multiple(ptk_connection_t** event_sources, size_t count, uint32_t timeout_ms) {
    fd_set readfds, writefds, exceptfds;
    int maxfd = -1;
    FD_ZERO(&readfds); FD_ZERO(&writefds); FD_ZERO(&exceptfds);
    uint64_t start = ptk_get_time_ms();
    int ready = 0;
    
    // Prepare fd sets
    for (size_t i = 0; i < count; ++i) {
        ptk_connection_t* conn = event_sources[i];
        if (!conn) continue;
        switch (conn->type) {
            case PTK_EVENT_SOURCE_TCP:
            case PTK_EVENT_SOURCE_UDP: {
                int fd = ((ptk_tcp_client_connection_t*)conn)->fd;
                if (fd >= 0) {
                    FD_SET(fd, &readfds);
                    FD_SET(fd, &exceptfds);
                    if (fd > maxfd) maxfd = fd;
                }
                break;
            }
            case PTK_EVENT_SOURCE_TIMER: {
                // Timers handled below
                break;
            }
            case PTK_EVENT_SOURCE_APP_EVENT: {
                // The fd is already included in the fd set, so state will be set by select()
                int fd = ((ptk_app_event_connection_t*)conn)->fd;
                if (fd >= 0 && FD_ISSET(fd, &readfds)) {
                    conn->state |= PTK_CONN_DATA_READY;
                }
                break;
            }
            default: break;
        }
    }

    // Compute select timeout (minimum of timer and user timeout)
    struct timeval tv, *tvp = NULL;
    uint64_t min_timeout = timeout_ms;
    uint64_t now = ptk_get_time_ms();
    for (size_t i = 0; i < count; ++i) {
        ptk_connection_t* conn = event_sources[i];
        if (conn && conn->type == PTK_EVENT_SOURCE_TIMER) {
            ptk_timer_connection_t* timer = (ptk_timer_connection_t*)conn;
            if (timer->active && timer->next_fire_time > now) {
                uint64_t tmo = timer->next_fire_time - now;
                if (tmo < min_timeout) min_timeout = tmo;
            } else if (timer->active && timer->next_fire_time <= now) {
                min_timeout = 0;
            }
        }
    }
    if (min_timeout != (uint64_t)-1) {
        tv.tv_sec = min_timeout / 1000;
        tv.tv_usec = (min_timeout % 1000) * 1000;
        tvp = &tv;
    }

    // Wait for events
    ready = select(maxfd + 1, &readfds, &writefds, &exceptfds, tvp);
    now = ptk_get_time_ms();

    // Update connection states
    for (size_t i = 0; i < count; ++i) {
        ptk_connection_t* conn = event_sources[i];
        if (!conn) continue;
        conn->state = 0;
        switch (conn->type) {
            case PTK_EVENT_SOURCE_TCP:
            case PTK_EVENT_SOURCE_UDP: {
                int fd = ((ptk_tcp_client_connection_t*)conn)->fd;
                if (fd >= 0) {
                    if (FD_ISSET(fd, &readfds)) conn->state |= PTK_CONN_DATA_READY;
                    if (FD_ISSET(fd, &exceptfds)) conn->state |= PTK_CONN_ERROR;
                }
                break;
            }
            case PTK_EVENT_SOURCE_TIMER: {
                ptk_timer_connection_t* timer = (ptk_timer_connection_t*)conn;
                if (timer->active && timer->next_fire_time <= now) {
                    conn->state |= PTK_CONN_DATA_READY;
                    if (timer->repeating) timer->next_fire_time = now + timer->interval_ms;
                    else timer->active = false;
                }
                break;
            }
            default: break;
        }
    }
    return ready;
}
