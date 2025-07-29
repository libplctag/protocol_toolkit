/**
 * @file simple_tcp_protothread.c
 * @brief Simple TCP client using protothread convenience macros
 *
 * This demonstrates the clean, synchronous-looking code that protothreads enable:
 *
 * 1. Connect to server
 * 2. Send request
 * 3. Receive response
 *
 * All with automatic event handling and resumption!
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __APPLE__
#    include "../include/macos/protocol_toolkit.h"
#elif __linux__
#    include "../lib/linux/protocol_toolkit.h"
#else
#    include "../include/protocol_toolkit.h"
#endif

// Application context with protothread as first field
typedef struct {
    ptk_pt_t pt; /**< Must be first field for embedded pattern */
    ptk_handle_t socket;
    ptk_buffer_t send_buf;
    ptk_buffer_t recv_buf;
    uint8_t send_data[256];
    uint8_t recv_data[1024];
} app_context_t;

/**
 * @brief The protothread function - looks like blocking code!
 */
void my_tcp_client(ptk_pt_t *pt) {
    // Cast pt to get full app context - this is the embedded pattern!
    app_context_t *ctx = (app_context_t *)pt;

    PT_BEGIN(pt);

    printf("1. Connecting to server...\n");
    PTK_PT_TCP_CONNECT(pt, ctx->socket, "httpbin.org", 80);
    printf("   ✓ Connected!\n\n");

    printf("2. Sending HTTP request...\n");
    strcpy((char *)ctx->send_buf.data, "GET /get HTTP/1.1\r\nHost: httpbin.org\r\n\r\n");
    ctx->send_buf.size = strlen((char *)ctx->send_buf.data);
    PTK_PT_TCP_SEND(pt, ctx->socket, &ctx->send_buf);
    printf("   ✓ Sent %zu bytes\n\n", ctx->send_buf.size);

    printf("3. Receiving response...\n");
    PTK_PT_TCP_RECEIVE(pt, ctx->socket, &ctx->recv_buf);
    printf("   ✓ Received %zu bytes\n", ctx->recv_buf.size);
    printf("   Response starts with: %.80s...\n\n", ctx->recv_buf.data);

    printf("4. Done! Closing connection.\n");
    ptk_socket_close(ctx->socket);

    PT_END(pt);
}

int main() {
    printf("Simple TCP Protothread Example\n");
    printf("==============================\n\n");

    // Setup resources
    PTK_DECLARE_EVENT_LOOP_SLOTS(loops, 1);
    PTK_DECLARE_EVENT_LOOP_RESOURCES(resources, 1, 2, 1);

    ptk_handle_t event_loop = ptk_event_loop_create(loops, 1, &resources);
    ptk_handle_t socket = ptk_socket_create_tcp(event_loop);

    // Setup context with embedded protothread
    app_context_t ctx = {.socket = socket,
                         .send_buf = ptk_buffer_create(ctx.send_data, sizeof(ctx.send_data)),
                         .recv_buf = ptk_buffer_create(ctx.recv_data, sizeof(ctx.recv_data))};

    // Initialize the embedded protothread
    ptk_protothread_init(&ctx.pt, my_tcp_client);

    // Start it running
    ptk_protothread_run(&ctx.pt);

    // Run event loop (in real app, this would be your main loop)
    printf("Running event loop...\n\n");
    for(int i = 0; i < 50; i++) {
        ptk_event_loop_run(event_loop);
        usleep(100000);  // 100ms delay
    }

    printf("\nExample complete!\n");
    return 0;
}

/*
 * Key Points About This Design:
 *
 * 1. The protothread function looks like normal, sequential code
 * 2. Each PTK_PT_* macro handles the async event setup automatically
 * 3. When an event occurs, the dispatcher:
 *    - Removes the protothread from the event handler
 *    - Calls ptk_protothread_run() which resumes from the saved line
 * 4. The protothread can set itself up for the next event or just finish
 * 5. No return values needed - execution just flows naturally
 */
