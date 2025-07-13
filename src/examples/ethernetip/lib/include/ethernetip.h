
#pragma once


#include <ptk_alloc.h>
#include <ptk_array.h>
#include <ptk_err.h>
#include <ptk_buf.h>
#include <ptk_sock.h>
#include <ptk_utils.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

typedef struct eip_pdu_base_t eip_pdu_base_t;
typedef struct eip_connection_t eip_connection_t;
typedef union cip_segment_u cip_segment_u;

//=============================================================================
// ARRAY TYPE DECLARATIONS
//=============================================================================

PTK_ARRAY_DECLARE(cip_segment, union cip_segment_u);

//=============================================================================
// BASE APU STRUCTURE
//=============================================================================

typedef struct eip_pdu_base_t {
    ptk_serializable_t base;  // Inherits serialization interface
    eip_connection_t *conn;   // Owning connection
    size_t pdu_type;          // Unique type identifier from #defines
} eip_pdu_base_t;

//=============================================================================
// CIP IOI PATH STRUCTURE AND UTILITIES
//=============================================================================

/* Consolidate into one set of segment types. */

/**
 * CIP Segment Types
 */
typedef enum {
    CIP_SEGMENT_TYPE_PORT = 0x00,       // Port segment
    CIP_SEGMENT_TYPE_LOGICAL = 0x20,    // Logical segment
    CIP_SEGMENT_TYPE_LOGICAL_CLASS = 0x21, // Logical segment with 8-bit class ID
    CIP_SEGMENT_TYPE_LOGICAL_INSTANCE = 0x24, // Logical segment with 8-bit instance ID
    CIP_SEGMENT_TYPE_LOGICAL_MEMBER = 0x28,   // Logical segment with 8-bit member ID
    CIP_SEGMENT_TYPE_LOGICAL_CONNECTION = 0x2C, // Logical segment with 8-bit connection point
    CIP_SEGMENT_TYPE_LOGICAL_ELEMENT = 0x30,    // Logical segment with 8-bit element index
    CIP_SEGMENT_TYPE_LOGICAL_SPECIAL = 0x38,    // Logical segment special (e.g. all
    CIP_SEGMENT_TYPE_NETWORK = 0x40,    // Network segment
    CIP_SEGMENT_TYPE_SYMBOLIC = 0x60,   // Symbolic segment
    CIP_SEGMENT_TYPE_DATA = 0x80,        // Data segment
    CIP_SEGMENT_TYPE_SYMBOLIC_EXTENDED = 0x91, // Extended symbolic segment
} cip_segment_type_t;


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
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL_CLASS
    uint32_t class_id;                  // CIP class identifier
} cip_class_segment_t;

/**
 * Instance Segment
 * Identifies a specific instance of a class
 */
typedef struct cip_instance_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL_INSTANCE
    uint32_t instance_id;               // Instance identifier
} cip_instance_segment_t;

/**
 * Member Segment
 * Identifies a member (attribute) of an instance
 */
typedef struct cip_member_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL_MEMBER
    uint32_t member_id;                 // Member/attribute identifier
} cip_member_segment_t;

/**
 * Connection Point Segment
 * Identifies a connection point for connected messaging
 */
typedef struct cip_connection_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL_CONNECTION
    uint32_t connection_point;          // Connection point identifier
} cip_connection_segment_t;

/**
 * Element Segment
 * Identifies an array element
 */
typedef struct cip_element_segment {
    cip_segment_type_t segment_type;    // CIP_SEGMENT_TYPE_LOGICAL_ELEMENT
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

// CIP Path functions
ptk_err cip_ioi_path_pdu_create_from_string(cip_segment_array_t *path, const char *path_string);


//=============================================================================
// DEVICE IDENTITY STRUCTURES (PUBLIC)
//=============================================================================


/*** MOVE TO IMPLEMENTATION */
/**
 * Device State Values - move these into the implementation.
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
    eip_pdu_base_t base;
    uint16_t response_time_range_ms;
    // Internal protocol details hidden from public API
} eip_list_identity_req_t;


// PDU type flags
#define EIP_PDU_TYPE_RESPONSE_FLAG 0x80000000
#define EIP_PDU_IS_RESPONSE(type) ((type) & EIP_PDU_TYPE_RESPONSE_FLAG)

#define EIP_LIST_IDENTITY_RESP_TYPE (EIP_PDU_TYPE_RESPONSE_FLAG | (__LINE__))

/**
 * Parsed Identity Response (application-friendly format)
 * All fields converted to host byte order and meaningful types
 */
typedef struct eip_list_identity_resp_t {
    eip_pdu_base_t base;               // Base PDU structure

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

    // Device status (parsed from status word)
    bool owned;                         // Device is owned by another master
    bool configured;                    // Device is configured
    bool minor_recoverable_fault;
    bool minor_unrecoverable_fault;
    bool major_recoverable_fault;
    bool major_unrecoverable_fault;

    // Device state (parsed from state byte)
    bool self_test_in_progress;        // true if self-test is in progress
    bool standby;                     // true if in standby mode
    bool operational;                 // true if operational
    bool major_fault;                 // true if in major fault state
    bool configuration_mode;          // true if in configuration mode
    bool waiting_for_reset;           // true if waiting for reset

    // Product information
    char product_name[64];              // Null-terminated string

    // Timing information (for discovery)
    int64_t discovery_timestamp_ms;     // When this response was received
} eip_list_identity_resp_t;


//=============================================================================
// APU UNION FOR RECEIVED MESSAGES
//=============================================================================

typedef union eip_pdu_u {
    eip_pdu_base_t *base;  // For type checking and generic access
    struct eip_list_identity_req_t *list_identity_req;
    struct eip_list_identity_resp_t *list_identity_resp;
    // Future: register_session_req, register_session_resp, etc.
} eip_pdu_u;

// Single APU receive function - handles any EIP message type
// Session registration is handled internally by the implementation
eip_pdu_u eip_pdu_recv(eip_connection_t *conn, ptk_duration_ms timeout_ms);

eip_pdu_base_t *eip_pdu_create_from_type(eip_connection_t *conn, size_t pdu_type);

/**
 * @brief Send an EIP PDU to the specified connection.
 * 
 * @param pdu The PDU to send (must be created with eip_pdu_create_from_type()). The implementation will free the PDU.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return if the PDU is a request, then a pointer to the response PDU is returned (must be freed by caller).
 *         If the PDU is a response, then NULL is returned (the caller should not expect a response).
 *         On error, NULL is returned and ptk_get_err() is set to PTK_ERR_TIMEOUT, PTK_ERR_NETWORK_ERROR, or PTK_ERR_INVALID_DATA.
 */
eip_pdu_base_t *eip_pdu_send(eip_pdu_base_t **pdu, ptk_duration_ms timeout_ms);

//=============================================================================
// CONNECTION MANAGEMENT
//=============================================================================

// Connection management functions
eip_connection_t *eip_client_connect(const char *host, int port);
eip_connection_t *eip_client_connect_udp(const char *host, int port);
eip_connection_t *eip_server_listen(const char *host, int port, int backlog);
ptk_err eip_abort(eip_connection_t *conn);
ptk_err eip_signal(eip_connection_t *conn);
ptk_err eip_wait_for_signal(eip_connection_t *conn, ptk_duration_ms timeout_ms);

//=============================================================================
// EIP PDU DEFINITIONS
//=============================================================================

// Function declaration for serialization functions
ptk_err eip_list_identity_req_serialize(ptk_buf *buf, struct eip_list_identity_req_t *req);
ptk_err eip_list_identity_resp_serialize(ptk_buf *buf, struct eip_list_identity_resp_t *resp);
ptk_err eip_list_identity_resp_deserialize(ptk_buf *buf, struct eip_list_identity_resp_t *resp);
ptk_err cip_segment_serialize(ptk_buf *buf, const union cip_segment_u *segment);
ptk_err cip_segment_array_serialize(ptk_buf *buf, const cip_segment_array_t *segments);

//=============================================================================
// CONVENIENCE FUNCTIONS
//=============================================================================

/**
 * @brief Simple device discovery using core PDU functions
 * 
 * This is a convenience wrapper around the core PDU functions for basic discovery.
 * For more control, use the core functions directly as shown in eip_discovery_example.c
 * 
 * @param response_time_range_ms Response time range for devices (max 2000ms)
 * @param callback Function called for each discovered device (can be NULL)
 * @param user_data User data passed to callback
 * @return Number of devices found, or -1 on error
 */
typedef void (*eip_device_found_callback_t)(const eip_list_identity_resp_t *device, void *user_data);
int eip_discover_devices_simple(ptk_duration_ms response_time_range_ms, 
                               eip_device_found_callback_t callback, 
                               void *user_data);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

// String utilities
const char *eip_device_state_to_string(uint8_t state);
const char *eip_vendor_id_to_string(uint16_t vendor_id);
const char *eip_device_type_to_string(uint16_t device_type);

