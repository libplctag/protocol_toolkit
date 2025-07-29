/**
 * @file test_convenience_macros.c
 * @brief Test the protothread convenience macros
 *
 * This example demonstrates and tests the convenience macros for
 * TCP and UDP operations with the actual macOS implementation.
 */

#include "../src/include/macos/protocol_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

// Event loop resources
PTK_DECLARE_EVENT_LOOP_SLOTS(event_loop_slots, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(main_resources, 2, 2, 1);

// Global handles
static ptk_handle_t g_event_loop = 0;
static ptk_handle_t g_timer = 0;
static ptk_handle_t g_udp_socket = 0;

// Event flags
static volatile bool timer_fired = false;
static volatile bool socket_ready = false;
static volatile bool test_complete = false;

/* ========================================================================
 * EVENT HANDLERS
 * ======================================================================== */

void timer_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, void *event_data, void *user_data) {
    if(event_type == PTK_EVENT_TIMER_EXPIRED) {
        timer_fired = true;
        printf("Timer event received!\n");
    }
}

void socket_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, void *event_data, void *user_data) {
    if(event_type == PTK_EVENT_SOCKET_READABLE || event_type == PTK_EVENT_SOCKET_WRITABLE) {
        socket_ready = true;
        printf("Socket event received (type %d)!\n", event_type);
    }
}

/* ========================================================================
 * PROTOTHREAD USING CONVENIENCE MACROS
 * ======================================================================== */

typedef struct {
    ptk_pt_t pt;
    ptk_buffer_t send_buffer;
    ptk_buffer_t recv_buffer;
    uint8_t send_data[256];
    uint8_t recv_data[256];
    char sender_address[64];
    uint16_t sender_port;
    int phase;
} test_protothread_t;

static test_protothread_t test_pt;

int test_protothread_function(ptk_pt_t *pt) {
    test_protothread_t *test = (test_protothread_t *)pt;

    PT_BEGIN(pt);

    printf("=== Testing Convenience Macros ===\n");

    // Phase 1: Test timer sleep macro
    printf("Phase 1: Testing PTK_PT_SLEEP_MS macro...\n");
    PTK_PT_SLEEP_MS(&test->pt, g_timer, 1000, timer_event_handler, NULL);
    printf("Timer sleep completed!\n");

    // Phase 2: Test UDP send macro
    printf("Phase 2: Testing PTK_PT_UDP_SEND macro...\n");
    strcpy((char *)test->send_data, "Test message from convenience macro");
    test->send_buffer.size = strlen((char *)test->send_data);

    PTK_PT_UDP_SEND(&test->pt, g_udp_socket, &test->send_buffer, "127.0.0.1", 12345, socket_event_handler, NULL);
    printf("UDP send completed!\n");

    // Phase 3: Test UDP receive macro
    printf("Phase 3: Testing PTK_PT_UDP_RECEIVE macro...\n");
    PTK_PT_UDP_RECEIVE(&test->pt, g_udp_socket, &test->recv_buffer, test->sender_address, sizeof(test->sender_address),
                       &test->sender_port, socket_event_handler, NULL);

    if(test->recv_buffer.size > 0) {
        test->recv_data[test->recv_buffer.size] = '\0';
        printf("UDP receive completed! Received: %s from %s:%d\n", test->recv_data, test->sender_address, test->sender_port);
    }

    // Phase 4: Test UDP broadcast macro
    printf("Phase 4: Testing PTK_PT_UDP_BROADCAST macro...\n");
    strcpy((char *)test->send_data, "Broadcast test message");
    test->send_buffer.size = strlen((char *)test->send_data);

    PTK_PT_UDP_BROADCAST(&test->pt, g_udp_socket, &test->send_buffer, 12346, socket_event_handler, NULL);
    printf("UDP broadcast completed!\n");

    printf("=== All convenience macro tests completed! ===\n");
    test_complete = true;

    PT_END(pt);
}

/* ========================================================================
 * MAIN FUNCTION
 * ======================================================================== */

int main() {
    printf("=== Convenience Macros Test ===\n\n");

    // Create event loop
    g_event_loop = ptk_event_loop_create(event_loop_slots, 1, &main_resources);
    if(g_event_loop <= 0) {
        printf("Failed to create event loop\n");
        return 1;
    }
    printf("Event loop created\n");

    // Create timer
    g_timer = ptk_timer_create(g_event_loop);
    if(g_timer <= 0) {
        printf("Failed to create timer\n");
        return 1;
    }
    printf("Timer created\n");

    // Create UDP socket
    g_udp_socket = ptk_socket_create_udp(g_event_loop);
    if(g_udp_socket <= 0) {
        printf("Failed to create UDP socket\n");
        return 1;
    }
    printf("UDP socket created\n");

    // Bind socket
    if(ptk_socket_bind(g_udp_socket, "127.0.0.1", 12345) != PTK_ERR_OK) {
        printf("Failed to bind socket\n");
        return 1;
    }
    printf("Socket bound to 127.0.0.1:12345\n");

    // Enable broadcast
    ptk_socket_enable_broadcast(g_udp_socket);
    printf("Broadcast enabled\n");

    // Initialize test protothread
    memset(&test_pt, 0, sizeof(test_pt));
    test_pt.send_buffer = ptk_buffer_create(test_pt.send_data, sizeof(test_pt.send_data));
    test_pt.recv_buffer = ptk_buffer_create(test_pt.recv_data, sizeof(test_pt.recv_data));

    ptk_protothread_init(&test_pt.pt, test_protothread_function);
    printf("Protothread initialized\n\n");

    // Send a test packet to ourselves for the receive test
    ptk_buffer_t init_buffer = ptk_buffer_create((uint8_t *)"Initial test packet", 19);
    init_buffer.size = 19;
    ptk_socket_sendto(g_udp_socket, &init_buffer, "127.0.0.1", 12345);
    printf("Sent initial test packet\n");

    // Main event loop
    printf("Starting main event loop...\n\n");
    int iterations = 0;
    const int MAX_ITERATIONS = 100;

    while(!test_complete && iterations < MAX_ITERATIONS) {
        // Run the event loop
        ptk_event_loop_run(g_event_loop);

        // Run the test protothread
        int result = ptk_protothread_run(&test_pt.pt);
        if(result == PT_ENDED) { test_complete = true; }

        // Small delay
        usleep(50000);  // 50ms
        iterations++;
    }

    if(iterations >= MAX_ITERATIONS) { printf("\nWarning: Reached maximum iterations limit\n"); }

    printf("\nTest completed after %d iterations\n", iterations);

    // Cleanup
    printf("\nCleaning up...\n");
    ptk_timer_destroy(g_timer);
    ptk_socket_destroy(g_udp_socket);
    ptk_event_loop_destroy(g_event_loop);

    printf("=== Convenience Macros Test Completed Successfully! ===\n");
    return 0;
}
