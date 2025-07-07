/**
 * EtherNet/IP Device Discovery Tool
 *
 * This tool discovers EtherNet/IP devices on the network by sending List Identity
 * broadcasts and processing responses. Rewritten to use the new Protocol Toolkit APIs.
 */

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    ptk_err err;

    // EtherNet/IP Encapsulation Header (24 bytes)
    err = ptk_buf_produce_u16(buffer, EIP_LIST_IDENTITY_CMD, PTK_BUF_LITTLE_ENDIAN);  // Command
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u16(buffer, 0, PTK_BUF_LITTLE_ENDIAN);  // Length (0 for List Identity)
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u32(buffer, 0, PTK_BUF_LITTLE_ENDIAN);  // Session Handle
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u32(buffer, 0, PTK_BUF_LITTLE_ENDIAN);  // Status
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u64(buffer, 0, PTK_BUF_LITTLE_ENDIAN);  // Sender Context (8 bytes)
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u32(buffer, 0, PTK_BUF_LITTLE_ENDIAN);  // Options
    if(err != PTK_OK) { return err; }

    return PTK_OK;
}

/**
 * Parse EtherNet/IP List Identity response
 */
static ptk_err parse_list_identity_response(ptk_buf_t *buffer, const char *sender_ip, int sender_port) {
    ptk_err err;

    printf("\n=== EtherNet/IP Device Found ===\n");
    printf("From: %s:%d\n", sender_ip, sender_port);

    // Parse Encapsulation Header
    uint16_t command, length;
    uint32_t session_handle, status;
    uint64_t sender_context;
    uint32_t options;

    err = ptk_buf_consume_u16(buffer, &command, PTK_BUF_LITTLE_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_consume_u16(buffer, &length, PTK_BUF_LITTLE_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_consume_u32(buffer, &session_handle, PTK_BUF_LITTLE_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_consume_u32(buffer, &status, PTK_BUF_LITTLE_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_consume_u64(buffer, &sender_context, PTK_BUF_LITTLE_ENDIAN, false);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_consume_u32(buffer, &options, PTK_BUF_LITTLE_ENDIAN, false);
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
        err = ptk_buf_consume_u16(buffer, &item_count, PTK_BUF_LITTLE_ENDIAN, false);
        if(err != PTK_OK) { return err; }

        printf("CPF Items: %u\n", item_count);

        for(uint16_t i = 0; i < item_count; i++) {
            uint16_t type_id, item_length;

            err = ptk_buf_consume_u16(buffer, &type_id, PTK_BUF_LITTLE_ENDIAN, false);
            if(err != PTK_OK) { return err; }

            err = ptk_buf_consume_u16(buffer, &item_length, PTK_BUF_LITTLE_ENDIAN, false);
            if(err != PTK_OK) { return err; }

            printf("  Item %u: Type 0x%04X, Length %u\n", i + 1, type_id, item_length);

            if(type_id == CPF_TYPE_CIP_IDENTITY && item_length >= 34) {
                // Parse CIP Identity data
                uint16_t vendor_id, device_type, product_code;
                uint16_t major_rev, minor_rev, status_word;
                uint32_t serial_number;
                uint8_t product_name_len;

                err = ptk_buf_consume_u16(buffer, &vendor_id, PTK_BUF_LITTLE_ENDIAN, false);
                if(err != PTK_OK) { break; }

                err = ptk_buf_consume_u16(buffer, &device_type, PTK_BUF_LITTLE_ENDIAN, false);
                if(err != PTK_OK) { break; }

                err = ptk_buf_consume_u16(buffer, &product_code, PTK_BUF_LITTLE_ENDIAN, false);
                if(err != PTK_OK) { break; }

                uint8_t major_rev_u8, minor_rev_u8;
                err = ptk_buf_consume_u8(buffer, &major_rev_u8, false);
                if(err != PTK_OK) { break; }
                major_rev = major_rev_u8;

                err = ptk_buf_consume_u8(buffer, &minor_rev_u8, false);
                if(err != PTK_OK) { break; }
                minor_rev = minor_rev_u8;

                err = ptk_buf_consume_u16(buffer, &status_word, PTK_BUF_LITTLE_ENDIAN, false);
                if(err != PTK_OK) { break; }

                err = ptk_buf_consume_u32(buffer, &serial_number, PTK_BUF_LITTLE_ENDIAN, false);
                if(err != PTK_OK) { break; }

                err = ptk_buf_consume_u8(buffer, &product_name_len, false);
                if(err != PTK_OK) { break; }

                printf("    Vendor ID: 0x%04X\n", vendor_id);
                printf("    Device Type: 0x%04X\n", device_type);
                printf("    Product Code: 0x%04X\n", product_code);
                printf("    Revision: %u.%u\n", major_rev, minor_rev);
                printf("    Status: 0x%04X\n", status_word);
                printf("    Serial Number: %u (0x%08X)\n", serial_number, serial_number);

                if(product_name_len > 0 && product_name_len < 64) {
                    char product_name[65] = {0};
                    uint8_t *name_ptr;
                    ptk_buf_get_start_ptr(&name_ptr, buffer);

                    size_t available;
                    ptk_buf_len(&available, buffer);

                    if(available >= product_name_len) {
                        memcpy(product_name, name_ptr, product_name_len);
                        product_name[product_name_len] = '\0';
                        printf("    Product Name: %s\n", product_name);

                        // Skip past the product name
                        size_t current_start;
                        ptk_buf_get_start(&current_start, buffer);
                        ptk_buf_set_start(buffer, current_start + product_name_len);
                    }
                }
            } else if(type_id == CPF_TYPE_SOCKET_ADDR && item_length >= 16) {
                // Parse Socket Address
                uint16_t sin_family, sin_port;
                uint32_t sin_addr;

                err = ptk_buf_consume_u16(buffer, &sin_family, PTK_BUF_BIG_ENDIAN, false);  // Network byte order
                if(err != PTK_OK) { break; }

                err = ptk_buf_consume_u16(buffer, &sin_port, PTK_BUF_BIG_ENDIAN, false);  // Network byte order
                if(err != PTK_OK) { break; }

                err = ptk_buf_consume_u32(buffer, &sin_addr, PTK_BUF_BIG_ENDIAN, false);  // Network byte order
                if(err != PTK_OK) { break; }

                // Skip padding (8 bytes)
                for(int j = 0; j < 8; j++) {
                    uint8_t padding;
                    ptk_buf_consume_u8(buffer, &padding, false);
                }

                uint8_t *addr_bytes = (uint8_t *)&sin_addr;
                printf("    Socket Address: %u.%u.%u.%u:%u\n", addr_bytes[0], addr_bytes[1], addr_bytes[2], addr_bytes[3],
                       sin_port);
            } else {
                // Skip unknown item type
                size_t current_start;
                ptk_buf_get_start(&current_start, buffer);
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
    ptk_network_info *networks;
    size_t num_networks;

    ptk_err err = ptk_socket_find_networks(&networks, &num_networks);
    if(err != PTK_OK) {
        printf("Warning: Failed to discover networks: %s\n", ptk_err_to_string(err));
        printf("Will use fallback broadcast to 255.255.255.255\n");
        networks = NULL;
        num_networks = 0;
    } else {
        printf("Discovered %zu network interfaces:\n", num_networks);
        for(size_t i = 0; i < num_networks; i++) {
            printf("  %zu: IP=%s, Netmask=%s, Broadcast=%s\n", i + 1, networks[i].network_ip, networks[i].netmask,
                   networks[i].broadcast);
        }
    }

    // Create UDP socket for discovery
    err = ptk_udp_socket_create(&g_udp_socket, "0.0.0.0", 0);
    if(err != PTK_OK) {
        printf("Failed to create UDP socket: %s\n", ptk_err_to_string(err));
        if(networks) { ptk_socket_network_info_dispose(networks, num_networks); }
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
            uint8_t request_data[32];
            ptk_buf_t request_buf;
            ptk_buf_make(&request_buf, request_data, sizeof(request_data));

            err = build_list_identity_request(&request_buf);
            if(err == PTK_OK) {
                // Send to all discovered networks
                if(networks && num_networks > 0) {
                    for(size_t i = 0; i < num_networks; i++) {
                        // Reset buffer for each send
                        ptk_buf_make(&request_buf, request_data, sizeof(request_data));
                        build_list_identity_request(&request_buf);

                        err = ptk_udp_socket_send(g_udp_socket, &request_buf, networks[i].broadcast, EIP_PORT, true);
                        if(err == PTK_OK) {
                            printf("Sent broadcast to %s:%d\n", networks[i].broadcast, EIP_PORT);
                        } else if(err != PTK_ERR_ABORT) {
                            printf("Failed to send to %s: %s\n", networks[i].broadcast, ptk_err_to_string(err));
                        }
                    }
                } else {
                    // Fallback broadcast
                    err = ptk_udp_socket_send(g_udp_socket, &request_buf, "255.255.255.255", EIP_PORT, true);
                    if(err == PTK_OK) {
                        printf("Sent fallback broadcast to 255.255.255.255:%d\n", EIP_PORT);
                    } else if(err != PTK_ERR_ABORT) {
                        printf("Failed to send fallback broadcast: %s\n", ptk_err_to_string(err));
                    }
                }
            }

            last_broadcast = current_time;
        }

        // Listen for responses
        uint8_t response_data[512];
        ptk_buf_t response_buf;
        ptk_buf_make(&response_buf, response_data, sizeof(response_data));

        char sender_host[256];
        int sender_port;

        err = ptk_udp_socket_recv(g_udp_socket, &response_buf, sender_host, &sender_port);

        if(err == PTK_OK) {
            g_responses_received++;
            parse_list_identity_response(&response_buf, sender_host, sender_port);
        } else if(err == PTK_ERR_ABORT) {
            printf("Discovery aborted\n");
            break;
        } else if(err == PTK_ERR_WOULD_BLOCK || err == PTK_ERR_TIMEOUT) {
            // No data available, continue
            usleep(100000);  // Sleep 100ms
        } else {
            printf("Receive error: %s\n", ptk_err_to_string(err));
            usleep(100000);  // Sleep 100ms before retry
        }
    }

    // Cleanup
    if(networks) { ptk_socket_network_info_dispose(networks, num_networks); }

    printf("Discovery thread ending\n");
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    printf("EtherNet/IP Device Discovery Tool\n");
    printf("Using Protocol Toolkit APIs\n\n");

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
    ptk_err err = ptk_thread_create(&g_discovery_thread, discovery_thread, &discovery_time);
    if(err != PTK_OK) {
        printf("Failed to create discovery thread: %s\n", ptk_err_to_string(err));
        return 1;
    }

    printf("Discovery started. Press Ctrl+C to stop early...\n\n");

    // Wait for discovery thread to complete
    err = ptk_thread_join(g_discovery_thread);
    if(err != PTK_OK) { printf("Error joining discovery thread: %s\n", ptk_err_to_string(err)); }

    // Cleanup
    if(g_udp_socket) { ptk_socket_close(g_udp_socket); }

    if(g_discovery_thread) { ptk_thread_destroy(g_discovery_thread); }

    printf("\n=== Discovery Summary ===\n");
    printf("Total devices found: %d\n", g_responses_received);
    printf("Discovery completed.\n");

    return 0;
}