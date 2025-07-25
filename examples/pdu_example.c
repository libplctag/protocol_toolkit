#include "pdu_example.h"
#include "ptk_pdu_macros.h"

/**
 * PDU Implementation using X-Macro System
 * 
 * This file shows how to implement the PDU functions declared in the header.
 * Each PTK_IMPLEMENT_PDU() call generates all the serialization functions.
 */

// Implement all the PDU functions for each type
PTK_IMPLEMENT_PDU(tcp_header)
PTK_IMPLEMENT_PDU(eth_header) 
PTK_IMPLEMENT_PDU(my_message)
PTK_IMPLEMENT_PDU(sensor_data)
PTK_IMPLEMENT_PDU(complex_pdu)

/**
 * Example usage function
 */
void demonstrate_pdu_system(void) {
    printf("=== PDU X-Macro System Demonstration ===\n\n");
    
    // Create a buffer for serialization
    uint8_t buffer[1024];
    ptk_slice_bytes_t slice = ptk_slice_bytes_make(buffer, sizeof(buffer));
    
    // Example 1: TCP Header
    printf("1. TCP Header Example:\n");
    tcp_header_t tcp;
    tcp_header_init(&tcp);
    tcp.src_port = 8080;
    tcp.dst_port = 443;
    tcp.seq_num = 0x12345678;
    tcp.ack_num = 0x87654321;
    tcp.flags = 0x18; // PSH + ACK
    tcp.window_size = 65535;
    
    tcp_header_print(&tcp);
    printf("Size: %zu bytes\n", tcp_header_size(&tcp));
    
    // Serialize
    ptk_status_t status = tcp_header_serialize(&slice, &tcp, PTK_ENDIAN_BIG);
    printf("Serialization: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    // Deserialize back
    tcp_header_t tcp_decoded;
    ptk_slice_bytes_t read_slice = ptk_slice_bytes_make(buffer, sizeof(buffer) - slice.len);
    status = tcp_header_deserialize(&read_slice, &tcp_decoded, PTK_ENDIAN_BIG);
    printf("Deserialization: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    printf("Decoded TCP Header:\n");
    tcp_header_print(&tcp_decoded);
    printf("\n");
    
    // Example 2: Sensor Data
    printf("2. Sensor Data Example:\n");
    sensor_data_t sensor;
    sensor_data_init(&sensor);
    sensor.sensor_id = 42;
    sensor.temperature_celsius = 2350; // 23.50°C (scaled by 100)
    sensor.humidity_percent = 6500;    // 65.00% (scaled by 100)
    sensor.timestamp = 1642780800;     // Unix timestamp
    
    sensor_data_print(&sensor);
    printf("Size: %zu bytes\n", sensor_data_size(&sensor));
    printf("\n");
    
    // Example 3: Complex PDU with mixed types
    printf("3. Complex PDU Example:\n");
    complex_pdu_t complex;
    complex_pdu_init(&complex);
    complex.magic_byte = 0xAB;
    complex.header_checksum = 0x1234;
    complex.sequence_number = 1000000;
    complex.signed_offset = -500;
    complex.float_value = 3.14159f;
    complex.large_counter = 0x123456789ABCDEFULL;
    complex.signed_large_value = -1234567890123LL;
    complex.double_precision = 2.718281828459045;
    
    complex_pdu_print(&complex);
    printf("Size: %zu bytes\n", complex_pdu_size(&complex));
    printf("\n");
    
    printf("=== End Demonstration ===\n");
}

/**
 * Example of custom validation functions
 */
bool tcp_header_validate(const tcp_header_t* header) {
    if (!header) return false;
    
    // Check for valid port ranges
    if (header->src_port == 0 || header->dst_port == 0) {
        return false;
    }
    
    // Check flags are reasonable (not all set)
    if (header->flags == 0xFFFF) {
        return false;
    }
    
    return true;
}

bool sensor_data_validate(const sensor_data_t* data) {
    if (!data) return false;
    
    // Check temperature is in reasonable range (-40°C to +85°C, scaled by 100)
    if (data->temperature_celsius < -4000 || data->temperature_celsius > 8500) {
        return false;
    }
    
    // Check humidity is 0-100% (scaled by 100)
    if (data->humidity_percent > 10000) {
        return false;
    }
    
    return true;
}

/**
 * Example of creating PDU variants
 */
#define PTK_PDU_FIELDS_tcp_header_v2(X) \
    X(PTK_PDU_U16, src_port) \
    X(PTK_PDU_U16, dst_port) \
    X(PTK_PDU_U32, seq_num) \
    X(PTK_PDU_U32, ack_num) \
    X(PTK_PDU_U16, flags) \
    X(PTK_PDU_U16, window_size) \
    X(PTK_PDU_U32, options)     /* New field in v2 */ \
    X(PTK_PDU_U16, checksum)    /* New field in v2 */

PTK_DECLARE_PDU(tcp_header_v2)
PTK_IMPLEMENT_PDU(tcp_header_v2)