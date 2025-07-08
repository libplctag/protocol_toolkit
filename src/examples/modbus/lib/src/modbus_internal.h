#pragma once

#include "modbus.h"
#include <arpa/inet.h>
#include <ptk_buf.h>
#include <ptk_log.h>
#include <ptk_socket.h>
#include <string.h>

//=============================================================================
// MODBUS TCP PROTOCOL CONSTANTS
//=============================================================================

#define MODBUS_TCP_PORT 502
#define MODBUS_HEADER_SIZE 7  // 6 bytes TCP header + 1 byte unit ID
#define MODBUS_MAX_PDU_SIZE 253
#define MODBUS_MAX_ADU_SIZE (MODBUS_HEADER_SIZE + MODBUS_MAX_PDU_SIZE)

// Function codes
#define MODBUS_FC_READ_COILS 0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS 0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS 0x03
#define MODBUS_FC_READ_INPUT_REGISTERS 0x04
#define MODBUS_FC_WRITE_SINGLE_COIL 0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER 0x06
#define MODBUS_FC_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10

// Exception codes
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE 0x03

//=============================================================================
// CONNECTION STRUCTURE
//=============================================================================

struct modbus_connection {
    ptk_allocator_t *allocator;
    ptk_sock *socket;
    ptk_address_t address;
    uint8_t unit_id;
    uint16_t transaction_id;
    bool is_server;
    bool is_connected;
    ptk_buf *buffer;  // Shared buffer for send/receive operations
};

//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Increment transaction ID for the connection
 */
uint16_t modbus_next_transaction_id(modbus_connection *conn);

/**
 * @brief Send Modbus TCP frame with header and PDU
 */
ptk_err modbus_send_frame(modbus_connection *conn, ptk_buf *pdu_buf);

/**
 * @brief Receive Modbus TCP frame and extract PDU
 */
ptk_err modbus_recv_frame(modbus_connection *conn, ptk_buf *pdu_buf);
