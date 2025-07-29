/**
 * UDP Buffer Example for Protocol Toolkit macOS
 *
 * This example demonstrates the buffer-based UDP API including:
 * - Buffer creation and management
 * - UDP sendto/recvfrom with buffers
 * - Broadcast and multicast operations
 */

#include <stdio.h>
#include <string.h>
#include "protocol_toolkit.h"

// Declare event loop slots and resources
PTK_DECLARE_EVENT_LOOP_SLOTS(udp_event_loops, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(udp_resources, 0, 2, 0);

int main() {
    printf("Protocol Toolkit UDP Buffer Example\n");
    printf("====================================\n\n");

    // Create event loop
    ptk_handle_t event_loop = ptk_event_loop_create(udp_event_loops, 1, &udp_resources);
    if(event_loop < 0) {
        printf("❌ Failed to create event loop: %s\n", ptk_error_string((ptk_err_t)event_loop));
        return 1;
    }
    printf("✓ Created event loop (handle: %lld)\n", event_loop);

    // Create UDP sockets
    ptk_handle_t udp_socket1 = ptk_socket_create_udp(event_loop);
    ptk_handle_t udp_socket2 = ptk_socket_create_udp(event_loop);

    if(udp_socket1 < 0 || udp_socket2 < 0) {
        printf("❌ Failed to create UDP sockets\n");
        ptk_event_loop_destroy(event_loop);
        return 1;
    }
    printf("✓ Created UDP sockets: %lld, %lld\n", udp_socket1, udp_socket2);

    // Test buffer creation and usage
    uint8_t send_data[256];
    uint8_t recv_data[256];

    // Create send buffer with some test data
    const char *test_message = "Hello, UDP with buffers!";
    strcpy((char *)send_data, test_message);
    ptk_buffer_t send_buffer = ptk_buffer_create(send_data, sizeof(send_data));
    send_buffer.size = strlen(test_message) + 1;  // Include null terminator

    // Create receive buffer
    ptk_buffer_t recv_buffer = ptk_buffer_create(recv_data, sizeof(recv_data));

    printf("✓ Created buffers:\n");
    printf("  - Send buffer: capacity=%zu, size=%zu, data='%s'\n", send_buffer.capacity, send_buffer.size,
           (char *)send_buffer.data);
    printf("  - Recv buffer: capacity=%zu, size=%zu\n", recv_buffer.capacity, recv_buffer.size);

    // Test broadcast functionality
    printf("\n📡 Testing UDP broadcast functionality...\n");

    ptk_err_t result = ptk_socket_enable_broadcast(udp_socket1);
    if(result == PTK_ERR_OK) {
        printf("✓ Enabled broadcast on socket\n");

        // Try to send broadcast (will fail if not bound, but shows API works)
        ptk_buffer_t broadcast_buffer = ptk_buffer_create(send_data, sizeof(send_data));
        broadcast_buffer.size = send_buffer.size;
        result = ptk_socket_broadcast(udp_socket1, &broadcast_buffer, 8080);
        if(result == PTK_ERR_NOT_CONNECTED) {
            printf("⚠️  Broadcast failed as expected (socket not bound): %s\n", ptk_error_string(result));
        } else {
            printf("✓ Broadcast result: %s\n", ptk_error_string(result));
        }
    } else {
        printf("⚠️  Failed to enable broadcast: %s\n", ptk_error_string(result));
    }

    // Test multicast functionality
    printf("\n🌐 Testing UDP multicast functionality...\n");

    result = ptk_socket_join_multicast_group(udp_socket2, "224.0.0.1", NULL);
    if(result == PTK_ERR_NOT_CONNECTED) {
        printf("⚠️  Multicast join failed as expected (socket not bound): %s\n", ptk_error_string(result));
    } else {
        printf("✓ Multicast join result: %s\n", ptk_error_string(result));
    }

    result = ptk_socket_set_multicast_ttl(udp_socket2, 1);
    if(result == PTK_ERR_NOT_CONNECTED) {
        printf("⚠️  Set multicast TTL failed as expected (socket not bound): %s\n", ptk_error_string(result));
    } else {
        printf("✓ Set multicast TTL result: %s\n", ptk_error_string(result));
    }

    result = ptk_socket_set_multicast_loopback(udp_socket2, true);
    if(result == PTK_ERR_NOT_CONNECTED) {
        printf("⚠️  Set multicast loopback failed as expected (socket not bound): %s\n", ptk_error_string(result));
    } else {
        printf("✓ Set multicast loopback result: %s\n", ptk_error_string(result));
    }

    // Test sendto functionality
    printf("\n📤 Testing UDP sendto with buffers...\n");

    ptk_buffer_t sendto_buffer = ptk_buffer_create(send_data, sizeof(send_data));
    sendto_buffer.size = send_buffer.size;
    result = ptk_socket_sendto(udp_socket1, &sendto_buffer, "127.0.0.1", 8080);
    if(result == PTK_ERR_NOT_CONNECTED) {
        printf("⚠️  Sendto failed as expected (socket not bound): %s\n", ptk_error_string(result));
    } else {
        printf("✓ Sendto result: %s, bytes in buffer: %zu\n", ptk_error_string(result), sendto_buffer.size);
    }

    // Test recvfrom functionality
    printf("\n📥 Testing UDP recvfrom with buffers...\n");

    char sender_addr[64];
    uint16_t sender_port;
    result = ptk_socket_recvfrom(udp_socket2, &recv_buffer, sender_addr, sizeof(sender_addr), &sender_port);
    if(result == PTK_ERR_NOT_CONNECTED || result == PTK_ERR_WOULD_BLOCK) {
        printf("⚠️  Recvfrom failed as expected (no data or not bound): %s\n", ptk_error_string(result));
    } else {
        printf("✓ Recvfrom result: %s, bytes received: %zu\n", ptk_error_string(result), recv_buffer.size);
        if(recv_buffer.size > 0) { printf("  Data: '%s', from: %s:%u\n", (char *)recv_buffer.data, sender_addr, sender_port); }
    }

    // Test resource type checking for UDP sockets
    printf("\n🔍 Testing handle validation...\n");
    printf("✓ Socket types: UDP1=%d, UDP2=%d (expected: %d)\n", PTK_HANDLE_TYPE(udp_socket1), PTK_HANDLE_TYPE(udp_socket2),
           PTK_TYPE_SOCKET);

    // Cleanup
    ptk_socket_destroy(udp_socket1);
    ptk_socket_destroy(udp_socket2);
    ptk_event_loop_destroy(event_loop);
    printf("✓ Cleaned up resources\n");

    printf("\n🎉 UDP buffer example completed successfully!\n");
    printf("Note: This example demonstrates the buffer-based UDP API.\n");
    printf("Socket operations that require binding will show expected failures.\n");
    printf("The API structure is working correctly for buffer management.\n");

    return 0;
}
