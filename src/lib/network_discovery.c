#include "network_discovery.h"
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

int network_discovery_enumerate(network_interface_info_t *out_array, int max_count) {
    struct ifaddrs *ifaddr, *ifa;
    int count = 0;
    if (getifaddrs(&ifaddr) == -1) return 0;
    for (ifa = ifaddr; ifa && count < max_count; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        network_interface_info_t *info = &out_array[count];
        strncpy(info->name, ifa->ifa_name, sizeof(info->name));
        info->is_up = (ifa->ifa_flags & IFF_UP) != 0;
        info->is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, info->ip, sizeof(info->ip));
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr, info->ip, sizeof(info->ip));
        } else {
            info->ip[0] = '\0';
        }
        count++;
    }
    freeifaddrs(ifaddr);
    return count;
}

