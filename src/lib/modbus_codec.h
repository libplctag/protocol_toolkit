#pragma once
#include <stdint.h>
#include <stddef.h>

// Forward declarations for Modbus PDU types (generated from modbus.pdl)
typedef struct modbus_pdu modbus_pdu_t;

// Serialize a Modbus PDU to wire format
size_t modbus_serialize_pdu(const modbus_pdu_t *pdu, uint8_t *out_buf, size_t buf_size);

// Parse a Modbus PDU from wire format
// Returns bytes consumed, or 0 on error
size_t modbus_parse_pdu(modbus_pdu_t *out_pdu, const uint8_t *in_buf, size_t buf_size);

// Utility: get function code from PDU
uint8_t modbus_pdu_get_function_code(const modbus_pdu_t *pdu);
