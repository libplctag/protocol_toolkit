#ifndef MODBUS_H
#define MODBUS_H

/**
 * @file modbus.h
 * @brief Modbus Protocol Implementation using Protocol Toolkit (PTK)
 *
 * =============================================================================
 * PROTOCOL IMPLEMENTATION DESIGN PATTERNS AND PRINCIPLES
 * =============================================================================
 *
 * This Modbus implementation serves as a reference design for protocol
 * implementations using the Protocol Toolkit. The following patterns and
 * principles should be applied to other protocol implementations:
 *
 * ## 1. MEMORY MANAGEMENT STRATEGY
 *
 * **Parent-Child Allocation Pattern:**
 * - All protocol structures use PTK's parent-child allocation system
 * - Parent objects automatically clean up child allocations when freed
 * - No manual memory management or explicit destructors needed
 * - Example: Connection → Buffers, PDU → Arrays, BitArray → PackedBytes
 *
 * **Benefits:**
 * - Eliminates memory leaks through automatic cleanup
 * - Simplifies error handling (just free the parent)
 * - Clear ownership hierarchy
 *
 * ## 2. TYPE-SAFE SERIALIZATION ARCHITECTURE
 *
 * **PTK Serializable Interface:**
 * - Every PDU implements ptk_serializable_t interface
 * - serialize() and deserialize() methods for wire format conversion
 * - Uses ptk_buf_serialize/deserialize macros for type safety
 * - Big-endian encoding for network protocols (Modbus standard)
 *
 * **Serialization Pattern:**
 * ```c
 * static ptk_err pdu_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
 *     my_pdu_t *pdu = (my_pdu_t*)obj;
 *     return ptk_buf_serialize(buf, PTK_BUF_BIG_ENDIAN,
 *         pdu->field1, pdu->field2, pdu->field3);
 * }
 * ```
 *
 * ## 3. EFFICIENT DATA STRUCTURES
 *
 * **Specialized Arrays for Protocol Needs:**
 * - modbus_bit_array_t: Bit-packed storage for coils/discrete inputs
 * - modbus_register_array_t: Standard PTK array for 16-bit registers
 * - Choose data structures that match wire format (no conversion overhead)
 *
 * **Bit Array Design Pattern:**
 * - Stores 8 bits per byte (direct wire format)
 * - Provides bool interface for individual bit access
 * - Automatic byte count calculation: (num_bits + 7) / 8
 * - No separate capacity field needed
 *
 * ## 4. PDU (PROTOCOL DATA UNIT) ORGANIZATION
 *
 * **Base PDU Structure:**
 * - Common base with serializable interface and type identifier
 * - Each PDU type has unique TYPE constant using __LINE__ macro
 * - PDU union for type-safe message handling
 *
 * **Function Code Organization:**
 * - Group related PDUs by function (e.g., modbus_coils.c)
 * - Request/Response pairs in same file
 * - Consistent naming: <protocol>_<function>_<req|resp>_<action>
 *
 * ## 5. VALIDATION AND ERROR HANDLING
 *
 * **Multi-Layer Validation:**
 * - Parameter validation in public API functions
 * - Protocol-specific limits enforcement (e.g., max coils per request)
 * - Address range validation for register/coil access
 * - Wire format validation during deserialization
 *
 * **Error Propagation:**
 * - Use PTK error codes consistently
 * - Validate inputs before serialization
 * - Handle partial failures gracefully
 *
 * ## 6. MODULAR DESIGN PATTERN
 *
 * **File Organization:**
 * - modbus_common.c: Utilities, validation, PDU base
 * - modbus_bit_array.c: Specialized data structure
 * - modbus_<function>.c: Function-specific implementations
 * - modbus_transport.c: Connection management (stubs)
 * - modbus_exception.c: Error response handling
 *
 * **Benefits:**
 * - Clear separation of concerns
 * - Easy to extend with new functions
 * - Testable individual components
 *
 * ## 7. CONNECTION ABSTRACTION
 *
 * **Connection Structure:**
 * - Contains transport details (socket, buffers)
 * - Protocol-specific state (unit_id, transaction_id)
 * - Buffers allocated as children of connection
 *
 * **Transport Independence:**
 * - PDU serialization separate from transport
 * - Can support TCP, RTU, ASCII transports
 * - Stub implementations for future transport layers
 *
 * ## 8. EXTENSIBILITY PATTERNS
 *
 * **Server API Design:**
 * - Handler function pointers for each function code
 * - User data context passed through
 * - Separate handlers from protocol logic
 *
 * **Function Code Dispatch:**
 * - Central function to map function codes to PDU types
 * - Handles exception responses (0x80 bit)
 * - Easy to add new function codes
 *
 * ## 9. WIRE FORMAT FIDELITY
 *
 * **Direct Format Mapping:**
 * - Bit arrays store data exactly as transmitted
 * - Register arrays use protocol's 16-bit format
 * - Byte count fields calculated, not stored
 * - No unnecessary format conversions
 *
 * **Benefits:**
 * - Maximum performance (no conversion overhead)
 * - Exact protocol compliance
 * - Easy debugging (matches wire captures)
 *
 * ## 10. API DESIGN PRINCIPLES
 *
 * **Consistent Function Naming:**
 * - <type>_create(parent, ...): Allocate and initialize
 * - <type>_send(conn, obj, timeout): Serialize and transmit
 * - <protocol>_<function>_<direction>_<action>: Full qualification
 *
 * **Parameter Patterns:**
 * - Parent parameter for memory management
 * - Connection parameter for network operations
 * - Timeout parameter for blocking operations
 * - Size parameters for variable-length structures
 *
 * **Return Value Conventions:**
 * - ptk_err for all operations that can fail
 * - NULL return for failed allocations
 * - Output parameters for complex return values
 *
 * =============================================================================
 * APPLYING THESE PATTERNS TO OTHER PROTOCOLS
 * =============================================================================
 *
 * When implementing a new protocol using PTK:
 *
 * 1. **Start with data structures**: Design arrays and PDUs for your protocol
 * 2. **Implement serialization**: Use ptk_buf_serialize with correct endianness
 * 3. **Organize by function**: Group related PDUs in separate files
 * 4. **Add validation**: Implement protocol-specific limits and checks
 * 5. **Design connections**: Abstract transport details from protocol logic
 * 6. **Plan extensibility**: Design handler interfaces for server implementations
 * 7. **Test systematically**: Validate serialization, memory management, and errors
 *
 * The result should be a clean, efficient, and maintainable protocol implementation
 * that leverages PTK's strengths while following consistent design patterns.
 */

#include <ptk_alloc.h>
#include <ptk_array.h>
#include <ptk_err.h>
#include <ptk_buf.h>
#include <ptk_socket.h>
#include <ptk_utils.h>
#include <string.h>  // for memset

//=============================================================================
// ARRAY TYPE DECLARATIONS
//=============================================================================

PTK_ARRAY_DECLARE(modbus_register, uint16_t);

//=============================================================================
// BIT ARRAY FOR COILS AND DISCRETE INPUTS
//=============================================================================

typedef struct modbus_bit_array {
    uint8_t *bytes;           // Packed bit storage (child allocation)
    size_t len;               // Number of bits (not bytes)
} modbus_bit_array_t;

// Bit array interface functions (similar to PTK_ARRAY_DECLARE pattern)
modbus_bit_array_t *modbus_bit_array_create(void *parent, size_t num_bits);
ptk_err modbus_bit_array_get(const modbus_bit_array_t *arr, size_t index, bool *value);
ptk_err modbus_bit_array_set(modbus_bit_array_t *arr, size_t index, bool value);
size_t modbus_bit_array_len(const modbus_bit_array_t *arr);
ptk_err modbus_bit_array_resize(modbus_bit_array_t *arr, size_t new_len);
modbus_bit_array_t *modbus_bit_array_copy(const modbus_bit_array_t *src, void *parent);
bool modbus_bit_array_is_valid(const modbus_bit_array_t *arr);

// Modbus-specific functions for wire format conversion
ptk_err modbus_bit_array_from_bytes(void *parent, const uint8_t *bytes, size_t num_bits, modbus_bit_array_t **arr);
ptk_err modbus_bit_array_to_bytes(const modbus_bit_array_t *arr, uint8_t **bytes, size_t *byte_count);

//=============================================================================
// BASE PDU STRUCTURE
//=============================================================================

typedef struct modbus_pdu_base_t {
    ptk_serializable_t buf_base;  // Inherits serialization interface
    size_t pdu_type;             // Unique type identifier from #defines
} modbus_pdu_base_t;

// PDU base initialization
void modbus_pdu_base_init(modbus_pdu_base_t *base, size_t pdu_type);

//=============================================================================
// PDU UNION FOR RECEIVED MESSAGES
//=============================================================================

typedef union modbus_pdu_u {
    modbus_pdu_base_t *base;  // For type checking and generic access
    struct modbus_read_coils_req_t *read_coils_req;
    struct modbus_read_coils_resp_t *read_coils_resp;
    struct modbus_read_discrete_inputs_req_t *read_discrete_inputs_req;
    struct modbus_read_discrete_inputs_resp_t *read_discrete_inputs_resp;
    struct modbus_read_holding_registers_req_t *read_holding_registers_req;
    struct modbus_read_holding_registers_resp_t *read_holding_registers_resp;
    struct modbus_read_input_registers_req_t *read_input_registers_req;
    struct modbus_read_input_registers_resp_t *read_input_registers_resp;
    struct modbus_write_single_coil_req_t *write_single_coil_req;
    struct modbus_write_single_coil_resp_t *write_single_coil_resp;
    struct modbus_write_single_register_req_t *write_single_register_req;
    struct modbus_write_single_register_resp_t *write_single_register_resp;
    struct modbus_write_multiple_coils_req_t *write_multiple_coils_req;
    struct modbus_write_multiple_coils_resp_t *write_multiple_coils_resp;
    struct modbus_write_multiple_registers_req_t *write_multiple_registers_req;
    struct modbus_write_multiple_registers_resp_t *write_multiple_registers_resp;
    struct modbus_exception_resp_t *exception_resp;
} modbus_pdu_u;

//=============================================================================
// APU STRUCTURE (Application Protocol Unit)
//=============================================================================

// #define MODBUS_APU_TYPE (__LINE__)
// typedef struct modbus_apu_t {
//     struct modbus_mbap_t mbap;           // MBAP header

//     // Union of all possible PDU payloads (as pointers)
//     union {
//         struct modbus_pdu_base_t *pdu_base;  // Base PDU for generic access
//         struct modbus_read_coils_req_t *read_coils_req;
//         struct modbus_read_coils_resp_t *read_coils_resp;
//         struct modbus_read_discrete_inputs_req_t *read_discrete_inputs_req;
//         struct modbus_read_discrete_inputs_resp_t *read_discrete_inputs_resp;
//         struct modbus_read_holding_registers_req_t *read_holding_registers_req;
//         struct modbus_read_holding_registers_resp_t *read_holding_registers_resp;
//         struct modbus_read_input_registers_req_t *read_input_registers_req;
//         struct modbus_read_input_registers_resp_t *read_input_registers_resp;
//         struct modbus_write_single_coil_req_t *write_single_coil_req;
//         struct modbus_write_single_coil_resp_t *write_single_coil_resp;
//         struct modbus_write_single_register_req_t *write_single_register_req;
//         struct modbus_write_single_register_resp_t *write_single_register_resp;
//         struct modbus_write_multiple_coils_req_t *write_multiple_coils_req;
//         struct modbus_write_multiple_coils_resp_t *write_multiple_coils_resp;
//         struct modbus_write_multiple_registers_req_t *write_multiple_registers_req;
//         struct modbus_write_multiple_registers_resp_t *write_multiple_registers_resp;
//         struct modbus_exception_resp_t *exception_resp;
//     } payload;
// } modbus_apu_t;

// APU function declarations - only deserialize is public
ptk_err modbus_pdu_recv(modbus_connection *conn, modbus_pdu_u **pdu, ptk_duration_ms timeout_ms);


// //=============================================================================
// // MBAP HEADER (TCP/IP TRANSPORT)
// //=============================================================================

// #define MODBUS_MBAP_TYPE (__LINE__)
// typedef struct modbus_mbap_t {
//     modbus_pdu_base_t base;
//     uint16_t transaction_id;
//     uint16_t protocol_id;      // Always 0 for Modbus
//     uint16_t length;           // Length of following bytes
//     uint8_t unit_id;           // Slave address
// } modbus_mbap_t;

// // MBAP functions are internal - no public declarations


//=============================================================================
// FUNCTION CODE 0x01 - READ COILS
//=============================================================================

#define MODBUS_READ_COILS_REQ_TYPE (__LINE__)
#define MODBUS_FC_READ_COILS 0x01
typedef struct modbus_read_coils_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_COILS
    uint16_t starting_address;
    uint16_t quantity_of_coils;
} modbus_read_coils_req_t;

// Read Coils Request function declarations
ptk_err modbus_read_coils_req_send(modbus_connection *conn, modbus_read_coils_req_t *obj, ptk_duration_ms timeout_ms);
modbus_read_coils_req_t* modbus_read_coils_req_create(void *parent);

#define MODBUS_READ_COILS_RESP_TYPE (__LINE__)
typedef struct modbus_read_coils_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_COILS
    /* uint8_t byte_count; no need for byte_count, we can calculate it from coil_status */
    modbus_bit_array_t *coil_status;  // bit array
} modbus_read_coils_resp_t;

// Read Coils Response function declarations
ptk_err modbus_read_coils_resp_send(modbus_connection *conn, modbus_read_coils_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_read_coils_resp_t* modbus_read_coils_resp_create(void *parent, size_t num_coils);

//=============================================================================
// FUNCTION CODE 0x02 - READ DISCRETE INPUTS
//=============================================================================

#define MODBUS_READ_DISCRETE_INPUTS_REQ_TYPE (__LINE__)
#define MODBUS_FC_READ_DISCRETE_INPUTS 0x02
typedef struct modbus_read_discrete_inputs_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_DISCRETE_INPUTS
    uint16_t starting_address;
    uint16_t quantity_of_inputs;
} modbus_read_discrete_inputs_req_t;

// Read Discrete Inputs Request function declarations
ptk_err modbus_read_discrete_inputs_req_send(modbus_connection *conn, modbus_read_discrete_inputs_req_t *obj, ptk_duration_ms timeout_ms);
modbus_read_discrete_inputs_req_t* modbus_read_discrete_inputs_req_create(void *parent);

#define MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE (__LINE__)
typedef struct modbus_read_discrete_inputs_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_DISCRETE_INPUTS
    /* uint8_t byte_count;  no need for byte_count, we can calculate it from input_status */
    modbus_bit_array_t *input_status;  // bit array
} modbus_read_discrete_inputs_resp_t;

// Read Discrete Inputs Response function declarations
ptk_err modbus_read_discrete_inputs_resp_send(modbus_connection *conn, modbus_read_discrete_inputs_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_read_discrete_inputs_resp_t* modbus_read_discrete_inputs_resp_create(void *parent, size_t num_inputs);

//=============================================================================
// FUNCTION CODE 0x03 - READ HOLDING REGISTERS
//=============================================================================

#define MODBUS_READ_HOLDING_REGISTERS_REQ_TYPE (__LINE__)
#define MODBUS_FC_READ_HOLDING_REGISTERS 0x03
typedef struct modbus_read_holding_registers_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_HOLDING_REGISTERS
    uint16_t starting_address;
    uint16_t quantity_of_registers;
} modbus_read_holding_registers_req_t;

// Read Holding Registers Request function declarations
ptk_err modbus_read_holding_registers_req_send(modbus_connection *conn, modbus_read_holding_registers_req_t *obj, ptk_duration_ms timeout_ms);
modbus_read_holding_registers_req_t* modbus_read_holding_registers_req_create(void *parent);

#define MODBUS_READ_HOLDING_REGISTERS_RESP_TYPE (__LINE__)
typedef struct modbus_read_holding_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_HOLDING_REGISTERS
    /* uint8_t byte_count; no need for byte_count, we can calculate it from register_values */
    modbus_register_array_t *register_values;  // Variable length
} modbus_read_holding_registers_resp_t;

// Read Holding Registers Response function declarations
ptk_err modbus_read_holding_registers_resp_send(modbus_connection *conn, modbus_read_holding_registers_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_read_holding_registers_resp_t* modbus_read_holding_registers_resp_create(void *parent, size_t num_registers);

//=============================================================================
// FUNCTION CODE 0x04 - READ INPUT REGISTERS
//=============================================================================

#define MODBUS_READ_INPUT_REGISTERS_REQ_TYPE (__LINE__)
#define MODBUS_FC_READ_INPUT_REGISTERS 0x04
typedef struct modbus_read_input_registers_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_INPUT_REGISTERS
    uint16_t starting_address;
    uint16_t quantity_of_registers;
} modbus_read_input_registers_req_t;

// Read Input Registers Request function declarations
ptk_err modbus_read_input_registers_req_send(modbus_connection *conn, modbus_read_input_registers_req_t *obj, ptk_duration_ms timeout_ms);
modbus_read_input_registers_req_t* modbus_read_input_registers_req_create(void *parent);

#define MODBUS_READ_INPUT_REGISTERS_RESP_TYPE (__LINE__)
typedef struct modbus_read_input_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_INPUT_REGISTERS
    /* uint8_t byte_count; */ // no need for byte_count, we can calculate it from register_values
    modbus_register_array_t *register_values;  // Variable length
} modbus_read_input_registers_resp_t;

// Read Input Registers Response function declarations
ptk_err modbus_read_input_registers_resp_send(modbus_connection *conn, modbus_read_input_registers_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_read_input_registers_resp_t* modbus_read_input_registers_resp_create(void *parent, size_t num_registers);

//=============================================================================
// FUNCTION CODE 0x05 - WRITE SINGLE COIL
//=============================================================================

#define MODBUS_WRITE_SINGLE_COIL_REQ_TYPE (__LINE__)
#define MODBUS_FC_WRITE_SINGLE_COIL 0x05
typedef struct modbus_write_single_coil_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_SINGLE_COIL
    uint16_t output_address;
    uint16_t output_value;     // 0x0000 = OFF, 0xFF00 = ON
} modbus_write_single_coil_req_t;

// Write Single Coil Request function declarations
ptk_err modbus_write_single_coil_req_send(modbus_connection *conn, modbus_write_single_coil_req_t *obj, ptk_duration_ms timeout_ms);
modbus_write_single_coil_req_t* modbus_write_single_coil_req_create(void *parent);

#define MODBUS_WRITE_SINGLE_COIL_RESP_TYPE (__LINE__)
typedef struct modbus_write_single_coil_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_SINGLE_COIL
    uint16_t output_address;   // Echo of request
    uint16_t output_value;     // Echo of request
} modbus_write_single_coil_resp_t;

// Write Single Coil Response function declarations
ptk_err modbus_write_single_coil_resp_send(modbus_connection *conn, modbus_write_single_coil_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_write_single_coil_resp_t* modbus_write_single_coil_resp_create(void *parent);

//=============================================================================
// FUNCTION CODE 0x06 - WRITE SINGLE REGISTER
//=============================================================================

#define MODBUS_WRITE_SINGLE_REGISTER_REQ_TYPE (__LINE__)
#define MODBUS_FC_WRITE_SINGLE_REGISTER 0x06
typedef struct modbus_write_single_register_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_SINGLE_REGISTER
    uint16_t register_address;
    uint16_t register_value;
} modbus_write_single_register_req_t;

// Write Single Register Request function declarations
ptk_err modbus_write_single_register_req_send(modbus_connection *conn, modbus_write_single_register_req_t *obj, ptk_duration_ms timeout_ms);
modbus_write_single_register_req_t* modbus_write_single_register_req_create(void *parent);

#define MODBUS_WRITE_SINGLE_REGISTER_RESP_TYPE (__LINE__)
typedef struct modbus_write_single_register_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_SINGLE_REGISTER
    uint16_t register_address; // Echo of request
    uint16_t register_value;   // Echo of request
} modbus_write_single_register_resp_t;

// Write Single Register Response function declarations
ptk_err modbus_write_single_register_resp_send(modbus_connection *conn, modbus_write_single_register_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_write_single_register_resp_t* modbus_write_single_register_resp_create(void *parent);

//=============================================================================
// FUNCTION CODE 0x0F - WRITE MULTIPLE COILS
//=============================================================================

#define MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE (__LINE__)
#define MODBUS_FC_WRITE_MULTIPLE_COILS 0x0F
typedef struct modbus_write_multiple_coils_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_MULTIPLE_COILS
    uint16_t starting_address;
    /* uint16_t quantity_of_outputs; no need for this, we can calculate it from output_values */
    /* uint8_t byte_count; no need for byte_count, we can calculate it from output_values */
    modbus_bit_array_t *output_values;  // bit array
} modbus_write_multiple_coils_req_t;

// Write Multiple Coils Request function declarations
ptk_err modbus_write_multiple_coils_req_send(modbus_connection *conn, modbus_write_multiple_coils_req_t *obj, ptk_duration_ms timeout_ms);
modbus_write_multiple_coils_req_t* modbus_write_multiple_coils_req_create(void *parent, size_t num_coils);

#define MODBUS_WRITE_MULTIPLE_COILS_RESP_TYPE (__LINE__)
typedef struct modbus_write_multiple_coils_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_MULTIPLE_COILS
    uint16_t starting_address; // Echo of request
    uint16_t quantity_of_outputs; // Echo of request
} modbus_write_multiple_coils_resp_t;

// Write Multiple Coils Response function declarations
ptk_err modbus_write_multiple_coils_resp_send(modbus_connection *conn, modbus_write_multiple_coils_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_write_multiple_coils_resp_t* modbus_write_multiple_coils_resp_create(void *parent);

//=============================================================================
// FUNCTION CODE 0x10 - WRITE MULTIPLE REGISTERS
//=============================================================================

#define MODBUS_WRITE_MULTIPLE_REGISTERS_REQ_TYPE (__LINE__)
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS 0x10
typedef struct modbus_write_multiple_registers_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_MULTIPLE_REGISTERS
    uint16_t starting_address;
    /* uint16_t quantity_of_registers; no need for this, we can calculate it from register_values */
    /* uint8_t byte_count; no need for byte_count, we can calculate it from register_values */
    modbus_register_array_t *register_values;  // Variable length
} modbus_write_multiple_registers_req_t;

// Write Multiple Registers Request function declarations
ptk_err modbus_write_multiple_registers_req_send(modbus_connection *conn, modbus_write_multiple_registers_req_t *obj, ptk_duration_ms timeout_ms);
modbus_write_multiple_registers_req_t* modbus_write_multiple_registers_req_create(void *parent, size_t num_registers);

#define MODBUS_WRITE_MULTIPLE_REGISTERS_RESP_TYPE (__LINE__)
typedef struct modbus_write_multiple_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_MULTIPLE_REGISTERS
    uint16_t starting_address; // Echo of request
    uint16_t quantity_of_registers; // Echo of request
} modbus_write_multiple_registers_resp_t;

// Write Multiple Registers Response function declarations
ptk_err modbus_write_multiple_registers_resp_send(modbus_connection *conn, modbus_write_multiple_registers_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_write_multiple_registers_resp_t* modbus_write_multiple_registers_resp_create(void *parent);

//=============================================================================
// EXCEPTION RESPONSE
//=============================================================================

#define MODBUS_EXCEPTION_RESP_TYPE (__LINE__)
typedef struct modbus_exception_resp_t {
    modbus_pdu_base_t base;
    uint8_t exception_function_code;  // Original function code + 0x80
    uint8_t exception_code;           // Error code (1-4, etc.)
} modbus_exception_resp_t;

// Exception Response function declarations
ptk_err modbus_exception_resp_send(modbus_connection *conn, modbus_exception_resp_t *obj, ptk_duration_ms timeout_ms);
modbus_exception_resp_t* modbus_exception_resp_create(void *parent);

// Exception utility functions
modbus_exception_resp_t* modbus_create_exception_response(void *parent, uint8_t original_function_code, uint8_t exception_code);

// Exception codes
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE 0x03
#define MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE 0x04

//=============================================================================
// CONNECTION MANAGEMENT
//=============================================================================

typedef struct modbus_connection {
    void *parent;               // Parent for this connection's allocations
    ptk_socket_t *socket;       // Child of connection
    ptk_buf *rx_buffer;         // Child of connection
    ptk_buf *tx_buffer;         // Child of connection
    uint8_t unit_id;
    uint16_t next_transaction_id;
} modbus_connection;

// Connection management functions
modbus_connection *modbus_client_connect(void *parent, const char *host, int port, uint8_t unit_id);
modbus_connection *modbus_server_listen(void *parent, const char *host, int port);
ptk_err modbus_close(modbus_connection *conn);

// //=============================================================================
// // LOW-LEVEL CLIENT API
// //=============================================================================

// // Send functions for individual Modbus function codes
// ptk_err modbus_send_read_coils_req(modbus_connection *conn, uint16_t starting_address,
//                                    uint16_t quantity, uint32_t timeout_ms);
// ptk_err modbus_send_read_discrete_inputs_req(modbus_connection *conn, uint16_t starting_address,
//                                              uint16_t quantity, uint32_t timeout_ms);
// ptk_err modbus_send_read_holding_registers_req(modbus_connection *conn, uint16_t starting_address,
//                                                uint16_t quantity, uint32_t timeout_ms);
// ptk_err modbus_send_read_input_registers_req(modbus_connection *conn, uint16_t starting_address,
//                                              uint16_t quantity, uint32_t timeout_ms);
// ptk_err modbus_send_write_single_coil_req(modbus_connection *conn, uint16_t address,
//                                           bool value, uint32_t timeout_ms);
// ptk_err modbus_send_write_single_register_req(modbus_connection *conn, uint16_t address,
//                                               uint16_t value, uint32_t timeout_ms);
// ptk_err modbus_send_write_multiple_coils_req(modbus_connection *conn, uint16_t starting_address,
//                                              const modbus_byte_array_t *coil_values, uint32_t timeout_ms);
// ptk_err modbus_send_write_multiple_registers_req(modbus_connection *conn, uint16_t starting_address,
//                                                  const modbus_register_array_t *register_values, uint32_t timeout_ms);

// // Single receive function for all responses
// ptk_err modbus_apu_recv(modbus_connection *conn, modbus_apu_t **apu, uint32_t timeout_ms);

//=============================================================================
// SERVER API (STUB)
//=============================================================================

// Server callback types
typedef ptk_err (*modbus_read_coils_handler_t)(uint16_t address, uint16_t quantity, modbus_bit_array_t **coil_status, void *user_data);
typedef ptk_err (*modbus_read_discrete_inputs_handler_t)(uint16_t address, uint16_t quantity, modbus_bit_array_t **input_status, void *user_data);
typedef ptk_err (*modbus_read_holding_registers_handler_t)(uint16_t address, uint16_t quantity, modbus_register_array_t **register_values, void *user_data);
typedef ptk_err (*modbus_read_input_registers_handler_t)(uint16_t address, uint16_t quantity, modbus_register_array_t **register_values, void *user_data);
typedef ptk_err (*modbus_write_single_coil_handler_t)(uint16_t address, bool value, void *user_data);
typedef ptk_err (*modbus_write_single_register_handler_t)(uint16_t address, uint16_t value, void *user_data);
typedef ptk_err (*modbus_write_multiple_coils_handler_t)(uint16_t address, const modbus_bit_array_t *coil_values, void *user_data);
typedef ptk_err (*modbus_write_multiple_registers_handler_t)(uint16_t address, const modbus_register_array_t *register_values, void *user_data);

// Server handler registration
typedef struct modbus_server_handlers {
    modbus_read_coils_handler_t read_coils;
    modbus_read_discrete_inputs_handler_t read_discrete_inputs;
    modbus_read_holding_registers_handler_t read_holding_registers;
    modbus_read_input_registers_handler_t read_input_registers;
    modbus_write_single_coil_handler_t write_single_coil;
    modbus_write_single_register_handler_t write_single_register;
    modbus_write_multiple_coils_handler_t write_multiple_coils;
    modbus_write_multiple_registers_handler_t write_multiple_registers;
    void *user_data;
} modbus_server_handlers_t;

// Server functions
ptk_err modbus_server_set_handlers(modbus_connection *conn, const modbus_server_handlers_t *handlers);
ptk_err modbus_server_process_request(modbus_connection *conn);
modbus_connection *modbus_server_accept_connection(modbus_connection *server_conn);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

// PDU type determination
size_t modbus_get_pdu_type_from_function_code(uint8_t function_code, bool is_request);

// Validation functions
ptk_err modbus_validate_address_range(uint16_t address, uint16_t quantity, uint16_t max_address);
ptk_err modbus_validate_quantity(uint16_t quantity, uint16_t max_quantity);

#endif // MODBUS_H
