#include <ptk_sock.h>
#include "threadlet_scheduler.h"
#include "threadlet_core.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct ptk_sock {
    int fd;
    ptk_sock_type type;
    event_loop_t *event_loop;
};

static void ptk_sock_destructor(void *ptr) {
    ptk_sock *sock = (ptk_sock *)ptr;
    if (!sock) return;
    
    info("Destroying socket fd=%d", sock->fd);
    
    if (sock->fd >= 0) {
        close(sock->fd);
    }
}

static ptk_sock *create_socket(ptk_sock_type type, int domain, int sock_type, int protocol) {
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), ptk_sock_destructor);
    if (!sock) {
        error("Failed to allocate socket structure");
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    sock->fd = socket(domain, sock_type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
    if (sock->fd < 0) {
        error("socket() failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        ptk_free(&sock);
        return NULL;
    }
    
    sock->type = type;
    sock->event_loop = get_thread_local_event_loop();
    if (!sock->event_loop) {
        error("No event loop available for socket");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        ptk_free(&sock);
        return NULL;
    }
    
    debug("Created socket fd=%d type=%d", sock->fd, type);
    return sock;
}

static ptk_err wait_for_io(ptk_sock *sock, uint32_t events, ptk_duration_ms timeout_ms) {
    if (!sock || !sock->event_loop) {
        warn("Invalid socket or event loop");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    threadlet_t *current = threadlet_get_current();
    if (!current) {
        warn("wait_for_io called outside threadlet context");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return PTK_ERR_INVALID_STATE;
    }
    
    debug("Waiting for I/O on fd=%d events=0x%x timeout=%ld", sock->fd, events, timeout_ms);
    
    ptk_err err = event_loop_register_io(sock->event_loop, sock->fd, events, current, timeout_ms);
    if (err != PTK_OK) {
        error("Failed to register I/O wait");
        return err;
    }
    
    threadlet_yield_to_scheduler(current);
    
    return ptk_get_err();
}

ptk_sock_type ptk_socket_type(ptk_sock *sock) {
    if (!sock) {
        warn("NULL socket in ptk_socket_type");
        return PTK_SOCK_INVALID;
    }
    return sock->type;
}

ptk_err ptk_socket_abort(ptk_sock *sock) {
    info("Aborting socket operations for fd=%d", sock ? sock->fd : -1);
    
    if (!sock) {
        warn("NULL socket in ptk_socket_abort");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    ptk_set_err(PTK_ERR_ABORT);
    
    if (sock->event_loop) {
        event_loop_unregister_io(sock->event_loop, sock->fd);
        platform_event_loop_wake(sock->event_loop->platform);
    }
    
    return PTK_OK;
}

ptk_err ptk_socket_wait(ptk_sock *sock, ptk_duration_ms timeout_ms) {
    debug("Socket wait fd=%d timeout=%ld", sock ? sock->fd : -1, timeout_ms);
    
    if (!sock) {
        warn("NULL socket in ptk_socket_wait");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    threadlet_t *current = threadlet_get_current();
    if (!current) {
        warn("ptk_socket_wait called outside threadlet context");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return PTK_ERR_INVALID_STATE;
    }
    
    ptk_time_ms deadline = (timeout_ms > 0) ? ptk_now_ms() + timeout_ms : 0;
    
    while (true) {
        if (deadline > 0 && ptk_now_ms() >= deadline) {
            ptk_set_err(PTK_ERR_TIMEOUT);
            return PTK_ERR_TIMEOUT;
        }
        
        ptk_err err = ptk_threadlet_yield();
        if (err != PTK_OK) {
            return err;
        }
        
        err = ptk_get_err();
        if (err == PTK_ERR_SIGNAL) {
            return PTK_ERR_SIGNAL;
        }
    }
}

ptk_err ptk_socket_signal(ptk_sock *sock) {
    debug("Signaling socket fd=%d", sock ? sock->fd : -1);
    
    if (!sock) {
        warn("NULL socket in ptk_socket_signal");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    ptk_set_err(PTK_ERR_SIGNAL);
    
    if (sock->event_loop) {
        platform_event_loop_wake(sock->event_loop->platform);
    }
    
    return PTK_OK;
}

ptk_sock *ptk_tcp_socket_connect(const ptk_address_t *remote_addr, ptk_duration_ms timeout_ms) {
    info("Connecting TCP socket with timeout=%ld", timeout_ms);
    
    if (!remote_addr) {
        warn("NULL remote address");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    ptk_sock *sock = create_socket(PTK_SOCK_TCP_CLIENT, AF_INET, SOCK_STREAM, 0);
    if (!sock) {
        return NULL;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = remote_addr->ip;
    addr.sin_port = htons(remote_addr->port);
    
    int result = connect(sock->fd, (struct sockaddr*)&addr, sizeof(addr));
    if (result == 0) {
        info("TCP connection established immediately");
        return sock;
    }
    
    if (errno != EINPROGRESS) {
        error("connect() failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        ptk_free(&sock);
        return NULL;
    }
    
    debug("TCP connection in progress, waiting for completion");
    ptk_err err = wait_for_io(sock, PTK_EVENT_WRITE, timeout_ms);
    if (err != PTK_OK) {
        warn("TCP connect wait failed: %d", err);
        ptk_free(&sock);
        return NULL;
    }
    
    int sock_err = 0;
    socklen_t err_len = sizeof(sock_err);
    if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &sock_err, &err_len) < 0) {
        error("getsockopt SO_ERROR failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        ptk_free(&sock);
        return NULL;
    }
    
    if (sock_err != 0) {
        error("TCP connect failed: %s", strerror(sock_err));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        ptk_free(&sock);
        return NULL;
    }
    
    info("TCP connection established");
    return sock;
}

ptk_buf *ptk_tcp_socket_recv(ptk_sock *sock, bool wait_for_data, ptk_duration_ms timeout_ms) {
    debug("TCP recv fd=%d wait=%d timeout=%ld", sock ? sock->fd : -1, wait_for_data, timeout_ms);
    
    if (!sock || sock->type != PTK_SOCK_TCP_CLIENT) {
        warn("Invalid socket for TCP recv");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    uint8_t buffer[8192];
    ssize_t bytes_read;
    
    while (true) {
        bytes_read = recv(sock->fd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (bytes_read > 0) {
            debug("TCP received %zd bytes", bytes_read);
            return ptk_buf_alloc_from_data(buffer, (buf_size_t)bytes_read);
        }
        
        if (bytes_read == 0) {
            warn("TCP connection closed by peer");
            ptk_set_err(PTK_ERR_NETWORK_ERROR);
            return NULL;
        }
        
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            error("recv() failed: %s", strerror(errno));
            ptk_set_err(PTK_ERR_NETWORK_ERROR);
            return NULL;
        }
        
        ptk_err err = wait_for_io(sock, PTK_EVENT_READ, timeout_ms);
        if (err != PTK_OK) {
            warn("TCP recv wait failed: %d", err);
            return NULL;
        }
    }
}

ptk_err ptk_tcp_socket_send(ptk_sock *sock, ptk_buf_array_t *data_array, ptk_duration_ms timeout_ms) {
    debug("TCP send fd=%d timeout=%ld", sock ? sock->fd : -1, timeout_ms);
    
    if (!sock || sock->type != PTK_SOCK_TCP_CLIENT || !data_array) {
        warn("Invalid arguments for TCP send");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    size_t array_len = ptk_buf_array_len(data_array);
    
    for (size_t i = 0; i < array_len; i++) {
        ptk_buf *buf = ptk_buf_array_get(data_array, i);
        if (!buf) continue;
        
        uint8_t *data = buf->data + buf->start;
        size_t remaining = ptk_buf_get_len(buf);
        
        while (remaining > 0) {
            ssize_t bytes_sent = send(sock->fd, data, remaining, MSG_DONTWAIT | MSG_NOSIGNAL);
            if (bytes_sent > 0) {
                data += bytes_sent;
                remaining -= bytes_sent;
                continue;
            }
            
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                error("send() failed: %s", strerror(errno));
                ptk_set_err(PTK_ERR_NETWORK_ERROR);
                return PTK_ERR_NETWORK_ERROR;
            }
            
            ptk_err err = wait_for_io(sock, PTK_EVENT_WRITE, timeout_ms);
            if (err != PTK_OK) {
                warn("TCP send wait failed: %d", err);
                return err;
            }
        }
    }
    
    debug("TCP send completed successfully");
    return PTK_OK;
}