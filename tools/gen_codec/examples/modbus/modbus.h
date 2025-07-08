#ifndef MODBUS_MODBUS_H
#define MODBUS_MODBUS_H

#include "ptk_array.h"
#include "ptk_alloc.h"
#include "ptk_buf.h"
#include "ptk_codec.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Message Type Enumeration */
typedef enum {
    MODBUS_TCP_HEADER_MESSAGE_TYPE = 1,
    READ_COILS_REQUEST_MESSAGE_TYPE = 2,
    READ_COILS_RESPONSE_MESSAGE_TYPE = 3,
    READ_DISCRETE_INPUTS_REQUEST_MESSAGE_TYPE = 4,
    READ_DISCRETE_INPUTS_RESPONSE_MESSAGE_TYPE = 5,
    READ_HOLDING_REGISTERS_REQUEST_MESSAGE_TYPE = 6,
    READ_HOLDING_REGISTERS_RESPONSE_MESSAGE_TYPE = 7,
    READ_INPUT_REGISTERS_REQUEST_MESSAGE_TYPE = 8,
    READ_INPUT_REGISTERS_RESPONSE_MESSAGE_TYPE = 9,
    WRITE_SINGLE_COIL_REQUEST_MESSAGE_TYPE = 10,
    WRITE_SINGLE_COIL_RESPONSE_MESSAGE_TYPE = 11,
    WRITE_SINGLE_REGISTER_REQUEST_MESSAGE_TYPE = 12,
    WRITE_SINGLE_REGISTER_RESPONSE_MESSAGE_TYPE = 13,
    WRITE_MULTIPLE_COILS_REQUEST_MESSAGE_TYPE = 14,
    WRITE_MULTIPLE_COILS_RESPONSE_MESSAGE_TYPE = 15,
    WRITE_MULTIPLE_REGISTERS_REQUEST_MESSAGE_TYPE = 16,
    WRITE_MULTIPLE_REGISTERS_RESPONSE_MESSAGE_TYPE = 17,
    MODBUS_EXCEPTION_RESPONSE_MESSAGE_TYPE = 18,
    MODBUS_TCP_PDU_MESSAGE_TYPE = 19,
    MODBUS_TCP_MESSAGE_MESSAGE_TYPE = 20,
} message_type_t;

/* Constants */
#define READ_COILS ((uint8_t)0x01)
#define READ_DISCRETE_INPUTS ((uint8_t)0x02)
#define READ_HOLDING_REGISTERS ((uint8_t)0x03)
#define READ_INPUT_REGISTERS ((uint8_t)0x04)
#define WRITE_SINGLE_COIL ((uint8_t)0x05)
#define WRITE_SINGLE_REGISTER ((uint8_t)0x06)
#define WRITE_MULTIPLE_COILS ((uint8_t)0x0F)
#define WRITE_MULTIPLE_REGISTERS ((uint8_t)0x10)
#define ILLEGAL_FUNCTION ((uint8_t)0x01)
#define ILLEGAL_DATA_ADDRESS ((uint8_t)0x02)
#define ILLEGAL_DATA_VALUE ((uint8_t)0x03)
#define SERVER_DEVICE_FAILURE ((uint8_t)0x04)
#define ACKNOWLEDGE ((uint8_t)0x05)
#define SERVER_DEVICE_BUSY ((uint8_t)0x06)

/* Declare standard array types we'll need */
PTK_ARRAY_DECLARE(uint8_t)
PTK_ARRAY_DECLARE(uint16_t)

/* modbus_tcp_header message definition */
typedef struct modbus_tcp_header_t {
    const int message_type;
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
} modbus_tcp_header_t;

/* Constructor/Destructor */
ptk_err modbus_tcp_header_create(ptk_allocator_t *alloc, modbus_tcp_header_t **instance);
void modbus_tcp_header_dispose(ptk_allocator_t *alloc, modbus_tcp_header_t *instance);

/* Encode/Decode */
ptk_err modbus_tcp_header_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const modbus_tcp_header_t *instance);
ptk_err modbus_tcp_header_decode(ptk_allocator_t *alloc, modbus_tcp_header_t **instance, ptk_buf_t *buf);

/* read_coils_request message definition */
typedef struct read_coils_request_t {
    const int message_type;
    uint8_t function_code;
    uint16_t starting_address;
    uint16_t quantity_of_coils;
} read_coils_request_t;

/* Constructor/Destructor */
ptk_err read_coils_request_create(ptk_allocator_t *alloc, read_coils_request_t **instance);
void read_coils_request_dispose(ptk_allocator_t *alloc, read_coils_request_t *instance);

/* Encode/Decode */
ptk_err read_coils_request_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_coils_request_t *instance);
ptk_err read_coils_request_decode(ptk_allocator_t *alloc, read_coils_request_t **instance, ptk_buf_t *buf);

/* read_coils_response message definition */
typedef struct read_coils_response_t {
    const int message_type;
    uint8_t function_code;
    uint8_t byte_count;
    uint8_t_array_t *coil_status;
} read_coils_response_t;

/* Constructor/Destructor */
ptk_err read_coils_response_create(ptk_allocator_t *alloc, read_coils_response_t **instance);
void read_coils_response_dispose(ptk_allocator_t *alloc, read_coils_response_t *instance);

/* Encode/Decode */
ptk_err read_coils_response_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_coils_response_t *instance);
ptk_err read_coils_response_decode(ptk_allocator_t *alloc, read_coils_response_t **instance, ptk_buf_t *buf);

/* Array Accessors */
ptk_err read_coils_response_get_coil_status_element(const read_coils_response_t *msg, size_t index, uint8_t *value);
ptk_err read_coils_response_set_coil_status_element(read_coils_response_t *msg, size_t index, uint8_t value);
size_t read_coils_response_get_coil_status_length(const read_coils_response_t *msg);

/* read_holding_registers_request message definition */
typedef struct read_holding_registers_request_t {
    const int message_type;
    uint8_t function_code;
    uint16_t starting_address;
    uint16_t quantity_of_registers;
} read_holding_registers_request_t;

/* Constructor/Destructor */
ptk_err read_holding_registers_request_create(ptk_allocator_t *alloc, read_holding_registers_request_t **instance);
void read_holding_registers_request_dispose(ptk_allocator_t *alloc, read_holding_registers_request_t *instance);

/* Encode/Decode */
ptk_err read_holding_registers_request_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_holding_registers_request_t *instance);
ptk_err read_holding_registers_request_decode(ptk_allocator_t *alloc, read_holding_registers_request_t **instance, ptk_buf_t *buf);

/* read_holding_registers_response message definition */
typedef struct read_holding_registers_response_t {
    const int message_type;
    uint8_t function_code;
    uint8_t byte_count;
    uint16_t_array_t *register_value;
} read_holding_registers_response_t;

/* Constructor/Destructor */
ptk_err read_holding_registers_response_create(ptk_allocator_t *alloc, read_holding_registers_response_t **instance);
void read_holding_registers_response_dispose(ptk_allocator_t *alloc, read_holding_registers_response_t *instance);

/* Encode/Decode */
ptk_err read_holding_registers_response_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const read_holding_registers_response_t *instance);
ptk_err read_holding_registers_response_decode(ptk_allocator_t *alloc, read_holding_registers_response_t **instance, ptk_buf_t *buf);

/* Array Accessors */
ptk_err read_holding_registers_response_get_register_value_element(const read_holding_registers_response_t *msg, size_t index, uint16_t *value);
ptk_err read_holding_registers_response_set_register_value_element(read_holding_registers_response_t *msg, size_t index, uint16_t value);
size_t read_holding_registers_response_get_register_value_length(const read_holding_registers_response_t *msg);

/* modbus_exception_response message definition */
typedef struct modbus_exception_response_t {
    const int message_type;
    uint8_t function_code;
    uint8_t exception_code;
} modbus_exception_response_t;

/* Constructor/Destructor */
ptk_err modbus_exception_response_create(ptk_allocator_t *alloc, modbus_exception_response_t **instance);
void modbus_exception_response_dispose(ptk_allocator_t *alloc, modbus_exception_response_t *instance);

/* Encode/Decode */
ptk_err modbus_exception_response_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const modbus_exception_response_t *instance);
ptk_err modbus_exception_response_decode(ptk_allocator_t *alloc, modbus_exception_response_t **instance, ptk_buf_t *buf);

/* Complete Modbus TCP Message */
typedef struct modbus_tcp_message_t {
    const int message_type;
    modbus_tcp_header_t header;
    uint8_t_array_t *pdu_data;
} modbus_tcp_message_t;

/* Constructor/Destructor */
ptk_err modbus_tcp_message_create(ptk_allocator_t *alloc, modbus_tcp_message_t **instance);
void modbus_tcp_message_dispose(ptk_allocator_t *alloc, modbus_tcp_message_t *instance);

/* Encode/Decode */
ptk_err modbus_tcp_message_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const modbus_tcp_message_t *instance);
ptk_err modbus_tcp_message_decode(ptk_allocator_t *alloc, modbus_tcp_message_t **instance, ptk_buf_t *buf);

/* Array Accessors */
ptk_err modbus_tcp_message_get_pdu_data_element(const modbus_tcp_message_t *msg, size_t index, uint8_t *value);
ptk_err modbus_tcp_message_set_pdu_data_element(modbus_tcp_message_t *msg, size_t index, uint8_t value);
size_t modbus_tcp_message_get_pdu_data_length(const modbus_tcp_message_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MODBUS_H */