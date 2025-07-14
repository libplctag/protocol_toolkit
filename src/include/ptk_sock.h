#pragma once

/**
 * @file ptk_sock.h
 *
 * This provides a blocking API for socket operations.  The operations can
 * be interrupted by other threads.
 *
 * Under the hood this is implemented by a platform-specific event loop.
 * Multiple threads run their own copies of the event loop.  Each socket is
 * monitored by only one thread. Sockets cannot migrate.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_utils.h>
#include <ptk_array.h>
#include <ptk_mem.h>

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
 * @brief Initialize an address structure from IP string and port
 *
 * @param address Output parameter for the address to initialize
 * @param ip_string The IP address as a string (e.g., "192.168.1.1" or NULL for INADDR_ANY)
 * @param port The port number in host byte order
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_address_init(ptk_address_t *address, const char *ip_string, uint16_t port);

/**
 * @brief Convert an address structure to an IP string
 *
 * @param address The address structure to convert
 * @return Allocated string containing IP address, or NULL on failure. Caller must free with ptk_free()
 */
char *ptk_address_to_string(const ptk_address_t *address);

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
 * @brief Initialize an address for any interface (INADDR_ANY)
 *
 * @param address Output parameter for the address to initialize
 * @param port The port number in host byte order
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_address_init_any(ptk_address_t *address, uint16_t port);

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
 * that can be freed with ptk_free().
 *
 * @return Array of network interfaces, or NULL on error.
 *         Use ptk_free() to free all associated memory.
 *         Check ptk_get_err() for error details on NULL return.
 *
 * @example
 * ```c
 * ptk_network_interface_array_t *interfaces = ptk_network_discover_interfaces();
 * if (interfaces) {
 *     size_t count = ptk_network_interface_array_len(interfaces);
 *     for (size_t i = 0; i < count; i++) {
 *         const ptk_network_interface_t *iface = ptk_network_interface_array_get(interfaces, i);
 *         printf("Interface: %s IP: %s Broadcast: %s\n", 
 *                iface->interface_name, iface->ip_address, iface->broadcast);
 *     }
 *     ptk_free(&interfaces);  // Frees everything
 * }
 * ```
 */
ptk_network_interface_array_t *ptk_network_discover_interfaces(void);


//=============================================================================
// GENERIC SOCKET OPERATIONS
//=============================================================================



/**
 * Abort any ongoing socket operations.
 * Blocking calls will return PTK_WAIT_ERROR, and ptk_get_err() will be set to PTK_ERR_ABORT.
 *
 * @param sock The socket to abort.
 * @return PTK_OK on success, error code on failure.
 */
ptk_err ptk_socket_abort(ptk_sock *sock);

/**
 * @brief Wait until either the timeout occurs or the socket is signalled.
 *
 * @return TIMEOUT err if timed out, ABORT if aborted, SIGNAL if signalled.
 */
ptk_err ptk_socket_wait(ptk_sock *sock, ptk_duration_ms timeout_ms);


/**
 * @brief Signal a socket
 *
 * This causes the event infrastructure to wake up the socket.  The response
 * depends on the operation ongoing at that time.  Socket connect, read, write
 * and accept will ignore this.  ptk_socket_wait() will immediately return with
 * a SIGNAL error.
 */
ptk_err ptk_socket_signal(ptk_sock *sock);

/**
 * @brief Close a socket and stop its dedicated thread
 *
 * Closes the socket and signals its dedicated thread to terminate.
 * This function is safe to call from any thread.
 *
 * @param socket The socket to close
 */
void ptk_socket_close(ptk_sock *socket);

//=============================================================================
// TCP Client Sockets
//=============================================================================

/**
 * Connect to a TCP server and start a dedicated thread.
 *
 * Creates a TCP connection to the remote address and spawns a dedicated thread
 * that will run the provided thread function with the socket and shared context.
 *
 * @param remote_addr The remote address to connect to
 * @param thread_func Function to run in the socket's dedicated thread
 * @param shared_context Shared memory handle for cross-thread data
 * @return A pointer to the connected TCP socket, or NULL on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_sock *ptk_tcp_connect(const ptk_address_t *remote_addr,
                         ptk_socket_thread_func thread_func,
                         ptk_shared_handle_t shared_context);

/**
 * Write data to a TCP socket (blocking).
 *
 * @param sock TCP client socket.
 * @param data Buffer containing data to write.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_err ptk_tcp_socket_send(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms);

/**
 * Read data from a TCP socket (blocking).
 *
 * @param sock TCP client socket.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return Returns a buffer containing the received data on success (must be freed with ptk_local_free()),
 *         or NULL on error. If NULL is returned, check ptk_get_err() for error details.
 */
ptk_buf *ptk_tcp_socket_recv(ptk_sock *sock, ptk_duration_ms timeout_ms);

//=============================================================================
// TCP Server Sockets
//=============================================================================

/**
 * Start a TCP server that accepts connections.
 *
 * Creates a listening socket, binds it to the local address, and spawns a thread
 * to accept connections in a loop. For each accepted connection, spawns a dedicated 
 * thread that runs the provided thread function with the client socket and shared context.
 * 
 * Returns immediately with a server socket handle that can be used to control the server.
 *
 * @param local_addr Address to bind the server to
 * @param thread_func Function to run for each accepted client connection
 * @param shared_context Shared memory handle for cross-thread data
 * @return Server socket handle for controlling the server, or NULL on error
 */
ptk_sock *ptk_tcp_server_start(const ptk_address_t *local_addr,
                              ptk_socket_thread_func thread_func,
                              ptk_shared_handle_t shared_context);

//=============================================================================
// UDP Sockets
//=============================================================================


/**
 * Create a UDP socket with a dedicated thread.
 *
 * Creates a UDP socket and spawns a dedicated thread that will run the provided
 * thread function with the socket and shared context.
 *
 * @param local_addr Address to bind to (NULL for client-only)
 * @param broadcast Whether to enable broadcast support
 * @param thread_func Function to run in the socket's dedicated thread
 * @param shared_context Shared memory handle for cross-thread data
 * @return Valid UDP socket on success, NULL on failure (ptk_get_err() set)
 */
ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr, 
                               bool broadcast,
                               ptk_socket_thread_func thread_func,
                               ptk_shared_handle_t shared_context);

/**
 * Send UDP data to a specific address (blocking).
 *
 * @param sock UDP socket.
 * @param data Buffer containing data to send.
 * @param dest_addr Destination address.
 * @param broadcast Whether to send as broadcast.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error.
 *         On error, ptk_get_err() provides the error code.
 */
ptk_err ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, bool broadcast, ptk_duration_ms timeout_ms);

/**
 * Receive UDP data (blocking).
 *
 * @param sock UDP socket.
 * @param sender_addr Output parameter for sender's address (can be NULL).
 * @param timeout_ms Timeout in milliseconds (0 to loop and get all available packets).
 * @return Buffer containing received packet on success (must be freed with ptk_local_free()), 
 *         NULL on error. Check ptk_get_err() for error details.
 *         When timeout_ms is 0, this function loops internally to collect all available packets.
 */
ptk_buf *ptk_udp_socket_recv_from(ptk_sock *sock, ptk_address_t *sender_addr, ptk_duration_ms timeout_ms);


//=============================================================================
// NETWORK DISCOVERY
//=============================================================================

/**
 * Network interface information
 */

#include <net/if.h>

typedef struct {
    char interface_name[32];     // Interface name (e.g., "eth0", "wlan0")
    char ip_address[16];         // IP address (e.g., "192.168.1.100")
    char netmask[16];            // Subnet mask (e.g., "255.255.255.0")
    char broadcast[16];          // Broadcast address (e.g., "192.168.1.255")
    bool is_up;                  // True if interface is up
    bool is_loopback;            // True if this is loopback interface
    bool supports_broadcast;     // True if interface supports broadcast
} ptk_network_info_entry;

typedef struct ptk_network_info ptk_network_info;

/**
 * Find all network interfaces and their broadcast addresses
 *
 * This function discovers all active network interfaces on the system
 * and returns their IP addresses, netmasks, and calculated broadcast addresses.
 *
 * Use ptk_free() to free the returned structure and its contents when done.
 *
 * @return A valid network info pointer on success, NULL on failure, sets last error.
 */
ptk_network_info *ptk_socket_network_list(void);

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
