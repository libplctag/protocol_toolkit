/**
 * @file tcp_client_protothread_example.c
 * @brief Example of a TCP client using protothread convenience macros
 *
 * This example demonstrates:
 * - Creating a TCP socket
 * - Connecting to a remote server
 * - Sending a request
 * - Receiving a response
 * - All using protothread macros for clean, synchronous-looking code
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __APPLE__
#    include "../include/macos/protocol_toolkit.h"
#elif __linux__
#    include "../lib/linux/protocol_toolkit.h"
#else
#    include "../include/protocol_toolkit.h"
#endif

/* ========================================================================
 * APPLICATION DATA STRUCTURES
 * ======================================================================== */

/**
 * @brief Application context for the TCP client with embedded protothread
 */
typedef struct tcp_client_context {
    ptk_pt_t pt;                  /**< Must be first field for embedded pattern */
    ptk_handle_t event_loop;      /**< Event loop handle */
    ptk_handle_t tcp_socket;      /**< TCP socket handle */
    ptk_buffer_t request_buffer;  /**< Buffer for outgoing request */
    ptk_buffer_t response_buffer; /**< Buffer for incoming response */
    uint8_t request_data[256];    /**< Storage for request data */
    uint8_t response_data[1024];  /**< Storage for response data */
    bool connection_complete;     /**< Flag indicating completion */
} tcp_client_context_t;

/* ========================================================================
 * PROTOTHREAD IMPLEMENTATION
 * ======================================================================== */

/**
 * @brief TCP client protothread that connects, sends, and receives
 */
void tcp_client_protothread(ptk_pt_t *pt) {
    // Cast pt to get full app context - this is the embedded pattern!
    tcp_client_context_t *ctx = (tcp_client_context_t *)pt;

    PT_BEGIN(pt);

    printf("Starting TCP client connection to httpbin.org:80\n");

    // Step 1: Connect to the remote server
    printf("Connecting to server...\n");
    PTK_PT_TCP_CONNECT(pt, ctx->tcp_socket, "httpbin.org", 80);
    printf("Connected successfully!\n");

    // Step 2: Send HTTP GET request
    printf("Sending HTTP request...\n");
    const char *http_request = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    strncpy((char *)ctx->request_buffer.data, http_request, ctx->request_buffer.capacity - 1);
    ctx->request_buffer.size = strlen(http_request);

    PTK_PT_TCP_SEND(pt, ctx->tcp_socket, &ctx->request_buffer);
    printf("Request sent (%zu bytes)\n", ctx->request_buffer.size);

    // Step 3: Receive HTTP response
    printf("Waiting for response...\n");
    PTK_PT_TCP_RECEIVE(pt, ctx->tcp_socket, &ctx->response_buffer);
    printf("Response received (%zu bytes):\n", ctx->response_buffer.size);

    // Print the first part of the response (headers)
    ctx->response_buffer.data[ctx->response_buffer.size] = '\0';
    char *header_end = strstr((char *)ctx->response_buffer.data, "\r\n\r\n");
    if(header_end) {
        *header_end = '\0';
        printf("HTTP Headers:\n%s\n", ctx->response_buffer.data);
    } else {
        printf("Partial response:\n%.200s\n", ctx->response_buffer.data);
    }

    // Step 4: Clean up
    printf("Closing connection...\n");
    ptk_socket_close(ctx->tcp_socket);

    // Mark completion
    ctx->connection_complete = true;
    printf("TCP client protothread completed!\n");

    PT_END(pt);
}

/* ========================================================================
 * MAIN APPLICATION
 * ======================================================================== */

int main(void) {
    printf("Protocol Toolkit TCP Client Protothread Example\n");
    printf("===============================================\n\n");

    // Declare resource pools
    PTK_DECLARE_EVENT_LOOP_SLOTS(event_loops, 1);
    PTK_DECLARE_EVENT_LOOP_RESOURCES(resources, 2, 4, 2);

    // Create event loop
    ptk_handle_t event_loop = ptk_event_loop_create(event_loops, 1, &resources);
    if(event_loop <= 0) {
        printf("Failed to create event loop\n");
        return 1;
    }

    // Initialize application context with embedded protothread
    tcp_client_context_t ctx = {0};
    ctx.event_loop = event_loop;
    ctx.request_buffer = ptk_buffer_create(ctx.request_data, sizeof(ctx.request_data));
    ctx.response_buffer = ptk_buffer_create(ctx.response_data, sizeof(ctx.response_data));
    ctx.connection_complete = false;

    // Create TCP socket
    ctx.tcp_socket = ptk_socket_create_tcp(event_loop);
    if(ctx.tcp_socket <= 0) {
        printf("Failed to create TCP socket\n");
        ptk_event_loop_destroy(event_loop);
        return 1;
    }

    // Initialize the embedded protothread
    ptk_protothread_init(&ctx.pt, tcp_client_protothread);

    // Start the protothread by calling it once
    printf("Starting TCP client protothread...\n\n");
    ptk_protothread_run(&ctx.pt);

    // Run event loop until completion
    printf("Running event loop...\n");
    int iterations = 0;
    const int max_iterations = 100;

    while(!ctx.connection_complete && iterations < max_iterations) {
        ptk_err_t result = ptk_event_loop_run(event_loop);
        if(result != PTK_ERR_OK) {
            printf("Event loop error: %s\n", ptk_error_string(result));
            break;
        }
        iterations++;

        // Small delay to prevent busy waiting
        usleep(100000);  // 100ms
    }

    if(iterations >= max_iterations) { printf("Event loop reached maximum iterations\n"); }

    // Cleanup
    printf("\nCleaning up...\n");
    ptk_socket_destroy(ctx.tcp_socket);
    ptk_event_loop_destroy(event_loop);

    printf("Example completed successfully!\n");
    return 0;
}
