#ifndef PTK_DGRAM_MEM_SOCKET_IMPL_H
#define PTK_DGRAM_MEM_SOCKET_IMPL_H

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

// Configuration

#define PTK_DGRAM_MEM_MSG_SIZE 256

// In-memory DGRAM socket for POSIX: circular buffer + notification pipe

typedef struct {
    uint8_t buffer[PTK_DGRAM_MEM_MSG_SIZE];
    int full; // 0 = empty, 1 = full
    int notify_pipe[2]; // [0]=read, [1]=write
} ptk_dgram_mem_socket_t;

// buffer_slice is ignored in this implementation (for API compatibility)
static inline ptk_status_t ptk_dgram_mem_socket_init(ptk_dgram_mem_socket_t *sock, ptk_slice_bytes_t buffer_slice) {
    if (!sock) return PTK_ERROR_INVALID_PARAM;
    memset(sock, 0, sizeof(*sock));
    sock->full = 0;
    if (pipe(sock->notify_pipe) < 0) return PTK_ERROR_OS;
    fcntl(sock->notify_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(sock->notify_pipe[1], F_SETFL, O_NONBLOCK);
    return PTK_OK;
}

static inline int ptk_dgram_mem_socket_get_fd(ptk_dgram_mem_socket_t *sock) {
    return sock->notify_pipe[0];
}

static inline ssize_t ptk_dgram_mem_socket_send(ptk_dgram_mem_socket_t *sock, const void *msg, size_t msg_size, uint32_t timeout_ms) {
    if (!sock || !msg || msg_size != PTK_DGRAM_MEM_MSG_SIZE) return -1;
    while (1) {
        if (sock->full) {
            // Buffer full, block or return
            if (timeout_ms == 0) return -1;
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock->notify_pipe[0], &rfds);
            int r = select(sock->notify_pipe[0]+1, &rfds, NULL, NULL, &tv);
            if (r <= 0) return -1; // timeout or error
            continue;
        }
        break;
    }
    memcpy(sock->buffer, msg, PTK_DGRAM_MEM_MSG_SIZE);
    sock->full = 1;
    // Notify reader
    uint8_t b = 1;
    write(sock->notify_pipe[1], &b, 1);
    return PTK_DGRAM_MEM_MSG_SIZE;
}

static inline ssize_t ptk_dgram_mem_socket_recv(ptk_dgram_mem_socket_t *sock, void *msg, size_t msg_size, uint32_t timeout_ms) {
    if (!sock || !msg || msg_size != PTK_DGRAM_MEM_MSG_SIZE) return -1;
    while (1) {
        if (!sock->full) {
            // Buffer empty, block or return
            if (timeout_ms == 0) return -1;
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock->notify_pipe[0], &rfds);
            int r = select(sock->notify_pipe[0]+1, &rfds, NULL, NULL, &tv);
            if (r <= 0) return -1; // timeout or error
            continue;
        }
        break;
    }
    memcpy(msg, sock->buffer, PTK_DGRAM_MEM_MSG_SIZE);
    sock->full = 0;
    // Drain notify pipe
    uint8_t b;
    read(sock->notify_pipe[0], &b, 1);
    return PTK_DGRAM_MEM_MSG_SIZE;
}

#endif // PTK_DGRAM_MEM_SOCKET_IMPL_H
