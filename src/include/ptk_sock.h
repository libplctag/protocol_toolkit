#pragma once

/**
 * @file ptk_sock.h
 *
 * This provides a blocking API for socket operations that can be interrupted 
 * by thread signals. All blocking socket operations (connect, accept, send, recv)
 * can be interrupted by abort signals.
 *
 * Socket ownership is automatically transferred to the calling thread when any
 * socket operation is performed. This ensures that each socket is monitored by 
 * only one thread's event system. Socket ownership transfer is implicit and 
 * handled internally by the API.
 *
 * Under the hood this is implemented by a platform-specific event loop (epoll, 
 * kqueue, IOCP) that integrates with the thread signal system.
 */

#include <ptk_defs.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_mem.h>
#include <ptk_os_thread.h>
#include <ptk_utils.h>

// Forward declarations
typedef struct ptk_sock ptk_sock;

/**
 * Socket thread function signature
 * 
 * This function is called in a dedicated thread for each socket.
 * The thread has access to the socket and shared context data.
 *
 * @param socket The socket being handled in this thread
 * @param shared_context Shared memory handle for cross-thread data
 */
typedef void (*ptk_socket_thread_func)(ptk_sock *socket, ptk_shared_handle_t shared_context);

/**
 * Socket types.
 */
typedef enum {
    PTK_SOCK_INVALID,     // Invalid socket type
    PTK_SOCK_TCP_SERVER,  // TCP listening socket
    PTK_SOCK_TCP_CLIENT,  // TCP client socket
    PTK_SOCK_UDP,         // UDP socket
} ptk_sock_type;


//=============================================================================
// COMMON DATA TYPES
//=============================================================================

//=============================================================================
// ADDRESS STRUCTURES AND FUNCTIONS
//=============================================================================

/**
 * Network address structure
 */
typedef struct {
    uint32_t ip;       // IPv4 address in network byte order
    uint16_t port;     // Port number in host byte order
    uint8_t family;    // Address family (AF_INET for IPv4)
    uint8_t reserved;  // Reserved for alignment/future use
} ptk_address_t;


/**
 * @brief Create a new address structure from IP string and port
 *
 * @param ip_string The IP address as a string (e.g., "192.168.1.1" or NULL for INADDR_ANY)
 * @param port The port number in host byte order
 * @return Pointer to allocated address struct, or NULL on failure. Caller must free with ptk_local_free().
 *         On failure, ptk_err_set() is called with the error code.
 */
PTK_API ptk_address_t *ptk_address_create(const char *ip_string, uint16_t port);

/**
 * @brief Convert an address structure to an IP string
 *
 * @param address The address structure to convert
 * @return Allocated string containing IP address, or NULL on failure. Caller must free with ptk_local_free()
 */
PTK_API char *ptk_address_to_string(const ptk_address_t *address);

/**
 * @brief Get the port number from an address structure
 *
 * @param address The address structure
 * @return Port number in host byte order, 0 if address is NULL
 */
PTK_API uint16_t ptk_address_get_port(const ptk_address_t *address);

/**
 * @brief Check if two addresses are equal
 *
 * @param addr1 First address
 * @param addr2 Second address
 * @return true if addresses are equal, false otherwise
 */
PTK_API bool ptk_address_equals(const ptk_address_t *addr1, const ptk_address_t *addr2);


/**
 * @brief Create a new address for any interface (INADDR_ANY)
 *
 * @param port The port number in host byte order
 * @return Pointer to allocated address struct, or NULL on failure. Caller must free with ptk_local_free().
 *         On failure, ptk_err_set() is called with the error code.
 */
PTK_API ptk_address_t *ptk_address_create_any(uint16_t port);

//=============================================================================
// NETWORK DISCOVERY API
//=============================================================================

/**
 * Network interface information structure
 */
typedef struct ptk_network_interface {
    char interface_name[32];     // Interface name (e.g., "eth0", "wlan0") 
    char ip_address[16];         // IP address (e.g., "192.168.1.100")
    char netmask[16];            // Subnet mask (e.g., "255.255.255.0")
    char broadcast[16];          // Broadcast address (e.g., "192.168.1.255")
    char network[16];            // Network address (e.g., "192.168.1.0")
    uint8_t prefix_length;       // CIDR prefix length (e.g., 24 for /24)
    bool is_up;                  // True if interface is up
    bool is_loopback;            // True if this is loopback interface
    bool supports_broadcast;     // True if interface supports broadcast
} ptk_network_interface_t;

// Declare array type for network interfaces
PTK_ARRAY_DECLARE(ptk_network_interface, ptk_network_interface_t);

/**
 * @brief Discover all available network interfaces
 *
 * Returns information about all network interfaces on the system.
 * The returned array and all its contents are managed by a single allocation
 * that can be freed with ptk_local_free().
 *
 * @return Array of network interfaces, or NULL on error.
 *         Use ptk_local_free() to free all associated memory.
 *         Check ptk_get_err() for error details on NULL return.
 *
 * @example
 * ```c
 * ptk_network_interface_array_t *interfaces = ptk_network_list_interfaces();
 * if (interfaces) {
 *     size_t count = ptk_network_interface_array_len(interfaces);
 *     for (size_t i = 0; i < count; i++) {
 *         const ptk_network_interface_t *iface = ptk_network_interface_array_get(interfaces, i);
 *         printf("Interface: %s IP: %s Broadcast: %s\n", 
 *                iface->interface_name, iface->ip_address, iface->broadcast);
 *     }
 *     ptk_local_free(&interfaces);  // Frees everything
 * }
 * ```
 */
ptk_network_interface_array_t *ptk_network_list_interfaces(void);


//=============================================================================
// GENERIC SOCKET OPERATIONS
//=============================================================================


/**
 * @brief Close a socket.
 *
 * Closes the socket. This function is NOT safe to call from any thread.
 * It must only be called from the thread that owns the socket.
 *
 * @param socket The socket to close
 */
void ptk_socket_close(ptk_sock *socket);

//=============================================================================
// TCP Client Sockets
//=============================================================================

/**
 * Connect to a TCP server.
 *
 * Creates a TCP connection to the remote address and waits up to connect_timeout_ms.
 * This operation can be interrupted by abort signals.
 * If interrupted, the connection attempt is aborted and the function returns NULL
 * with ptk_get_err() returning PTK_ERR_SIGNAL.
 *
 * @param remote_addr The remote address to connect to
 * @param connect_timeout_ms Timeout for the connection attempt in milliseconds (0 for infinite)
 * @return A pointer to the connected TCP socket, or NULL on error.
 *         On error, ptk_get_err() provides the error code (PTK_ERR_SIGNAL if interrupted).
 */
ptk_sock *ptk_tcp_connect(const ptk_address_t *remote_addr, ptk_duration_ms connect_timeout_ms);

/**
 * Write data to a TCP socket (blocking).
 *
 * Socket ownership is automatically transferred to the calling thread if needed.
 *
 * @param sock TCP client socket.
 * @param data Buffer containing data to write.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_SIGNAL if interrupted,
 *         PTK_ERR_NETWORK_ERROR on error. On error, ptk_get_err() provides the error code.
 */
ptk_err_t ptk_tcp_socket_send(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms);

/**
 * Read data from a TCP socket (blocking).
 *
 * Socket ownership is automatically transferred to the calling thread if needed.
 * This operation can be interrupted by abort signals.
 *
 * @param sock TCP client socket.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return Buffer containing received data or NULL on error. If NULL is returned, check ptk_get_err() for error details.
 *         PTK_ERR_SIGNAL indicates the operation was interrupted by a thread signal.
 */
ptk_buf *ptk_tcp_socket_recv(ptk_sock *sock, ptk_duration_ms timeout_ms);

//=============================================================================
// TCP Server Sockets
//=============================================================================
/**
 * Create a TCP listening socket.
 *
 * Creates a listening socket and binds it to the local address.
 * Use ptk_tcp_accept() to accept incoming connections.
 *
 * @param local_addr Address to bind the server to
 * @return Server socket handle for accepting connections, or NULL on error
 */
ptk_sock *ptk_tcp_server_create(const ptk_address_t *local_addr);

/**
 * Accept an incoming TCP connection (blocking).
 *
 * Waits for an incoming connection on the server socket and returns
 * a new socket for the accepted client connection. Socket ownership is
 * automatically transferred to the calling thread if needed.
 * This operation can be interrupted by abort signals.
 *
 * @param server_sock TCP server socket created with ptk_tcp_server_create()
 * @param client_addr Output parameter for client's address (can be NULL)
 * @param timeout_ms Timeout in milliseconds (0 for infinite)
 * @return New socket for the client connection, or NULL on error/timeout.
 *         On error, ptk_get_err() provides the error code (PTK_ERR_SIGNAL if interrupted).
 */
ptk_sock *ptk_tcp_accept(ptk_sock *server_sock, ptk_address_t *client_addr, ptk_duration_ms timeout_ms);


//=============================================================================
// UDP Sockets
//=============================================================================


/**
 * Create a UDP socket.
 *
 * Creates a UDP socket.  The broadcast flag determines whether the socket
 * will be used for broadcasting.
 *
 * @param local_addr Address to bind to (NULL for client-only)
 * @param broadcast Whether to enable broadcast support
 * @return Valid UDP socket on success, NULL on failure (ptk_get_err() set)
 */
ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr, bool broadcast);

/**
 * Send UDP data to a specific address (blocking).
 *
 * Socket ownership is automatically transferred to the calling thread if needed.
 * This operation can be interrupted by abort signals.
 *
 * @param sock UDP socket.
 * @param data Buffer containing data to send.
 * @param dest_addr Destination address.
 * @param broadcast Whether to send as broadcast.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_SIGNAL if interrupted,
 *         PTK_ERR_NETWORK_ERROR on error. On error, ptk_get_err() provides the error code.
 */
ptk_err_t ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, bool broadcast, ptk_duration_ms timeout_ms);

/**
 * Receive UDP data (blocking).
 *
 * Socket ownership is automatically transferred to the calling thread if needed.
 * This operation can be interrupted by abort signals.
 *
 * @param sock UDP socket.
 * @param sender_addr Output parameter for sender's address (can be NULL).
 * @param timeout_ms Timeout in milliseconds (0 to loop and get all available packets).
 * @return Buffer containing received packet on success (must be freed with ptk_local_free()), 
 *         NULL on error. Check ptk_get_err() for error details.
 *         PTK_ERR_SIGNAL indicates the operation was interrupted by a thread signal.
 *         When timeout_ms is 0, this function loops internally to collect all available packets.
 */
ptk_buf *ptk_udp_socket_recv_from(ptk_sock *sock, ptk_address_t *sender_addr, ptk_duration_ms timeout_ms);


