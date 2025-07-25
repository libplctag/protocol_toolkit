#include "modbus_tcp_example.h"
#include <stdlib.h>
#include <string.h>

/**
 * Implementation of Modbus TCP PDUs and custom types
 */

/* Implement basic PDUs */
PTK_IMPLEMENT_PDU(modbus_mbap_header)
PTK_IMPLEMENT_PDU(modbus_write_multiple_response)
PTK_IMPLEMENT_PDU(modbus_exception_response)

/* Implement custom PDUs */
PTK_IMPLEMENT_PDU_CUSTOM(modbus_write_multiple_request)
PTK_IMPLEMENT_PDU_CUSTOM(modbus_write_multiple_frame)
PTK_IMPLEMENT_PDU_CUSTOM(modbus_write_response_frame)
PTK_IMPLEMENT_PDU_CUSTOM(modbus_generic_frame)

/**
 * Modbus Registers Array Implementation
 */
ptk_status_t modbus_registers_serialize(ptk_slice_bytes_t* slice, const modbus_registers_t* regs, ptk_endian_t endian) {
    if (!slice || !regs) return PTK_ERROR_INVALID_PARAM;
    
    // Serialize each register in big-endian (Modbus standard)
    for (uint8_t i = 0; i < regs->count; i++) {
        *slice = ptk_write_uint16(*slice, regs->registers[i], PTK_ENDIAN_BIG);
        if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    }
    
    return PTK_OK;
}

ptk_status_t modbus_registers_deserialize(ptk_slice_bytes_t* slice, modbus_registers_t* regs, ptk_endian_t endian) {
    if (!slice || !regs) return PTK_ERROR_INVALID_PARAM;
    
    // The count should be set by the calling PDU based on byte_count or quantity
    if (regs->count > regs->capacity) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    // Deserialize each register in big-endian (Modbus standard)
    for (uint8_t i = 0; i < regs->count; i++) {
        regs->registers[i] = ptk_read_uint16(slice, PTK_ENDIAN_BIG);
    }
    
    return PTK_OK;
}

size_t modbus_registers_size(const modbus_registers_t* regs) {
    if (!regs) return 0;
    return regs->count * 2; // Each register is 2 bytes
}

void modbus_registers_init(modbus_registers_t* regs, size_t capacity) {
    if (!regs) return;
    
    regs->count = 0;
    regs->capacity = capacity;
    regs->registers = (capacity > 0) ? malloc(capacity * sizeof(uint16_t)) : NULL;
}

void modbus_registers_destroy(modbus_registers_t* regs) {
    if (!regs) return;
    
    free(regs->registers);
    regs->registers = NULL;
    regs->count = 0;
    regs->capacity = 0;
}

void modbus_registers_print(const modbus_registers_t* regs) {
    if (!regs) {
        printf("NULL registers");
        return;
    }
    
    printf("Registers[%u]: { ", regs->count);
    for (uint8_t i = 0; i < regs->count; i++) {
        printf("0x%04X", regs->registers[i]);
        if (i < regs->count - 1) printf(", ");
    }
    printf(" }");
}

/**
 * Generic PDU Implementation (handles different PDU types)
 */
ptk_status_t modbus_generic_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_generic_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM;
    
    switch (pdu->type) {
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_REQUEST:
            return modbus_write_multiple_request_serialize(slice, &pdu->data.write_multiple_req, endian);
            
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_RESPONSE:
            return modbus_write_multiple_response_serialize(slice, &pdu->data.write_multiple_resp, endian);
            
        case MODBUS_PDU_TYPE_EXCEPTION_RESPONSE:
            return modbus_exception_response_serialize(slice, &pdu->data.exception_resp, endian);
            
        default:
            return PTK_ERROR_INVALID_PARAM;
    }
}

ptk_status_t modbus_generic_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_generic_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM;
    
    // Peek at the function code to determine PDU type
    if (slice->len < 1) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    uint8_t function_code = slice->data[0];
    
    // Check if it's an exception response (function code + 0x80)
    if (function_code & 0x80) {
        pdu->type = MODBUS_PDU_TYPE_EXCEPTION_RESPONSE;
        return modbus_exception_response_deserialize(slice, &pdu->data.exception_resp, endian);
    }
    
    // Determine PDU type based on function code
    switch (function_code) {
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            // Could be request or response - need to look at length
            if (slice->len >= 6) { // Minimum for request with 1 register
                pdu->type = MODBUS_PDU_TYPE_WRITE_MULTIPLE_REQUEST;
                // Set up register count based on byte_count field
                uint8_t byte_count = slice->data[5];
                pdu->data.write_multiple_req.register_values.count = byte_count / 2;
                return modbus_write_multiple_request_deserialize(slice, &pdu->data.write_multiple_req, endian);
            } else if (slice->len == 5) { // Response is exactly 5 bytes
                pdu->type = MODBUS_PDU_TYPE_WRITE_MULTIPLE_RESPONSE;
                return modbus_write_multiple_response_deserialize(slice, &pdu->data.write_multiple_resp, endian);
            }
            break;
            
        default:
            return PTK_ERROR_INVALID_PARAM;
    }
    
    return PTK_ERROR_INVALID_PARAM;
}

size_t modbus_generic_pdu_size(const modbus_generic_pdu_t* pdu) {
    if (!pdu) return 0;
    
    switch (pdu->type) {
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_REQUEST:
            return modbus_write_multiple_request_size(&pdu->data.write_multiple_req);
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_RESPONSE:
            return modbus_write_multiple_response_size(&pdu->data.write_multiple_resp);
        case MODBUS_PDU_TYPE_EXCEPTION_RESPONSE:
            return modbus_exception_response_size(&pdu->data.exception_resp);
        default:
            return 0;
    }
}

void modbus_generic_pdu_init(modbus_generic_pdu_t* pdu) {
    if (!pdu) return;
    memset(pdu, 0, sizeof(*pdu));
}

void modbus_generic_pdu_destroy(modbus_generic_pdu_t* pdu) {
    if (!pdu) return;
    
    switch (pdu->type) {
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_REQUEST:
            modbus_write_multiple_request_destroy(&pdu->data.write_multiple_req);
            break;
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_RESPONSE:
            modbus_write_multiple_response_destroy(&pdu->data.write_multiple_resp);
            break;
        case MODBUS_PDU_TYPE_EXCEPTION_RESPONSE:
            modbus_exception_response_destroy(&pdu->data.exception_resp);
            break;
    }
    
    pdu->type = MODBUS_PDU_TYPE_WRITE_MULTIPLE_REQUEST; // Reset to default
}

void modbus_generic_pdu_print(const modbus_generic_pdu_t* pdu) {
    if (!pdu) {
        printf("NULL PDU");
        return;
    }
    
    printf("Modbus PDU (");
    switch (pdu->type) {
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_REQUEST:
            printf("Write Multiple Request): ");
            modbus_write_multiple_request_print(&pdu->data.write_multiple_req);
            break;
        case MODBUS_PDU_TYPE_WRITE_MULTIPLE_RESPONSE:
            printf("Write Multiple Response): ");
            modbus_write_multiple_response_print(&pdu->data.write_multiple_resp);
            break;
        case MODBUS_PDU_TYPE_EXCEPTION_RESPONSE:
            printf("Exception Response): ");
            modbus_exception_response_print(&pdu->data.exception_resp);
            break;
        default:
            printf("Unknown): <invalid>");
            break;
    }
}

/**
 * Helper Functions
 */
ptk_status_t modbus_create_write_multiple_request(
    modbus_write_multiple_request_t* request,
    uint16_t starting_address,
    const uint16_t* register_values,
    uint8_t count
) {
    if (!request || !register_values || count == 0 || count > MODBUS_MAX_REGISTERS_WRITE) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    request->function_code = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    request->starting_address = starting_address;
    request->quantity_of_registers = count;
    request->byte_count = count * 2;
    
    // Initialize register values
    modbus_registers_init(&request->register_values, count);
    request->register_values.count = count;
    
    // Copy register values
    memcpy(request->register_values.registers, register_values, count * sizeof(uint16_t));
    
    return PTK_OK;
}

ptk_status_t modbus_create_write_multiple_response(
    modbus_write_multiple_response_t* response,
    uint16_t starting_address,
    uint16_t quantity
) {
    if (!response) return PTK_ERROR_INVALID_PARAM;
    
    response->function_code = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    response->starting_address = starting_address;
    response->quantity_of_registers = quantity;
    
    return PTK_OK;
}

ptk_status_t modbus_create_exception_response(
    modbus_exception_response_t* response,
    uint8_t function_code,
    uint8_t exception_code
) {
    if (!response) return PTK_ERROR_INVALID_PARAM;
    
    response->function_code = function_code | 0x80; // Set exception bit
    response->exception_code = exception_code;
    
    return PTK_OK;
}

ptk_status_t modbus_create_mbap_header(
    modbus_mbap_header_t* header,
    uint16_t transaction_id,
    uint8_t unit_id,
    uint16_t pdu_length
) {
    if (!header) return PTK_ERROR_INVALID_PARAM;
    
    header->transaction_id = transaction_id;
    header->protocol_id = MODBUS_TCP_PROTOCOL_ID;
    header->length = pdu_length + 1; // PDU length + unit ID
    header->unit_id = unit_id;
    
    return PTK_OK;
}

bool modbus_validate_write_multiple_request(const modbus_write_multiple_request_t* request) {
    if (!request) return false;
    
    // Check function code
    if (request->function_code != MODBUS_FC_WRITE_MULTIPLE_REGISTERS) return false;
    
    // Check quantity limits
    if (request->quantity_of_registers == 0 || request->quantity_of_registers > MODBUS_MAX_REGISTERS_WRITE) {
        return false;
    }
    
    // Check byte count consistency
    if (request->byte_count != request->quantity_of_registers * 2) return false;
    
    // Check register array consistency
    if (request->register_values.count != request->quantity_of_registers) return false;
    
    return true;
}

/**
 * Application-Specific Helper Functions
 */
ptk_status_t modbus_pack_hvac_registers(const hvac_control_registers_t* hvac, modbus_registers_t* regs) {
    if (!hvac || !regs) return PTK_ERROR_INVALID_PARAM;
    if (regs->capacity < 4) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    regs->registers[0] = hvac->setpoint_temperature;
    regs->registers[1] = hvac->setpoint_humidity;
    regs->registers[2] = hvac->control_mode;
    regs->registers[3] = hvac->alarm_mask;
    regs->count = 4;
    
    return PTK_OK;
}

ptk_status_t modbus_unpack_hvac_registers(const modbus_registers_t* regs, hvac_control_registers_t* hvac) {
    if (!regs || !hvac || regs->count < 4) return PTK_ERROR_INVALID_PARAM;
    
    hvac->setpoint_temperature = regs->registers[0];
    hvac->setpoint_humidity = regs->registers[1];
    hvac->control_mode = regs->registers[2];
    hvac->alarm_mask = regs->registers[3];
    
    return PTK_OK;
}

ptk_status_t modbus_pack_motor_registers(const motor_control_registers_t* motor, modbus_registers_t* regs) {
    if (!motor || !regs) return PTK_ERROR_INVALID_PARAM;
    if (regs->capacity < 4) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    regs->registers[0] = motor->motor_speed_rpm;
    regs->registers[1] = motor->motor_torque_percent;
    regs->registers[2] = motor->motor_direction;
    regs->registers[3] = motor->motor_enable;
    regs->count = 4;
    
    return PTK_OK;
}

ptk_status_t modbus_unpack_motor_registers(const modbus_registers_t* regs, motor_control_registers_t* motor) {
    if (!regs || !motor || regs->count < 4) return PTK_ERROR_INVALID_PARAM;
    
    motor->motor_speed_rpm = regs->registers[0];
    motor->motor_torque_percent = regs->registers[1];
    motor->motor_direction = regs->registers[2];
    motor->motor_enable = regs->registers[3];
    
    return PTK_OK;
}

/**
 * Demonstration Function
 */
void demonstrate_modbus_tcp(void) {
    printf("=== Modbus TCP Write Multiple Registers Demo ===\n\n");
    
    uint8_t buffer[512];
    ptk_slice_bytes_t slice = ptk_slice_bytes_make(buffer, sizeof(buffer));
    
    // Example 1: Create a write multiple registers request
    printf("1. Creating Write Multiple Registers Request:\n");
    
    // HVAC control example
    hvac_control_registers_t hvac_settings = {
        .setpoint_temperature = 235,    // 23.5°C
        .setpoint_humidity = 450,       // 45.0%
        .control_mode = 1,              // Manual mode
        .alarm_mask = 0x00FF            // All alarms enabled
    };
    
    modbus_write_multiple_request_t request;
    modbus_write_multiple_request_init(&request);
    
    // Pack HVAC settings into registers
    modbus_registers_t hvac_regs;
    modbus_registers_init(&hvac_regs, 4);
    modbus_pack_hvac_registers(&hvac_settings, &hvac_regs);
    
    // Create the request
    modbus_create_write_multiple_request(&request, 1000, hvac_regs.registers, hvac_regs.count);
    
    printf("HVAC Settings:\n");
    printf("  Temperature: %.1f°C\n", hvac_settings.setpoint_temperature / 10.0);
    printf("  Humidity: %.1f%%\n", hvac_settings.setpoint_humidity / 10.0);
    printf("  Mode: %s\n", hvac_settings.control_mode == 1 ? "Manual" : "Auto");
    printf("  Alarms: 0x%04X\n", hvac_settings.alarm_mask);
    
    printf("\nModbus Request:\n");
    modbus_write_multiple_request_print(&request);
    printf("\nRequest size: %zu bytes\n", modbus_write_multiple_request_size(&request));
    
    // Example 2: Create complete Modbus TCP frame
    printf("\n2. Creating Complete Modbus TCP Frame:\n");
    
    modbus_write_multiple_frame_t frame;
    MODBUS_CREATE_WRITE_MULTIPLE_FRAME(frame, 0x1234, 0x01, 1000, hvac_regs.registers, hvac_regs.count);
    
    printf("Complete Frame:\n");
    modbus_write_multiple_frame_print(&frame);
    printf("Frame size: %zu bytes\n", modbus_write_multiple_frame_size(&frame));
    
    // Example 3: Serialize the complete frame
    printf("\n3. Serializing Modbus TCP Frame:\n");
    
    ptk_slice_bytes_t write_slice = slice;
    ptk_status_t status = modbus_write_multiple_frame_serialize(&write_slice, &frame, PTK_ENDIAN_BIG);
    printf("Serialization: %s\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    if (status == PTK_OK) {
        size_t bytes_written = sizeof(buffer) - write_slice.len;
        printf("Bytes written: %zu\n", bytes_written);
        
        printf("Raw bytes: ");
        for (size_t i = 0; i < bytes_written; i++) {
            printf("%02X ", buffer[i]);
            if ((i + 1) % 8 == 0) printf("\n           ");
        }
        printf("\n");
        
        // Break down the frame
        printf("\nFrame breakdown:\n");
        printf("  MBAP Header (7 bytes):\n");
        printf("    Transaction ID: 0x%02X%02X\n", buffer[0], buffer[1]);
        printf("    Protocol ID:    0x%02X%02X\n", buffer[2], buffer[3]);
        printf("    Length:         0x%02X%02X (%u)\n", buffer[4], buffer[5], (buffer[4] << 8) | buffer[5]);
        printf("    Unit ID:        0x%02X\n", buffer[6]);
        
        printf("  PDU (%zu bytes):\n", bytes_written - 7);
        printf("    Function Code:  0x%02X (Write Multiple Registers)\n", buffer[7]);
        printf("    Start Address:  0x%02X%02X (%u)\n", buffer[8], buffer[9], (buffer[8] << 8) | buffer[9]);
        printf("    Quantity:       0x%02X%02X (%u registers)\n", buffer[10], buffer[11], (buffer[10] << 8) | buffer[11]);
        printf("    Byte Count:     0x%02X (%u bytes)\n", buffer[12], buffer[12]);
        printf("    Register Data:  ");
        for (size_t i = 13; i < bytes_written; i += 2) {
            printf("0x%02X%02X ", buffer[i], buffer[i+1]);
        }
        printf("\n");
    }
    
    // Example 4: Create response
    printf("\n4. Creating Response:\n");
    
    modbus_write_multiple_response_t response;
    modbus_write_multiple_response_init(&response);
    modbus_create_write_multiple_response(&response, 1000, 4);
    
    printf("Response:\n");
    modbus_write_multiple_response_print(&response);
    printf("Response size: %zu bytes\n", modbus_write_multiple_response_size(&response));
    
    // Example 5: Exception response
    printf("\n5. Creating Exception Response:\n");
    
    modbus_exception_response_t exception;
    modbus_exception_response_init(&exception);
    modbus_create_exception_response(&exception, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    
    printf("Exception Response:\n");
    modbus_exception_response_print(&exception);
    printf("Exception size: %zu bytes\n", modbus_exception_response_size(&exception));
    
    // Cleanup
    modbus_write_multiple_request_destroy(&request);
    modbus_write_multiple_frame_destroy(&frame);
    modbus_registers_destroy(&hvac_regs);
    
    printf("\n=== End Modbus TCP Demo ===\n");
}