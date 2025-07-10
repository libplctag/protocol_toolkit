/**
 * @file ethernetip_usage_example.c
 * @brief Example demonstrating the clean EtherNet/IP API usage patterns
 *
 * This example shows how to use the EtherNet/IP library following the same
 * patterns as the Modbus reference implementation. All protocol internals
 * are hidden, and only application-level PDUs and data structures are exposed.
 */

#include "lib/include/ethernetip.h"
#include <ptk_alloc.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Example 1: Device Discovery
 * Discover EtherNet/IP devices on the local network
 */
void example_device_discovery() {
    printf("=== Device Discovery Example ===\n");

    // Create parent allocator for this operation
    void *parent = ptk_alloc_create_parent();

    // Discover devices on the network (broadcast)
    eip_discovery_result_t *result = NULL;
    ptk_err err = eip_discover_devices(parent, NULL, 5000, &result);

    if(err != PTK_OK) {
        printf("Discovery failed: %d\n", err);
        ptk_alloc_free(parent);
        return;
    }

    printf("Found %zu devices:\n", result->device_count);

    // Iterate through discovered devices
    for(size_t i = 0; i < result->device_count; i++) {
        eip_identity_response_t *device = eip_identity_array_get(result->devices, i);

        printf("Device %zu:\n", i + 1);
        printf("  IP Address: %s:%u\n", device->ip_address, device->port);
        printf("  Product: %s\n", device->product_name);
        printf("  Vendor ID: 0x%04x (%s)\n", device->vendor_id, eip_vendor_id_to_string(device->vendor_id));
        printf("  Device Type: 0x%04x (%s)\n", device->device_type, eip_device_type_to_string(device->device_type));
        printf("  Product Code: 0x%04x\n", device->product_code);
        printf("  Revision: %u.%u\n", device->revision_major, device->revision_minor);
        printf("  Serial: %u\n", device->serial_number);
        printf("  State: %s\n", eip_device_state_to_string(device->state));
        printf("  Status: %s%s%s%s\n", device->owned ? "OWNED " : "", device->configured ? "CONFIGURED " : "",
               device->minor_recoverable_fault ? "MINOR_FAULT " : "", device->major_recoverable_fault ? "MAJOR_FAULT " : "");
        printf("\n");
    }

    // Clean up (frees all child allocations automatically)
    ptk_alloc_free(parent);
}

/**
 * Example 2: Building CIP Paths
 * Shows how to construct CIP IOI paths using the new PDU-based API
 */
void example_cip_path_construction() {
    printf("=== CIP Path Construction Example ===\n");

    void *parent = ptk_alloc_create_parent();

    // Method 1: Parse from string (common case)
    cip_ioi_path_pdu_t *path1 = NULL;
    ptk_err err = cip_ioi_path_pdu_create_from_string(parent, "1,0", &path1);
    if(err == PTK_OK) {
        char path_str[256];
        cip_ioi_path_pdu_to_string(path1, path_str, sizeof(path_str));
        printf("Parsed path: %s\n", path_str);
    }

    // Method 2: Build programmatically using the PDU API
    cip_ioi_path_pdu_t *path2 = cip_ioi_path_pdu_create(parent);
    if(path2) {
        // Route to backplane slot 2
        cip_ioi_path_pdu_add_port(path2, 1);  // Backplane port
        cip_ioi_path_pdu_add_port(path2, 2);  // Slot 2

        // Access Identity Object (class 1), instance 1
        cip_ioi_path_pdu_add_class(path2, 0x01);     // Identity Object
        cip_ioi_path_pdu_add_instance(path2, 0x01);  // Instance 1

        char path_str[256];
        cip_ioi_path_pdu_to_string(path2, path_str, sizeof(path_str));
        printf("Built path: %s\n", path_str);
        printf("Segment count: %zu\n", cip_ioi_path_pdu_get_segment_count(path2));
        printf("Wire length: %zu bytes\n", cip_ioi_path_pdu_get_wire_length(path2));
    }

    // Method 3: Access tag by symbolic name
    cip_ioi_path_pdu_t *path3 = cip_ioi_path_pdu_create(parent);
    if(path3) {
        cip_ioi_path_pdu_add_port(path3, 1);            // Backplane
        cip_ioi_path_pdu_add_port(path3, 0);            // Slot 0 (controller)
        cip_ioi_path_pdu_add_symbolic(path3, "MyTag");  // Tag name

        char path_str[256];
        cip_ioi_path_pdu_to_string(path3, path_str, sizeof(path_str));
        printf("Tag access path: %s\n", path_str);
    }

    ptk_alloc_free(parent);
}

/**
 * Example 3: TCP Connection and Messaging
 * Shows how to establish a connection and send messages
 */
void example_tcp_messaging() {
    printf("=== TCP Messaging Example ===\n");

    void *parent = ptk_alloc_create_parent();

    // Create CIP path for routing (to slot 0 - controller)
    cip_ioi_path_pdu_t *path = NULL;
    ptk_err err = cip_ioi_path_pdu_create_from_string(parent, "1,0", &path);
    if(err != PTK_OK) {
        printf("Failed to create CIP path\n");
        ptk_alloc_free(parent);
        return;
    }

    // Connect to PLC (session management handled internally)
    eip_connection *conn = eip_client_connect_tcp(parent, "192.168.1.100", 44818, path->segments);
    if(!conn) {
        printf("Failed to connect to PLC\n");
        ptk_alloc_free(parent);
        return;
    }

    printf("Connected to PLC at %s:%u\n", conn->host, conn->port);

    // Send List Identity request (example of messaging)
    eip_list_identity_req_t *req = NULL;
    err = eip_list_identity_req_create(conn, &req);
    if(err == PTK_OK) {
        err = eip_list_identity_req_send(conn, req, 5000);
        if(err == PTK_OK) { printf("List Identity request sent successfully\n"); }
    }

    // Receive any message (type-safe dispatch)
    eip_apu_base_t *received_apu = NULL;
    err = eip_apu_recv(conn, &received_apu, 5000);
    if(err == PTK_OK && received_apu) {
        // Type-safe message dispatch based on APU type
        switch(received_apu->apu_type) {
            case EIP_LIST_IDENTITY_RESP_TYPE: {
                eip_list_identity_resp_t *resp = (eip_list_identity_resp_t *)received_apu;
                printf("Received List Identity Response:\n");
                printf("  Product: %s\n", resp->identity.product_name);
                printf("  IP: %s\n", resp->identity.ip_address);
                break;
            }
            default: printf("Received unknown message type: %zu\n", received_apu->apu_type); break;
        }
    }

    // Close connection (cleanup handled automatically)
    eip_close(conn);
    ptk_alloc_free(parent);
}

/**
 * Example 4: UDP Discovery with Specific Network Interface
 */
void example_targeted_discovery() {
    printf("=== Targeted Discovery Example ===\n");

    void *parent = ptk_alloc_create_parent();

    // Discover devices on specific subnet
    eip_discovery_result_t *result = NULL;
    ptk_err err = eip_discover_devices_subnet(parent, "192.168.1.0/24", 3000, &result);

    if(err == PTK_OK && result->device_count > 0) {
        printf("Found %zu devices on 192.168.1.0/24 in %lu ms:\n", result->device_count, result->discovery_time_ms);

        // Show just the essential info for each device
        for(size_t i = 0; i < result->device_count; i++) {
            eip_identity_response_t *device = eip_identity_array_get(result->devices, i);
            printf("  %s - %s (%s)\n", device->ip_address, device->product_name, eip_device_state_to_string(device->state));
        }
    } else {
        printf("No devices found or discovery failed\n");
    }

    ptk_alloc_free(parent);
}

int main() {
    printf("EtherNet/IP Protocol Toolkit Usage Examples\n");
    printf("===========================================\n\n");

    // Run all examples
    example_device_discovery();
    printf("\n");

    example_cip_path_construction();
    printf("\n");

    example_tcp_messaging();
    printf("\n");

    example_targeted_discovery();

    return 0;
}
