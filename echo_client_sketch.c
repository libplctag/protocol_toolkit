/**
 * @file echo_client_sketch.c
 * @brief Echo client that connects to an echo server and sends test messages
 * 
 * This client connects to an echo server, sends a series of messages,
 * and verifies that the echoed responses match what was sent.
 */

#include <ptk_sock.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <ptk_alloc.h>
#include <ptk_utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// GLOBAL STATE
//=============================================================================

static volatile bool client_running = true;

//=============================================================================
// INTERRUPT HANDLER
//=============================================================================

/**
 * @brief Cross-platform interrupt handler for graceful shutdown
 */
void interrupt_handler(void) {
    info("Received interrupt signal, shutting down client");
    client_running = false;
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Compare two buffers for equality using the byte access API
 * @param buf1 First buffer
 * @param buf2 Second buffer
 * @return true if buffers contain identical data, false otherwise
 */
bool buffers_equal(ptk_buf *buf1, ptk_buf *buf2) {
    if (!buf1 || !buf2) {
        return false;
    }
    
    size_t len1 = ptk_buf_get_len(buf1);
    size_t len2 = ptk_buf_get_len(buf2);
    
    if (len1 != len2) {
        return false;
    }
    
    if (len1 == 0) {
        return true; // Both empty
    }
    
    // Save original positions
    buf_size_t orig_start1 = ptk_buf_get_start(buf1);
    buf_size_t orig_start2 = ptk_buf_get_start(buf2);
    
    // Reset to beginning for comparison
    ptk_buf_set_start(buf1, 0);
    ptk_buf_set_start(buf2, 0);
    
    // Compare byte by byte using the new API
    bool equal = true;
    for (size_t i = 0; i < len1 && equal; i++) {
        u8 byte1 = ptk_buf_get_u8(buf1);
        u8 byte2 = ptk_buf_get_u8(buf2);
        
        // Check for errors in reading
        if (ptk_get_err() != PTK_OK) {
            equal = false;
            break;
        }
        
        if (byte1 != byte2) {
            equal = false;
        }
    }
    
    // Restore original positions
    ptk_buf_set_start(buf1, orig_start1);
    ptk_buf_set_start(buf2, orig_start2);
    
    return equal;
}

//=============================================================================
// MAIN CLIENT FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    ptk_sock *client_sock = NULL;
    ptk_address_t server_addr;
    ptk_buf *send_buffer = NULL;
    ptk_buf *recv_buffer = NULL;
    ptk_err err;
    const char *host = "127.0.0.1";
    uint16_t port = 8080;
    int message_count = 10;
    
    // Parse command line arguments
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = (uint16_t)atoi(argv[2]);
        if (port == 0) {
            fprintf(stderr, "Invalid port number: %s\n", argv[2]);
            return 1;
        }
    }
    if (argc > 3) {
        message_count = atoi(argv[3]);
        if (message_count <= 0) {
            fprintf(stderr, "Invalid message count: %s\n", argv[3]);
            return 1;
        }
    }
    
    // Set log level
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    // Set up cross-platform interrupt handler
    err = ptk_set_interrupt_handler(interrupt_handler);
    if (err != PTK_OK) {
        error("Failed to set interrupt handler: %s", ptk_err_to_string(err));
        return 1;
    }
    
    info("Echo client connecting to %s:%u", host, port);
    
    // Create server address
    err = ptk_address_create(&server_addr, host, port);
    if (err != PTK_OK) {
        error("Failed to create server address: %s", ptk_err_to_string(err));
        return 1;
    }
    
    // Allocate buffers
    send_buffer = ptk_buf_alloc(1024);
    recv_buffer = ptk_buf_alloc(1024);
    if (!send_buffer || !recv_buffer) {
        error("Failed to allocate buffers: %s", ptk_err_to_string(ptk_get_err()));
        goto cleanup;
    }
    
    // Connect to server - ptk_tcp_socket_connect creates the socket and connects
    client_sock = ptk_tcp_socket_connect(&server_addr, 10000); // 10 second timeout
    if (!client_sock) {
        error("Failed to connect to server: %s", ptk_err_to_string(ptk_get_err()));
        goto cleanup;
    }
    
    info("Connected to echo server at %s:%u", host, port);
    
    // Send and receive messages
    int successful_echoes = 0;
    
    for (int i = 1; i <= message_count && client_running; i++) {
        char message[256];
        int msg_len;
        
        // Create test message
        msg_len = snprintf(message, sizeof(message), "Echo test message #%d: Hello from PTK client!", i);
        
        // Reset send buffer and copy message
        ptk_buf_set_start(send_buffer, 0);
        ptk_buf_set_end(send_buffer, 0);
        
        // Copy message to buffer using the byte access API
        if (ptk_buf_get_capacity(send_buffer) < (size_t)msg_len) {
            error("Message too large for send buffer");
            break;
        }
        
        // Copy message byte by byte using the API
        for (int j = 0; j < msg_len; j++) {
            err = ptk_buf_set_u8(send_buffer, (u8)message[j]);
            if (err != PTK_OK) {
                error("Failed to write to send buffer: %s", ptk_err_to_string(err));
                goto cleanup;
            }
        }
        
        info("Sending message %d: \"%s\"", i, message);
        debug_buf(send_buffer);
        
        // Send message
        err = ptk_tcp_socket_send(client_sock, send_buffer, 5000); // 5 second timeout
        if (err != PTK_OK) {
            error("Failed to send message %d: %s", i, ptk_err_to_string(err));
            break;
        }
        
        // Reset receive buffer
        ptk_buf_set_start(recv_buffer, 0);
        ptk_buf_set_end(recv_buffer, 0);
        
        // Receive echo response
        err = ptk_tcp_socket_recv(client_sock, recv_buffer, 5000); // 5 second timeout
        if (err != PTK_OK) {
            error("Failed to receive echo for message %d: %s", i, ptk_err_to_string(err));
            break;
        }
        
        debug("Received echo response for message %d", i);
        debug_buf(recv_buffer);
        
        // Verify echo matches what we sent
        if (buffers_equal(send_buffer, recv_buffer)) {
            info("✓ Message %d echoed correctly", i);
            successful_echoes++;
        } else {
            warn("✗ Message %d echo mismatch!", i);
            warn("Sent:     %zu bytes", ptk_buf_get_len(send_buffer));
            warn("Received: %zu bytes", ptk_buf_get_len(recv_buffer));
        }
        
        // Brief pause between messages using socket wait
        ptk_socket_wait(client_sock, 100); // 100ms pause
    }
    
    // Summary
    info("Echo test complete: %d/%d messages echoed successfully", 
         successful_echoes, message_count);
    
    if (successful_echoes == message_count) {
        info("✓ All echo tests passed!");
    } else {
        warn("✗ Some echo tests failed");
    }
    
cleanup:
    // Clean up resources - ptk_free() calls destructors automatically
    if (send_buffer) {
        ptk_free(send_buffer);
    }
    
    if (recv_buffer) {
        ptk_free(recv_buffer);
    }
    
    if (client_sock) {
        ptk_free(client_sock); // Destructor calls ptk_socket_close() internally
    }
    
    info("Echo client shutdown complete");
    return (successful_echoes == message_count) ? 0 : 1;
}