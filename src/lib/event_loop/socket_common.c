// ...existing includes...
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ptk_err.h>
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_sock.h>
#include "event_loop.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "ptk_threadlet.h"

// Helper to set non-blocking mode
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0) { return -1; }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

//==============================================================================
// Socket address conversion functions

/**
 * @brief Initialize a ptk_address_t structure from an IP string and port.
 *
 * @param address
 * @param ip_string
 * @param port
 * @return ptk_err
 */
ptk_err ptk_address_init(ptk_address_t *address, const char *ip_string, uint16_t port) {
    if(!address) { return PTK_ERR_NULL_PTR; }

    memset(address, 0, sizeof(ptk_address_t));
    address->family = AF_INET;
    address->port = port;

    if(!ip_string || strcmp(ip_string, "0.0.0.0") == 0) {
        address->ip = INADDR_ANY;
    } else {
        struct in_addr addr;
        if(inet_pton(AF_INET, ip_string, &addr) != 1) { return PTK_ERR_INVALID_PARAM; }
        address->ip = addr.s_addr;  // Already in network byte order
    }

    return PTK_OK;
}

char *ptk_address_to_string(const ptk_address_t *address) {
    if(!address) { return NULL; }

    char *str = ptk_alloc(INET_ADDRSTRLEN, NULL);
    if(!str) { return NULL; }
    if(address->family != AF_INET) {
        ptk_free(str);
        return NULL;  // Only supports IPv4 for now
    }
    struct in_addr addr;
    addr.s_addr = address->ip;

    if(!inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN)) {
        ptk_free(str);
        return NULL;
    }

    return str;
}

uint16_t ptk_address_get_port(const ptk_address_t *address) {
    if(!address) { return 0; }
    return address->port;
}

bool ptk_address_equals(const ptk_address_t *addr1, const ptk_address_t *addr2) {
    if(!addr1 || !addr2) { return false; }
    return addr1->ip == addr2->ip && addr1->port == addr2->port && addr1->family == addr2->family;
}

ptk_err ptk_address_init_any(ptk_address_t *address, uint16_t port) {
    if(!address) { return PTK_ERR_NULL_PTR; }

    memset(address, 0, sizeof(ptk_address_t));
    address->family = AF_INET;
    address->port = port;
    address->ip = INADDR_ANY;

    return PTK_OK;
}

/**
 * @brief Convert ptk_address_t to sockaddr_storage
 */
static ptk_err ptk_address_to_sockaddr(const ptk_address_t *address, struct sockaddr_storage *sockaddr, socklen_t *sockaddr_len) {
    if(!address || !sockaddr || !sockaddr_len) { return PTK_ERR_NULL_PTR; }

    struct sockaddr_in *addr_in = (struct sockaddr_in *)sockaddr;
    memset(addr_in, 0, sizeof(struct sockaddr_in));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(address->port);
    addr_in->sin_addr.s_addr = address->ip;
    *sockaddr_len = sizeof(struct sockaddr_in);

    return PTK_OK;
}

/**
 * @brief Convert sockaddr_storage to ptk_address_t
 */
static ptk_err ptk_sockaddr_to_address(const struct sockaddr_storage *sockaddr, socklen_t sockaddr_len, ptk_address_t *address) {
    if(!sockaddr || !address) { return PTK_ERR_NULL_PTR; }

    if(sockaddr->ss_family != AF_INET) { return PTK_ERR_INVALID_PARAM; }

    const struct sockaddr_in *addr_in = (const struct sockaddr_in *)sockaddr;
    memset(address, 0, sizeof(ptk_address_t));
    address->family = AF_INET;
    address->port = ntohs(addr_in->sin_port);
    address->ip = addr_in->sin_addr.s_addr;

    return PTK_OK;
}


//=============================================================================
// Common socket operations
//=============================================================================


// Forward declaration of current threadlet (set by event loop)
static ptk_threadlet_t *current_threadlet;

ptk_err ptk_socket_close(ptk_sock *sock) {
    if(!sock) { return PTK_ERR_INVALID_ARGUMENT; }
    debug("ptk_socket_close: entry");

    // If the current threadlet is waiting on this socket, abort it before closing fd
    if(current_threadlet && sock->event_loop && current_threadlet->status == THREADLET_WAITING) {
        event_registration_t *reg = event_registration_lookup(sock->event_loop->registrations, sock->fd);
        if(reg && reg->waiting_threadlet == current_threadlet) {
            current_threadlet->status = THREADLET_ABORTED;
            threadlet_queue_enqueue(&sock->event_loop->ready_queue, current_threadlet);
            event_registration_remove(sock->event_loop->registrations, sock->fd);
            platform_remove_fd(sock->event_loop->platform, sock->fd);
            trace("Aborted waiting threadlet for fd %d", sock->fd);
        }
    }

    if(sock->fd >= 0) {
        trace("Closing socket fd %d", sock->fd);
        // Remove from event loop if not already done
        event_registration_remove(sock->event_loop->registrations, sock->fd);
        platform_remove_fd(sock->event_loop->platform, sock->fd);
        // Shutdown first to notify peer
        shutdown(sock->fd, SHUT_RDWR);
        close(sock->fd);
        sock->fd = -1;
    }

    if(sock->type == PTK_SOCK_TCP_CLIENT || sock->type == PTK_SOCK_TCP_SERVER) {
        // Additional cleanup if needed
    }
    sock->type = PTK_SOCK_INVALID;

    debug("Socket closed");
    ptk_free(sock);

    ptk_set_err(PTK_OK);
    debug("ptk_socket_close: exit");
    return PTK_OK;
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
    if(!local_addr) {
        warn("ptk_tcp_socket_listen: local_addr is NULL");
        return NULL;
    }
    trace("Creating socket");
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        warn("socket() failed: %s", strerror(errno));
        return NULL;
    }
    trace("Setting non-blocking mode");
    if(set_nonblocking(fd) < 0) {
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
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        warn("bind() failed: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    trace("Listening on socket");
    if(listen(fd, backlog) < 0) {
        warn("listen() failed: %s", strerror(errno));
        close(fd);
        return NULL;
    }
    trace("Allocating ptk_sock");
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), NULL);
    if(!sock) {
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
 * @param out_client Output parameter to receive the accepted client socket.
 * @param timeout_ms Maximum time to wait in milliseconds (0 for infinite).
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK on error (ptk_get_err() set).
 */
ptk_err ptk_tcp_socket_accept(ptk_sock *server, ptk_sock **out_client, ptk_duration_ms timeout_ms) {
    debug("ptk_tcp_socket_accept: entry");
    if(!server || server->type != PTK_SOCK_TCP_SERVER || !out_client) {
        warn("Invalid arguments to ptk_tcp_socket_accept");
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }
    int fd = server->fd;
    trace("Calling accept() on fd %d", fd);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
    if(client_fd >= 0) {
        trace("Accepted new connection, client fd %d", client_fd);
        set_nonblocking(client_fd);
        ptk_sock *client_sock = ptk_alloc(sizeof(ptk_sock), NULL);
        if(!client_sock) {
            warn("ptk_alloc for client_sock failed");
            close(client_fd);
            ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
            return PTK_ERR_OUT_OF_MEMORY;
        }
        client_sock->fd = client_fd;
        client_sock->type = PTK_SOCK_TCP_CLIENT;
        // Attach to event loop if needed
        // client_sock->event_loop = server->event_loop;
        *out_client = client_sock;
        debug("ptk_tcp_socket_accept: exit");
        return PTK_WAIT_OK;
    }
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
        trace("accept() would block, registering for read event");
        event_registration_add(server->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ,
                               ptk_now_ms() + timeout_ms);
        platform_add_fd_read(server->event_loop->platform, fd);
        timeout_heap_insert(server->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        if(current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_tcp_socket_accept: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("ptk_tcp_socket_accept: exit");
            return PTK_ERR_TIMEOUT;
        }
        trace("Resuming accept after yield");
        debug("ptk_tcp_socket_accept: exit");
        return ptk_tcp_socket_accept(server, out_client, timeout_ms);
    }
    warn("accept() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("ptk_tcp_socket_accept: exit");
    return PTK_ERR_NETWORK_ERROR;
}


//=============================================================================
// TCP Client Socket Functions

/**
 * Connect to a TCP server (blocking).
 * This function initiates a connection to a remote TCP server. If the connection cannot be completed immediately,
 * it will yield the current threadlet and wait until the connection is established or the timeout expires.
 * If the connection is successful, it returns PTK_OK. If it times out, it returns PTK_ERR_TIMEOUT.
 * If there is a network error, it returns PTK_ERR_NETWORK_ERROR.
 * @param remote_addr The remote address to connect to (cannot be NULL).
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return a pointer to the connected socket on success, NULL on failure (ptk_get_err() set).
 */
ptk_sock *ptk_tcp_socket_connect(const ptk_address_t *remote_addr, ptk_duration_ms timeout_ms) {
    debug("entry");
    if(!sock || sock->type != PTK_SOCK_TCP_CLIENT || !remote_addr) {
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
    if(res == 0) {
        trace("Connected immediately");
        debug("exit");
        return PTK_OK;
    }
    if(errno == EINPROGRESS) {
        trace("connect() in progress, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE,
                               ptk_now_ms() + timeout_ms);
        platform_add_fd_write(sock->event_loop->platform, fd);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        if(current_threadlet->status == THREADLET_TIMEOUT) {
            warn("timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("exit");
            return PTK_ERR_TIMEOUT;
        }
        trace("Resuming connect after yield");
        debug("exit");
        return ptk_tcp_socket_connect(sock, remote_addr, timeout_ms);
    }
    warn("connect() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("exit");
    return PTK_ERR_NETWORK_ERROR;
}


/**
 * Read data from a TCP socket (blocking).
 *
 * This function attempts to read data from a TCP socket into the provided buffer.
 * If data is available, it reads as much as possible and returns PTK_OK.
 * If no data is available, it registers the socket for read events and yields the current threadlet.
 * When the socket becomes readable or the timeout expires, it resumes and attempts to read again.
 * If the read operation times out, it returns PTK_ERR_TIMEOUT. If there is a network error, it returns PTK_ERR_NETWORK_ERROR.
 * If the socket is invalid or arguments are bad, it returns PTK_ERR_INVALID_ARGUMENT.
 * If the socket was aborted while waiting, it returns PTK_ERR_ABORT (handled by threadlet status).
 * The buffer must have enough space to hold the incoming data; this function does not handle partial reads beyond what recv()
 * provides.
 * @param sock TCP client socket (must be valid and of type PTK_SOCK_TCP_CLIENT).
 * @param data Buffer to read data into (must be valid and have enough space).
 * @param timeout_ms Timeout for the read operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success (data read), PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 *         If the socket is invalid or arguments are bad, returns PTK_ERR_INVALID_ARGUMENT.
 *         If the socket was aborted while waiting, returns PTK_ERR_ABORT (handled by threadlet status).
 */
ptk_err ptk_tcp_socket_recv(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms) {
    debug("ptk_tcp_socket_recv: entry");
    int fd = sock->fd;
    trace("Calling recv() on fd %d", fd);
    ssize_t bytes_read = recv(fd, data->data, data->data_len, MSG_DONTWAIT);
    if(bytes_read > 0) {
        data->end += bytes_read;
        trace("Read %zd bytes", bytes_read);
        debug("ptk_tcp_socket_recv: exit");
        return PTK_OK;
    }
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
        trace("recv() would block, registering for read event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, ptk_now_ms() + timeout_ms);
        platform_add_fd_read(sock->event_loop->platform, fd);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        if(current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_tcp_socket_recv: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("ptk_tcp_socket_recv: exit");
            return PTK_ERR_TIMEOUT;
        }
        trace("Resuming read after yield");
        debug("ptk_tcp_socket_recv: exit");
        return ptk_tcp_socket_recv(sock, data, timeout_ms);
    }
    warn("recv() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("ptk_tcp_socket_recv: exit");
    return PTK_ERR_NETWORK_ERROR;
}


/**
 * Write data to a TCP socket (blocking).
 *
 * This function attempts to write data from the provided buffer to a TCP socket.
 * If the socket is writable, it writes as much data as possible and returns PTK_OK.
 * If the socket is not writable, it registers the socket for write events and yields the current threadlet.
 * When the socket becomes writable or the timeout expires, it resumes and attempts to write again.
 * If the write operation times out, it returns PTK_ERR_TIMEOUT. If there is a network error, it returns PTK_ERR_NETWORK_ERROR.
 * If the socket is invalid or arguments are bad, it returns PTK_ERR_INVALID_ARGUMENT.
 * If the socket was aborted while waiting, it returns PTK_ERR_ABORT (handled by threadlet status).
 * The buffer must have data to write; this function does not handle partial writes beyond what send() provides.
 * The buffer's start index is updated to reflect the number of bytes successfully written.
 * @param sock TCP client socket (must be valid and of type PTK_SOCK_TCP_CLIENT).
 * @param data Buffer containing data to write (must be valid and have data).
 * @param timeout_ms Timeout for the write operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success (data written), PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 *         If the socket is invalid or arguments are bad, returns PTK_ERR_INVALID_ARGUMENT.
 *         If the socket was aborted while waiting, returns PTK_ERR_ABORT (handled by threadlet status).
 *         If all data is written, data->start will equal data->end.
 *         If partial data is written, data->start will be updated accordingly; caller should check this.
 *         If no data is written, data->start remains unchanged.
 */
ptk_err ptk_tcp_socket_send(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms) {
    debug("ptk_tcp_socket_send: entry");
    int fd = sock->fd;
    ssize_t bytes_sent = send(fd, data->data + data->start, data->end - data->start, MSG_DONTWAIT);
    if(bytes_sent >= 0) {
        data->start += bytes_sent;
        trace("Wrote %zd bytes", bytes_sent);
        debug("ptk_tcp_socket_send: exit");
        return PTK_OK;
    }
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
        trace("send() would block, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE,
                               ptk_now_ms() + timeout_ms);
        platform_add_fd_write(sock->event_loop->platform, fd);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        if(current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_tcp_socket_send: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("ptk_tcp_socket_send: exit");
            return PTK_ERR_TIMEOUT;
        }
        trace("Resuming write after yield");
        debug("ptk_tcp_socket_send: exit");
        return ptk_tcp_socket_send(sock, data, timeout_ms);
    }
    warn("send() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("ptk_tcp_socket_send: exit");
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
    if(fd < 0) {
        warn("socket() failed: %s", strerror(errno));
        return NULL;
    }
    set_nonblocking(fd);
    if(broadcast) {
        int opt = 1;
        if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
            warn("setsockopt(SO_BROADCAST) failed: %s", strerror(errno));
            close(fd);
            return NULL;
        }
        trace("SO_BROADCAST enabled on UDP socket");
    }
    if(local_addr) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = local_addr->ip;
        addr.sin_port = htons(local_addr->port);
        if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            warn("bind() failed: %s", strerror(errno));
            close(fd);
            return NULL;
        }
    }
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), NULL);
    if(!sock) {
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
 * Send UDP data to a specific address (blocking).
 * This function sends data from the provided buffer to the specified destination address.
 * If the socket is not writable, it registers for write events and yields the current threadlet.
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param data Buffer containing data to send (must be valid and have data).
 * @param dest_addr Destination address to send to (cannot be NULL).
 * @param broadcast If true, allows sending to broadcast addresses.
 * @param timeout_ms Timeout for the send operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 */
ptk_err ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, bool broadcast,
                               ptk_duration_ms timeout_ms) {
    debug("ptk_udp_socket_send_to: entry");
    int fd = sock->fd;
    ssize_t bytes_sent = sendto(fd, data->data + data->start, data->end - data->start, MSG_DONTWAIT, (struct sockaddr *)dest_addr,
                                sizeof(*dest_addr));
    if(bytes_sent >= 0) {
        data->start += bytes_sent;
        trace("Wrote %zd bytes", bytes_sent);
        debug("ptk_udp_socket_send_to: exit");
        return PTK_OK;
    }
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
        trace("sendto() would block, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE,
                               ptk_now_ms() + timeout_ms);
        platform_add_fd_write(sock->event_loop->platform, fd);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        if(current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_udp_socket_send_to: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("ptk_udp_socket_send_to: exit");
            return PTK_ERR_TIMEOUT;
        }
        trace("Resuming sendto after yield");
        debug("ptk_udp_socket_send_to: exit");
        return ptk_udp_socket_send_to(sock, data, dest_addr, broadcast, timeout_ms);
    }
    warn("sendto() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("ptk_udp_socket_send_to: exit");
    return PTK_ERR_NETWORK_ERROR;
}


/**
 * Receive UDP data from any address (blocking).
 *
 * This function attempts to receive data from a UDP socket into the provided buffer.
 * It fills the out_addr parameter with the source address of the received packet if provided.
 * If no data is available, it registers the socket for read events and yields the current threadlet.
 * When data is available or the timeout expires, it resumes and attempts to read again.
 * If the read operation times out, it returns PTK_ERR_TIMEOUT. If there is a network error, it returns PTK_ERR_NETWORK_ERROR.
 * If the socket is invalid or arguments are bad, it returns PTK_ERR_INVALID_ARGUMENT.
 * If the socket was aborted while waiting, it returns PTK_ERR_ABORT (handled by threadlet status).
 * The buffer must have enough space to hold the incoming data; this function does not handle partial reads beyond what recvfrom()
 * provides.
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param data Buffer to read data into (must be valid and have enough space).
 * @param out_addr Output parameter to receive the source address of the packet (can be NULL).
 * @param timeout_ms Timeout for the read operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success (data read), PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 *         If the socket is invalid or arguments are bad, returns PTK_ERR_INVALID_ARGUMENT.
 *         If the socket was aborted while waiting, returns PTK_ERR_ABORT (handled by threadlet status).
 *         If data is received, data->end will be updated to reflect the number of bytes read.
 */
ptk_err ptk_udp_socket_recv_from(ptk_sock *sock, ptk_buf *data, ptk_address_t *out_addr, ptk_duration_ms timeout_ms) {
    debug("ptk_udp_socket_recv_from: entry");
    int fd = sock->fd;
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    ssize_t bytes_read = recvfrom(fd, data->data, data->data_len, MSG_DONTWAIT, (struct sockaddr *)&src_addr, &addr_len);
    if(bytes_read >= 0) {
        data->end += bytes_read;
        if(out_addr) { memcpy(out_addr, &src_addr, sizeof(src_addr)); }
        trace("Read %zd bytes", bytes_read);
        debug("ptk_udp_socket_recv_from: exit");
        return PTK_OK;
    }
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
        trace("recvfrom() would block, registering for read event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, ptk_now_ms() + timeout_ms);
        platform_add_fd_read(sock->event_loop->platform, fd);
        timeout_heap_insert(sock->event_loop->timeout_heap, fd, ptk_now_ms() + timeout_ms);
        trace("Yielding threadlet");
        ptk_threadlet_yield();
        if(current_threadlet->status == THREADLET_TIMEOUT) {
            warn("ptk_udp_socket_recv_from: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            debug("ptk_udp_socket_recv_from: exit");
            return PTK_ERR_TIMEOUT;
        }
        trace("Resuming recvfrom after yield");
        debug("ptk_udp_socket_recv_from: exit");
        return ptk_udp_socket_recv_from(sock, data, out_addr, timeout_ms);
    }
    warn("recvfrom() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    debug("ptk_udp_socket_recv_from: exit");
    return PTK_ERR_NETWORK_ERROR;
}


//=============================================================================
// NETWORK DISCOVERY
//=============================================================================


static void network_info_dispose(void *ptr) {
    ptk_network_info *info = (ptk_network_info *)ptr;
    if(info) {
        if(info->interfaces) {
            for(size_t i = 0; i < info->interface_count; i++) {
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
 * This function returns a list of network interfaces and their properties (IP addresses, etc).
 * The returned ptk_network_info structure must be freed by the caller using ptk_free().
 * If discovery fails, it returns NULL and sets an appropriate error code via ptk_set_err().
 * This is a stub implementation; platform-specific code is needed to actually discover interfaces.
 * @return Pointer to ptk_network_info on success, NULL on failure (ptk_get_err() set).
 */
ptk_network_info *ptk_socket_list_networks(void) {
    debug("ptk_socket_list_networks: entry");
    // Discover network interfaces and populate info
    ptk_network_info *info = ptk_alloc(sizeof(ptk_network_info), network_info_dispose);
    if(!info) {
        warn("ptk_alloc for ptk_network_info failed");
        ptk_set_err(PTK_ERR_NO_MEMORY);
        debug("ptk_socket_list_networks: exit");
        return NULL;
    }
    // Call into the platform-specific implementation
    if(platform_discover_network(info) != PTK_OK) {
        warn("platform_discover_network failed");
        ptk_free(info);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        debug("ptk_socket_list_networks: exit");
        return NULL;
    }

    debug("ptk_socket_list_networks: exit");
    return info;
}

/**
 * Get the number of network interface entries
 *
 * @param network_info The network information structure
 * @return Number of network entries, 0 if network_info is NULL
 */
size_t ptk_socket_network_info_count(const ptk_network_info *network_info) {
    if(!network_info) { return 0; }
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
    if(!network_info || index >= network_info->interface_count) { return NULL; }
    return &network_info->interfaces[index];
}
