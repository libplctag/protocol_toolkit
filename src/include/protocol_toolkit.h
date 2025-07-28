#ifndef PROTOCOL_TOOLKIT_H
#define PROTOCOL_TOOLKIT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h> // For size_t

/**
 * @brief The API for the Protocol Toolkit (PTK)
 *
 * General Concepts:
 * - The PTK provides a protothread-style event handling system.
 * - Allocation is done either by having the application provide the memory (as for protothreads),
 *   or via opaque handles.  On embedded systems these handles point to fixed arrays of structures.
 * - On embedded systems, there is no dynamic memory allocation.
 */


/**
 * Event definitions
 */
typedef enum {
    PTK_EVENT_READABLE = 0, /**< Event generated when a socket is readable */
    PTK_EVENT_WRITABLE,     /**< Event generated when a socket is writable */
    PTK_EVENT_TIMER,        /**< Event generated when a timer expires */
    PTK_EVENT_SHUTDOWN,     /**< Event generated when the event loop is shutting down */

    PTK_EVENT_MAX = 10 /**< App events should be above this. */
} ptk_event_type_t;

/**
 * Error codes
 */
typedef enum {
    PTK_ERR_NONE = 0,              /**< No error */
    PTK_ERR_INVALID_HANDLE = -1,    /**< Invalid handle */
    PTK_ERR_WRONG_HANDLE_TYPE = -2,  /**< Handle type mismatch */
    PTK_ERR_NO_RESOURCES = -3,      /**< No resources available */
    PTK_ERR_INVALID_ARGUMENT = -4,  /**< Invalid argument passed to a function */
    PTK_ERR_TIMER_FAILURE = -5,     /**< Timer-related error */
    PTK_ERR_SOCKET_FAILURE = -6,   /**< Socket-related error */
    PTK_ERR_EVENT_FAILURE = -7,    /**< Event-related error */
    PTK_ERR_NOT_IMPLEMENTED = -8, /**< Functionality not implemented */

    PTK_ERR_UNKNOWN = -9           /**< Unknown error */
} ptk_err_t;


/**
 * @brief Handle type
 *
 * This is used to uniquely identify resources such as sockets, timers, and event sources.
 * Handles are typically 64-bit integers, allowing for a large number of unique handles.
 *
 * Negative numbers are reserved for error codes.
 *
 * The applications should treat handles as opaque identifiers.
 * They should not assume any specific structure or meaning behind the handle value.
 * Instead, they should use the provided API functions to interact with the resources associated with the handle
 */
typedef int64_t ptk_handle_t;


/**
 * @brief The buffer type
 *
 * The application is responsible for providing buffers for data transfer.
 */
typedef struct ptk_buffer {
    uint8_t *data;     /**< Pointer to the buffer data */
    size_t size;       /**< Size of the buffer */
    size_t capacity;   /**< Capacity of the buffer */
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
static inline ptk_buffer_t ptk_buffer_create(void *data, size_t capacity) {
    ptk_buffer_t buffer;
    buffer.data = data;
    buffer.size = 0;
    buffer.capacity = capacity;
    return buffer;
}


/**
 * @brief Protothread control block
 * This should be wrapped in a struct to allow for application-specific data
 * to be associated with the protothread.
 */
typedef struct ptk_pt {
    int step;                                /**< Step for saving the current state of the protothread */
    void (*function)(struct ptk_pt *self);   /**< Function pointer for the ptk_pt */
} ptk_pt_t;

/**
 * @brief Protothread-style event handling macros
 *
 * The macros below are for protothread-style event handling.
 * They allow for cooperative multitasking by saving the current
 * step in the protothread and resuming from that point on the next event.
 */
#define PTK_PT_BEGIN(pt) switch ((pt)->step) { case 0:
#define PTK_PT_END(pt) }

/**
 * @brief Wait for an event
 * @param pt Protothread control block
 * @param src Event source
 * @param evt Event type
 */
#define ptk_wait_for_event(pt, src, evt) \
    do { \
        (src)->events[evt] = (pt); \
        (pt)->step = __LINE__; \
        ptk_set_event_handler(src, evt, pt); \
        case __LINE__:; \
    } while (0)

/**
 * @brief Receive data from a socket
 * @param pt Protothread control block
 * @param sock Socket handle
 * @param buffer Buffer to store received data
 * @param len Length of the buffer
 */
#define ptk_receive_data(pt, sock, buffer, len) \
    do { \
        ptk_wait_for_event(pt, sock, EVENT_READABLE); \
        ptk_read_socket(sock, buffer, len); \
    } while (0)

/**
 * @brief Send data to a socket
 * @param pt Protothread control block
 * @param sock Socket handle
 * @param buffer Buffer containing data to send
 * @param len Length of the data to send
 */
#define ptk_send_data(pt, sock, buffer, len) \
    do { \
        ptk_wait_for_event(pt, sock, EVENT_WRITABLE); \
        ptk_write_socket(sock, buffer, len); \
    } while (0)

/**
 * @brief Sleep for a specified duration
 * @param pt Protothread control block
 * @param timer Timer handle
 * @param delay_ms Delay in milliseconds
 */
#define pkt_sleep_ms(pt, timer, delay_ms) \
    do { \
        (timer)->expiry_time = ptk_time_ms() + (delay_ms); \
        ptk_wait_for_event(pt, timer, TIMER_EVENT); \
    } while (0)

/**
 * @brief Destroy a handle
 * @param handle Handle to destroy
 * @return Status code
 */
int ptk_handle_destroy(ptk_handle_t handle);

/**
 * @brief Initialize a protothread
 * @param pt Protothread control block
 * @param pt_function Function to execute in the protothread
 * @return Status code
 */
int ptk_protothread_init(ptk_pt_t *pt, void (*pt_function)(struct ptk_pt *self));

/**
 * @brief Create an event loop
 * @return Handle to the event loop
 */
ptk_handle_t ptk_event_loop_create(void);

/**
 * @brief Run the event loop
 *
 * This runs the event loop once, processing events and executing associated protothreads.
 * It will block until there are no more events to process.  The application should call this function
 * in a loop to keep the event loop running.
 *
 * @param loop Handle to the event loop
 * @return Status code
 */
ptk_err_t ptk_event_loop_run(ptk_handle_t loop);

/**
 * Set an event handler
 * @param src_handle Handle to the event source
 * @param event_type Type of event
 * @param handler Protothread to handle the event
 * @return Status code
 */
ptk_err_t ptk_set_event_handler(ptk_handle_t src_handle, int event_type, ptk_pt_t *handler);

/**
 * Remove an event handler
 * @param src_handle Handle to the event source
 * @param event_type Type of event
 * @return Status code
 */
ptk_err_t ptk_remove_event_handler(ptk_handle_t src_handle, int event_type);

/**
 * Raise an event
 * @param src_handle Handle to the event source
 * @param event_type Type of event
 * @return Status code
 */
ptk_err_t ptk_raise_event(ptk_handle_t src_handle, int event_type);

/**
 * Create a timer
 * @return Handle to the timer, or error code if creation failed
 */
ptk_handle_t ptk_timer_create(void);

/**
 * Start a timer
 * @param timer_handle Handle to the timer
 * @param interval_ms Timer interval in milliseconds
 * @param is_repeating Whether the timer is repeating
 * @return Status code
 */
ptk_err_t ptk_timer_start(ptk_handle_t timer_handle, uint64_t interval_ms, bool is_repeating);

/**
 * @brief Stop a timer
 * @param timer_handle Handle to the timer
 * @return Status code
 */
ptk_err_t ptk_timer_stop(ptk_handle_t timer_handle);

/**
 * Check if a timer is running
 * @param timer_handle Handle to the timer
 * @return True if the timer is running, false otherwise
 */
bool ptk_timer_is_running(ptk_handle_t timer_handle);

/**
 * @brief Create a socket
 * @return Handle to the socket
 */
ptk_handle_t ptk_socket_create(void);

/**
 * @brief Close a socket
 * @param sock_handle Handle to the socket
 * @return Status code
 */
ptk_err_t ptk_socket_close(ptk_handle_t sock_handle);

/**
 * @brief Connect a TCP socket to a remote address
 *
 * A PTK_EVENT_CONNECTED event will be raised when the connection is established.
 *
 * @param sock_handle Handle to the socket
 * @param address Remote address
 * @param port Remote port
 * @return Status code
 */
ptk_err_t ptk_tcp_socket_connect(ptk_handle_t sock_handle, const char* address, int port);

/**
 * @brief Read data from a TCP socket
 *
 * A PTK_EVENT_READABLE event must be raised first or the read will return an error.
 *
 * @param sock_handle Handle to the socket
 * @param buffer Buffer to store received data
 * @param len Length of the buffer
 * @return Status code
 */
ptk_err_t ptk_tcp_socket_read(ptk_handle_t sock_handle, ptk_buffer_t* buffer);

/**
 * @brief Write data to a TCP socket
 *
 * The app should wait for a PTK_EVENT_WRITABLE event before calling this function.
 *
 * @param sock_handle Handle to the socket
 * @param buffer Buffer containing data to send
 * @param len Length of the data to send
 * @return Status code
 */
ptk_err_t ptk_tcp_socket_write(ptk_handle_t sock_handle, const ptk_buffer_t* buffer);

/**
 * @brief Listen for incoming connections on a TCP socket
 * @param server_sock_handle Handle to the server socket
 * @param address Local address to bind to
 * @param port Local port to bind to
 * @return Status code
 */
ptk_err_t ptk_tcp_socket_listen(ptk_handle_t server_sock_handle, const char* address, int port);

/**
 * @brief Accept an incoming connection on a TCP socket
 *
 * The app should wait for a PTK_EVENT_READABLE event before calling this function.
 *
 * @param server_sock_handle Handle to the server socket
 * @param client_sock_handle Handle to the client socket
 * @return Status code
 */
ptk_err_t ptk_tcp_socket_accept(ptk_handle_t server_sock_handle, ptk_handle_t client_sock_handle);

/**
 * @brief Bind a UDP socket to a local address
 * @param sock_handle Handle to the socket
 * @param address Local address to bind to
 * @param port Local port to bind to
 * @return Status code
 */
ptk_err_t ptk_udp_socket_bind(ptk_handle_t sock_handle, const char* address, int port);

/**
 * @brief Send data to a remote address using a UDP socket
 *
 * The app should wait for a PTK_EVENT_WRITABLE event before calling this function.
 *
 * @param sock_handle Handle to the socket
 * @param buffer Buffer containing data to send
 * @param len Length of the data to send
 * @param address Remote address
 * @param port Remote port
 * @return Status code
 */
ptk_err_t ptk_udp_socket_sendto(ptk_handle_t sock_handle, ptk_buffer_t *buffer, const char* address, int port);

/**
 * @brief Receive data from a remote address using a UDP socket
 *
 * The app should wait for a PTK_EVENT_READABLE event before calling this function.
 *
 * @param sock_handle Handle to the socket
 * @param buffer Buffer to store received data
 * @param len Length of the buffer
 * @param address Remote address
 * @param port Remote port
 * @return Status code
 */
ptk_err_t ptk_udp_socket_recvfrom(ptk_handle_t sock_handle, ptk_buffer_t *buffer, char* address, int* port);

/**
 * @brief Join a multicast group using a UDP socket
 * @param sock_handle Handle to the socket
 * @param group_address Multicast group address
 * @param interface_address Interface address to use
 * @return Status code
 */
ptk_err_t ptk_udp_socket_join_multicast_group(ptk_handle_t sock_handle, const char* group_address, const char* interface_address);

/**
 * @brief Leave a multicast group using a UDP socket
 * @param sock_handle Handle to the socket
 * @param group_address Multicast group address
 * @param interface_address Interface address to use
 * @return Status code
 */
ptk_err_t ptk_udp_socket_leave_multicast_group(ptk_handle_t sock_handle, const char* group_address, const char* interface_address);

/**
 * @brief Send data to a multicast group using a UDP socket
 *
 * The app should wait for a PTK_EVENT_WRITABLE event before calling this function.
 *
 * @param sock_handle Handle to the socket
 * @param buffer Buffer containing data to send
 * @param len Length of the data to send
 * @param group_address Multicast group address
 * @return Status code
 */
ptk_err_t ptk_udp_socket_send_multicast(ptk_handle_t sock_handle, ptk_buffer_t *buffer, const char* group_address);

/**
 * @brief Broadcast data using a UDP socket
 *
 * The app should wait for a PTK_EVENT_WRITABLE event before calling this function.
 *
 * @param sock_handle Handle to the socket
 * @param buffer Buffer containing data to send
 * @param address Broadcast address
 * @param port Broadcast port
 * @return Status code
 */
ptk_err_t ptk_udp_socket_broadcast(ptk_handle_t sock_handle, ptk_buffer_t *buffer, const char* address, int port);

/**
 * @brief Get the last error code
 * @return Last error code
 */
int ptk_get_last_err(void);

/**
 * @brief Set the last error code
 * @param err Error code to set
 */
void ptk_set_last_err(int err);

/**
 * Create a user-defined event source
 * @return Handle to the event source
 */
ptk_handle_t ptk_user_event_source_create(void);

/**
 * @brief Signal a user-defined event source
 * @param handle Handle to the event source
 * @return Status code
 */
ptk_err_t ptk_user_event_source_signal(ptk_handle_t handle);

#endif // PROTOCOL_TOOLKIT_H
