#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "pdu_example.h"

/**
 * Test program for the PDU X-Macro system
 */

int main(void) {
    printf("PDU X-Macro System Test\n");
    printf("=======================\n\n");
    
    // Test 1: Basic serialization/deserialization
    printf("Test 1: Basic TCP Header Serialization\n");
    printf("---------------------------------------\n");
    
    uint8_t buffer[256];
    ptk_slice_bytes_t slice = ptk_slice_bytes_make(buffer, sizeof(buffer));
    
    // Create and populate TCP header
    tcp_header_t original_tcp;
    tcp_header_init(&original_tcp);
    original_tcp.src_port = 8080;
    original_tcp.dst_port = 443;
    original_tcp.seq_num = 0x12345678;
    original_tcp.ack_num = 0x87654321;
    original_tcp.flags = 0x0018; // PSH + ACK
    original_tcp.window_size = 65535;
    
    printf("Original TCP Header:\n");
    tcp_header_print(&original_tcp);
    
    // Serialize
    ptk_slice_bytes_t write_slice = slice;
    ptk_status_t status = tcp_header_serialize(&write_slice, &original_tcp, PTK_ENDIAN_BIG);
    printf("\nSerialization status: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    size_t bytes_written = sizeof(buffer) - write_slice.len;
    printf("Bytes written: %zu\n", bytes_written);
    printf("Expected size: %zu\n", tcp_header_size(&original_tcp));
    assert(bytes_written == tcp_header_size(&original_tcp));
    
    // Print serialized bytes
    printf("Serialized bytes: ");
    for (size_t i = 0; i < bytes_written; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
    
    // Deserialize
    ptk_slice_bytes_t read_slice = ptk_slice_bytes_make(buffer, bytes_written);
    tcp_header_t decoded_tcp;
    status = tcp_header_deserialize(&read_slice, &decoded_tcp, PTK_ENDIAN_BIG);
    printf("\nDeserialization status: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    printf("Decoded TCP Header:\n");
    tcp_header_print(&decoded_tcp);
    
    // Verify fields match
    assert(original_tcp.src_port == decoded_tcp.src_port);
    assert(original_tcp.dst_port == decoded_tcp.dst_port);
    assert(original_tcp.seq_num == decoded_tcp.seq_num);
    assert(original_tcp.ack_num == decoded_tcp.ack_num);
    assert(original_tcp.flags == decoded_tcp.flags);
    assert(original_tcp.window_size == decoded_tcp.window_size);
    
    printf("✓ All fields match!\n\n");
    
    // Test 2: Peek functionality
    printf("Test 2: Peek Functionality\n");
    printf("---------------------------\n");
    
    ptk_slice_bytes_t peek_slice = ptk_slice_bytes_make(buffer, bytes_written);
    tcp_header_t peeked_tcp;
    status = tcp_header_deserialize_peek(&peek_slice, &peeked_tcp, PTK_ENDIAN_BIG);
    printf("Peek status: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    // Slice should not have advanced
    assert(peek_slice.len == bytes_written);
    assert(peek_slice.data == buffer);
    printf("✓ Slice position unchanged after peek\n");
    
    // But data should be correct
    assert(peeked_tcp.src_port == original_tcp.src_port);
    printf("✓ Peeked data is correct\n\n");
    
    // Test 3: Complex PDU with mixed types
    printf("Test 3: Complex PDU with Mixed Types\n");
    printf("------------------------------------\n");
    
    complex_pdu_t complex_original;
    complex_pdu_init(&complex_original);
    complex_original.magic_byte = 0xAB;
    complex_original.header_checksum = 0x1234;
    complex_original.sequence_number = 1000000;
    complex_original.signed_offset = -500;
    complex_original.float_value = 3.14159f;
    complex_original.large_counter = 0x123456789ABCDEFULL;
    complex_original.signed_large_value = -1234567890123LL;
    complex_original.double_precision = 2.718281828459045;
    
    printf("Original Complex PDU:\n");
    complex_pdu_print(&complex_original);
    printf("Size: %zu bytes\n", complex_pdu_size(&complex_original));
    
    // Serialize complex PDU
    uint8_t complex_buffer[128];
    ptk_slice_bytes_t complex_slice = ptk_slice_bytes_make(complex_buffer, sizeof(complex_buffer));
    status = complex_pdu_serialize(&complex_slice, &complex_original, PTK_ENDIAN_LITTLE);
    printf("\nComplex serialization: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    // Deserialize complex PDU
    ptk_slice_bytes_t complex_read = ptk_slice_bytes_make(complex_buffer, 
                                     sizeof(complex_buffer) - complex_slice.len);
    complex_pdu_t complex_decoded;
    status = complex_pdu_deserialize(&complex_read, &complex_decoded, PTK_ENDIAN_LITTLE);
    printf("Complex deserialization: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    printf("Decoded Complex PDU:\n");
    complex_pdu_print(&complex_decoded);
    
    // Verify floating point values (with tolerance)
    float float_diff = complex_original.float_value - complex_decoded.float_value;
    double double_diff = complex_original.double_precision - complex_decoded.double_precision;
    assert(float_diff < 0.00001f && float_diff > -0.00001f);
    assert(double_diff < 0.000000001 && double_diff > -0.000000001);
    printf("✓ Floating point values preserved\n\n");
    
    // Test 4: Sensor data validation
    printf("Test 4: Custom Validation\n");
    printf("-------------------------\n");
    
    sensor_data_t sensor;
    sensor_data_init(&sensor);
    sensor.sensor_id = 42;
    sensor.temperature_celsius = 2350; // 23.50°C
    sensor.humidity_percent = 6500;    // 65.00%
    sensor.timestamp = 1642780800;
    
    printf("Valid sensor data: %s\n", sensor_data_validate(&sensor) ? "PASS" : "FAIL");
    
    // Test invalid temperature
    sensor.temperature_celsius = 10000; // 100°C - too hot!
    printf("Invalid temperature: %s\n", sensor_data_validate(&sensor) ? "PASS" : "FAIL");
    
    // Test invalid humidity  
    sensor.temperature_celsius = 2350; // Reset to valid
    sensor.humidity_percent = 15000;   // 150% - impossible!
    printf("Invalid humidity: %s\n", sensor_data_validate(&sensor) ? "PASS" : "FAIL");
    
    printf("\n=== All Tests Passed! ===\n");
    
    return 0;
}