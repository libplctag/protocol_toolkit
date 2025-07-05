
/**
 * @file ethernetip_protocol.h
 * @brief C structs for EtherNet/IP encapsulation and List Identity command.
 *
 * These structs are designed to be mapped directly onto network packet buffers.
 * They use fixed-width integer types and are packed to prevent compiler-inserted
 * padding, ensuring a correct memory layout.
 */

#include <stdint.h>

#include "buf.h"
#include "log.h"


// --- EtherNet/IP Encapsulation Commands (Stored as Little-Endian) ---
#define EIP_LIST_IDENTITY       0x0063 // Command for List Identity (UDP)
#define EIP_REGISTER_SESSION    0x0065 // Command for Register Session (TCP)
#define EIP_UNREGISTER_SESSION  0x0066 // Command for Unregister Session (TCP)
#define EIP_UNCONNECTED_SEND    0x006F // Command for Unconnected Send (UCMM)
#define EIP_CONNECTED_SEND      0x0070 // Command for Connected Send


/**
 * @brief EtherNet/IP Encapsulation Header (24 bytes).
 * All multi-byte fields are little-endian.
 */
typedef struct eip_encap_header {
    buf_u16_le command;
    buf_u16_le length;
    buf_u32_le session_handle;
    buf_u32_le status;
    buf_u64_le sender_context;
    buf_u32_le options;
} eip_encap_header;




/**
 * @brief List Identity Request (Client -> Broadcast).
 * This is simply the encapsulation header with command 0x0063 and zero data length.
 */
typedef struct eip_encap_header eip_list_identity_request;

buf_err_t eip_list_identity_request_decode(eip_list_identity_request **header, buf *src);
buf_err_t eip_list_identity_request_encode(buf *dest, eip_list_identity_request *header);
void eip_list_identity_request_dispose(eip_list_identity_request *header);
void eip_list_identity_request_log_impl(const char *function, int line, ev_log_level level, const eip_list_identity_request *header);

#define eip_list_identity_request_log_error(value) eip_list_identity_request_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_list_identity_request_log_warn(value) eip_list_identity_request_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_list_identity_request_log_info(value) eip_list_identity_request_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_list_identity_request_log_debug(value) eip_list_identity_request_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_list_identity_request_log_trace(value) eip_list_identity_request_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)


// --- List Identity Response Structures ---
// A response contains a list of "Common Packet Format" (CPF) items.

// --- Common Packet Format (CPF) Type IDs (Stored as Little-Endian) ---
#define CPF_TYPE_ID_CIP_IDENTITY    0x000C // CIP Identity Item
#define CPF_TYPE_ID_SOCKET_ADDR     0x8002 // Socket Address Info Item
#define CPF_TYPE_ID_NULL_ADDRESS    0x0000 // NULL Address Item
#define CPF_TYPE_ID_CONNECTED_ADDR  0x00A1 // Connected Address Item
#define CPF_TYPE_ID_CONNECTED_DATA  0x00B1 // Connected Data Item
#define CPF_TYPE_ID_UNCONNECTED_DATA 0x00B2 // Unconnected Data Item


/**
 * @brief Common Packet Format (CPF) Item Header.
 * This precedes each data item in a list.
 */
typedef struct eip_cpf_item_header {
    buf_u16_le type_id;
    buf_u16_le length;
} eip_cpf_item_header;



/* CPF Item Payload Types */

/**
 * @brief NULL Address Item (Payload for CPF Item with Type ID 0x0000).
 * This item has a length of 0.
 */
typedef struct eip_cpf_item_header eip_cpf_null_address_item;

buf_err_t eip_cpf_null_address_item_decode(eip_cpf_null_address_item **header, buf *src);
buf_err_t eip_cpf_null_address_item_encode(buf *dest, eip_cpf_null_address_item *header);
void eip_cpf_null_address_item_dispose(eip_cpf_null_address_item *header);
void eip_cpf_null_address_item_log_impl(const char *function, int line, ev_log_level level, const eip_cpf_null_address_item *header);

#define eip_cpf_null_address_item_log_error(value) eip_cpf_null_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_cpf_null_address_item_log_warn(value) eip_cpf_null_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_cpf_null_address_item_log_info(value) eip_cpf_null_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_cpf_null_address_item_log_debug(value) eip_cpf_null_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_cpf_null_address_item_log_trace(value) eip_cpf_null_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)



/**
 * @brief Connected Address Item (Payload for CPF Item with Type ID 0x00A1).
 * This item contains the connection ID for a transport class 1 connection.
 */
typedef struct eip_cpf_connected_address_item {
    struct eip_cpf_item_header header;
    buf_u32_le connection_id;
} eip_cpf_connected_address_item;

buf_err_t eip_cpf_connected_address_item_decode(eip_cpf_connected_address_item **header, buf *src);
buf_err_t eip_cpf_connected_address_item_encode(buf *dest, eip_cpf_connected_address_item *header);
void eip_cpf_connected_address_item_dispose(eip_cpf_connected_address_item *header);
void eip_cpf_connected_address_item_log_impl(const char *function, int line, ev_log_level level, const eip_cpf_connected_address_item *header);

#define eip_cpf_connected_address_item_log_error(value) eip_cpf_connected_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_cpf_connected_address_item_log_warn(value) eip_cpf_connected_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_cpf_connected_address_item_log_info(value) eip_cpf_connected_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_cpf_connected_address_item_log_debug(value) eip_cpf_connected_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_cpf_connected_address_item_log_trace(value) eip_cpf_connected_address_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)

/**
 * @brief Connected Data Item (Payload for CPF Item with Type ID 0x00B1).
 * This item contains sequenced data for a transport class 1 connection.
 */
typedef struct eip_cpf_connected_data_item {
    struct eip_cpf_item_header header;
    buf_u32_le sequence_number;
    buf *payload;
} eip_cpf_connected_data_item;

buf_err_t eip_cpf_connected_data_item_decode(eip_cpf_connected_data_item **header, buf *src);
buf_err_t eip_cpf_connected_data_item_encode(buf *dest, eip_cpf_connected_data_item *header);
void eip_cpf_connected_data_item_dispose(eip_cpf_connected_data_item *header);
void eip_cpf_connected_data_item_log_impl(const char *function, int line, ev_log_level level, const eip_cpf_connected_data_item *header);

#define eip_cpf_connected_data_item_log_error(value) eip_cpf_connected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_cpf_connected_data_item_log_warn(value) eip_cpf_connected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_cpf_connected_data_item_log_info(value) eip_cpf_connected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_cpf_connected_data_item_log_debug(value) eip_cpf_connected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_cpf_connected_data_item_log_trace(value) eip_cpf_connected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)



/**
 * @brief Unconnected Data Item (Payload for CPF Item with Type ID 0x00B2).
 * This item encapsulates a UCMM message.
 */
typedef struct eip_cpf_unconnected_data_item {
    struct eip_cpf_item_header header;
    buf *payload;
} eip_cpf_unconnected_data_item;

buf_err_t eip_cpf_unconnected_data_item_decode(eip_cpf_unconnected_data_item **header, buf *src);
buf_err_t eip_cpf_unconnected_data_item_encode(buf *dest, eip_cpf_unconnected_data_item *header);
void eip_cpf_unconnected_data_item_dispose(eip_cpf_unconnected_data_item *header);
void eip_cpf_unconnected_data_item_log_impl(const char *function, int line, ev_log_level level, const eip_cpf_unconnected_data_item *header);

#define eip_cpf_unconnected_data_item_log_error(value) eip_cpf_unconnected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_cpf_unconnected_data_item_log_warn(value) eip_cpf_unconnected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_cpf_unconnected_data_item_log_info(value) eip_cpf_unconnected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_cpf_unconnected_data_item_log_debug(value) eip_cpf_unconnected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_cpf_unconnected_data_item_log_trace(value) eip_cpf_unconnected_data_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)



/**
 * @brief CIP Identity Object (Payload for CPF Item with Type ID 0x000C).
 */
typedef struct eip_cpf_cip_identity_item {
    struct eip_cpf_item_header header;
    buf_u16_le vendor_id;
    buf_u16_le device_type;
    buf_u16_le product_code;
    buf_u8  major_revision;
    buf_u8  minor_revision;
    buf_u16_le status;
    buf_u32_le serial_number;
    buf_u8  product_name_length;
    buf_u8 *product_name;
} eip_cpf_cip_identity_item;

buf_err_t eip_cpf_cip_identity_item_decode(eip_cpf_cip_identity_item **header, buf *src);
buf_err_t eip_cpf_cip_identity_item_encode(buf *dest, eip_cpf_cip_identity_item *header);
void eip_cpf_cip_identity_item_dispose(eip_cpf_cip_identity_item *header);
void eip_cpf_cip_identity_item_log_impl(const char *function, int line, ev_log_level level, const eip_cpf_cip_identity_item *header);

#define eip_cpf_cip_identity_item_log_error(value) eip_cpf_cip_identity_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_cpf_cip_identity_item_log_warn(value) eip_cpf_cip_identity_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_cpf_cip_identity_item_log_info(value) eip_cpf_cip_identity_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_cpf_cip_identity_item_log_debug(value) eip_cpf_cip_identity_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_cpf_cip_identity_item_log_trace(value) eip_cpf_cip_identity_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)



/**
 * @brief Socket Address Info (Payload for CPF Item with Type ID 0x8002).
 * NOTE: The fields in this struct are in network byte order (big-endian),
 * which is an exception to the little-endian rule of most of the protocol.
 */
typedef struct eip_cpf_socket_addr_item {
    struct eip_cpf_item_header header;
    buf_u16_be sin_family; // Big-Endian (e.g., AF_INET = 2)
    buf_u16_be sin_port;   // Big-Endian
    buf_u32_be sin_addr;   // Big-Endian
    buf_u64_le sin_zero;   // Padding, should be zero
} eip_cpf_socket_addr_item;

buf_err_t eip_cpf_socket_addr_item_decode(eip_cpf_socket_addr_item **header, buf *src);
buf_err_t eip_cpf_socket_addr_item_encode(buf *dest, eip_cpf_socket_addr_item *header);
void eip_cpf_socket_addr_item_dispose(eip_cpf_socket_addr_item *header);
void eip_cpf_socket_addr_item_log_impl(const char *function, int line, ev_log_level level, const eip_cpf_socket_addr_item *header);

#define eip_cpf_socket_addr_item_log_error(value) eip_cpf_socket_addr_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_cpf_socket_addr_item_log_warn(value) eip_cpf_socket_addr_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_cpf_socket_addr_item_log_info(value) eip_cpf_socket_addr_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_cpf_socket_addr_item_log_debug(value) eip_cpf_socket_addr_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_cpf_socket_addr_item_log_trace(value) eip_cpf_socket_addr_item_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)



/**
 * @brief Start of a List Identity Response (Device -> Client).
 */
typedef struct eip_list_identity_response {
    eip_encap_header header;
    buf_u16_le item_count;
    struct eip_cpf_item_header **items; // array, length item_count, of pointers to specific CPF items.
} eip_list_identity_response;

buf_err_t eip_list_identity_response_decode(eip_list_identity_response **header, buf *src);
buf_err_t eip_list_identity_response_encode(buf *dest, eip_list_identity_response *header);
void eip_list_identity_response_dispose(eip_list_identity_response *header);
void eip_list_identity_response_log_impl(const char *function, int line, ev_log_level level, const eip_list_identity_response *header);

#define eip_list_identity_response_log_error(value) eip_list_identity_response_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value)
#define eip_list_identity_response_log_warn(value) eip_list_identity_response_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value)
#define eip_list_identity_response_log_info(value) eip_list_identity_response_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value)
#define eip_list_identity_response_log_debug(value) eip_list_identity_response_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value)
#define eip_list_identity_response_log_trace(value) eip_list_identity_response_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value)
