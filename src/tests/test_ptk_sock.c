/**
 * @file test_ptk_sock.c
 * @brief Test for the simplified socket API
 *
 * This test creates a TCP echo server and multiple clients to test the
 * simplified socket API with one thread per socket.
 */

#include <ptk_sock.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_os_thread.h>
#include <ptk_buf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 12345
#define NUM_CLIENTS 5
#define MESSAGES_PER_CLIENT 10

// Shared server context
typedef struct {
    int message_counter;
    int total_connections;
    int total_messages;
    bool should_stop;
} server_context_t;

// Client context
typedef struct {
    int client_id;
    int messages_sent;
    int messages_received;
} client_context_t;

// Server thread function - echo with counter
void server_thread_func(ptk_sock *socket, ptk_shared_handle_t ctx_handle) {
    info("Server thread started for new client connection");
    
    server_context_t *ctx = ptk_shared_acquire(ctx_handle);
    if (ctx) {
        ctx->total_connections++;
        info("Total connections: %d", ctx->total_connections);
        ptk_shared_release(ctx_handle);
    } else {
        error("Failed to access server context");
        return;
    }
    
    while (true) {
        // Receive message from client
        ptk_buf *received = ptk_tcp_socket_recv(socket, 5000); // 5 second timeout
        if (!received) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_TIMEOUT) {
                debug("Server socket timeout, continuing...");
                continue;
            } else if (err == PTK_ERR_CLOSED) {
                info("Client disconnected");
                break;
            } else if (err == PTK_ERR_ABORT) {
                info("Server socket aborted");
                break;
            } else {
                error("Server recv failed with error: %d", err);
                break;
            }
        }
        
        // Get message content
        const char *msg = (const char *)(received->data + received->start);
        size_t msg_len = ptk_buf_get_len(received);
        
        info("Server received: %.*s", (int)msg_len, msg);
        
        // Create response with counter
        int counter = 0;
        ctx = ptk_shared_acquire(ctx_handle);
        if (ctx) {
            ctx->total_messages++;
            counter = ctx->total_messages;
            ptk_shared_release(ctx_handle);
        } else {
            error("Failed to access server context for counter");
            ptk_local_free(&received);
            break;
        }
        
        // Create echo response with counter
        char response[256];
        snprintf(response, sizeof(response), "Echo #%d: %.*s", counter, (int)msg_len, msg);
        
        ptk_buf *response_buf = ptk_buf_alloc_from_data((const uint8_t *)response, strlen(response));
        if (!response_buf) {
            error("Failed to create response buffer");
            ptk_local_free(&received);
            break;
        }
        
        // Send response back
        ptk_err send_err = ptk_tcp_socket_send(socket, response_buf, 5000);
        if (send_err != PTK_OK) {
            error("Server send failed with error: %d", send_err);
            ptk_local_free(&response_buf);
            ptk_local_free(&received);
            break;
        }
        
        info("Server sent: %s", response);
        
        // Clean up
        ptk_local_free(&response_buf);
        ptk_local_free(&received);
    }
    
    info("Server thread finished for client");
}

// Client thread function
void client_thread_func(ptk_sock *socket, ptk_shared_handle_t ctx_handle) {
    client_context_t *ctx = ptk_shared_acquire(ctx_handle);
    if (ctx) {
        info("Client %d thread started", ctx->client_id);
        
        for (int i = 0; i < MESSAGES_PER_CLIENT; i++) {
            // Create message
            char message[64];
            snprintf(message, sizeof(message), "Hello from client %d, message %d", ctx->client_id, i + 1);
            
            ptk_buf *msg_buf = ptk_buf_alloc_from_data((const uint8_t *)message, strlen(message));
            if (!msg_buf) {
                error("Client %d: Failed to create message buffer", ctx->client_id);
                break;
            }
            
            // Send message
            ptk_err send_err = ptk_tcp_socket_send(socket, msg_buf, 5000);
            if (send_err != PTK_OK) {
                error("Client %d: Send failed with error: %d", ctx->client_id, send_err);
                ptk_local_free(&msg_buf);
                break;
            }
            
            ctx->messages_sent++;
            info("Client %d sent: %s", ctx->client_id, message);
            ptk_local_free(&msg_buf);
            
            // Receive echo response
            ptk_buf *response = ptk_tcp_socket_recv(socket, 5000);
            if (!response) {
                error("Client %d: Recv failed with error: %d", ctx->client_id, ptk_get_err());
                break;
            }
            
            const char *resp_data = (const char *)(response->data + response->start);
            size_t resp_len = ptk_buf_get_len(response);
            
            ctx->messages_received++;
            info("Client %d received: %.*s", ctx->client_id, (int)resp_len, resp_data);
            
            ptk_local_free(&response);
            
            // Small delay between messages
            usleep(100000); // 100ms
        }
        
        info("Client %d finished: sent %d, received %d", 
             ctx->client_id, ctx->messages_sent, ctx->messages_received);
        ptk_shared_release(ctx_handle);
    } else {
        error("Failed to access client context");
    }
}

// Global server socket for cleanup
ptk_sock *g_server_socket = NULL;

// Client starter thread
void client_starter_thread(void *arg) {
    int client_id = *(int *)arg;
    
    // Give server time to start
    sleep(1);
    
    // Create client context
    client_context_t *client_ctx = ptk_local_alloc(sizeof(client_context_t), NULL);
    client_ctx->client_id = client_id;
    client_ctx->messages_sent = 0;
    client_ctx->messages_received = 0;
    
    ptk_shared_handle_t client_ctx_handle = ptk_shared_create(client_ctx);
    
    // Connect to server
    ptk_address_t server_addr;
    ptk_address_init(&server_addr, "127.0.0.1", SERVER_PORT);
    
    info("Client %d connecting to server", client_id);
    
    ptk_sock *client_sock = ptk_tcp_connect(&server_addr, client_thread_func, client_ctx_handle);
    if (!client_sock) {
        error("Client %d: Failed to connect: %d", client_id, ptk_get_err());
        ptk_shared_release(client_ctx_handle);
        return;
    }
    
    info("Client %d connected", client_id);
    
    // Wait for client thread to finish
    sleep(5); // Give time for messages to complete
    
    // Close client socket
    ptk_socket_close(client_sock);
    ptk_shared_release(client_ctx_handle);
    
    info("Client %d thread finished", client_id);
    return;
}

int main() {
    info("=== Starting PTK Socket Test ===");
    
    // Initialize shared memory subsystem
    ptk_shared_init();
    
    // Create server context
    server_context_t *server_ctx = ptk_local_alloc(sizeof(server_context_t), NULL);
    server_ctx->message_counter = 0;
    server_ctx->total_connections = 0;
    server_ctx->total_messages = 0;
    server_ctx->should_stop = false;
    
    ptk_shared_handle_t server_ctx_handle = ptk_shared_create(server_ctx);
    
    // Start server (returns immediately with server socket handle)
    ptk_address_t server_addr;
    ptk_address_init_any(&server_addr, SERVER_PORT);
    
    info("Starting TCP server on port %d", SERVER_PORT);
    g_server_socket = ptk_tcp_server_start(&server_addr, server_thread_func, server_ctx_handle);
    if (!g_server_socket) {
        error("Failed to start server: %d", ptk_get_err());
        return 1;
    }
    
    info("TCP server started successfully");
    
    // Give server time to start accepting
    sleep(1);
    
    // Start client threads
    ptk_thread *client_threads[NUM_CLIENTS];
    int client_ids[NUM_CLIENTS];
    
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_ids[i] = i + 1;
        client_threads[i] = ptk_thread_create(client_starter_thread, &client_ids[i]);
        if (!client_threads[i]) {
            error("Failed to create client thread %d", i + 1);
        }
    }
    
    // Wait for all clients to finish
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (client_threads[i]) {
            ptk_thread_join(client_threads[i]);
        }
    }
    
    info("All clients finished");
    
    // Print final server stats
    server_context_t *ctx = ptk_shared_acquire(server_ctx_handle);
    if (ctx) {
        info("=== Test Summary ===");
        info("Total connections: %d", ctx->total_connections);
        info("Total messages processed: %d", ctx->total_messages);
        info("Expected messages: %d", NUM_CLIENTS * MESSAGES_PER_CLIENT);
        
        if (ctx->total_messages == NUM_CLIENTS * MESSAGES_PER_CLIENT) {
            info("✓ All messages processed successfully!");
        } else {
            error("✗ Message count mismatch!");
        }
        ptk_shared_release(server_ctx_handle);
    } else {
        error("Failed to access server context for final stats");
    }
    
    // Stop the server gracefully
    info("Stopping TCP server...");
    ptk_socket_close(g_server_socket);
    g_server_socket = NULL;
    
    info("=== Test Complete Successfully! ===");
    
    // Clean up
    ptk_shared_release(server_ctx_handle);
    ptk_shared_shutdown();
    
    return 0;
}
