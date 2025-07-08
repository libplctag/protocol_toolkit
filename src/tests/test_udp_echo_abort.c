/**
 * Test 2: UDP Echo Server/Client with Abort
 *
 * - Server thread: opens UDP socket, receives packets, echoes back to sender
 * - When server receives abort, it stops the receive loop
 * - Client thread: connects, sets 500ms timer, waits for interrupt, sends message, reads response
 * - Test runs for 5 seconds then cleanly shuts down
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ptk_alloc.h"
#include "ptk_buf.h"
#include "ptk_err.h"
#include "ptk_socket.h"
#include "ptk_thread.h"
#include "ptk_utils.h"

//=============================================================================
// GLOBALS
//=============================================================================

static volatile bool g_test_running = true;
static ptk_sock *g_server_socket = NULL;
static ptk_thread *g_server_thread = NULL;
static ptk_thread *g_client_thread = NULL;
static ptk_allocator_t *g_allocator = NULL;

#define TEST_PORT 12346
#define TIMER_PERIOD_MS 500
#define TEST_DURATION_SEC 5

//=============================================================================
// SERVER THREAD
//=============================================================================

static void server_thread(void *arg) {
    printf("[SERVER] Starting UDP server on port %d\n", TEST_PORT);

    ptk_address_t server_addr;
    ptk_err err = ptk_address_create(&server_addr, "127.0.0.1", TEST_PORT);
    if(err != PTK_OK) {
        printf("[SERVER] Failed to create server address\n");
        return;
    }

    g_server_socket = ptk_udp_socket_create(g_allocator, &server_addr);
    if(!g_server_socket) {
        printf("[SERVER] Failed to create UDP socket\n");
        return;
    }

    printf("[SERVER] Listening for UDP packets\n");

    uint8_t buffer_data[1024];

    while(g_test_running) {
        ptk_buf_t *recv_buf = ptk_buf_create(g_allocator, sizeof(buffer_data));

        // Receive packet
        ptk_address_t client_addr;
        err = ptk_udp_socket_recv_from(g_server_socket, recv_buf, &client_addr);

        if(err == PTK_ERR_ABORT) {
            printf("[SERVER] Receive aborted\n");
            break;
        } else if(err == PTK_ERR_WOULD_BLOCK) {
            // No data available, continue waiting
            continue;
        } else if(err != PTK_OK) {
            printf("[SERVER] Receive error: %s\n", ptk_err_to_string(err));
            continue;
        }

        // Check if we received any data
        size_t data_len;
        data_len = ptk_buf_len(recv_buf);
        if(data_len == 0) { continue; }

        char *client_ip = ptk_address_to_string(g_allocator, &client_addr);
        printf("[SERVER] Received %zu bytes from %s:%d\n", data_len, 
               client_ip ? client_ip : "unknown", ptk_address_get_port(&client_addr));
        if(client_ip) ptk_free(g_allocator, client_ip);

        // Echo the data back
        ptk_buf_t *send_buf = ptk_buf_create(g_allocator, sizeof(buffer_data));
        if(!send_buf) {
            printf("[SERVER] Failed to create send buffer\n");
            continue;
        }
        memcpy(ptk_buf_get_end_ptr(send_buf), ptk_buf_get_start_ptr(recv_buf), data_len);
        ptk_buf_set_end(send_buf, data_len);

        err = ptk_udp_socket_send_to(g_server_socket, send_buf, &client_addr, false);
        if(err == PTK_ERR_ABORT) {
            printf("[SERVER] Send aborted\n");
            break;
        } else if(err != PTK_OK) {
            printf("[SERVER] Send error: %s\n", ptk_err_to_string(err));
            continue;
        }

        printf("[SERVER] Echoed %zu bytes back to client\n", data_len);
    }

    printf("[SERVER] Stopping\n");
}

//=============================================================================
// CLIENT INTERRUPT HANDLER
//=============================================================================

static void client_timer_interrupt(ptk_sock *sock, ptk_time_ms time_ms, void *user_data) {
    printf("[CLIENT] Timer interrupt fired at %lld ms\n", time_ms);
}

//=============================================================================
// CLIENT THREAD
//=============================================================================

static void client_thread(void *arg) {
    printf("[CLIENT] Starting UDP echo client\n");

    ptk_sock *client_socket = ptk_udp_socket_create(g_allocator, NULL);  // Don't bind to specific port
    if(!client_socket) {
        printf("[CLIENT] Failed to create UDP socket\n");
        return;
    }

    printf("[CLIENT] Created UDP socket\n");

    // Set interrupt handler and start timer
    ptk_socket_set_interrupt_handler(client_socket, client_timer_interrupt, NULL);
    ptk_socket_start_repeat_interrupt(client_socket, TIMER_PERIOD_MS);

    printf("[CLIENT] Waiting for timer interrupt (%d ms)\n", TIMER_PERIOD_MS);
    ptk_err err = ptk_socket_wait_for_interrupt(client_socket);

    if(err == PTK_ERR_ABORT) {
        printf("[CLIENT] Wait for interrupt aborted\n");
        ptk_socket_close(client_socket);
        return;
    } else if(err != PTK_OK) {
        printf("[CLIENT] Wait for interrupt failed: %s\n", ptk_err_to_string(err));
        ptk_socket_close(client_socket);
        return;
    }

    // Send message to server
    const char *message = "Hello UDP from client!";
    uint8_t buffer_data[1024];

    ptk_buf_t *send_buf = ptk_buf_create(g_allocator, sizeof(buffer_data));
    if(!send_buf) {
        printf("[CLIENT] Failed to create send buffer\n");
        ptk_socket_close(client_socket);
        return;
    }
    memcpy(ptk_buf_get_end_ptr(send_buf), message, strlen(message));
    ptk_buf_set_end(send_buf, strlen(message));

    ptk_address_t server_dest_addr;
    err = ptk_address_create(&server_dest_addr, "127.0.0.1", TEST_PORT);
    if(err != PTK_OK) {
        printf("[CLIENT] Failed to create server address\n");
        ptk_socket_close(client_socket);
        return;
    }

    printf("[CLIENT] Sending UDP message: '%s'\n", message);
    err = ptk_udp_socket_send_to(client_socket, send_buf, &server_dest_addr, false);
    if(err != PTK_OK) {
        printf("[CLIENT] Failed to send UDP message: %s\n", ptk_err_to_string(err));
        ptk_socket_close(client_socket);
        return;
    }

    // Read response
    ptk_buf_t *recv_buf = ptk_buf_create(g_allocator, sizeof(buffer_data));

    printf("[CLIENT] Reading UDP response\n");
    ptk_address_t response_addr;
    err = ptk_udp_socket_recv_from(client_socket, recv_buf, &response_addr);
    if(err != PTK_OK) {
        printf("[CLIENT] Failed to read UDP response: %s\n", ptk_err_to_string(err));
        ptk_socket_close(client_socket);
        return;
    }

    // Print response
    size_t response_len;
    response_len = ptk_buf_len(recv_buf);
    if(response_len > 0) {
        buffer_data[response_len] = '\0';  // Null terminate for printing
        char *response_ip = ptk_address_to_string(g_allocator, &response_addr);
        printf("[CLIENT] Received UDP response from %s:%d: '%s'\n", 
               response_ip ? response_ip : "unknown", ptk_address_get_port(&response_addr), buffer_data);
        if(response_ip) ptk_free(g_allocator, response_ip);
    }

    printf("[CLIENT] Test completed successfully\n");
    ptk_socket_close(client_socket);
}

//=============================================================================
// SIGNAL HANDLER
//=============================================================================

static void signal_handler(void) {
    g_test_running = false;
    printf("\n[MAIN] Signal received, stopping test\n");
}

//=============================================================================
// MAIN TEST
//=============================================================================

int main() {
    printf("=== UDP Echo Server/Client Abort Test ===\n");

    // Create allocator
    g_allocator = allocator_default_create(8);
    if(!g_allocator) {
        printf("Failed to create allocator\n");
        return 1;
    }

    // Setup signal handler
    ptk_set_interrupt_handler(signal_handler);

    // Start server thread
    g_server_thread = ptk_thread_create(allocator_default_create(0), server_thread, NULL);
    if(!g_server_thread) {
        printf("[MAIN] Failed to create server thread\n");
        return 1;
    }

    // Give server time to start
    sleep(1);

    // Start client thread
    g_client_thread = ptk_thread_create(allocator_default_create(0), client_thread, NULL);
    if(!g_client_thread) {
        printf("[MAIN] Failed to create client thread\n");
        return 1;
    }

    // Run test for specified duration
    printf("[MAIN] Test will run for %d seconds\n", TEST_DURATION_SEC);
    sleep(TEST_DURATION_SEC);

    // Stop test cleanly
    printf("[MAIN] Test duration completed, stopping cleanly\n");
    g_test_running = false;

    // Abort server socket to stop receive loop
    if(g_server_socket) { ptk_socket_abort(g_server_socket); }

    // Wait for threads to complete
    if(g_server_thread) {
        ptk_thread_join(g_server_thread);
        ptk_thread_destroy(g_server_thread);
    }

    if(g_client_thread) {
        ptk_thread_join(g_client_thread);
        ptk_thread_destroy(g_client_thread);
    }

    // Cleanup
    if(g_server_socket) { ptk_socket_close(g_server_socket); }

    // Cleanup allocator
    ptk_allocator_destroy(g_allocator);

    printf("[MAIN] Test completed\n");
    return 0;
}