/**
 * @file network_list.c
 * @brief POSIX implementation of network interface listing for Protocol Toolkit
 */
#include <ptk_sock.h>
#include <ptk_mem.h>
#include <ptk_err.h>
#include <ptk_array.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <net/if.h>

/**
 * @brief Calculate network address from IP and netmask
 */
static void calculate_network_address(const char *ip, const char *netmask, char *network) {
    struct in_addr ip_addr, mask_addr, net_addr;
    
    if (inet_aton(ip, &ip_addr) && inet_aton(netmask, &mask_addr)) {
        net_addr.s_addr = ip_addr.s_addr & mask_addr.s_addr;
        strcpy(network, inet_ntoa(net_addr));
    } else {
        network[0] = '\0';
    }
}

/**
 * @brief Calculate CIDR prefix length from netmask
 */
static uint8_t calculate_prefix_length(const char *netmask) {
    struct in_addr mask_addr;
    if (!inet_aton(netmask, &mask_addr)) {
        return 0;
    }
    
    uint32_t mask = ntohl(mask_addr.s_addr);
    uint8_t prefix = 0;
    
    while (mask & 0x80000000) {
        prefix++;
        mask <<= 1;
    }
    
    return prefix;
}

/**
 * @brief Discover all network interfaces (POSIX)
 */
ptk_network_interface_array_t *ptk_network_list_interfaces(void) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }

    // Create the array to hold network interfaces (start with 1 element, will resize as needed)
    ptk_network_interface_array_t *interfaces = ptk_network_interface_array_create(1, NULL);
    if (!interfaces) {
        freeifaddrs(ifaddr);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    // Reset length to 0 since we'll append elements
    interfaces->len = 0;

    // Process each interface
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            ptk_network_interface_t interface_info = {0};
            
            // Copy interface name (ensure null termination)
            strncpy(interface_info.interface_name, ifa->ifa_name, 
                   sizeof(interface_info.interface_name) - 1);
            interface_info.interface_name[sizeof(interface_info.interface_name) - 1] = '\0';
            
            // Get IP address
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            strncpy(interface_info.ip_address, inet_ntoa(sa->sin_addr), 
                   sizeof(interface_info.ip_address) - 1);
            interface_info.ip_address[sizeof(interface_info.ip_address) - 1] = '\0';
            
            // Get netmask
            if (ifa->ifa_netmask) {
                struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
                strncpy(interface_info.netmask, inet_ntoa(nm->sin_addr), 
                       sizeof(interface_info.netmask) - 1);
                interface_info.netmask[sizeof(interface_info.netmask) - 1] = '\0';
                
                // Calculate prefix length
                interface_info.prefix_length = calculate_prefix_length(interface_info.netmask);
                
                // Calculate network address
                calculate_network_address(interface_info.ip_address, 
                                        interface_info.netmask, 
                                        interface_info.network);
            } else {
                interface_info.netmask[0] = '\0';
                interface_info.network[0] = '\0';
                interface_info.prefix_length = 0;
            }
            
            // Get broadcast address
            if (ifa->ifa_broadaddr && (ifa->ifa_flags & IFF_BROADCAST)) {
                struct sockaddr_in *ba = (struct sockaddr_in *)ifa->ifa_broadaddr;
                strncpy(interface_info.broadcast, inet_ntoa(ba->sin_addr), 
                       sizeof(interface_info.broadcast) - 1);
                interface_info.broadcast[sizeof(interface_info.broadcast) - 1] = '\0';
                interface_info.supports_broadcast = true;
            } else {
                interface_info.broadcast[0] = '\0';
                interface_info.supports_broadcast = false;
            }
            
            // Set flags
            interface_info.is_up = (ifa->ifa_flags & IFF_UP) != 0;
            interface_info.is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
            
            // Add interface to array
            ptk_err_t result = ptk_network_interface_array_append(interfaces, interface_info);
            if (result != PTK_OK) {
                ptk_local_free(&interfaces);
                freeifaddrs(ifaddr);
                ptk_set_err(result);
                return NULL;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    
    // Check if we found any interfaces
    if (ptk_network_interface_array_len(interfaces) == 0) {
        ptk_local_free(&interfaces);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    
    return interfaces;
}

