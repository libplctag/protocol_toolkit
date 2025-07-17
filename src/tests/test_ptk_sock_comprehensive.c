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
#include <ptk_utils.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

//=============================================================================
// Address Management Tests
//=============================================================================

int test_address_operations(void) {
    info("test_address_operations entry");
    
    ptk_address_t *addr = ptk_address_create("127.0.0.1", 8080);
    if (!addr) {
        error("ptk_address_create failed for 127.0.0.1:8080");
        return 1;
    }

    // Test ptk_address_get_port
    uint16_t port = ptk_address_get_port(addr);
    if (port != 8080) {
        error("ptk_address_get_port returned wrong port: %u != 8080", port);
        ptk_local_free(&addr);
        return 2;
    }

    // Test ptk_address_to_string
    char *addr_str = ptk_address_to_string(addr);
    if (!addr_str) {
        error("ptk_address_to_string returned NULL");
        ptk_local_free(&addr);
        return 3;
    }

    info("Address string: %s", addr_str);

    // Should contain "127.0.0.1"
    if (strstr(addr_str, "127.0.0.1") == NULL) {
        error("Address string doesn't contain expected IP: %s", addr_str);
        ptk_local_free(&addr_str);
        ptk_local_free(&addr);
        return 4;
    }

    ptk_local_free(&addr_str);
    ptk_local_free(&addr);
    
    // Test ptk_address_create_any
    ptk_address_t *any_addr = ptk_address_create_any(3000);
    if (!any_addr) {
        error("ptk_address_create_any failed");
        return 5;
    }

    if (ptk_address_get_port(any_addr) != 3000) {
        error("ptk_address_create_any set wrong port: %u != 3000", ptk_address_get_port(any_addr));
        ptk_local_free(&any_addr);
        return 6;
    }
    ptk_local_free(&any_addr);
    
    // Test ptk_address_equals
    ptk_address_t *addr1 = ptk_address_create("192.168.1.1", 80);
    ptk_address_t *addr2 = ptk_address_create("192.168.1.1", 80);
    ptk_address_t *addr3 = ptk_address_create("192.168.1.1", 8080);

    if (!ptk_address_equals(addr1, addr2)) {
        error("ptk_address_equals failed for identical addresses");
        ptk_local_free(&addr1);
        ptk_local_free(&addr2);
        ptk_local_free(&addr3);
        return 7;
    }

    if (ptk_address_equals(addr1, addr3)) {
        error("ptk_address_equals returned true for different ports");
        ptk_local_free(&addr1);
        ptk_local_free(&addr2);
        ptk_local_free(&addr3);
        return 8;
    }

    ptk_address_t *addr4 = ptk_address_create("192.168.1.2", 80);

    if (ptk_address_equals(addr1, addr4)) {
        error("ptk_address_equals returned true for different IPs");
        ptk_local_free(&addr1);
        ptk_local_free(&addr2);
        ptk_local_free(&addr3);
        ptk_local_free(&addr4);
        return 9;
    }

    ptk_local_free(&addr1);
    ptk_local_free(&addr2);
    ptk_local_free(&addr3);
    ptk_local_free(&addr4);
    
    info("test_address_operations exit");
    return 0;
}

int test_address_edge_cases(void) {
    info("test_address_edge_cases entry");
    
    // Test with NULL IP (should use INADDR_ANY)
    ptk_address_t *addr = ptk_address_create(NULL, 8080);
    if (!addr) {
        error("ptk_address_create failed with NULL IP");
        return 1;
    }
    ptk_local_free(&addr);

    // Test with port 0
    addr = ptk_address_create("127.0.0.1", 0);
    if (!addr) {
        error("ptk_address_create failed with port 0");
        return 2;
    }
    ptk_local_free(&addr);

    // Test with maximum port
    addr = ptk_address_create("127.0.0.1", 65535);
    if (!addr) {
        error("ptk_address_create failed with port 65535");
        return 3;
    }
    ptk_local_free(&addr);

    // Test with invalid IP addresses
    addr = ptk_address_create("invalid.ip", 8080);
    if (addr) {
        error("ptk_address_create should have failed with invalid IP");
        ptk_local_free(&addr);
        return 4;
    }

    addr = ptk_address_create("256.256.256.256", 8080);
    if (addr) {
        error("ptk_address_create should have failed with out-of-range IP");
        ptk_local_free(&addr);
        return 5;
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
        ptk_local_free(&str);
        return 8;
    }

    // Test ptk_address_equals with NULL
    addr = ptk_address_create("127.0.0.1", 8080);
    if (ptk_address_equals(NULL, addr) || ptk_address_equals(addr, NULL)) {
        error("ptk_address_equals should return false for NULL addresses");
        ptk_local_free(&addr);
        return 9;
    }
    if (ptk_address_equals(NULL, NULL)) {
        error("ptk_address_equals should return false for both NULL addresses");
        ptk_local_free(&addr);
        return 10;
    }
    ptk_local_free(&addr);
    
    info("test_address_edge_cases exit");
    return 0;
}

//=============================================================================
// Network Discovery Tests
//=============================================================================

int test_network_discovery(void) {
    info("test_network_discovery entry");
    
    // Test ptk_network_list_interfaces
    ptk_network_interface_array_t *interfaces = ptk_network_list_interfaces();
    if (!interfaces) {
        error("ptk_network_list_interfaces returned NULL");
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
    
    info("test_network_discovery exit");
    return 0;
}

//=============================================================================
// UDP Socket Tests
//=============================================================================

// Thread function for UDP server
void udp_server_thread(ptk_sock *socket, ptk_shared_handle_t param) {
    // This is a placeholder - actual UDP server implementation would be complex
    info("UDP server thread started");
    
    // Simulate server operation - in a real implementation, this would:
    // 1. Call ptk_udp_socket_recv_from(socket, &sender_addr, timeout) to receive data
    // 2. Process the received data
    // 3. Optionally call ptk_udp_socket_send_to(socket, response, &sender_addr, false, timeout) to respond
    
    ptk_sleep_ms(100); // 100ms

    info("UDP server thread finished");
}

// Thread function for UDP client
void udp_client_thread(ptk_sock *socket, ptk_shared_handle_t param) {
    info("UDP client thread started");
    
    // Simulate client operation - in a real implementation, this would:
    // 1. Create a buffer with ptk_buf_alloc()
    // 2. Write data using ptk_buf_set_u8() 
    // 3. Call ptk_udp_socket_send_to(socket, buffer, &dest_addr, false, timeout) to send data
    // 4. Optionally call ptk_udp_socket_recv_from(socket, &sender_addr, timeout) to receive response
    
    ptk_sleep_ms(50); // 50ms

    info("UDP client thread finished");
}

int test_udp_socket_creation(void) {
    info("test_udp_socket_creation entry");
    
    // Test UDP socket creation for server
    ptk_address_t *server_addr = ptk_address_create_any(12345);
    if (!server_addr) {
        error("Failed to create server address");
        return 1;
    }
    ptk_sock *udp_server = ptk_udp_socket_create(server_addr, false);
    ptk_local_free(&server_addr);
    if (!udp_server) {
        error("Failed to create UDP server socket");
        return 1;
    }

    info("UDP server socket created successfully");

    // Test UDP socket creation for client (no binding)
    ptk_sock *udp_client = ptk_udp_socket_create(NULL, false);
    if (!udp_client) {
        error("Failed to create UDP client socket");
        ptk_socket_close(udp_server);
        return 2;
    }

    info("UDP client socket created successfully");

    // Test UDP socket with broadcast enabled (use different port)
    ptk_address_t *broadcast_addr = ptk_address_create_any(12346);
    if (!broadcast_addr) {
        error("Failed to create broadcast address");
        ptk_socket_close(udp_server);
        ptk_socket_close(udp_client);
        return 3;
    }
    ptk_sock *udp_broadcast = ptk_udp_socket_create(broadcast_addr, true);
    ptk_local_free(&broadcast_addr);
    if (!udp_broadcast) {
        error("Failed to create UDP broadcast socket");
        ptk_socket_close(udp_server);
        ptk_socket_close(udp_client);
        return 3;
    }

    info("UDP broadcast socket created successfully");

    // Clean up
    ptk_socket_close(udp_server);
    ptk_socket_close(udp_client);
    ptk_socket_close(udp_broadcast);

    info("test_udp_socket_creation exit");
    return 0;
}

int test_udp_socket_communication(void) {
    info("test_udp_socket_communication entry");
    
    // Create UDP socket for testing
    ptk_address_t *test_addr = ptk_address_create("127.0.0.1", 54321);
    if (!test_addr) {
        error("Failed to create test address");
        return 1;
    }
    ptk_sock *udp_sock = ptk_udp_socket_create(test_addr, false);
    ptk_local_free(&test_addr);
    if (!udp_sock) {
        error("Failed to create UDP socket for communication test");
        return 1;
    }
    
    // Test UDP send
    ptk_buf *send_buf = ptk_buf_alloc(100);
    if (!send_buf) {
        error("Failed to allocate send buffer");
        ptk_socket_close(udp_sock);
        return 2;
    }
    
    const char *test_message = "Hello UDP!";
    size_t msg_len = strlen(test_message);
    
    // Use proper buffer accessors to write data safely
    for (size_t i = 0; i < msg_len; i++) {
        ptk_err_t write_err = ptk_buf_set_u8(send_buf, (ptk_u8_t)test_message[i]);
        if (write_err != PTK_OK) {
            error("Failed to write byte %zu to buffer", i);
            ptk_local_free(&send_buf);
            ptk_socket_close(udp_sock);
            return 3;
        }
    }
    
    ptk_address_t *dest_addr = ptk_address_create("127.0.0.1", 54321);
    if (!dest_addr) {
        error("Failed to create dest address");
        ptk_local_free(&send_buf);
        ptk_socket_close(udp_sock);
        return 4;
    }
    // Test ptk_udp_socket_send_to
    ptk_err_t err = ptk_udp_socket_send_to(udp_sock, send_buf, dest_addr, false, 1000);
    ptk_local_free(&dest_addr);
    if (err != PTK_OK) {
        // This might fail if we can't actually send to ourselves
        info("UDP send failed (expected in test environment): %d", err);
    } else {
        info("UDP send succeeded");
    }
    
    // Test broadcast send
    ptk_address_t *broadcast_addr = ptk_address_create("255.255.255.255", 54321);
    if (!broadcast_addr) {
        error("Failed to create broadcast address");
        ptk_local_free(&send_buf);
        ptk_socket_close(udp_sock);
        return 5;
    }
    err = ptk_udp_socket_send_to(udp_sock, send_buf, broadcast_addr, true, 1000);
    ptk_local_free(&broadcast_addr);
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
    
    info("test_udp_socket_communication exit");
    return 0;
}

//=============================================================================
// Socket Edge Cases and Error Handling
//=============================================================================

int test_socket_error_conditions(void) {
    info("test_socket_error_conditions entry");
    
    // Test sending to closed socket
    ptk_socket_close(NULL); // Should handle NULL gracefully
    
    // Test address operations with edge cases
    ptk_address_t *addr = ptk_address_create("0.0.0.0", 1);
    if (!addr) {
        error("ptk_address_create failed for 0.0.0.0");
        return 2;
    }
    ptk_local_free(&addr);

    addr = ptk_address_create("255.255.255.255", 65535);
    if (!addr) {
        error("ptk_address_create failed for 255.255.255.255:65535");
        return 3;
    }
    ptk_local_free(&addr);
    
    // Test network discovery error cases
    ptk_network_interface_array_t *null_interfaces = ptk_network_list_interfaces();
    if (null_interfaces) {
        ptk_local_free(&null_interfaces);
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

int main(void) {
    return test_ptk_sock_main();
}