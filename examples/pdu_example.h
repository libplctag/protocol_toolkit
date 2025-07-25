#ifndef PDU_EXAMPLE_H
#define PDU_EXAMPLE_H

#include "ptk_pdu_macros.h"
#include <stdio.h>

/**
 * Example PDU Declarations using X-Macro System
 * 
 * This shows how to declare various PDU types and automatically
 * generate all serialization/deserialization functions.
 */

/**
 * Example 1: Simple TCP Header PDU
 */
#define PTK_PDU_FIELDS_tcp_header(X) \
    X(PTK_PDU_U16, src_port) \
    X(PTK_PDU_U16, dst_port) \
    X(PTK_PDU_U32, seq_num) \
    X(PTK_PDU_U32, ack_num) \
    X(PTK_PDU_U16, flags) \
    X(PTK_PDU_U16, window_size)

// Generate struct and function declarations
PTK_DECLARE_PDU(tcp_header)

/**
 * Example 2: Ethernet Frame Header
 */
#define PTK_PDU_FIELDS_eth_header(X) \
    X(PTK_PDU_U64, dst_mac_high)   /* First 6 bytes as part of u64 */ \
    X(PTK_PDU_U64, src_mac_high)   /* First 6 bytes as part of u64 */ \
    X(PTK_PDU_U16, ethertype)

PTK_DECLARE_PDU(eth_header)

/**
 * Example 3: Custom Protocol Message
 */
#define PTK_PDU_FIELDS_my_message(X) \
    X(PTK_PDU_U8,  version) \
    X(PTK_PDU_U8,  message_type) \
    X(PTK_PDU_U16, message_id) \
    X(PTK_PDU_U32, timestamp) \
    X(PTK_PDU_U16, payload_length) \
    X(PTK_PDU_F32, temperature) \
    X(PTK_PDU_F64, precision_value)

PTK_DECLARE_PDU(my_message)

/**
 * Example 4: Simple sensor data
 */
#define PTK_PDU_FIELDS_sensor_data(X) \
    X(PTK_PDU_U32, sensor_id) \
    X(PTK_PDU_S16, temperature_celsius) \
    X(PTK_PDU_U16, humidity_percent) \
    X(PTK_PDU_U32, timestamp)

PTK_DECLARE_PDU(sensor_data)

/**
 * Example 5: Complex message with mixed types
 */
#define PTK_PDU_FIELDS_complex_pdu(X) \
    X(PTK_PDU_U8,  magic_byte) \
    X(PTK_PDU_U16, header_checksum) \
    X(PTK_PDU_U32, sequence_number) \
    X(PTK_PDU_S32, signed_offset) \
    X(PTK_PDU_F32, float_value) \
    X(PTK_PDU_U64, large_counter) \
    X(PTK_PDU_S64, signed_large_value) \
    X(PTK_PDU_F64, double_precision)

PTK_DECLARE_PDU(complex_pdu)

#endif /* PDU_EXAMPLE_H */