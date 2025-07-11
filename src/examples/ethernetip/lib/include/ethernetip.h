#ifndef ETHERNETIP_H
#define ETHERNETIP_H

/**
 * @file ethernetip.h
 * @brief EtherNet/IP Protocol Implementation using Protocol Toolkit (PTK)
 *
 * =============================================================================
 * ETHERNETIP PROTOCOL IMPLEMENTATION DESIGN PATTERNS
 * =============================================================================
 *
 * This EtherNet/IP implementation follows the same design patterns established
 * in the Modbus reference implementation, adapted for EtherNet/IP specifics:
 *
 * ## 1. MEMORY MANAGEMENT STRATEGY
 *
 * **Parent-Child Allocation Pattern:**
 * - Connection → Buffers, CIP Path, Session State
 * - APU → CPF Items → Identity Data, CIP Requests
 * - Arrays → Packed data (CPF items, identity strings)
 *
 * ## 2. TYPE-SAFE SERIALIZATION ARCHITECTURE
 *
 * **Little-Endian with Exceptions:**
 * - EIP Headers: Little-endian (PTK_BUF_LITTLE_ENDIAN)
 * - Socket Addresses: Big-endian (network byte order)
 * - CIP Data: Little-endian
 * - String parsing: Host byte order for application use
 *
 * ## 3. CIP PATH ABSTRACTION
 *
 * **Path Construction:**
 * - String format: "1,0" (backplane, slot 0)
 * - Parsed into efficient binary IOI segments
 * - Automatic validation and error reporting
 *
 * ## 4. APPLICATION-FRIENDLY DATA STRUCTURES
 *
 * **Identity Response Parsing:**
 * - Raw wire format → Host byte order
 * - IP addresses as readable strings
 * - Device names as null-terminated strings
 * - Status fields as meaningful booleans
 *
 * ## 5. CONNECTION-ORIENTED DESIGN
 *
 * **Connection Parameters:**
 * - Host/port for transport layer
 * - CIP path for routing within device
 * - Session management for stateful operations
 * - Timeout handling for discovery and messaging
 */

#include <ptk_alloc.h>
#include <ptk_array.h>
#include <ptk_err.h>
#include <ptk_buf.h>
#include <ptk_socket.h>
#include <ptk_utils.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

typedef struct eip_apu_base_t eip_apu_base_t;
typedef struct eip_connection eip_connection;
typedef union cip_segment_u cip_segment_u;

//=============================================================================
// ARRAY TYPE DECLARATIONS
//=============================================================================

PTK_ARRAY_DECLARE(eip_identity, struct eip_identity_response_t);
PTK_ARRAY_DECLARE(cip_segment, cip_segment_u);

//=============================================================================
// BASE APU STRUCTURE
//=============================================================================

typedef struct eip_apu_base_t {
    ptk_serializable_t buf_base;  // Inherits serialization interface
    size_t apu_type;             // Unique type identifier from #defines
} eip_apu_base_t;

//=============================================================================
// CIP IOI PATH STRUCTURE AND UTILITIES
//=============================================================================

/**
 * CIP Segment Types
 */
typedef enum {
    CIP_SEGMENT_TYPE_PORT = 0x00,       // Port segment
    CIP_SEGMENT_TYPE_LOGICAL = 0x20,    // Logical segment
    CIP_SEGMENT_TYPE_NETWORK = 0x40,    // Network segment
    CIP_SEGMENT_TYPE_SYMBOLIC = 0x60,   // Symbolic segment
    CIP_SEGMENT_TYPE_DATA = 0x80        // Data segment
} cip_segment_type_t;

/**
 * Logical Segment Subtypes
 */
typedef enum {
    CIP_LOGICAL_CLASS = 0x00,           // Class ID
    CIP_LOGICAL_INSTANCE = 0x04,        // Instance ID
    CIP_LOGICAL_MEMBER = 0x08,          // Member ID (attribute)
    CIP_LOGICAL_CONNECTION = 0x0C,      // Connection point
    CIP_LOGICAL_ELEMENT = 0x10,         // Array element
    CIP_LOGICAL_SPECIAL = 0x18          // Special
} cip_logical_subtype_t;

/**
 * Port Segment
 * Routes to a specific port (backplane slot, etc.)
 */
typedef struct cip_port_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_PORT
    uint8_t port_number;                // Port number (1-255)
} cip_port_segment_t;

/**
 * Class Segment
 * Identifies a CIP object class
 */
typedef struct cip_class_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL
    uint32_t class_id;                  // CIP class identifier
} cip_class_segment_t;

/**
 * Instance Segment
 * Identifies a specific instance of a class
 */
typedef struct cip_instance_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL
    uint32_t instance_id;               // Instance identifier
} cip_instance_segment_t;

/**
 * Member Segment
 * Identifies a member (attribute) of an instance
 */
typedef struct cip_member_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL
    uint32_t member_id;                 // Member/attribute identifier
} cip_member_segment_t;

/**
 * Connection Point Segment
 * Identifies a connection point for connected messaging
 */
typedef struct cip_connection_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL
    uint32_t connection_point;          // Connection point identifier
} cip_connection_segment_t;

/**
 * Element Segment
 * Identifies an array element
 */
typedef struct cip_element_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL
    uint32_t element_index;             // Array element index
} cip_element_segment_t;

/**
 * Symbolic Segment
 * Variable-length symbolic name (tag name, etc.)
 */
typedef struct cip_symbolic_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_SYMBOLIC
    char *symbol_name;                  // Symbolic name (child allocation)
    size_t symbol_length;               // Length of symbol name
} cip_symbolic_segment_t;

/**
 * Data Segment
 * Variable-length raw data
 */
typedef struct cip_data_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_DATA
    uint8_t *data;                      // Raw data bytes (child allocation)
    size_t data_length;                 // Length of data
} cip_data_segment_t;

/**
 * Generic Segment Union
 * Used for type-safe access to different segment types
 */
typedef union cip_segment_u {
    cip_segment_type_t segment_type;    // Common type field for all segments
    cip_port_segment_t port;
    cip_class_segment_t class;
    cip_instance_segment_t instance;
    cip_member_segment_t member;
    cip_connection_segment_t connection;
    cip_element_segment_t element;
    cip_symbolic_segment_t symbolic;
    cip_data_segment_t data;
} cip_segment_u;

// Declare segment array
PTK_ARRAY_DECLARE(cip_segment, cip_segment_u);

/**
 * CIP IOI Path PDU
 * Represents a complete CIP path with multiple segments
 */
#define CIP_IOI_PATH_TYPE (__LINE__)
typedef struct cip_ioi_path_pdu {
    eip_apu_base_t base;                // Base PDU structure
    cip_segment_array_t *segments;      // Array of segments (child allocation)
} cip_ioi_path_pdu_t;

// CIP Path PDU functions
ptk_err cip_ioi_path_pdu_create_from_string(void *parent, const char *path_string, cip_ioi_path_pdu_t **path);
cip_ioi_path_pdu_t *cip_ioi_path_pdu_create(void *parent);

// Segment addition functions - directly add segments to existing path PDU
ptk_err cip_ioi_path_pdu_add_port(cip_ioi_path_pdu_t *path, uint8_t port_number);
ptk_err cip_ioi_path_pdu_add_class(cip_ioi_path_pdu_t *path, uint32_t class_id);
ptk_err cip_ioi_path_pdu_add_instance(cip_ioi_path_pdu_t *path, uint32_t instance_id);
ptk_err cip_ioi_path_pdu_add_member(cip_ioi_path_pdu_t *path, uint32_t member_id);
ptk_err cip_ioi_path_pdu_add_connection(cip_ioi_path_pdu_t *path, uint32_t connection_point);
ptk_err cip_ioi_path_pdu_add_element(cip_ioi_path_pdu_t *path, uint32_t element_index);
ptk_err cip_ioi_path_pdu_add_symbolic(cip_ioi_path_pdu_t *path, const char *symbol_name);
ptk_err cip_ioi_path_pdu_add_data(cip_ioi_path_pdu_t *path, const uint8_t *data, size_t data_length);

// CIP Path PDU utility functions
size_t cip_ioi_path_pdu_get_segment_count(const cip_ioi_path_pdu_t *path);
cip_segment_u *cip_ioi_path_pdu_get_segment(const cip_ioi_path_pdu_t *path, size_t index);
size_t cip_ioi_path_pdu_get_wire_length(const cip_ioi_path_pdu_t *path);
ptk_err cip_ioi_path_pdu_serialize(ptk_buf *buf, const cip_ioi_path_pdu_t *path);
ptk_err cip_ioi_path_pdu_deserialize(ptk_buf *buf, void *parent, cip_ioi_path_pdu_t **path);
bool cip_ioi_path_pdu_is_valid(const cip_ioi_path_pdu_t *path);

// String parsing and formatting
ptk_err cip_ioi_path_pdu_parse_string(cip_ioi_path_pdu_t *path, const char *path_string);
ptk_err cip_ioi_path_pdu_to_string(const cip_ioi_path_pdu_t *path, char *buffer, size_t buffer_size);

//=============================================================================
// DEVICE IDENTITY STRUCTURES (PUBLIC)
//=============================================================================

/**
 * Parsed Identity Response (application-friendly format)
 * All fields converted to host byte order and meaningful types
 */
typedef struct eip_identity_response_t {
    eip_apu_base_t base;               // Base APU structure

    // Network information
    char ip_address[16];                // "192.168.1.100" format
    uint16_t port;                      // Host byte order

    // Device identification
    uint16_t vendor_id;                 // Host byte order
    uint16_t device_type;               // Host byte order
    uint16_t product_code;              // Host byte order
    uint8_t revision_major;
    uint8_t revision_minor;
    uint32_t serial_number;             // Host byte order

    // Device status (parsed bitfield)
    bool owned;                         // Device is owned by another master
    bool configured;                    // Device is configured
    bool minor_recoverable_fault;
    bool minor_unrecoverable_fault;
    bool major_recoverable_fault;
    bool major_unrecoverable_fault;

    // Device state
    uint8_t state;                      // Device operational state

    // Product information
    char product_name[64];              // Null-terminated string

    // Timing information (for discovery)
    int64_t discovery_timestamp_ms;     // When this response was received
} eip_identity_response_t;

/**
 * Device State Values
 */
#define EIP_DEVICE_STATE_NONEXISTENT        0x00
#define EIP_DEVICE_STATE_SELF_TESTING       0x01
#define EIP_DEVICE_STATE_STANDBY            0x02
#define EIP_DEVICE_STATE_OPERATIONAL        0x03
#define EIP_DEVICE_STATE_MAJOR_FAULT        0x04
#define EIP_DEVICE_STATE_CONFIGURATION      0x05
#define EIP_DEVICE_STATE_WAITING_FOR_RESET  0x06

//=============================================================================
// LIST IDENTITY REQUEST AND RESPONSE (PUBLIC LEAF PDUS)
//=============================================================================

#define EIP_LIST_IDENTITY_REQ_TYPE (__LINE__)
typedef struct eip_list_identity_req_t {
    eip_apu_base_t base;
    uint16_t response_time_range_ms;
    // Internal protocol details hidden from public API
} eip_list_identity_req_t;

// List Identity Request function declarations
ptk_err eip_list_identity_req_create(void *parent, eip_list_identity_req_t **req);
ptk_err eip_list_identity_req_send(eip_connection *conn, eip_list_identity_req_t *req, ptk_duration_ms timeout_ms);

#define EIP_LIST_IDENTITY_RESP_TYPE (__LINE__)
typedef struct eip_list_identity_resp_t {
    eip_apu_base_t base;
    eip_identity_response_t identity;   // Parsed identity data (public)
    // Internal protocol details hidden from public API
} eip_list_identity_resp_t;

// List Identity Response function declarations
ptk_err eip_list_identity_resp_create(void *parent, eip_list_identity_resp_t **resp);
ptk_err eip_list_identity_resp_send(eip_connection *conn, eip_list_identity_resp_t *resp, ptk_duration_ms timeout_ms);

//=============================================================================
// APU UNION FOR RECEIVED MESSAGES
//=============================================================================

typedef union eip_apu_u {
    eip_apu_base_t *base;  // For type checking and generic access
    struct eip_list_identity_req_t *list_identity_req;
    struct eip_list_identity_resp_t *list_identity_resp;
    // Future: register_session_req, register_session_resp, etc.
} eip_apu_u;

// Single APU receive function - handles any EIP message type
// Session registration is handled internally by the implementation
ptk_err eip_apu_recv(eip_connection *conn, eip_apu_base_t **apu, ptk_duration_ms timeout_ms);

//=============================================================================
// CONNECTION MANAGEMENT
//=============================================================================

typedef struct eip_connection {
    void *parent;                   // Parent for this connection's allocations
    ptk_socket_t *socket;           // Child of connection
    ptk_buf *rx_buffer;             // Child of connection
    ptk_buf *tx_buffer;             // Child of connection

    // Connection parameters
    char host[256];                 // Target host (IP or hostname)
    uint16_t port;                  // Target port (typically 44818)
    cip_segment_array_t *cip_path;  // CIP routing path (child allocation)

    // Session state
    uint32_t session_handle;        // Current session handle (0 if no session)
    uint64_t next_sender_context;   // For request/response matching

    // Transport type
    bool is_udp;                    // true for UDP (discovery), false for TCP
} eip_connection;

// Connection creation functions
eip_connection *eip_client_connect_tcp(void *parent, const char *host, uint16_t port,
                                       const cip_segment_array_t *cip_path);
eip_connection *eip_client_connect_udp(void *parent, const char *host, uint16_t port);
eip_connection *eip_server_listen(void *parent, const char *host, uint16_t port);
ptk_err eip_close(eip_connection *conn);

// Session management is handled internally - no public session functions needed

//=============================================================================
// DEVICE DISCOVERY API
//=============================================================================

/**
 * Device Discovery Result
 * Contains results from a broadcast ListIdentity operation
 */
typedef struct eip_discovery_result_t {
    eip_identity_array_t *devices;      // Array of discovered devices (child allocation)
    size_t device_count;                // Number of devices found
    ptk_duration_ms discovery_time_ms;  // Total time taken for discovery
} eip_discovery_result_t;

// Discovery functions
ptk_err eip_discover_devices(void *parent, const char *network_interface,
                            ptk_duration_ms timeout_ms, eip_discovery_result_t **result);
ptk_err eip_discover_devices_subnet(void *parent, const char *subnet,
                                   ptk_duration_ms timeout_ms, eip_discovery_result_t **result);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

// APU type determination
size_t eip_get_apu_type_from_command(uint16_t command);

// String utilities
const char *eip_device_state_to_string(uint8_t state);
const char *eip_vendor_id_to_string(uint16_t vendor_id);
const char *eip_device_type_to_string(uint16_t device_type);

#endif // ETHERNETIP_H
