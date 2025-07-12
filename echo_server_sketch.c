/**
 * @file echo_server_sketch.c
 * @brief Multi-threaded echo server using threadlets
 * 
 * This server creates a threadlet for each connecting client.
 * Each client threadlet handles echo requests independently.
 */

#include <ptk_threadlet.h>
#include <ptk_sock.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <ptk_alloc.h>
#include <ptk_utils.h>
#include <stdio.h>
#include <stdlib.h>

//=============================================================================
// GLOBAL STATE
//=============================================================================

static volatile bool server_running = true;

//=============================================================================
// INTERRUPT HANDLER
//=============================================================================

/**
 * @brief Cross-platform interrupt handler for graceful shutdown
 */
void interrupt_handler(void) {
    info("Received interrupt signal, shutting down server");
    server_running = false;
}

//=============================================================================
// CLIENT HANDLER THREADLET
//=============================================================================

/**
 * @brief Threadlet function to handle one client connection
 * @param param Pointer to client socket (ptk_sock *)
 */
void client_handler_threadlet(void *param) {
    ptk_sock *client_sock = (ptk_sock *)param;
    ptk_buf *recv_buffer = NULL;
    ptk_err err;
    
    info("Client handler threadlet started");
    
    // Allocate receive buffer
    recv_buffer = ptk_buf_alloc(1024);
    if (!recv_buffer) {
        error("Failed to allocate receive buffer: %s", ptk_err_to_string(ptk_get_err()));
        goto cleanup;
    }
    
    // Echo loop - continue until client disconnects or error
    while (server_running) {
        // Reset buffer for next message
        ptk_buf_set_start(recv_buffer, 0);
        ptk_buf_set_end(recv_buffer, 0);
        
        // Receive data from client (blocking, with timeout)
        err = ptk_tcp_socket_recv(client_sock, recv_buffer, 5000); // 5 second timeout
        
        if (err == PTK_ERR_TIMEOUT) {
            // Timeout is normal, continue loop
            continue;
        } else if (err == PTK_ERR_CLOSED) {
            info("Client disconnected");
            break;
        } else if (err != PTK_OK) {
            error("Failed to receive from client: %s", ptk_err_to_string(err));
            break;
        }
        
        // Check if we received any data
        if (ptk_buf_get_len(recv_buffer) == 0) {
            continue;
        }
        
        debug("Received %zu bytes from client", ptk_buf_get_len(recv_buffer));
        debug_buf(recv_buffer);
        
        // Echo the data back to client
        err = ptk_tcp_socket_send(client_sock, recv_buffer, 5000); // 5 second timeout
        
        if (err == PTK_ERR_CLOSED) {
            info("Client disconnected during send");
            break;
        } else if (err != PTK_OK) {
            error("Failed to send to client: %s", ptk_err_to_string(err));
            break;
        }
        
        debug("Echoed %zu bytes back to client", ptk_buf_get_len(recv_buffer));
    }
    
cleanup:
    // Clean up resources - ptk_free() calls destructors automatically
    if (recv_buffer) {
        ptk_free(recv_buffer);
    }
    
    if (client_sock) {
        ptk_free(client_sock); // Destructor calls ptk_socket_close() internally
    }
    
    info("Client handler threadlet exiting");
}

//=============================================================================
// MAIN SERVER LOOP
//=============================================================================

int main(int argc, char *argv[]) {
    ptk_sock *server_sock = NULL;
    ptk_address_t server_addr;
    ptk_err err;
    uint16_t port = 8080;
    
    // Parse command line arguments
    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
        if (port == 0) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            return 1;
        }
    }
    
    // Set log level for debugging
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    // Set up cross-platform interrupt handler for graceful shutdown
    err = ptk_set_interrupt_handler(interrupt_handler);
    if (err != PTK_OK) {
        error("Failed to set interrupt handler: %s", ptk_err_to_string(err));
        return 1;
    }
    
    info("Starting echo server on port %u", port);
    
    // Create server address (listen on all interfaces)
    err = ptk_address_create_any(&server_addr, port);
    if (err != PTK_OK) {
        error("Failed to create server address: %s", ptk_err_to_string(err));
        return 1;
    }
    
    // Create and bind listening socket
    server_sock = ptk_tcp_socket_listen(&server_addr, 10); // backlog of 10
    if (!server_sock) {
        error("Failed to create listening socket: %s", ptk_err_to_string(ptk_get_err()));
        return 1;
    }
    
    info("Echo server listening on port %u", port);
    
    // Main accept loop
    while (server_running) {
        ptk_sock *client_sock = NULL;
        threadlet_t *client_threadlet = NULL;
        
        debug("Waiting for client connection...");
        
        // Accept new client connection (blocking, with timeout)
        err = ptk_tcp_socket_accept(server_sock, &client_sock, 1000); // 1 second timeout
        
        if (err == PTK_ERR_TIMEOUT) {
            // Timeout is normal, check if we should continue
            continue;
        } else if (err != PTK_OK) {
            if (server_running) {
                error("Failed to accept client: %s", ptk_err_to_string(err));
            }
            break;
        }
        
        if (!client_sock) {
            error("Accept succeeded but client socket is NULL");
            continue;
        }
        
        info("New client connected");
        
        // Create threadlet to handle this client
        client_threadlet = ptk_threadlet_create(client_handler_threadlet, client_sock);
        if (!client_threadlet) {
            error("Failed to create client threadlet: %s", ptk_err_to_string(ptk_get_err()));
            ptk_free(client_sock); // Clean up client socket
            continue;
        }
        
        // Start the client threadlet
        err = ptk_threadlet_resume(client_threadlet);
        if (err != PTK_OK) {
            error("Failed to start client threadlet: %s", ptk_err_to_string(err));
            ptk_free(client_threadlet); // Clean up threadlet
            ptk_free(client_sock);      // Clean up client socket
            continue;
        }
        
        // Note: We don't wait for the threadlet to complete here.
        // Each client threadlet runs independently and cleans up after itself.
        // In a real implementation, you might want to track threadlets for 
        // graceful shutdown.
        
        debug("Client threadlet started successfully");
    }
    
    // Cleanup
    info("Shutting down echo server");
    
    if (server_sock) {
        ptk_free(server_sock); // Destructor calls ptk_socket_close() internally
    }
    
    // Note: In a production server, you would want to:
    // 1. Keep track of all active client threadlets
    // 2. Signal them to shutdown gracefully  
    // 3. Wait for them to complete with timeouts
    // 4. Force cleanup any remaining resources
    
    info("Echo server shutdown complete");
    return 0;
}