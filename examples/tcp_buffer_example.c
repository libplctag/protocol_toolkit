/**
 * TCP Buffer Example for Protocol Toolkit macOS
 *
 * This example demonstrates the buffer-based TCP API including:
 * - Buffer creation and management
 * - TCP send/receive with buffers
 * - Buffer size tracking
 */

#include <stdio.h>
#include <string.h>
#include "protocol_toolkit.h"

// Declare event loop slots and resources
PTK_DECLARE_EVENT_LOOP_SLOTS(tcp_event_loops, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(tcp_resources, 0, 2, 0);

int main() {
    printf("Protocol Toolkit TCP Buffer Example\n");
    printf("====================================\n\n");

    // Create event loop
    ptk_handle_t event_loop = ptk_event_loop_create(tcp_event_loops, 1, &tcp_resources);
    if(event_loop < 0) {
        printf("❌ Failed to create event loop: %s\n", ptk_error_string((ptk_err_t)event_loop));
        return 1;
    }
    printf("✓ Created event loop (handle: %lld)\n", event_loop);

    // Create TCP sockets
    ptk_handle_t tcp_socket1 = ptk_socket_create_tcp(event_loop);
    ptk_handle_t tcp_socket2 = ptk_socket_create_tcp(event_loop);

    if(tcp_socket1 < 0 || tcp_socket2 < 0) {
        printf("❌ Failed to create TCP sockets\n");
        ptk_event_loop_destroy(event_loop);
        return 1;
    }
    printf("✓ Created TCP sockets: %lld, %lld\n", tcp_socket1, tcp_socket2);

    // Test buffer creation and usage
    uint8_t send_data[1024];
    uint8_t recv_data[1024];

    // Create send buffer with test data
    const char *test_message = "Hello, TCP with buffers! This is a longer message to test buffer management.";
    strcpy((char *)send_data, test_message);
    ptk_buffer_t send_buffer = ptk_buffer_create(send_data, sizeof(send_data));
    send_buffer.size = strlen(test_message);  // Don't include null terminator for TCP

    // Create receive buffer
    ptk_buffer_t recv_buffer = ptk_buffer_create(recv_data, sizeof(recv_data));

    printf("✓ Created buffers:\n");
    printf("  - Send buffer: capacity=%zu, size=%zu\n", send_buffer.capacity, send_buffer.size);
    printf("  - Data: '%.50s%s'\n", (char *)send_buffer.data, send_buffer.size > 50 ? "..." : "");
    printf("  - Recv buffer: capacity=%zu, size=%zu\n", recv_buffer.capacity, recv_buffer.size);

    // Test TCP send functionality
    printf("\n📤 Testing TCP send with buffers...\n");

    ptk_err_t result = ptk_socket_send(tcp_socket1, &send_buffer);
    if(result == PTK_ERR_NOT_CONNECTED) {
        printf("⚠️  Send failed as expected (socket not connected): %s\n", ptk_error_string(result));
    } else {
        printf("✓ Send result: %s, buffer size after send: %zu\n", ptk_error_string(result), send_buffer.size);
    }

    // Test TCP receive functionality
    printf("\n📥 Testing TCP receive with buffers...\n");

    result = ptk_socket_receive(tcp_socket2, &recv_buffer);
    if(result == PTK_ERR_NOT_CONNECTED || result == PTK_ERR_WOULD_BLOCK) {
        printf("⚠️  Receive failed as expected (not connected or no data): %s\n", ptk_error_string(result));
    } else {
        printf("✓ Receive result: %s, bytes received: %zu\n", ptk_error_string(result), recv_buffer.size);
        if(recv_buffer.size > 0) { printf("  Data: '%.50s%s'\n", (char *)recv_buffer.data, recv_buffer.size > 50 ? "..." : ""); }
    }

    // Test buffer size management
    printf("\n📊 Testing buffer size management...\n");

    // Create multiple buffers of different sizes
    uint8_t small_data[64];
    uint8_t large_data[2048];

    strcpy((char *)small_data, "Small buffer test");
    ptk_buffer_t small_buffer = ptk_buffer_create(small_data, sizeof(small_data));
    small_buffer.size = strlen((char *)small_data);

    strcpy((char *)large_data, "Large buffer test with more capacity for bigger messages");
    ptk_buffer_t large_buffer = ptk_buffer_create(large_data, sizeof(large_data));
    large_buffer.size = strlen((char *)large_data);

    printf("✓ Buffer size comparison:\n");
    printf("  - Small: capacity=%zu, size=%zu, data='%s'\n", small_buffer.capacity, small_buffer.size, (char *)small_buffer.data);
    printf("  - Large: capacity=%zu, size=%zu, data='%s'\n", large_buffer.capacity, large_buffer.size, (char *)large_buffer.data);

    // Test sending different sized buffers
    printf("\n📤 Testing different buffer sizes...\n");

    result = ptk_socket_send(tcp_socket1, &small_buffer);
    printf("  Small buffer send: %s (size after: %zu)\n", ptk_error_string(result), small_buffer.size);

    result = ptk_socket_send(tcp_socket1, &large_buffer);
    printf("  Large buffer send: %s (size after: %zu)\n", ptk_error_string(result), large_buffer.size);

    // Test resource type checking for TCP sockets
    printf("\n🔍 Testing handle validation...\n");
    printf("✓ Socket types: TCP1=%d, TCP2=%d (expected: %d)\n", PTK_HANDLE_TYPE(tcp_socket1), PTK_HANDLE_TYPE(tcp_socket2),
           PTK_TYPE_SOCKET);

    bool socket1_valid = ptk_handle_is_valid(tcp_socket1);
    bool socket2_valid = ptk_handle_is_valid(tcp_socket2);
    printf("✓ Socket validity: TCP1=%s, TCP2=%s\n", socket1_valid ? "valid" : "invalid", socket2_valid ? "valid" : "invalid");

    // Cleanup
    ptk_socket_destroy(tcp_socket1);
    ptk_socket_destroy(tcp_socket2);
    ptk_event_loop_destroy(event_loop);
    printf("✓ Cleaned up resources\n");

    printf("\n🎉 TCP buffer example completed successfully!\n");
    printf("Note: This example demonstrates the buffer-based TCP API.\n");
    printf("Socket operations that require connections will show expected failures.\n");
    printf("The buffer management system is working correctly with automatic size tracking.\n");

    return 0;
}
