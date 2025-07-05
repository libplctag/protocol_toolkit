#include <arpa/inet.h>  // For inet_ntop, inet_pton
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>  // For AF_INET (IPv4)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

// Helper function to convert sockaddr_in to a uint32_t for bitwise operations
uint32_t sockaddr_to_uint32(const struct sockaddr_in *addr) { return ntohl(addr->sin_addr.s_addr); }

// Helper function to convert uint32_t to sockaddr_in
void uint32_to_sockaddr(uint32_t val, struct sockaddr_in *addr) {
    addr->sin_family = AF_INET;
    addr->sin_port = 0;  // Not relevant for address only
    addr->sin_addr.s_addr = htonl(val);
}

int main() {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    char ip_str[NI_MAXHOST];
    char netmask_str[NI_MAXHOST];
    char broadcast_str[NI_MAXHOST];

    if(getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    /* Walk through linked list, maintaining pointer to current entry. */
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr == NULL || ifa->ifa_netmask == NULL) {
            continue;  // Skip if address or netmask is NULL
        }

        family = ifa->ifa_addr->sa_family;

        // Process IPv4 interfaces only
        if(family == AF_INET) {
            printf("Interface: %s\n", ifa->ifa_name);

            struct sockaddr_in *sa_ip = (struct sockaddr_in *)ifa->ifa_addr;
            struct sockaddr_in *sa_netmask = (struct sockaddr_in *)ifa->ifa_netmask;

            // Convert IP and Netmask to string for display
            inet_ntop(AF_INET, &(sa_ip->sin_addr), ip_str, sizeof(ip_str));
            inet_ntop(AF_INET, &(sa_netmask->sin_addr), netmask_str, sizeof(netmask_str));

            printf("  IP Address: %s\n", ip_str);
            printf("  Netmask:    %s\n", netmask_str);

            // --- Calculate Broadcast Address ---
            uint32_t ip_int = sockaddr_to_uint32(sa_ip);
            uint32_t netmask_int = sockaddr_to_uint32(sa_netmask);

            // Calculate network address (IP AND Netmask)
            uint32_t network_int = ip_int & netmask_int;

            // Calculate inverse netmask (NOT Netmask) - bitwise NOT
            uint32_t inverse_netmask_int = ~netmask_int;

            // Calculate broadcast address (Network OR Inverse Netmask)
            uint32_t broadcast_int = network_int | inverse_netmask_int;

            // Convert broadcast address back to sockaddr_in and then string
            struct sockaddr_in sa_broadcast;
            uint32_to_sockaddr(broadcast_int, &sa_broadcast);
            inet_ntop(AF_INET, &(sa_broadcast.sin_addr), broadcast_str, sizeof(broadcast_str));

            printf("  Broadcast:  %s\n", broadcast_str);
            printf("\n");
        }
        // You can add similar logic for AF_INET6 if you need IPv6 broadcast addresses
        // IPv6 uses a different broadcast concept (multicast) so this calculation wouldn't apply directly.
    }

    freeifaddrs(ifaddr);  // Free the allocated memory
    exit(EXIT_SUCCESS);
}
