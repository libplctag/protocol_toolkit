#include "ev_loop.h"
#include "ev_loop_platform.h"
#include "ev_loop_common.h"
#include "log.h"
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

// Event flags for platform interface
#define EV_READ  0x01
#define EV_WRITE 0x02

//=============================================================================
// INTERNAL STRUCTURES
//=============================================================================



//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * Get current time in milliseconds since epoch
 */
uint64_t ev_now_ms(void) {
    trace("Getting current time in milliseconds");
    
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        uint64_t ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        trace("Current time: %llu ms", (unsigned long long)ms);
        return ms;
    }
    
    // Fallback to time() if clock_gettime fails
    warn("clock_gettime failed, using time() fallback");
    return (uint64_t)time(NULL) * 1000;
}

/**
 * Convert error code to human-readable string
 */
const char* ev_err_string(ev_err err) {
    switch (err) {
        case EV_OK: return "Success";
        case EV_ERR_OUT_OF_BOUNDS: return "Out of bounds";
        case EV_ERR_NULL_PTR: return "Null pointer";
        case EV_ERR_NO_RESOURCES: return "No resources";
        case EV_ERR_BAD_FORMAT: return "Bad format";
        case EV_ERR_INVALID_PARAM: return "Invalid parameter";
        case EV_ERR_NETWORK_ERROR: return "Network error";
        case EV_ERR_CLOSED: return "Closed";
        case EV_ERR_TIMEOUT: return "Timeout";
        case EV_ERR_WOULD_BLOCK: return "Would block";
        case EV_ERR_ADDRESS_IN_USE: return "Address in use";
        case EV_ERR_CONNECTION_REFUSED: return "Connection refused";
        case EV_ERR_HOST_UNREACHABLE: return "Host unreachable";
        default: return "Unknown error";
    }
}

/**
 * Convert event type to human-readable string
 */
const char* ev_event_string(ev_event_type type) {
    switch (type) {
        case EV_EVENT_ACCEPT: return "Accept";
        case EV_EVENT_CONNECT: return "Connect";
        case EV_EVENT_READ: return "Read";
        case EV_EVENT_WRITE_DONE: return "Write done";
        case EV_EVENT_CLOSE: return "Close";
        case EV_EVENT_ERROR: return "Error";
        case EV_EVENT_TICK: return "Tick";
        default: return "Unknown event";
    }
}

//=============================================================================
// SOCKET MANAGEMENT
//=============================================================================

/**
 * Add a socket to the event loop
 */
static ev_err ev_loop_add_socket(ev_loop *loop, ev_sock *sock) {
    trace("Adding socket to event loop");
    
    if (!loop || !sock) {
        error("NULL pointer in ev_loop_add_socket");
        return EV_ERR_NULL_PTR;
    }
    
    // Expand socket array if needed
    if (loop->socket_count >= loop->socket_capacity) {
        size_t new_capacity = loop->socket_capacity ? loop->socket_capacity * 2 : 16;
        ev_sock **new_sockets = realloc(loop->sockets, new_capacity * sizeof(ev_sock*));
        if (!new_sockets) {
            error("Failed to expand socket array");
            return EV_ERR_NO_RESOURCES;
        }
        loop->sockets = new_sockets;
        loop->socket_capacity = new_capacity;
    }
    
    loop->sockets[loop->socket_count++] = sock;
    trace("Socket added to event loop, total sockets: %zu", loop->socket_count);
    return EV_OK;
}

/**
 * Remove a socket from the event loop
 */
static ev_err ev_loop_remove_socket(ev_loop *loop, ev_sock *sock) {
    trace("Removing socket from event loop");
    
    if (!loop || !sock) {
        error("NULL pointer in ev_loop_remove_socket");
        return EV_ERR_NULL_PTR;
    }
    
    // Find and remove the socket
    for (size_t i = 0; i < loop->socket_count; i++) {
        if (loop->sockets[i] == sock) {
            // Shift remaining sockets down
            memmove(&loop->sockets[i], &loop->sockets[i + 1], 
                   (loop->socket_count - i - 1) * sizeof(ev_sock*));
            loop->socket_count--;
            trace("Socket removed from event loop, remaining sockets: %zu", loop->socket_count);
            return EV_OK;
        }
    }
    
    warn("Socket not found in event loop");
    return EV_ERR_INVALID_PARAM;
}

//=============================================================================
// EVENT LOOP MANAGEMENT
//=============================================================================

ev_err ev_loop_create(ev_loop **loop, const ev_loop_opts *opts) {
    trace("Creating event loop");
    
    if (!loop) {
        error("NULL loop pointer");
        return EV_ERR_NULL_PTR;
    }
    
    // Allocate event loop structure
    *loop = calloc(1, sizeof(ev_loop));
    if (!*loop) {
        error("Failed to allocate event loop");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Set default options
    (*loop)->worker_threads = opts && opts->worker_threads ? opts->worker_threads : 0;
    (*loop)->max_events = opts && opts->max_events ? opts->max_events : 1024;
    (*loop)->auto_started = opts ? opts->auto_start : true;
    (*loop)->running = false;
    
    // Initialize platform-specific data
    ev_err result = platform_init(*loop);
    if (result != EV_OK) {
        error("Failed to initialize platform data");
        free(*loop);
        *loop = NULL;
        return result;
    }
    
    // Start worker threads if auto_start is enabled
    if ((*loop)->auto_started) {
        result = platform_start_threads(*loop);
        if (result != EV_OK) {
            error("Failed to start worker threads");
            platform_cleanup(*loop);
            free(*loop);
            *loop = NULL;
            return result;
        }
        (*loop)->running = true;
    }
    
    info("Event loop created with %zu worker threads", (*loop)->worker_threads);
    return EV_OK;
}

ev_err ev_loop_wait(ev_loop *loop) {
    trace("Waiting for event loop to finish");
    
    if (!loop) {
        error("NULL loop pointer");
        return EV_ERR_NULL_PTR;
    }
    
    return platform_wait_threads(loop, 0); // Wait indefinitely
}

ev_err ev_loop_wait_timeout(ev_loop *loop, uint64_t timeout_ms) {
    trace("Waiting for event loop to finish with timeout %llu ms", (unsigned long long)timeout_ms);
    
    if (!loop) {
        error("NULL loop pointer");
        return EV_ERR_NULL_PTR;
    }
    
    return platform_wait_threads(loop, timeout_ms);
}

void ev_loop_stop(ev_loop *loop) {
    trace("Stopping event loop");
    
    if (!loop) {
        error("NULL loop pointer");
        return;
    }
    
    loop->running = false;
    platform_stop_threads(loop);
    
    info("Event loop stopped");
}

bool ev_loop_is_running(ev_loop *loop) {
    return loop ? loop->running : false;
}

void ev_loop_destroy(ev_loop *loop) {
    trace("Destroying event loop");
    
    if (!loop) {
        return;
    }
    
    // Stop the loop if still running
    if (loop->running) {
        ev_loop_stop(loop);
    }
    
    // Cleanup platform-specific data
    platform_cleanup(loop);
    
    // Free socket array
    if (loop->sockets) {
        free(loop->sockets);
    }
    
    free(loop);
    info("Event loop destroyed");
}

ev_err ev_loop_post(ev_loop *loop, void (*callback)(void *user_data), void *user_data) {
    trace("Posting callback to event loop");
    
    if (!loop || !callback) {
        error("NULL pointer in ev_loop_post");
        return EV_ERR_NULL_PTR;
    }
    
    // For now, just call the callback directly
    // A full implementation would queue this for execution on a worker thread
    callback(user_data);
    
    return EV_OK;
}

//=============================================================================
// TCP CLIENT
//=============================================================================

ev_err ev_tcp_connect(ev_loop *loop, ev_sock **client, const ev_tcp_client_opts *opts) {
    trace("Creating TCP client connection");
    
    if (!loop || !client || !opts || !opts->host || !opts->callback) {
        error("Invalid parameters in ev_tcp_connect");
        return EV_ERR_INVALID_PARAM;
    }
    
    // Allocate socket structure
    *client = calloc(1, sizeof(ev_sock));
    if (!*client) {
        error("Failed to allocate client socket");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Initialize socket
    (*client)->type = EV_SOCK_TCP_CLIENT;
    (*client)->sock_state = EV_SOCK_STATE_CREATED;
    (*client)->callback = opts->callback;
    (*client)->user_data = opts->user_data;
    (*client)->read_buffer_size = opts->read_buffer_size ? opts->read_buffer_size : 8192;
    
    // Copy connection details
    strncpy((*client)->remote_host, opts->host, sizeof((*client)->remote_host) - 1);
    (*client)->remote_port = opts->port;
    
    // Add to event loop
    ev_err result = ev_loop_add_socket(loop, *client);
    if (result != EV_OK) {
        error("Failed to add socket to event loop");
        free(*client);
        *client = NULL;
        return result;
    }
    
    // Platform-specific connection would be handled by platform layer
    result = platform_add_socket(loop, *client, EV_READ | EV_WRITE);
    if (result != EV_OK) {
        error("Failed to add socket to platform monitoring");
        ev_loop_remove_socket(loop, *client);
        free(*client);
        *client = NULL;
        return result;
    }
    
    info("TCP client created for %s:%d", opts->host, opts->port);
    return EV_OK;
}

ev_err ev_tcp_write(ev_sock *sock, buf **data) {
    trace("Writing data to TCP socket");
    
    if (!sock || !data || !*data) {
        error("Invalid parameters in ev_tcp_write");
        return EV_ERR_INVALID_PARAM;
    }
    
    // For now, just free the buffer (actual write would be handled by platform layer)
    buf_free(*data);
    *data = NULL;
    
    trace("TCP write operation queued");
    return EV_OK;
}

//=============================================================================
// TCP SERVER
//=============================================================================

ev_err ev_tcp_server_start(ev_loop *loop, ev_sock **server, const ev_tcp_server_opts *opts) {
    trace("Starting TCP server");
    
    if (!loop || !server || !opts || !opts->callback) {
        error("Invalid parameters in ev_tcp_server_start");
        return EV_ERR_INVALID_PARAM;
    }
    
    // Allocate server socket structure
    *server = calloc(1, sizeof(ev_sock));
    if (!*server) {
        error("Failed to allocate server socket");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Initialize server socket
    (*server)->type = EV_SOCK_TCP_SERVER;
    (*server)->sock_state = EV_SOCK_STATE_CREATED;
    (*server)->callback = opts->callback;
    (*server)->user_data = opts->user_data;
    (*server)->read_buffer_size = opts->read_buffer_size ? opts->read_buffer_size : 8192;
    
    // Copy binding details
    const char *bind_host = opts->bind_host ? opts->bind_host : "0.0.0.0";
    strncpy((*server)->local_host, bind_host, sizeof((*server)->local_host) - 1);
    (*server)->local_port = opts->bind_port;
    
    // Add to event loop
    ev_err result = ev_loop_add_socket(loop, *server);
    if (result != EV_OK) {
        error("Failed to add server socket to event loop");
        free(*server);
        *server = NULL;
        return result;
    }
    
    // Platform-specific server creation would be handled by platform layer
    result = platform_add_socket(loop, *server, EV_READ);
    if (result != EV_OK) {
        error("Failed to add server socket to platform monitoring");
        ev_loop_remove_socket(loop, *server);
        free(*server);
        *server = NULL;
        return result;
    }
    
    (*server)->sock_state = EV_SOCK_STATE_LISTENING;
    info("TCP server started on %s:%d", bind_host, opts->bind_port);
    return EV_OK;
}

//=============================================================================
// UDP SOCKETS
//=============================================================================

ev_err ev_udp_create(ev_loop *loop, ev_sock **udp_sock, const ev_udp_opts *opts) {
    trace("Creating UDP socket");
    
    if (!loop || !udp_sock || !opts || !opts->callback) {
        error("Invalid parameters in ev_udp_create");
        return EV_ERR_INVALID_PARAM;
    }
    
    // Allocate UDP socket structure
    *udp_sock = calloc(1, sizeof(ev_sock));
    if (!*udp_sock) {
        error("Failed to allocate UDP socket");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Create the actual UDP socket
    (*udp_sock)->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ((*udp_sock)->fd == -1) {
        error("Failed to create UDP socket: %s", strerror(errno));
        free(*udp_sock);
        *udp_sock = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
    
    // Set socket to non-blocking mode
    ev_err result = set_socket_nonblocking((*udp_sock)->fd);
    if (result != EV_OK) {
        error("Failed to set UDP socket non-blocking");
        close((*udp_sock)->fd);
        free(*udp_sock);
        *udp_sock = NULL;
        return result;
    }
    
    // Initialize UDP socket
    (*udp_sock)->type = EV_SOCK_UDP;
    (*udp_sock)->sock_state = EV_SOCK_STATE_CREATED;
    (*udp_sock)->callback = opts->callback;
    (*udp_sock)->user_data = opts->user_data;
    (*udp_sock)->read_buffer_size = opts->read_buffer_size ? opts->read_buffer_size : 8192;
    
    // Set socket options
    if (opts->broadcast) {
        int broadcast = 1;
        if (setsockopt((*udp_sock)->fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) != 0) {
            error("Failed to set broadcast option: %s", strerror(errno));
            close((*udp_sock)->fd);
            free(*udp_sock);
            *udp_sock = NULL;
            return EV_ERR_NETWORK_ERROR;
        }
    }
    
    if (opts->reuse_addr) {
        result = set_socket_reuse_addr((*udp_sock)->fd, true);
        if (result != EV_OK) {
            error("Failed to set reuse address option");
            close((*udp_sock)->fd);
            free(*udp_sock);
            *udp_sock = NULL;
            return result;
        }
    }
    
    // Bind socket if host is specified
    if (opts->bind_host) {
        strncpy((*udp_sock)->local_host, opts->bind_host, sizeof((*udp_sock)->local_host) - 1);
        (*udp_sock)->local_port = opts->bind_port;
        
        result = bind_socket((*udp_sock)->fd, opts->bind_host, opts->bind_port);
        if (result != EV_OK) {
            error("Failed to bind UDP socket");
            close((*udp_sock)->fd);
            free(*udp_sock);
            *udp_sock = NULL;
            return result;
        }
    } else {
        // Socket is unbound - for sending only
        strcpy((*udp_sock)->local_host, "unbound");
        (*udp_sock)->local_port = 0;
    }
    
    // Add to event loop
    result = ev_loop_add_socket(loop, *udp_sock);
    if (result != EV_OK) {
        error("Failed to add UDP socket to event loop");
        close((*udp_sock)->fd);
        free(*udp_sock);
        *udp_sock = NULL;
        return result;
    }
    
    // Platform-specific UDP creation would be handled by platform layer
    // Only monitor read events for UDP sockets by default (write events are handled on-demand)
    result = platform_add_socket(loop, *udp_sock, EV_READ);
    if (result != EV_OK) {
        error("Failed to add UDP socket to platform monitoring");
        ev_loop_remove_socket(loop, *udp_sock);
        close((*udp_sock)->fd);
        free(*udp_sock);
        *udp_sock = NULL;
        return result;
    }
    
    (*udp_sock)->sock_state = EV_SOCK_STATE_UDP_BOUND;
    info("UDP socket created");
    return EV_OK;
}

ev_err ev_udp_send(ev_sock *sock, buf **data, const char *host, int port) {
    trace("Sending UDP data to %s:%d", host, port);
    
    if (!sock || !data || !*data || !host) {
        error("Invalid parameters in ev_udp_send");
        return EV_ERR_INVALID_PARAM;
    }
    
    // Create destination address
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &dest_addr.sin_addr) != 1) {
        error("Invalid destination address: %s", host);
        buf_free(*data);
        *data = NULL;
        return EV_ERR_INVALID_PARAM;
    }
    
    // Send the data
    ssize_t bytes_sent = sendto(sock->fd, (*data)->data, (*data)->cursor, 0, 
                               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent == -1) {
        int error_code = errno;
        error("UDP send failed to %s:%d - errno=%d (%s)", host, port, error_code, strerror(error_code));
        buf_free(*data);
        *data = NULL;
        return EV_ERR_NETWORK_ERROR;
    }
    
    trace("UDP sent %zd bytes to %s:%d", bytes_sent, host, port);
    buf_free(*data);
    *data = NULL;
    
    return EV_OK;
}

//=============================================================================
// TIMERS
//=============================================================================

ev_err ev_timer_start(ev_loop *loop, ev_sock **timer, const ev_timer_opts *opts) {
    trace("Starting timer");
    
    if (!loop || !timer || !opts) {
        error("Invalid parameters in ev_timer_start");
        return EV_ERR_INVALID_PARAM;
    }
    
    // Allocate timer structure
    *timer = calloc(1, sizeof(ev_sock));
    if (!*timer) {
        error("Failed to allocate timer");
        return EV_ERR_NO_RESOURCES;
    }
    
    // Initialize timer
    (*timer)->type = EV_SOCK_TIMER;
    (*timer)->sock_state = EV_SOCK_STATE_CREATED;
    (*timer)->callback = opts->callback;
    (*timer)->user_data = opts->user_data;
    (*timer)->timeout_ms = opts->timeout_ms;
    (*timer)->timer_repeat = opts->repeat;
    (*timer)->next_fire_time = ev_now_ms() + opts->timeout_ms;
    
    // Use a unique timer ID starting from a high number to avoid conflicts with file descriptors
    static int timer_id_counter = 10000;
    (*timer)->fd = timer_id_counter++;
    
    // Add to event loop
    ev_err result = ev_loop_add_socket(loop, *timer);
    if (result != EV_OK) {
        error("Failed to add timer to event loop");
        free(*timer);
        *timer = NULL;
        return result;
    }
    
    // Add timer to platform monitoring (kqueue)
    result = platform_add_socket(loop, *timer, 0); // Special case for timers
    if (result != EV_OK) {
        error("Failed to add timer to platform monitoring");
        ev_loop_remove_socket(loop, *timer);
        free(*timer);
        *timer = NULL;
        return result;
    }
    
    info("Timer started with %llu ms timeout", (unsigned long long)opts->timeout_ms);
    return EV_OK;
}

ev_err ev_timer_stop(ev_sock *timer) {
    trace("Stopping timer");
    
    if (!timer) {
        error("NULL timer pointer");
        return EV_ERR_NULL_PTR;
    }
    
    // Timer cleanup would be handled by platform layer
    timer->sock_state = EV_SOCK_STATE_CLOSED;
    
    info("Timer stopped");
    return EV_OK;
}

//=============================================================================
// SOCKET OPERATIONS
//=============================================================================

ev_err ev_close(ev_sock *sock) {
    trace("Closing socket");
    
    if (!sock) {
        error("NULL socket pointer");
        return EV_ERR_NULL_PTR;
    }
    
    sock->sock_state = EV_SOCK_STATE_CLOSING;
    
    // Close the actual file descriptor if it's a real socket
    if (sock->fd >= 0 && sock->type != EV_SOCK_TIMER) {
        close(sock->fd);
        sock->fd = -1;
    }
    
    // Free read buffer if allocated
    if (sock->read_buffer) {
        buf_free(sock->read_buffer);
        sock->read_buffer = NULL;
    }
    
    sock->sock_state = EV_SOCK_STATE_CLOSED;
    
    info("Socket closed");
    return EV_OK;
}

ev_sock_type ev_sock_get_type(ev_sock *sock) {
    return sock ? sock->type : EV_SOCK_TCP_CLIENT;
}

ev_err ev_sock_get_local_addr(ev_sock *sock, char *host, size_t host_len, int *port) {
    if (!sock || !host || !port) {
        return EV_ERR_NULL_PTR;
    }
    
    strncpy(host, sock->local_host, host_len - 1);
    host[host_len - 1] = '\0';
    *port = sock->local_port;
    
    return EV_OK;
}

ev_err ev_sock_get_remote_addr(ev_sock *sock, char *host, size_t host_len, int *port) {
    if (!sock || !host || !port) {
        return EV_ERR_NULL_PTR;
    }
    
    strncpy(host, sock->remote_host, host_len - 1);
    host[host_len - 1] = '\0';
    *port = sock->remote_port;
    
    return EV_OK;
}

ev_err ev_sock_wake(ev_sock *sock, void *user_data) {
    trace("Waking socket");
    
    if (!sock) {
        return EV_ERR_NULL_PTR;
    }
    
    // Wake operation would be handled by platform layer
    
    return EV_OK;
}

ev_err ev_sock_set_option(ev_sock *sock, ev_sockopt opt, const void *value, size_t value_len) {
    trace("Setting socket option");
    
    if (!sock || !value) {
        return EV_ERR_NULL_PTR;
    }
    
    // Socket option setting would be handled by platform layer
    
    return EV_OK;
}

ev_err ev_sock_get_option(ev_sock *sock, ev_sockopt opt, void *value, size_t *value_len) {
    trace("Getting socket option");
    
    if (!sock || !value || !value_len) {
        return EV_ERR_NULL_PTR;
    }
    
    // Socket option getting would be handled by platform layer
    
    return EV_OK;
}

//=============================================================================
// NETWORK DISCOVERY
//=============================================================================

ev_err ev_loop_find_networks(ev_network_info **network_info, size_t *num_networks, ev_loop *loop) {
    trace("Finding network interfaces");
    
    if (!network_info || !num_networks) {
        error("NULL pointer in ev_loop_find_networks");
        return EV_ERR_NULL_PTR;
    }
    
    *network_info = NULL;
    *num_networks = 0;
    
    // Delegate to platform-specific implementation
    ev_err result = platform_find_networks(network_info, num_networks);
    if (result != EV_OK) {
        error("Platform network discovery failed: %s", ev_err_string(result));
        return result;
    }
    
    info("Found %zu network interfaces", *num_networks);
    return EV_OK;
}

void ev_network_info_dispose(ev_network_info *network_info, size_t num_networks) {
    trace("Disposing network info for %zu networks", num_networks);
    
    if (!network_info) {
        return;
    }
    
    for (size_t i = 0; i < num_networks; i++) {
        if (network_info[i].network_ip) {
            free(network_info[i].network_ip);
        }
        if (network_info[i].netmask) {
            free(network_info[i].netmask);
        }
        if (network_info[i].broadcast) {
            free(network_info[i].broadcast);
        }
    }
    
    free(network_info);
    trace("Network info disposed");
}