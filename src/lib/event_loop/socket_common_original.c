// ...existing includes...
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ptk_err.h>
#include <ptk_alloc.h>
#include <ptk_log.h>
#include "socket_common.h"
#include "event_loop.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "ptk_threadlet.h"

// Helper to set non-blocking mode
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

//=============================================================================
// Common socket operations
//=============================================================================


// Forward declaration of current threadlet (set by event loop)
static ptk_threadlet_t *current_threadlet;

/**
 * @brief Socket destructor called when ptk_free() is called on a socket
 *
 * @param ptr Pointer to the socket to destroy
 */
static void ptk_socket_destructor(void *ptr) {
    ptk_sock *sock = (ptk_sock *)ptr;
    if (!sock) return;
    
    debug("destroying socket");

    // If the current threadlet is waiting on this socket, abort it before closing fd
    if (current_threadlet && sock->event_loop && current_threadlet->status == THREADLET_WAITING) {
        event_registration_t *reg = event_registration_lookup(sock->event_loop->registrations, sock->fd);
        if (reg && reg->waiting_threadlet == current_threadlet) {
            current_threadlet->status = THREADLET_ABORTED;
            threadlet_queue_enqueue(&sock->event_loop->ready_queue, current_threadlet);
            event_registration_remove(sock->event_loop->registrations, sock->fd);
            platform_remove_fd(sock->event_loop->platform, sock->fd);
            trace("Aborted waiting threadlet for fd %d", sock->fd);
        }
    }

    if (sock->fd >= 0) {
        trace("Closing socket fd %d", sock->fd);
        // Remove from event loop if not already done
        event_registration_remove(sock->event_loop->registrations, sock->fd);
        platform_remove_fd(sock->event_loop->platform, sock->fd);
        // Shutdown first to notify peer
        shutdown(sock->fd, SHUT_RDWR);
        close(sock->fd);
        sock->fd = -1;
    }

    if (sock->type == PTK_SOCK_TCP_CLIENT || sock->type == PTK_SOCK_TCP_SERVER) {
        // Additional cleanup if needed
    }
    sock->type = PTK_SOCK_INVALID;

    debug("socket destroyed");
}

//============================================================================
// TCP Server Socket Functions
//============================================================================

/** 
 * Listen on a local address as a TCP server (blocking).
 * This function creates a TCP socket, binds it to the specified local address,
 * and starts listening for incoming connections.
 * 
 * @param local_addr Address to bind to (cannot be NULL).
 * @param backlog Maximum number of pending connections.
 * @return Valid server socket on success, NULL on failure (ptk_get_err() set).
 */
ptk_sock *ptk_tcp_socket_listen(const ptk_address_t *local_addr, int backlog) {
    debug("ptk_tcp_socket_listen: entry");
    if (!local_addr) {
        warn("ptk_tcp_socket_listen: local_addr is NULL");
        return NULL;
    }
    trace("Creating socket");
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        warn("socket() failed: %s", strerror(errno));
        return NULL;
    }
    trace("Setting non-blocking mode");
    if (set_nonblocking(fd) < 0) {
        warn("set_nonblocking() failed: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    int opt = 1;
    trace("Setting SO_REUSEADDR");
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = local_addr->ip;
    addr.sin_port = htons(local_addr->port);
    trace("Binding socket");
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        warn("bind() failed: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    trace("Listening on socket");
    if (listen(fd, backlog) < 0) {
        warn("listen() failed: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    trace("Allocating ptk_sock");
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), ptk_socket_destructor);
    if (!sock) {
        warn("ptk_alloc for ptk_sock failed");
        close(fd);
        return NULL;
    }
    sock->fd = fd;
    sock->type = PTK_SOCK_TCP_SERVER;
    // Attach to event loop if needed
    // sock->event_loop = ...
    debug("ptk_tcp_socket_listen: exit");
    return sock;
}

/**
 * Accept a new TCP connection (blocking).
 * This function accepts a new incoming connection on a listening server socket.
 * If no connection is immediately available, it will yield the current threadlet
 * and wait until a connection is ready or the timeout expires.
 * 
 * @param server Listening server socket (must be valid and of type PTK_SOCK_TCP_SERVER).
 * @param timeout_ms Maximum time to wait in milliseconds (0 for infinite).
 * @return Valid client socket on success, NULL on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_sock *ptk_tcp_socket_accept(ptk_sock *server, ptk_duration_ms timeout_ms) {
    info("ptk_tcp_socket_accept: entry");
    
    if (!server || server->type != PTK_SOCK_TCP_SERVER) {
        warn("Invalid arguments to ptk_tcp_socket_accept");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    int fd = server->fd;
    debug("Calling accept() on fd %d", fd);
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    
    if (client_fd >= 0) {
        debug("Accepted new connection, client fd %d", client_fd);
        set_nonblocking(client_fd);
        
        ptk_sock *client_sock = ptk_alloc(sizeof(ptk_sock), ptk_socket_destructor);
        if (!client_sock) {
            error("ptk_alloc for client_sock failed");
            close(client_fd);
            ptk_set_err(PTK_ERR_NO_RESOURCES);
            return NULL;
        }
        
        client_sock->fd = client_fd;
        client_sock->type = PTK_SOCK_TCP_CLIENT;
        // Attach to event loop if needed
        // client_sock->event_loop = server->event_loop;
        
        info("ptk_tcp_socket_accept: exit");
        return client_sock;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        debug("accept() would block, registering for read event");
        event_registration_add(server->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, ptk_now_ms() + timeout_ms);
        platform_add_fd(server->event_loop->platform, fd, PTK_EVENT_READ);
        timeout_heap_insert(server->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_tcp_socket_accept: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            info("ptk_tcp_socket_accept: exit");
            return NULL;
        }
        
        debug("Resuming accept after yield");
        info("ptk_tcp_socket_accept: exit");
        return ptk_tcp_socket_accept(server, timeout_ms);
    }
    
    warn("accept() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    info("ptk_tcp_socket_accept: exit");
    return NULL;
}


//=============================================================================
// TCP Client Socket Functions

/**
 * Connect to a TCP server (blocking).
 * This function initiates a connection to a remote TCP server. If the connection cannot be completed immediately,
 * it will yield the current threadlet and wait until the connection is established or the timeout expires. 
 * If the connection is successful, it returns PTK_OK. If it times out, it returns PTK_ERR_TIMEOUT.
 * If there is a network error, it returns PTK_ERR_NETWORK_ERROR.
 * @param sock TCP client socket (must be valid and of type PTK_SOCK_TCP_CLIENT).
 * @param remote_addr The remote address to connect to (cannot be NULL).
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_OK on connection, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 *         If the socket is invalid or arguments are bad, returns PTK_ERR_INVALID_ARGUMENT.
 *         If the socket is already connected, returns PTK_ERR_ALREADY_CONNECTED (not implemented here, user must track state).
 *         If the socket was aborted while waiting, returns PTK_ERR_ABORT (handled by threadlet status).
 */
ptk_err ptk_tcp_socket_connect(ptk_sock *sock, const ptk_address_t *remote_addr, ptk_duration_ms timeout_ms) {
    debug("ptk_tcp_socket_connect: entry");
    if (!sock || sock->type != PTK_SOCK_TCP_CLIENT || !remote_addr) {
        warn("Invalid arguments to ptk_tcp_socket_connect");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    int fd = sock->fd;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = remote_addr->ip;
    addr.sin_port = htons(remote_addr->port);
    trace("Calling connect() on fd %d", fd);
    int res = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (res == 0) {
        trace("Connected immediately");
        debug("ptk_tcp_socket_connect: exit");
        return PTK_OK;
    }
    if (errno == EINPROGRESS) {
        trace("connect() in progress, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_WRITE);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        if (current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_tcp_socket_connect: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("ptk_tcp_socket_connect: exit");
            return PTK_ERR_TIMEOUT;
        }
        trace("Resuming connect after yield");
        debug("ptk_tcp_socket_connect: exit");
        return ptk_tcp_socket_connect(sock, remote_addr, timeout_ms);
    }
    warn("connect() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("ptk_tcp_socket_connect: exit");
    return PTK_ERR_NETWORK_ERROR;
}


/**
 * Read data from a TCP socket (blocking).
 * 
 * This function attempts to read data from a TCP socket and returns a single buffer containing the data.
 * If data is available, it reads as much as possible and returns the buffer.
 * If no data is available, it registers the socket for read events and yields the current threadlet.
 * When the socket becomes readable or the timeout expires, it resumes and attempts to read again.
 * If the read operation times out, it returns NULL. If there is a network error, it returns NULL.
 * If the socket is invalid or arguments are bad, it returns NULL.
 * If the socket was aborted while waiting, it returns NULL (handled by threadlet status).
 * The returned buffer must be freed by the caller using ptk_free().
 * @param sock TCP client socket (must be valid and of type PTK_SOCK_TCP_CLIENT).
 * @param timeout_ms Timeout for the read operation (in milliseconds). If 0, wait indefinitely.
 * @return Buffer containing received data on success (must be freed with ptk_free()), NULL on error.
 *         On error, check ptk_get_err() for error details.
 */
ptk_buf *ptk_tcp_socket_recv(ptk_sock *sock, ptk_duration_ms timeout_ms) {
    info("ptk_tcp_socket_recv: entry");
    
    if (!sock || sock->type != PTK_SOCK_TCP_CLIENT) {
        warn("Invalid arguments to ptk_tcp_socket_recv");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    int fd = sock->fd;
    
    // Create a buffer to receive data
    ptk_buf *data = ptk_buf_alloc(4096); // Default 4KB buffer
    if (!data) {
        error("Failed to create receive buffer");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    debug("Calling recv() on fd %d", fd);
    ssize_t bytes_read = recv(fd, data->data, data->data_len, MSG_DONTWAIT);
    if (bytes_read > 0) {
        data->end = bytes_read;
        debug("Read %zd bytes", bytes_read);
        info("ptk_tcp_socket_recv: exit");
        return data;
    }
    
    if (bytes_read == 0) {
        debug("Connection closed by peer");
        ptk_free(&data);
        ptk_set_err(PTK_ERR_CLOSED);
        info("ptk_tcp_socket_recv: exit");
        return NULL;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        debug("recv() would block, registering for read event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_READ);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_tcp_socket_recv: timeout");
            ptk_free(&data);
            ptk_set_err(PTK_ERR_TIMEOUT);
            info("ptk_tcp_socket_recv: exit");
            return NULL;
        }
        
        debug("Resuming read after yield");
        ptk_free(&data); // Free the temporary buffer and try again
        info("ptk_tcp_socket_recv: exit");
        return ptk_tcp_socket_recv(sock, timeout_ms);
    }
    
    warn("recv() failed: %s", strerror(errno));
    ptk_free(&data);
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    info("ptk_tcp_socket_recv: exit");
    return NULL;
}


/**
 * Write data to a TCP socket using vectored I/O (blocking).
 * 
 * This function uses writev() to efficiently send multiple buffers in a single system call.
 * If the socket is writable, it writes as much data as possible and returns PTK_OK.
 * If the socket is not writable, it registers the socket for write events and yields the current threadlet.
 * When the socket becomes writable or the timeout expires, it resumes and attempts to write again.
 * If the write operation times out, it returns PTK_ERR_TIMEOUT. If there is a network error, it returns PTK_ERR_NETWORK_ERROR.
 * If the socket is invalid or arguments are bad, it returns PTK_ERR_INVALID_PARAM.
 * If the socket was aborted while waiting, it returns PTK_ERR_ABORT (handled by threadlet status).
 * The buffer start indices are updated to reflect the number of bytes successfully written.
 * @param sock TCP client socket (must be valid and of type PTK_SOCK_TCP_CLIENT).
 * @param data_array Array of buffers containing data to write (must be valid and have data).
 * @param timeout_ms Timeout for the write operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success (data written), PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 *         If the socket is invalid or arguments are bad, returns PTK_ERR_INVALID_PARAM.
 *         If the socket was aborted while waiting, returns PTK_ERR_ABORT (handled by threadlet status).
 *         If all data is written, each buffer's start will equal its end.
 *         If partial data is written, buffer start indices will be updated accordingly; caller should check this.
 */
ptk_err ptk_tcp_socket_send(ptk_sock *sock, ptk_buf_array_t *data_array, ptk_duration_ms timeout_ms) {
    info("ptk_tcp_socket_send: entry");
    
    if (!sock || sock->type != PTK_SOCK_TCP_CLIENT || !data_array) {
        warn("Invalid arguments to ptk_tcp_socket_send");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    size_t array_len = ptk_buf_array_len(data_array);
    if (array_len == 0) {
        debug("Empty buffer array, nothing to send");
        info("ptk_tcp_socket_send: exit");
        return PTK_OK;
    }
    
    int fd = sock->fd;
    
    // Prepare iovec array for writev()
    struct iovec *iov = ptk_alloc(array_len * sizeof(struct iovec), NULL);
    if (!iov) {
        error("Failed to allocate iovec array");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_ERR_NO_RESOURCES;
    }
    
    // Fill iovec array with buffer data
    size_t total_bytes = 0;
    for (size_t i = 0; i < array_len; i++) {
        ptk_buf *data;
        ptk_err err = ptk_buf_array_get(data_array, i, &data);
        if (err != PTK_OK || !data) {
            warn("Failed to get buffer %zu from array", i);
            iov[i].iov_base = NULL;
            iov[i].iov_len = 0;
            continue;
        }
        
        size_t remaining = data->end - data->start;
        iov[i].iov_base = data->data + data->start;
        iov[i].iov_len = remaining;
        total_bytes += remaining;
        debug("Buffer %zu: %zu bytes at offset %zu", i, remaining, data->start);
    }
    
    if (total_bytes == 0) {
        debug("No data to send");
        ptk_free(&iov);
        info("ptk_tcp_socket_send: exit");
        return PTK_OK;
    }
    
    debug("Using writev() to send %zu bytes across %zu buffers", total_bytes, array_len);
    ssize_t bytes_sent = writev(fd, iov, array_len);
    
    if (bytes_sent >= 0) {
        debug("writev() sent %zd bytes", bytes_sent);
        
        // Update buffer start indices based on bytes sent
        size_t remaining_sent = bytes_sent;
        for (size_t i = 0; i < array_len && remaining_sent > 0; i++) {
            ptk_buf *data;
            ptk_err err = ptk_buf_array_get(data_array, i, &data);
            if (err != PTK_OK || !data) continue;
            
            size_t buf_remaining = data->end - data->start;
            size_t buf_sent = (remaining_sent >= buf_remaining) ? buf_remaining : remaining_sent;
            data->start += buf_sent;
            remaining_sent -= buf_sent;
            trace("Updated buffer %zu: sent %zu bytes, new start=%zu", i, buf_sent, data->start);
        }
        
        ptk_free(&iov);
        info("ptk_tcp_socket_send: exit");
        return PTK_OK;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        debug("writev() would block, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_WRITE);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_tcp_socket_send: timeout");
            ptk_free(&iov);
            ptk_set_err(PTK_ERR_TIMEOUT);
            info("ptk_tcp_socket_send: exit");
            return PTK_ERR_TIMEOUT;
        }
        
        debug("Resuming write after yield");
        ptk_free(&iov);
        info("ptk_tcp_socket_send: exit");
        return ptk_tcp_socket_send(sock, data_array, timeout_ms);
    }
    
    warn("writev() failed: %s", strerror(errno));
    ptk_free(&iov);
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    info("ptk_tcp_socket_send: exit");
    return PTK_ERR_NETWORK_ERROR;
}

//=============================================================================
// UDP Socket Functions
//=============================================================================

/**
 * Create a UDP socket.
 * 
 * This function creates a UDP socket and binds it to the specified local address if provided.
 * If the broadcast flag is true, it enables the SO_BROADCAST option on the socket.
 * If local_addr is NULL, the socket is created without binding (for sending only).
 * @param local_addr Address to bind to (NULL for client-only).
 * @param broadcast If true, enables SO_BROADCAST option on the socket.
 * @return Valid UDP socket on success, NULL on failure (ptk_get_err() set).
 *         On failure, ptk_get_err() will be set to an appropriate error code.
 */
ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr, bool broadcast) {
    debug("ptk_udp_socket_create: entry");
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        warn("socket() failed: %s", strerror(errno));
        return NULL;
    }
    set_nonblocking(fd);
    if (broadcast) {
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
            warn("setsockopt(SO_BROADCAST) failed: %s", strerror(errno));
            close(fd);
            return NULL;
        }
        trace("SO_BROADCAST enabled on UDP socket");
    }
    if (local_addr) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = local_addr->ip;
        addr.sin_port = htons(local_addr->port);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            warn("bind() failed: %s", strerror(errno));
            close(fd);
            return NULL;
        }
    }
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), ptk_socket_destructor);
    if (!sock) {
        warn("ptk_alloc for ptk_sock failed");
        close(fd);
        return NULL;
    }
    sock->fd = fd;
    sock->type = PTK_SOCK_UDP;
    debug("ptk_udp_socket_create: exit");
    return sock;
}



/**
 * Create a UDP multicast socket (stub).
 *
 * This function should create a UDP socket, bind to INADDR_ANY and the group port,
 * and join the specified multicast group. Additional options (TTL, loopback, interface)
 * may be set as needed.
 * @param group_addr Multicast group address to join.
 * @param port Port to bind to.
 * @return Valid UDP socket on success, NULL on failure (ptk_get_err() set).
 */
ptk_sock *ptk_udp_multicast_socket_create(const char *group_addr, uint16_t port) {
    debug("ptk_udp_multicast_socket_create: entry");
    // TODO: Implement UDP multicast socket creation
    debug("ptk_udp_multicast_socket_create: exit");
    return NULL;
}


/**
 * Send UDP data to a specific address using vectored I/O (blocking).
 * This function uses sendmsg() to efficiently send multiple buffers in a single UDP packet.
 * If the socket is not writable, it registers for write events and yields the current threadlet.
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param data_array Array of buffers containing data to send (must be valid and have data).
 * @param dest_addr Destination address to send to (cannot be NULL).
 * @param broadcast If true, allows sending to broadcast addresses.
 * @param timeout_ms Timeout for the send operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 */
ptk_err ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf_array_t *data_array, const ptk_address_t *dest_addr, bool broadcast, ptk_duration_ms timeout_ms) {
    info("ptk_udp_socket_send_to: entry");
    
    if (!sock || sock->type != PTK_SOCK_UDP || !data_array || !dest_addr) {
        warn("Invalid arguments to ptk_udp_socket_send_to");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    size_t array_len = ptk_buf_array_len(data_array);
    if (array_len == 0) {
        debug("Empty buffer array, nothing to send");
        info("ptk_udp_socket_send_to: exit");
        return PTK_OK;
    }
    
    int fd = sock->fd;
    
    // Prepare iovec array for sendmsg()
    struct iovec *iov = ptk_alloc(array_len * sizeof(struct iovec), NULL);
    if (!iov) {
        error("Failed to allocate iovec array");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_ERR_NO_RESOURCES;
    }
    
    // Fill iovec array with buffer data
    size_t total_bytes = 0;
    for (size_t i = 0; i < array_len; i++) {
        ptk_buf *data;
        ptk_err err = ptk_buf_array_get(data_array, i, &data);
        if (err != PTK_OK || !data) {
            warn("Failed to get buffer %zu from array", i);
            iov[i].iov_base = NULL;
            iov[i].iov_len = 0;
            continue;
        }
        
        size_t remaining = data->end - data->start;
        iov[i].iov_base = data->data + data->start;
        iov[i].iov_len = remaining;
        total_bytes += remaining;
        debug("Buffer %zu: %zu bytes at offset %zu", i, remaining, data->start);
    }
    
    if (total_bytes == 0) {
        debug("No data to send");
        ptk_free(&iov);
        info("ptk_udp_socket_send_to: exit");
        return PTK_OK;
    }
    
    // Prepare destination address for sendmsg
    struct sockaddr_in dest_sockaddr;
    memset(&dest_sockaddr, 0, sizeof(dest_sockaddr));
    dest_sockaddr.sin_family = AF_INET;
    dest_sockaddr.sin_addr.s_addr = dest_addr->ip;
    dest_sockaddr.sin_port = htons(dest_addr->port);
    
    // Prepare msghdr for sendmsg()
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &dest_sockaddr;
    msg.msg_namelen = sizeof(dest_sockaddr);
    msg.msg_iov = iov;
    msg.msg_iovlen = array_len;
    
    debug("Using sendmsg() to send %zu bytes across %zu buffers", total_bytes, array_len);
    ssize_t bytes_sent = sendmsg(fd, &msg, MSG_DONTWAIT);
    
    if (bytes_sent >= 0) {
        debug("sendmsg() sent %zd bytes", bytes_sent);
        
        // Update buffer start indices based on bytes sent
        size_t remaining_sent = bytes_sent;
        for (size_t i = 0; i < array_len && remaining_sent > 0; i++) {
            ptk_buf *data;
            ptk_err err = ptk_buf_array_get(data_array, i, &data);
            if (err != PTK_OK || !data) continue;
            
            size_t buf_remaining = data->end - data->start;
            size_t buf_sent = (remaining_sent >= buf_remaining) ? buf_remaining : remaining_sent;
            data->start += buf_sent;
            remaining_sent -= buf_sent;
            trace("Updated buffer %zu: sent %zu bytes, new start=%zu", i, buf_sent, data->start);
        }
        
        ptk_free(&iov);
        info("ptk_udp_socket_send_to: exit");
        return PTK_OK;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        debug("sendmsg() would block, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_WRITE);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_udp_socket_send_to: timeout");
            ptk_free(&iov);
            ptk_set_err(PTK_ERR_TIMEOUT);
            info("ptk_udp_socket_send_to: exit");
            return PTK_ERR_TIMEOUT;
        }
        
        debug("Resuming sendmsg after yield");
        ptk_free(&iov);
        info("ptk_udp_socket_send_to: exit");
        return ptk_udp_socket_send_to(sock, data_array, dest_addr, broadcast, timeout_ms);
    }
    
    warn("sendmsg() failed: %s", strerror(errno));
    ptk_free(&iov);
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    info("ptk_udp_socket_send_to: exit");
    return PTK_ERR_NETWORK_ERROR;
}


/**
 * Receive UDP data from any address, returning an array of buffers (blocking).
 * 
 * This function receives UDP packets and returns them as an array of buffers.
 * It fills the sender_addr parameter with the source address of the last received packet if provided.
 * If wait_for_packets is true, it waits the entire timeout period collecting multiple packets.
 * If wait_for_packets is false, it returns as soon as any packets are available.
 * If no data is available, it registers the socket for read events and yields the current threadlet.
 * When data is available or the timeout expires, it resumes and attempts to read again.
 * If the read operation times out, it returns an empty array. If there is a network error, it returns NULL.
 * If the socket is invalid or arguments are bad, it returns NULL.
 * If the socket was aborted while waiting, it returns NULL (handled by threadlet status).
 * The returned buffer array must be freed by the caller using ptk_free().
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param sender_addr Output parameter to receive the source address of the last packet (can be NULL).
 * @param wait_for_packets If true, wait the entire timeout period and return all packets.
 *                        If false, return as soon as any packets are available.
 * @param timeout_ms Timeout for the read operation (in milliseconds). If 0, wait indefinitely.
 * @return Buffer array containing received packets on success (must be freed with ptk_free()), 
 *         NULL on error. Check ptk_get_err() for error details.
 */
ptk_buf_array_t *ptk_udp_socket_recv_from(ptk_sock *sock, ptk_address_t *sender_addr, bool wait_for_packets, ptk_duration_ms timeout_ms) {
    info("ptk_udp_socket_recv_from: entry");
    
    if (!sock || sock->type != PTK_SOCK_UDP) {
        warn("Invalid arguments to ptk_udp_socket_recv_from");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    int fd = sock->fd;
    ptk_time_ms start_time = ptk_now_ms();
    ptk_time_ms end_time = (timeout_ms == 0) ? PTK_TIME_WAIT_FOREVER : start_time + timeout_ms;
    
    // Create buffer array to collect packets
    ptk_buf_array_t *packet_array = ptk_buf_array_create(1, NULL); // Start with space for 1 packet
    if (!packet_array) {
        error("Failed to create buffer array");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    while (true) {
        // Try to receive a packet
        ptk_buf *packet_buf = ptk_buf_alloc(65536); // Max UDP packet size
        if (!packet_buf) {
            error("Failed to create packet buffer");
            ptk_free(&packet_array);
            ptk_set_err(PTK_ERR_NO_RESOURCES);
            return NULL;
        }
        
        struct sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);
        ssize_t bytes_read = recvfrom(fd, packet_buf->data, packet_buf->data_len, MSG_DONTWAIT, 
                                     (struct sockaddr *)&src_addr, &addr_len);
        
        if (bytes_read >= 0) {
            packet_buf->end = bytes_read;
            debug("Received %zd bytes in UDP packet", bytes_read);
            
            // Add packet to array
            ptk_err err = ptk_buf_array_append(packet_array, packet_buf);
            if (err != PTK_OK) {
                warn("Failed to append packet to array");
                ptk_free(&packet_buf);
                ptk_free(&packet_array);
                ptk_set_err(err);
                return NULL;
            }
            
            // Update sender address with the last received packet
            if (sender_addr) {
                sender_addr->ip = src_addr.sin_addr.s_addr;
                sender_addr->port = ntohs(src_addr.sin_port);
                sender_addr->family = AF_INET;
            }
            
            // If not waiting for multiple packets, return immediately
            if (!wait_for_packets) {
                debug("Returning immediately with %zu packets", ptk_buf_array_len(packet_array));
                info("ptk_udp_socket_recv_from: exit");
                return packet_array;
            }
            
            // Check if timeout has been reached
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu packets", ptk_buf_array_len(packet_array));
                info("ptk_udp_socket_recv_from: exit");
                return packet_array;
            }
            
            // Continue waiting for more packets
            continue;
        }
        
        // Free unused packet buffer
        ptk_free(&packet_buf);
        
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No more packets available right now
            if (ptk_buf_array_len(packet_array) > 0 && !wait_for_packets) {
                // We have packets and not waiting for more
                debug("No more packets, returning %zu packets", ptk_buf_array_len(packet_array));
                info("ptk_udp_socket_recv_from: exit");
                return packet_array;
            }
            
            // Calculate remaining timeout
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu packets", ptk_buf_array_len(packet_array));
                info("ptk_udp_socket_recv_from: exit");
                return packet_array;
            }
            
            ptk_duration_ms remaining_timeout = (timeout_ms == 0) ? 0 : end_time - current_time;
            debug("recvfrom() would block, registering for read event (remaining timeout: %lld ms)", remaining_timeout);
            
            event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, 
                                 ptk_now_ms() + remaining_timeout);
            platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_READ);
            timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + remaining_timeout);
            debug("Yielding threadlet");
            ptk_threadlet_yield();
            
            if (current_threadlet->status == THREADLET_TIMEOUT) {
                debug("Timeout occurred, returning %zu packets", ptk_buf_array_len(packet_array));
                info("ptk_udp_socket_recv_from: exit");
                return packet_array; // Return whatever packets we have
            }
            
            debug("Resuming recvfrom after yield");
            continue; // Try receiving again
        }
        
        // Network error occurred
        warn("recvfrom() failed: %s", strerror(errno));
        ptk_free(&packet_array);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        info("ptk_udp_socket_recv_from: exit");
        return NULL;
    }
}


//=============================================================================
// NETWORK DISCOVERY
//=============================================================================


static void network_info_dispose(void *ptr) {
    ptk_network_info *info = (ptk_network_info *)ptr;
    if (info) {
        if (info->interfaces) {
            for (size_t i = 0; i < info->interface_count; i++) {
                ptk_free(info->interfaces[i].name);
                ptk_free(info->interfaces[i].addresses);
            }
            ptk_free(info->interfaces);
        }
        info->interface_count = 0;
    }
}


/**
 * Discover network interfaces on the local machine.
 * 
 * This function should return a list of network interfaces and their properties (IP addresses, etc).
 * The returned ptk_network_info structure must be freed by the caller using ptk_free().
 * If discovery fails, it returns NULL and sets an appropriate error code via ptk_set_err().
 * This is a stub implementation; platform-specific code is needed to actually discover interfaces.
 * @return Pointer to ptk_network_info on success, NULL on failure (ptk_get_err() set).
 */
ptk_network_info *ptk_network_discover(void) {
    debug("ptk_network_discover: entry");
    // Discover network interfaces and populate info
    ptk_network_info *info = ptk_alloc(sizeof(ptk_network_info), network_info_dispose);
    if (!info) {
        warn("ptk_alloc for ptk_network_info failed");
        ptk_set_err(PTK_ERR_NO_MEMORY);
        debug("ptk_network_discover: exit");
        return NULL;
    }
    // Call into the platform-specific implementation
    if (platform_discover_network(info) != PTK_OK) {
        warn("platform_discover_network failed");
        ptk_free(info);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        debug("ptk_network_discover: exit");
        return NULL;
    }

    debug("ptk_network_discover: exit");
    return info;
}

/**
 * Get the number of network interface entries
 *
 * @param network_info The network information structure
 * @return Number of network entries, 0 if network_info is NULL
 */
size_t ptk_socket_network_info_count(const ptk_network_info *network_info) {
    if (!network_info) return 0;
    return network_info->interface_count;
}

/**
 * Get a specific network interface entry by index
 *
 * @param network_info The network information structure
 * @param index Index of the entry to retrieve (0-based)
 * @return Pointer to network entry, NULL if index is out of bounds or network_info is NULL
 */
const ptk_network_info_entry *ptk_socket_network_info_get(const ptk_network_info *network_info, size_t index) {
    if (!network_info || index >= network_info->interface_count) return NULL;
    return &network_info->interfaces[index];
}
