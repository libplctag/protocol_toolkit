#pragma once

/**
 * @file ptk_socket.h
 *
 * A small API for synchronous sockets.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_utils.h>


// Forward declarations
typedef struct ptk_sock ptk_sock;

/*
All operations are synchronous.

In general, all read operations move the end of the passed buffer.  In general all write
operations mmove the start index of the passed buffer.  Buffers are provided by the caller.
*/



//=============================================================================
// GENERIC SOCKET OPERATIONS
//=============================================================================

/**
 * Socket types
 */
typedef enum {
    PTK_SOCK_TCP_SERVER,        // TCP listening socket
    PTK_SOCK_TCP_CLIENT,        // TCP client socket
    PTK_SOCK_UDP,               // UDP socket
} ptk_sock_type;

ptk_err ptk_socket_type(ptk_sock *sock, ptk_sock_type *type);

/**
 * @brief Close the socket. Any type of socket.
 *
 * @param sock
 * @return ptk_err
 */
ptk_err ptk_socket_close(ptk_sock *sock);

/**
 * @brief Set the interrupt handler for a socket.
 *
 * @param sock
 * @param interrupt_handler
 * @param user_data
 * @return ptk_err
 */
ptk_err ptk_socket_set_interrupt_handler(ptk_sock *sock, void (*interrupt_handler)(ptk_sock *sock, ptk_time_ms time_ms, void *user_data), void *user_data);

/**
 * @brief Waits for an interrupt.
 *
 * The ptk_socket_interrupt_once() function wakes up the socket thread.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while waiting.
 *
 * @param sock
 * @return ptk_err PTK_OK on interrupt, PTK_ERR_ABORT if aborted
 */
ptk_err ptk_socket_wait_for_interrupt(ptk_sock *sock);

/**
 * @brief Wait for socket events with timeout.
 *
 * Waits for any events on the socket for the specified timeout period.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while waiting.
 *
 * @param sock
 * @param timeout_ms Timeout in milliseconds
 * @return PTK_OK on event, PTK_ERR_TIMEOUT on timeout, PTK_ERR_ABORT if aborted
 */
ptk_err ptk_socket_wait(ptk_sock *sock, ptk_time_ms timeout_ms);

/**
 * @brief Call the interrupt callback and return from ptk_socket_wait_for_interrupt.
 *
 * Anything waiting on a synchronous operation will fail with the status PTK_ERR_INTERRUPT.
 *
 * @param socket
 * @param timer_period_ms
 * @param wake_callback
 * @param user_data
 * @return ptk_err
 */
ptk_err ptk_socket_start_repeat_interrupt(ptk_sock *socket, ptk_duration_ms timer_period_ms);

/**
 * @brief stop any timer running on a socket.
 *
 * @param socket
 * @return ptk_err
 */
ptk_err ptk_socket_stop_repeat_interrupt(ptk_sock *socket);

/**
 * @brief Raise the event to call the interrupt callback.
 *
 * @param socket
 * @return ptk_err
 */
ptk_err ptk_socket_interrupt_once(ptk_sock *socket);

/**
 * @brief Abort any ongoing socket operations.
 *
 * This function can be called from external threads to stop ongoing
 * operations like send, receive, accept, etc. The stopped operations
 * will return PTK_ERR_ABORT.
 *
 * @param socket
 * @return ptk_err
 */
ptk_err ptk_socket_abort(ptk_sock *socket);


//=============================================================================
// TCP Client Sockets
//=============================================================================

/**
 * Connect to a TCP server
 *
 * Connection is performed synchronously.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while connecting.
 *
 * @param client
 * @param remote_ip
 * @param remote_port
 * @return PTK_OK on success, PTK_ERR_ABORT if aborted, error code on failure
 */
ptk_err ptk_tcp_socket_connect(ptk_sock **client, const char *remote_ip, int remote_port);

/**
 * Write data to a TCP socket
 *
 * The operation is performed synchronously.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while writing.
 *
 * @param sock The TCP client socket to write to
 * @param data Data to write (ownership transferred)
 * @return PTK_OK on success, PTK_ERR_ABORT if aborted, error code on failure
 */
ptk_err ptk_tcp_socket_write(ptk_sock *sock, ptk_buf *data);

/**
 * @brief Read data from the socket.
 *
 * This is synchronous.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while reading.
 *
 * @param sock
 * @param data
 * @return PTK_OK on success, PTK_ERR_ABORT if aborted, error code on failure
 */
ptk_err ptk_tcp_socket_read(ptk_sock *sock, ptk_buf *data);



//=============================================================================
// TCP Server
//=============================================================================


/**
 * Start a TCP server
 *
 * The server will immediately start listening and accepting connections.
 *
 * @param loop The event loop
 * @param server Pointer to store the created server socket
 * @param server_opts Server configuration
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_tcp_socket_listen(ptk_sock **server, const char *local_ip, int local_port, int backlog);


/**
 * @brief Accept a new connection
 *
 * This is done synchronously.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while waiting for connections.
 *
 * @param server
 * @param client
 * @return PTK_OK on success, PTK_ERR_ABORT if aborted, error code on failure
 */
ptk_err ptk_tcp_socket_accept(ptk_sock *server, ptk_sock **client);


//=============================================================================
// UDP SOCKETS
//=============================================================================


/**
 * Create a UDP socket
 *
 * Can be used for both client and server operations.
 * If bind_host/bind_port are specified, the socket is bound for receiving.
 *
 * @param udp_sock Pointer to store the created UDP socket
 * @param local_host the local host IP to bind to.
 * @param local_port the local port to bind to.
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_udp_socket_create(ptk_sock **udp_sock, const char *local_host, int local_port);

/**
 * @brief Send UDP data to a specific address
 *
 * The operation is performed synchronously.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while sending.
 *
 * @param sock The UDP socket
 * @param data Data to send
 * @param host Destination host
 * @param port Destination port
 * @param broadcast If true, send the data as a broadcast.
 * @return PTK_OK on success, PTK_ERR_ABORT if aborted, error code on failure
 */
ptk_err ptk_udp_socket_send(ptk_sock *sock, ptk_buf *data, const char *host, int port, bool broadcast);

/**
 * @brief Get a UDT packet from the socket
 *
 * @param sock
 * @param data received data
 * @param host remote host that sent the packet
 * @param port remote port on the host that sent the packet
 * @return ptk_err
 */
ptk_err ptk_udp_socket_recv(ptk_sock *sock, ptk_buf *data, char *host, int *port);


//=============================================================================
// NETWORK DISCOVERY
//=============================================================================

/**
 * Network interface information
 */
typedef struct {
    char *network_ip;          // Network interface IP address (allocated)
    char *netmask;             // Network mask (allocated)
    char *broadcast;           // Broadcast address (allocated)
} ptk_network_info;

/**
 * Find all network interfaces and their broadcast addresses
 *
 * This function discovers all active network interfaces on the system
 * and returns their IP addresses, netmasks, and calculated broadcast addresses.
 *
 * @param network_info Pointer to store allocated array of network info structures
 * @param num_networks Pointer to store the number of networks found
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_socket_find_networks(ptk_network_info **network_info, size_t *num_networks);

/**
 * Free network information array allocated by ptk_loop_find_networks
 *
 * @param network_info The network information array to free
 * @param num_networks The number of network entries in the array
 */
void ptk_socket_network_info_dispose(ptk_network_info *network_info, size_t num_networks);
