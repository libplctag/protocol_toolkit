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

#include "ptk_alloc.h"
#include "ptk_buf.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include "ptk_socket.h"
#include "ptk_thread.h"
#include "ptk_utils.h"

//=============================================================================
// GLOBALS
//=============================================================================

static volatile bool g_test_running = true;
static ptk_sock *g_server_socket = NULL;
static ptk_thread *g_server_thread = NULL;
static ptk_allocator_t *g_allocator = NULL;
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
// SERVER CLIENT THREAD
//=============================================================================

typedef struct {
    ptk_sock *client_socket;
    int client_id;
    int message_num;
} server_client_data;

static void server_client_thread(void *arg) {
    ptk_err err = PTK_OK;
    server_client_data *data = (server_client_data *)arg;
    ptk_sock *client = data->client_socket;
    int client_id = data->client_id;

    info("[CLIENT_HANDLER_%d] Started\n", client_id);

    ptk_buf *io_buf = ptk_buf_create(g_allocator, 1024);
    if(!io_buf) {
        error("Unable to allocated buffer memory!");
        return;
    }

    while(g_test_running) {

        // Read data from client
        ptk_err err = ptk_tcp_socket_recv(io_buf, client);

        if(err == PTK_ERR_ABORT) {
            info("[CLIENT_HANDLER_%d] Aborted.", client_id);
            break;
        } else if(err == PTK_ERR_CLOSED) {
            info("[CLIENT_HANDLER_%d] Client disconnected\n", client_id);
            break;
        } else if(err != PTK_OK) {
            info("[CLIENT_HANDLER_%d] Read error: %s\n", client_id, ptk_err_to_string(err));
            break;
        }

        // Check if we received any data
        size_t data_len;
        data_len = ptk_buf_len(io_buf);
        if(data_len == 0) { continue; }

        char msg_buf[1024] = {0};
        size_t client_msg_len = 0;
        u8 *client_msg = NULL;

        client_msg_len = ptk_buf_len(io_buf);

        snprintf(msg_buf, sizeof(msg_buf), "Message %d: %.*s", ++data->message_num, (int)client_msg_len,
                 ptk_buf_get_start_ptr(io_buf));

        info("[CLIENT_HANDLER_%d] Echoing %zu bytes\n", client_id, data_len);

        err = ptk_tcp_socket_send(client, io_buf);
        if(err == PTK_ERR_ABORT) {
            info("[CLIENT_HANDLER_%d] Write aborted\n", client_id);
            break;
        } else if(err != PTK_OK) {
            info("[CLIENT_HANDLER_%d] Write error: %s\n", client_id, ptk_err_to_string(err));
            break;
        }
    }

    info("[CLIENT_HANDLER_%d] Stopping\n", client_id);
    ptk_buf_dispose(io_buf);
    ptk_socket_close(client);
    free(data);
}

//=============================================================================
// SERVER THREAD
//=============================================================================

static void server_thread(void *arg) {
    info("[SERVER] Starting TCP server on port %d\n", TEST_PORT);

    ptk_address_t server_addr;
    ptk_err err = ptk_address_create(&server_addr, "127.0.0.1", TEST_PORT);
    if(err != PTK_OK) {
        info("[SERVER] Failed to create server address\n");
        return;
    }

    g_server_socket = ptk_tcp_socket_listen(g_allocator, &server_addr, 5);
    if(!g_server_socket) {
        info("[SERVER] Failed to start server\n");
        return;
    }

    info("[SERVER] Listening for connections\n");

    while(g_test_running) {
        ptk_sock *client_socket = ptk_tcp_socket_accept(g_server_socket);

        if(!client_socket) {
            if(ptk_socket_last_error(g_server_socket) == PTK_ERR_ABORT) {
                info("[SERVER] Accept aborted\n");
                break;
            } else {
                info("[SERVER] Accept error\n");
                continue;
            }
        }

        info("[SERVER] New client connected\n");

        // Add client to list and start handler thread
        ptk_mutex_wait_lock(g_clients_mutex, PTK_TIME_WAIT_FOREVER);

        if(g_num_clients < MAX_CLIENTS) {
            g_client_sockets[g_num_clients] = client_socket;

            server_client_data *handler_data = malloc(sizeof(server_client_data));
            handler_data->client_socket = client_socket;
            handler_data->client_id = (int)g_num_clients;

            g_client_handler_threads[g_num_clients] =
                ptk_thread_create(allocator_default_create(0), server_client_thread, handler_data);

            g_num_clients++;
        } else {
            info("[SERVER] Too many clients, rejecting connection\n");
            ptk_socket_close(client_socket);
        }

        ptk_mutex_unlock(g_clients_mutex);
    }

    // Abort all client sockets when server is aborted
    info("[SERVER] Aborting all client connections\n");
    ptk_mutex_wait_lock(g_clients_mutex, PTK_TIME_WAIT_FOREVER);

    for(size_t i = 0; i < g_num_clients; i++) {
        if(g_client_sockets[i]) { ptk_socket_abort(g_client_sockets[i]); }
    }

    ptk_mutex_unlock(g_clients_mutex);

    info("[SERVER] Stopping\n");
}

//=============================================================================
// CLIENT INTERRUPT HANDLER
//=============================================================================

static void client_timer_interrupt(ptk_sock *sock, ptk_time_ms time_ms, void *user_data) {
    info("[CLIENT] Timer interrupt fired at %lld ms\n", time_ms);
}

//=============================================================================
// CLIENT THREAD
//=============================================================================

static void client_thread(void *arg) {
    info("[CLIENT] Starting echo client\n");

    ptk_err err;
    ptk_address_t remote_addr;
    err = ptk_address_create(&remote_addr, "127.0.0.1", TEST_PORT);
    if(err != PTK_OK) {
        info("[CLIENT] Failed to create remote address\n");
        return;
    }

    ptk_sock *client_socket = ptk_tcp_socket_connect(g_allocator, &remote_addr);
    if(!client_socket) {
        info("[CLIENT] Failed to connect\n");
        return;
    }

    info("[CLIENT] Connected to server\n");

    // Set interrupt handler and start timer
    ptk_socket_set_interrupt_handler(client_socket, client_timer_interrupt, NULL);
    ptk_socket_start_repeat_interrupt(client_socket, TIMER_PERIOD_MS);

    info("[CLIENT] Waiting for timer interrupt (%d ms)\n", TIMER_PERIOD_MS);
    err = ptk_socket_wait_for_interrupt(client_socket);

    if(err == PTK_ERR_ABORT) {
        info("[CLIENT] Wait for interrupt aborted\n");
        ptk_socket_close(client_socket);
        return;
    } else if(err != PTK_OK) {
        info("[CLIENT] Wait for interrupt failed: %s\n", ptk_err_to_string(err));
        ptk_socket_close(client_socket);
        return;
    }

    // Send message to server
    const char *message = "Hello from client!";

    ptk_buf *send_buf = ptk_buf_create(g_allocator, 1024);
    if(!send_buf) {
        info("[CLIENT] Failed to create send buffer\n");
        ptk_socket_close(client_socket);
        return;
    }

    // Copy message to buffer
    memcpy(ptk_buf_get_end_ptr(send_buf), message, strlen(message));
    ptk_buf_set_end(send_buf, strlen(message));

    info("[CLIENT] Sending message: '%s'\n", message);
    err = ptk_tcp_socket_send(client_socket, send_buf);
    if(err != PTK_OK) {
        info("[CLIENT] Failed to send message: %s\n", ptk_err_to_string(err));
        ptk_buf_dispose(send_buf);
        ptk_socket_close(client_socket);
        return;
    }

    // Read response
    ptk_buf *recv_buf = ptk_buf_create(g_allocator, 1024);
    if(!recv_buf) {
        info("[CLIENT] Failed to create receive buffer\n");
        ptk_buf_dispose(send_buf);
        ptk_socket_close(client_socket);
        return;
    }

    info("[CLIENT] Reading response\n");
    err = ptk_tcp_socket_recv(recv_buf, client_socket);
    if(err != PTK_OK) {
        info("[CLIENT] Failed to read response: %s\n", ptk_err_to_string(err));
        ptk_buf_dispose(send_buf);
        ptk_buf_dispose(recv_buf);
        ptk_socket_close(client_socket);
        return;
    }

    // Print response
    size_t response_len = ptk_buf_len(recv_buf);
    if(response_len > 0) {
        u8 *response_data = ptk_buf_get_start_ptr(recv_buf);
        info("[CLIENT] Received response (%zu bytes): '%.*s'\n", response_len, (int)response_len, response_data);
    }

    ptk_buf_dispose(send_buf);
    ptk_buf_dispose(recv_buf);

    info("[CLIENT] Test completed successfully\n");
    ptk_socket_close(client_socket);
}

//=============================================================================
// SIGNAL HANDLER
//=============================================================================

static void signal_handler(void) {
    g_test_running = false;
    info("\n[MAIN] Signal received, stopping test\n");
}

//=============================================================================
// MAIN TEST
//=============================================================================

int main() {
    info("=== TCP Echo Server/Client Abort Test ===\n");

    // Create allocator
    g_allocator = allocator_default_create(8);
    if(!g_allocator) {
        error("Failed to create allocator\n");
        return 1;
    }

    // Setup signal handler
    ptk_set_interrupt_handler(signal_handler);

    // Create mutex for client list
    g_clients_mutex = ptk_mutex_create(allocator_default_create(0));
    if(!g_clients_mutex) {
        info("[MAIN] Failed to create mutex\n");
        return 1;
    }

    // Initialize client arrays
    memset(g_client_sockets, 0, sizeof(g_client_sockets));
    memset(g_client_handler_threads, 0, sizeof(g_client_handler_threads));

    // Start server thread
    g_server_thread = ptk_thread_create(allocator_default_create(0), server_thread, NULL);
    if(!g_server_thread) {
        info("[MAIN] Failed to create server thread\n");
        return 1;
    }

    // Give server time to start
    sleep(1);

    // Start client thread
    g_client_thread = ptk_thread_create(allocator_default_create(0), client_thread, NULL);
    if(!g_client_thread) {
        info("[MAIN] Failed to create client thread\n");
        return 1;
    }

    // Run test for specified duration
    info("[MAIN] Test will run for %d seconds\n", TEST_DURATION_SEC);
    sleep(TEST_DURATION_SEC);

    // Stop test cleanly
    info("[MAIN] Test duration completed, stopping cleanly\n");
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

    // Cleanup allocator
    ptk_allocator_destroy(g_allocator);

    info("[MAIN] Test completed\n");
    return 0;
}