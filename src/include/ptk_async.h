#ifndef PROTOCOLTOOLKIT_H
#define PROTOCOLTOOLKIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Platform-independent thread handle - opaque type
// Implementation details hidden in platform-specific source files
typedef struct ptk_thread_handle ptk_thread_handle_t;

// Thread management functions - implemented per platform
ptk_thread_handle_t* ptk_thread_current(void);
bool ptk_thread_equal(ptk_thread_handle_t* a, ptk_thread_handle_t* b);
void ptk_thread_handle_destroy(ptk_thread_handle_t* handle);

#ifdef __cplusplus
extern "C" {
#endif



/* Error codes */
typedef enum {
    PTK_OK = 0,
    PTK_ERROR_INVALID_HANDLE = -1,
    PTK_ERROR_BUFFER_FULL = -2,
    PTK_ERROR_BUFFER_EMPTY = -3,
    PTK_ERROR_OUT_OF_BOUNDS = -4,
    PTK_ERROR_HANDLE_CLOSED = -5,
    PTK_ERROR_NULL_POINTER = -6,
    PTK_ERROR_WRONG_THREAD = -7
} ptk_error_t;

/* Event Loop */

void ptk_run(void);
void ptk_stop(void);


/* Buffers */

typedef struct {
    uint8_t *data;
    size_t read_index;
    size_t write_index;
    size_t capacity;
    ptk_thread_handle_t* owner_thread;  // Always present, NULL if no thread safety
} ptk_buf_t;

ptk_error_t ptk_buffer_init(ptk_buf_t *buf, uint8_t *backing_data, size_t backing_capacity);

/* Transfer buffer ownership to current thread - use with caution */
ptk_error_t ptk_buffer_transfer_ownership(ptk_buf_t *buf);
/* Check if buffer is owned by current thread */
bool ptk_buffer_owned_by_current_thread(const ptk_buf_t *buf);


static inline size_t ptk_buffer_data_len(const ptk_buf_t *buf) {
    if (!buf) return 0;
    return buf->write_index - buf->read_index;
}

static inline size_t ptk_buffer_free_space(const ptk_buf_t *buf) {
    if (!buf) return 0;
    return buf->capacity - buf->write_index;
}

static inline size_t ptk_buffer_capacity(const ptk_buf_t *buf) {
    if (!buf) return 0;
    return buf->capacity;
}

static inline ptk_error_t ptk_buffer_reset(ptk_buf_t *buf) {
    if (!buf) return PTK_ERROR_NULL_POINTER;
    buf->read_index = 0;
    buf->write_index = 0;
    return PTK_OK;
}

ptk_error_t ptk_buffer_read(ptk_buf_t *buf, uint8_t *data, size_t size, size_t *bytes_read);

ptk_error_t ptk_buffer_write(ptk_buf_t *buf, const uint8_t *data, size_t size, size_t *bytes_written);

ptk_error_t ptk_buffer_peek(const ptk_buf_t *buf, uint8_t *data, size_t size, size_t *bytes_peeked);

/* trim everything before the read index and move the data down to index 0 */
ptk_error_t ptk_buffer_trim(ptk_buf_t *buf);


/* forward declarations */

// TCP client handle
typedef struct ptk_tcp_client_s ptk_tcp_client_t;

// TCP server handle
typedef struct ptk_tcp_server_s ptk_tcp_server_t;

// UDP socket handle
typedef struct ptk_udp_socket_s ptk_udp_socket_t;

// Timer handle
typedef struct ptk_timer_s ptk_timer_t;

/* Callback definitions*/

/* TCP client callbacks */
typedef void (*ptk_tcp_client_connect_cb)(ptk_tcp_client_t *client, int status, void *user_ctx);
typedef void (*ptk_tcp_client_read_cb)(ptk_tcp_client_t *client, const ptk_buf_t *buf, void *user_ctx);


/* TCP server callbacks */
typedef void (*ptk_tcp_server_accept_cb)(ptk_tcp_server_t *server, ptk_tcp_client_t *client, void *user_ctx);

/* UDP callbacks */
typedef void (*ptk_udp_recv_cb)(ptk_udp_socket_t *sock, const ptk_buf_t *buf, const char *from_addr, uint16_t port, void *user_ctx);

/* Timer callbacks */
typedef void (*ptk_timer_cb)(ptk_timer_t *timer, void *user_ctx);

// TCP client (not thread-safe - must be used from single thread)
ptk_tcp_client_t *ptk_tcp_client_create(void *user_ctx);

#ifdef PTK_THREAD_SAFETY
// Thread-safe alternative - allows controlled cross-thread access
typedef struct ptk_tcp_client_safe_s ptk_tcp_client_safe_t;
ptk_tcp_client_safe_t *ptk_tcp_client_safe_create(void *user_ctx);
ptk_error_t ptk_tcp_client_safe_connect(ptk_tcp_client_safe_t *client, const char *host, uint16_t port, ptk_tcp_client_connect_cb cb);
ptk_error_t ptk_tcp_client_safe_write(ptk_tcp_client_safe_t *client, const ptk_buf_t *buf);
void ptk_tcp_client_safe_close(ptk_tcp_client_safe_t *client);
#endif
int ptk_tcp_client_connect(ptk_tcp_client_t *client, const char *host, uint16_t port, ptk_tcp_client_connect_cb cb);
int ptk_tcp_client_write(ptk_tcp_client_t *client, const ptk_buf_t *buf);
int ptk_tcp_client_set_read_cb(ptk_tcp_client_t *client, ptk_tcp_client_read_cb cb);
int ptk_tcp_client_close(ptk_tcp_client_t *client);

// TCP server
ptk_tcp_server_t *ptk_tcp_server_create(void *user_ctx);
int ptk_tcp_server_listen(ptk_tcp_server_t *server, const char *bind_addr, uint16_t port, ptk_tcp_server_accept_cb accept_cb);
void ptk_tcp_server_set_client_read_cb(ptk_tcp_server_t *server, ptk_tcp_client_read_cb read_cb);
void ptk_tcp_server_close(ptk_tcp_server_t *server);

// UDP socket (regular, broadcast, multicast)
ptk_udp_socket_t *ptk_udp_socket_create(void *user_ctx);
int ptk_udp_bind(ptk_udp_socket_t *sock, const char *bind_addr, uint16_t port);
int ptk_udp_enable_broadcast(ptk_udp_socket_t *sock, int enable);
int ptk_udp_join_multicast(ptk_udp_socket_t *sock, const char *multicast_addr);
int ptk_udp_sendto(ptk_udp_socket_t *sock, const char *dest_addr, uint16_t port, const ptk_buf_t *buf);
void ptk_udp_set_recv_cb(ptk_udp_socket_t *sock, ptk_udp_recv_cb cb);
void ptk_udp_close(ptk_udp_socket_t *sock);

// Timers
ptk_timer_t *ptk_timer_start(uint64_t timeout_ms, uint64_t repeat_ms, ptk_timer_cb cb, void *user_ctx);
void ptk_timer_stop(ptk_timer_t *timer);
void ptk_timer_free(ptk_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif // PROTOCOLTOOLKIT_H

