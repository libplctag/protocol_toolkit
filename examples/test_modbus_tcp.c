#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "modbus_tcp_example.h"

/**
 * Test program demonstrating Modbus TCP Write Multiple Registers
 * using the PDU X-Macro system
 */

int main(void) {
    printf("Modbus TCP Write Multiple Registers Test\n");
    printf("========================================\n\n");
    
    // Test data: HVAC control settings
    uint16_t register_values[4] = {
        235,        // Temperature setpoint: 23.5°C
        600,        // Humidity setpoint: 60.0%
        1,          // Control mode: Manual
        0x00FF      // Alarm mask: All enabled
    };
    
    printf("Test Data - HVAC Control Settings:\n");
    printf("  Temperature Setpoint: %.1f°C\n", register_values[0] / 10.0);
    printf("  Humidity Setpoint: %.1f%%\n", register_values[1] / 10.0);
    printf("  Control Mode: %s\n", register_values[2] == 1 ? "Manual" : "Auto");
    printf("  Alarm Mask: 0x%04X\n\n", register_values[3]);
    
    // === Test 1: Create and serialize request ===
    printf("Test 1: Write Multiple Registers Request\n");
    printf("-----------------------------------------\n");
    
    modbus_write_multiple_request_t request;
    modbus_write_multiple_request_init(&request);
    
    ptk_status_t status = modbus_create_write_multiple_request(
        &request, 
        1000,           // Starting address
        register_values, 
        4               // Number of registers
    );
    
    assert(status == PTK_OK);
    printf("✓ Request created successfully\n");
    
    // Validate the request
    assert(modbus_validate_write_multiple_request(&request));
    printf("✓ Request validation passed\n");
    
    modbus_write_multiple_request_print(&request);
    printf("Request PDU size: %zu bytes\n\n", modbus_write_multiple_request_size(&request));
    
    // === Test 2: Create complete TCP frame ===
    printf("Test 2: Complete Modbus TCP Frame\n");
    printf("----------------------------------\n");
    
    modbus_write_multiple_frame_t frame;
    MODBUS_CREATE_WRITE_MULTIPLE_FRAME(frame, 0x1234, 0x01, 1000, register_values, 4);
    
    printf("Complete frame:\n");
    modbus_write_multiple_frame_print(&frame);
    
    size_t frame_size = modbus_write_multiple_frame_size(&frame);
    printf("Total frame size: %zu bytes\n", frame_size);
    
    // Expected size: 7 (MBAP) + 1 (FC) + 2 (addr) + 2 (qty) + 1 (count) + 8 (data) = 21 bytes
    assert(frame_size == 21);
    printf("✓ Frame size correct\n\n");
    
    // === Test 3: Serialize to wire format ===
    printf("Test 3: Serialization to Wire Format\n");
    printf("-------------------------------------\n");
    
    uint8_t buffer[256];
    ptk_slice_bytes_t slice = ptk_slice_bytes_make(buffer, sizeof(buffer));
    
    status = modbus_write_multiple_frame_serialize(&slice, &frame, PTK_ENDIAN_BIG);
    assert(status == PTK_OK);
    printf("✓ Serialization successful\n");
    
    size_t bytes_written = sizeof(buffer) - slice.len;
    assert(bytes_written == frame_size);
    printf("✓ Correct number of bytes written: %zu\n", bytes_written);
    
    // Print the raw bytes in hex
    printf("Wire format (hex): ");
    for (size_t i = 0; i < bytes_written; i++) {
        printf("%02X", buffer[i]);
        if (i < bytes_written - 1) printf(" ");
    }
    printf("\n");
    
    // Verify specific bytes
    assert(buffer[0] == 0x12 && buffer[1] == 0x34);  // Transaction ID
    assert(buffer[2] == 0x00 && buffer[3] == 0x00);  // Protocol ID
    assert(buffer[4] == 0x00 && buffer[5] == 0x0E);  // Length (14 bytes)
    assert(buffer[6] == 0x01);                       // Unit ID
    assert(buffer[7] == 0x10);                       // Function Code
    assert(buffer[8] == 0x03 && buffer[9] == 0xE8);  // Starting address (1000)
    assert(buffer[10] == 0x00 && buffer[11] == 0x04); // Quantity (4)
    assert(buffer[12] == 0x08);                      // Byte count (8)
    printf("✓ All header fields correct\n");
    
    // Verify register data (big-endian)
    assert(buffer[13] == 0x00 && buffer[14] == 0xEB); // 235
    assert(buffer[15] == 0x02 && buffer[16] == 0x58); // 600
    assert(buffer[17] == 0x00 && buffer[18] == 0x01); // 1
    assert(buffer[19] == 0x00 && buffer[20] == 0xFF); // 0x00FF
    printf("✓ All register data correct\n\n");
    
    // === Test 4: Deserialize from wire format ===
    printf("Test 4: Deserialization from Wire Format\n");
    printf("------------------------------------------\n");
    
    ptk_slice_bytes_t read_slice = ptk_slice_bytes_make(buffer, bytes_written);
    modbus_write_multiple_frame_t decoded_frame;
    modbus_write_multiple_frame_init(&decoded_frame);
    
    // Initialize the register array for decoding
    modbus_registers_init(&decoded_frame.pdu.register_values, 10);
    decoded_frame.pdu.register_values.count = 4; // We know there are 4 registers
    
    status = modbus_write_multiple_frame_deserialize(&read_slice, &decoded_frame, PTK_ENDIAN_BIG);
    assert(status == PTK_OK);
    printf("✓ Deserialization successful\n");
    
    // Verify decoded data matches original
    assert(decoded_frame.mbap.transaction_id == frame.mbap.transaction_id);
    assert(decoded_frame.mbap.protocol_id == frame.mbap.protocol_id);
    assert(decoded_frame.mbap.length == frame.mbap.length);
    assert(decoded_frame.mbap.unit_id == frame.mbap.unit_id);
    printf("✓ MBAP header matches\n");
    
    assert(decoded_frame.pdu.function_code == frame.pdu.function_code);
    assert(decoded_frame.pdu.starting_address == frame.pdu.starting_address);
    assert(decoded_frame.pdu.quantity_of_registers == frame.pdu.quantity_of_registers);
    assert(decoded_frame.pdu.byte_count == frame.pdu.byte_count);
    printf("✓ PDU header matches\n");
    
    for (int i = 0; i < 4; i++) {
        assert(decoded_frame.pdu.register_values.registers[i] == register_values[i]);
    }
    printf("✓ Register data matches\n");
    
    printf("Decoded frame:\n");
    modbus_write_multiple_frame_print(&decoded_frame);
    printf("\n");
    
    // === Test 5: Create response ===
    printf("Test 5: Write Multiple Registers Response\n");
    printf("------------------------------------------\n");
    
    modbus_write_response_frame_t response_frame;
    MODBUS_CREATE_WRITE_RESPONSE_FRAME(response_frame, 0x1234, 0x01, 1000, 4);
    
    printf("Response frame:\n");
    modbus_write_response_frame_print(&response_frame);
    
    size_t response_size = modbus_write_response_frame_size(&response_frame);
    printf("Response size: %zu bytes\n", response_size);
    
    // Expected: 7 (MBAP) + 1 (FC) + 2 (addr) + 2 (qty) = 12 bytes
    assert(response_size == 12);
    printf("✓ Response size correct\n\n");
    
    // === Test 6: Exception response ===
    printf("Test 6: Exception Response\n");
    printf("---------------------------\n");
    
    modbus_exception_response_t exception;
    modbus_exception_response_init(&exception);
    
    status = modbus_create_exception_response(
        &exception, 
        MODBUS_FC_WRITE_MULTIPLE_REGISTERS, 
        MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS
    );
    assert(status == PTK_OK);
    
    printf("Exception response:\n");
    modbus_exception_response_print(&exception);
    
    // Function code should have 0x80 bit set
    assert(exception.function_code == (MODBUS_FC_WRITE_MULTIPLE_REGISTERS | 0x80));
    assert(exception.exception_code == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    printf("✓ Exception response correct\n\n");
    
    // === Test 7: Application-specific helpers ===
    printf("Test 7: Application-Specific Helpers\n");
    printf("-------------------------------------\n");
    
    // Test HVAC register packing/unpacking
    hvac_control_registers_t hvac_original = {
        .setpoint_temperature = 235,
        .setpoint_humidity = 600,
        .control_mode = 1,
        .alarm_mask = 0x00FF
    };
    
    modbus_registers_t hvac_regs;
    modbus_registers_init(&hvac_regs, 4);
    
    status = modbus_pack_hvac_registers(&hvac_original, &hvac_regs);
    assert(status == PTK_OK);
    printf("✓ HVAC registers packed\n");
    
    hvac_control_registers_t hvac_decoded;
    status = modbus_unpack_hvac_registers(&hvac_regs, &hvac_decoded);
    assert(status == PTK_OK);
    
    assert(hvac_decoded.setpoint_temperature == hvac_original.setpoint_temperature);
    assert(hvac_decoded.setpoint_humidity == hvac_original.setpoint_humidity);
    assert(hvac_decoded.control_mode == hvac_original.control_mode);
    assert(hvac_decoded.alarm_mask == hvac_original.alarm_mask);
    printf("✓ HVAC registers unpacked correctly\n");
    
    // === Cleanup ===
    modbus_write_multiple_request_destroy(&request);
    modbus_write_multiple_frame_destroy(&frame);
    modbus_write_multiple_frame_destroy(&decoded_frame);
    modbus_write_response_frame_destroy(&response_frame);
    modbus_registers_destroy(&hvac_regs);
    
    printf("\n=== All Tests Passed! ===\n");
    printf("Successfully demonstrated:\n");
    printf("  ✓ Modbus TCP frame creation\n");
    printf("  ✓ Variable-length register arrays\n");
    printf("  ✓ Complete serialization/deserialization\n");
    printf("  ✓ Wire format validation\n");
    printf("  ✓ Response generation\n");
    printf("  ✓ Exception handling\n");
    printf("  ✓ Application-specific data mapping\n");
    
    return 0;
}