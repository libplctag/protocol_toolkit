/**
 * @file network_list.c
 * @brief POSIX implementation of network interface listing for Protocol Toolkit
 */
#include <ptk_sock.h>
#include <ptk_alloc.h>
#include <ptk_err.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

struct ptk_network_info {
    ptk_network_info_entry *entries;
    size_t count;
};

/**
 * @brief Discover all network interfaces (POSIX)
 */
ptk_network_info *ptk_socket_network_list(void) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    size_t count = 0;
    // First pass: count interfaces
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
            count++;
    }
    if (count == 0) {
        freeifaddrs(ifaddr);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }
    ptk_network_info *info = ptk_alloc(sizeof(ptk_network_info), NULL);
    if (!info) {
        freeifaddrs(ifaddr);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    info->entries = ptk_alloc(sizeof(ptk_network_info_entry) * count, NULL);
    if (!info->entries) {
        ptk_free(&info);
        freeifaddrs(ifaddr);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    info->count = count;
    size_t idx = 0;
    for (ifa = ifaddr; ifa && idx < count; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            ptk_network_info_entry *entry = &info->entries[idx++];
            strncpy(entry->interface_name, ifa->ifa_name, sizeof(entry->interface_name));
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            strncpy(entry->ip_address, inet_ntoa(sa->sin_addr), sizeof(entry->ip_address));
            if (ifa->ifa_netmask) {
                struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
                strncpy(entry->netmask, inet_ntoa(nm->sin_addr), sizeof(entry->netmask));
            } else {
                entry->netmask[0] = '\0';
            }
            if (ifa->ifa_broadaddr) {
                struct sockaddr_in *ba = (struct sockaddr_in *)ifa->ifa_broadaddr;
                strncpy(entry->broadcast, inet_ntoa(ba->sin_addr), sizeof(entry->broadcast));
                entry->supports_broadcast = true;
            } else {
                entry->broadcast[0] = '\0';
                entry->supports_broadcast = false;
            }
            entry->is_up = (ifa->ifa_flags & IFF_UP) != 0;
            entry->is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
        }
    }
    freeifaddrs(ifaddr);
    return info;
}

/**
 * @brief Get the number of network interface entries
 */
size_t ptk_socket_network_info_count(const ptk_network_info *network_info) {
    if (!network_info) return 0;
    return network_info->count;
}

/**
 * @brief Get a specific network interface entry by index
 */
const ptk_network_info_entry *ptk_socket_network_info_get(const ptk_network_info *network_info, size_t index) {
    if (!network_info || index >= network_info->count) return NULL;
    return &network_info->entries[index];
}
