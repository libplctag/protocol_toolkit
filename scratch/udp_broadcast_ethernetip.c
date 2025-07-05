#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

// Global flag for graceful shutdown
static volatile bool g_running = true;

static void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    g_running = false;
}

// Print buffer as hex and ASCII
static void print_buffer(const unsigned char *data, size_t len, const char *prefix) {
    printf("%s (%zu bytes):\n", prefix, len);

    for(size_t i = 0; i < len; i += 16) {
        // Print hex values
        printf("  %04zx: ", i);
        for(size_t j = 0; j < 16; j++) {
            if(i + j < len) {
                printf("%02x ", data[i + j]);
            } else {
                printf("   ");
            }
        }

        // Print ASCII characters
        printf(" |");
        for(size_t j = 0; j < 16 && i + j < len; j++) {
            unsigned char c = data[i + j];
            if(c >= 32 && c <= 126) {
                printf("%c", c);
            } else {
                printf(".");
            }
        }
        printf("|\n");
    }
    printf("\n");
}

int main() {
    printf("EtherNet/IP GetIdentity UDP Broadcast Tool\n");
    printf("==========================================\n");

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == -1) {
        printf("Failed to create socket: %s\n", strerror(errno));
        return 1;
    }

    // Bind to local interface to ensure proper routing
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = inet_addr("10.206.1.66");  // Our local IP
    local_addr.sin_port = 0;                                // Let system choose port

    if(bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) != 0) {
        printf("Failed to bind socket to local interface: %s\n", strerror(errno));
        close(sock);
        return 1;
    }

    // Enable broadcast
    int broadcast = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) != 0) {
        printf("Failed to enable broadcast: %s\n", strerror(errno));
        close(sock);
        return 1;
    }

    // Set socket timeout for receiving
    struct timeval timeout;
    timeout.tv_sec = 1;  // 1 second timeout
    timeout.tv_usec = 0;
    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        printf("Failed to set socket timeout: %s\n", strerror(errno));
        close(sock);
        return 1;
    }

    printf("UDP socket created successfully (fd=%d)\n", sock);

    // EtherNet/IP GetIdentity request (24 bytes)
    // Command: 0x63 (List Identity), Length: 0x00, Session: 0x00, Status: 0x00
    unsigned char get_identity_request[] = {
        0x63, 0x00,                                     // Command: List Identity
        0x00, 0x00,                                     // Length: 0 (no data)
        0x00, 0x00, 0x00, 0x00,                         // Session Handle: 0
        0x00, 0x00, 0x00, 0x00,                         // Status: 0
        0x00, 0x00, 0x00, 0x00,                         // Sender Context (8 bytes)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Options: 0
    };

    print_buffer(get_identity_request, sizeof(get_identity_request), "GetIdentity Request");

    // Focus on local broadcast network
    const char *broadcast_addresses[] = {
        "255.255.255.255",  // fails
        "10.206.1.255"      // works, subnet broadcast address.
    };
    int num_addresses = sizeof(broadcast_addresses) / sizeof(broadcast_addresses[0]);

    int port = 2222;
    int responses_received = 0;

    printf("Sending GetIdentity requests to port %d...\n", port);

    // Send to each broadcast address
    for(int i = 0; i < num_addresses && g_running; i++) {
        printf("\nSending to broadcast address %s:%d...\n", broadcast_addresses[i], port);

        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);

        if(inet_pton(AF_INET, broadcast_addresses[i], &dest_addr.sin_addr) != 1) {
            printf("Invalid broadcast address: %s\n", broadcast_addresses[i]);
            continue;
        }

        ssize_t bytes_sent =
            sendto(sock, get_identity_request, sizeof(get_identity_request), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        if(bytes_sent == -1) {
            printf("sendto() failed: errno=%d (%s)\n", errno, strerror(errno));
            continue;
        }

        printf("Successfully sent %zd bytes to %s:%d\n", bytes_sent, broadcast_addresses[i], port);

        // Wait for responses for 5 seconds after each broadcast
        printf("Waiting for responses...\n");
        time_t start_time = time(NULL);

        while(g_running && (time(NULL) - start_time) < 5) {
            unsigned char response_buffer[1024];
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);

            ssize_t bytes_received =
                recvfrom(sock, response_buffer, sizeof(response_buffer), 0, (struct sockaddr *)&sender_addr, &sender_len);

            if(bytes_received == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout, continue waiting
                    continue;
                } else {
                    printf("recvfrom() error: %s\n", strerror(errno));
                    break;
                }
            }

            if(bytes_received > 0) {
                responses_received++;

                char sender_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
                int sender_port = ntohs(sender_addr.sin_port);

                printf("\n=== Response #%d from %s:%d ===\n", responses_received, sender_ip, sender_port);
                print_buffer(response_buffer, bytes_received, "Response Data");

                // Try to parse basic EtherNet/IP header
                if(bytes_received >= 24) {
                    printf("EtherNet/IP Header Analysis:\n");
                    printf("  Command: 0x%02x%02x\n", response_buffer[1], response_buffer[0]);
                    printf("  Length: %d\n", response_buffer[2] | (response_buffer[3] << 8));
                    printf("  Session: 0x%02x%02x%02x%02x\n", response_buffer[7], response_buffer[6], response_buffer[5],
                           response_buffer[4]);
                    printf("  Status: 0x%02x%02x%02x%02x\n", response_buffer[11], response_buffer[10], response_buffer[9],
                           response_buffer[8]);
                }
            }
        }
    }

    // Wait a bit longer for any delayed responses
    printf("\nWaiting for any additional responses...\n");
    time_t final_start = time(NULL);

    while(g_running && (time(NULL) - final_start) < 3) {
        unsigned char response_buffer[1024];
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);

        ssize_t bytes_received =
            recvfrom(sock, response_buffer, sizeof(response_buffer), 0, (struct sockaddr *)&sender_addr, &sender_len);

        if(bytes_received == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                printf("recvfrom() error: %s\n", strerror(errno));
                break;
            }
        }

        if(bytes_received > 0) {
            responses_received++;

            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
            int sender_port = ntohs(sender_addr.sin_port);

            printf("\n=== Response #%d from %s:%d ===\n", responses_received, sender_ip, sender_port);
            print_buffer(response_buffer, bytes_received, "Response Data");

            // Try to parse basic EtherNet/IP header
            if(bytes_received >= 24) {
                printf("EtherNet/IP Header Analysis:\n");
                printf("  Command: 0x%02x%02x\n", response_buffer[1], response_buffer[0]);
                printf("  Length: %d\n", response_buffer[2] | (response_buffer[3] << 8));
                printf("  Session: 0x%02x%02x%02x%02x\n", response_buffer[7], response_buffer[6], response_buffer[5],
                       response_buffer[4]);
                printf("  Status: 0x%02x%02x%02x%02x\n", response_buffer[11], response_buffer[10], response_buffer[9],
                       response_buffer[8]);
            }
        }
    }

    printf("\n=== Summary ===\n");
    printf("Total responses received: %d\n", responses_received);

    close(sock);
    printf("UDP broadcast tool completed.\n");
    return responses_received > 0 ? 0 : 1;
}