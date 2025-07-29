/**
 * @file embedded_pattern_example.c
 * @brief Demonstrates the proper embedded protothread pattern
 *
 * This shows the intended design where:
 * 1. ptk_pt_t is the FIRST field in the application context
 * 2. Protothread functions cast pt to get the full context
 * 3. No need for associated_resource or user_data lookups
 *
 * This is the clean, embedded systems approach!
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

/**
 * @brief Application context with embedded protothread
 *
 * CRITICAL: ptk_pt_t MUST be the first field for the casting to work!
 */
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

    int state;
    bool done;
} my_app_context_t;

/* ========================================================================
 * PROTOTHREAD FUNCTIONS
 * ======================================================================== */

/**
 * @brief Main application protothread
 */
void my_app_protothread(ptk_pt_t *pt) {
    // This is the magic! Cast pt to get the full context
    my_app_context_t *app = (my_app_context_t *)pt;

    PT_BEGIN(pt);

    printf("🚀 Starting application...\n");

    // Connect to server
    printf("📡 Connecting to httpbin.org:80...\n");
    PTK_PT_TCP_CONNECT(pt, app->tcp_socket, "httpbin.org", 80);
    printf("✅ Connected!\n\n");

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
    printf("⏰ Waiting 2 seconds before cleanup...\n");
    PTK_PT_SLEEP_MS(pt, app->timer, 2000);

    // Cleanup
    printf("🧹 Cleaning up...\n");
    ptk_socket_close(app->tcp_socket);
    app->done = true;

    printf("🎉 Application complete!\n");

    PT_END(pt);
}

/* ========================================================================
 * MAIN APPLICATION
 * ======================================================================== */

int main() {
    printf("Embedded Protothread Pattern Example\n");
    printf("====================================\n\n");

    // Setup protocol toolkit resources
    PTK_DECLARE_EVENT_LOOP_SLOTS(loops, 1);
    PTK_DECLARE_EVENT_LOOP_RESOURCES(resources, 2, 4, 2);

    ptk_handle_t event_loop = ptk_event_loop_create(loops, 1, &resources);

    // Initialize application context with embedded protothread
    my_app_context_t app = {0};
    app.event_loop = event_loop;
    app.tcp_socket = ptk_socket_create_tcp(event_loop);
    app.timer = ptk_timer_create(event_loop);
    app.send_buf = ptk_buffer_create(app.send_data, sizeof(app.send_data));
    app.recv_buf = ptk_buffer_create(app.recv_data, sizeof(app.recv_data));
    app.done = false;

    // Initialize the embedded protothread
    // Note: we pass &app.pt (the protothread field) and my_app_protothread
    ptk_protothread_init(&app.pt, my_app_protothread);

    // Start the application
    printf("🔄 Starting protothread...\n\n");
    ptk_protothread_run(&app.pt);

    // Run event loop until done
    printf("🔄 Running event loop...\n");
    while(!app.done) {
        ptk_event_loop_run(event_loop);
        usleep(50000);  // 50ms
    }

    printf("\n✨ Example completed successfully!\n");
    return 0;
}

/*
 * Key Benefits of This Pattern:
 *
 * 1. 🎯 Clean Design: No need for user_data or associated_resource lookups
 * 2. 🚀 Performance: Direct pointer cast, no indirection
 * 3. 💡 Type Safety: Compiler knows the exact context type
 * 4. 🔧 Embedded Friendly: No dynamic allocation, everything static
 * 5. 📖 Readable: Clear relationship between protothread and context
 *
 * The pattern works because:
 * - ptk_pt_t is the first field in my_app_context_t
 * - C guarantees first field has same address as struct
 * - So (my_app_context_t*)pt == (my_app_context_t*)&app.pt
 * - This gives us the full context from just the pt pointer!
 */
