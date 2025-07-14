/**
 * @file ptk_sock_linux.c
 * @brief Linux implementation of simplified socket API using epoll
 *
 * This implementation uses one thread per socket with epoll for event handling.
 * Each socket has a dedicated thread that runs pseudo-blocking operations using
 * epoll underneath for efficient I/O multiplexing.
 */

#include <ptk_sock.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_os_thread.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

// Socket structure
struct ptk_sock {
    int fd;                             // Socket file descriptor
    int epoll_fd;                      // Epoll instance for this socket
    int signal_fd;                     // eventfd for external signaling
    int abort_fd;                      // eventfd for abort operations
    ptk_sock_type type;                // Socket type
    ptk_thread *dedicated_thread;      // Dedicated thread for this socket
    ptk_socket_thread_func user_func;  // User thread function
    ptk_shared_handle_t shared_context; // Shared context handle
    volatile bool should_stop;         // Thread stop flag
    ptk_address_t local_addr;         // Local address (for servers)
    ptk_address_t remote_addr;        // Remote address (for clients)
};

// Thread context for socket operations
typedef struct {
    ptk_sock *socket;
    ptk_socket_thread_func user_func;
    ptk_shared_handle_t shared_context;
} socket_thread_context_t;

// Thread context for server accept loop
typedef struct {
    ptk_sock *server_socket;
    int listen_fd;
    ptk_socket_thread_func client_thread_func;
    ptk_shared_handle_t shared_context;
} server_accept_context_t;

// Helper function to set socket non-blocking
static ptk_err set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        error("fcntl F_GETFL failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        error("fcntl F_SETFL failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    return PTK_OK;
}

// Helper function to create epoll instance and add socket
static ptk_err setup_epoll(ptk_sock *sock) {
    sock->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (sock->epoll_fd == -1) {
        error("epoll_create1 failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Create signal eventfd
    sock->signal_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (sock->signal_fd == -1) {
        error("eventfd for signal failed: %s", strerror(errno));
        close(sock->epoll_fd);
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Create abort eventfd
    sock->abort_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (sock->abort_fd == -1) {
        error("eventfd for abort failed: %s", strerror(errno));
        close(sock->signal_fd);
        close(sock->epoll_fd);
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Add socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET; // Edge-triggered
    ev.data.fd = sock->fd;
    if (epoll_ctl(sock->epoll_fd, EPOLL_CTL_ADD, sock->fd, &ev) == -1) {
        error("epoll_ctl ADD socket failed: %s", strerror(errno));
        close(sock->abort_fd);
        close(sock->signal_fd);
        close(sock->epoll_fd);
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Add signal eventfd to epoll
    ev.events = EPOLLIN;
    ev.data.fd = sock->signal_fd;
    if (epoll_ctl(sock->epoll_fd, EPOLL_CTL_ADD, sock->signal_fd, &ev) == -1) {
        error("epoll_ctl ADD signal_fd failed: %s", strerror(errno));
        close(sock->abort_fd);
        close(sock->signal_fd);
        close(sock->epoll_fd);
        return PTK_ERR_NETWORK_ERROR;
    }
    
    // Add abort eventfd to epoll
    ev.events = EPOLLIN;
    ev.data.fd = sock->abort_fd;
    if (epoll_ctl(sock->epoll_fd, EPOLL_CTL_ADD, sock->abort_fd, &ev) == -1) {
        error("epoll_ctl ADD abort_fd failed: %s", strerror(errno));
        close(sock->abort_fd);
        close(sock->signal_fd);
        close(sock->epoll_fd);
        return PTK_ERR_NETWORK_ERROR;
    }
    
    return PTK_OK;
}

// Socket destructor
static void socket_destructor(void *ptr) {
    ptk_sock *sock = (ptk_sock *)ptr;
    if (!sock) return;
    
    info("Destroying socket");
    
    // Signal thread to stop
    sock->should_stop = true;
    if (sock->abort_fd >= 0) {
        uint64_t val = 1;
        (void)write(sock->abort_fd, &val, sizeof(val));
    }
    
    // Wait for thread to finish
    if (sock->dedicated_thread) {
        ptk_thread_join(sock->dedicated_thread);
    }
    
    // Close file descriptors
    if (sock->fd >= 0) close(sock->fd);
    if (sock->epoll_fd >= 0) close(sock->epoll_fd);
    if (sock->signal_fd >= 0) close(sock->signal_fd);
    if (sock->abort_fd >= 0) close(sock->abort_fd);
    
    // Release shared context
    if (PTK_SHARED_IS_VALID(sock->shared_context)) {
        ptk_shared_release(sock->shared_context);
    }
}

// Helper to wait for socket events
static ptk_err wait_for_events(ptk_sock *sock, uint32_t events, ptk_duration_ms timeout_ms) {
    struct epoll_event epoll_events[8];
    int timeout = (timeout_ms == 0) ? -1 : (int)timeout_ms;
    
    int nfds = epoll_wait(sock->epoll_fd, epoll_events, 8, timeout);
    if (nfds == -1) {
        if (errno == EINTR) {
            return PTK_ERR_INTERRUPT;
        }
        error("epoll_wait failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    if (nfds == 0) {
        return PTK_ERR_TIMEOUT;
    }
    
    // Check for abort or signal
    for (int i = 0; i < nfds; i++) {
        if (epoll_events[i].data.fd == sock->abort_fd) {
            return PTK_ERR_ABORT;
        }
        if (epoll_events[i].data.fd == sock->signal_fd) {
            return PTK_ERR_SIGNAL;
        }
        if (epoll_events[i].data.fd == sock->fd && (epoll_events[i].events & events)) {
            return PTK_OK;
        }
    }
    
    return PTK_ERR_WOULD_BLOCK;
}

// Socket thread function
static void socket_thread_main(void *context) {
    socket_thread_context_t *ctx = (socket_thread_context_t *)context;
    ptk_sock *sock = ctx->socket;
    
    info("Socket thread started for fd=%d", sock->fd);
    
    // Call user function with socket and shared context
    ctx->user_func(sock, ctx->shared_context);
    
    info("Socket thread finished for fd=%d", sock->fd);
    
    // Clean up context
    ptk_local_free(&ctx);
}

// Create socket with thread
static ptk_sock *create_socket_with_thread(int fd, ptk_sock_type type, 
                                          ptk_socket_thread_func thread_func,
                                          ptk_shared_handle_t shared_context) {
    ptk_sock *sock = ptk_local_alloc(sizeof(ptk_sock), socket_destructor);
    if (!sock) {
        error("Failed to allocate socket structure");
        return NULL;
    }
    
    sock->fd = fd;
    sock->epoll_fd = -1;
    sock->signal_fd = -1;
    sock->abort_fd = -1;
    sock->type = type;
    sock->dedicated_thread = NULL;
    sock->user_func = thread_func;
    sock->shared_context = shared_context;
    sock->should_stop = false;
    
    // Set socket non-blocking
    if (set_nonblocking(fd) != PTK_OK) {
        ptk_local_free(&sock);
        return NULL;
    }
    
    // Setup epoll
    if (setup_epoll(sock) != PTK_OK) {
        ptk_local_free(&sock);
        return NULL;
    }
    
    // Create thread context
    socket_thread_context_t *thread_ctx = ptk_local_alloc(sizeof(socket_thread_context_t), NULL);
    if (!thread_ctx) {
        error("Failed to allocate thread context");
        ptk_local_free(&sock);
        return NULL;
    }
    
    thread_ctx->socket = sock;
    thread_ctx->user_func = thread_func;
    thread_ctx->shared_context = shared_context;
    
    // Create dedicated thread
    sock->dedicated_thread = ptk_thread_create(socket_thread_main, thread_ctx);
    if (!sock->dedicated_thread) {
        error("Failed to create socket thread");
        ptk_local_free(&thread_ctx);
        ptk_local_free(&sock);
        return NULL;
    }
    
    return sock;
}

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

ptk_sock *ptk_tcp_connect(const ptk_address_t *remote_addr,
                         ptk_socket_thread_func thread_func,
                         ptk_shared_handle_t shared_context) {
    if (!remote_addr || !thread_func) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Create TCP socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        error("socket creation failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Set up address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = remote_addr->ip;
    addr.sin_port = htons(remote_addr->port);
    
    // Connect (this will be non-blocking after socket creation)
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if (errno != EINPROGRESS) {
            error("connect failed: %s", strerror(errno));
            close(fd);
            ptk_set_err(PTK_ERR_CONNECTION_REFUSED);
            return NULL;
        }
    }
    
    // Create socket with thread
    ptk_sock *sock = create_socket_with_thread(fd, PTK_SOCK_TCP_CLIENT, thread_func, shared_context);
    if (!sock) {
        close(fd);
        return NULL;
    }
    
    sock->remote_addr = *remote_addr;
    
    info("TCP connection initiated to %s:%d", 
         inet_ntoa(*(struct in_addr*)&remote_addr->ip), remote_addr->port);
    
    return sock;
}

// Server accept thread function
static void server_accept_thread_main(void *context) {
    server_accept_context_t *ctx = (server_accept_context_t *)context;
    ptk_sock *server_sock = ctx->server_socket;
    int listen_fd = ctx->listen_fd;
    
    info("TCP server accept thread started for fd=%d", listen_fd);
    
    // Accept loop
    while (!server_sock->should_stop) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                // Check if we should stop
                if (server_sock->should_stop) break;
                continue;
            }
            error("accept failed: %s", strerror(errno));
            break;
        }
        
        // Check if we should stop after accepting
        if (server_sock->should_stop) {
            close(client_fd);
            break;
        }
        
        info("Accepted connection from %s:%d", 
             inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Create socket with thread for client
        ptk_sock *client_sock = create_socket_with_thread(client_fd, PTK_SOCK_TCP_CLIENT, 
                                                         ctx->client_thread_func, ctx->shared_context);
        if (!client_sock) {
            error("Failed to create client socket");
            close(client_fd);
            continue;
        }
        
        // Set client addresses
        client_sock->remote_addr.ip = client_addr.sin_addr.s_addr;
        client_sock->remote_addr.port = ntohs(client_addr.sin_port);
        client_sock->local_addr = server_sock->local_addr;
    }
    
    close(listen_fd);
    info("TCP server accept thread stopped");
    
    // Clean up context
    ptk_local_free(&ctx);
}

ptk_sock *ptk_tcp_server_start(const ptk_address_t *local_addr,
                              ptk_socket_thread_func thread_func,
                              ptk_shared_handle_t shared_context) {
    if (!local_addr || !thread_func) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Create listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        error("socket creation failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Set SO_REUSEADDR
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        warn("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
    }
    
    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = local_addr->ip;
    addr.sin_port = htons(local_addr->port);
    
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        error("bind failed: %s", strerror(errno));
        close(listen_fd);
        ptk_set_err(PTK_ERR_ADDRESS_IN_USE);
        return NULL;
    }
    
    // Listen
    if (listen(listen_fd, 128) == -1) {
        error("listen failed: %s", strerror(errno));
        close(listen_fd);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    info("TCP server listening on %s:%d", 
         inet_ntoa(*(struct in_addr*)&local_addr->ip), local_addr->port);
    
    // Create server socket structure (not using create_socket_with_thread since this is different)
    ptk_sock *server_sock = ptk_local_alloc(sizeof(ptk_sock), socket_destructor);
    if (!server_sock) {
        error("Failed to allocate server socket structure");
        close(listen_fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    server_sock->fd = listen_fd;
    server_sock->epoll_fd = -1;
    server_sock->signal_fd = -1;
    server_sock->abort_fd = -1;
    server_sock->type = PTK_SOCK_TCP_SERVER;
    server_sock->dedicated_thread = NULL;
    server_sock->user_func = thread_func;
    server_sock->shared_context = shared_context;
    server_sock->should_stop = false;
    server_sock->local_addr = *local_addr;
    
    // Create accept thread context
    server_accept_context_t *accept_ctx = ptk_local_alloc(sizeof(server_accept_context_t), NULL);
    if (!accept_ctx) {
        error("Failed to allocate server accept context");
        ptk_local_free(&server_sock);
        return NULL;
    }
    
    accept_ctx->server_socket = server_sock;
    accept_ctx->listen_fd = listen_fd;
    accept_ctx->client_thread_func = thread_func;
    accept_ctx->shared_context = shared_context;
    
    // Create accept thread
    server_sock->dedicated_thread = ptk_thread_create(server_accept_thread_main, accept_ctx);
    if (!server_sock->dedicated_thread) {
        error("Failed to create server accept thread");
        ptk_local_free(&accept_ctx);
        ptk_local_free(&server_sock);
        return NULL;
    }
    
    return server_sock;
}

ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr, 
                               bool broadcast,
                               ptk_socket_thread_func thread_func,
                               ptk_shared_handle_t shared_context) {
    if (!thread_func) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Create UDP socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        error("UDP socket creation failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Enable broadcast if requested
    if (broadcast) {
        int broadcast_enable = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1) {
            error("setsockopt SO_BROADCAST failed: %s", strerror(errno));
            close(fd);
            ptk_set_err(PTK_ERR_NETWORK_ERROR);
            return NULL;
        }
    }
    
    // Bind if local address provided
    if (local_addr) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = local_addr->ip;
        addr.sin_port = htons(local_addr->port);
        
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            error("UDP bind failed: %s", strerror(errno));
            close(fd);
            ptk_set_err(PTK_ERR_ADDRESS_IN_USE);
            return NULL;
        }
        
        info("UDP socket bound to %s:%d", 
             inet_ntoa(*(struct in_addr*)&local_addr->ip), local_addr->port);
    }
    
    // Create socket with thread
    ptk_sock *sock = create_socket_with_thread(fd, PTK_SOCK_UDP, thread_func, shared_context);
    if (!sock) {
        close(fd);
        return NULL;
    }
    
    if (local_addr) {
        sock->local_addr = *local_addr;
    }
    
    return sock;
}

//=============================================================================
// SOCKET I/O FUNCTIONS
//=============================================================================

ptk_err ptk_tcp_socket_send(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms) {
    if (!sock || !data || sock->type != PTK_SOCK_TCP_CLIENT) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    const char *buf_data = (const char *)(data->data + data->start);
    size_t buf_size = ptk_buf_get_len(data);
    size_t sent = 0;
    
    while (sent < buf_size) {
        // Wait for socket to be writable
        ptk_err wait_result = wait_for_events(sock, EPOLLOUT, timeout_ms);
        if (wait_result != PTK_OK) {
            return wait_result;
        }
        
        ssize_t result = send(sock->fd, buf_data + sent, buf_size - sent, MSG_NOSIGNAL);
        if (result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Try again
            }
            error("send failed: %s", strerror(errno));
            return PTK_ERR_NETWORK_ERROR;
        }
        
        sent += result;
    }
    
    return PTK_OK;
}

ptk_buf *ptk_tcp_socket_recv(ptk_sock *sock, ptk_duration_ms timeout_ms) {
    if (!sock || sock->type != PTK_SOCK_TCP_CLIENT) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Wait for data to be available
    ptk_err wait_result = wait_for_events(sock, EPOLLIN, timeout_ms);
    if (wait_result != PTK_OK) {
        ptk_set_err(wait_result);
        return NULL;
    }
    
    // Allocate buffer for received data
    char temp_buf[8192];
    ssize_t received = recv(sock->fd, temp_buf, sizeof(temp_buf), 0);
    if (received == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ptk_set_err(PTK_ERR_WOULD_BLOCK);
            return NULL;
        }
        error("recv failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    if (received == 0) {
        // Connection closed
        ptk_set_err(PTK_ERR_CLOSED);
        return NULL;
    }
    
    // Create buffer with received data
    ptk_buf *result = ptk_buf_alloc_from_data((const uint8_t *)temp_buf, received);
    if (!result) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    return result;
}

ptk_err ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, 
                               bool broadcast, ptk_duration_ms timeout_ms) {
    if (!sock || !data || !dest_addr || sock->type != PTK_SOCK_UDP) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    // Enable/disable broadcast for this send if needed
    if (broadcast) {
        int broadcast_enable = 1;
        if (setsockopt(sock->fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1) {
            error("setsockopt SO_BROADCAST failed: %s", strerror(errno));
            return PTK_ERR_NETWORK_ERROR;
        }
    }
    
    // Wait for socket to be writable
    ptk_err wait_result = wait_for_events(sock, EPOLLOUT, timeout_ms);
    if (wait_result != PTK_OK) {
        return wait_result;
    }
    
    // Set up destination address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = dest_addr->ip;
    addr.sin_port = htons(dest_addr->port);
    
    const char *buf_data = (const char *)(data->data + data->start);
    size_t buf_size = ptk_buf_get_len(data);
    
    ssize_t sent = sendto(sock->fd, buf_data, buf_size, 0, 
                         (struct sockaddr *)&addr, sizeof(addr));
    if (sent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PTK_ERR_WOULD_BLOCK;
        }
        error("sendto failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    if ((size_t)sent != buf_size) {
        warn("Partial UDP send: %zd of %zu bytes", sent, buf_size);
        return PTK_ERR_NETWORK_ERROR;
    }
    
    return PTK_OK;
}

ptk_buf *ptk_udp_socket_recv_from(ptk_sock *sock, ptk_address_t *sender_addr, ptk_duration_ms timeout_ms) {
    if (!sock || sock->type != PTK_SOCK_UDP) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // If timeout is 0, loop to get all available packets
    if (timeout_ms == 0) {
        // TODO: Implement loop to collect all available packets
        // For now, just do a single receive with no timeout
        timeout_ms = 0; // Infinite timeout for single packet
    }
    
    // Wait for data to be available
    ptk_err wait_result = wait_for_events(sock, EPOLLIN, timeout_ms);
    if (wait_result != PTK_OK) {
        ptk_set_err(wait_result);
        return NULL;
    }
    
    // Receive packet
    char temp_buf[65536]; // Max UDP packet size
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t received = recvfrom(sock->fd, temp_buf, sizeof(temp_buf), 0,
                               (struct sockaddr *)&from_addr, &from_len);
    if (received == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ptk_set_err(PTK_ERR_WOULD_BLOCK);
            return NULL;
        }
        error("recvfrom failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Fill sender address if requested
    if (sender_addr) {
        sender_addr->ip = from_addr.sin_addr.s_addr;
        sender_addr->port = ntohs(from_addr.sin_port);
        sender_addr->family = AF_INET;
        sender_addr->reserved = 0;
    }
    
    // Create buffer with received data
    ptk_buf *result = ptk_buf_alloc_from_data((const uint8_t *)temp_buf, received);
    if (!result) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    return result;
}

//=============================================================================
// SOCKET CONTROL FUNCTIONS
//=============================================================================

ptk_err ptk_socket_abort(ptk_sock *sock) {
    if (!sock) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    // Signal abort via eventfd
    uint64_t val = 1;
    if (write(sock->abort_fd, &val, sizeof(val)) == -1) {
        error("Failed to signal abort: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    return PTK_OK;
}

ptk_err ptk_socket_signal(ptk_sock *sock) {
    if (!sock) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    // Signal via eventfd
    uint64_t val = 1;
    if (write(sock->signal_fd, &val, sizeof(val)) == -1) {
        error("Failed to signal socket: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    return PTK_OK;
}

ptk_err ptk_socket_wait(ptk_sock *sock, ptk_duration_ms timeout_ms) {
    if (!sock) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    // Wait for any events (socket, signal, or abort)
    return wait_for_events(sock, EPOLLIN | EPOLLOUT, timeout_ms);
}

void ptk_socket_close(ptk_sock *socket) {
    if (!socket) return;
    
    info("Closing socket fd=%d", socket->fd);
    
    // This will trigger the destructor which handles cleanup
    ptk_local_free(&socket);
}