/**
 * @file linux_example.c
 * @brief Example demonstrating Linux-specific Protocol Toolkit usage
 *
 * This shows how to use the Linux implementation with the same API
 * as macOS, but with Linux-specific optimizations under the hood.
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

/* ========================================================================
 * APPLICATION CONTEXT WITH EMBEDDED PROTOTHREAD
 * ======================================================================== */

typedef struct {
    ptk_pt_t pt; /**< MUST be first field! */

    /* Application-specific data */
    ptk_handle_t event_loop;
    ptk_handle_t tcp_socket;
    ptk_handle_t timer;

    ptk_buffer_t send_buf;
    ptk_buffer_t recv_buf;
    uint8_t send_data[512];
    uint8_t recv_data[2048];

    bool done;
} linux_app_context_t;

/* ========================================================================
 * PROTOTHREAD FUNCTIONS
 * ======================================================================== */

void linux_app_protothread(ptk_pt_t *pt) {
    // Cast pt to get the full context - embedded pattern!
    linux_app_context_t *app = (linux_app_context_t *)pt;

    PT_BEGIN(pt);

    printf("🐧 Linux Protocol Toolkit Example\n");
    printf("==================================\n\n");

    // Connect to server
    printf("📡 Connecting to httpbin.org:80...\n");
    PTK_PT_TCP_CONNECT(pt, app->tcp_socket, "httpbin.org", 80);
    printf("✅ Connected using Linux epoll!\n\n");

    // Send HTTP request
    printf("📤 Sending HTTP request...\n");
    const char *request = "GET /json HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    strcpy((char *)app->send_buf.data, request);
    app->send_buf.size = strlen(request);
    PTK_PT_TCP_SEND(pt, app->tcp_socket, &app->send_buf);
    printf("✅ Request sent (%zu bytes)\n\n", app->send_buf.size);

    // Receive response
    printf("📥 Waiting for response...\n");
    PTK_PT_TCP_RECEIVE(pt, app->tcp_socket, &app->recv_buf);
    printf("✅ Response received (%zu bytes)\n", app->recv_buf.size);
    printf("📄 First 200 chars: %.200s...\n\n", app->recv_buf.data);

    // Wait a bit before closing
    printf("⏰ Waiting 2 seconds (Linux timerfd)...\n");
    PTK_PT_SLEEP_MS(pt, app->timer, 2000);

    // Cleanup
    printf("🧹 Cleaning up...\n");
    ptk_socket_close(app->tcp_socket);
    app->done = true;

    printf("🎉 Linux example complete!\n");

    PT_END(pt);
}

/* ========================================================================
 * MAIN APPLICATION
 * ======================================================================== */

int main() {
    printf("Protocol Toolkit - Linux Implementation\n");
    printf("=======================================\n\n");

    // Setup protocol toolkit resources
    PTK_DECLARE_EVENT_LOOP_SLOTS(loops, 1);
    PTK_DECLARE_EVENT_LOOP_RESOURCES(resources, 2, 4, 2);

    ptk_handle_t event_loop = ptk_event_loop_create(loops, 1, &resources);
    if(event_loop == 0) {
        printf("❌ Failed to create event loop\n");
        return 1;
    }

    // Initialize application context with embedded protothread
    linux_app_context_t app = {0};
    app.event_loop = event_loop;
    app.tcp_socket = ptk_socket_create_tcp(event_loop);
    app.timer = ptk_timer_create(event_loop);
    app.send_buf = ptk_buffer_create(app.send_data, sizeof(app.send_data));
    app.recv_buf = ptk_buffer_create(app.recv_data, sizeof(app.recv_data));
    app.done = false;

    if(app.tcp_socket == 0 || app.timer == 0) {
        printf("❌ Failed to create resources\n");
        ptk_event_loop_destroy(event_loop);
        return 1;
    }

    // Initialize the embedded protothread
    ptk_protothread_init(&app.pt, linux_app_protothread);

    // Start the application
    printf("🔄 Starting Linux protothread...\n\n");
    ptk_protothread_run(&app.pt);

    // Run event loop until done
    printf("🔄 Running Linux epoll event loop...\n");
    while(!app.done) {
        ptk_err_t result = ptk_event_loop_run(event_loop);
        if(result != PTK_ERR_OK && result != PTK_ERR_WOULD_BLOCK) {
            printf("❌ Event loop error: %s\n", ptk_error_string(result));
            break;
        }
        usleep(10000);  // 10ms
    }

    // Cleanup
    ptk_timer_destroy(app.timer);
    ptk_socket_destroy(app.tcp_socket);
    ptk_event_loop_destroy(event_loop);

    printf("\n✨ Linux example completed successfully!\n");
    return 0;
}

/*
 * Linux Implementation Highlights:
 *
 * 🔧 Under the Hood:
 * - epoll() for efficient event multiplexing
 * - timerfd_create() for high-resolution timers
 * - eventfd() for thread-safe user events
 * - pthread_mutex for thread safety
 * - Non-blocking BSD sockets
 *
 * 🚀 Performance Benefits:
 * - O(1) event notification with epoll
 * - No polling overhead
 * - Kernel-managed timer precision
 * - Efficient for high connection counts
 *
 * 🎯 Same API, Different Platform:
 * - Identical function signatures as macOS
 * - Same protothread macros work unchanged
 * - Zero source code changes needed
 * - Just recompile with Linux headers
 *
 * 💡 Linux-Specific Optimizations:
 * - EPOLLONESHOT for edge-triggered efficiency
 * - eventfd for minimal user event overhead
 * - timerfd for precise timer management
 * - Proper error mapping from errno
 */
