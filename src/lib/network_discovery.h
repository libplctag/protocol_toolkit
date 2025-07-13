#pragma once
#include <stdint.h>
#include <stdbool.h>

// Network interface info
typedef struct {
    char name[32];
    char ip[40]; // IPv4 or IPv6
    bool is_up;
    bool is_loopback;
} network_interface_info_t;

// Enumerate all network interfaces
// Returns number of interfaces found, fills array up to max_count
int network_discovery_enumerate(network_interface_info_t *out_array, int max_count);

