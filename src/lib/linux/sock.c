/**
 * @file sock.c
 * @brief Linux implementation of socket API with thread signal integration
 *
 * This implementation provides socket operations that can be interrupted by
 * abort signals and automatically transfers socket ownership between threads.
 */

#include <ptk_sock.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_os_thread.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

// Socket structure with ownership tracking
struct ptk_sock {
    int fd;                             // Socket file descriptor
    ptk_sock_type type;                // Socket type
    ptk_thread_handle_t owner_thread;  // Thread that owns this socket
    ptk_address_t local_addr;          // Local address (for servers)
    ptk_address_t remote_addr;         // Remote address (for clients)
};

// Helper function to set socket non-blocking
static ptk_err_t set_nonblocking(int fd) {
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

// Transfer socket ownership to current thread
static ptk_err_t transfer_socket_ownership(ptk_sock *sock) {
    ptk_thread_handle_t current_thread = ptk_thread_self();
    
    if (ptk_shared_handle_equal(sock->owner_thread, current_thread)) {
        // Already owned by current thread
        return PTK_OK;
    }
    
    int epoll_fd = ptk_thread_get_epoll_fd();
    int signal_fd = ptk_thread_get_signal_fd();
    
    if (epoll_fd == -1 || signal_fd == -1) {
        error("Current thread does not have event system");
        return PTK_ERR_INVALID_STATE;
    }
    
    // Remove from previous thread's epoll (if any)
    // Note: This is a simplification - in a full implementation, we'd need
    // to coordinate with the previous thread's epoll instance
    
    // Add socket to current thread's epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = sock->fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock->fd, &ev) == -1) {
        if (errno != EEXIST) {
            error("epoll_ctl ADD socket failed: %s", strerror(errno));
            return PTK_ERR_NETWORK_ERROR;
        }
        // If socket already exists, modify it
        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock->fd, &ev) == -1) {
            error("epoll_ctl MOD socket failed: %s", strerror(errno));
            return PTK_ERR_NETWORK_ERROR;
        }
    }
    
    sock->owner_thread = current_thread;
    debug("Transferred socket fd=%d to current thread", sock->fd);
    
    return PTK_OK;
}

// Wait for socket events with abort signal checking
static ptk_err_t wait_for_socket_events(ptk_sock *sock, uint32_t events, ptk_duration_ms timeout_ms) {
    int epoll_fd = ptk_thread_get_epoll_fd();
    int signal_fd = ptk_thread_get_signal_fd();
    
    if (epoll_fd == -1 || signal_fd == -1) {
        error("Current thread does not have event system");
        return PTK_ERR_INVALID_STATE;
    }
    
    // Check for pending abort signals first
    if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT_MASK)) {
        return PTK_ERR_SIGNAL;
    }
    
    struct epoll_event epoll_events[8];
    int timeout = (timeout_ms == 0) ? -1 : (int)timeout_ms;
    
    int nfds = epoll_wait(epoll_fd, epoll_events, 8, timeout);
    if (nfds == -1) {
        if (errno == EINTR) {
            // Check for abort signals
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT_MASK)) {
                return PTK_ERR_SIGNAL;
            }
            return PTK_ERR_INTERRUPT;
        }
        error("epoll_wait failed: %s", strerror(errno));
        return PTK_ERR_NETWORK_ERROR;
    }
    
    if (nfds == 0) {
        return PTK_ERR_TIMEOUT;
    }
    
    // Check events
    for (int i = 0; i < nfds; i++) {
        if (epoll_events[i].data.fd == signal_fd) {
            // Signal received, drain the eventfd
            uint64_t val;
            (void)read(signal_fd, &val, sizeof(val));
            
            // Check for abort signals
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT_MASK)) {
                return PTK_ERR_SIGNAL;
            }
        }
        
        if (epoll_events[i].data.fd == sock->fd) {
            if (epoll_events[i].events & events) {
                return PTK_OK;
            }
        }
    }
    
    return PTK_ERR_WOULD_BLOCK;
}

// Socket destructor
static void socket_destructor(void *ptr) {
    ptk_sock *sock = (ptk_sock *)ptr;
    if (!sock) return;
    
    debug("Destroying socket fd=%d", sock->fd);
    
    // Remove from epoll if we're in the owning thread
    if (ptk_shared_handle_equal(sock->owner_thread, ptk_thread_self())) {
        int epoll_fd = ptk_thread_get_epoll_fd();
        if (epoll_fd >= 0) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock->fd, NULL);
        }
    }
    
    // Close socket
    if (sock->fd >= 0) {
        close(sock->fd);
    }
}

//=============================================================================
// PUBLIC API IMPLEMENTATION
//=============================================================================

ptk_sock *ptk_tcp_connect(const ptk_address_t *remote_addr, ptk_duration_ms connect_timeout_ms) {
    if (!remote_addr) {
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
    
    // Set non-blocking
    if (set_nonblocking(fd) != PTK_OK) {
        close(fd);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Create socket structure
    ptk_sock *sock = ptk_local_alloc(sizeof(ptk_sock), socket_destructor);
    if (!sock) {
        error("Failed to allocate socket structure");
        close(fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    sock->fd = fd;
    sock->type = PTK_SOCK_TCP_CLIENT;
    sock->owner_thread = ptk_thread_self();
    sock->remote_addr = *remote_addr;
    
    // Transfer ownership to current thread (adds to epoll)
    if (transfer_socket_ownership(sock) != PTK_OK) {
        ptk_local_free(&sock);
        return NULL;
    }
    
    // Set up address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = remote_addr->ip;
    addr.sin_port = htons(remote_addr->port);
    
    // Attempt connection
    int connect_result = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (connect_result == -1 && errno != EINPROGRESS) {
        error("connect failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_CONNECTION_REFUSED);
        ptk_local_free(&sock);
        return NULL;
    }
    
    // Wait for connection completion if needed
    if (connect_result == -1) {
        ptk_err_t wait_result = wait_for_socket_events(sock, EPOLLOUT, connect_timeout_ms);
        if (wait_result != PTK_OK) {
            ptk_set_err(wait_result);
            ptk_local_free(&sock);
            return NULL;
        }
        
        // Check if connection succeeded
        int error_code;
        socklen_t error_len = sizeof(error_code);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error_code, &error_len) == -1) {
            error("getsockopt SO_ERROR failed: %s", strerror(errno));
            ptk_set_err(PTK_ERR_NETWORK_ERROR);
            ptk_local_free(&sock);
            return NULL;
        }
        
        if (error_code != 0) {
            error("connect failed: %s", strerror(error_code));
            ptk_set_err(PTK_ERR_CONNECTION_REFUSED);
            ptk_local_free(&sock);
            return NULL;
        }
    }
    
    info("TCP connected to %s:%d", 
         inet_ntoa(*(struct in_addr*)&remote_addr->ip), remote_addr->port);
    
    return sock;
}

ptk_sock *ptk_tcp_server_create(const ptk_address_t *local_addr) {
    if (!local_addr) {
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
    
    // Set non-blocking
    if (set_nonblocking(listen_fd) != PTK_OK) {
        close(listen_fd);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
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
    
    // Create server socket structure
    ptk_sock *server_sock = ptk_local_alloc(sizeof(ptk_sock), socket_destructor);
    if (!server_sock) {
        error("Failed to allocate server socket structure");
        close(listen_fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    server_sock->fd = listen_fd;
    server_sock->type = PTK_SOCK_TCP_SERVER;
    server_sock->owner_thread = ptk_thread_self();
    server_sock->local_addr = *local_addr;
    
    // Transfer ownership to current thread
    if (transfer_socket_ownership(server_sock) != PTK_OK) {
        ptk_local_free(&server_sock);
        return NULL;
    }
    
    info("TCP server listening on %s:%d", 
         inet_ntoa(*(struct in_addr*)&local_addr->ip), local_addr->port);
    
    return server_sock;
}

ptk_sock *ptk_tcp_accept(ptk_sock *server_sock, ptk_address_t *client_addr, ptk_duration_ms timeout_ms) {
    if (!server_sock || server_sock->type != PTK_SOCK_TCP_SERVER) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Transfer ownership if needed
    if (transfer_socket_ownership(server_sock) != PTK_OK) {
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Wait for incoming connection
    ptk_err_t wait_result = wait_for_socket_events(server_sock, EPOLLIN, timeout_ms);
    if (wait_result != PTK_OK) {
        ptk_set_err(wait_result);
        return NULL;
    }
    
    // Accept connection
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int client_fd = accept(server_sock->fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ptk_set_err(PTK_ERR_WOULD_BLOCK);
            return NULL;
        }
        error("accept failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Fill client address if requested
    if (client_addr) {
        client_addr->ip = addr.sin_addr.s_addr;
        client_addr->port = ntohs(addr.sin_port);
        client_addr->family = AF_INET;
        client_addr->reserved = 0;
    }
    
    // Set non-blocking
    if (set_nonblocking(client_fd) != PTK_OK) {
        close(client_fd);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Create client socket structure
    ptk_sock *client_sock = ptk_local_alloc(sizeof(ptk_sock), socket_destructor);
    if (!client_sock) {
        error("Failed to allocate client socket structure");
        close(client_fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    client_sock->fd = client_fd;
    client_sock->type = PTK_SOCK_TCP_CLIENT;
    client_sock->owner_thread = ptk_thread_self();
    client_sock->local_addr = server_sock->local_addr;
    client_sock->remote_addr.ip = addr.sin_addr.s_addr;
    client_sock->remote_addr.port = ntohs(addr.sin_port);
    client_sock->remote_addr.family = AF_INET;
    client_sock->remote_addr.reserved = 0;
    
    // Transfer ownership to current thread
    if (transfer_socket_ownership(client_sock) != PTK_OK) {
        ptk_local_free(&client_sock);
        return NULL;
    }
    
    info("Accepted connection from %s:%d", 
         inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    
    return client_sock;
}

ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr, bool broadcast) {
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
    
    // Set non-blocking
    if (set_nonblocking(fd) != PTK_OK) {
        close(fd);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
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
    
    // Create socket structure
    ptk_sock *sock = ptk_local_alloc(sizeof(ptk_sock), socket_destructor);
    if (!sock) {
        error("Failed to allocate socket structure");
        close(fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    sock->fd = fd;
    sock->type = PTK_SOCK_UDP;
    sock->owner_thread = ptk_thread_self();
    
    if (local_addr) {
        sock->local_addr = *local_addr;
    }
    
    // Transfer ownership to current thread
    if (transfer_socket_ownership(sock) != PTK_OK) {
        ptk_local_free(&sock);
        return NULL;
    }
    
    return sock;
}

ptk_err_t ptk_tcp_socket_send(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms) {
    if (!sock || !data || sock->type != PTK_SOCK_TCP_CLIENT) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    // Transfer ownership if needed
    if (transfer_socket_ownership(sock) != PTK_OK) {
        return PTK_ERR_NETWORK_ERROR;
    }
    
    const char *buf_data = (const char *)(data->data + data->start);
    size_t buf_size = ptk_buf_get_len(data);
    size_t sent = 0;
    
    while (sent < buf_size) {
        // Wait for socket to be writable
        ptk_err_t wait_result = wait_for_socket_events(sock, EPOLLOUT, timeout_ms);
        if (wait_result != PTK_OK) {
            return wait_result;
        }
        
        ssize_t result = send(sock->fd, buf_data + sent, buf_size - sent, MSG_NOSIGNAL);
        if (result == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Try again
            }
            if (errno == EINTR) {
                // Check for abort signals
                if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT_MASK)) {
                    return PTK_ERR_SIGNAL;
                }
                continue;
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
    
    // Transfer ownership if needed
    if (transfer_socket_ownership(sock) != PTK_OK) {
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Wait for data to be available
    ptk_err_t wait_result = wait_for_socket_events(sock, EPOLLIN, timeout_ms);
    if (wait_result != PTK_OK) {
        ptk_set_err(wait_result);
        return NULL;
    }
    
    // Receive data
    char temp_buf[8192];
    ssize_t received = recv(sock->fd, temp_buf, sizeof(temp_buf), 0);
    if (received == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ptk_set_err(PTK_ERR_WOULD_BLOCK);
            return NULL;
        }
        if (errno == EINTR) {
            // Check for abort signals
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT_MASK)) {
                ptk_set_err(PTK_ERR_SIGNAL);
                return NULL;
            }
            ptk_set_err(PTK_ERR_INTERRUPT);
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

ptk_err_t ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, 
                               bool broadcast, ptk_duration_ms timeout_ms) {
    if (!sock || !data || !dest_addr || sock->type != PTK_SOCK_UDP) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    // Transfer ownership if needed
    if (transfer_socket_ownership(sock) != PTK_OK) {
        return PTK_ERR_NETWORK_ERROR;
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
    ptk_err_t wait_result = wait_for_socket_events(sock, EPOLLOUT, timeout_ms);
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
        if (errno == EINTR) {
            // Check for abort signals
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT_MASK)) {
                return PTK_ERR_SIGNAL;
            }
            return PTK_ERR_INTERRUPT;
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
    
    // Transfer ownership if needed
    if (transfer_socket_ownership(sock) != PTK_OK) {
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Wait for data to be available
    ptk_err_t wait_result = wait_for_socket_events(sock, EPOLLIN, timeout_ms);
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
        if (errno == EINTR) {
            // Check for abort signals
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT_MASK)) {
                ptk_set_err(PTK_ERR_SIGNAL);
                return NULL;
            }
            ptk_set_err(PTK_ERR_INTERRUPT);
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

void ptk_socket_close(ptk_sock *socket) {
    if (!socket) return;
    
    info("Closing socket fd=%d", socket->fd);
    
    // Ensure we're in the owning thread
    if (!ptk_shared_handle_equal(socket->owner_thread, ptk_thread_self())) {
        warn("Closing socket from non-owning thread");
    }
    
    // This will trigger the destructor which handles cleanup
    ptk_local_free(&socket);
}