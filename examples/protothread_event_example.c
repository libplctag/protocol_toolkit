/**
 * @file protothread_event_example.c
 * @brief Protothread Event Handling Example
 *
 * This example demonstrates:
 * - Using protothreads to handle timer and socket events
 * - Event-driven programming with the protocol toolkit
 * - Coordinating multiple protothreads in an event loop
 */

#include "../src/include/macos/protocol_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ========================================================================
 * PROTOTHREAD MACROS (from protothreads.c - needed for examples)
 * ======================================================================== */

#define PT_WAITING 0
#define PT_YIELDED 1
#define PT_EXITED 2
#define PT_ENDED 3

#define PT_INIT(pt) \
    do { (pt)->lc = 0; } while(0)

#define PT_BEGIN(pt)            \
    do {                        \
        char PT_YIELD_FLAG = 1; \
        if(PT_YIELD_FLAG) {     \
            switch((pt)->lc) {  \
                case 0:

#define PT_END(pt)   \
    }                \
    }                \
    PT_INIT(pt);     \
    return PT_ENDED; \
    }                \
    while(0)

#define PT_YIELD(pt)         \
    do {                     \
        PT_YIELD_FLAG = 0;   \
        (pt)->lc = __LINE__; \
        return PT_YIELDED;   \
        case __LINE__:;      \
    } while(0)

#define PT_EXIT(pt)       \
    do {                  \
        PT_INIT(pt);      \
        return PT_EXITED; \
    } while(0)

#define PT_WAIT_UNTIL(pt, condition)            \
    do {                                        \
        PT_YIELD(pt);                           \
        if(!(condition)) { return PT_WAITING; } \
    } while(0)

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

// Event loop resources
PTK_DECLARE_EVENT_LOOP_SLOTS(event_loop_slots, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(main_resources, 2, 2, 1);

// Global handles
static ptk_handle_t g_event_loop = 0;
static ptk_handle_t g_timer1 = 0;
static ptk_handle_t g_timer2 = 0;
static ptk_handle_t g_udp_socket = 0;
static ptk_handle_t g_user_event_source = 0;

// Protothread states
static ptk_pt_t timer_pt;
static ptk_pt_t socket_pt;
static ptk_pt_t coordinator_pt;

// Event flags (set by event handlers)
static volatile bool timer1_fired = false;
static volatile bool timer2_fired = false;
static volatile bool socket_ready = false;
static volatile bool user_event_received = false;
static volatile int message_count = 0;

/* ========================================================================
 * EVENT HANDLERS
 * ======================================================================== */

void timer_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, void *event_data, void *user_data) {
    if(event_type == PTK_EVENT_TIMER_EXPIRED) {
        if(resource == g_timer1) {
            timer1_fired = true;
            printf("Timer 1 fired!\n");
        } else if(resource == g_timer2) {
            timer2_fired = true;
            printf("Timer 2 fired!\n");
        }
    }
}

void socket_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, void *event_data, void *user_data) {
    if(event_type == PTK_EVENT_SOCKET_READABLE) {
        socket_ready = true;
        printf("Socket has data to read!\n");
    }
}

void user_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, void *event_data, void *user_data) {
    if(event_type == 1001) {  // PTK_EVENT_USER_DEFINED + 1
        user_event_received = true;
        message_count++;
        printf("User event received! Count: %d\n", message_count);
    }
}

/* ========================================================================
 * PROTOTHREAD FUNCTIONS
 * ======================================================================== */

/**
 * Timer management protothread
 */
int timer_protothread(ptk_pt_t *pt) {
    PT_BEGIN(pt);

    printf("Timer protothread started\n");

    // Start timer 1 (1 second, repeating)
    ptk_timer_start(g_timer1, 1000, true);
    printf("Started timer 1 (1s repeating)\n");

    // Wait for timer 1 to fire 3 times
    static int timer_count = 0;
    while(timer_count < 3) {
        PT_WAIT_UNTIL(pt, timer1_fired);
        timer1_fired = false;  // Reset flag
        timer_count++;
        printf("Timer protothread: Timer 1 fired %d/3 times\n", timer_count);
    }

    // Stop timer 1 and start timer 2 (2 second, one-shot)
    ptk_timer_stop(g_timer1);
    ptk_timer_start(g_timer2, 2000, false);
    printf("Stopped timer 1, started timer 2 (2s one-shot)\n");

    // Wait for timer 2 to fire
    PT_WAIT_UNTIL(pt, timer2_fired);
    timer2_fired = false;
    printf("Timer protothread: Timer 2 fired, exiting\n");

    PT_END(pt);
}

/**
 * Socket communication protothread
 */
int socket_protothread(ptk_pt_t *pt) {
    static uint8_t send_buffer[256];
    static uint8_t recv_buffer[256];
    static ptk_buffer_t send_buf, recv_buf;
    static int packet_count = 0;

    PT_BEGIN(pt);

    printf("Socket protothread started\n");

    // Initialize buffers
    send_buf = ptk_buffer_create(send_buffer, sizeof(send_buffer));
    recv_buf = ptk_buffer_create(recv_buffer, sizeof(recv_buffer));

    // Bind socket to localhost:12345
    if(ptk_socket_bind(g_udp_socket, "127.0.0.1", 12345) != PTK_ERR_OK) {
        printf("Failed to bind socket\n");
        PT_EXIT(pt);
    }
    printf("Socket bound to 127.0.0.1:12345\n");

    // Send a few test packets to ourselves
    static int send_count = 0;
    while(send_count < 3) {
        // Prepare message
        snprintf((char *)send_buffer, sizeof(send_buffer), "Test message %d", send_count + 1);
        send_buf.size = strlen((char *)send_buffer);

        // Send packet
        if(ptk_socket_sendto(g_udp_socket, &send_buf, "127.0.0.1", 12345) == PTK_ERR_OK) {
            printf("Sent: %s\n", send_buffer);
            packet_count++;
        }

        send_count++;
        // Wait a bit between sends
        PT_YIELD(pt);
    }

    // Wait for and receive packets
    while(packet_count > 0) {
        PT_WAIT_UNTIL(pt, socket_ready);
        socket_ready = false;  // Reset flag

        // Try to receive data
        char sender_addr[64];
        uint16_t sender_port;
        if(ptk_socket_recvfrom(g_udp_socket, &recv_buf, sender_addr, sizeof(sender_addr), &sender_port) == PTK_ERR_OK) {
            recv_buffer[recv_buf.size] = '\0';  // Null terminate
            printf("Received from %s:%d: %s\n", sender_addr, sender_port, recv_buffer);
            packet_count--;
        }
    }

    printf("Socket protothread: All packets processed, exiting\n");

    PT_END(pt);
}

/**
 * Coordinator protothread that manages the overall flow
 */
int coordinator_protothread(ptk_pt_t *pt) {
    static int phase = 0;

    PT_BEGIN(pt);

    printf("Coordinator protothread started\n");

    // Phase 1: Wait for timer protothread to complete
    printf("Phase 1: Running timer tests\n");
    while(ptk_protothread_run(&timer_pt) != PT_ENDED) { PT_YIELD(pt); }
    printf("Phase 1 complete: Timer tests finished\n");

    // Phase 2: Wait for socket protothread to complete
    printf("Phase 2: Running socket tests\n");
    while(ptk_protothread_run(&socket_pt) != PT_ENDED) { PT_YIELD(pt); }
    printf("Phase 2 complete: Socket tests finished\n");

    // Phase 3: Generate some user events
    printf("Phase 3: Testing user events\n");
    static int event_count = 0;
    while(event_count < 3) {
        ptk_raise_event(g_user_event_source, (ptk_event_type_t)1001, NULL);
        PT_WAIT_UNTIL(pt, user_event_received);
        user_event_received = false;  // Reset flag
        event_count++;
        PT_YIELD(pt);  // Give time for processing
    }
    printf("Phase 3 complete: User events finished\n");

    printf("Coordinator protothread: All phases complete, exiting\n");

    PT_END(pt);
}

/* ========================================================================
 * MAIN FUNCTION
 * ======================================================================== */

int main() {
    printf("=== Protothread Event Handling Example ===\n\n");

    // Create event loop
    g_event_loop = ptk_event_loop_create(event_loop_slots, 1, &main_resources);
    if(g_event_loop <= 0) {
        printf("Failed to create event loop\n");
        return 1;
    }
    printf("Event loop created\n");

    // Create timers
    g_timer1 = ptk_timer_create(g_event_loop);
    g_timer2 = ptk_timer_create(g_event_loop);
    if(g_timer1 <= 0 || g_timer2 <= 0) {
        printf("Failed to create timers\n");
        return 1;
    }
    printf("Timers created\n");

    // Create UDP socket
    g_udp_socket = ptk_socket_create_udp(g_event_loop);
    if(g_udp_socket <= 0) {
        printf("Failed to create UDP socket\n");
        return 1;
    }
    printf("UDP socket created\n");

    // Create user event source
    g_user_event_source = ptk_user_event_source_create(g_event_loop);
    if(g_user_event_source <= 0) {
        printf("Failed to create user event source\n");
        return 1;
    }
    printf("User event source created\n");

    // Set up event handlers
    ptk_set_event_handler(g_timer1, PTK_EVENT_TIMER_EXPIRED, timer_event_handler, NULL);
    ptk_set_event_handler(g_timer2, PTK_EVENT_TIMER_EXPIRED, timer_event_handler, NULL);
    ptk_set_event_handler(g_udp_socket, PTK_EVENT_SOCKET_READABLE, socket_event_handler, NULL);
    ptk_set_event_handler(g_user_event_source, (ptk_event_type_t)1001, user_event_handler, NULL);
    printf("Event handlers set up\n");

    // Initialize protothreads
    ptk_protothread_init(&timer_pt, timer_protothread);
    ptk_protothread_init(&socket_pt, socket_protothread);
    ptk_protothread_init(&coordinator_pt, coordinator_protothread);
    printf("Protothreads initialized\n\n");

    // Main event loop
    printf("Starting main event loop...\n\n");
    bool all_done = false;
    int iterations = 0;
    const int MAX_ITERATIONS = 1000;  // Safety limit

    while(!all_done && iterations < MAX_ITERATIONS) {
        // Run the event loop once
        ptk_event_loop_run(g_event_loop);

        // Run the coordinator protothread
        int coordinator_result = ptk_protothread_run(&coordinator_pt);
        if(coordinator_result == PT_ENDED) { all_done = true; }

        // Small delay to prevent busy waiting
        usleep(10000);  // 10ms
        iterations++;
    }

    if(iterations >= MAX_ITERATIONS) { printf("\nWarning: Reached maximum iterations limit\n"); }

    printf("\nEvent loop finished after %d iterations\n", iterations);

    // Cleanup
    printf("\nCleaning up...\n");
    ptk_timer_destroy(g_timer1);
    ptk_timer_destroy(g_timer2);
    ptk_socket_destroy(g_udp_socket);
    ptk_user_event_source_destroy(g_user_event_source);
    ptk_event_loop_destroy(g_event_loop);

    printf("=== Example completed successfully! ===\n");
    return 0;
}
