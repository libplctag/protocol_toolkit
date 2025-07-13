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
#include <ptk_buf.h>
#include "socket_tcp.h"
#include "socket_internal.h"
#include "event_loop.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "threadlet_integration.h"

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
        timeout_heap_add(server->event_loop->timeouts, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_ABORTED) {
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
 * This function creates a TCP client socket and connects to a remote server.
 * If the connection cannot be completed immediately, it will yield the current threadlet
 * and wait until the connection is established or the timeout expires.
 * @param remote_addr The remote address to connect to (cannot be NULL).
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return A pointer to the connected TCP socket, or NULL on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_sock *ptk_tcp_socket_connect(const ptk_address_t *remote_addr, ptk_duration_ms timeout_ms) {
    debug("ptk_tcp_socket_connect: entry");
    if (!remote_addr) {
        warn("Invalid arguments to ptk_tcp_socket_connect");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Create TCP socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        warn("socket() failed: %s", strerror(errno));
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Set non-blocking mode
    if (set_nonblocking(fd) < 0) {
        warn("set_nonblocking() failed: %s", strerror(errno));
        close(fd);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    // Attempt to connect
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = remote_addr->ip;
    addr.sin_port = htons(remote_addr->port);
    
    trace("Calling connect() on fd %d", fd);
    int res = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    
    if (res == 0) {
        trace("Connected immediately");
        // Create socket structure
        ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), ptk_socket_destructor);
        if (!sock) {
            warn("ptk_alloc for ptk_sock failed");
            close(fd);
            ptk_set_err(PTK_ERR_NO_RESOURCES);
            return NULL;
        }
        sock->fd = fd;
        sock->type = PTK_SOCK_TCP_CLIENT;
        debug("ptk_tcp_socket_connect: exit");
        return sock;
    }
    
    if (errno == EINPROGRESS) {
        // Create socket structure for async connection
        ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), ptk_socket_destructor);
        if (!sock) {
            warn("ptk_alloc for ptk_sock failed");
            close(fd);
            ptk_set_err(PTK_ERR_NO_RESOURCES);
            return NULL;
        }
        sock->fd = fd;
        sock->type = PTK_SOCK_TCP_CLIENT;
        
        trace("connect() in progress, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_WRITE);
        timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_ABORTED) {
            warn("ptk_tcp_socket_connect: timeout");
            ptk_free(&sock);
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("ptk_tcp_socket_connect: exit");
            return NULL;
        }
        
        // Check if connection succeeded
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            warn("Connection failed: %s", strerror(error ? error : errno));
            ptk_free(&sock);
            ptk_set_err(PTK_ERR_NETWORK_ERROR);
            debug("ptk_tcp_socket_connect: exit");
            return NULL;
        }
        
        trace("Connection established after yield");
        debug("ptk_tcp_socket_connect: exit");
        return sock;
    }
    
    warn("connect() failed: %s", strerror(errno));
    close(fd);
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("ptk_tcp_socket_connect: exit");
    return NULL;
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
 * @param wait_for_data If true, wait the entire timeout period and return as much data as is available.
 * @param timeout_ms Timeout for the read operation (in milliseconds). If 0, wait indefinitely.
 * @return Buffer containing received data on success (must be freed with ptk_free()), NULL on error.
 *         On error, check ptk_get_err() for error details.
 */
ptk_buf *ptk_tcp_socket_recv(ptk_sock *sock, bool wait_for_data, ptk_duration_ms timeout_ms) {
    info("ptk_tcp_socket_recv: entry");
    
    if (!sock || sock->type != PTK_SOCK_TCP_CLIENT) {
        warn("Invalid arguments to ptk_tcp_socket_recv");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    int fd = sock->fd;
    ptk_time_ms start_time = ptk_now_ms();
    ptk_time_ms end_time = (timeout_ms == 0) ? PTK_TIME_WAIT_FOREVER : start_time + timeout_ms;
    
    // Create a buffer to receive data
    ptk_buf *data = ptk_buf_alloc(4096); // Default 4KB buffer
    if (!data) {
        error("Failed to create receive buffer");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    while (true) {
        debug("Calling recv() on fd %d", fd);
        ssize_t bytes_read = recv(fd, data->data + data->end, data->data_len - data->end, MSG_DONTWAIT);
        
        if (bytes_read > 0) {
            data->end += bytes_read;
            debug("Read %zd bytes, total in buffer: %zu", bytes_read, data->end - data->start);
            
            // If not waiting for more data, return immediately
            if (!wait_for_data) {
                info("ptk_tcp_socket_recv: exit");
                return data;
            }
            
            // Check if timeout has been reached
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu bytes", data->end - data->start);
                info("ptk_tcp_socket_recv: exit");
                return data;
            }
            
            // Continue reading if there's more space in buffer
            if (data->end < data->data_len) {
                continue;
            } else {
                // Buffer is full, return what we have
                debug("Buffer full, returning %zu bytes", data->end - data->start);
                info("ptk_tcp_socket_recv: exit");
                return data;
            }
        }
        
        if (bytes_read == 0) {
            debug("Connection closed by peer");
            if (data->end > data->start) {
                // Return any data we have
                info("ptk_tcp_socket_recv: exit");
                return data;
            } else {
                ptk_free(&data);
                ptk_set_err(PTK_ERR_CLOSED);
                info("ptk_tcp_socket_recv: exit");
                return NULL;
            }
        }
        
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No more data available right now
            if (!wait_for_data && data->end > data->start) {
                // We have some data and not waiting for more
                debug("No more data, returning %zu bytes", data->end - data->start);
                info("ptk_tcp_socket_recv: exit");
                return data;
            }
            
            // Calculate remaining timeout
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu bytes", data->end - data->start);
                if (data->end > data->start) {
                    info("ptk_tcp_socket_recv: exit");
                    return data;
                } else {
                    ptk_free(&data);
                    ptk_set_err(PTK_ERR_TIMEOUT);
                    info("ptk_tcp_socket_recv: exit");
                    return NULL;
                }
            }
            
            ptk_duration_ms remaining_timeout = (timeout_ms == 0) ? 0 : end_time - current_time;
            debug("recv() would block, registering for read event (remaining timeout: %lld ms)", remaining_timeout);
            
            event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, 
                                 ptk_now_ms() + remaining_timeout);
            platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_READ);
            timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + remaining_timeout);
            debug("Yielding threadlet");
            ptk_threadlet_yield();
            
            if (current_threadlet->status == THREADLET_ABORTED) {
                warn("ptk_tcp_socket_recv: timeout");
                if (data->end > data->start) {
                    info("ptk_tcp_socket_recv: exit");
                    return data;
                } else {
                    ptk_free(&data);
                    ptk_set_err(PTK_ERR_TIMEOUT);
                    info("ptk_tcp_socket_recv: exit");
                    return NULL;
                }
            }
            
            debug("Resuming read after yield");
            continue; // Try reading again
        }
        
        warn("recv() failed: %s", strerror(errno));
        ptk_free(&data);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        info("ptk_tcp_socket_recv: exit");
        return NULL;
    }
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
        ptk_buf data_value;
        ptk_err err = ptk_buf_array_get(data_array, i, &data_value);
        if (err != PTK_OK) {
            error("Failed to get buffer %zu from array", i);
            return err;
        }
        ptk_buf *data = &data_value;
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
            ptk_buf data_value;
            ptk_err err = ptk_buf_array_get(data_array, i, &data_value);
            if (err != PTK_OK) {
                error("Failed to get buffer %zu from array", i);
                return err;
            }
            ptk_buf *data = &data_value;
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
        timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_ABORTED) {
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
