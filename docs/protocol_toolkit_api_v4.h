/**
 * @file protocol_toolkit_api_v4.h
 * @brief Generic Protocol Toolkit API v4 - Public Interface
 *
 * This is a generic API specification showing the public interface.
 * Each platform provides its own complete protocol_toolkit.h with
 * platform-specific implementations of these structures and functions.
 *
 * Key Design Principles:
 * - Zero global state - all resources are application-managed
 * - Zero runtime allocation - all memory pre-allocated at compile time
 * - Event-loop-centric resource management
 * - Handle-based safety with generation counters
 * - Cross-platform portability (Linux, macOS, Windows, FreeRTOS, Zephyr, NuttX)
 */

#ifndef PROTOCOL_TOOLKIT_H
#define PROTOCOL_TOOLKIT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CORE TYPES AND CONSTANTS
 * ======================================================================== */

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
    PTK_ERR_OK = 0,                    /**< Success */
    PTK_ERR_INVALID_HANDLE = -1,       /**< Handle is invalid or stale */
    PTK_ERR_INVALID_ARGUMENT = -2,     /**< Invalid function argument */
    PTK_ERR_OUT_OF_MEMORY = -3,        /**< No available resource slots */
    PTK_ERR_NOT_SUPPORTED = -4,        /**< Operation not supported on this platform */
    PTK_ERR_NETWORK_ERROR = -5,        /**< Network operation failed */
    PTK_ERR_TIMEOUT = -6,              /**< Operation timed out */
    PTK_ERR_WOULD_BLOCK = -7,          /**< Operation would block (try again) */
    PTK_ERR_CONNECTION_REFUSED = -8,   /**< Connection refused by remote host */
    PTK_ERR_CONNECTION_RESET = -9,     /**< Connection reset by peer */
    PTK_ERR_NOT_CONNECTED = -10,       /**< Socket not connected */
    PTK_ERR_ALREADY_CONNECTED = -11,   /**< Socket already connected */
    PTK_ERR_ADDRESS_IN_USE = -12,      /**< Address already in use */
    PTK_ERR_NO_ROUTE = -13,            /**< No route to host */
    PTK_ERR_MESSAGE_TOO_LARGE = -14,   /**< Message too large for transport */
    PTK_ERR_PROTOCOL_ERROR = -15       /**< Protocol-specific error */
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
#define PTK_HANDLE_TYPE(h)           ((uint8_t)((h) & 0xFF))

/**
 * @brief Extract event loop ID from handle
 */
#define PTK_HANDLE_EVENT_LOOP_ID(h)  ((uint8_t)(((h) >> 8) & 0xFF))

/**
 * @brief Extract generation counter from handle
 */
#define PTK_HANDLE_GENERATION(h)     ((uint16_t)(((h) >> 16) & 0xFFFF))

/**
 * @brief Extract handle ID from handle
 */
#define PTK_HANDLE_ID(h)             ((uint32_t)((h) >> 32))

/**
 * @brief Create a handle from components (internal use)
 */
#define PTK_MAKE_HANDLE(type, loop_id, gen, id) \
    ((ptk_handle_t)(type) | ((ptk_handle_t)(loop_id) << 8) | \
     ((ptk_handle_t)(gen) << 16) | ((ptk_handle_t)(id) << 32))

/* ========================================================================
 * RESOURCE STRUCTURES (PLATFORM-SPECIFIC IMPLEMENTATIONS)
 * ======================================================================== */

/**
 * @brief Base structure for all protocol toolkit resources
 *
 * All platform-specific resource structures must begin with this base.
 */
typedef struct ptk_resource_base {
    ptk_handle_t handle;           /**< Complete handle (0 = unused slot) */
    ptk_handle_t event_loop;       /**< Handle of owning event loop */
} ptk_resource_base_t;

/**
 * @brief Platform-specific timer implementation
 *
 * Each platform defines this structure with platform-specific fields.
 */
typedef struct ptk_timer_internal {
    ptk_resource_base_t base;      /**< Must be first field */
    /* PLATFORM-SPECIFIC FIELDS:
     * - Linux: int timerfd, struct timespec interval
     * - Windows: HANDLE timer_handle, LARGE_INTEGER due_time
     * - macOS: dispatch_source_t timer_source
     * - FreeRTOS: TimerHandle_t timer_handle
     * - Zephyr: struct k_timer timer
     * - NuttX: struct wdog_s *wdog
     */
    uint32_t generation_counter;   /**< Per-slot generation counter */
    /* Additional platform-specific timer fields follow... */
} ptk_timer_internal_t;

/**
 * @brief Platform-specific socket implementation
 */
typedef struct ptk_socket_internal {
    ptk_resource_base_t base;      /**< Must be first field */
    /* PLATFORM-SPECIFIC FIELDS:
     * - POSIX: int sockfd, struct sockaddr_storage addr
     * - Windows: SOCKET sockfd, WSAOVERLAPPED overlapped
     * - Embedded: platform-specific network handle
     */
    uint32_t generation_counter;   /**< Per-slot generation counter */
    /* Additional platform-specific socket fields follow... */
} ptk_socket_internal_t;

/**
 * @brief Platform-specific user event source implementation
 */
typedef struct ptk_user_event_source_internal {
    ptk_resource_base_t base;      /**< Must be first field */
    /* PLATFORM-SPECIFIC FIELDS:
     * - Linux: int eventfd or pipe fds
     * - Windows: HANDLE event_handle
     * - macOS: dispatch_source_t user_source
     * - Embedded: platform-specific signaling mechanism
     */
    uint32_t generation_counter;   /**< Per-slot generation counter */
    /* Additional platform-specific event source fields follow... */
} ptk_user_event_source_internal_t;

/**
 * @brief Protothread state structure
 */
typedef struct ptk_pt {
    /* PLATFORM-SPECIFIC FIELDS:
     * - State variables for protothread implementation
     * - Line number for resumption point
     * - Platform-specific context if needed
     */
    unsigned short lc;             /**< Line continuation state */
    /* Additional platform-specific protothread fields follow... */
} ptk_pt_t;

/**
 * @brief Resource pools for an event loop
 */
typedef struct ptk_event_loop_resources {
    ptk_timer_internal_t *timers;                    /**< Array of timer resources */
    size_t num_timers;                               /**< Number of timer slots */

    ptk_socket_internal_t *sockets;                  /**< Array of socket resources */
    size_t num_sockets;                              /**< Number of socket slots */

    ptk_user_event_source_internal_t *user_events;  /**< Array of user event sources */
    size_t num_user_events;                          /**< Number of user event slots */
} ptk_event_loop_resources_t;

/**
 * @brief Event loop instance
 */
typedef struct ptk_event_loop_slot {
    ptk_handle_t handle;                      /**< Event loop handle (0 = unused) */
    ptk_event_loop_resources_t *resources;    /**< Assigned resource pools */
    ptk_err_t last_error;                     /**< Event-loop-scoped error state */

    /* PLATFORM-SPECIFIC FIELDS:
     * - Linux: int epollfd, struct epoll_event *events
     * - Windows: HANDLE iocp, OVERLAPPED_ENTRY *entries
     * - macOS: dispatch_queue_t queue
     * - FreeRTOS: event group or task notification mechanism
     * - Zephyr: struct k_poll_event *events
     * - NuttX: platform-specific event structures
     */
    /* Additional platform-specific event loop fields follow... */
} ptk_event_loop_slot_t;

/* ========================================================================
 * RESOURCE DECLARATION MACROS
 * ======================================================================== */

/**
 * @brief Declare an array of event loop slots
 *
 * @param name Variable name for the event loop slots array
 * @param max_loops Maximum number of event loops
 */
#define PTK_DECLARE_EVENT_LOOP_SLOTS(name, max_loops) \
    static ptk_event_loop_slot_t name[max_loops]

/**
 * @brief Declare resource pools for an event loop
 *
 * @param name Variable name for the resource pool structure
 * @param timers Number of timer slots
 * @param sockets Number of socket slots
 * @param user_events Number of user event source slots
 */
#define PTK_DECLARE_EVENT_LOOP_RESOURCES(name, timers, sockets, user_events) \
    static ptk_timer_internal_t name##_timers[timers]; \
    static ptk_socket_internal_t name##_sockets[sockets]; \
    static ptk_user_event_source_internal_t name##_user_events[user_events]; \
    static ptk_event_loop_resources_t name = { \
        .timers = name##_timers, .num_timers = timers, \
        .sockets = name##_sockets, .num_sockets = sockets, \
        .user_events = name##_user_events, .num_user_events = user_events \
    }

/* ========================================================================
 * EVENT LOOP MANAGEMENT
 * ======================================================================== */

/**
 * @brief Create a new event loop
 *
 * @param slots Array of event loop slots provided by application
 * @param num_slots Number of slots in the array
 * @param resources Pre-allocated resource pools for this event loop
 * @return Event loop handle or negative error code
 */
ptk_handle_t ptk_event_loop_create(ptk_event_loop_slot_t *slots,
                                   size_t num_slots,
                                   ptk_event_loop_resources_t *resources);

/**
 * @brief Run the event loop once
 *
 * Processes all pending events and returns. Applications should call this
 * in a loop to keep the event loop running.
 *
 * @param event_loop Handle to the event loop
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_event_loop_run(ptk_handle_t event_loop);

/**
 * @brief Destroy an event loop
 *
 * Cleans up all resources associated with the event loop.
 *
 * @param event_loop Handle to the event loop
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_event_loop_destroy(ptk_handle_t event_loop);

/* ========================================================================
 * TIMER MANAGEMENT
 * ======================================================================== */

/**
 * @brief Create a new timer
 *
 * @param event_loop Handle to the owning event loop
 * @return Timer handle or negative error code
 */
ptk_handle_t ptk_timer_create(ptk_handle_t event_loop);

/**
 * @brief Start a timer
 *
 * @param timer Handle to the timer
 * @param interval_ms Timer interval in milliseconds
 * @param repeat true for repeating timer, false for one-shot
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_timer_start(ptk_handle_t timer, uint64_t interval_ms, bool repeat);

/**
 * @brief Stop a timer
 *
 * @param timer Handle to the timer
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_timer_stop(ptk_handle_t timer);

/**
 * @brief Destroy a timer
 *
 * @param timer Handle to the timer
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_timer_destroy(ptk_handle_t timer);

/* ========================================================================
 * SOCKET MANAGEMENT
 * ======================================================================== */

/**
 * @brief Create a new TCP socket
 *
 * @param event_loop Handle to the owning event loop
 * @return Socket handle or negative error code
 */
ptk_handle_t ptk_socket_create_tcp(ptk_handle_t event_loop);

/**
 * @brief Create a new UDP socket
 *
 * @param event_loop Handle to the owning event loop
 * @return Socket handle or negative error code
 */
ptk_handle_t ptk_socket_create_udp(ptk_handle_t event_loop);

/**
 * @brief Connect a socket to a remote address
 *
 * @param socket Handle to the socket
 * @param address Remote address string (IPv4/IPv6)
 * @param port Remote port number
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_socket_connect(ptk_handle_t socket, const char *address, uint16_t port);

/**
 * @brief Bind a socket to a local address
 *
 * @param socket Handle to the socket
 * @param address Local address string (IPv4/IPv6, NULL for any)
 * @param port Local port number (0 for any)
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_socket_bind(ptk_handle_t socket, const char *address, uint16_t port);

/**
 * @brief Listen for incoming connections (TCP only)
 *
 * @param socket Handle to the socket
 * @param backlog Maximum number of pending connections
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_socket_listen(ptk_handle_t socket, int backlog);

/**
 * @brief Accept an incoming connection (TCP only)
 *
 * @param listener Handle to the listening socket
 * @return New socket handle for the connection or negative error code
 */
ptk_handle_t ptk_socket_accept(ptk_handle_t listener);

/**
 * @brief Send data on a socket
 *
 * @param socket Handle to the socket
 * @param data Buffer containing data to send
 * @param size Number of bytes to send
 * @param bytes_sent Pointer to store actual bytes sent (may be NULL)
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_socket_send(ptk_handle_t socket, const void *data, size_t size, size_t *bytes_sent);

/**
 * @brief Receive data from a socket
 *
 * @param socket Handle to the socket
 * @param buffer Buffer to store received data
 * @param size Size of the buffer
 * @param bytes_received Pointer to store actual bytes received (may be NULL)
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_socket_receive(ptk_handle_t socket, void *buffer, size_t size, size_t *bytes_received);

/**
 * @brief Close a socket
 *
 * @param socket Handle to the socket
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_socket_close(ptk_handle_t socket);

/**
 * @brief Destroy a socket
 *
 * @param socket Handle to the socket
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_socket_destroy(ptk_handle_t socket);

/* ========================================================================
 * USER EVENT SOURCES
 * ======================================================================== */

/**
 * @brief Create a new user event source
 *
 * @param event_loop Handle to the owning event loop
 * @return User event source handle or negative error code
 */
ptk_handle_t ptk_user_event_source_create(ptk_handle_t event_loop);

/**
 * @brief Raise an event on a user event source
 *
 * This function is thread-safe and can be called from any thread.
 *
 * @param event_source Handle to the event source
 * @param event_type Type of event to raise
 * @param event_data Optional event data (platform-specific)
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_raise_event(ptk_handle_t event_source, ptk_event_type_t event_type, void *event_data);

/**
 * @brief Destroy a user event source
 *
 * @param event_source Handle to the event source
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_user_event_source_destroy(ptk_handle_t event_source);

/* ========================================================================
 * EVENT HANDLING
 * ======================================================================== */

/**
 * @brief Event handler function type
 *
 * @param resource Handle to the resource that generated the event
 * @param event_type Type of event that occurred
 * @param event_data Platform-specific event data (may be NULL)
 * @param user_data User-provided context data
 */
typedef void (*ptk_event_handler_t)(ptk_handle_t resource,
                                     ptk_event_type_t event_type,
                                     void *event_data,
                                     void *user_data);

/**
 * @brief Set an event handler for a resource
 *
 * @param resource Handle to the resource
 * @param event_type Type of event to handle
 * @param handler Event handler function
 * @param user_data User-provided context data
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_set_event_handler(ptk_handle_t resource,
                                 ptk_event_type_t event_type,
                                 ptk_event_handler_t handler,
                                 void *user_data);

/**
 * @brief Remove an event handler for a resource
 *
 * @param resource Handle to the resource
 * @param event_type Type of event to stop handling
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_remove_event_handler(ptk_handle_t resource, ptk_event_type_t event_type);

/* ========================================================================
 * PROTOTHREADS
 * ======================================================================== */

/**
 * @brief Protothread function type
 *
 * @param pt Pointer to the protothread state
 * @return Non-zero if the protothread is still running, zero if finished
 */
typedef int (*ptk_protothread_func_t)(ptk_pt_t *pt);

/**
 * @brief Initialize a protothread
 *
 * @param pt Pointer to protothread state (allocated by application)
 * @param func Protothread function to execute
 * @return PTK_ERR_OK on success, error code on failure
 */
ptk_err_t ptk_protothread_init(ptk_pt_t *pt, ptk_protothread_func_t func);

/**
 * @brief Run a protothread
 *
 * @param pt Pointer to protothread state
 * @return Non-zero if still running, zero if finished
 */
int ptk_protothread_run(ptk_pt_t *pt);

/* ========================================================================
 * ERROR HANDLING
 * ======================================================================== */

/**
 * @brief Get the last error for an event loop
 *
 * Uses any resource handle to identify the owning event loop.
 *
 * @param any_resource_handle Handle to any resource in the event loop
 * @return Last error code for the event loop
 */
ptk_err_t ptk_get_last_error(ptk_handle_t any_resource_handle);

/**
 * @brief Set the last error for an event loop (internal use)
 *
 * @param any_resource_handle Handle to any resource in the event loop
 * @param error Error code to set
 */
void ptk_set_last_error(ptk_handle_t any_resource_handle, ptk_err_t error);

/**
 * @brief Get a human-readable error message
 *
 * @param error Error code
 * @return Static string describing the error
 */
const char *ptk_error_string(ptk_err_t error);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * @brief Check if a handle is valid
 *
 * @param handle Handle to validate
 * @return true if valid, false if invalid or stale
 */
bool ptk_handle_is_valid(ptk_handle_t handle);

/**
 * @brief Get the resource type from a handle
 *
 * @param handle Handle to examine
 * @return Resource type or PTK_TYPE_INVALID if handle is invalid
 */
ptk_resource_type_t ptk_handle_get_type(ptk_handle_t handle);

/**
 * @brief Get the event loop that owns a resource
 *
 * @param resource_handle Handle to any resource
 * @return Event loop handle or negative error code
 */
ptk_handle_t ptk_get_owning_event_loop(ptk_handle_t resource_handle);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_TOOLKIT_H */
