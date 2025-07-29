/**
 * @file protothread_macros_example.c
 * @brief Example showing usage of protothread convenience macros
 *
 * This example demonstrates how to use the convenience macros for
 * TCP and UDP socket operations with protothreads.
 */

#include "../src/include/protocol_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Example protothread structures with application data
typedef struct {
    ptk_pt_t pt;
    ptk_handle_t socket;
    ptk_buffer_t buffer;
    uint8_t data[256];
    char address[64];
    int port;
    int state;
} tcp_client_pt_t;

typedef struct {
    ptk_pt_t pt;
    ptk_handle_t socket;
    ptk_buffer_t send_buffer;
    ptk_buffer_t recv_buffer;
    uint8_t send_data[256];
    uint8_t recv_data[256];
    char remote_address[64];
    int remote_port;
    int packet_count;
} udp_echo_pt_t;

// Example TCP client protothread using convenience macros
void tcp_client_protothread(ptk_pt_t *pt) {
    tcp_client_pt_t *client = (tcp_client_pt_t *)pt;

    PTK_PT_BEGIN(&client->pt);

    printf("TCP Client: Starting connection...\n");

    // Connect to server
    ptk_tcp_connect(&client->pt, client->socket, "127.0.0.1", 8080);
    printf("TCP Client: Connected!\n");

    // Prepare message
    strcpy((char *)client->data, "Hello, TCP Server!");
    client->buffer.size = strlen((char *)client->data);

    // Send message
    ptk_tcp_send(&client->pt, client->socket, &client->buffer);
    printf("TCP Client: Message sent!\n");

    // Receive response
    client->buffer.size = 0;  // Reset for receive
    ptk_tcp_receive(&client->pt, client->socket, &client->buffer);
    client->data[client->buffer.size] = '\0';  // Null terminate
    printf("TCP Client: Received: %s\n", client->data);

    printf("TCP Client: Done!\n");

    PTK_PT_END(&client->pt);
}

// Example UDP echo protothread using convenience macros
void udp_echo_protothread(ptk_pt_t *pt) {
    udp_echo_pt_t *echo = (udp_echo_pt_t *)pt;

    PTK_PT_BEGIN(&echo->pt);

    printf("UDP Echo: Starting...\n");

    // Send initial message
    strcpy((char *)echo->send_data, "Hello, UDP World!");
    echo->send_buffer.size = strlen((char *)echo->send_data);
    strcpy(echo->remote_address, "127.0.0.1");
    echo->remote_port = 12345;

    ptk_udp_send(&echo->pt, echo->socket, &echo->send_buffer, echo->remote_address, echo->remote_port);
    printf("UDP Echo: Sent message to %s:%d\n", echo->remote_address, echo->remote_port);

    // Receive response
    ptk_udp_receive(&echo->pt, echo->socket, &echo->recv_buffer, echo->remote_address, &echo->remote_port);
    echo->recv_data[echo->recv_buffer.size] = '\0';  // Null terminate
    printf("UDP Echo: Received from %s:%d: %s\n", echo->remote_address, echo->remote_port, echo->recv_data);

    // Send broadcast message
    strcpy((char *)echo->send_data, "Broadcast message!");
    echo->send_buffer.size = strlen((char *)echo->send_data);

    ptk_udp_broadcast(&echo->pt, echo->socket, &echo->send_buffer, "255.255.255.255", 12346);
    printf("UDP Echo: Broadcast sent!\n");

    // Send multicast message
    strcpy((char *)echo->send_data, "Multicast message!");
    echo->send_buffer.size = strlen((char *)echo->send_data);

    ptk_udp_multicast_send(&echo->pt, echo->socket, &echo->send_buffer, "224.0.0.1");
    printf("UDP Echo: Multicast sent!\n");

    printf("UDP Echo: Done!\n");

    PTK_PT_END(&echo->pt);
}

// Example showing timer-based delays
void timer_delay_protothread(ptk_pt_t *pt) {
    static ptk_handle_t timer;
    static int count = 0;

    PTK_PT_BEGIN(pt);

    printf("Timer Delay: Starting...\n");

    timer = ptk_timer_create();

    while(count < 3) {
        printf("Timer Delay: Sleeping for 1 second... (%d/3)\n", count + 1);
        ptk_sleep_ms(pt, timer, 1000);
        count++;
        printf("Timer Delay: Woke up!\n");
    }

    ptk_handle_destroy(timer);
    printf("Timer Delay: Done!\n");

    PTK_PT_END(pt);
}

int main() {
    printf("=== Protothread Convenience Macros Example ===\n\n");
    printf("This example shows the usage patterns for the convenience macros.\n");
    printf("Note: This is a demonstration of syntax - actual execution would\n");
    printf("require proper event loop setup and socket initialization.\n\n");

    // Example initialization patterns
    tcp_client_pt_t tcp_client;
    udp_echo_pt_t udp_echo;
    ptk_pt_t timer_pt;

    // Initialize protothread structures
    memset(&tcp_client, 0, sizeof(tcp_client));
    memset(&udp_echo, 0, sizeof(udp_echo));
    memset(&timer_pt, 0, sizeof(timer_pt));

    // Initialize buffers
    tcp_client.buffer = ptk_buffer_create(tcp_client.data, sizeof(tcp_client.data));
    udp_echo.send_buffer = ptk_buffer_create(udp_echo.send_data, sizeof(udp_echo.send_data));
    udp_echo.recv_buffer = ptk_buffer_create(udp_echo.recv_data, sizeof(udp_echo.recv_data));

    printf("Convenience macros provide simplified syntax for:\n");
    printf("- ptk_tcp_connect(pt, socket, address, port)\n");
    printf("- ptk_tcp_send(pt, socket, buffer)\n");
    printf("- ptk_tcp_receive(pt, socket, buffer)\n");
    printf("- ptk_udp_send(pt, socket, buffer, address, port)\n");
    printf("- ptk_udp_receive(pt, socket, buffer, address, port)\n");
    printf("- ptk_udp_broadcast(pt, socket, buffer, address, port)\n");
    printf("- ptk_udp_multicast_send(pt, socket, buffer, group_address)\n");
    printf("- ptk_sleep_ms(pt, timer, delay_ms)\n");

    printf("\nThese macros automatically handle:\n");
    printf("- Event waiting and continuation\n");
    printf("- Protothread state management\n");
    printf("- Error handling integration\n\n");

    printf("=== Example completed! ===\n");
    return 0;
}
