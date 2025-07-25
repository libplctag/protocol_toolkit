#ifndef MODBUS_GENERIC_RESPONSE_H
#define MODBUS_GENERIC_RESPONSE_H

#include "ptk_pdu_custom.h"
#include "modbus_tcp_example.h"

/**
 * Generic Modbus Response Frame with Multiple PDU Types
 * 
 * This demonstrates how to handle different response types within
 * a single frame structure using unions and conditional serialization.
 */

/**
 * Additional Modbus Function Codes for responses
 */
#define MODBUS_FC_READ_COILS_RESPONSE               0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS_RESPONSE     0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS_RESPONSE   0x03
#define MODBUS_FC_READ_INPUT_REGISTERS_RESPONSE     0x04

/**
 * Response type enumeration
 */
typedef enum {
    MODBUS_RESPONSE_TYPE_WRITE_SINGLE_COIL,
    MODBUS_RESPONSE_TYPE_WRITE_SINGLE_REGISTER,
    MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_COILS,
    MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_REGISTERS,
    MODBUS_RESPONSE_TYPE_READ_COILS,
    MODBUS_RESPONSE_TYPE_READ_DISCRETE_INPUTS,
    MODBUS_RESPONSE_TYPE_READ_HOLDING_REGISTERS,
    MODBUS_RESPONSE_TYPE_READ_INPUT_REGISTERS,
    MODBUS_RESPONSE_TYPE_EXCEPTION,
    MODBUS_RESPONSE_TYPE_UNKNOWN
} modbus_response_type_t;

/**
 * Individual response PDU types
 */

/* Read Coils Response - variable length bit data */
typedef struct {
    uint8_t* coil_data;     // Bit-packed coil states
    uint8_t byte_count;     // Number of bytes
    size_t capacity;        // Allocated capacity
} modbus_coil_data_t;

#define PTK_PDU_FIELDS_modbus_read_coils_response(X) \
    X(PTK_PDU_U8, function_code) \
    X(PTK_PDU_U8, byte_count) \
    X(PTK_PDU_CUSTOM, coil_data, modbus_coil_data_t)

PTK_DECLARE_PDU_CUSTOM(modbus_read_coils_response)

/* Read Holding Registers Response - variable length register data */
#define PTK_PDU_FIELDS_modbus_read_holding_response(X) \
    X(PTK_PDU_U8, function_code) \
    X(PTK_PDU_U8, byte_count) \
    X(PTK_PDU_CUSTOM, register_data, modbus_registers_t)

PTK_DECLARE_PDU_CUSTOM(modbus_read_holding_response)

/* Write Single Coil Response - echo of request */
#define PTK_PDU_FIELDS_modbus_write_single_coil_response(X) \
    X(PTK_PDU_U8, function_code) \
    X(PTK_PDU_U16, coil_address) \
    X(PTK_PDU_U16, coil_value)

PTK_DECLARE_PDU(modbus_write_single_coil_response)

/* Write Single Register Response - echo of request */
#define PTK_PDU_FIELDS_modbus_write_single_register_response(X) \
    X(PTK_PDU_U8, function_code) \
    X(PTK_PDU_U16, register_address) \
    X(PTK_PDU_U16, register_value)

PTK_DECLARE_PDU(modbus_write_single_register_response)

/**
 * Generic Response PDU Union
 */
typedef struct {
    modbus_response_type_t response_type;
    union {
        modbus_write_multiple_response_t write_multiple_regs;
        modbus_read_coils_response_t read_coils;
        modbus_read_holding_response_t read_holding;
        modbus_write_single_coil_response_t write_single_coil;
        modbus_write_single_register_response_t write_single_register;
        modbus_exception_response_t exception;
    } pdu;
} modbus_generic_response_pdu_t;

/**
 * Method 1: Manual union handling with custom type
 */
#define PTK_PDU_FIELDS_modbus_generic_response_frame_v1(X) \
    X(PTK_PDU_NESTED, mbap, modbus_mbap_header) \
    X(PTK_PDU_CUSTOM, response, modbus_generic_response_pdu_t)

PTK_DECLARE_PDU_CUSTOM(modbus_generic_response_frame_v1)

/**
 * Method 2: Conditional PDU fields based on function code
 * This uses the function code from MBAP to determine PDU type
 */

/* Helper to extract function code from MBAP context */
typedef struct {
    modbus_mbap_header_t mbap;
    uint8_t function_code;  // Extracted from first PDU byte for conditionals
    union {
        modbus_write_multiple_response_t write_multiple_regs;
        modbus_read_coils_response_t read_coils;
        modbus_read_holding_response_t read_holding;
        modbus_write_single_coil_response_t write_single_coil;
        modbus_write_single_register_response_t write_single_register;
        modbus_exception_response_t exception;
    } pdu_data;
} modbus_conditional_response_frame_t;

/* This would use conditional fields - showing concept */
#define PTK_PDU_FIELDS_modbus_conditional_response_frame(X) \
    X(PTK_PDU_NESTED, mbap, modbus_mbap_header) \
    X(PTK_PDU_U8, function_code) \
    X(PTK_PDU_CONDITIONAL, function_code, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, PTK_PDU_NESTED, write_multiple, modbus_write_multiple_response) \
    X(PTK_PDU_CONDITIONAL, function_code, MODBUS_FC_READ_COILS, PTK_PDU_NESTED, read_coils, modbus_read_coils_response) \
    X(PTK_PDU_CONDITIONAL, function_code, MODBUS_FC_READ_HOLDING_REGISTERS, PTK_PDU_NESTED, read_holding, modbus_read_holding_response)

/* Note: This would need full conditional support implementation */

/**
 * Method 3: Tagged union with explicit type field
 */
typedef enum {
    MODBUS_PDU_TAG_WRITE_MULTIPLE_REGS = MODBUS_FC_WRITE_MULTIPLE_REGISTERS,
    MODBUS_PDU_TAG_READ_COILS = MODBUS_FC_READ_COILS,
    MODBUS_PDU_TAG_READ_HOLDING = MODBUS_FC_READ_HOLDING_REGISTERS,
    MODBUS_PDU_TAG_WRITE_SINGLE_COIL = MODBUS_FC_WRITE_SINGLE_COIL,
    MODBUS_PDU_TAG_WRITE_SINGLE_REGISTER = MODBUS_FC_WRITE_SINGLE_REGISTER,
    MODBUS_PDU_TAG_EXCEPTION = 0x80  // Exception flag
} modbus_pdu_tag_t;

typedef struct {
    modbus_pdu_tag_t tag;
    union {
        modbus_write_multiple_response_t write_multiple_regs;
        modbus_read_coils_response_t read_coils;
        modbus_read_holding_response_t read_holding;
        modbus_write_single_coil_response_t write_single_coil;
        modbus_write_single_register_response_t write_single_register;
        modbus_exception_response_t exception;
    } data;
} modbus_tagged_pdu_t;

#define PTK_PDU_FIELDS_modbus_tagged_response_frame(X) \
    X(PTK_PDU_NESTED, mbap, modbus_mbap_header) \
    X(PTK_PDU_CUSTOM, pdu, modbus_tagged_pdu_t)

PTK_DECLARE_PDU_CUSTOM(modbus_tagged_response_frame)

/**
 * Method 4: Factory pattern with function pointers
 */
typedef struct modbus_response_vtable {
    ptk_status_t (*serialize)(ptk_slice_bytes_t* slice, const void* pdu, ptk_endian_t endian);
    ptk_status_t (*deserialize)(ptk_slice_bytes_t* slice, void* pdu, ptk_endian_t endian);
    size_t (*size)(const void* pdu);
    void (*print)(const void* pdu);
    const char* name;
} modbus_response_vtable_t;

typedef struct {
    const modbus_response_vtable_t* vtable;
    void* pdu_data;
    size_t pdu_size;
} modbus_polymorphic_pdu_t;

#define PTK_PDU_FIELDS_modbus_polymorphic_response_frame(X) \
    X(PTK_PDU_NESTED, mbap, modbus_mbap_header) \
    X(PTK_PDU_CUSTOM, pdu, modbus_polymorphic_pdu_t)

PTK_DECLARE_PDU_CUSTOM(modbus_polymorphic_response_frame)

/**
 * Custom type function declarations
 */

/* Coil data functions */
ptk_status_t modbus_coil_data_serialize(ptk_slice_bytes_t* slice, const modbus_coil_data_t* data, ptk_endian_t endian);
ptk_status_t modbus_coil_data_deserialize(ptk_slice_bytes_t* slice, modbus_coil_data_t* data, ptk_endian_t endian);
size_t modbus_coil_data_size(const modbus_coil_data_t* data);
void modbus_coil_data_init(modbus_coil_data_t* data, size_t capacity);
void modbus_coil_data_destroy(modbus_coil_data_t* data);
void modbus_coil_data_print(const modbus_coil_data_t* data);

/* Generic response PDU functions (Method 1) */
ptk_status_t modbus_generic_response_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_generic_response_pdu_t* pdu, ptk_endian_t endian);
ptk_status_t modbus_generic_response_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_generic_response_pdu_t* pdu, ptk_endian_t endian);
size_t modbus_generic_response_pdu_size(const modbus_generic_response_pdu_t* pdu);
void modbus_generic_response_pdu_init(modbus_generic_response_pdu_t* pdu);
void modbus_generic_response_pdu_destroy(modbus_generic_response_pdu_t* pdu);
void modbus_generic_response_pdu_print(const modbus_generic_response_pdu_t* pdu);

/* Tagged PDU functions (Method 3) */
ptk_status_t modbus_tagged_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_tagged_pdu_t* pdu, ptk_endian_t endian);
ptk_status_t modbus_tagged_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_tagged_pdu_t* pdu, ptk_endian_t endian);
size_t modbus_tagged_pdu_size(const modbus_tagged_pdu_t* pdu);
void modbus_tagged_pdu_init(modbus_tagged_pdu_t* pdu);
void modbus_tagged_pdu_destroy(modbus_tagged_pdu_t* pdu);
void modbus_tagged_pdu_print(const modbus_tagged_pdu_t* pdu);

/* Polymorphic PDU functions (Method 4) */
ptk_status_t modbus_polymorphic_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_polymorphic_pdu_t* pdu, ptk_endian_t endian);
ptk_status_t modbus_polymorphic_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_polymorphic_pdu_t* pdu, ptk_endian_t endian);
size_t modbus_polymorphic_pdu_size(const modbus_polymorphic_pdu_t* pdu);
void modbus_polymorphic_pdu_init(modbus_polymorphic_pdu_t* pdu);
void modbus_polymorphic_pdu_destroy(modbus_polymorphic_pdu_t* pdu);
void modbus_polymorphic_pdu_print(const modbus_polymorphic_pdu_t* pdu);

/**
 * Helper functions for different approaches
 */

/* Method 1: Generic response creation */
ptk_status_t modbus_create_generic_response(
    modbus_generic_response_frame_v1_t* frame,
    uint16_t transaction_id,
    uint8_t unit_id,
    modbus_response_type_t response_type,
    const void* response_data
);

/* Method 3: Tagged PDU creation */
ptk_status_t modbus_create_tagged_response(
    modbus_tagged_response_frame_t* frame,
    uint16_t transaction_id,
    uint8_t unit_id,
    modbus_pdu_tag_t tag,
    const void* pdu_data
);

/* Method 4: Polymorphic PDU creation */
ptk_status_t modbus_create_polymorphic_response(
    modbus_polymorphic_response_frame_t* frame,
    uint16_t transaction_id,
    uint8_t unit_id,
    const modbus_response_vtable_t* vtable,
    const void* pdu_data,
    size_t pdu_size
);

/* Response type detection from function code */
modbus_response_type_t modbus_detect_response_type(uint8_t function_code);
modbus_pdu_tag_t modbus_function_code_to_tag(uint8_t function_code);

/* VTable registry for different response types */
extern const modbus_response_vtable_t modbus_write_multiple_vtable;
extern const modbus_response_vtable_t modbus_read_coils_vtable;
extern const modbus_response_vtable_t modbus_read_holding_vtable;
extern const modbus_response_vtable_t modbus_exception_vtable;

/**
 * Utility macros for different approaches
 */

/* Create generic response frame (Method 1) */
#define MODBUS_CREATE_GENERIC_RESPONSE(frame, trans_id, unit, type, data) \
    modbus_create_generic_response(&(frame), trans_id, unit, type, data)

/* Create tagged response frame (Method 3) */
#define MODBUS_CREATE_TAGGED_RESPONSE(frame, trans_id, unit, tag, data) \
    modbus_create_tagged_response(&(frame), trans_id, unit, tag, data)

/* Create polymorphic response frame (Method 4) */
#define MODBUS_CREATE_POLYMORPHIC_RESPONSE(frame, trans_id, unit, vtable, data, size) \
    modbus_create_polymorphic_response(&(frame), trans_id, unit, vtable, data, size)

/**
 * Example usage patterns
 */

/* Pattern 1: Switch-based dispatch */
ptk_status_t modbus_handle_response_v1(const modbus_generic_response_frame_v1_t* frame);

/* Pattern 2: Tagged union dispatch */
ptk_status_t modbus_handle_response_v2(const modbus_tagged_response_frame_t* frame);

/* Pattern 3: Polymorphic dispatch */
ptk_status_t modbus_handle_response_v3(const modbus_polymorphic_response_frame_t* frame);

#endif /* MODBUS_GENERIC_RESPONSE_H */