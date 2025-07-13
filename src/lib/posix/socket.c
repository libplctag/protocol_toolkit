/**
 * @file socket.c
 * @brief POSIX implementation of TCP/UDP socket functions for Protocol Toolkit
 */
#include "ptk_sock.h"
#include "ptk_alloc.h"
#include "ptk_err.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

// Helper for error mapping
static ptk_err map_errno(int err) {
    switch (err) {
        case EAGAIN: case EWOULDBLOCK: return PTK_ERR_TIMEOUT;
        case ECONNREFUSED: return PTK_ERR_CONNECTION_REFUSED;
        case ENETUNREACH: return PTK_ERR_HOST_UNREACHABLE;
        case EADDRINUSE: return PTK_ERR_ADDRESS_IN_USE;
        default: return PTK_ERR_NETWORK_ERROR;
    }
}

/**
 * @brief Listen on a local address as a TCP server.
 */
ptk_sock *ptk_tcp_socket_listen(const ptk_address_t *local_addr, int backlog) {
    if (!local_addr) return NULL;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ptk_set_err(map_errno(errno));
        return NULL;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = local_addr->ip;
    addr.sin_port = htons(local_addr->port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        ptk_set_err(map_errno(errno));
        return NULL;
    }
    if (listen(fd, backlog) < 0) {
        close(fd);
        ptk_set_err(map_errno(errno));
        return NULL;
    }
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), NULL);
    if (!sock) {
        close(fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    sock->fd = fd;
    sock->type = PTK_SOCK_TCP_SERVER;
    return sock;
}

/**
 * @brief Accept a new TCP connection (blocking).
 */
ptk_sock *ptk_tcp_socket_accept(ptk_sock *server, ptk_duration_ms timeout_ms) {
    if (!server || server->type != PTK_SOCK_TCP_SERVER) return NULL;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(server->fd, (struct sockaddr *)&addr, &addrlen);
    if (fd < 0) {
        ptk_set_err(map_errno(errno));
        return NULL;
    }
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), NULL);
    if (!sock) {
        close(fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    sock->fd = fd;
    sock->type = PTK_SOCK_TCP_CLIENT;
    return sock;
}

/**
 * @brief Create a UDP socket.
 */
ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr, bool broadcast) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ptk_set_err(map_errno(errno));
        return NULL;
    }
    if (local_addr) {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = local_addr->ip;
        addr.sin_port = htons(local_addr->port);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(fd);
            ptk_set_err(map_errno(errno));
            return NULL;
        }
    }
    if (broadcast) {
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    }
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), NULL);
    if (!sock) {
        close(fd);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    sock->fd = fd;
    sock->type = PTK_SOCK_UDP;
    return sock;
}

/**
 * @brief Receive UDP data, returning one buffer (blocking).
 */
ptk_buf *ptk_udp_socket_recv_from(ptk_sock *sock, ptk_address_t *sender_addr, ptk_duration_ms timeout_ms) {
    if (!sock || sock->type != PTK_SOCK_UDP) return NULL;
    struct sockaddr_in src_addr;
    socklen_t addrlen = sizeof(src_addr);
    char buf[2048];
    ssize_t n = recvfrom(sock->fd, buf, sizeof(buf), 0, (struct sockaddr *)&src_addr, &addrlen);
    if (n < 0) {
        ptk_set_err(map_errno(errno));
        return NULL;
    }
    if (sender_addr) {
        sender_addr->ip = src_addr.sin_addr.s_addr;
        sender_addr->port = ntohs(src_addr.sin_port);
        sender_addr->family = AF_INET;
        sender_addr->reserved = 0;
    }
    ptk_buf *result = ptk_alloc(sizeof(ptk_buf), NULL);
    if (!result) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    result->data = ptk_alloc(n, NULL);
    if (!result->data) {
        ptk_free(&result);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    memcpy(result->data, buf, n);
    result->data_len = n;
    result->start = 0;
    result->end = n;
    return result;
}

/**
 * @brief Receive UDP data, returning an array of buffers (blocking).
 */
ptk_udp_buf_entry_array_t *ptk_udp_socket_recv_many_from(ptk_sock *sock, bool wait_for_packets, ptk_duration_ms timeout_ms) {
    // For simplicity, just call ptk_udp_socket_recv_from once and wrap result
    ptk_udp_buf_entry_array_t *arr = ptk_alloc(sizeof(ptk_udp_buf_entry_array_t), NULL);
    if (!arr) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    arr->len = 0;
    arr->data = ptk_alloc(sizeof(ptk_udp_buf_entry_t), NULL);
    if (!arr->data) {
        ptk_free(&arr);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    ptk_udp_buf_entry_t entry = {0};
    entry.buf = ptk_udp_socket_recv_from(sock, &entry.sender_addr, timeout_ms);
    if (entry.buf) {
        arr->data[0] = entry;
        arr->len = 1;
    }
    return arr;
}

/**
 * @brief Send UDP data to a specific address (blocking).
 */
ptk_err ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, bool broadcast, ptk_duration_ms timeout_ms) {
    if (!sock || sock->type != PTK_SOCK_UDP || !data || !dest_addr) return PTK_ERR_INVALID_PARAM;
    struct sockaddr_in dst_addr = {0};
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_addr.s_addr = dest_addr->ip;
    dst_addr.sin_port = htons(dest_addr->port);
    ssize_t n = sendto(sock->fd, data->data, data->data_len, 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr));
    if (n < 0) {
        return map_errno(errno);
    }
    return PTK_OK;
}

/**
 * @brief Send UDP data to a specific address using vectored I/O (blocking).
 */
ptk_err ptk_udp_socket_send_many_to(ptk_sock *sock, ptk_udp_buf_entry_array_t *data_array, const ptk_address_t *dest_addr, bool broadcast, ptk_duration_ms timeout_ms) {
    if (!sock || sock->type != PTK_SOCK_UDP || !data_array || !dest_addr) return PTK_ERR_INVALID_PARAM;
    for (size_t i = 0; i < data_array->len; ++i) {
        ptk_udp_buf_entry_t *entry = &data_array->data[i];
        ptk_err err = ptk_udp_socket_send_to(sock, entry->buf, dest_addr, broadcast, timeout_ms);
        if (err != PTK_OK) return err;
    }
    return PTK_OK;
}
