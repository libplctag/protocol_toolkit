// Generated header from modbus_basic.pdl
// This shows what the generator would create

#pragma once

#include "ptk_array.h"
#include "ptk_allocator.h"
#include "ptk_buf.h"
#include "ptk_err.h"

//=============================================================================
// MESSAGE TYPE ENUMERATION
//=============================================================================

typedef enum {
    MODBUS_MESSAGE_TYPE_READ_HOLDING_REGISTERS_REQUEST = 1,
    MODBUS_MESSAGE_TYPE_READ_HOLDING_REGISTERS_RESPONSE = 2,
    MODBUS_MESSAGE_TYPE_READ_COILS_REQUEST = 3,
    MODBUS_MESSAGE_TYPE_READ_COILS_RESPONSE = 4,
} modbus_message_type_t;

//=============================================================================
// CONSTANTS
//=============================================================================

#define MODBUS_READ_HOLDING_REGISTERS 0x03
#define MODBUS_READ_COILS 0x01

//=============================================================================
// MESSAGE STRUCTURES
//=============================================================================

// Read Multiple Holding Registers Request (Function Code 0x03)
typedef struct {
    const int message_type; // MODBUS_MESSAGE_TYPE_READ_HOLDING_REGISTERS_REQUEST
    u16 starting_address;
    u16 quantity;
} read_holding_registers_request_t;

// Read Multiple Holding Registers Response (Function Code 0x03)
typedef struct {
    const int message_type; // MODBUS_MESSAGE_TYPE_READ_HOLDING_REGISTERS_RESPONSE
    u8 byte_count;
    u16_array_t *register_values; // Array of u16 values
} read_holding_registers_response_t;

// Read Multiple Coils Request (Function Code 0x01)
typedef struct {
    const int message_type; // MODBUS_MESSAGE_TYPE_READ_COILS_REQUEST
    u16 starting_address;
    u16 quantity;
} read_coils_request_t;

// Read Multiple Coils Response (Function Code 0x01)
typedef struct {
    const int message_type; // MODBUS_MESSAGE_TYPE_READ_COILS_RESPONSE
    u8 byte_count;
    u8_bit_array_t *coil_status; // Bit array with u8 containers
} read_coils_response_t;

//=============================================================================
// DISCRIMINATED UNION FOR PDU PAYLOAD
//=============================================================================

typedef enum {
    MODBUS_PDU_PAYLOAD_TYPE_READ_HOLDING_REGISTERS_REQUEST,
    MODBUS_PDU_PAYLOAD_TYPE_READ_HOLDING_REGISTERS_RESPONSE,
    MODBUS_PDU_PAYLOAD_TYPE_READ_COILS_REQUEST,
    MODBUS_PDU_PAYLOAD_TYPE_READ_COILS_RESPONSE,
} modbus_pdu_payload_type_t;

typedef struct {
    modbus_pdu_payload_type_t payload_type;
    union {
        read_holding_registers_request_t *read_holding_registers_request;
        read_holding_registers_response_t *read_holding_registers_response;
        read_coils_request_t *read_coils_request;
        read_coils_response_t *read_coils_response;
    } payload_value;
} modbus_pdu_payload_t;

//=============================================================================
// HEADER AND CONTAINER STRUCTURES
//=============================================================================

// ADU header (Application Data Unit) - simplified for TCP
typedef struct {
    const int message_type;
    u16 transaction_id;
    u16 protocol_id; // Always 0x0000
    u16 length;
    u8 unit_id;
} modbus_adu_header_t;

// PDU (Protocol Data Unit) - the actual Modbus message
typedef struct {
    const int message_type;
    u8 function_code;
    modbus_pdu_payload_t payload;
} modbus_pdu_t;

// Complete Modbus message
typedef struct {
    const int message_type;
    modbus_adu_header_t header;
    modbus_pdu_t pdu;
} modbus_message_t;

//=============================================================================
// CONSTRUCTOR/DESTRUCTOR FUNCTIONS
//=============================================================================

ptk_err read_holding_registers_request_create(ptk_allocator_t *alloc, read_holding_registers_request_t **msg);
void read_holding_registers_request_dispose(ptk_allocator_t *alloc, read_holding_registers_request_t *msg);

ptk_err read_holding_registers_response_create(ptk_allocator_t *alloc, read_holding_registers_response_t **msg);
void read_holding_registers_response_dispose(ptk_allocator_t *alloc, read_holding_registers_response_t *msg);

ptk_err read_coils_request_create(ptk_allocator_t *alloc, read_coils_request_t **msg);
void read_coils_request_dispose(ptk_allocator_t *alloc, read_coils_request_t *msg);

ptk_err read_coils_response_create(ptk_allocator_t *alloc, read_coils_response_t **msg);
void read_coils_response_dispose(ptk_allocator_t *alloc, read_coils_response_t *msg);

ptk_err modbus_pdu_create(ptk_allocator_t *alloc, modbus_pdu_t **msg);
void modbus_pdu_dispose(ptk_allocator_t *alloc, modbus_pdu_t *msg);

ptk_err modbus_message_create(ptk_allocator_t *alloc, modbus_message_t **msg);
void modbus_message_dispose(ptk_allocator_t *alloc, modbus_message_t *msg);

//=============================================================================
// ENCODE/DECODE FUNCTIONS
//=============================================================================

ptk_err read_holding_registers_request_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_holding_registers_request_t *msg);
ptk_err read_holding_registers_request_decode(ptk_allocator_t *alloc, read_holding_registers_request_t **msg, ptk_buf_t *buf);

ptk_err read_holding_registers_response_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_holding_registers_response_t *msg);
ptk_err read_holding_registers_response_decode(ptk_allocator_t *alloc, read_holding_registers_response_t **msg, ptk_buf_t *buf);

ptk_err read_coils_request_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_coils_request_t *msg);
ptk_err read_coils_request_decode(ptk_allocator_t *alloc, read_coils_request_t **msg, ptk_buf_t *buf);

ptk_err read_coils_response_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_coils_response_t *msg);
ptk_err read_coils_response_decode(ptk_allocator_t *alloc, read_coils_response_t **msg, ptk_buf_t *buf);

ptk_err modbus_pdu_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const modbus_pdu_t *msg);
ptk_err modbus_pdu_decode(ptk_allocator_t *alloc, modbus_pdu_t **msg, ptk_buf_t *buf);

ptk_err modbus_message_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const modbus_message_t *msg);
ptk_err modbus_message_decode(ptk_allocator_t *alloc, modbus_message_t **msg, ptk_buf_t *buf);

//=============================================================================
// SAFE ARRAY ACCESSORS
//=============================================================================

// Read Holding Registers Response - register_values array
ptk_err read_holding_registers_response_get_register_value(const read_holding_registers_response_t *msg, size_t index, u16 *value);
ptk_err read_holding_registers_response_set_register_value(read_holding_registers_response_t *msg, size_t index, u16 value);
size_t read_holding_registers_response_get_register_count(const read_holding_registers_response_t *msg);

// Read Coils Response - bit-level convenience accessors (hide container complexity)
static inline ptk_err read_coils_response_get_coil_status(const read_coils_response_t *msg, size_t bit_index, bool *value) {
    if (!msg || !msg->coil_status) return PTK_ERR_NULL_PTR;
    return u8_bit_array_safe_get(msg->coil_status, bit_index, value);
}

static inline ptk_err read_coils_response_set_coil_status(read_coils_response_t *msg, size_t bit_index, bool value) {
    if (!msg || !msg->coil_status) return PTK_ERR_NULL_PTR;
    return u8_bit_array_safe_set(msg->coil_status, bit_index, value);
}

static inline size_t read_coils_response_get_coil_status_length(const read_coils_response_t *msg) {
    return msg && msg->coil_status ? u8_bit_array_safe_len(msg->coil_status) : 0;  // Returns bit count
}

// Read Coils Response - container-level access (efficient byte manipulation)
static inline ptk_err read_coils_response_get_coil_status_container(const read_coils_response_t *msg, size_t container_index, u8 *value) {
    if (!msg || !msg->coil_status) return PTK_ERR_NULL_PTR;
    return u8_bit_array_get_container(msg->coil_status, container_index, value);
}

static inline ptk_err read_coils_response_set_coil_status_container(read_coils_response_t *msg, size_t container_index, u8 value) {
    if (!msg || !msg->coil_status) return PTK_ERR_NULL_PTR;
    return u8_bit_array_set_container(msg->coil_status, container_index, value);
}

static inline size_t read_coils_response_get_coil_status_container_count(const read_coils_response_t *msg) {
    return msg && msg->coil_status ? u8_bit_array_container_count(msg->coil_status) : 0;  // Returns container count
}

//=============================================================================
// CONTEXT AND UTILITY TYPES
//=============================================================================

// Context for maintaining transaction IDs and other state
typedef struct {
    u16 next_transaction_id;
    // Other protocol state...
} modbus_context_t;

ptk_err modbus_context_create(ptk_allocator_t *alloc, modbus_context_t **ctx);
void modbus_context_dispose(ptk_allocator_t *alloc, modbus_context_t *ctx);
u16 modbus_context_get_next_transaction_id(modbus_context_t *ctx);

//=============================================================================
// CONVENIENCE MACROS
//=============================================================================

#define alloc_reset(alloc) ((alloc)->reset ? (alloc)->reset(alloc) : PTK_OK)
#define alloc_free(alloc, ptr) ((alloc)->free ? (alloc)->free(alloc, ptr) : (void)0)