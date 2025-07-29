/**
 * @file socket.c
 * @brief macOS Socket Implementation using BSD sockets + GCD
 */

#include "../../include/macos/protocol_toolkit.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

/* External reference to global event loop slots */
extern ptk_event_loop_slot_t *g_event_loop_slots;
extern size_t g_num_slots;

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/**
 * @brief Find socket by handle
 */
static ptk_socket_internal_t *find_socket(ptk_handle_t handle) {
    if(handle == 0 || PTK_HANDLE_TYPE(handle) != PTK_TYPE_SOCKET) { return NULL; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(handle);
    if(loop_id >= g_num_slots) { return NULL; }

    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources || !slot->resources->sockets) { return NULL; }

    /* Linear scan for exact handle match */
    for(size_t i = 0; i < slot->resources->num_sockets; i++) {
        ptk_socket_internal_t *socket = &slot->resources->sockets[i];
        if(socket->base.handle == handle) { return socket; }
    }

    return NULL;
}

/**
 * @brief Parse address string to sockaddr
 */
static ptk_err_t parse_address(const char *address, uint16_t port, struct sockaddr_storage *addr, socklen_t *addr_len) {
    if(!addr || !addr_len) { return PTK_ERR_INVALID_ARGUMENT; }

    memset(addr, 0, sizeof(*addr));

    /* Try IPv4 first */
    struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
    if(inet_pton(AF_INET, address ? address : "0.0.0.0", &addr4->sin_addr) == 1) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port);
        *addr_len = sizeof(*addr4);
        return PTK_ERR_OK;
    }

    /* Try IPv6 */
    struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
    if(inet_pton(AF_INET6, address ? address : "::", &addr6->sin6_addr) == 1) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);
        *addr_len = sizeof(*addr6);
        return PTK_ERR_OK;
    }

    return PTK_ERR_INVALID_ARGUMENT;
}

/**
 * @brief Set socket to non-blocking mode
 */
static ptk_err_t set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) { return PTK_ERR_NETWORK_ERROR; }

    if(fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_ERR_OK;
}

/**
 * @brief Socket read event handler
 */
static void socket_read_handler(void *context) {
    ptk_socket_internal_t *socket = (ptk_socket_internal_t *)context;

    /* Call registered read event handlers */
    for(size_t i = 0; i < sizeof(socket->event_handlers) / sizeof(socket->event_handlers[0]); i++) {
        ptk_event_handler_t *handler = &socket->event_handlers[i];
        if(handler->is_active && handler->event_type == PTK_EVENT_SOCKET_READABLE && handler->handler) {
            handler->handler(socket->base.handle, PTK_EVENT_SOCKET_READABLE, NULL, handler->user_data);
        }
    }
}

/**
 * @brief Socket write event handler
 */
static void socket_write_handler(void *context) {
    ptk_socket_internal_t *socket = (ptk_socket_internal_t *)context;

    /* Call registered write event handlers */
    for(size_t i = 0; i < sizeof(socket->event_handlers) / sizeof(socket->event_handlers[0]); i++) {
        ptk_event_handler_t *handler = &socket->event_handlers[i];
        if(handler->is_active && handler->event_type == PTK_EVENT_SOCKET_WRITABLE && handler->handler) {
            handler->handler(socket->base.handle, PTK_EVENT_SOCKET_WRITABLE, NULL, handler->user_data);
        }
    }
}

/**
 * @brief Create socket with given type
 */
static ptk_handle_t socket_create_internal(ptk_handle_t event_loop, int socket_type) {
    if(event_loop == 0 || PTK_HANDLE_TYPE(event_loop) != PTK_TYPE_EVENT_LOOP) { return PTK_ERR_INVALID_HANDLE; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(event_loop);
    if(loop_id >= g_num_slots) { return PTK_ERR_INVALID_HANDLE; }

    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources || !slot->resources->sockets) {
        ptk_set_last_error(event_loop, PTK_ERR_INVALID_HANDLE);
        return PTK_ERR_INVALID_HANDLE;
    }

    /* Find free socket slot */
    for(size_t i = 0; i < slot->resources->num_sockets; i++) {
        ptk_socket_internal_t *sock_obj = &slot->resources->sockets[i];
        if(sock_obj->base.handle == 0) {
            /* Create BSD socket */
            int sockfd = socket(AF_INET, socket_type, 0);
            if(sockfd == -1) {
                ptk_set_last_error(event_loop, PTK_ERR_NETWORK_ERROR);
                return PTK_ERR_NETWORK_ERROR;
            }

            /* Set non-blocking */
            if(set_nonblocking(sockfd) != PTK_ERR_OK) {
                close(sockfd);
                ptk_set_last_error(event_loop, PTK_ERR_NETWORK_ERROR);
                return PTK_ERR_NETWORK_ERROR;
            }

            /* Initialize socket */
            sock_obj->generation_counter++;
            sock_obj->base.handle = PTK_MAKE_HANDLE(PTK_TYPE_SOCKET, loop_id, sock_obj->generation_counter, i);
            sock_obj->base.event_loop = event_loop;
            sock_obj->sockfd = sockfd;
            sock_obj->read_source = NULL;
            sock_obj->write_source = NULL;
            sock_obj->is_connected = false;
            sock_obj->is_listening = false;
            sock_obj->socket_type = socket_type;
            sock_obj->local_addr_len = 0;
            sock_obj->remote_addr_len = 0;

            /* Clear event handlers */
            memset(sock_obj->event_handlers, 0, sizeof(sock_obj->event_handlers));

            return sock_obj->base.handle;
        }
    }

    ptk_set_last_error(event_loop, PTK_ERR_OUT_OF_MEMORY);
    return PTK_ERR_OUT_OF_MEMORY;
}

/* ========================================================================
 * SOCKET MANAGEMENT
 * ======================================================================== */

ptk_handle_t ptk_socket_create_tcp(ptk_handle_t event_loop) { return socket_create_internal(event_loop, SOCK_STREAM); }

ptk_handle_t ptk_socket_create_udp(ptk_handle_t event_loop) { return socket_create_internal(event_loop, SOCK_DGRAM); }

ptk_err_t ptk_socket_connect(ptk_handle_t socket, const char *address, uint16_t port) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_HANDLE; }

    if(!address || port == 0) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }

    /* Parse remote address */
    if(parse_address(address, port, &sock->remote_addr, &sock->remote_addr_len) != PTK_ERR_OK) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }

    /* Attempt connection */
    int result = connect(sock->sockfd, (struct sockaddr *)&sock->remote_addr, sock->remote_addr_len);
    if(result == -1) {
        if(errno == EINPROGRESS) {
            /* Non-blocking connect in progress */
            return PTK_ERR_WOULD_BLOCK;
        } else if(errno == ECONNREFUSED) {
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_CONNECTION_REFUSED);
            return PTK_ERR_CONNECTION_REFUSED;
        } else {
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
            return PTK_ERR_NETWORK_ERROR;
        }
    }

    sock->is_connected = true;
    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_bind(ptk_handle_t socket, const char *address, uint16_t port) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_HANDLE; }

    /* Parse local address */
    if(parse_address(address, port, &sock->local_addr, &sock->local_addr_len) != PTK_ERR_OK) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }

    /* Enable address reuse */
    int reuse = 1;
    if(setsockopt(sock->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
        return PTK_ERR_NETWORK_ERROR;
    }

    /* Bind socket */
    if(bind(sock->sockfd, (struct sockaddr *)&sock->local_addr, sock->local_addr_len) == -1) {
        if(errno == EADDRINUSE) {
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_ADDRESS_IN_USE);
            return PTK_ERR_ADDRESS_IN_USE;
        } else {
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
            return PTK_ERR_NETWORK_ERROR;
        }
    }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_listen(ptk_handle_t socket, int backlog) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_HANDLE; }

    if(sock->socket_type != SOCK_STREAM) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_NOT_SUPPORTED);
        return PTK_ERR_NOT_SUPPORTED;
    }

    if(listen(sock->sockfd, backlog) == -1) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
        return PTK_ERR_NETWORK_ERROR;
    }

    sock->is_listening = true;
    return PTK_ERR_OK;
}

ptk_handle_t ptk_socket_accept(ptk_handle_t listener) {
    ptk_socket_internal_t *listen_sock = find_socket(listener);
    if(!listen_sock) { return PTK_ERR_INVALID_HANDLE; }

    if(!listen_sock->is_listening) {
        ptk_set_last_error(listen_sock->base.event_loop, PTK_ERR_NOT_CONNECTED);
        return PTK_ERR_NOT_CONNECTED;
    }

    /* Accept connection */
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(listen_sock->sockfd, (struct sockaddr *)&client_addr, &client_addr_len);

    if(client_fd == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return PTK_ERR_WOULD_BLOCK;
        } else {
            ptk_set_last_error(listen_sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
            return PTK_ERR_NETWORK_ERROR;
        }
    }

    /* Set client socket non-blocking */
    if(set_nonblocking(client_fd) != PTK_ERR_OK) {
        close(client_fd);
        ptk_set_last_error(listen_sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
        return PTK_ERR_NETWORK_ERROR;
    }

    /* Find free socket slot for client */
    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(listener);
    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];

    for(size_t i = 0; i < slot->resources->num_sockets; i++) {
        ptk_socket_internal_t *socket = &slot->resources->sockets[i];
        if(socket->base.handle == 0) {
            /* Initialize client socket */
            socket->generation_counter++;
            socket->base.handle = PTK_MAKE_HANDLE(PTK_TYPE_SOCKET, loop_id, socket->generation_counter, i);
            socket->base.event_loop = listen_sock->base.event_loop;
            socket->sockfd = client_fd;
            socket->read_source = NULL;
            socket->write_source = NULL;
            socket->is_connected = true;
            socket->is_listening = false;
            socket->socket_type = SOCK_STREAM;
            socket->remote_addr = client_addr;
            socket->remote_addr_len = client_addr_len;
            socket->local_addr_len = 0;

            /* Clear event handlers */
            memset(socket->event_handlers, 0, sizeof(socket->event_handlers));

            return socket->base.handle;
        }
    }

    /* No free slots */
    close(client_fd);
    ptk_set_last_error(listen_sock->base.event_loop, PTK_ERR_OUT_OF_MEMORY);
    return PTK_ERR_OUT_OF_MEMORY;
}

ptk_err_t ptk_socket_send(ptk_handle_t socket, const ptk_buffer_t *buffer) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_HANDLE; }

    if(!buffer || !buffer->data || buffer->size == 0) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }

    ssize_t result = send(sock->sockfd, buffer->data, buffer->size, 0);
    if(result == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return PTK_ERR_WOULD_BLOCK;
        } else if(errno == ECONNRESET) {
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_CONNECTION_RESET);
            return PTK_ERR_CONNECTION_RESET;
        } else {
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
            return PTK_ERR_NETWORK_ERROR;
        }
    }

    /* Update buffer size with actual bytes sent */
    ((ptk_buffer_t *)buffer)->size = (size_t)result;

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_receive(ptk_handle_t socket, ptk_buffer_t *buffer) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_HANDLE; }

    if(!buffer || !buffer->data || buffer->capacity == 0) {
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }

    ssize_t result = recv(sock->sockfd, buffer->data, buffer->capacity, 0);
    if(result == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            buffer->size = 0;
            return PTK_ERR_WOULD_BLOCK;
        } else if(errno == ECONNRESET) {
            buffer->size = 0;
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_CONNECTION_RESET);
            return PTK_ERR_CONNECTION_RESET;
        } else {
            buffer->size = 0;
            ptk_set_last_error(sock->base.event_loop, PTK_ERR_NETWORK_ERROR);
            return PTK_ERR_NETWORK_ERROR;
        }
    } else if(result == 0) {
        /* Connection closed by peer */
        buffer->size = 0;
        ptk_set_last_error(sock->base.event_loop, PTK_ERR_CONNECTION_RESET);
        return PTK_ERR_CONNECTION_RESET;
    }

    buffer->size = (size_t)result;

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_close(ptk_handle_t socket) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_HANDLE; }

    if(sock->sockfd != -1) {
        close(sock->sockfd);
        sock->sockfd = -1;
        sock->is_connected = false;
        sock->is_listening = false;
    }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_destroy(ptk_handle_t socket) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_HANDLE; }

    /* Close socket if still open */
    if(sock->sockfd != -1) { close(sock->sockfd); }

    /* Clean up GCD sources */
    if(sock->read_source) {
        dispatch_source_cancel(sock->read_source);
        dispatch_release(sock->read_source);
        sock->read_source = NULL;
    }

    if(sock->write_source) {
        dispatch_source_cancel(sock->write_source);
        dispatch_release(sock->write_source);
        sock->write_source = NULL;
    }

    /* Clear socket data */
    memset(sock, 0, sizeof(*sock));
    sock->sockfd = -1;

    return PTK_ERR_OK;
}

/* ========================================================================
 * UDP-SPECIFIC SOCKET OPERATIONS
 * ======================================================================== */

ptk_err_t ptk_socket_sendto(ptk_handle_t socket, const ptk_buffer_t *buffer, const char *address, uint16_t port) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock || !buffer || !buffer->data || !address) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }
    if(buffer->size == 0) { return PTK_ERR_INVALID_ARGUMENT; }

    struct sockaddr_storage dest_addr;
    socklen_t dest_len;
    ptk_err_t result = parse_address(address, port, &dest_addr, &dest_len);
    if(result != PTK_ERR_OK) { return result; }

    ssize_t sent = sendto(sock->sockfd, buffer->data, buffer->size, 0, (struct sockaddr *)&dest_addr, dest_len);
    if(sent < 0) {
        switch(errno) {
            case EWOULDBLOCK: return PTK_ERR_WOULD_BLOCK;
            case ECONNREFUSED: return PTK_ERR_CONNECTION_REFUSED;
            case ENETUNREACH: return PTK_ERR_NO_ROUTE;
            case EMSGSIZE: return PTK_ERR_MESSAGE_TOO_LARGE;
            default: return PTK_ERR_NETWORK_ERROR;
        }
    }

    /* Update buffer size with actual bytes sent */
    ((ptk_buffer_t *)buffer)->size = (size_t)sent;
    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_recvfrom(ptk_handle_t socket, ptk_buffer_t *buffer, char *sender_address, size_t address_len,
                              uint16_t *sender_port) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock || !buffer || !buffer->data) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }
    if(buffer->capacity == 0) { return PTK_ERR_INVALID_ARGUMENT; }

    struct sockaddr_storage sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    ssize_t received = recvfrom(sock->sockfd, buffer->data, buffer->capacity, 0, (struct sockaddr *)&sender_addr, &sender_len);
    if(received < 0) {
        switch(errno) {
            case EWOULDBLOCK: buffer->size = 0; return PTK_ERR_WOULD_BLOCK;
            case ECONNRESET: buffer->size = 0; return PTK_ERR_CONNECTION_RESET;
            default: buffer->size = 0; return PTK_ERR_NETWORK_ERROR;
        }
    }

    buffer->size = (size_t)received;

    /* Extract sender address and port if requested */
    if(sender_address && address_len > 0 && sender_port) {
        if(sender_addr.ss_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)&sender_addr;
            inet_ntop(AF_INET, &sin->sin_addr, sender_address, address_len);
            *sender_port = ntohs(sin->sin_port);
        } else if(sender_addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&sender_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, sender_address, address_len);
            *sender_port = ntohs(sin6->sin6_port);
        } else {
            sender_address[0] = '\0';
            *sender_port = 0;
        }
    }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_enable_broadcast(ptk_handle_t socket) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    int broadcast = 1;
    if(setsockopt(sock->sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_disable_broadcast(ptk_handle_t socket) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    int broadcast = 0;
    if(setsockopt(sock->sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_broadcast(ptk_handle_t socket, const ptk_buffer_t *buffer, uint16_t port) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock || !buffer || !buffer->data) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    /* Enable broadcast first */
    ptk_err_t result = ptk_socket_enable_broadcast(socket);
    if(result != PTK_ERR_OK) { return result; }

    /* Send to broadcast address */
    return ptk_socket_sendto(socket, buffer, "255.255.255.255", port);
}

ptk_err_t ptk_socket_join_multicast_group(ptk_handle_t socket, const char *multicast_address, const char *interface_address) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock || !multicast_address) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    struct ip_mreq mreq;

    /* Parse multicast address */
    if(inet_pton(AF_INET, multicast_address, &mreq.imr_multiaddr) != 1) { return PTK_ERR_INVALID_ARGUMENT; }

    /* Parse interface address or use INADDR_ANY */
    if(interface_address && strlen(interface_address) > 0) {
        if(inet_pton(AF_INET, interface_address, &mreq.imr_interface) != 1) { return PTK_ERR_INVALID_ARGUMENT; }
    } else {
        mreq.imr_interface.s_addr = INADDR_ANY;
    }

    if(setsockopt(sock->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_leave_multicast_group(ptk_handle_t socket, const char *multicast_address, const char *interface_address) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock || !multicast_address) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    struct ip_mreq mreq;

    /* Parse multicast address */
    if(inet_pton(AF_INET, multicast_address, &mreq.imr_multiaddr) != 1) { return PTK_ERR_INVALID_ARGUMENT; }

    /* Parse interface address or use INADDR_ANY */
    if(interface_address && strlen(interface_address) > 0) {
        if(inet_pton(AF_INET, interface_address, &mreq.imr_interface) != 1) { return PTK_ERR_INVALID_ARGUMENT; }
    } else {
        mreq.imr_interface.s_addr = INADDR_ANY;
    }

    if(setsockopt(sock->sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_set_multicast_ttl(ptk_handle_t socket, uint8_t ttl) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    int ttl_int = (int)ttl;
    if(setsockopt(sock->sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl_int, sizeof(ttl_int)) < 0) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_set_multicast_loopback(ptk_handle_t socket, bool enable) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    int loopback = enable ? 1 : 0;
    if(setsockopt(sock->sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) < 0) { return PTK_ERR_NETWORK_ERROR; }

    return PTK_ERR_OK;
}

ptk_err_t ptk_socket_multicast_send(ptk_handle_t socket, const ptk_buffer_t *buffer, const char *multicast_address,
                                    uint16_t port) {
    ptk_socket_internal_t *sock = find_socket(socket);
    if(!sock || !buffer || !buffer->data || !multicast_address) { return PTK_ERR_INVALID_ARGUMENT; }

    if(sock->socket_type != SOCK_DGRAM) { return PTK_ERR_NOT_SUPPORTED; }
    if(sock->sockfd == -1) { return PTK_ERR_NOT_CONNECTED; }

    /* Send to multicast address */
    return ptk_socket_sendto(socket, buffer, multicast_address, port);
}
