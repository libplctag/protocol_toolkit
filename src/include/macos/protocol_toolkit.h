/**
 * @file protocol_toolkit.h
 * @brief Protocol Toolkit API v4 - macOS Implementation
 *
 * macOS-specific implementation using:
 * - Grand Central Dispatch (GCD) for event loops
 * - dispatch_source_t for timers and socket events
 * - BSD sockets for networking
 * - dispatch_queue_t for thread-safe event raising
 *
 * Design Principles:
 * - Zero global state - all resources are application-managed
 * - Zero runtime allocation - all memory pre-allocated at compile time
 * - Event-loop-centric resource management using GCD
 * - Handle-based safety with generation counters
 */

#ifndef PROTOCOL_TOOLKIT_H
#define PROTOCOL_TOOLKIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dispatch/dispatch.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CORE TYPES AND CONSTANTS
 * ======================================================================== */

/**
 * @brief The buffer type
 *
 * The application is responsible for providing buffers for data transfer.
 */
typedef struct ptk_buffer {
    uint8_t *data;   /**< Pointer to the buffer data */
    size_t size;     /**< Size of the buffer */
    size_t capacity; /**< Capacity of the buffer */
} ptk_buffer_t;

/**
 * @brief Initialize a buffer
 *
 * This initializes a buffer with the provided data and capacity.
 *
 * @param data Pointer to the buffer data
 * @param capacity Capacity of the buffer
 * @return ptk_buffer_t Initialized buffer
 */
static inline ptk_buffer_t ptk_buffer_create(uint8_t *data, size_t capacity) {
    ptk_buffer_t buffer;
    buffer.data = data;
    buffer.size = 0;
    buffer.capacity = capacity;
    return buffer;
}

/**
 * @brief Opaque handle type for all protocol toolkit resources
 *
 * Handles are 64-bit values containing:
 * - Type field (8 bits): Resource type identifier
 * - Event Loop ID (8 bits): Which event loop owns this resource
 * - Generation (16 bits): Incremented on slot reuse to prevent stale handles
 * - Handle ID (32 bits): Unique identifier within the event loop
 */
typedef int64_t ptk_handle_t;

/**
 * @brief Error codes returned by protocol toolkit functions
 */
typedef enum {
    PTK_ERR_OK = 0,                  /**< Success */
    PTK_ERR_INVALID_HANDLE = -1,     /**< Handle is invalid or stale */
    PTK_ERR_INVALID_ARGUMENT = -2,   /**< Invalid function argument */
    PTK_ERR_OUT_OF_MEMORY = -3,      /**< No available resource slots */
    PTK_ERR_NOT_SUPPORTED = -4,      /**< Operation not supported on this platform */
    PTK_ERR_NETWORK_ERROR = -5,      /**< Network operation failed */
    PTK_ERR_TIMEOUT = -6,            /**< Operation timed out */
    PTK_ERR_WOULD_BLOCK = -7,        /**< Operation would block (try again) */
    PTK_ERR_CONNECTION_REFUSED = -8, /**< Connection refused by remote host */
    PTK_ERR_CONNECTION_RESET = -9,   /**< Connection reset by peer */
    PTK_ERR_NOT_CONNECTED = -10,     /**< Socket not connected */
    PTK_ERR_ALREADY_CONNECTED = -11, /**< Socket already connected */
    PTK_ERR_ADDRESS_IN_USE = -12,    /**< Address already in use */
    PTK_ERR_NO_ROUTE = -13,          /**< No route to host */
    PTK_ERR_MESSAGE_TOO_LARGE = -14, /**< Message too large for transport */
    PTK_ERR_PROTOCOL_ERROR = -15     /**< Protocol-specific error */
} ptk_err_t;

/**
 * @brief Resource type identifiers (internal use)
 */
typedef enum {
    PTK_TYPE_INVALID = 0,
    PTK_TYPE_EVENT_LOOP = 1,
    PTK_TYPE_TIMER = 2,
    PTK_TYPE_SOCKET = 3,
    PTK_TYPE_USER_EVENT_SOURCE = 4,
    PTK_TYPE_PROTOTHREAD = 5
} ptk_resource_type_t;

/**
 * @brief Event types that can be raised on resources
 */
typedef enum {
    PTK_EVENT_TIMER_EXPIRED = 1,       /**< Timer has expired */
    PTK_EVENT_SOCKET_READABLE = 2,     /**< Socket has data to read */
    PTK_EVENT_SOCKET_WRITABLE = 3,     /**< Socket is ready for writing */
    PTK_EVENT_SOCKET_CONNECTED = 4,    /**< Socket connection established */
    PTK_EVENT_SOCKET_DISCONNECTED = 5, /**< Socket connection lost */
    PTK_EVENT_SOCKET_ERROR = 6,        /**< Socket error occurred */
    PTK_EVENT_USER_DEFINED = 1000      /**< Base for user-defined events */
} ptk_event_type_t;

/* ========================================================================
 * HANDLE MANIPULATION MACROS
 * ======================================================================== */

/**
 * @brief Extract resource type from handle
 */
#define PTK_HANDLE_TYPE(h) ((uint8_t)((h) & 0xFF))

/**
 * @brief Extract event loop ID from handle
 */
#define PTK_HANDLE_EVENT_LOOP_ID(h) ((uint8_t)(((h) >> 8) & 0xFF))

/**
 * @brief Extract generation counter from handle
 */
#define PTK_HANDLE_GENERATION(h) ((uint16_t)(((h) >> 16) & 0xFFFF))

/**
 * @brief Extract handle ID from handle
 */
#define PTK_HANDLE_ID(h) ((uint32_t)((h) >> 32))

/**
 * @brief Create a handle from components (internal use)
 */
#define PTK_MAKE_HANDLE(type, loop_id, gen, id) \
    ((ptk_handle_t)(type) | ((ptk_handle_t)(loop_id) << 8) | ((ptk_handle_t)(gen) << 16) | ((ptk_handle_t)(id) << 32))

/* ========================================================================
 * MACOS-SPECIFIC RESOURCE STRUCTURES
 * ======================================================================== */

/**
 * @brief Base structure for all protocol toolkit resources
 */
typedef struct ptk_resource_base {
    ptk_handle_t handle;     /**< Complete handle (0 = unused slot) */
    ptk_handle_t event_loop; /**< Handle of owning event loop */
} ptk_resource_base_t;

/**
 * @brief Event handler function type
 */
typedef void (*ptk_event_handler_func_t)(ptk_handle_t resource, ptk_event_type_t event_type, void *event_data, void *user_data);

/**
 * @brief Protothread state structure
 *
 * This structure is designed to be the first field in application-specific
 * context structures, allowing protothread functions to cast the pt pointer
 * to access the full application context.
 */
typedef struct ptk_pt {
    uint32_t magic;                    /**< Magic number for type safety (PTK_PT_MAGIC) */
    unsigned short lc;                 /**< Line continuation state */
    void (*function)(struct ptk_pt *); /**< Protothread function pointer */
} ptk_pt_t;

#define PTK_PT_MAGIC 0x50544B50 /**< "PTKP" in ASCII */

/**
 * @brief Event handler registration
 */
typedef struct ptk_event_handler {
    ptk_event_type_t event_type;      /**< Type of event this handler processes */
    ptk_event_handler_func_t handler; /**< Handler function pointer (NULL if using protothread) */
    ptk_pt_t *protothread;            /**< Protothread pointer (NULL if using function handler) */
    void *user_data;                  /**< User-provided context data (for function handlers) */
    bool is_active;                   /**< Whether this handler is currently active */
} ptk_event_handler_t;

/**
 * @brief macOS-specific timer implementation using dispatch_source_t
 */
typedef struct ptk_timer_internal {
    ptk_resource_base_t base;              /**< Must be first field */
    dispatch_source_t timer_source;        /**< GCD timer source */
    uint64_t interval_ms;                  /**< Timer interval in milliseconds */
    bool is_repeating;                     /**< true for repeating timer, false for one-shot */
    bool is_running;                       /**< Whether timer is currently running */
    uint32_t generation_counter;           /**< Per-slot generation counter */
    ptk_event_handler_t event_handlers[4]; /**< Event handlers for this timer */
} ptk_timer_internal_t;

/**
 * @brief macOS-specific socket implementation using BSD sockets + GCD
 */
typedef struct ptk_socket_internal {
    ptk_resource_base_t base;              /**< Must be first field */
    int sockfd;                            /**< BSD socket file descriptor */
    dispatch_source_t read_source;         /**< GCD source for read events */
    dispatch_source_t write_source;        /**< GCD source for write events */
    struct sockaddr_storage local_addr;    /**< Local address */
    struct sockaddr_storage remote_addr;   /**< Remote address */
    socklen_t local_addr_len;              /**< Length of local address */
    socklen_t remote_addr_len;             /**< Length of remote address */
    bool is_connected;                     /**< Whether socket is connected */
    bool is_listening;                     /**< Whether socket is listening */
    int socket_type;                       /**< SOCK_STREAM or SOCK_DGRAM */
    uint32_t generation_counter;           /**< Per-slot generation counter */
    ptk_event_handler_t event_handlers[8]; /**< Event handlers for this socket */
} ptk_socket_internal_t;

/**
 * @brief macOS-specific user event source using dispatch_queue_t
 */
typedef struct ptk_user_event_source_internal {
    ptk_resource_base_t base;               /**< Must be first field */
    dispatch_queue_t event_queue;           /**< Queue for handling user events */
    dispatch_source_t user_source;          /**< GCD source for user events */
    uint32_t generation_counter;            /**< Per-slot generation counter */
    ptk_event_handler_t event_handlers[16]; /**< Event handlers for user events */
} ptk_user_event_source_internal_t;

/**
 * @brief Resource pools for an event loop
 */
typedef struct ptk_event_loop_resources {
    ptk_timer_internal_t *timers; /**< Array of timer resources */
    size_t num_timers;            /**< Number of timer slots */

    ptk_socket_internal_t *sockets; /**< Array of socket resources */
    size_t num_sockets;             /**< Number of socket slots */

    ptk_user_event_source_internal_t *user_events; /**< Array of user event sources */
    size_t num_user_events;                        /**< Number of user event slots */
} ptk_event_loop_resources_t;

/**
 * @brief macOS-specific event loop using dispatch_queue_t
 */
typedef struct ptk_event_loop_slot {
    ptk_handle_t handle;                   /**< Event loop handle (0 = unused) */
    ptk_event_loop_resources_t *resources; /**< Assigned resource pools */
    ptk_err_t last_error;                  /**< Event-loop-scoped error state */

    /* macOS-specific fields */
    dispatch_queue_t main_queue;  /**< Main dispatch queue for this event loop */
    dispatch_group_t event_group; /**< Group for synchronizing events */
    bool is_running;              /**< Whether event loop is currently running */
    uint32_t generation_counter;  /**< Per-slot generation counter */
} ptk_event_loop_slot_t;

/* ========================================================================
 * RESOURCE DECLARATION MACROS
 * ======================================================================== */

/**
 * @brief Declare an array of event loop slots
 */
#define PTK_DECLARE_EVENT_LOOP_SLOTS(name, max_loops) static ptk_event_loop_slot_t name[max_loops]

/**
 * @brief Declare resource pools for an event loop
 */
#define PTK_DECLARE_EVENT_LOOP_RESOURCES(name, timers, sockets, user_events) \
    static ptk_timer_internal_t name##_timers[timers];                       \
    static ptk_socket_internal_t name##_sockets[sockets];                    \
    static ptk_user_event_source_internal_t name##_user_events[user_events]; \
    static ptk_event_loop_resources_t name = {name##_timers, timers, name##_sockets, sockets, name##_user_events, user_events}

/* ========================================================================
 * EVENT LOOP MANAGEMENT
 * ======================================================================== */

/**
 * @brief Create a new event loop
 */
ptk_handle_t ptk_event_loop_create(ptk_event_loop_slot_t *slots, size_t num_slots, ptk_event_loop_resources_t *resources);

/**
 * @brief Run the event loop once
 */
ptk_err_t ptk_event_loop_run(ptk_handle_t event_loop);

/**
 * @brief Destroy an event loop
 */
ptk_err_t ptk_event_loop_destroy(ptk_handle_t event_loop);

/* ========================================================================
 * TIMER MANAGEMENT
 * ======================================================================== */

/**
 * @brief Create a new timer
 */
ptk_handle_t ptk_timer_create(ptk_handle_t event_loop);

/**
 * @brief Start a timer
 */
ptk_err_t ptk_timer_start(ptk_handle_t timer, uint64_t interval_ms, bool repeat);

/**
 * @brief Stop a timer
 */
ptk_err_t ptk_timer_stop(ptk_handle_t timer);

/**
 * @brief Destroy a timer
 */
ptk_err_t ptk_timer_destroy(ptk_handle_t timer);

/* ========================================================================
 * SOCKET MANAGEMENT
 * ======================================================================== */

/**
 * @brief Create a new TCP socket
 */
ptk_handle_t ptk_socket_create_tcp(ptk_handle_t event_loop);

/**
 * @brief Create a new UDP socket
 */
ptk_handle_t ptk_socket_create_udp(ptk_handle_t event_loop);

/**
 * @brief Connect a socket to a remote address
 */
ptk_err_t ptk_socket_connect(ptk_handle_t socket, const char *address, uint16_t port);

/**
 * @brief Bind a socket to a local address
 */
ptk_err_t ptk_socket_bind(ptk_handle_t socket, const char *address, uint16_t port);

/**
 * @brief Listen for incoming connections (TCP only)
 */
ptk_err_t ptk_socket_listen(ptk_handle_t socket, int backlog);

/**
 * @brief Accept an incoming connection (TCP only)
 */
ptk_handle_t ptk_socket_accept(ptk_handle_t listener);

/**
 * @brief Send data on a socket
 */
ptk_err_t ptk_socket_send(ptk_handle_t socket, const ptk_buffer_t *buffer);

/**
 * @brief Receive data from a socket
 */
ptk_err_t ptk_socket_receive(ptk_handle_t socket, ptk_buffer_t *buffer);

/**
 * @brief Close a socket
 */
ptk_err_t ptk_socket_close(ptk_handle_t socket);

/**
 * @brief Destroy a socket
 */
ptk_err_t ptk_socket_destroy(ptk_handle_t socket);

/* ========================================================================
 * UDP-SPECIFIC SOCKET OPERATIONS
 * ======================================================================== */

/**
 * @brief Send data to a specific address (UDP only)
 */
ptk_err_t ptk_socket_sendto(ptk_handle_t socket, const ptk_buffer_t *buffer, const char *address, uint16_t port);

/**
 * @brief Receive data and get sender's address (UDP only)
 */
ptk_err_t ptk_socket_recvfrom(ptk_handle_t socket, ptk_buffer_t *buffer, char *sender_address, size_t address_len,
                              uint16_t *sender_port);

/**
 * @brief Enable broadcast mode on a UDP socket
 */
ptk_err_t ptk_socket_enable_broadcast(ptk_handle_t socket);

/**
 * @brief Disable broadcast mode on a UDP socket
 */
ptk_err_t ptk_socket_disable_broadcast(ptk_handle_t socket);

/**
 * @brief Send broadcast data on a UDP socket
 */
ptk_err_t ptk_socket_broadcast(ptk_handle_t socket, const ptk_buffer_t *buffer, uint16_t port);

/**
 * @brief Join a multicast group
 */
ptk_err_t ptk_socket_join_multicast_group(ptk_handle_t socket, const char *multicast_address, const char *interface_address);

/**
 * @brief Leave a multicast group
 */
ptk_err_t ptk_socket_leave_multicast_group(ptk_handle_t socket, const char *multicast_address, const char *interface_address);

/**
 * @brief Set multicast TTL (Time To Live)
 */
ptk_err_t ptk_socket_set_multicast_ttl(ptk_handle_t socket, uint8_t ttl);

/**
 * @brief Enable/disable multicast loopback
 */
ptk_err_t ptk_socket_set_multicast_loopback(ptk_handle_t socket, bool enable);

/**
 * @brief Send multicast data
 */
ptk_err_t ptk_socket_multicast_send(ptk_handle_t socket, const ptk_buffer_t *buffer, const char *multicast_address,
                                    uint16_t port);

/* ========================================================================
 * USER EVENT SOURCES
 * ======================================================================== */

/**
 * @brief Create a new user event source
 */
ptk_handle_t ptk_user_event_source_create(ptk_handle_t event_loop);

/**
 * @brief Raise an event on a user event source (thread-safe)
 */
ptk_err_t ptk_raise_event(ptk_handle_t event_source, ptk_event_type_t event_type, void *event_data);

/**
 * @brief Destroy a user event source
 */
ptk_err_t ptk_user_event_source_destroy(ptk_handle_t event_source);

/* ========================================================================
 * EVENT HANDLING
 * ======================================================================== */

/**
 * @brief Set an event handler for a resource
 */
ptk_err_t ptk_set_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, ptk_event_handler_func_t handler,
                                void *user_data);

/**
 * @brief Set a protothread event handler for a resource
 * @param resource Resource handle to set handler for
 * @param event_type Type of event to handle
 * @param protothread Protothread to resume when event occurs
 * @return Status code (PTK_ERR_INVALID_ARGUMENT if a handler is already set)
 */
ptk_err_t ptk_set_protothread_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, ptk_pt_t *protothread);

/**
 * @brief Remove an event handler for a resource
 */
ptk_err_t ptk_remove_event_handler(ptk_handle_t resource, ptk_event_type_t event_type);

/* ========================================================================
 * PROTOTHREADS
 * ======================================================================== */

/**
 * @brief Protothread function type
 */
typedef void (*ptk_protothread_func_t)(ptk_pt_t *pt);

/**
 * @brief Initialize a protothread
 */
ptk_err_t ptk_protothread_init(ptk_pt_t *pt, ptk_protothread_func_t func);

/**
 * @brief Run a protothread
 */
void ptk_protothread_run(ptk_pt_t *pt);

/* ========================================================================
 * ERROR HANDLING
 * ======================================================================== */

/**
 * @brief Get the last error for an event loop
 */
ptk_err_t ptk_get_last_error(ptk_handle_t any_resource_handle);

/**
 * @brief Set the last error for an event loop (internal use)
 */
void ptk_set_last_error(ptk_handle_t any_resource_handle, ptk_err_t error);

/**
 * @brief Get a human-readable error message
 */
const char *ptk_error_string(ptk_err_t error);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * @brief Check if a handle is valid
 */
bool ptk_handle_is_valid(ptk_handle_t handle);

/**
 * @brief Get the resource type from a handle
 */
ptk_resource_type_t ptk_handle_get_type(ptk_handle_t handle);

/**
 * @brief Get the event loop that owns a resource
 */
ptk_handle_t ptk_get_owning_event_loop(ptk_handle_t resource_handle);

/**
 * @brief Set user data for a handle
 */
ptk_err_t ptk_handle_set_user_data(ptk_handle_t handle, void *user_data);

/**
 * @brief Get user data from a handle
 */
void *ptk_handle_get_user_data(ptk_handle_t handle);

/* ========================================================================
 * PROTOTHREAD CONVENIENCE MACROS
 * ======================================================================== */

/**
 * @brief Basic protothread macros
 */
#define PT_INIT(pt)                 \
    do {                            \
        (pt)->magic = PTK_PT_MAGIC; \
        (pt)->lc = 0;               \
    } while(0)

#define PT_BEGIN(pt)            \
    do {                        \
        char PT_YIELD_FLAG = 1; \
        if(PT_YIELD_FLAG) {     \
            switch((pt)->lc) {  \
                case 0:

#define PT_END(pt) \
    }              \
    }              \
    PT_INIT(pt);   \
    }              \
    while(0)

#define PT_YIELD(pt)         \
    do {                     \
        PT_YIELD_FLAG = 0;   \
        (pt)->lc = __LINE__; \
        return;              \
        case __LINE__:;      \
    } while(0)

#define PT_EXIT(pt)  \
    do {             \
        PT_INIT(pt); \
        return;      \
    } while(0)

/**
 * @brief Protothread convenience macros for common socket operations
 *
 * These macros provide simplified syntax for TCP and UDP operations
 * within protothreads, automatically handling event waiting and continuation.
 */

/**
 * @brief Wait for an event with protothread continuation
 * @param pt Protothread control block
 * @param resource Resource handle to wait on
 * @param event_type Event type to wait for
 */
#define PTK_PT_WAIT_EVENT(pt, resource, event_type)                  \
    do {                                                             \
        (pt)->lc = __LINE__;                                         \
        ptk_set_protothread_event_handler(resource, event_type, pt); \
        return;                                                      \
        case __LINE__:;                                              \
    } while(0)

/**
 * @brief TCP Connect with protothread support
 * @param pt Protothread control block
 * @param sock TCP socket handle
 * @param address Remote address
 * @param port Remote port
 */
#define PTK_PT_TCP_CONNECT(pt, sock, address, port)              \
    do {                                                         \
        ptk_socket_connect(sock, address, port);                 \
        PTK_PT_WAIT_EVENT(pt, sock, PTK_EVENT_SOCKET_CONNECTED); \
    } while(0)

/**
 * @brief TCP Send with protothread support
 * @param pt Protothread control block
 * @param sock TCP socket handle
 * @param buffer Buffer containing data to send
 */
#define PTK_PT_TCP_SEND(pt, sock, buffer)                       \
    do {                                                        \
        PTK_PT_WAIT_EVENT(pt, sock, PTK_EVENT_SOCKET_WRITABLE); \
        ptk_socket_send(sock, buffer);                          \
    } while(0)

/**
 * @brief TCP Receive with protothread support
 * @param pt Protothread control block
 * @param sock TCP socket handle
 * @param buffer Buffer to store received data
 */
#define PTK_PT_TCP_RECEIVE(pt, sock, buffer)                    \
    do {                                                        \
        PTK_PT_WAIT_EVENT(pt, sock, PTK_EVENT_SOCKET_READABLE); \
        ptk_socket_receive(sock, buffer);                       \
    } while(0)

/**
 * @brief UDP Send with protothread support
 * @param pt Protothread control block
 * @param sock UDP socket handle
 * @param buffer Buffer containing data to send
 * @param address Remote address
 * @param port Remote port
 */
#define PTK_PT_UDP_SEND(pt, sock, buffer, address, port)        \
    do {                                                        \
        PTK_PT_WAIT_EVENT(pt, sock, PTK_EVENT_SOCKET_WRITABLE); \
        ptk_socket_sendto(sock, buffer, address, port);         \
    } while(0)

/**
 * @brief UDP Receive with protothread support
 * @param pt Protothread control block
 * @param sock UDP socket handle
 * @param buffer Buffer to store received data
 * @param sender_address Buffer to store sender address
 * @param address_len Length of sender address buffer
 * @param sender_port Pointer to store sender port
 */
#define PTK_PT_UDP_RECEIVE(pt, sock, buffer, sender_address, address_len, sender_port) \
    do {                                                                               \
        PTK_PT_WAIT_EVENT(pt, sock, PTK_EVENT_SOCKET_READABLE);                        \
        ptk_socket_recvfrom(sock, buffer, sender_address, address_len, sender_port);   \
    } while(0)

/**
 * @brief UDP Broadcast with protothread support
 * @param pt Protothread control block
 * @param sock UDP socket handle
 * @param buffer Buffer containing data to send
 * @param port Broadcast port
 */
#define PTK_PT_UDP_BROADCAST(pt, sock, buffer, port)            \
    do {                                                        \
        PTK_PT_WAIT_EVENT(pt, sock, PTK_EVENT_SOCKET_WRITABLE); \
        ptk_socket_broadcast(sock, buffer, port);               \
    } while(0)

/**
 * @brief UDP Multicast send with protothread support
 * @param pt Protothread control block
 * @param sock UDP socket handle
 * @param buffer Buffer containing data to send
 * @param multicast_address Multicast group address
 * @param port Multicast port
 */
#define PTK_PT_UDP_MULTICAST_SEND(pt, sock, buffer, multicast_address, port) \
    do {                                                                     \
        PTK_PT_WAIT_EVENT(pt, sock, PTK_EVENT_SOCKET_WRITABLE);              \
        ptk_socket_multicast_send(sock, buffer, multicast_address, port);    \
    } while(0)

/**
 * @brief Timer sleep with protothread support
 * @param pt Protothread control block
 * @param timer Timer handle
 * @param delay_ms Delay in milliseconds
 */
#define PTK_PT_SLEEP_MS(pt, timer, delay_ms)                   \
    do {                                                       \
        ptk_timer_start(timer, delay_ms, false);               \
        PTK_PT_WAIT_EVENT(pt, timer, PTK_EVENT_TIMER_EXPIRED); \
        ptk_timer_stop(timer);                                 \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_TOOLKIT_H */
