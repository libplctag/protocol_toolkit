/**
 * EtherNet/IP Device Discovery Tool - Simplified Version
 *
 * This tool discovers EtherNet/IP devices on the network by sending List Identity
 * broadcasts and processing responses. Uses the Protocol Toolkit APIs with
 * structured protocol definitions and type-safe serialization.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ptk_mem.h"
#include "ptk_buf.h"
#include "ptk_err.h"
#include "ptk_sock.h"
#include "ptk_utils.h"

//=============================================================================
// ETHERNETIP PROTOCOL CONSTANTS AND STRUCTURES
//=============================================================================

#define EIP_PORT 44818
#define EIP_LIST_IDENTITY_CMD 0x0063

// Common Packet Format (CPF) Item Types
#define CPF_TYPE_NULL 0x0000
#define CPF_TYPE_CIP_IDENTITY 0x000C
#define CPF_TYPE_SOCKET_ADDR 0x8000

// EtherNet/IP Encapsulation Header
typedef struct __attribute__((packed)) {
    uint16_t command;         // EIP command (ListIdentity=0x0063)
    uint16_t length;          // Length of data following this header
    uint32_t session_handle;  // Session identifier (0 for unregistered)
    uint32_t status;          // Status code (0 = success)
    uint64_t sender_context;  // Echo data for request/response matching
    uint32_t options;         // Options flags (typically 0)
} eip_encap_header_t;

// CIP Identity Item Structure
typedef struct __attribute__((packed)) {
    uint16_t item_type;      // 0x000C for CIP Identity
    uint16_t item_length;    // Length of following data
    uint16_t encap_version;  // Encapsulation protocol version
    int16_t sin_family;      // Address family (big-endian)
    uint16_t sin_port;       // Port number (big-endian)
    uint8_t sin_addr[4];     // IP address (network byte order)
    uint8_t sin_zero[8];     // Padding
    uint16_t vendor_id;      // Vendor ID (little-endian)
    uint16_t device_type;    // Device type (little-endian)
    uint16_t product_code;   // Product code (little-endian)
    uint8_t revision_major;  // Major revision
    uint8_t revision_minor;  // Minor revision
    uint16_t status;         // Device status
    uint32_t serial_number;  // Serial number
    // Variable length product name and state follow
} cip_identity_item_t;

// Configuration Structure
typedef struct {
    int discovery_time_seconds;
    int broadcast_interval_ms;
    int response_timeout_ms;
    uint16_t eip_port;
    bool verbose_output;
} discovery_config_t;

// Error handling macro
#define CHECK_PTK_ERR(call)             \
    do {                                \
        ptk_err_t _err = (call);          \
        if(_err != PTK_OK) return _err; \
    } while(0)

//=============================================================================
// GLOBAL STATE
//=============================================================================

static volatile bool g_running = true;
static int g_responses_received = 0;
static ptk_sock *g_udp_socket = NULL;
static ptk_thread *g_discovery_thread = NULL;
static discovery_config_t g_config = {.discovery_time_seconds = 30,
                                      .broadcast_interval_ms = 5000,
                                      .response_timeout_ms = 500,
                                      .eip_port = EIP_PORT,
                                      .verbose_output = false};

//=============================================================================
// SIGNAL HANDLING
//=============================================================================

static void signal_handler(void) {
    printf("\nReceived signal, shutting down...\n");
    g_running = false;
    if(g_udp_socket) { ptk_socket_abort(g_udp_socket); }
}

//=============================================================================
// CIP PROTOCOL LOOKUP TABLES
//=============================================================================

/**
 * CIP Vendor ID Registry - Most common vendors
 */
typedef struct {
    uint16_t vendor_id;
    const char *vendor_name;
} cip_vendor_t;

static const cip_vendor_t CIP_VENDORS[] = {
    {1, "Rockwell Automation/Allen-Bradley"},
    {47, "OMRON Corporation"},
    {8, "Molex Incorporated"},
    {26, "Festo SE & Co KG"},
    {29, "OPTO 22"},
    {40, "WAGO Corporation"},
    {108, "Beckhoff Automation"},
    {252, "OMRON Software Co., Ltd."},
    {678, "Cognex Corporation"},
    {808, "SICK AG"},
    {1988, "Unitronics (1989) (RG) LTD"},
    {0, NULL}  // End marker
};

/**
 * CIP Device Type Registry - Common device types
 */
typedef struct {
    uint16_t device_type;
    const char *device_name;
} cip_device_type_t;

static const cip_device_type_t CIP_DEVICE_TYPES[] = {
    {0x00, "Generic Device"},      {0x02, "AC Drive"}, {0x0C, "Communications Adapter"},  {0x0E, "Programmable Logic Controller"},
    {0x10, "Position Controller"}, {0x13, "DC Drive"}, {0x18, "Human-Machine Interface"}, {0x25, "CIP Motion Drive"},
    {0x2C, "Managed Switch"},      {0, NULL}  // End marker
};

//=============================================================================
// PROTOCOL HELPER FUNCTIONS
//=============================================================================

/**
 * Get vendor name from vendor ID
 */
static const char *cip_get_vendor_name(uint16_t vendor_id) {
    for(const cip_vendor_t *vendor = CIP_VENDORS; vendor->vendor_name != NULL; vendor++) {
        if(vendor->vendor_id == vendor_id) { return vendor->vendor_name; }
    }
    return NULL;
}

/**
 * Get device type name from device type ID
 */
static const char *cip_get_device_type_name(uint16_t device_type) {
    for(const cip_device_type_t *type = CIP_DEVICE_TYPES; type->device_name != NULL; type++) {
        if(type->device_type == device_type) { return type->device_name; }
    }
    return NULL;
}

/**
 * Get device state name from state value
 */
static const char *cip_get_device_state_name(uint8_t state) {
    switch(state) {
        case 0x00: return "Nonexistent";
        case 0x01: return "Self Testing";
        case 0x02: return "Standby";
        case 0x03: return "Operational";
        case 0x04: return "Major Recoverable Fault";
        case 0x05: return "Major Unrecoverable Fault";
        default: return "Unknown";
    }
}

/**
 * Parse SHORT_STRING from buffer
 */
static ptk_err_t parse_short_string(ptk_buf *buffer, char *output, size_t max_len) {
    uint8_t length;
    CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &length));

    if(length >= max_len) { return PTK_ERR_BUFFER_TOO_SMALL; }

    for(uint8_t i = 0; i < length; i++) { CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &output[i])); }
    output[length] = '\0';

    return PTK_OK;
}

//=============================================================================
// ETHERNETIP PROTOCOL FUNCTIONS
//=============================================================================

/**
 * Build EtherNet/IP List Identity request packet using type-safe serialization
 */
static ptk_err_t build_list_identity_request(ptk_buf *buffer) {
    return ptk_buf_serialize(buffer, PTK_BUF_LITTLE_ENDIAN,
                             (uint16_t)EIP_LIST_IDENTITY_CMD,  // Command
                             (uint16_t)0,                      // Length
                             (uint32_t)0,                      // Session Handle
                             (uint32_t)0,                      // Status
                             (uint64_t)1000,                   // Sender Context
                             (uint32_t)0);                     // Options
}

/**
 * Parse EIP encapsulation header
 */
static ptk_err_t parse_eip_header(ptk_buf *buffer, eip_encap_header_t *header) {
    return ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &header->command, &header->length, &header->session_handle,
                               &header->status, &header->sender_context, &header->options);
}

/**
 * Parse CPF (Common Packet Format) header
 */
static ptk_err_t parse_cpf_header(ptk_buf *buffer, uint16_t *item_count) {
    return ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, item_count);
}

/**
 * Parse CIP Identity item structure
 */
static ptk_err_t parse_cip_identity_item(ptk_buf *buffer, cip_identity_item_t *identity, uint16_t item_length) {
    if(item_length < 34) {  // Minimum size for identity item
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    // Parse fixed-size fields
    CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &identity->encap_version));
    CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_BIG_ENDIAN, &identity->sin_family, &identity->sin_port));

    // Parse IP address bytes
    for(int i = 0; i < 4; i++) {
        CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &identity->sin_addr[i]));
    }

    // Parse padding
    for(int i = 0; i < 8; i++) {
        CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &identity->sin_zero[i]));
    }

    // Parse device identity fields
    CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &identity->vendor_id, &identity->device_type,
                                      &identity->product_code, &identity->revision_major, &identity->revision_minor));

    // Parse optional fields if present
    size_t remaining = item_length - 34;
    if(remaining >= 6) {
        CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &identity->status, &identity->serial_number));
    }

    return PTK_OK;
}

/**
 * Display device information in a formatted way
 */
static void display_device_info(const cip_identity_item_t *identity, const char *product_name, const char *sender_ip,
                                uint16_t sender_port, uint8_t device_state) {
    printf("\n=== EtherNet/IP Device Found ===\n");
    printf("From: %s:%d\n", sender_ip ? sender_ip : "unknown", sender_port);

    // Socket address info
    printf("Socket Address: %u.%u.%u.%u:%u\n", identity->sin_addr[0], identity->sin_addr[1], identity->sin_addr[2],
           identity->sin_addr[3], identity->sin_port);

    // Device identity
    printf("=== Device Identity ===\n");
    printf("Vendor ID: 0x%04X", identity->vendor_id);
    const char *vendor_name = cip_get_vendor_name(identity->vendor_id);
    if(vendor_name) { printf(" (%s)", vendor_name); }
    printf("\n");

    printf("Device Type: 0x%04X", identity->device_type);
    const char *device_type_name = cip_get_device_type_name(identity->device_type);
    if(device_type_name) { printf(" (%s)", device_type_name); }
    printf("\n");

    printf("Product Code: 0x%04X\n", identity->product_code);
    printf("Revision: %u.%u\n", identity->revision_major, identity->revision_minor);
    printf("Status: 0x%04X\n", identity->status);
    printf("Serial Number: 0x%08X\n", identity->serial_number);

    if(product_name && strlen(product_name) > 0) { printf("Product Name: \"%s\"\n", product_name); }

    printf("Device State: %s\n", cip_get_device_state_name(device_state));
    printf("================================\n");
}

/**
 * Broadcast to all network interfaces
 */
static ptk_err_t broadcast_to_all_networks(ptk_sock *socket, ptk_buf *request_buf, ptk_network_info *networks) {
    size_t num_networks = ptk_socket_network_info_count(networks);
    bool broadcast_sent = false;

    // Create buffer array with single buffer for sending
    ptk_buf_array_t *buf_array = ptk_buf_array_create(1, NULL);
    if (!buf_array) {
        return PTK_ERR_NO_RESOURCES;
    }

    for(size_t i = 0; i < num_networks; i++) {
        const ptk_network_info_entry *entry = ptk_socket_network_info_get(networks, i);
        if(entry && entry->broadcast) {
            // Reset buffer for each send
            ptk_buf_set_start(request_buf, 0);
            ptk_buf_set_end(request_buf, 0);
            build_list_identity_request(request_buf);

            // Reset buffer array and add the request buffer
            ptk_buf_array_resize(buf_array, 1);
            ptk_buf_array_set(buf_array, 0, *request_buf);

            ptk_address_t broadcast_addr;
            if(ptk_address_create(&broadcast_addr, entry->broadcast, g_config.eip_port) == PTK_OK) {
                if(ptk_udp_socket_send_to(socket, buf_array, &broadcast_addr, true, 1000) == PTK_OK) {
                    broadcast_sent = true;
                    if(g_config.verbose_output) { printf("Broadcast sent to %s:%d\n", entry->broadcast, g_config.eip_port); }
                }
            }
        }
    }

    ptk_free(&buf_array);
    return broadcast_sent ? PTK_OK : PTK_ERR_NETWORK_ERROR;
}

/**
 * Parse EtherNet/IP List Identity response using structured approach
 */
static ptk_err_t parse_list_identity_response(ptk_buf *buffer, const ptk_address_t *sender_addr) {
    eip_encap_header_t header;

    // Parse EIP header
    CHECK_PTK_ERR(parse_eip_header(buffer, &header));

    if(header.command != EIP_LIST_IDENTITY_CMD) {
        if(g_config.verbose_output) { printf("Warning: Unexpected command 0x%04X\n", header.command); }
        return PTK_ERR_PROTOCOL_ERROR;
    }

    if(header.status != 0) {
        if(g_config.verbose_output) { printf("Error: Non-zero status 0x%08X\n", header.status); }
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Parse CPF data if present
    if(header.length > 0) {
        uint16_t item_count;
        CHECK_PTK_ERR(parse_cpf_header(buffer, &item_count));

        char *sender_ip = ptk_address_to_string(sender_addr);
        uint16_t sender_port = ptk_address_get_port(sender_addr);

        for(uint16_t i = 0; i < item_count; i++) {
            uint16_t type_id, item_length;
            CHECK_PTK_ERR(ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &type_id, &item_length));

            if(type_id == CPF_TYPE_CIP_IDENTITY && item_length >= 34) {
                cip_identity_item_t identity = {0};
                identity.item_type = type_id;
                identity.item_length = item_length;

                CHECK_PTK_ERR(parse_cip_identity_item(buffer, &identity, item_length));

                // Parse product name if present
                char product_name[256] = {0};
                size_t parsed_bytes = 34;             // Fixed part of identity
                if(item_length > parsed_bytes + 6) {  // status + serial + at least 1 byte for name
                    parsed_bytes += 6;                // status + serial
                    if(item_length > parsed_bytes) {
                        ptk_err_t name_err = parse_short_string(buffer, product_name, sizeof(product_name));
                        if(name_err == PTK_OK) {
                            parsed_bytes += strlen(product_name) + 1;  // +1 for length byte
                        }
                    }
                }

                // Parse device state if present
                uint8_t device_state = 0;
                if(item_length > parsed_bytes) { ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &device_state); }

                display_device_info(&identity, product_name, sender_ip, sender_port, device_state);
            } else {
                // Skip unknown item types
                size_t current_start = ptk_buf_get_start(buffer);
                ptk_buf_set_start(buffer, current_start + item_length);
            }
        }

        if(sender_ip) { ptk_free(&sender_ip); }
    }

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
    ptk_network_info *networks = ptk_socket_find_networks();
    size_t num_networks = 0;

    if(!networks) {
        printf("Warning: Failed to discover networks, using fallback broadcast\n");
    } else {
        num_networks = ptk_socket_network_info_count(networks);
        if(g_config.verbose_output) {
            printf("Discovered %zu network interface(s):\n", num_networks);
            for(size_t i = 0; i < num_networks; i++) {
                const ptk_network_info_entry *entry = ptk_socket_network_info_get(networks, i);
                if(entry) { printf("  %zu: %s/%s -> %s\n", i + 1, entry->network_ip, entry->netmask, entry->broadcast); }
            }
        }
    }

    // Create local address for UDP socket
    ptk_address_t local_addr;
    ptk_err_t err = ptk_address_create_any(&local_addr, 0);
    if(err != PTK_OK) {
        printf("Failed to create local address: %s\n", ptk_err_to_string(err));
        if(networks) { ptk_socket_network_info_dispose(networks); }
        return;
    }

    // Create UDP socket for discovery
    g_udp_socket = ptk_udp_socket_create(&local_addr, true); // Enable broadcast
    if(!g_udp_socket) {
        printf("Failed to create UDP socket: %s\n", ptk_err_to_string(ptk_get_err()));
        if(networks) { ptk_free(&networks); }
        return;
    }

    if(g_config.verbose_output) { printf("UDP socket created for discovery\n"); }

    // Broadcast interval
    ptk_time_ms last_broadcast = 0;

    while(g_running && ptk_now_ms() < end_time) {
        ptk_time_ms current_time = ptk_now_ms();

        // Send broadcast if it's time
        if(current_time - last_broadcast >= g_config.broadcast_interval_ms) {
            ptk_buf *request_buf = ptk_buf_create(g_allocator, 32);
            if(!request_buf) {
                printf("Failed to create request buffer\n");
                break;
            }

            err = build_list_identity_request(request_buf);
            if(err == PTK_OK) {
                bool broadcast_sent = false;

                // Send to all discovered networks
                if(networks && num_networks > 0) {
                    ptk_err_t broadcast_err = broadcast_to_all_networks(g_udp_socket, request_buf, networks);
                    if(broadcast_err == PTK_OK) { broadcast_sent = true; }
                }

                // Fallback broadcast if no networks or no broadcasts were sent
                if(!broadcast_sent) {
                    ptk_buf_set_start(request_buf, 0);
                    ptk_buf_set_end(request_buf, 0);
                    build_list_identity_request(request_buf);

                    // Create buffer array for fallback send
                    ptk_buf_array_t *buf_array = ptk_buf_array_create(1, NULL);
                    if (buf_array) {
                        ptk_buf_array_set(buf_array, 0, *request_buf);
                        
                        ptk_address_t broadcast_addr;
                        err = ptk_address_create(&broadcast_addr, "255.255.255.255", g_config.eip_port);
                        if(err == PTK_OK) {
                            err = ptk_udp_socket_send_to(g_udp_socket, buf_array, &broadcast_addr, true, 1000);
                            if(err == PTK_OK && g_config.verbose_output) {
                                printf("Sent fallback broadcast to 255.255.255.255:%d\n", g_config.eip_port);
                            }
                        }
                        ptk_free(&buf_array);
                    }
                }
            }

            ptk_buf_dispose(request_buf);
            last_broadcast = current_time;
        }

        // Listen for responses
        ptk_address_t sender_addr;
        ptk_buf_array_t *response_buffers = ptk_udp_socket_recv_from(g_udp_socket, &sender_addr, false, g_config.response_timeout_ms);

        if(response_buffers) {
            size_t num_packets = ptk_buf_array_len(response_buffers);
            for(size_t i = 0; i < num_packets; i++) {
                ptk_buf *response_buf;
                if(ptk_buf_array_get(response_buffers, i, &response_buf) == PTK_OK) {
                    g_responses_received++;
                    parse_list_identity_response(response_buf, &sender_addr);
                }
            }
            ptk_free(&response_buffers);
        } else {
            ptk_err_t recv_err = ptk_get_err();
            if(recv_err == PTK_ERR_ABORT) {
                if(g_config.verbose_output) { printf("Discovery aborted\n"); }
                break;
            } else if(recv_err != PTK_ERR_TIMEOUT && g_config.verbose_output) {
                printf("Receive error: %s\n", ptk_err_to_string(recv_err));
            }
        }

        usleep(100000);  // Sleep 100ms
    }

    // Cleanup
    if(networks) { ptk_free(&networks); }

    if(g_config.verbose_output) { printf("Discovery thread ending\n"); }
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);

    printf("EtherNet/IP Device Discovery Tool - Simplified Version\n");
    printf("Using Protocol Toolkit APIs with structured protocol definitions\n\n");

    // Create allocator
    g_allocator = allocator_default_create(8);
    if(!g_allocator) {
        printf("Failed to create allocator\n");
        return 1;
    }

    // Parse command line arguments
    if(argc > 1) {
        g_config.discovery_time_seconds = atoi(argv[1]);
        if(g_config.discovery_time_seconds < 1) { g_config.discovery_time_seconds = 30; }
    }

    if(argc > 2) { g_config.verbose_output = (strcmp(argv[2], "-v") == 0); }

    printf("Configuration:\n");
    printf("  Discovery time: %d seconds\n", g_config.discovery_time_seconds);
    printf("  EtherNet/IP Port: %d\n", g_config.eip_port);
    printf("  Broadcast interval: %d ms\n", g_config.broadcast_interval_ms);
    printf("  Response timeout: %d ms\n", g_config.response_timeout_ms);
    printf("  Verbose output: %s\n\n", g_config.verbose_output ? "enabled" : "disabled");

    // Set up signal handling
    ptk_set_interrupt_handler(signal_handler);

    // Start discovery thread
    g_discovery_thread = ptk_thread_create(g_allocator, discovery_thread, &g_config.discovery_time_seconds);
    if(!g_discovery_thread) {
        printf("Failed to create discovery thread\n");
        ptk_allocator_destroy(g_allocator);
        return 1;
    }

    printf("Discovery started. Press Ctrl+C to stop early...\n\n");

    // Wait for discovery thread to complete
    ptk_err_t err = ptk_thread_join(g_discovery_thread);
    if(err != PTK_OK) { printf("Error joining discovery thread: %s\n", ptk_err_to_string(err)); }

    // Cleanup
    if(g_udp_socket) { ptk_free(&g_udp_socket); }
    if(g_discovery_thread) { ptk_free(&g_discovery_thread); }
    if(g_allocator) { ptk_free(&g_allocator); }

    printf("\n=== Discovery Summary ===\n");
    printf("Total devices found: %d\n", g_responses_received);
    printf("Discovery completed.\n");

    return 0;
}
