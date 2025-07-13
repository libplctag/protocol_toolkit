// ...existing includes...
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <ptk_err.h>
#include <ptk_alloc.h>
#include <ptk_log.h>
#include "socket_common.h"
#include "socket_internal.h"
#include "event_loop.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "threadlet_integration.h"

//=============================================================================
// Common socket operations
//=============================================================================

// Forward declaration of current threadlet (set by event loop)
threadlet_t *current_threadlet;

// Helper to set non-blocking mode
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags < 0) { return -1; }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// char *ptk_address_to_string(const ptk_address_t *address) {
//     if(!address) { return NULL; }

//     char *str = ptk_alloc(INET_ADDRSTRLEN, NULL);
//     if(!str) { return NULL; }
//     if(address->family != AF_INET) {
//         ptk_free(str);
//         return NULL;  // Only supports IPv4 for now
//     }
//     struct in_addr addr;
//     addr.s_addr = address->ip;

//     if(!inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN)) {
//         ptk_free(str);
//         return NULL;
//     }

//     return str;
// }

// uint16_t ptk_address_get_port(const ptk_address_t *address) {
//     if(!address) { return 0; }
//     return address->port;
// }

// bool ptk_address_equals(const ptk_address_t *addr1, const ptk_address_t *addr2) {
//     if(!addr1 || !addr2) { return false; }
//     return addr1->ip == addr2->ip && addr1->port == addr2->port && addr1->family == addr2->family;
// }

// /**
//  * @brief Convert ptk_address_t to sockaddr_storage
//  */
// static ptk_err ptk_address_to_sockaddr(const ptk_address_t *address, struct sockaddr_storage *sockaddr, socklen_t *sockaddr_len) {
//     if(!address || !sockaddr || !sockaddr_len) { return PTK_ERR_NULL_PTR; }

//     struct sockaddr_in *addr_in = (struct sockaddr_in *)sockaddr;
//     memset(addr_in, 0, sizeof(struct sockaddr_in));
//     addr_in->sin_family = AF_INET;
//     addr_in->sin_port = htons(address->port);
//     addr_in->sin_addr.s_addr = address->ip;
//     *sockaddr_len = sizeof(struct sockaddr_in);

//     return PTK_OK;
// }

// /**
//  * @brief Convert sockaddr_storage to ptk_address_t
//  */
// static ptk_err ptk_sockaddr_to_address(const struct sockaddr_storage *sockaddr, socklen_t sockaddr_len, ptk_address_t *address) {
//     if(!sockaddr || !address) { return PTK_ERR_NULL_PTR; }

//     if(sockaddr->ss_family != AF_INET) { return PTK_ERR_INVALID_PARAM; }

//     const struct sockaddr_in *addr_in = (const struct sockaddr_in *)sockaddr;
//     memset(address, 0, sizeof(ptk_address_t));
//     address->family = AF_INET;
//     address->port = ntohs(addr_in->sin_port);
//     address->ip = addr_in->sin_addr.s_addr;

//     return PTK_OK;
// }


/**
 * @brief Socket destructor called when ptk_free() is called on a socket
 *
 * @param ptr Pointer to the socket to destroy
 */
void ptk_socket_destructor(void *ptr) {
    ptk_sock *sock = (ptk_sock *)ptr;
    if (!sock) return;
    
    debug("destroying socket");

    // If the current threadlet is waiting on this socket, abort it before closing fd
    if(current_threadlet && sock->event_loop && current_threadlet->status == THREADLET_WAITING) {
        event_registration_t *reg = event_registration_lookup(sock->event_loop->registrations, sock->fd);
        if(reg && reg->waiting_threadlet == current_threadlet) {
            current_threadlet->status = THREADLET_ABORTED;
            threadlet_queue_enqueue(&sock->event_loop->ready_queue, current_threadlet);
            event_registration_remove(sock->event_loop->registrations, sock->fd);
            platform_remove_fd(sock->event_loop->platform, sock->fd);
            trace("Aborted waiting threadlet for fd %d", sock->fd);
        }
    }

    if(sock->fd >= 0) {
        trace("Closing socket fd %d", sock->fd);
        // Remove from event loop if not already done
        event_registration_remove(sock->event_loop->registrations, sock->fd);
        platform_remove_fd(sock->event_loop->platform, sock->fd);
        // Shutdown first to notify peer
        shutdown(sock->fd, SHUT_RDWR);
        close(sock->fd);
        sock->fd = -1;
    }

    if(sock->type == PTK_SOCK_TCP_CLIENT || sock->type == PTK_SOCK_TCP_SERVER) {
        // Additional cleanup if needed
    }
    sock->type = PTK_SOCK_INVALID;

    debug("socket destroyed");
}

//=============================================================================
// ADDRESS UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Initialize an address structure from IP string and port
 *
 * @param address Output parameter for the initialized address
 * @param ip_string The IP address as a string (e.g., "192.168.1.1" or NULL for INADDR_ANY)
 * @param port The port number in host byte order
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_address_init(ptk_address_t *address, const char *ip_string, uint16_t port) {
    if (!address) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }

    memset(address, 0, sizeof(ptk_address_t));
    address->family = AF_INET;
    address->port = port;

    if (ip_string == NULL) {
        // Use INADDR_ANY (0.0.0.0)
        address->ip = INADDR_ANY;
    } else {
        struct in_addr addr;
        if (inet_aton(ip_string, &addr) == 0) {
            ptk_set_err(PTK_ERR_INVALID_PARAM);
            return PTK_ERR_INVALID_PARAM;
        }
        address->ip = addr.s_addr; // Already in network byte order
    }

    return PTK_OK;
}

/**
 * @brief Convert an address structure to an IP string
 *
 * @param address The address structure to convert
 * @return Allocated string containing IP address, or NULL on failure. Caller must free with ptk_free()
 */
char *ptk_address_to_string(const ptk_address_t *address) {
    if (!address) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    struct in_addr addr;
    addr.s_addr = address->ip;
    
    char *ip_str = inet_ntoa(addr);
    if (!ip_str) {
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }

    // Allocate and copy the string
    size_t len = strlen(ip_str) + 1;
    char *result = ptk_alloc(len, NULL);
    if (!result) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }

    strcpy(result, ip_str);
    return result;
}

/**
 * @brief Get the port number from an address structure
 *
 * @param address The address structure
 * @return Port number in host byte order, 0 if address is NULL
 */
uint16_t ptk_address_get_port(const ptk_address_t *address) {
    if (!address) {
        return 0;
    }
    return address->port;
}

/**
 * @brief Check if two addresses are equal
 *
 * @param addr1 First address
 * @param addr2 Second address
 * @return true if addresses are equal, false otherwise
 */
bool ptk_address_equals(const ptk_address_t *addr1, const ptk_address_t *addr2) {
    if (!addr1 || !addr2) {
        return false;
    }
    
    return (addr1->ip == addr2->ip && 
            addr1->port == addr2->port && 
            addr1->family == addr2->family);
}

/**
 * @brief Initialize an address for any interface (INADDR_ANY)
 *
 * @param address Output parameter for the initialized address
 * @param port The port number in host byte order
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_address_init_any(ptk_address_t *address, uint16_t port) {
    return ptk_address_init(address, NULL, port);
}

//=============================================================================
// NETWORK DISCOVERY
//=============================================================================

static void network_info_dispose(void *ptr) {
    ptk_network_info *info = (ptk_network_info *)ptr;
    if (info) {
        if (info->interfaces) {
            // for (size_t i = 0; i < info->interface_count; i++) {
            //     ptk_free(&(info->interfaces[i].network_ip));
            //     ptk_free(&(info->interfaces[i].netmask));
            //     ptk_free(&(info->interfaces[i].broadcast));
            // }
            ptk_free(info->interfaces);
        }
        info->interface_count = 0;
    }
}

/**
 * Discover network interfaces on the local machine.
 *
 * This function returns a list of network interfaces and their properties (IP addresses, etc).
 * The returned ptk_network_info structure must be freed by the caller using ptk_free().
 * If discovery fails, it returns NULL and sets an appropriate error code via ptk_set_err().
 * This is a stub implementation; platform-specific code is needed to actually discover interfaces.
 * @return Pointer to ptk_network_info on success, NULL on failure (ptk_get_err() set).
 */
ptk_network_info *ptk_socket_find_networks(void) {
    debug("ptk_socket_find_networks: entry");
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        return NULL;
    }

    // Count IPv4 interfaces
    size_t count = 0;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
            count++;
    }

    ptk_network_info *info = ptk_alloc(sizeof(ptk_network_info), network_info_dispose);
    if (!info) {
        freeifaddrs(ifaddr);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    info->interface_count = count;
    info->interfaces = ptk_alloc(count * sizeof(ptk_network_info_entry), NULL);

    size_t idx = 0;
    for (ifa = ifaddr; ifa && idx < count; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
            struct sockaddr_in *br = (ifa->ifa_broadaddr && ifa->ifa_broadaddr->sa_family == AF_INET) ? (struct sockaddr_in *)ifa->ifa_broadaddr : NULL;
            memset(&info->interfaces[idx], 0, sizeof(ptk_network_info_entry));
            snprintf(info->interfaces[idx].interface_name, sizeof(info->interfaces[idx].interface_name), "%s", ifa->ifa_name);
            snprintf(info->interfaces[idx].ip_address, sizeof(info->interfaces[idx].ip_address), "%s", inet_ntoa(sa->sin_addr));
            snprintf(info->interfaces[idx].netmask, sizeof(info->interfaces[idx].netmask), "%s", inet_ntoa(nm->sin_addr));
            if (br) {
                snprintf(info->interfaces[idx].broadcast, sizeof(info->interfaces[idx].broadcast), "%s", inet_ntoa(br->sin_addr));
            } else {
                info->interfaces[idx].broadcast[0] = '\0';
            }
            info->interfaces[idx].is_up = (ifa->ifa_flags & IFF_UP) != 0;
            info->interfaces[idx].is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;
            info->interfaces[idx].supports_broadcast = (ifa->ifa_flags & IFF_BROADCAST) != 0;
            idx++;
        }
    }

    freeifaddrs(ifaddr);
    debug("ptk_socket_find_networks: exit");
    return info;
}

/**
 * Get the number of network interface entries
 *
 * @param network_info The network information structure
 * @return Number of network entries, 0 if network_info is NULL
 */
size_t ptk_socket_network_info_count(const ptk_network_info *network_info) {
    if(!network_info) { return 0; }
    return network_info->interface_count;
}

/**
 * Get a specific network interface entry by index
 *
 * @param network_info The network information structure
 * @param index Index of the entry to retrieve (0-based)
 * @return Pointer to network entry, NULL if index is out of bounds or network_info is NULL
 */
const ptk_network_info_entry *ptk_socket_network_info_get(const ptk_network_info *network_info, size_t index) {
    if(!network_info || index >= network_info->interface_count) { return NULL; }
    return &network_info->interfaces[index];
}

