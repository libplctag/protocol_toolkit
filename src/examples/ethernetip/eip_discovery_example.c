/**
 * EtherNet/IP Network Discovery Example
 * 
 * Shows how to use the core EtherNet/IP PDU functions for device discovery.
 * This follows the modbus pattern: simple PDU send/receive instead of complex APIs.
 */

#include "lib/include/ethernetip.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static volatile bool g_running = true;

static void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    g_running = false;
}

static void device_found_callback(const eip_list_identity_resp_t *resp, void *user_data) {
    int *device_count = (int *)user_data;
    (*device_count)++;
    
    printf("\n=== Device #%d Found ===\n", *device_count);
    printf("IP Address: %s:%d\n", resp->ip_address, resp->port);
    
    // Vendor information
    printf("Vendor ID: 0x%04X", resp->vendor_id);
    const char *vendor_name = eip_vendor_id_to_string(resp->vendor_id);
    if (vendor_name) {
        printf(" (%s)", vendor_name);
    }
    printf("\n");
    
    // Device type
    printf("Device Type: 0x%04X", resp->device_type);
    const char *device_type_name = eip_device_type_to_string(resp->device_type);
    if (device_type_name) {
        printf(" (%s)", device_type_name);
    }
    printf("\n");
    
    // Product information
    printf("Product Code: 0x%04X\n", resp->product_code);
    printf("Revision: %d.%d\n", resp->revision_major, resp->revision_minor);
    printf("Serial Number: 0x%08X\n", resp->serial_number);
    
    if (strlen(resp->product_name) > 0) {
        printf("Product Name: \"%s\"\n", resp->product_name);
    }
    
    // Device status (app-friendly parsed fields)
    printf("Status:");
    if (resp->owned) printf(" [OWNED]");
    if (resp->configured) printf(" [CONFIGURED]");
    if (resp->major_fault) printf(" [MAJOR_FAULT]");
    if (resp->minor_recoverable_fault) printf(" [MINOR_FAULT]");
    if (resp->major_unrecoverable_fault) printf(" [MAJOR_UNRECOVERABLE]");
    printf("\n");
    
    // Device state (app-friendly parsed fields)
    printf("State:");
    if (resp->operational) printf(" [OPERATIONAL]");
    if (resp->standby) printf(" [STANDBY]");
    if (resp->self_test_in_progress) printf(" [SELF_TEST]");
    if (resp->configuration_mode) printf(" [CONFIG_MODE]");
    if (resp->waiting_for_reset) printf(" [WAITING_RESET]");
    printf("\n");
    printf("Discovery Time: %lld ms ago\n", ptk_now_ms() - resp->discovery_timestamp_ms);
    printf("==========================\n");
}

/**
 * Discover EtherNet/IP devices using CORRECTED approach
 * 
 * Key insight: A single ListIdentity broadcast triggers hundreds of responses!
 * Devices use the response_time_range to randomize their reply timing.
 * 
 * WRONG approach (old): Send frequent broadcasts, short timeouts
 * CORRECT approach: Send ONE broadcast per network, wait FULL response time
 */
static void discover_devices(int response_time_range_ms) {
    printf("=== EtherNet/IP Device Discovery (CORRECTED) ===\n");
    printf("Response time range: %d ms\n", response_time_range_ms);
    printf("This will send ONE broadcast per network interface,\n");
    printf("then wait the FULL response time for all replies.\n\n");
    
    // Validate response time range (EtherNet/IP spec limit)
    if (response_time_range_ms > 2000) {
        printf("Warning: Response time range capped at 2000ms (EtherNet/IP spec limit)\n");
        response_time_range_ms = 2000;
    }
    
    int device_count = 0;
    
    printf("Starting discovery using convenience function...\n");
    int total_found = eip_discover_devices_simple(response_time_range_ms, 
                                                 device_found_callback, 
                                                 &device_count);
    
    printf("\n=== Discovery Summary ===\n");
    printf("Total devices found: %d\n", total_found);
    printf("Discovery completed.\n");
}

int main(int argc, char *argv[]) {
    printf("EtherNet/IP Device Discovery Example\n");
    printf("Using CORRECTED Discovery Pattern\n\n");
    
    // Parse command line - now it's response time range, not total discovery time
    int response_time_range = 1000; // Default 1000ms response window
    if (argc > 1) {
        response_time_range = atoi(argv[1]);
        if (response_time_range < 100) response_time_range = 1000;
        if (response_time_range > 2000) response_time_range = 2000; // EIP spec limit
    }
    
    printf("Usage: %s [response_time_range_ms]\n", argv[0]);
    printf("  response_time_range_ms: 100-2000ms (default: 1000ms)\n");
    printf("  This tells devices 'respond within 0 to X milliseconds'\n\n");
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Run discovery
    discover_devices(response_time_range);
    
    return 0;
}

/**
 * NETWORK DISCOVERY EXPLANATION:
 * 
 * Instead of a complex discovery API like the old code had, we now use
 * the simple modbus-style pattern:
 * 
 * 1. Create UDP connection: eip_client_connect_udp()
 * 2. Create ListIdentity PDU: eip_pdu_create_from_type()  
 * 3. Send broadcast: eip_pdu_send()
 * 4. Receive responses: eip_pdu_recv()
 * 5. Parse response data from app-friendly structure
 * 
 * This is much simpler and follows the proven modbus pattern exactly.
 * The PDU structures contain all the parsed, app-friendly data.
 * 
 * Benefits:
 * - Simple, consistent API
 * - Easy to understand and debug
 * - Follows established patterns
 * - Rich data structures with parsed fields
 * - Automatic memory management via ptk_alloc
 */