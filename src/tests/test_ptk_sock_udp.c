/**
 * @file test_ptk_sock_udp.c
 * @brief Test for the simplified UDP socket API
 *
 * This test creates a UDP echo server and multiple clients to test the
 * simplified socket API with one thread per socket using UDP.
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
#include <arpa/inet.h>
#include <netinet/in.h>

#define SERVER_PORT 12346
#define NUM_CLIENTS 5
#define MESSAGES_PER_CLIENT 10

// Shared server context
typedef struct {
    int message_counter;
    int total_clients;
    int total_messages;
    bool should_stop;
} udp_server_context_t;

// Client context
typedef struct {
    int client_id;
    int messages_sent;
    int messages_received;
} udp_client_context_t;

// Global server socket for cleanup
ptk_sock *g_udp_server_socket = NULL;

// UDP Server thread function - echo with counter
void udp_server_thread_func(ptk_sock *socket, ptk_shared_handle_t ctx_handle) {
    info("UDP server thread started");
    
    udp_server_context_t *ctx = ptk_shared_acquire(ctx_handle);
    if (!ctx) {
        error("Failed to access UDP server context");
        return;
    }
    
    info("UDP server ready to receive messages");
    ptk_shared_release(ctx_handle);
    
    while (true) {
        // Receive message from any client
        ptk_address_t sender_addr;
        ptk_buf *received = ptk_udp_socket_recv_from(socket, &sender_addr, 5000); // 5 second timeout
        if (!received) {
            ptk_err err = ptk_get_err();
            if (err == PTK_ERR_TIMEOUT) {
                debug("UDP server socket timeout, continuing...");
                continue;
            } else if (err == PTK_ERR_ABORT) {
                info("UDP server socket aborted");
                break;
            } else {
                error("UDP server recv failed with error: %d", err);
                break;
            }
        }
        
        // Get message content
        const char *msg = (const char *)(received->data + received->start);
        size_t msg_len = ptk_buf_get_len(received);
        
        char sender_ip[16];
        struct in_addr addr;
        addr.s_addr = sender_addr.ip;
        strcpy(sender_ip, inet_ntoa(addr));
        
        info("UDP server received from %s:%d: %.*s", sender_ip, sender_addr.port, (int)msg_len, msg);
        
        // Create response with counter
        int counter = 0;
        ctx = ptk_shared_acquire(ctx_handle);
        if (ctx) {
            ctx->total_messages++;
            counter = ctx->total_messages;
            ptk_shared_release(ctx_handle);
        } else {
            error("Failed to access UDP server context for counter");
            ptk_local_free(&received);
            break;
        }
        
        // Create echo response with counter
        char response[256];
        snprintf(response, sizeof(response), "UDP Echo #%d: %.*s", counter, (int)msg_len, msg);
        
        ptk_buf *response_buf = ptk_buf_alloc_from_data((const uint8_t *)response, strlen(response));
        if (!response_buf) {
            error("Failed to create UDP response buffer");
            ptk_local_free(&received);
            break;
        }
        
        // Send response back to sender
        ptk_err send_err = ptk_udp_socket_send_to(socket, response_buf, &sender_addr, false, 5000);
        if (send_err != PTK_OK) {
            error("UDP server send failed with error: %d", send_err);
            ptk_local_free(&response_buf);
            ptk_local_free(&received);
            break;
        }
        
        info("UDP server sent to %s:%d: %s", sender_ip, sender_addr.port, response);
        
        // Clean up
        ptk_local_free(&response_buf);
        ptk_local_free(&received);
    }
    
    info("UDP server thread finished");
}

// UDP Client thread function
void udp_client_thread_func(ptk_sock *socket, ptk_shared_handle_t ctx_handle) {
    udp_client_context_t *ctx = ptk_shared_acquire(ctx_handle);
    if (ctx) {
        info("UDP Client %d thread started", ctx->client_id);
        
        // Server address
        ptk_address_t server_addr;
        ptk_address_init(&server_addr, "127.0.0.1", SERVER_PORT);
        
        for (int i = 0; i < MESSAGES_PER_CLIENT; i++) {
            // Create message
            char message[64];
            snprintf(message, sizeof(message), "Hello from UDP client %d, message %d", ctx->client_id, i + 1);
            
            ptk_buf *msg_buf = ptk_buf_alloc_from_data((const uint8_t *)message, strlen(message));
            if (!msg_buf) {
                error("UDP Client %d: Failed to create message buffer", ctx->client_id);
                break;
            }
            
            // Send message to server
            ptk_err send_err = ptk_udp_socket_send_to(socket, msg_buf, &server_addr, false, 5000);
            if (send_err != PTK_OK) {
                error("UDP Client %d: Send failed with error: %d", ctx->client_id, send_err);
                ptk_local_free(&msg_buf);
                break;
            }
            
            ctx->messages_sent++;
            info("UDP Client %d sent: %s", ctx->client_id, message);
            ptk_local_free(&msg_buf);
            
            // Receive echo response
            ptk_address_t sender_addr;
            ptk_buf *response = ptk_udp_socket_recv_from(socket, &sender_addr, 5000);
            if (!response) {
                error("UDP Client %d: Recv failed with error: %d", ctx->client_id, ptk_get_err());
                break;
            }
            
            const char *resp_data = (const char *)(response->data + response->start);
            size_t resp_len = ptk_buf_get_len(response);
            
            ctx->messages_received++;
            info("UDP Client %d received: %.*s", ctx->client_id, (int)resp_len, resp_data);
            
            ptk_local_free(&response);
            
            // Small delay between messages
            usleep(100000); // 100ms
        }
        
        info("UDP Client %d finished: sent %d, received %d", 
             ctx->client_id, ctx->messages_sent, ctx->messages_received);
        ptk_shared_release(ctx_handle);
    } else {
        error("Failed to access UDP client context");
    }
}

// UDP Client starter thread
void udp_client_starter_thread(void *arg) {
    int client_id = *(int *)arg;
    
    // Give server time to start
    sleep(1);
    
    // Create client context
    udp_client_context_t *client_ctx = ptk_local_alloc(sizeof(udp_client_context_t), NULL);
    client_ctx->client_id = client_id;
    client_ctx->messages_sent = 0;
    client_ctx->messages_received = 0;
    
    ptk_shared_handle_t client_ctx_handle = ptk_shared_create(client_ctx);
    
    info("UDP Client %d connecting to server", client_id);
    
    // Create UDP client socket (no binding - let system assign port)
    ptk_sock *client_sock = ptk_udp_socket_create(NULL, false, udp_client_thread_func, client_ctx_handle);
    if (!client_sock) {
        error("UDP Client %d: Failed to create socket: %d", client_id, ptk_get_err());
        ptk_shared_release(client_ctx_handle);
        return;
    }
    
    info("UDP Client %d socket created", client_id);
    
    // Wait for client thread to finish
    sleep(5); // Give time for messages to complete
    
    // Close client socket
    ptk_socket_close(client_sock);
    ptk_shared_release(client_ctx_handle);
    
    info("UDP Client %d thread finished", client_id);
    return;
}

int main() {
    info("=== Starting PTK UDP Socket Test ===");
    
    // Initialize shared memory subsystem
    ptk_shared_init();
    
    // Create server context
    udp_server_context_t *server_ctx = ptk_local_alloc(sizeof(udp_server_context_t), NULL);
    server_ctx->message_counter = 0;
    server_ctx->total_clients = 0;
    server_ctx->total_messages = 0;
    server_ctx->should_stop = false;
    
    ptk_shared_handle_t server_ctx_handle = ptk_shared_create(server_ctx);
    
    // Start UDP server
    ptk_address_t server_addr;
    ptk_address_init_any(&server_addr, SERVER_PORT);
    
    info("Starting UDP server on port %d", SERVER_PORT);
    g_udp_server_socket = ptk_udp_socket_create(&server_addr, false, udp_server_thread_func, server_ctx_handle);
    if (!g_udp_server_socket) {
        error("Failed to start UDP server: %d", ptk_get_err());
        return 1;
    }
    
    info("UDP server started successfully");
    
    // Give server time to start
    sleep(1);
    
    // Start client threads
    ptk_thread *client_threads[NUM_CLIENTS];
    int client_ids[NUM_CLIENTS];
    
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_ids[i] = i + 1;
        client_threads[i] = ptk_thread_create(udp_client_starter_thread, &client_ids[i]);
        if (!client_threads[i]) {
            error("Failed to create UDP client thread %d", i + 1);
        }
    }
    
    // Wait for all clients to finish
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (client_threads[i]) {
            ptk_thread_join(client_threads[i]);
        }
    }
    
    info("All UDP clients finished");
    
    // Print final server stats
    udp_server_context_t *ctx = ptk_shared_acquire(server_ctx_handle);
    if (ctx) {
        info("=== UDP Test Summary ===");
        info("Total clients: %d", ctx->total_clients);
        info("Total messages processed: %d", ctx->total_messages);
        info("Expected messages: %d", NUM_CLIENTS * MESSAGES_PER_CLIENT);
        
        if (ctx->total_messages == NUM_CLIENTS * MESSAGES_PER_CLIENT) {
            info("✓ All UDP messages processed successfully!");
        } else {
            error("✗ UDP Message count mismatch!");
        }
        ptk_shared_release(server_ctx_handle);
    } else {
        error("Failed to access UDP server context for final stats");
    }
    
    // Stop the UDP server gracefully
    info("Stopping UDP server...");
    ptk_socket_close(g_udp_server_socket);
    g_udp_server_socket = NULL;
    
    info("=== UDP Test Complete Successfully! ===");
    
    // Clean up
    ptk_shared_release(server_ctx_handle);
    ptk_shared_shutdown();
    
    return 0;
}