#ifndef PROTOCOL_TOOLKIT_H
#define PROTOCOL_TOOLKIT_H

#include <stdint.h>
#include <stddef.h> // For size_t


// Event definitions
#define PTK_EVENT_READABLE 1 // Event generated when a socket is readable
#define PTK_EVENT_WRITABLE 2 // Event generated when a socket is writable
#define PTK_EVENT_TIMER 3    // Event generated when a timer expires
#define PTK_EVENT_SHUTDOWN 4 // Event generated when the event loop is shutting down

#define MAX_EVENTS 10 // Maximum number of events per event source


// Error codes
typedef enum {
    PTK_ERR_NONE = 0,          // No error
    PTK_ERR_INVALID_HANDLE,   // Invalid handle
    PTK_ERR_WRONG_HANDLE_TYPE, // Handle type mismatch
    PTK_ERR_NO_RESOURCES,     // No resources available
    PTK_ERR_INVALID_ARGUMENT, // Invalid argument passed to a function
    PTK_ERR_TIMER_FAILURE,    // Timer-related error
    PTK_ERR_SOCKET_FAILURE,   // Socket-related error
    PTK_ERR_EVENT_FAILURE,    // Event-related error
    PTK_ERR_UNKNOWN           // Unknown error
} ptk_error_t;



// Protothread structure
typedef struct ptk_pt {
    void (*function)(struct ptk_pt *self);   // Function pointer for the ptk_pt
    int step;                                // Step for saving the current state of the protothread
} ptk_pt_t;

typedef uint64_t ptk_handle_t;


// Macros
#define PTK_PT_BEGIN(pt) switch ((pt)->step) { case 0:
#define PTK_PT_END(pt) }

#define ptk_wait_for_event(pt, src, evt) \
    do { \
        (src)->events[evt] = (pt); \
        (pt)->step = __LINE__; \
        ptk_set_event_handler(src, evt, pt); \
        case __LINE__:; \
    } while (0)

#define ptk_receive_data(pt, sock, buffer, len) \
    do { \
        ptk_wait_for_event(pt, sock, EVENT_READABLE); \
        ptk_read_socket(sock, buffer, len); \
    } while (0)

#define send_data(pt, sock, buffer, len) \
    do { \
        ptk_wait_for_event(pt, sock, EVENT_WRITABLE); \
        ptk_write_socket(sock, buffer, len); \
    } while (0)

#define pkt_sleep_ms(pt, timer, delay_ms) \
    do { \
        (timer)->expiry_time = ptk_time_ms() + (delay_ms); \
        ptk_wait_for_event(pt, timer, TIMER_EVENT); \
    } while (0)


// general handle management
int ptk_handle_destroy(ptk_handle_t handle);

// protothread management
int ptk_protothread_init(ptk_pt_t *pt, void (*pt_function)(struct ptk_pt *self));

// Event loop management
ptk_handle_t ptk_event_loop_create(void);
int ptk_event_loop_run(ptk_handle_t loop);

// Event management
void ptk_set_event_handler(ptk_handle_t src_handle, int event_type, ptk_pt_t *handler);
void ptk_remove_event_handler(ptk_handle_t src_handle, int event_type);
void ptk_raise_event(ptk_handle_t src_handle, int event_type);

// Timers
ptk_handle_t ptk_timer_create(void);
void ptk_sleep_ms(ptk_pt_t *pt, ptk_handle_t timer_handle, uint64_t delay_ms);

// Sockets
ptk_handle_t ptk_socket_create(void);
void ptk_socket_close(ptk_handle_t sock_handle);

// TCP client Sockets
void ptk_tcp_socket_connect(ptk_handle_t sock_handle, const char* address, int port);
void ptk_tcp_socket_read(ptk_handle_t sock_handle, char *buffer, size_t len);
void ptk_tcp_socket_write(ptk_handle_t sock_handle, const char* buffer, size_t len);

// TCP server Sockets
void ptk_tcp_socket_listen(ptk_handle_t server_sock_handle, const char* address, int port);
void ptk_tcp_socket_accept(ptk_handle_t server_sock_handle, ptk_handle_t client_sock_handle);

// UDP Sockets
void ptk_udp_socket_bind(ptk_handle_t sock_handle, const char* address, int port);
void ptk_udp_socket_sendto(ptk_handle_t sock_handle, const char* buffer, size_t len, const char* address, int port);
void ptk_udp_socket_recvfrom(ptk_handle_t sock_handle, char* buffer, size_t len, char* address, int* port);

// UDP multicast Sockets
void ptk_udp_socket_join_multicast_group(ptk_handle_t sock_handle, const char* group_address, const char* interface_address);
void ptk_udp_socket_leave_multicast_group(ptk_handle_t sock_handle, const char* group_address, const char* interface_address);
void ptk_udp_socket_send_multicast(ptk_handle_t sock_handle, const char* buffer, size_t len, const char* group_address);

// UDP broadcast Sockets
void ptk_udp_socket_broadcast(ptk_handle_t sock_handle, const char* address, int port, const char* buffer, size_t len);

// Error handling
int ptk_get_last_err();
void ptk_set_last_err(int err);

// Function declarations for user event sources
ptk_handle_t ptk_user_event_source_create();
void ptk_user_event_source_signal(ptk_handle_t handle);
void ptk_user_event_source_wait(ptk_handle_t handle);

// Timer functions
ptk_handle_t ptk_timer_start(uint64_t interval_ms, bool is_repeating);
void ptk_timer_stop(ptk_handle_t timer_handle);
bool ptk_timer_is_running(ptk_handle_t timer_handle);

#endif // PROTOCOL_TOOLKIT_H
