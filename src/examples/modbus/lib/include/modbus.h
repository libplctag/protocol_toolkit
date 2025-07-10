#ifndef MODBUS_H
#define MODBUS_H

#include <ptk_alloc.h>
#include <ptk_array.h>
#include <ptk_err.h>
#include <ptk_buf.h>
#include <ptk_socket.h>
#include <string.h>  // for memset

//=============================================================================
// ARRAY TYPE DECLARATIONS
//=============================================================================

PTK_ARRAY_DECLARE(modbus_register, uint16_t);
PTK_ARRAY_DECLARE(modbus_coil, bool);
PTK_ARRAY_DECLARE(modbus_byte, uint8_t);

//=============================================================================
// BASE PDU STRUCTURE
//=============================================================================

typedef struct modbus_pdu_base_t {
    ptk_serializable_t buf_base;  // Inherits serialization interface
    size_t pdu_type;             // Unique type identifier from #defines
} modbus_pdu_base_t;

//=============================================================================
// MBAP HEADER (TCP/IP TRANSPORT)
//=============================================================================

#define MODBUS_MBAP_TYPE (__LINE__)
typedef struct modbus_mbap_t {
    modbus_pdu_base_t base;
    uint16_t transaction_id;
    uint16_t protocol_id;      // Always 0 for Modbus
    uint16_t length;           // Length of following bytes
    uint8_t unit_id;           // Slave address

    // Union of all possible PDU payloads
    union {
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
    } pdu;
} modbus_mbap_t;

// MBAP function declarations
ptk_err modbus_mbap_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_mbap_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_mbap_t* modbus_mbap_create(void *parent);
void modbus_mbap_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x01 - READ COILS
//=============================================================================

#define MODBUS_READ_COILS_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_READ_COILS 0x01
typedef struct modbus_read_coils_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_COILS
    uint16_t starting_address;
    uint16_t quantity_of_coils;
} modbus_read_coils_req_t;

// Read Coils Request function declarations
ptk_err modbus_read_coils_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_coils_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_coils_req_t* modbus_read_coils_req_create(void *parent);
void modbus_read_coils_req_destructor(void *ptr);

#define MODBUS_READ_COILS_RESP_TYPE (__LINE__)
typedef struct modbus_read_coils_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_COILS
    uint8_t byte_count;
    modbus_byte_array_t *coil_status;  // Variable length, packed bits
} modbus_read_coils_resp_t;

// Read Coils Response function declarations
ptk_err modbus_read_coils_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_coils_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_coils_resp_t* modbus_read_coils_resp_create(void *parent);
void modbus_read_coils_resp_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x02 - READ DISCRETE INPUTS
//=============================================================================

#define MODBUS_READ_DISCRETE_INPUTS_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_READ_DISCRETE_INPUTS 0x02
typedef struct modbus_read_discrete_inputs_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_DISCRETE_INPUTS
    uint16_t starting_address;
    uint16_t quantity_of_inputs;
} modbus_read_discrete_inputs_req_t;

// Read Discrete Inputs Request function declarations
ptk_err modbus_read_discrete_inputs_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_discrete_inputs_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_discrete_inputs_req_t* modbus_read_discrete_inputs_req_create(void *parent);
void modbus_read_discrete_inputs_req_destructor(void *ptr);

#define MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE (__LINE__)
typedef struct modbus_read_discrete_inputs_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_DISCRETE_INPUTS
    uint8_t byte_count;
    modbus_byte_array_t *input_status;  // Variable length, packed bits
} modbus_read_discrete_inputs_resp_t;

// Read Discrete Inputs Response function declarations
ptk_err modbus_read_discrete_inputs_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_discrete_inputs_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_discrete_inputs_resp_t* modbus_read_discrete_inputs_resp_create(void *parent);
void modbus_read_discrete_inputs_resp_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x03 - READ HOLDING REGISTERS
//=============================================================================

#define MODBUS_READ_HOLDING_REGISTERS_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_READ_HOLDING_REGISTERS 0x03
typedef struct modbus_read_holding_registers_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_HOLDING_REGISTERS
    uint16_t starting_address;
    uint16_t quantity_of_registers;
} modbus_read_holding_registers_req_t;

// Read Holding Registers Request function declarations
ptk_err modbus_read_holding_registers_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_holding_registers_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_holding_registers_req_t* modbus_read_holding_registers_req_create(void *parent);
void modbus_read_holding_registers_req_destructor(void *ptr);

#define MODBUS_READ_HOLDING_REGISTERS_RESP_TYPE (__LINE__)
typedef struct modbus_read_holding_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_HOLDING_REGISTERS
    uint8_t byte_count;
    modbus_register_array_t *register_values;  // Variable length
} modbus_read_holding_registers_resp_t;

// Read Holding Registers Response function declarations
ptk_err modbus_read_holding_registers_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_holding_registers_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_holding_registers_resp_t* modbus_read_holding_registers_resp_create(void *parent);
void modbus_read_holding_registers_resp_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x04 - READ INPUT REGISTERS
//=============================================================================

#define MODBUS_READ_INPUT_REGISTERS_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_READ_INPUT_REGISTERS 0x04
typedef struct modbus_read_input_registers_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_INPUT_REGISTERS
    uint16_t starting_address;
    uint16_t quantity_of_registers;
} modbus_read_input_registers_req_t;

// Read Input Registers Request function declarations
ptk_err modbus_read_input_registers_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_input_registers_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_input_registers_req_t* modbus_read_input_registers_req_create(void *parent);
void modbus_read_input_registers_req_destructor(void *ptr);

#define MODBUS_READ_INPUT_REGISTERS_RESP_TYPE (__LINE__)
typedef struct modbus_read_input_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_READ_INPUT_REGISTERS
    uint8_t byte_count;
    modbus_register_array_t *register_values;  // Variable length
} modbus_read_input_registers_resp_t;

// Read Input Registers Response function declarations
ptk_err modbus_read_input_registers_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_read_input_registers_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_read_input_registers_resp_t* modbus_read_input_registers_resp_create(void *parent);
void modbus_read_input_registers_resp_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x05 - WRITE SINGLE COIL
//=============================================================================

#define MODBUS_WRITE_SINGLE_COIL_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_WRITE_SINGLE_COIL 0x05
typedef struct modbus_write_single_coil_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_SINGLE_COIL
    uint16_t output_address;
    uint16_t output_value;     // 0x0000 = OFF, 0xFF00 = ON
} modbus_write_single_coil_req_t;

// Write Single Coil Request function declarations
ptk_err modbus_write_single_coil_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_single_coil_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_single_coil_req_t* modbus_write_single_coil_req_create(void *parent);
void modbus_write_single_coil_req_destructor(void *ptr);

#define MODBUS_WRITE_SINGLE_COIL_RESP_TYPE (__LINE__)
typedef struct modbus_write_single_coil_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_SINGLE_COIL
    uint16_t output_address;   // Echo of request
    uint16_t output_value;     // Echo of request
} modbus_write_single_coil_resp_t;

// Write Single Coil Response function declarations
ptk_err modbus_write_single_coil_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_single_coil_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_single_coil_resp_t* modbus_write_single_coil_resp_create(void *parent);
void modbus_write_single_coil_resp_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x06 - WRITE SINGLE REGISTER
//=============================================================================

#define MODBUS_WRITE_SINGLE_REGISTER_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_WRITE_SINGLE_REGISTER 0x06
typedef struct modbus_write_single_register_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_SINGLE_REGISTER
    uint16_t register_address;
    uint16_t register_value;
} modbus_write_single_register_req_t;

// Write Single Register Request function declarations
ptk_err modbus_write_single_register_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_single_register_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_single_register_req_t* modbus_write_single_register_req_create(void *parent);
void modbus_write_single_register_req_destructor(void *ptr);

#define MODBUS_WRITE_SINGLE_REGISTER_RESP_TYPE (__LINE__)
typedef struct modbus_write_single_register_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_SINGLE_REGISTER
    uint16_t register_address; // Echo of request
    uint16_t register_value;   // Echo of request
} modbus_write_single_register_resp_t;

// Write Single Register Response function declarations
ptk_err modbus_write_single_register_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_single_register_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_single_register_resp_t* modbus_write_single_register_resp_create(void *parent);
void modbus_write_single_register_resp_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x0F - WRITE MULTIPLE COILS
//=============================================================================

#define MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_WRITE_MULTIPLE_COILS 0x0F
typedef struct modbus_write_multiple_coils_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_MULTIPLE_COILS
    uint16_t starting_address;
    uint16_t quantity_of_outputs;
    uint8_t byte_count;
    modbus_byte_array_t *output_values;  // Variable length, packed bits
} modbus_write_multiple_coils_req_t;

// Write Multiple Coils Request function declarations
ptk_err modbus_write_multiple_coils_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_multiple_coils_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_multiple_coils_req_t* modbus_write_multiple_coils_req_create(void *parent);
void modbus_write_multiple_coils_req_destructor(void *ptr);

#define MODBUS_WRITE_MULTIPLE_COILS_RESP_TYPE (__LINE__)
typedef struct modbus_write_multiple_coils_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_MULTIPLE_COILS
    uint16_t starting_address; // Echo of request
    uint16_t quantity_of_outputs; // Echo of request
} modbus_write_multiple_coils_resp_t;

// Write Multiple Coils Response function declarations
ptk_err modbus_write_multiple_coils_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_multiple_coils_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_multiple_coils_resp_t* modbus_write_multiple_coils_resp_create(void *parent);
void modbus_write_multiple_coils_resp_destructor(void *ptr);

//=============================================================================
// FUNCTION CODE 0x10 - WRITE MULTIPLE REGISTERS
//=============================================================================

#define MODBUS_WRITE_MULTIPLE_REGISTERS_REQ_TYPE (__LINE__)
#define MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS 0x10
typedef struct modbus_write_multiple_registers_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS
    uint16_t starting_address;
    uint16_t quantity_of_registers;
    uint8_t byte_count;
    modbus_register_array_t *register_values;  // Variable length
} modbus_write_multiple_registers_req_t;

// Write Multiple Registers Request function declarations
ptk_err modbus_write_multiple_registers_req_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_multiple_registers_req_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_multiple_registers_req_t* modbus_write_multiple_registers_req_create(void *parent);
void modbus_write_multiple_registers_req_destructor(void *ptr);

#define MODBUS_WRITE_MULTIPLE_REGISTERS_RESP_TYPE (__LINE__)
typedef struct modbus_write_multiple_registers_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;     // MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS
    uint16_t starting_address; // Echo of request
    uint16_t quantity_of_registers; // Echo of request
} modbus_write_multiple_registers_resp_t;

// Write Multiple Registers Response function declarations
ptk_err modbus_write_multiple_registers_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_write_multiple_registers_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_write_multiple_registers_resp_t* modbus_write_multiple_registers_resp_create(void *parent);
void modbus_write_multiple_registers_resp_destructor(void *ptr);

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
ptk_err modbus_exception_resp_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err modbus_exception_resp_deserialize(ptk_buf *buf, ptk_serializable_t *obj);
modbus_exception_resp_t* modbus_exception_resp_create(void *parent);
void modbus_exception_resp_destructor(void *ptr);

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
modbus_connection *modbus_open_client(void *parent, ptk_address_t *addr, uint8_t unit_id);
modbus_connection *modbus_open_server(void *parent, ptk_address_t *addr, uint8_t unit_id);
ptk_err modbus_close(modbus_connection *conn);

//=============================================================================
// HIGH-LEVEL CLIENT API
//=============================================================================

// Read functions
ptk_err modbus_client_read_coils(modbus_connection *conn, uint16_t starting_address,
                                uint16_t quantity, modbus_byte_array_t **coil_status);
ptk_err modbus_client_read_discrete_inputs(modbus_connection *conn, uint16_t starting_address,
                                          uint16_t quantity, modbus_byte_array_t **input_status);
ptk_err modbus_client_read_holding_registers(modbus_connection *conn, uint16_t starting_address,
                                            uint16_t quantity, modbus_register_array_t **register_values);
ptk_err modbus_client_read_input_registers(modbus_connection *conn, uint16_t starting_address,
                                          uint16_t quantity, modbus_register_array_t **register_values);

// Write functions
ptk_err modbus_client_write_single_coil(modbus_connection *conn, uint16_t address, bool value);
ptk_err modbus_client_write_single_register(modbus_connection *conn, uint16_t address, uint16_t value);
ptk_err modbus_client_write_multiple_coils(modbus_connection *conn, uint16_t starting_address,
                                          const modbus_byte_array_t *coil_values);
ptk_err modbus_client_write_multiple_registers(modbus_connection *conn, uint16_t starting_address,
                                              const modbus_register_array_t *register_values);

//=============================================================================
// HIGH-LEVEL SERVER API
//=============================================================================

// Server callback types
typedef ptk_err (*modbus_read_coils_handler_t)(uint16_t address, uint16_t quantity, modbus_byte_array_t **coil_status, void *user_data);
typedef ptk_err (*modbus_read_discrete_inputs_handler_t)(uint16_t address, uint16_t quantity, modbus_byte_array_t **input_status, void *user_data);
typedef ptk_err (*modbus_read_holding_registers_handler_t)(uint16_t address, uint16_t quantity, modbus_register_array_t **register_values, void *user_data);
typedef ptk_err (*modbus_read_input_registers_handler_t)(uint16_t address, uint16_t quantity, modbus_register_array_t **register_values, void *user_data);
typedef ptk_err (*modbus_write_single_coil_handler_t)(uint16_t address, bool value, void *user_data);
typedef ptk_err (*modbus_write_single_register_handler_t)(uint16_t address, uint16_t value, void *user_data);
typedef ptk_err (*modbus_write_multiple_coils_handler_t)(uint16_t address, const modbus_byte_array_t *coil_values, void *user_data);
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

// Coil packing/unpacking utilities
ptk_err modbus_pack_coils(const modbus_coil_array_t *coils, modbus_byte_array_t *packed_bytes);
ptk_err modbus_unpack_coils(const modbus_byte_array_t *packed_bytes, uint16_t quantity, modbus_coil_array_t *coils);

// Validation functions
ptk_err modbus_validate_address_range(uint16_t address, uint16_t quantity, uint16_t max_address);
ptk_err modbus_validate_quantity(uint16_t quantity, uint16_t max_quantity);

#endif // MODBUS_H
