#pragma once

#include <ptk_sock.h>
#include "event_loop.h"

//=============================================================================
// Internal Socket Structure Definition
//=============================================================================

/**
 * @brief Internal socket structure 
 * This defines the complete structure that's forward-declared in the public API
 */
struct ptk_sock {
    int fd;                         // Socket file descriptor
    ptk_sock_type type;            // Socket type (TCP/UDP/etc)
    event_loop_t *event_loop;      // Associated event loop
    ptk_address_t local_addr;      // Local address
    ptk_address_t remote_addr;     // Remote address (for connected sockets)
    void *user_data;               // User-defined data pointer
};

//=============================================================================
// Internal Network Discovery Structure Definition  
//=============================================================================
// Internal Network Discovery Structure Definition  
//=============================================================================

/**
 * @brief Network interface information structure (uses public API definition)
 */
struct ptk_network_info {
    ptk_network_info_entry *interfaces;
    size_t interface_count;
};

//=============================================================================
// Platform-specific Network Discovery
//=============================================================================

/**
 * @brief Platform-specific network discovery function
 * @param info Network info structure to populate
 * @return PTK_OK on success, error code otherwise
 */
ptk_err platform_discover_network(ptk_network_info *info);
