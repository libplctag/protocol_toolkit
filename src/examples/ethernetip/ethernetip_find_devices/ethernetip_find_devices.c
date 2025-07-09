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
// IDENTITY OBJECT DECODING HELPERS
//=============================================================================

/**
 * Get vendor name from vendor ID (partial list of common vendors)
 */
static const char *get_vendor_name(uint16_t vendor_id) {
    switch(vendor_id) {
        case 0x0001: return "Rockwell Automation/Allen-Bradley";
        case 0x0002: return "Namco";
        case 0x0003: return "Honeywell";
        case 0x0004: return "Parker Hannifin";
        case 0x0005: return "Rockwell Software";
        case 0x0006: return "A-B Quality";
        case 0x0007: return "Hayworth";
        case 0x0008: return "Barber-Colman";
        case 0x0009: return "Minnesota Mining & Manufacturing";
        case 0x000A: return "Parametric Technology Corp";
        case 0x000B: return "Control Technology Inc";
        case 0x000C: return "Schneider Electric";
        case 0x000D: return "Woodhead Software & Electronics";
        case 0x000E: return "Bradshaw Electric";
        case 0x000F: return "Adagio";
        case 0x0010: return "Super Radiator Coils";
        case 0x0011: return "Reserved";
        case 0x0012: return "Watlow";
        case 0x0013: return "SBS";
        case 0x0014: return "Hewlett Packard";
        case 0x0015: return "Maig";
        case 0x0016: return "Staubli";
        case 0x0017: return "Advantech";
        case 0x0018: return "Inexta";
        case 0x0019: return "Acromag";
        case 0x001A: return "Hilscher";
        case 0x001B: return "IXXAT";
        case 0x001C: return "Phoenix Contact";
        case 0x001D: return "ICP DAS";
        case 0x001E: return "Klinkmann";
        case 0x001F: return "Cogent ChipWare";
        // Add more as needed...
        default: return NULL;
    }
}

/**
 * Get device type name from device type ID (partial list)
 */
static const char *get_device_type_name(uint16_t device_type) {
    switch(device_type) {
        case 0x00: return "Generic Device";
        case 0x02: return "AC Drive";
        case 0x03: return "Motor Overload";
        case 0x04: return "Limit Switch";
        case 0x05: return "Inductive Proximity Switch";
        case 0x06: return "Photoelectric Switch";
        case 0x07: return "General Purpose Discrete I/O";
        case 0x08: return "Resolver";
        case 0x09: return "General Purpose Analog I/O";
        case 0x0A: return "Generic Data Server";
        case 0x0B: return "DeviceNet to ControlNet Gateway";
        case 0x0C: return "Communications Adapter";
        case 0x0D: return "Programmable Logic Controller";
        case 0x0E: return "Position Controller";
        case 0x10: return "DC Drive";
        case 0x13: return "Vacuum/Pressure Switch";
        case 0x15: return "Process Control Value";
        case 0x16: return "Residual Gas Analyzer";
        case 0x1A: return "DC Power Generator";
        case 0x1B: return "RF Power Generator";
        case 0x1C: return "Turbomolecular Vacuum Pump";
        case 0x1D: return "Analysis Equipment";
        case 0x22: return "Pneumatic Valve";
        case 0x23: return "Process Instrument";
        // Add more as needed...
        default: return NULL;
    }
}

/**
 * Decode device status word bits
 */
static void decode_device_status(uint16_t status) {
    printf(" (");

    // Bit 0: Owned
    if(status & 0x0001) { printf("Owned "); }

    // Bit 1: Reserved

    // Bit 2-3: Configured
    uint8_t configured = (status >> 2) & 0x03;
    switch(configured) {
        case 0: printf("NotConfigured "); break;
        case 1: printf("Configured "); break;
        case 2: printf("ConfigInvalid "); break;
        case 3: printf("ConfigReserved "); break;
    }

    // Bit 4-7: Extended Device Status
    uint8_t ext_status = (status >> 4) & 0x0F;
    if(ext_status != 0) { printf("ExtStatus:0x%X ", ext_status); }

    // Bit 8-11: Minor Recoverable Fault
    if(status & 0x0100) { printf("MinorRecoverableFault "); }
    if(status & 0x0200) { printf("MinorUnrecoverableFault "); }
    if(status & 0x0400) { printf("MajorRecoverableFault "); }
    if(status & 0x0800) { printf("MajorUnrecoverableFault "); }

    // Bit 12-15: Reserved

    printf(")");
}


//=============================================================================
// ETHERNETIP PROTOCOL FUNCTIONS
//=============================================================================

/**
 * Build EtherNet/IP List Identity request packet
 */
static ptk_err build_list_identity_request(ptk_buf *buffer) {
    return ptk_buf_serialize(buffer, PTK_BUF_LITTLE_ENDIAN,
                             (uint16_t)EIP_LIST_IDENTITY_CMD,  // Command
                             (uint16_t)0,                      // Length
                             (uint32_t)0,                      // Session Handle
                             (uint32_t)0,                      // Status
                             (uint64_t)0,                      // Sender Context
                             (uint32_t)0);                     // Options
}

/**
 * Parse EtherNet/IP List Identity response
 */
static ptk_err parse_list_identity_response(ptk_buf *buffer, const ptk_address_t *sender_addr) {
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

    err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &command, &length, &session_handle, &status, &sender_context,
                              &options);
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
        err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &item_count);
        if(err != PTK_OK) { return err; }

        printf("CPF Items: %u\n", item_count);

        for(uint16_t i = 0; i < item_count; i++) {
            uint16_t type_id, item_length;

            err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &type_id, &item_length);
            if(err != PTK_OK) { return err; }

            printf("  Item %u: Type 0x%04X, Length %u\n", i + 1, type_id, item_length);

            if(type_id == CPF_TYPE_CIP_IDENTITY && item_length >= 34) {
                // Parse CIP Identity data using type-safe deserialization
                uint16_t vendor_id, device_type, product_code;
                uint8_t major_rev, minor_rev;
                uint16_t status_word;
                uint32_t serial_number;

                // Deserialize the fixed-size fields using the new type-safe API
                err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN,
                                          &vendor_id,     // UINT (2 bytes) - Vendor ID
                                          &device_type,   // UINT (2 bytes) - Device Type
                                          &product_code,  // UINT (2 bytes) - Product Code
                                          &major_rev,     // USINT (1 byte) - Major Revision
                                          &minor_rev,     // USINT (1 byte) - Minor Revision
                                          &status_word,   // WORD (2 bytes) - Status
                                          &serial_number  // UDINT (4 bytes) - Serial Number
                );

                if(err != PTK_OK) {
                    printf("    Error parsing identity data: %d\n", err);
                    break;
                }

                // Display the decoded identity information
                printf("    === Device Identity ===\n");
                printf("    Vendor ID: 0x%04X", vendor_id);
                const char *vendor_name = get_vendor_name(vendor_id);
                if(vendor_name) { printf(" (%s)", vendor_name); }
                printf("\n");

                printf("    Device Type: 0x%04X", device_type);
                const char *device_type_name = get_device_type_name(device_type);
                if(device_type_name) { printf(" (%s)", device_type_name); }
                printf("\n");

                printf("    Product Code: 0x%04X\n", product_code);
                printf("    Revision: %u.%u\n", major_rev, minor_rev);

                printf("    Status: 0x%04X", status_word);
                decode_device_status(status_word);
                printf("\n");

                printf("    Serial Number: %u (0x%08X)\n", serial_number, serial_number);

                // Parse product name (SHORT_STRING) - it's located after socket address info
                // The structure after the 14-byte identity is: SocketAddr(variable) + ProductName(SHORT_STRING)
                size_t remaining_bytes = item_length - 14;  // 14 bytes for the fixed identity fields

                // Search for the product name string in the remaining data
                // Look for a length byte followed by printable ASCII characters
                uint8_t *current_ptr = ptk_buf_get_start_ptr(buffer);
                size_t available = ptk_buf_len(buffer);
                bool found_name = false;

                for(size_t offset = 0; offset < available && offset < remaining_bytes; offset++) {
                    uint8_t potential_len = current_ptr[offset];
                    // Check for reasonable string length and sufficient remaining data
                    if(potential_len > 0 && potential_len < 64 && (offset + potential_len + 1) <= available) {
                        // Verify the following bytes look like printable ASCII
                        bool looks_like_string = true;
                        for(uint8_t i = 1; i <= potential_len; i++) {
                            uint8_t c = current_ptr[offset + i];
                            if(c < 0x20 || c > 0x7E) {
                                looks_like_string = false;
                                break;
                            }
                        }

                        if(looks_like_string) {
                            char product_name[65] = {0};
                            memcpy(product_name, &current_ptr[offset + 1], potential_len);
                            product_name[potential_len] = '\0';
                            printf("    Product Name: \"%s\"\n", product_name);
                            found_name = true;

                            // Skip past the entire remaining data for this item
                            size_t current_start = ptk_buf_get_start(buffer);
                            ptk_buf_set_start(buffer, current_start + remaining_bytes);
                            break;
                        }
                    }
                }

                if(!found_name) {
                    printf("    Product Name: <not found>\n");
                    // Skip all remaining bytes for this item
                    size_t current_start = ptk_buf_get_start(buffer);
                    ptk_buf_set_start(buffer, current_start + remaining_bytes);
                }
            } else if(type_id == CPF_TYPE_SOCKET_ADDR && item_length >= 16) {
                // Parse Socket Address (network byte order)
                uint16_t sin_family, sin_port;
                uint32_t sin_addr;
                uint8_t padding[8];

                err = ptk_buf_deserialize(buffer, false, PTK_BUF_BIG_ENDIAN, &sin_family, &sin_port, &sin_addr);
                if(err != PTK_OK) { break; }

                // Skip 8 bytes of padding
                for(int j = 0; j < 8; j++) {
                    err = ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &padding[j]);
                    if(err != PTK_OK) { break; }
                }
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
            ptk_buf *request_buf = ptk_buf_create(g_allocator, 32);
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
        ptk_buf *response_buf = ptk_buf_create(g_allocator, 512);
        if(!response_buf) {
            printf("Failed to create response buffer\n");
            usleep(100000);
            continue;
        }

        ptk_address_t sender_addr;
        err = ptk_udp_socket_recv_from(g_udp_socket, response_buf, &sender_addr);

        if(err == PTK_OK) {
            g_responses_received++;
            char *sender_ip = ptk_address_to_string(g_allocator, &sender_addr);
            error("Received response from %s:%d", sender_ip ? sender_ip : "unknown", ptk_address_get_port(&sender_addr));
            if(sender_ip) { ptk_free(g_allocator, sender_ip); }
            error_buf(response_buf);

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
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);

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