/**
 * EtherNet/IP Device Discovery Tool
 *
 * This tool discovers EtherNet/IP devices on the network by sending List Identity
 * broadcasts and processing responses. Uses the Protocol Toolkit APIs.
 */

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ptk_alloc.h"
#include "ptk_buf.h"
#include "ptk_err.h"
#include "ptk_socket.h"
#include "ptk_thread.h"
#include "ptk_utils.h"

//=============================================================================
// ETHERNETIP PROTOCOL CONSTANTS
//=============================================================================

#define EIP_PORT 44818
#define EIP_LIST_IDENTITY_CMD 0x0063

// EtherNet/IP Encapsulation Header
#define EIP_ENCAP_HEADER_SIZE 24

// Common Packet Format (CPF) Item Types
#define CPF_TYPE_NULL 0x0000
#define CPF_TYPE_CIP_IDENTITY 0x000C
#define CPF_TYPE_SOCKET_ADDR 0x8000

//=============================================================================
// GLOBAL STATE
//=============================================================================

static volatile bool g_running = true;
static int g_responses_received = 0;
static ptk_sock *g_udp_socket = NULL;
static ptk_thread *g_discovery_thread = NULL;
static ptk_allocator_t *g_allocator = NULL;

//=============================================================================
// SIGNAL HANDLING
//=============================================================================

static void signal_handler(void) {
    printf("\nReceived signal, shutting down...\n");
    g_running = false;

    if(g_udp_socket) { ptk_socket_abort(g_udp_socket); }
}

//=============================================================================
// ETHERNETIP PROTOCOL FUNCTIONS
//=============================================================================

/**
 * Build EtherNet/IP List Identity request packet
 */
static ptk_err build_list_identity_request(ptk_buf_t *buffer) {
    // EtherNet/IP Encapsulation Header (24 bytes) - all little endian
    return ptk_buf_produce(buffer, "< w w d d q d",
                           (uint16_t)EIP_LIST_IDENTITY_CMD,  // Command
                           (uint16_t)0,                      // Length (0 for List Identity)
                           (uint32_t)0,                      // Session Handle
                           (uint32_t)0,                      // Status
                           (uint64_t)0,                      // Sender Context (8 bytes)
                           (uint32_t)0);                     // Options
}

/**
 * Parse EtherNet/IP List Identity response
 */
static ptk_err parse_list_identity_response(ptk_buf_t *buffer, const ptk_address_t *sender_addr) {
    ptk_err err;

    char *sender_ip = ptk_address_to_string(g_allocator, sender_addr);
    uint16_t sender_port = ptk_address_get_port(sender_addr);

    printf("\n=== EtherNet/IP Device Found ===\n");
    printf("From: %s:%d\n", sender_ip ? sender_ip : "unknown", sender_port);

    if(sender_ip) { ptk_free(g_allocator, sender_ip); }

    // Parse Encapsulation Header (all little endian)
    uint16_t command, length;
    uint32_t session_handle, status;
    uint64_t sender_context;
    uint32_t options;

    err = ptk_buf_consume(buffer, false, "< w w d d q d", &command, &length, &session_handle, &status, &sender_context, &options);
    if(err != PTK_OK) { return err; }

    printf("Command: 0x%04X\n", command);
    printf("Status: 0x%08X\n", status);

    if(command != EIP_LIST_IDENTITY_CMD) {
        printf("Warning: Unexpected command 0x%04X\n", command);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    if(status != 0) {
        printf("Error: Non-zero status 0x%08X\n", status);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Parse CPF (Common Packet Format) data if present
    if(length > 0) {
        uint16_t item_count;
        err = ptk_buf_consume(buffer, false, "w", &item_count);
        if(err != PTK_OK) { return err; }

        printf("CPF Items: %u\n", item_count);

        for(uint16_t i = 0; i < item_count; i++) {
            uint16_t type_id, item_length;

            err = ptk_buf_consume(buffer, false, "w w", &type_id, &item_length);
            if(err != PTK_OK) { return err; }

            printf("  Item %u: Type 0x%04X, Length %u\n", i + 1, type_id, item_length);

            if(type_id == CPF_TYPE_CIP_IDENTITY && item_length >= 34) {
                // Parse CIP Identity data
                uint16_t vendor_id, device_type, product_code;
                uint16_t status_word;
                uint32_t serial_number;
                uint8_t major_rev, minor_rev, product_name_len;

                err = ptk_buf_consume(buffer, false, "w w w b b w d b", &vendor_id, &device_type, &product_code, &major_rev,
                                      &minor_rev, &status_word, &serial_number, &product_name_len);
                if(err != PTK_OK) { break; }

                printf("    Vendor ID: 0x%04X\n", vendor_id);
                printf("    Device Type: 0x%04X\n", device_type);
                printf("    Product Code: 0x%04X\n", product_code);
                printf("    Revision: %u.%u\n", major_rev, minor_rev);
                printf("    Status: 0x%04X\n", status_word);
                printf("    Serial Number: %u (0x%08X)\n", serial_number, serial_number);

                if(product_name_len > 0 && product_name_len < 64) {
                    char product_name[65] = {0};
                    uint8_t *name_ptr = ptk_buf_get_start_ptr(buffer);
                    size_t available = ptk_buf_len(buffer);

                    if(available >= product_name_len) {
                        memcpy(product_name, name_ptr, product_name_len);
                        product_name[product_name_len] = '\0';
                        printf("    Product Name: %s\n", product_name);

                        // Skip past the product name
                        size_t current_start = ptk_buf_get_start(buffer);
                        ptk_buf_set_start(buffer, current_start + product_name_len);
                    }
                }
            } else if(type_id == CPF_TYPE_SOCKET_ADDR && item_length >= 16) {
                // Parse Socket Address (network byte order)
                uint16_t sin_family, sin_port;
                uint32_t sin_addr;
                uint8_t padding[8];

                err = ptk_buf_consume(buffer, false, "> w w d 8*b", &sin_family, &sin_port, &sin_addr, 8, padding);
                if(err != PTK_OK) { break; }

                uint8_t *addr_bytes = (uint8_t *)&sin_addr;
                printf("    Socket Address: %u.%u.%u.%u:%u\n", addr_bytes[0], addr_bytes[1], addr_bytes[2], addr_bytes[3],
                       sin_port);
            } else {
                // Skip unknown item type
                size_t current_start = ptk_buf_get_start(buffer);
                ptk_buf_set_start(buffer, current_start + item_length);
            }
        }
    }

    printf("================================\n");
    return PTK_OK;
}

//=============================================================================
// DISCOVERY THREAD
//=============================================================================

static void discovery_thread(void *arg) {
    int *discovery_time = (int *)arg;
    ptk_time_ms start_time = ptk_now_ms();
    ptk_time_ms end_time = start_time + (*discovery_time * 1000);

    printf("Starting EtherNet/IP device discovery for %d seconds...\n", *discovery_time);

    // Get network interfaces for broadcasting
    ptk_network_info *networks = ptk_socket_find_networks(g_allocator);
    size_t num_networks = 0;

    if(!networks) {
        printf("Warning: Failed to discover networks\n");
        printf("Will use fallback broadcast to 255.255.255.255\n");
    } else {
        num_networks = ptk_socket_network_info_count(networks);
        printf("Discovered %zu network interfaces:\n", num_networks);
        for(size_t i = 0; i < num_networks; i++) {
            const ptk_network_info_entry *entry = ptk_socket_network_info_get(networks, i);
            if(entry) {
                printf("  %zu: IP=%s, Netmask=%s, Broadcast=%s\n", i + 1, entry->network_ip, entry->netmask, entry->broadcast);
            }
        }
    }

    // Create local address for UDP socket
    ptk_address_t local_addr;
    ptk_err err = ptk_address_create_any(&local_addr, 0);
    if(err != PTK_OK) {
        printf("Failed to create local address: %s\n", ptk_err_to_string(err));
        if(networks) { ptk_socket_network_info_dispose(networks); }
        return;
    }

    // Create UDP socket for discovery
    g_udp_socket = ptk_udp_socket_create(g_allocator, &local_addr);
    if(!g_udp_socket) {
        printf("Failed to create UDP socket\n");
        if(networks) { ptk_socket_network_info_dispose(networks); }
        return;
    }

    printf("UDP socket created for discovery\n");

    // Broadcast interval (5 seconds)
    ptk_time_ms last_broadcast = 0;
    const ptk_time_ms BROADCAST_INTERVAL = 5000;

    while(g_running && ptk_now_ms() < end_time) {
        ptk_time_ms current_time = ptk_now_ms();

        // Send broadcast if it's time
        if(current_time - last_broadcast >= BROADCAST_INTERVAL) {
            ptk_buf_t *request_buf = ptk_buf_create(g_allocator, 32);
            if(!request_buf) {
                printf("Failed to create request buffer\n");
                break;
            }

            err = build_list_identity_request(request_buf);
            if(err == PTK_OK) {
                // Send to all discovered networks
                if(networks && num_networks > 0) {
                    for(size_t i = 0; i < num_networks; i++) {
                        const ptk_network_info_entry *entry = ptk_socket_network_info_get(networks, i);
                        if(!entry) { continue; }

                        // Reset buffer for each send
                        ptk_buf_set_start(request_buf, 0);
                        ptk_buf_set_end(request_buf, 0);
                        build_list_identity_request(request_buf);

                        // Create broadcast address
                        ptk_address_t broadcast_addr;
                        err = ptk_address_create(&broadcast_addr, entry->broadcast, EIP_PORT);
                        if(err == PTK_OK) {
                            err = ptk_udp_socket_send_to(g_udp_socket, request_buf, &broadcast_addr, true);
                            if(err == PTK_OK) {
                                printf("Sent broadcast to %s:%d\n", entry->broadcast, EIP_PORT);
                            } else if(err != PTK_ERR_ABORT) {
                                printf("Failed to send to %s: %s\n", entry->broadcast, ptk_err_to_string(err));
                            }
                        }
                    }
                } else {
                    // Fallback broadcast
                    ptk_address_t broadcast_addr;
                    err = ptk_address_create(&broadcast_addr, "255.255.255.255", EIP_PORT);
                    if(err == PTK_OK) {
                        err = ptk_udp_socket_send_to(g_udp_socket, request_buf, &broadcast_addr, true);
                        if(err == PTK_OK) {
                            printf("Sent fallback broadcast to 255.255.255.255:%d\n", EIP_PORT);
                        } else if(err != PTK_ERR_ABORT) {
                            printf("Failed to send fallback broadcast: %s\n", ptk_err_to_string(err));
                        }
                    }
                }
            }

            ptk_buf_dispose(request_buf);
            last_broadcast = current_time;
        }

        // Listen for responses
        ptk_buf_t *response_buf = ptk_buf_create(g_allocator, 512);
        if(!response_buf) {
            printf("Failed to create response buffer\n");
            usleep(100000);
            continue;
        }

        ptk_address_t sender_addr;
        err = ptk_udp_socket_recv_from(g_udp_socket, response_buf, &sender_addr);

        if(err == PTK_OK) {
            g_responses_received++;
            parse_list_identity_response(response_buf, &sender_addr);
        } else if(err == PTK_ERR_ABORT) {
            printf("Discovery aborted\n");
            ptk_buf_dispose(response_buf);
            break;
        } else if(err == PTK_ERR_WOULD_BLOCK || err == PTK_ERR_TIMEOUT) {
            // No data available, continue
            ptk_buf_dispose(response_buf);
            usleep(100000);  // Sleep 100ms
        } else {
            printf("Receive error: %s\n", ptk_err_to_string(err));
            ptk_buf_dispose(response_buf);
            usleep(100000);  // Sleep 100ms before retry
        }
    }

    // Cleanup
    if(networks) { ptk_socket_network_info_dispose(networks); }

    printf("Discovery thread ending\n");
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    printf("EtherNet/IP Device Discovery Tool\n");
    printf("Using Protocol Toolkit APIs\n\n");

    // Create allocator
    g_allocator = allocator_default_create(8);
    if(!g_allocator) {
        printf("Failed to create allocator\n");
        return 1;
    }

    // Parse command line arguments
    int discovery_time = 30;  // Default 30 seconds

    if(argc > 1) {
        discovery_time = atoi(argv[1]);
        if(discovery_time < 1) { discovery_time = 30; }
    }

    printf("Configuration:\n");
    printf("  Discovery time: %d seconds\n", discovery_time);
    printf("  EtherNet/IP Port: %d\n", EIP_PORT);
    printf("  Broadcast interval: 5 seconds\n\n");

    // Set up signal handling
    ptk_set_interrupt_handler(signal_handler);

    // Start discovery thread
    g_discovery_thread = ptk_thread_create(g_allocator, discovery_thread, &discovery_time);
    if(!g_discovery_thread) {
        printf("Failed to create discovery thread\n");
        ptk_allocator_destroy(g_allocator);
        return 1;
    }

    printf("Discovery started. Press Ctrl+C to stop early...\n\n");

    // Wait for discovery thread to complete
    ptk_err err = ptk_thread_join(g_discovery_thread);
    if(err != PTK_OK) { printf("Error joining discovery thread: %s\n", ptk_err_to_string(err)); }

    // Cleanup
    if(g_udp_socket) { ptk_socket_close(g_udp_socket); }

    if(g_discovery_thread) { ptk_thread_destroy(g_discovery_thread); }

    if(g_allocator) { ptk_allocator_destroy(g_allocator); }

    printf("\n=== Discovery Summary ===\n");
    printf("Total devices found: %d\n", g_responses_received);
    printf("Discovery completed.\n");

    return 0;
}