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
#include <ptk_alloc.h>


// Forward declarations
typedef struct ptk_sock ptk_sock;

/*
All operations are synchronous.

In general, all read operations move the end of the passed buffer.  In general all write
operations mmove the start index of the passed buffer.  Buffers are provided by the caller.
*/



//=============================================================================
// ADDRESS STRUCTURES AND FUNCTIONS
//=============================================================================

/**
 * Network address structure
 */
typedef struct {
    uint32_t ip;        // IPv4 address in network byte order
    uint16_t port;      // Port number in host byte order
    uint8_t family;     // Address family (AF_INET for IPv4)
    uint8_t reserved;   // Reserved for alignment/future use
} ptk_address_t;

/**
 * @brief Create an address structure from IP string and port
 *
 * @param address Output parameter for the created address
 * @param ip_string The IP address as a string (e.g., "192.168.1.1" or NULL for INADDR_ANY)
 * @param port The port number in host byte order
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_address_create(ptk_address_t *address, const char *ip_string, uint16_t port);

/**
 * @brief Convert an address structure to an IP string
 *
 * @param allocator The allocator to use for string allocation
 * @param address The address structure to convert
 * @return Allocated string containing IP address, or NULL on failure. Caller must free with ptk_free()
 */
char *ptk_address_to_string(ptk_allocator_t *allocator, const ptk_address_t *address);

/**
 * @brief Get the port number from an address structure
 *
 * @param address The address structure
 * @return Port number in host byte order, 0 if address is NULL
 */
uint16_t ptk_address_get_port(const ptk_address_t *address);

/**
 * @brief Check if two addresses are equal
 *
 * @param addr1 First address
 * @param addr2 Second address
 * @return true if addresses are equal, false otherwise
 */
bool ptk_address_equals(const ptk_address_t *addr1, const ptk_address_t *addr2);

/**
 * @brief Create an address for any interface (INADDR_ANY)
 *
 * @param address Output parameter for the created address
 * @param port The port number in host byte order
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_address_create_any(ptk_address_t *address, uint16_t port);

//=============================================================================
// GENERIC SOCKET OPERATIONS
//=============================================================================

/**
 * Socket types
 */
typedef enum {
    PRK_SOCK_INVALID,           // Invalid socket type
    PTK_SOCK_TCP_SERVER,        // TCP listening socket
    PTK_SOCK_TCP_CLIENT,        // TCP client socket
    PTK_SOCK_UDP,               // UDP socket
} ptk_sock_type;

/**
 * @brief The success or failure of any operation on a socket is stored in the last error.
 */

/**
 * @brief Return the socket type.
 *
 * @param sock The socket to get the type for
 * @return socket type on success, PTK_SOCK_INVALID on error
 */
ptk_sock_type ptk_socket_type(ptk_sock *sock);

/**
 * @brief Close the socket. Any type of socket.
 *
 * @param sock The socket to close
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_socket_close(ptk_sock *sock);

/**
 * @brief Set the interrupt handler for a socket.
 *
 * @param sock The socket to set the handler on
 * @param interrupt_handler Function to call when interrupt occurs
 * @param user_data User data to pass to the interrupt handler
 * @return PTK_OK on success, error code on failure
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
 * @param sock The socket to wait for interrupt on
 * @return PTK_OK on interrupt, PTK_ERR_ABORT if aborted
 */
ptk_err ptk_socket_wait_for_interrupt(ptk_sock *sock);

/**
 * @brief Set up a periodic interrupt event.
 *
 * Anything waiting on a synchronous operation will fail with the status PTK_ERR_INTERRUPT.
 *
 * @param socket The socket to set the timer on
 * @param timer_period_ms Timer period in milliseconds
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_socket_start_repeat_interrupt(ptk_sock *socket, ptk_duration_ms timer_period_ms);

/**
 * @brief Stop any timer running on a socket.
 *
 * @param socket The socket to stop the timer on
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_socket_stop_repeat_interrupt(ptk_sock *socket);

/**
 * @brief Raise the event to call the interrupt callback.
 *
 * @param socket The socket to raise the interrupt on
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_socket_interrupt_once(ptk_sock *socket);

/**
 * @brief Abort any ongoing socket operations.
 *
 * This function can be called from external threads to stop ongoing
 * operations like send, receive, accept, etc. The stopped operations
 * will return PTK_ERR_ABORT.
 *
 * @param socket The socket to abort operations on
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_socket_abort(ptk_sock *socket);

/**
 * @brief Return the last status on the socket
 *
 * The status is set by the last operation or set to PTK_ERR_ABORT if the
 * socket was aborted.
 *
 * @param sock The socket to get the last error for
 * @return The last error code for this socket
 */
ptk_err ptk_socket_last_error(ptk_sock *sock);


//=============================================================================
// TCP Client Sockets
//=============================================================================

/**
 * Connect to a TCP server
 *
 * Connection is performed synchronously.
 *
 * **Abort Behavior**: Returns NULL immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while connecting.
 *
 * @param allocator The allocator to use for socket creation
 * @param remote_addr The remote address to connect to
 * @return a valid client socket on success, NULL on failure or abort.
 */
ptk_sock *ptk_tcp_socket_connect(ptk_allocator_t *allocator, const ptk_address_t *remote_addr);

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
 * @param data
 * @param sock
 * @return PTK_OK on success, PTK_ERR_ABORT if aborted, error code on failure
 */
ptk_err ptk_tcp_socket_read(ptk_buf *data, ptk_sock *sock);



//=============================================================================
// TCP Server
//=============================================================================


/**
 * Start a TCP server
 *
 * The server will immediately start listening and accepting connections.
 *
 * @param allocator The allocator to use for creating this socket and any of its operations in the future
 * @param local_addr The local address to bind to (use ptk_address_create_any for all interfaces)
 * @param backlog The maximum number of pending connections
 * @return a valid socket pointer on success, NULL on failure, last err set.
 */
ptk_sock *ptk_tcp_socket_listen(ptk_allocator_t *allocator, const ptk_address_t *local_addr, int backlog);


/**
 * @brief Accept a new connection
 *
 * This is done synchronously.
 *
 * **Abort Behavior**: Returns PTK_ERR_ABORT immediately if ptk_socket_abort()
 * has been called on this socket, or if abort is called while waiting for connections.
 *
 * @param server
 * @return a valid socket pointer on success, NULL on failure, last err set.
 */
ptk_sock *ptk_tcp_socket_accept(ptk_sock *server);


//=============================================================================
// UDP SOCKETS
//=============================================================================


/**
 * Create a UDP socket
 *
 * Can be used for both client and server operations.
 * If local_addr is specified, the socket is bound for receiving.
 *
 * @param allocator The allocator to use for creating this socket and any of its operations in the future
 * @param local_addr The local address to bind to (NULL for client-only socket)
 * @return a valid socket pointer on success, NULL on failure, last err set.
 */
ptk_sock *ptk_udp_socket_create(ptk_allocator_t *allocator, const ptk_address_t *local_addr);

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
 * @param dest_addr Destination address
 * @param broadcast If true, send the data as a broadcast
 * @return PTK_OK on success, PTK_ERR_ABORT if aborted, error code on failure
 */
ptk_err ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, bool broadcast);

/**
 * @brief Get a UDP packet from the socket
 *
 * @param sock The UDP socket
 * @param data Buffer to receive data into
 * @param sender_addr Address of the sender (output parameter, can be NULL)
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_udp_socket_recv_from(ptk_sock *sock, ptk_buf *data, ptk_address_t *sender_addr);


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
} ptk_network_info_entry;

typedef struct ptk_network_info ptk_network_info;

/**
 * Find all network interfaces and their broadcast addresses
 *
 * This function discovers all active network interfaces on the system
 * and returns their IP addresses, netmasks, and calculated broadcast addresses.
 *
 * @param allocator - the allocator to use to allocate memory for the results.
 * @return A valid network info pointer on success, NULL on failure, sets last error.
 */
ptk_network_info *ptk_socket_find_networks(ptk_allocator_t *allocator);

/**
 * Get the number of network interface entries
 *
 * @param network_info The network information structure
 * @return Number of network entries, 0 if network_info is NULL
 */
size_t ptk_socket_network_info_count(const ptk_network_info *network_info);

/**
 * Get a specific network interface entry by index
 *
 * @param network_info The network information structure
 * @param index Index of the entry to retrieve (0-based)
 * @return Pointer to network entry, NULL if index is out of bounds or network_info is NULL
 */
const ptk_network_info_entry *ptk_socket_network_info_get(const ptk_network_info *network_info, size_t index);

/**
 * Free network information structure allocated by ptk_socket_find_networks
 *
 * @param network_info The network information structure to free
 */
void ptk_socket_network_info_dispose(ptk_network_info *network_info);
