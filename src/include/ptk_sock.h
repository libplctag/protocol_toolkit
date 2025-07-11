#pragma once

/**
 * @file ptk_sock.h
 *
 * Synchronous/blocking socket API over an event loop.
 * All blocking socket operations use the waitable API and return a ptk_wait_status.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_waitable.h>

// Forward declarations
typedef struct ptk_sock ptk_sock;

/**
 * Socket types.
 */
typedef enum {
    PTK_SOCK_INVALID,           // Invalid socket type
    PTK_SOCK_TCP_SERVER,        // TCP listening socket
    PTK_SOCK_TCP_CLIENT,        // TCP client socket
    PTK_SOCK_UDP,               // UDP socket
} ptk_sock_type;

//=============================================================================
// GENERIC SOCKET OPERATIONS
//=============================================================================

/**
 * Get socket type.
 *
 * @param sock The socket to query.
 * @return Socket type.
 */
ptk_sock_type ptk_socket_type(ptk_sock *sock);

/**
 * Close the socket.
 *
 * @param sock The socket to close.
 * @return PTK_OK on success, error code on failure.
 */
ptk_err ptk_socket_close(ptk_sock *sock);

/**
 * Abort any ongoing socket operations.
 * Blocking calls will return PTK_WAIT_ERROR, and ptk_get_err() will be set to PTK_ERR_ABORT.
 *
 * @param sock The socket to abort.
 * @return PTK_OK on success, error code on failure.
 */
ptk_err ptk_socket_abort(ptk_sock *sock);

//=============================================================================
// TCP Client Sockets
//=============================================================================

/**
 * Connect to a TCP server (blocking).
 *
 * @param remote_addr The remote address to connect to.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_WAIT_OK on connection, PTK_WAIT_TIMEOUT on timeout, PTK_WAIT_ERROR on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_wait_status ptk_tcp_socket_connect(ptk_sock *sock, const ptk_address_t *remote_addr, uint32_t timeout_ms);

/**
 * Write data to a TCP socket (blocking).
 *
 * @param sock TCP client socket.
 * @param data Data to write.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_WAIT_OK on success, PTK_WAIT_TIMEOUT on timeout, PTK_WAIT_ERROR on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_wait_status ptk_tcp_socket_write(ptk_sock *sock, ptk_buf *data, uint32_t timeout_ms);

/**
 * Read data from a TCP socket (blocking).
 *
 * @param sock TCP client socket.
 * @param data Buffer to receive data.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_WAIT_OK on success, PTK_WAIT_TIMEOUT on timeout, PTK_WAIT_ERROR on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_wait_status ptk_tcp_socket_read(ptk_sock *sock, ptk_buf *data, uint32_t timeout_ms);

//=============================================================================
// TCP Server Sockets
//=============================================================================

/**
 * Listen on a local address as a TCP server.
 *
 * @param local_addr Address to bind to.
 * @param backlog Maximum number of pending connections.
 * @return Valid server socket on success, NULL on failure (ptk_get_err() set).
 */
ptk_sock *ptk_tcp_socket_listen(const ptk_address_t *local_addr, int backlog);

/**
 * Accept a new TCP connection (blocking).
 *
 * @param server Listening server socket.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return Valid client socket on PTK_WAIT_OK, NULL on PTK_WAIT_ERROR or PTK_WAIT_TIMEOUT (ptk_get_err() set).
 */
ptk_wait_status ptk_tcp_socket_accept(ptk_sock *server, ptk_sock **out_client, uint32_t timeout_ms);

//=============================================================================
// UDP Sockets
//=============================================================================

/**
 * Create a UDP socket.
 *
 * @param local_addr Address to bind to (NULL for client-only).
 * @return Valid UDP socket on success, NULL on failure (ptk_get_err() set).
 */
ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr);

/**
 * Send UDP data to a specific address (blocking).
 *
 * @param sock UDP socket.
 * @param data Data to send.
 * @param dest_addr Destination address.
 * @param broadcast Whether to send as broadcast.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_WAIT_OK on success, PTK_WAIT_TIMEOUT on timeout, PTK_WAIT_ERROR on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_wait_status ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, bool broadcast, uint32_t timeout_ms);

/**
 * Receive UDP data (blocking).
 *
 * @param sock UDP socket.
 * @param data Buffer to receive data.
 * @param sender_addr Output parameter for sender's address (can be NULL).
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_WAIT_OK on data receipt, PTK_WAIT_TIMEOUT on timeout, PTK_WAIT_ERROR on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_wait_status ptk_udp_socket_recv_from(ptk_sock *sock, ptk_buf *data, ptk_address_t *sender_addr, uint32_t timeout_ms);
