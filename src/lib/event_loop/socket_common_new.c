// ...existing includes...
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ptk_err.h>
#include <ptk_alloc.h>
#include <ptk_log.h>
#include "socket_common.h"
#include "event_loop.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "ptk_threadlet.h"

//=============================================================================
// Common socket operations
//=============================================================================

// Forward declaration of current threadlet (set by event loop)
ptk_threadlet_t *current_threadlet;

// Helper to set non-blocking mode
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

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
    if (current_threadlet && sock->event_loop && current_threadlet->status == THREADLET_WAITING) {
        event_registration_t *reg = event_registration_lookup(sock->event_loop->registrations, sock->fd);
        if (reg && reg->waiting_threadlet == current_threadlet) {
            current_threadlet->status = THREADLET_ABORTED;
            threadlet_queue_enqueue(&sock->event_loop->ready_queue, current_threadlet);
            event_registration_remove(sock->event_loop->registrations, sock->fd);
            platform_remove_fd(sock->event_loop->platform, sock->fd);
            trace("Aborted waiting threadlet for fd %d", sock->fd);
        }
    }

    if (sock->fd >= 0) {
        trace("Closing socket fd %d", sock->fd);
        // Remove from event loop if not already done
        event_registration_remove(sock->event_loop->registrations, sock->fd);
        platform_remove_fd(sock->event_loop->platform, sock->fd);
        // Shutdown first to notify peer
        shutdown(sock->fd, SHUT_RDWR);
        close(sock->fd);
        sock->fd = -1;
    }

    if (sock->type == PTK_SOCK_TCP_CLIENT || sock->type == PTK_SOCK_TCP_SERVER) {
        // Additional cleanup if needed
    }
    sock->type = PTK_SOCK_INVALID;

    debug("socket destroyed");
}

//=============================================================================
// NETWORK DISCOVERY
//=============================================================================

static void network_info_dispose(void *ptr) {
    ptk_network_info *info = (ptk_network_info *)ptr;
    if (info) {
        if (info->interfaces) {
            for (size_t i = 0; i < info->interface_count; i++) {
                ptk_free(info->interfaces[i].name);
                ptk_free(info->interfaces[i].addresses);
            }
            ptk_free(info->interfaces);
        }
        info->interface_count = 0;
    }
}

/**
 * Discover network interfaces on the local machine.
 * 
 * This function should return a list of network interfaces and their properties (IP addresses, etc).
 * The returned ptk_network_info structure must be freed by the caller using ptk_free().
 * If discovery fails, it returns NULL and sets an appropriate error code via ptk_set_err().
 * This is a stub implementation; platform-specific code is needed to actually discover interfaces.
 * @return Pointer to ptk_network_info on success, NULL on failure (ptk_get_err() set).
 */
ptk_network_info *ptk_network_discover(void) {
    debug("ptk_network_discover: entry");
    // Discover network interfaces and populate info
    ptk_network_info *info = ptk_alloc(sizeof(ptk_network_info), network_info_dispose);
    if (!info) {
        warn("ptk_alloc for ptk_network_info failed");
        ptk_set_err(PTK_ERR_NO_MEMORY);
        debug("ptk_network_discover: exit");
        return NULL;
    }
    // Call into the platform-specific implementation
    if (platform_discover_network(info) != PTK_OK) {
        warn("platform_discover_network failed");
        ptk_free(info);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        debug("ptk_network_discover: exit");
        return NULL;
    }

    debug("ptk_network_discover: exit");
    return info;
}

/**
 * Get the number of network interface entries
 *
 * @param network_info The network information structure
 * @return Number of network entries, 0 if network_info is NULL
 */
size_t ptk_socket_network_info_count(const ptk_network_info *network_info) {
    if (!network_info) return 0;
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
    if (!network_info || index >= network_info->interface_count) return NULL;
    return &network_info->interfaces[index];
}
