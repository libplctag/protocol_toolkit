#ifndef MODBUS_TCP_EXAMPLE_H
#define MODBUS_TCP_EXAMPLE_H

#include "ptk_pdu_custom.h"
#include <stdio.h>

/**
 * Modbus TCP Protocol Example using PDU System
 * 
 * Demonstrates Write Multiple Holding Registers (Function Code 0x10)
 * Request/Response pair with variable-length register data.
 */

/**
 * Modbus Function Codes
 */
#define MODBUS_FC_READ_COILS                    0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS          0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS        0x03
#define MODBUS_FC_READ_INPUT_REGISTERS          0x04
#define MODBUS_FC_WRITE_SINGLE_COIL             0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER         0x06
#define MODBUS_FC_WRITE_MULTIPLE_COILS          0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS      0x10

/**
 * Modbus Exception Codes
 */
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION       0x01
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS   0x02
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE     0x03
#define MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE   0x04

/**
 * Custom types for Modbus variable-length data
 */

/* Register array for multiple register operations */
typedef struct {
    uint16_t* registers;    // Array of register values
    uint8_t count;          // Number of registers (max 123 for FC 0x10)
    size_t capacity;        // Allocated capacity
} modbus_registers_t;

/* Raw byte data for write operations */
typedef struct {
    uint8_t* data;          // Raw byte data
    uint8_t byte_count;     // Number of bytes
    size_t capacity;        // Allocated capacity
} modbus_byte_data_t;

/**
 * Modbus TCP MBAP Header (7 bytes)
 */
#define PTK_PDU_FIELDS_modbus_mbap_header(X) \
    X(PTK_PDU_U16, transaction_id) \
    X(PTK_PDU_U16, protocol_id) \
    X(PTK_PDU_U16, length) \
    X(PTK_PDU_U8,  unit_id)

PTK_DECLARE_PDU(modbus_mbap_header)

/**
 * Write Multiple Holding Registers Request (Function Code 0x10)
 * Format: FC + Start Address + Quantity + Byte Count + Register Values
 */
#define PTK_PDU_FIELDS_modbus_write_multiple_request(X) \
    X(PTK_PDU_U8,  function_code) \
    X(PTK_PDU_U16, starting_address) \
    X(PTK_PDU_U16, quantity_of_registers) \
    X(PTK_PDU_U8,  byte_count) \
    X(PTK_PDU_CUSTOM, register_values, modbus_registers_t)

PTK_DECLARE_PDU_CUSTOM(modbus_write_multiple_request)

/**
 * Write Multiple Holding Registers Response (Function Code 0x10)
 * Format: FC + Start Address + Quantity (echo of request)
 */
#define PTK_PDU_FIELDS_modbus_write_multiple_response(X) \
    X(PTK_PDU_U8,  function_code) \
    X(PTK_PDU_U16, starting_address) \
    X(PTK_PDU_U16, quantity_of_registers)

PTK_DECLARE_PDU(modbus_write_multiple_response)

/**
 * Modbus Exception Response (for any function code)
 * Format: (FC + 0x80) + Exception Code
 */
#define PTK_PDU_FIELDS_modbus_exception_response(X) \
    X(PTK_PDU_U8, function_code) \
    X(PTK_PDU_U8, exception_code)

PTK_DECLARE_PDU(modbus_exception_response)

/**
 * Complete Modbus TCP Write Multiple Registers Frame
 * MBAP Header + PDU
 */
#define PTK_PDU_FIELDS_modbus_write_multiple_frame(X) \
    X(PTK_PDU_NESTED, mbap, modbus_mbap_header) \
    X(PTK_PDU_CUSTOM, pdu, modbus_write_multiple_request)

PTK_DECLARE_PDU_CUSTOM(modbus_write_multiple_frame)

/**
 * Complete Modbus TCP Write Multiple Response Frame
 */
#define PTK_PDU_FIELDS_modbus_write_response_frame(X) \
    X(PTK_PDU_NESTED, mbap, modbus_mbap_header) \
    X(PTK_PDU_CUSTOM, pdu, modbus_write_multiple_response)

PTK_DECLARE_PDU_CUSTOM(modbus_write_response_frame)

/**
 * Generic Modbus Request/Response with conditional PDU types
 * This shows how to handle different function codes in one structure
 */
typedef enum {
    MODBUS_PDU_TYPE_WRITE_MULTIPLE_REQUEST,
    MODBUS_PDU_TYPE_WRITE_MULTIPLE_RESPONSE,
    MODBUS_PDU_TYPE_EXCEPTION_RESPONSE,
    MODBUS_PDU_TYPE_READ_HOLDING_REQUEST,
    MODBUS_PDU_TYPE_READ_HOLDING_RESPONSE
} modbus_pdu_type_t;

typedef struct {
    modbus_pdu_type_t type;
    union {
        modbus_write_multiple_request_t write_multiple_req;
        modbus_write_multiple_response_t write_multiple_resp;
        modbus_exception_response_t exception_resp;
        // Add other PDU types as needed
    } data;
} modbus_generic_pdu_t;

#define PTK_PDU_FIELDS_modbus_generic_frame(X) \
    X(PTK_PDU_NESTED, mbap, modbus_mbap_header) \
    X(PTK_PDU_CUSTOM, pdu, modbus_generic_pdu_t)

PTK_DECLARE_PDU_CUSTOM(modbus_generic_frame)

/**
 * Custom type function declarations
 */

/* Modbus registers array functions */
ptk_status_t modbus_registers_serialize(ptk_slice_bytes_t* slice, const modbus_registers_t* regs, ptk_endian_t endian);
ptk_status_t modbus_registers_deserialize(ptk_slice_bytes_t* slice, modbus_registers_t* regs, ptk_endian_t endian);
size_t modbus_registers_size(const modbus_registers_t* regs);
void modbus_registers_init(modbus_registers_t* regs, size_t capacity);
void modbus_registers_destroy(modbus_registers_t* regs);
void modbus_registers_print(const modbus_registers_t* regs);

/* Modbus byte data functions */
ptk_status_t modbus_byte_data_serialize(ptk_slice_bytes_t* slice, const modbus_byte_data_t* data, ptk_endian_t endian);
ptk_status_t modbus_byte_data_deserialize(ptk_slice_bytes_t* slice, modbus_byte_data_t* data, ptk_endian_t endian);
size_t modbus_byte_data_size(const modbus_byte_data_t* data);
void modbus_byte_data_init(modbus_byte_data_t* data, size_t capacity);
void modbus_byte_data_destroy(modbus_byte_data_t* data);
void modbus_byte_data_print(const modbus_byte_data_t* data);

/* Generic PDU functions */
ptk_status_t modbus_generic_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_generic_pdu_t* pdu, ptk_endian_t endian);
ptk_status_t modbus_generic_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_generic_pdu_t* pdu, ptk_endian_t endian);
size_t modbus_generic_pdu_size(const modbus_generic_pdu_t* pdu);
void modbus_generic_pdu_init(modbus_generic_pdu_t* pdu);
void modbus_generic_pdu_destroy(modbus_generic_pdu_t* pdu);
void modbus_generic_pdu_print(const modbus_generic_pdu_t* pdu);

/**
 * Helper functions for Modbus operations
 */

/* Create a write multiple registers request */
ptk_status_t modbus_create_write_multiple_request(
    modbus_write_multiple_request_t* request,
    uint16_t starting_address,
    const uint16_t* register_values,
    uint8_t count
);

/* Create a write multiple registers response */
ptk_status_t modbus_create_write_multiple_response(
    modbus_write_multiple_response_t* response,
    uint16_t starting_address,
    uint16_t quantity
);

/* Create an exception response */
ptk_status_t modbus_create_exception_response(
    modbus_exception_response_t* response,
    uint8_t function_code,
    uint8_t exception_code
);

/* Create MBAP header */
ptk_status_t modbus_create_mbap_header(
    modbus_mbap_header_t* header,
    uint16_t transaction_id,
    uint8_t unit_id,
    uint16_t pdu_length
);

/* Validate Modbus request parameters */
bool modbus_validate_write_multiple_request(const modbus_write_multiple_request_t* request);

/* Calculate PDU length for MBAP header */
size_t modbus_calculate_pdu_length(const void* pdu, modbus_pdu_type_t type);

/**
 * Utility macros for common Modbus operations
 */

/* Create a complete write multiple registers frame */
#define MODBUS_CREATE_WRITE_MULTIPLE_FRAME(frame, trans_id, unit, start_addr, regs, count) \
    do { \
        modbus_write_multiple_frame_init(&(frame)); \
        modbus_create_mbap_header(&(frame).mbap, trans_id, unit, \
            6 + 1 + (count * 2)); /* FC + addr + qty + count + data */ \
        modbus_create_write_multiple_request(&(frame).pdu, start_addr, regs, count); \
    } while(0)

/* Create a complete write multiple response frame */
#define MODBUS_CREATE_WRITE_RESPONSE_FRAME(frame, trans_id, unit, start_addr, qty) \
    do { \
        modbus_write_response_frame_init(&(frame)); \
        modbus_create_mbap_header(&(frame).mbap, trans_id, unit, 5); /* FC + addr + qty */ \
        modbus_create_write_multiple_response(&(frame).pdu, start_addr, qty); \
    } while(0)

/**
 * Modbus TCP constants
 */
#define MODBUS_TCP_PORT                 502
#define MODBUS_TCP_PROTOCOL_ID          0x0000
#define MODBUS_MAX_REGISTERS_WRITE      123
#define MODBUS_MAX_PDU_LENGTH           253

/**
 * Example register layouts for common applications
 */
typedef struct {
    uint16_t setpoint_temperature;      // °C * 10
    uint16_t setpoint_humidity;         // % * 10
    uint16_t control_mode;              // 0=Auto, 1=Manual, 2=Off
    uint16_t alarm_mask;                // Bit field
} hvac_control_registers_t;

typedef struct {
    uint16_t motor_speed_rpm;           // RPM
    uint16_t motor_torque_percent;      // % * 10
    uint16_t motor_direction;           // 0=Forward, 1=Reverse
    uint16_t motor_enable;              // 0=Disabled, 1=Enabled
} motor_control_registers_t;

/* Helper to convert typed structures to register arrays */
ptk_status_t modbus_pack_hvac_registers(const hvac_control_registers_t* hvac, modbus_registers_t* regs);
ptk_status_t modbus_unpack_hvac_registers(const modbus_registers_t* regs, hvac_control_registers_t* hvac);

ptk_status_t modbus_pack_motor_registers(const motor_control_registers_t* motor, modbus_registers_t* regs);
ptk_status_t modbus_unpack_motor_registers(const modbus_registers_t* regs, motor_control_registers_t* motor);

#endif /* MODBUS_TCP_EXAMPLE_H */