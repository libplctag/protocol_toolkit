
#pragma once


#include <ptk_mem.h>
#include <ptk_array.h>
#include <ptk_err.h>
#include <ptk_buf.h>
#include <ptk_sock.h>
#include <ptk_utils.h>
#include <string.h>  // for memset

//=============================================================================
// MODBUS FORWARD DECLARATIONS
//=============================================================================

struct modbus_connection_t;
typedef struct modbus_connection_t modbus_connection_t;

//=============================================================================
// CONNECTION MANAGEMENT
//=============================================================================


// Connection management functions
modbus_connection_t *modbus_client_connect(const char *host, int port);
modbus_connection_t *modbus_server_listen(const char *host, int port, uint8_t unit_id, int backlog);
ptk_err_t modbus_abort(modbus_connection_t *conn);
ptk_err_t modbus_signal(modbus_connection_t *conn);
ptk_err_t modbus_wait_for_signal(modbus_connection_t *conn, ptk_duration_ms timeout_ms);

//=============================================================================
// ARRAY TYPE DECLARATIONS
//=============================================================================

PTK_ARRAY_DECLARE(modbus_register, uint16_t);

//=============================================================================
// BIT ARRAY FOR COILS AND DISCRETE INPUTS
//=============================================================================

typedef struct modbus_bit_array modbus_bit_array_t;

// Bit array interface functions (similar to PTK_ARRAY_DECLARE pattern)
modbus_bit_array_t *modbus_bit_array_create(size_t num_bits);
ptk_err_t modbus_bit_array_get(const modbus_bit_array_t *arr, size_t index, bool *value);
ptk_err_t modbus_bit_array_set(modbus_bit_array_t *arr, size_t index, bool value);
size_t modbus_bit_array_len(const modbus_bit_array_t *arr);
ptk_err_t modbus_bit_array_resize(modbus_bit_array_t *arr, size_t new_len);
modbus_bit_array_t *modbus_bit_array_copy(const modbus_bit_array_t *src);
bool modbus_bit_array_is_valid(const modbus_bit_array_t *arr);

// Modbus-specific functions for wire format conversion
ptk_err_t modbus_bit_array_from_bytes(const uint8_t *bytes, size_t num_bits, modbus_bit_array_t **arr);
ptk_err_t modbus_bit_array_to_bytes(const modbus_bit_array_t *arr, uint8_t **bytes, size_t *byte_count);

//=============================================================================
// BASE PDU STRUCTURE
//=============================================================================

typedef struct modbus_pdu_base_t {
    ptk_serializable_t buf_base;  // Inherits serialization interface
    modbus_connection_t *conn;      // Owning connection
    size_t pdu_type;              // Unique type identifier from #defines
} modbus_pdu_base_t;


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
// PDU HANDLING
//=============================================================================


/**
 * @brief Receive a Modbus PDU from the specified connection.
 * 
 * If the base is not NULL, then use a switch to get the type of the PDU.
 * 
 * @param conn The modbus connection to receive from. Must be connected.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return a valid modbus_pdu_u union on success, NULL on error (check ptk_get_err()).
 *         On error, ptk_get_err() will be set to PTK_ERR_TIMEOUT, PTK_ERR_NETWORK_ERROR, or PTK_ERR_INVALID_DATA.
 *         If the connection is not valid, returns NULL and sets ptk_get_err() to PTK_ERR_INVALID_ARGUMENT.
 */
modbus_pdu_u modbus_pdu_recv(modbus_connection_t *conn, ptk_duration_ms timeout_ms);

/**
 * @brief Create a new Modbus PDU from the specified type.
 * 
 * @param conn The modbus connection to associate with the PDU.
 * @param pdu_type The type of the PDU to create.
 * @return A pointer to the newly created PDU, or NULL on error (check ptk_get_err()).
 */
modbus_pdu_base_t *modbus_pdu_create_from_type(modbus_connection_t *conn, size_t pdu_type);


/**
 * @brief Send a Modbus PDU to the specified connection.
 * 
 * @param pdu The PDU to send (must be created with modbus_pdu_create_from_type()). The implementation will free the PDU.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return if the PDU is a request, then a pointer to the response PDU is returned (must be freed by caller).
 *         If the PDU is a response, then NULL is returned (the caller should not expect a response).
 *         On error, NULL is returned and ptk_get_err() is set to PTK_ERR_TIMEOUT, PTK_ERR_NETWORK_ERROR, or PTK_ERR_INVALID_DATA.
 */
modbus_pdu_base_t *
modbus_pdu_send(modbus_pdu_base_t **pdu, ptk_duration_ms timeout_ms);

//=============================================================================
// MODBUS PDU DEFINITIONS
//=============================================================================

// PDU type flags
#define MODBUS_PDU_TYPE_RESPONSE_FLAG 0x80000000
#define MODBUS_PDU_IS_RESPONSE(type) ((type) & MODBUS_PDU_TYPE_RESPONSE_FLAG)

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


#define MODBUS_READ_COILS_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_read_coils_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_COILS
    /* uint8_t byte_count; no need for byte_count, we can calculate it from coil_status */
    modbus_bit_array_t *coil_status;  // bit array
} modbus_read_coils_resp_t;


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

#define MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_read_discrete_inputs_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_DISCRETE_INPUTS
    /* uint8_t byte_count;  no need for byte_count, we can calculate it from input_status */
    modbus_bit_array_t *input_status;  // bit array
} modbus_read_discrete_inputs_resp_t;


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


#define MODBUS_READ_HOLDING_REGISTERS_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_read_holding_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_HOLDING_REGISTERS
    /* uint8_t byte_count; no need for byte_count, we can calculate it from register_values */
    modbus_register_array_t *register_values;  // Variable length
} modbus_read_holding_registers_resp_t;


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

#define MODBUS_READ_INPUT_REGISTERS_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_read_input_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_READ_INPUT_REGISTERS
    /* uint8_t byte_count; */ // no need for byte_count, we can calculate it from register_values
    modbus_register_array_t *register_values;  // Variable length
} modbus_read_input_registers_resp_t;


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


#define MODBUS_WRITE_SINGLE_COIL_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_write_single_coil_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_SINGLE_COIL
    uint16_t output_address;   // Echo of request
    uint16_t output_value;     // Echo of request
} modbus_write_single_coil_resp_t;


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


#define MODBUS_WRITE_SINGLE_REGISTER_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_write_single_register_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_SINGLE_REGISTER
    uint16_t register_address; // Echo of request
    uint16_t register_value;   // Echo of request
} modbus_write_single_register_resp_t;


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


#define MODBUS_WRITE_MULTIPLE_COILS_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_write_multiple_coils_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_MULTIPLE_COILS
    uint16_t starting_address; // Echo of request
    uint16_t quantity_of_outputs; // Echo of request
} modbus_write_multiple_coils_resp_t;


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


#define MODBUS_WRITE_MULTIPLE_REGISTERS_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_write_multiple_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FC_WRITE_MULTIPLE_REGISTERS
    uint16_t starting_address; // Echo of request
    uint16_t quantity_of_registers; // Echo of request
} modbus_write_multiple_registers_resp_t;


//=============================================================================
// EXCEPTION RESPONSE
//=============================================================================

#define MODBUS_EXCEPTION_RESP_TYPE (MODBUS_PDU_TYPE_RESPONSE_FLAG | (__LINE__))
typedef struct modbus_exception_resp_t {
    modbus_pdu_base_t base;
    uint8_t exception_function_code;  // Original function code + 0x80
    uint8_t exception_code;           // Error code (1-4, etc.)
} modbus_exception_resp_t;


// Exception codes
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE 0x03
#define MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE 0x04

