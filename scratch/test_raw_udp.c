#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int main() {
    printf("Testing raw UDP socket to PLCs...\n");
    
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        printf("Failed to create socket: %s\n", strerror(errno));
        return 1;
    }
    
    printf("Socket created successfully (fd=%d)\n", sock);
    
    // Test data (same as EtherNet/IP List Identity)
    unsigned char test_data[] = {
        0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    // Test sending to both PLCs
    const char* hosts[] = {"10.206.1.39", "10.206.1.40"};
    int port = 2222;
    
    for (int i = 0; i < 2; i++) {
        printf("Sending to %s:%d...\n", hosts[i], port);
        
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, hosts[i], &dest_addr.sin_addr) != 1) {
            printf("Invalid IP address: %s\n", hosts[i]);
            continue;
        }
        
        ssize_t bytes_sent = sendto(sock, test_data, sizeof(test_data), 0,
                                   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        if (bytes_sent == -1) {
            printf("sendto() failed: errno=%d (%s)\n", errno, strerror(errno));
        } else {
            printf("Successfully sent %zd bytes to %s:%d\n", bytes_sent, hosts[i], port);
        }
    }
    
    close(sock);
    printf("Test completed.\n");
    return 0;
}