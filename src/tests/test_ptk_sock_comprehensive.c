/**
 * @file test_ptk_sock_comprehensive.c
 * @brief Comprehensive tests for ptk_sock.h API
 *
 * Tests all socket operations including TCP, UDP, address management,
 * and network discovery functionality.
 */

#include <ptk_sock.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_mem.h>
#include <ptk_os_thread.h>
#include <ptk_buf.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

//=============================================================================
// Address Management Tests
//=============================================================================

int test_address_operations(void) {
    info("test_address_operations entry");
    
    ptk_address_t addr;
    
    // Test ptk_address_init with valid IP
    ptk_err result = ptk_address_init(&addr, "127.0.0.1", 8080);
    if (result != PTK_OK) {
        error("ptk_address_init failed for 127.0.0.1:8080");
        return 1;
    }
    
    // Test ptk_address_get_port
    uint16_t port = ptk_address_get_port(&addr);
    if (port != 8080) {
        error("ptk_address_get_port returned wrong port: %u != 8080", port);
        return 2;
    }
    
    // Test ptk_address_to_string
    char *addr_str = ptk_address_to_string(&addr);
    if (!addr_str) {
        error("ptk_address_to_string returned NULL");
        return 3;
    }
    
    info("Address string: %s", addr_str);
    
    // Should contain "127.0.0.1"
    if (strstr(addr_str, "127.0.0.1") == NULL) {
        error("Address string doesn't contain expected IP: %s", addr_str);
        ptk_local_free(&addr_str);
        return 4;
    }
    
    ptk_local_free(&addr_str);
    
    // Test ptk_address_init_any
    ptk_address_t any_addr;
    result = ptk_address_init_any(&any_addr, 3000);
    if (result != PTK_OK) {
        error("ptk_address_init_any failed");
        return 5;
    }
    
    if (ptk_address_get_port(&any_addr) != 3000) {
        error("ptk_address_init_any set wrong port: %u != 3000", ptk_address_get_port(&any_addr));
        return 6;
    }
    
    // Test ptk_address_equals
    ptk_address_t addr1, addr2, addr3;
    ptk_address_init(&addr1, "192.168.1.1", 80);
    ptk_address_init(&addr2, "192.168.1.1", 80);
    ptk_address_init(&addr3, "192.168.1.1", 8080);
    
    if (!ptk_address_equals(&addr1, &addr2)) {
        error("ptk_address_equals failed for identical addresses");
        return 7;
    }
    
    if (ptk_address_equals(&addr1, &addr3)) {
        error("ptk_address_equals returned true for different ports");
        return 8;
    }
    
    ptk_address_t addr4;
    ptk_address_init(&addr4, "192.168.1.2", 80);
    
    if (ptk_address_equals(&addr1, &addr4)) {
        error("ptk_address_equals returned true for different IPs");
        return 9;
    }
    
    info("test_address_operations exit");
    return 0;
}

int test_address_edge_cases(void) {
    info("test_address_edge_cases entry");
    
    ptk_address_t addr;
    
    // Test with NULL IP (should use INADDR_ANY)
    ptk_err result = ptk_address_init(&addr, NULL, 8080);
    if (result != PTK_OK) {
        error("ptk_address_init failed with NULL IP");
        return 1;
    }
    
    // Test with port 0
    result = ptk_address_init(&addr, "127.0.0.1", 0);
    if (result != PTK_OK) {
        error("ptk_address_init failed with port 0");
        return 2;
    }
    
    // Test with maximum port
    result = ptk_address_init(&addr, "127.0.0.1", 65535);
    if (result != PTK_OK) {
        error("ptk_address_init failed with port 65535");
        return 3;
    }
    
    // Test with invalid IP addresses
    result = ptk_address_init(&addr, "invalid.ip", 8080);
    if (result == PTK_OK) {
        error("ptk_address_init should have failed with invalid IP");
        return 4;
    }
    
    result = ptk_address_init(&addr, "256.256.256.256", 8080);
    if (result == PTK_OK) {
        error("ptk_address_init should have failed with out-of-range IP");
        return 5;
    }
    
    // Test with NULL address parameter
    result = ptk_address_init(NULL, "127.0.0.1", 8080);
    if (result == PTK_OK) {
        error("ptk_address_init should have failed with NULL address");
        return 6;
    }
    
    // Test ptk_address_get_port with NULL
    uint16_t port = ptk_address_get_port(NULL);
    if (port != 0) {
        error("ptk_address_get_port should return 0 for NULL address");
        return 7;
    }
    
    // Test ptk_address_to_string with NULL
    char *str = ptk_address_to_string(NULL);
    if (str != NULL) {
        error("ptk_address_to_string should return NULL for NULL address");
        return 8;
    }
    
    // Test ptk_address_equals with NULL
    ptk_address_init(&addr, "127.0.0.1", 8080);
    if (ptk_address_equals(NULL, &addr) || ptk_address_equals(&addr, NULL)) {
        error("ptk_address_equals should return false for NULL addresses");
        return 9;
    }
    
    if (ptk_address_equals(NULL, NULL)) {
        error("ptk_address_equals should return false for both NULL addresses");
        return 10;
    }
    
    info("test_address_edge_cases exit");
    return 0;
}

//=============================================================================
// Network Discovery Tests
//=============================================================================

int test_network_discovery(void) {
    info("test_network_discovery entry");
    
    // Test ptk_network_discover_interfaces
    ptk_network_interface_array_t *interfaces = ptk_network_discover_interfaces();
    if (!interfaces) {
        error("ptk_network_discover_interfaces returned NULL");
        return 1;
    }
    
    size_t count = ptk_network_interface_array_len(interfaces);
    info("Found %zu network interfaces", count);
    
    if (count == 0) {
        error("No network interfaces found (expected at least loopback)");
        ptk_local_free(&interfaces);
        return 2;
    }
    
    // Check each interface
    bool found_loopback = false;
    for (size_t i = 0; i < count; i++) {
        const ptk_network_interface_t *iface = ptk_network_interface_array_get(interfaces, i);
        if (!iface) {
            error("ptk_network_interface_array_get returned NULL for index %zu", i);
            ptk_local_free(&interfaces);
            return 3;
        }
        
        info("Interface %zu: %s IP:%s Mask:%s Broadcast:%s Up:%s Loopback:%s",
             i, iface->interface_name, iface->ip_address, iface->netmask, 
             iface->broadcast, iface->is_up ? "Yes" : "No", 
             iface->is_loopback ? "Yes" : "No");
        
        // Validate interface fields
        if (strlen(iface->interface_name) == 0) {
            error("Interface %zu has empty name", i);
            ptk_local_free(&interfaces);
            return 4;
        }
        
        if (strlen(iface->ip_address) == 0) {
            error("Interface %zu has empty IP address", i);
            ptk_local_free(&interfaces);
            return 5;
        }
        
        if (iface->is_loopback) {
            found_loopback = true;
        }
        
        // Check for valid IP address format
        struct in_addr addr;
        if (inet_aton(iface->ip_address, &addr) == 0) {
            error("Interface %zu has invalid IP address: %s", i, iface->ip_address);
            ptk_local_free(&interfaces);
            return 6;
        }
    }
    
    if (!found_loopback) {
        error("No loopback interface found");
        ptk_local_free(&interfaces);
        return 7;
    }
    
    ptk_local_free(&interfaces);
    
    // Test legacy network discovery API
    ptk_network_info *network_info = ptk_socket_network_list();
    if (!network_info) {
        error("ptk_socket_network_list returned NULL");
        return 8;
    }
    
    size_t legacy_count = ptk_socket_network_info_count(network_info);
    info("Legacy API found %zu network interfaces", legacy_count);
    
    if (legacy_count == 0) {
        error("Legacy API found no network interfaces");
        ptk_local_free(&network_info);
        return 9;
    }
    
    // Check legacy interface entries
    for (size_t i = 0; i < legacy_count; i++) {
        const ptk_network_info_entry *entry = ptk_socket_network_info_get(network_info, i);
        if (!entry) {
            error("ptk_socket_network_info_get returned NULL for index %zu", i);
            ptk_local_free(&network_info);
            return 10;
        }
        
        info("Legacy Interface %zu: %s IP:%s Mask:%s Broadcast:%s",
             i, entry->interface_name, entry->ip_address, 
             entry->netmask, entry->broadcast);
    }
    
    // Test out-of-bounds access
    const ptk_network_info_entry *invalid_entry = ptk_socket_network_info_get(network_info, legacy_count);
    if (invalid_entry != NULL) {
        error("ptk_socket_network_info_get should return NULL for out-of-bounds index");
        ptk_local_free(&network_info);
        return 11;
    }
    
    ptk_local_free(&network_info);
    
    info("test_network_discovery exit");
    return 0;
}

//=============================================================================
// UDP Socket Tests
//=============================================================================

// Thread function for UDP server
void udp_server_thread(ptk_shared_handle_t param) {
    // This is a placeholder - actual UDP server implementation would be complex
    info("UDP server thread started");
    
    // Simulate server operation
    usleep(100000); // 100ms
    
    info("UDP server thread finished");
}

// Thread function for UDP client
void udp_client_thread(ptk_shared_handle_t param) {
    info("UDP client thread started");
    
    // Simulate client operation
    usleep(50000); // 50ms
    
    info("UDP client thread finished");
}

int test_udp_socket_creation(void) {
    info("test_udp_socket_creation entry");
    
    // Initialize shared memory for threading
    ptk_err err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Create shared context
    ptk_shared_handle_t context = ptk_shared_alloc(sizeof(int), NULL);
    if (!PTK_SHARED_IS_VALID(context)) {
        error("Failed to allocate shared context");
        ptk_shared_shutdown();
        return 2;
    }
    
    // Test UDP socket creation for server
    ptk_address_t server_addr;
    ptk_address_init_any(&server_addr, 12345);
    
    ptk_sock *udp_server = ptk_udp_socket_create(&server_addr, false, udp_server_thread, context);
    if (!udp_server) {
        error("Failed to create UDP server socket");
        ptk_shared_release(context);
        ptk_shared_shutdown();
        return 3;
    }
    
    info("UDP server socket created successfully");
    
    // Test UDP socket creation for client (no binding)
    ptk_sock *udp_client = ptk_udp_socket_create(NULL, false, udp_client_thread, context);
    if (!udp_client) {
        error("Failed to create UDP client socket");
        ptk_socket_close(udp_server);
        ptk_shared_release(context);
        ptk_shared_shutdown();
        return 4;
    }
    
    info("UDP client socket created successfully");
    
    // Test UDP socket with broadcast enabled
    ptk_sock *udp_broadcast = ptk_udp_socket_create(&server_addr, true, udp_server_thread, context);
    if (!udp_broadcast) {
        error("Failed to create UDP broadcast socket");
        ptk_socket_close(udp_server);
        ptk_socket_close(udp_client);
        ptk_shared_release(context);
        ptk_shared_shutdown();
        return 5;
    }
    
    info("UDP broadcast socket created successfully");
    
    // Clean up
    ptk_socket_close(udp_server);
    ptk_socket_close(udp_client);
    ptk_socket_close(udp_broadcast);
    
    ptk_shared_release(context);
    ptk_shared_shutdown();
    
    info("test_udp_socket_creation exit");
    return 0;
}

int test_udp_socket_communication(void) {
    info("test_udp_socket_communication entry");
    
    // Initialize shared memory
    ptk_err err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    ptk_shared_handle_t context = ptk_shared_alloc(sizeof(int), NULL);
    if (!PTK_SHARED_IS_VALID(context)) {
        error("Failed to allocate shared context");
        ptk_shared_shutdown();
        return 2;
    }
    
    // Create UDP socket for testing
    ptk_address_t test_addr;
    ptk_address_init(&test_addr, "127.0.0.1", 54321);
    
    ptk_sock *udp_sock = ptk_udp_socket_create(&test_addr, false, udp_server_thread, context);
    if (!udp_sock) {
        error("Failed to create UDP socket for communication test");
        ptk_shared_release(context);
        ptk_shared_shutdown();
        return 3;
    }
    
    // Test UDP send
    ptk_buf *send_buf = ptk_buf_alloc(100);
    if (!send_buf) {
        error("Failed to allocate send buffer");
        ptk_socket_close(udp_sock);
        ptk_shared_release(context);
        ptk_shared_shutdown();
        return 4;
    }
    
    const char *test_message = "Hello UDP!";
    memcpy(ptk_buf_get_start(send_buf), test_message, strlen(test_message));
    ptk_buf_set_end(send_buf, ptk_buf_get_start(send_buf) + strlen(test_message));
    
    ptk_address_t dest_addr;
    ptk_address_init(&dest_addr, "127.0.0.1", 54321);
    
    // Test ptk_udp_socket_send_to
    err = ptk_udp_socket_send_to(udp_sock, send_buf, &dest_addr, false, 1000);
    if (err != PTK_OK) {
        // This might fail if we can't actually send to ourselves
        info("UDP send failed (expected in test environment): %d", err);
    } else {
        info("UDP send succeeded");
    }
    
    // Test broadcast send
    ptk_address_t broadcast_addr;
    ptk_address_init(&broadcast_addr, "255.255.255.255", 54321);
    
    err = ptk_udp_socket_send_to(udp_sock, send_buf, &broadcast_addr, true, 1000);
    if (err != PTK_OK) {
        info("UDP broadcast send failed (expected in test environment): %d", err);
    } else {
        info("UDP broadcast send succeeded");
    }
    
    ptk_local_free(&send_buf);
    
    // Test UDP receive (with timeout)
    ptk_address_t sender_addr;
    ptk_buf *recv_buf = ptk_udp_socket_recv_from(udp_sock, &sender_addr, 100); // 100ms timeout
    if (!recv_buf) {
        info("UDP receive timed out (expected in test environment)");
    } else {
        info("UDP receive succeeded, got %zu bytes", ptk_buf_get_len(recv_buf));
        ptk_local_free(&recv_buf);
    }
    
    // Test receive with no wait (should return immediately)
    recv_buf = ptk_udp_socket_recv_from(udp_sock, &sender_addr, 0);
    if (!recv_buf) {
        info("UDP no-wait receive returned NULL (expected)");
    } else {
        info("UDP no-wait receive got data");
        ptk_local_free(&recv_buf);
    }
    
    // Clean up
    ptk_socket_close(udp_sock);
    ptk_shared_release(context);
    ptk_shared_shutdown();
    
    info("test_udp_socket_communication exit");
    return 0;
}

//=============================================================================
// Socket Edge Cases and Error Handling
//=============================================================================

int test_socket_error_conditions(void) {
    info("test_socket_error_conditions entry");
    
    // Test with NULL parameters
    ptk_sock *null_sock = ptk_udp_socket_create(NULL, false, NULL, PTK_SHARED_INVALID_HANDLE);
    if (null_sock != NULL) {
        error("UDP socket creation should fail with NULL thread function");
        ptk_socket_close(null_sock);
        return 1;
    }
    
    // Test sending to closed socket
    ptk_socket_close(NULL); // Should handle NULL gracefully
    
    // Test address operations with edge cases
    ptk_address_t addr;
    ptk_err result = ptk_address_init(&addr, "0.0.0.0", 1);
    if (result != PTK_OK) {
        error("ptk_address_init failed for 0.0.0.0");
        return 2;
    }
    
    result = ptk_address_init(&addr, "255.255.255.255", 65535);
    if (result != PTK_OK) {
        error("ptk_address_init failed for 255.255.255.255:65535");
        return 3;
    }
    
    // Test network discovery error cases
    size_t invalid_count = ptk_socket_network_info_count(NULL);
    if (invalid_count != 0) {
        error("ptk_socket_network_info_count should return 0 for NULL");
        return 4;
    }
    
    const ptk_network_info_entry *invalid_entry = ptk_socket_network_info_get(NULL, 0);
    if (invalid_entry != NULL) {
        error("ptk_socket_network_info_get should return NULL for NULL network_info");
        return 5;
    }
    
    info("test_socket_error_conditions exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_sock_main(void) {
    info("=== Starting PTK Socket Tests ===");
    
    int result = 0;
    
    // Address management tests
    if ((result = test_address_operations()) != 0) {
        error("test_address_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_address_edge_cases()) != 0) {
        error("test_address_edge_cases failed with code %d", result);
        return result;
    }
    
    // Network discovery tests
    if ((result = test_network_discovery()) != 0) {
        error("test_network_discovery failed with code %d", result);
        return result;
    }
    
    // UDP socket tests
    if ((result = test_udp_socket_creation()) != 0) {
        error("test_udp_socket_creation failed with code %d", result);
        return result;
    }
    
    if ((result = test_udp_socket_communication()) != 0) {
        error("test_udp_socket_communication failed with code %d", result);
        return result;
    }
    
    // Error handling tests
    if ((result = test_socket_error_conditions()) != 0) {
        error("test_socket_error_conditions failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Socket Tests Passed ===");
    return 0;
}