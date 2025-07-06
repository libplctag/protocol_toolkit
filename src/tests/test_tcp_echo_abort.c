/**
 * Test 1: TCP Echo Server/Client with Abort
 *
 * - Server thread: opens TCP listener, accepts connections, spawns client handlers
 * - When server receives abort, it aborts all client sockets
 * - Client handlers: echo back anything received
 * - Client thread: connects, sets 500ms timer, waits for interrupt, sends message, reads response
 * - Test runs for 5 seconds then cleanly shuts down
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define MAX_CLIENTS 10
static ptk_sock *g_client_sockets[MAX_CLIENTS];
static ptk_thread *g_client_handler_threads[MAX_CLIENTS];
static size_t g_num_clients = 0;
static ptk_mutex *g_clients_mutex = NULL;

#define TEST_PORT 12345
#define TIMER_PERIOD_MS 500
#define TEST_DURATION_SEC 5

//=============================================================================
// CLIENT HANDLER THREAD
//=============================================================================

typedef struct {
    ptk_sock *client_socket;
    int client_id;
} client_handler_data;

static void client_handler_thread(void *arg) {
    client_handler_data *data = (client_handler_data *)arg;
    ptk_sock *client = data->client_socket;
    int client_id = data->client_id;

    printf("[CLIENT_HANDLER_%d] Started\n", client_id);

    uint8_t buffer_data[1024];
    ptk_buf recv_buf, send_buf;

    while(g_test_running) {
        // Setup receive buffer
        ptk_buf_make(&recv_buf, buffer_data, sizeof(buffer_data));

        // Read data from client
        ptk_err err = ptk_tcp_socket_read(client, &recv_buf);

        if(err == PTK_ERR_ABORT) {
            printf("[CLIENT_HANDLER_%d] Aborted\n", client_id);
            break;
        } else if(err == PTK_ERR_CLOSED) {
            printf("[CLIENT_HANDLER_%d] Client disconnected\n", client_id);
            break;
        } else if(err != PTK_OK) {
            printf("[CLIENT_HANDLER_%d] Read error: %s\n", client_id, ptk_err_to_string(err));
            break;
        }

        // Check if we received any data
        size_t data_len;
        ptk_buf_len(&data_len, &recv_buf);
        if(data_len == 0) { continue; }

        // Echo the data back
        ptk_buf_make(&send_buf, buffer_data, sizeof(buffer_data));
        ptk_buf_set_end(&send_buf, data_len);

        printf("[CLIENT_HANDLER_%d] Echoing %zu bytes\n", client_id, data_len);

        err = ptk_tcp_socket_write(client, &send_buf);
        if(err == PTK_ERR_ABORT) {
            printf("[CLIENT_HANDLER_%d] Write aborted\n", client_id);
            break;
        } else if(err != PTK_OK) {
            printf("[CLIENT_HANDLER_%d] Write error: %s\n", client_id, ptk_err_to_string(err));
            break;
        }
    }

    printf("[CLIENT_HANDLER_%d] Stopping\n", client_id);
    ptk_socket_close(client);
    free(data);
}

//=============================================================================
// SERVER THREAD
//=============================================================================

static void server_thread(void *arg) {
    printf("[SERVER] Starting TCP server on port %d\n", TEST_PORT);

    ptk_err err = ptk_tcp_socket_listen(&g_server_socket, "127.0.0.1", TEST_PORT, 5);
    if(err != PTK_OK) {
        printf("[SERVER] Failed to start server: %s\n", ptk_err_to_string(err));
        return;
    }

    printf("[SERVER] Listening for connections\n");

    while(g_test_running) {
        ptk_sock *client_socket;
        err = ptk_tcp_socket_accept(g_server_socket, &client_socket);

        if(err == PTK_ERR_ABORT) {
            printf("[SERVER] Accept aborted\n");
            break;
        } else if(err != PTK_OK) {
            printf("[SERVER] Accept error: %s\n", ptk_err_to_string(err));
            continue;
        }

        printf("[SERVER] New client connected\n");

        // Add client to list and start handler thread
        ptk_mutex_wait_lock(g_clients_mutex, PTK_TIME_WAIT_FOREVER);

        if(g_num_clients < MAX_CLIENTS) {
            g_client_sockets[g_num_clients] = client_socket;

            client_handler_data *handler_data = malloc(sizeof(client_handler_data));
            handler_data->client_socket = client_socket;
            handler_data->client_id = (int)g_num_clients;

            ptk_thread_create(&g_client_handler_threads[g_num_clients], client_handler_thread, handler_data);

            g_num_clients++;
        } else {
            printf("[SERVER] Too many clients, rejecting connection\n");
            ptk_socket_close(client_socket);
        }

        ptk_mutex_unlock(g_clients_mutex);
    }

    // Abort all client sockets when server is aborted
    printf("[SERVER] Aborting all client connections\n");
    ptk_mutex_wait_lock(g_clients_mutex, PTK_TIME_WAIT_FOREVER);

    for(size_t i = 0; i < g_num_clients; i++) {
        if(g_client_sockets[i]) { ptk_socket_abort(g_client_sockets[i]); }
    }

    ptk_mutex_unlock(g_clients_mutex);

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
    printf("[CLIENT] Starting echo client\n");

    ptk_sock *client_socket;
    ptk_err err = ptk_tcp_socket_connect(&client_socket, "127.0.0.1", TEST_PORT);
    if(err != PTK_OK) {
        printf("[CLIENT] Failed to connect: %s\n", ptk_err_to_string(err));
        return;
    }

    printf("[CLIENT] Connected to server\n");

    // Set interrupt handler and start timer
    ptk_socket_set_interrupt_handler(client_socket, client_timer_interrupt, NULL);
    ptk_socket_start_repeat_interrupt(client_socket, TIMER_PERIOD_MS);

    printf("[CLIENT] Waiting for timer interrupt (%d ms)\n", TIMER_PERIOD_MS);
    err = ptk_socket_wait_for_interrupt(client_socket);

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
    const char *message = "Hello from client!";
    uint8_t buffer_data[1024];
    ptk_buf send_buf;

    ptk_buf_make(&send_buf, buffer_data, sizeof(buffer_data));
    memcpy(buffer_data, message, strlen(message));
    ptk_buf_set_end(&send_buf, strlen(message));

    printf("[CLIENT] Sending message: '%s'\n", message);
    err = ptk_tcp_socket_write(client_socket, &send_buf);
    if(err != PTK_OK) {
        printf("[CLIENT] Failed to send message: %s\n", ptk_err_to_string(err));
        ptk_socket_close(client_socket);
        return;
    }

    // Read response
    ptk_buf recv_buf;
    ptk_buf_make(&recv_buf, buffer_data, sizeof(buffer_data));

    printf("[CLIENT] Reading response\n");
    err = ptk_tcp_socket_read(client_socket, &recv_buf);
    if(err != PTK_OK) {
        printf("[CLIENT] Failed to read response: %s\n", ptk_err_to_string(err));
        ptk_socket_close(client_socket);
        return;
    }

    // Print response
    size_t response_len;
    ptk_buf_len(&response_len, &recv_buf);
    if(response_len > 0) {
        buffer_data[response_len] = '\0';  // Null terminate for printing
        printf("[CLIENT] Received response: '%s'\n", buffer_data);
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
    printf("=== TCP Echo Server/Client Abort Test ===\n");

    // Setup signal handler
    ptk_set_interrupt_handler(signal_handler);

    // Create mutex for client list
    ptk_err err = ptk_mutex_create(&g_clients_mutex);
    if(err != PTK_OK) {
        printf("[MAIN] Failed to create mutex: %s\n", ptk_err_to_string(err));
        return 1;
    }

    // Initialize client arrays
    memset(g_client_sockets, 0, sizeof(g_client_sockets));
    memset(g_client_handler_threads, 0, sizeof(g_client_handler_threads));

    // Start server thread
    err = ptk_thread_create(&g_server_thread, server_thread, NULL);
    if(err != PTK_OK) {
        printf("[MAIN] Failed to create server thread: %s\n", ptk_err_to_string(err));
        return 1;
    }

    // Give server time to start
    sleep(1);

    // Start client thread
    err = ptk_thread_create(&g_client_thread, client_thread, NULL);
    if(err != PTK_OK) {
        printf("[MAIN] Failed to create client thread: %s\n", ptk_err_to_string(err));
        return 1;
    }

    // Run test for specified duration
    printf("[MAIN] Test will run for %d seconds\n", TEST_DURATION_SEC);
    sleep(TEST_DURATION_SEC);

    // Stop test cleanly
    printf("[MAIN] Test duration completed, stopping cleanly\n");
    g_test_running = false;

    // Abort server socket to stop accept loop
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

    // Wait for client handler threads
    ptk_mutex_wait_lock(g_clients_mutex, PTK_TIME_WAIT_FOREVER);
    for(size_t i = 0; i < g_num_clients; i++) {
        if(g_client_handler_threads[i]) {
            ptk_thread_join(g_client_handler_threads[i]);
            ptk_thread_destroy(g_client_handler_threads[i]);
        }
    }
    ptk_mutex_unlock(g_clients_mutex);

    // Cleanup
    if(g_server_socket) { ptk_socket_close(g_server_socket); }

    ptk_mutex_destroy(g_clients_mutex);

    printf("[MAIN] Test completed\n");
    return 0;
}