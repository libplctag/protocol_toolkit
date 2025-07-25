#ifndef ETHERNET_IP_EXAMPLE_H
#define ETHERNET_IP_EXAMPLE_H

#include "ptk_pdu_custom.h"
#include <stdio.h>

/**
 * EtherNet/IP Protocol Example using Extended PDU System
 * 
 * This demonstrates how to handle complex, variable-sized protocols
 * like EtherNet/IP with the extended PDU macro system.
 */

/**
 * Custom types for EtherNet/IP
 */

/* CIP Path - variable length routing path */
typedef struct {
    uint8_t path_size;  // Size in 16-bit words
    uint8_t* path_data; // Variable-length path data
    size_t capacity;    // Allocated capacity
} cip_path_t;

/* CIP Data - variable length application data */
typedef struct {
    uint16_t data_length;
    uint8_t* data;
    size_t capacity;
} cip_data_t;

/* CIP Service codes */
typedef enum {
    CIP_SERVICE_GET_ATTRIBUTE_ALL = 0x01,
    CIP_SERVICE_SET_ATTRIBUTE_ALL = 0x02,
    CIP_SERVICE_GET_ATTRIBUTE_SINGLE = 0x0E,
    CIP_SERVICE_SET_ATTRIBUTE_SINGLE = 0x10,
    CIP_SERVICE_MULTIPLE_SERVICE_PACKET = 0x0A
} cip_service_t;

/* CIP Status codes */
typedef enum {
    CIP_STATUS_SUCCESS = 0x00,
    CIP_STATUS_CONNECTION_FAILURE = 0x01,
    CIP_STATUS_RESOURCE_UNAVAILABLE = 0x02,
    CIP_STATUS_INVALID_PARAMETER = 0x09,
    CIP_STATUS_PATH_SEGMENT_ERROR = 0x04
} cip_status_t;

/**
 * EtherNet/IP Encapsulation Header
 */
#define PTK_PDU_FIELDS_enip_header(X) \
    X(PTK_PDU_U16, command) \
    X(PTK_PDU_U16, length) \
    X(PTK_PDU_U32, session_handle) \
    X(PTK_PDU_U32, status) \
    X(PTK_PDU_U64, sender_context) \
    X(PTK_PDU_U32, options)

PTK_DECLARE_PDU(enip_header)

/**
 * CIP Request - demonstrates custom types
 */
#define PTK_PDU_FIELDS_cip_request(X) \
    X(PTK_PDU_U8, service) \
    X(PTK_PDU_CUSTOM, path, cip_path_t) \
    X(PTK_PDU_CUSTOM, data, cip_data_t)

PTK_DECLARE_PDU_CUSTOM(cip_request)

/**
 * CIP Response with conditional error data
 */
#define PTK_PDU_FIELDS_cip_response(X) \
    X(PTK_PDU_U8, service) \
    X(PTK_PDU_U8, reserved) \
    X(PTK_PDU_U8, general_status) \
    X(PTK_PDU_U8, additional_status_size) \
    X(PTK_PDU_CONDITIONAL, general_status, 0x00, PTK_PDU_CUSTOM, response_data, cip_data_t) \
    X(PTK_PDU_CONDITIONAL, general_status, !0x00, PTK_PDU_U16, extended_status)

PTK_DECLARE_PDU_CUSTOM(cip_response)

/**
 * Multiple Service Packet - demonstrates arrays
 */
typedef struct {
    uint16_t service_offset;
} service_offset_t;

#define PTK_PDU_FIELDS_multiple_service_packet(X) \
    X(PTK_PDU_U16, number_of_services) \
    X(PTK_PDU_ARRAY, service_offsets, service_offset_t, number_of_services) \
    /* Variable-length service data follows */

PTK_DECLARE_PDU_CUSTOM(multiple_service_packet)

/**
 * Forward/Open Request - complex EtherNet/IP example
 */
#define PTK_PDU_FIELDS_forward_open_request(X) \
    X(PTK_PDU_U8, priority_tick_time) \
    X(PTK_PDU_U8, timeout_ticks) \
    X(PTK_PDU_U32, originator_to_target_connection_id) \
    X(PTK_PDU_U32, target_to_originator_connection_id) \
    X(PTK_PDU_U16, connection_serial_number) \
    X(PTK_PDU_U16, originator_vendor_id) \
    X(PTK_PDU_U32, originator_serial_number) \
    X(PTK_PDU_U8, connection_timeout_multiplier) \
    X(PTK_PDU_U8, reserved1) \
    X(PTK_PDU_U8, reserved2) \
    X(PTK_PDU_U8, reserved3) \
    X(PTK_PDU_U32, originator_to_target_rpi) \
    X(PTK_PDU_U16, originator_to_target_connection_parameters) \
    X(PTK_PDU_U32, target_to_originator_rpi) \
    X(PTK_PDU_U16, target_to_originator_connection_parameters) \
    X(PTK_PDU_U8, transport_type_trigger) \
    X(PTK_PDU_U8, connection_path_size) \
    X(PTK_PDU_CUSTOM, connection_path, cip_path_t)

PTK_DECLARE_PDU_CUSTOM(forward_open_request)

/**
 * Identity Object Response - demonstrates nested structures
 */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_type;
    uint16_t product_code;
    uint8_t major_revision;
    uint8_t minor_revision;
    uint16_t status;
    uint32_t serial_number;
    uint8_t product_name_length;
    char product_name[32];
} identity_object_t;

#define PTK_PDU_FIELDS_identity_response(X) \
    X(PTK_PDU_CUSTOM, identity, identity_object_t)

PTK_DECLARE_PDU_CUSTOM(identity_response)

/**
 * Custom type implementations
 */

/* CIP Path functions */
ptk_status_t cip_path_serialize(ptk_slice_bytes_t* slice, const cip_path_t* path, ptk_endian_t endian);
ptk_status_t cip_path_deserialize(ptk_slice_bytes_t* slice, cip_path_t* path, ptk_endian_t endian);
size_t cip_path_size(const cip_path_t* path);
void cip_path_init(cip_path_t* path, size_t capacity);
void cip_path_destroy(cip_path_t* path);
void cip_path_print(const cip_path_t* path);

/* CIP Data functions */
ptk_status_t cip_data_serialize(ptk_slice_bytes_t* slice, const cip_data_t* data, ptk_endian_t endian);
ptk_status_t cip_data_deserialize(ptk_slice_bytes_t* slice, cip_data_t* data, ptk_endian_t endian);
size_t cip_data_size(const cip_data_t* data);
void cip_data_init(cip_data_t* data, size_t capacity);
void cip_data_destroy(cip_data_t* data);
void cip_data_print(const cip_data_t* data);

/* Identity Object functions */
ptk_status_t identity_object_serialize(ptk_slice_bytes_t* slice, const identity_object_t* obj, ptk_endian_t endian);
ptk_status_t identity_object_deserialize(ptk_slice_bytes_t* slice, identity_object_t* obj, ptk_endian_t endian);
size_t identity_object_size(const identity_object_t* obj);
void identity_object_init(identity_object_t* obj);
void identity_object_destroy(identity_object_t* obj);
void identity_object_print(const identity_object_t* obj);

/**
 * Helper functions for EtherNet/IP protocols
 */

/* Build a simple class/instance/attribute path */
ptk_status_t cip_path_build_simple(cip_path_t* path, uint16_t class_id, uint16_t instance_id, uint16_t attribute_id);

/* Parse a CIP path and extract routing information */
ptk_status_t cip_path_parse(const cip_path_t* path, uint16_t* class_id, uint16_t* instance_id, uint16_t* attribute_id);

/* Build CIP data from raw bytes */
ptk_status_t cip_data_from_bytes(cip_data_t* data, const uint8_t* bytes, size_t length);

/* EtherNet/IP command constants */
#define ENIP_CMD_NOP                    0x0000
#define ENIP_CMD_LIST_SERVICES          0x0004
#define ENIP_CMD_LIST_IDENTITY          0x0063
#define ENIP_CMD_LIST_INTERFACES        0x0064
#define ENIP_CMD_REGISTER_SESSION       0x0065
#define ENIP_CMD_UNREGISTER_SESSION     0x0066
#define ENIP_CMD_SEND_RR_DATA           0x006F
#define ENIP_CMD_SEND_UNIT_DATA         0x0070

/* EtherNet/IP status constants */
#define ENIP_STATUS_SUCCESS             0x0000
#define ENIP_STATUS_INVALID_COMMAND     0x0001
#define ENIP_STATUS_INSUFFICIENT_MEMORY 0x0002
#define ENIP_STATUS_INCORRECT_DATA      0x0003
#define ENIP_STATUS_INVALID_SESSION     0x0064
#define ENIP_STATUS_INVALID_LENGTH      0x0065
#define ENIP_STATUS_UNSUPPORTED_PROTOCOL 0x0069

/**
 * Utility macros for common EtherNet/IP patterns
 */

/* Create a Get Attribute All request */
#define ENIP_CREATE_GET_ATTR_ALL_REQUEST(class_id, instance_id) \
    do { \
        cip_request_t request; \
        cip_request_init(&request); \
        request.service = CIP_SERVICE_GET_ATTRIBUTE_ALL; \
        cip_path_build_simple(&request.path, class_id, instance_id, 0); \
        cip_data_init(&request.data, 0); \
    } while(0)

/* Create an identity request */
#define ENIP_CREATE_IDENTITY_REQUEST() \
    ENIP_CREATE_GET_ATTR_ALL_REQUEST(0x01, 0x01)

#endif /* ETHERNET_IP_EXAMPLE_H */