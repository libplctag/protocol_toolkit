/**
 * @file modbus.h  
 * @brief Complete Modbus protocol implementation with threadsafe data store
 *
 * This implementation provides:
 * - Modbus TCP and RTU protocol support
 * - Thread-safe register and coil data store
 * - Server and client functionality
 * - All standard Modbus function codes
 * - Error handling and logging
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "buf.h"
#include "log.h"
#include "ev_loop.h"

//=============================================================================
// MODBUS PROTOCOL CONSTANTS
//=============================================================================

// Standard Modbus TCP port
#define MODBUS_TCP_PORT             502

// Maximum payload sizes
#define MODBUS_TCP_MAX_PDU_SIZE     253
#define MODBUS_RTU_MAX_PDU_SIZE     253
#define MODBUS_MAX_REGISTERS        125    // Max registers per read/write
#define MODBUS_MAX_COILS            2000   // Max coils per read/write

// Function codes
#define MODBUS_FC_READ_COILS                0x01
#define MODBUS_FC_READ_DISCRETE_INPUTS      0x02
#define MODBUS_FC_READ_HOLDING_REGISTERS    0x03
#define MODBUS_FC_READ_INPUT_REGISTERS      0x04
#define MODBUS_FC_WRITE_SINGLE_COIL         0x05
#define MODBUS_FC_WRITE_SINGLE_REGISTER     0x06
#define MODBUS_FC_WRITE_MULTIPLE_COILS      0x0F
#define MODBUS_FC_WRITE_MULTIPLE_REGISTERS  0x10

// Exception codes
#define MODBUS_EX_ILLEGAL_FUNCTION          0x01
#define MODBUS_EX_ILLEGAL_DATA_ADDRESS      0x02
#define MODBUS_EX_ILLEGAL_DATA_VALUE        0x03
#define MODBUS_EX_SLAVE_DEVICE_FAILURE      0x04
#define MODBUS_EX_ACKNOWLEDGE               0x05
#define MODBUS_EX_SLAVE_DEVICE_BUSY         0x06
#define MODBUS_EX_MEMORY_PARITY_ERROR       0x08
#define MODBUS_EX_GATEWAY_PATH_UNAVAILABLE  0x0A
#define MODBUS_EX_GATEWAY_TARGET_FAILED     0x0B

// Special coil values for function 0x05
#define MODBUS_COIL_ON                      0xFF00
#define MODBUS_COIL_OFF                     0x0000

// Default data store sizes
#define MODBUS_DEFAULT_COIL_COUNT           10000
#define MODBUS_DEFAULT_DISCRETE_INPUT_COUNT 10000
#define MODBUS_DEFAULT_HOLDING_REG_COUNT    10000
#define MODBUS_DEFAULT_INPUT_REG_COUNT      10000

//=============================================================================
// MODBUS ERROR TYPES
//=============================================================================

typedef enum {
    MODBUS_OK = 0,
    MODBUS_ERR_NULL_PTR,
    MODBUS_ERR_NO_RESOURCES,
    MODBUS_ERR_INVALID_PARAM,
    MODBUS_ERR_ILLEGAL_FUNCTION,
    MODBUS_ERR_ILLEGAL_DATA_ADDRESS,
    MODBUS_ERR_ILLEGAL_DATA_VALUE,
    MODBUS_ERR_SERVER_DEVICE_FAILURE,
    MODBUS_ERR_CRC_MISMATCH,
    MODBUS_ERR_BUFFER_TOO_SMALL,
    MODBUS_ERR_TIMEOUT,
    MODBUS_ERR_CONNECTION_FAILED,
    MODBUS_ERR_PARSE_ERROR,
} modbus_err_t;

//=============================================================================
// MODBUS TCP PROTOCOL STRUCTURES
//=============================================================================

/**
 * @brief Modbus TCP Application Protocol (MBAP) Header (7 bytes)
 * All multi-byte fields are big-endian as per Modbus TCP specification
 */
typedef struct {
    uint16_t transaction_id;       // Transaction identifier
    uint16_t protocol_id;          // Protocol identifier (always 0)
    uint16_t length;               // Length of following bytes
    uint8_t unit_id;               // Unit identifier (slave address)
} modbus_mbap_header_t;

/**
 * @brief Read Coils Request (Function Code 0x01)
 */
typedef struct {
    uint8_t function_code;         // Always 0x01
    uint16_t starting_address;     // Starting coil address
    uint16_t quantity_of_coils;    // Number of coils to read
} modbus_read_coils_req_t;

/**
 * @brief Read Coils Response (Function Code 0x01)
 */
typedef struct {
    uint8_t function_code;         // Always 0x01
    uint8_t byte_count;            // Number of data bytes to follow
    uint8_t *coil_status;          // Coil status bytes (packed bits)
} modbus_read_coils_resp_t;

/**
 * @brief Read Discrete Inputs Request (Function Code 0x02)
 */
typedef struct {
    uint8_t function_code;         // Always 0x02
    uint16_t starting_address;     // Starting input address
    uint16_t quantity_of_inputs;   // Number of inputs to read
} modbus_read_discrete_inputs_req_t;

/**
 * @brief Read Discrete Inputs Response (Function Code 0x02)
 */
typedef struct {
    uint8_t function_code;         // Always 0x02
    uint8_t byte_count;            // Number of data bytes to follow
    uint8_t *input_status;         // Input status bytes (packed bits)
} modbus_read_discrete_inputs_resp_t;

/**
 * @brief Read Holding Registers Request (Function Code 0x03)
 */
typedef struct {
    uint8_t function_code;         // Always 0x03
    uint16_t starting_address;     // Starting register address
    uint16_t quantity_of_registers; // Number of registers to read
} modbus_read_holding_registers_req_t;

/**
 * @brief Read Holding Registers Response (Function Code 0x03)
 */
typedef struct {
    uint8_t function_code;         // Always 0x03
    uint8_t byte_count;            // Number of data bytes to follow
    uint16_t *register_values;     // Register values
} modbus_read_holding_registers_resp_t;

/**
 * @brief Read Input Registers Request (Function Code 0x04)
 */
typedef struct {
    uint8_t function_code;         // Always 0x04
    uint16_t starting_address;     // Starting register address
    uint16_t quantity_of_registers; // Number of registers to read
} modbus_read_input_registers_req_t;

/**
 * @brief Read Input Registers Response (Function Code 0x04)
 */
typedef struct {
    uint8_t function_code;         // Always 0x04
    uint8_t byte_count;            // Number of data bytes to follow
    uint16_t *register_values;     // Register values
} modbus_read_input_registers_resp_t;

/**
 * @brief Write Single Coil Request (Function Code 0x05)
 */
typedef struct {
    uint8_t function_code;         // Always 0x05
    uint16_t output_address;       // Coil address
    uint16_t output_value;         // 0xFF00 (ON) or 0x0000 (OFF)
} modbus_write_single_coil_req_t;

/**
 * @brief Write Single Coil Response (Function Code 0x05)
 */
typedef struct {
    uint8_t function_code;         // Always 0x05
    uint16_t output_address;       // Echo of request
    uint16_t output_value;         // Echo of request
} modbus_write_single_coil_resp_t;

/**
 * @brief Write Single Register Request (Function Code 0x06)
 */
typedef struct {
    uint8_t function_code;         // Always 0x06
    uint16_t register_address;     // Register address
    uint16_t register_value;       // Register value
} modbus_write_single_register_req_t;

/**
 * @brief Write Single Register Response (Function Code 0x06)
 */
typedef struct {
    uint8_t function_code;         // Always 0x06
    uint16_t register_address;     // Echo of request
    uint16_t register_value;       // Echo of request
} modbus_write_single_register_resp_t;

/**
 * @brief Write Multiple Coils Request (Function Code 0x0F)
 */
typedef struct {
    uint8_t function_code;         // Always 0x0F
    uint16_t starting_address;     // Starting coil address
    uint16_t quantity_of_outputs;  // Number of coils to write
    uint8_t byte_count;            // Number of data bytes to follow
    uint8_t *output_values;        // Coil values (packed bits)
} modbus_write_multiple_coils_req_t;

/**
 * @brief Write Multiple Coils Response (Function Code 0x0F)
 */
typedef struct {
    uint8_t function_code;         // Always 0x0F
    uint16_t starting_address;     // Echo of request
    uint16_t quantity_of_outputs;  // Echo of request
} modbus_write_multiple_coils_resp_t;

/**
 * @brief Write Multiple Registers Request (Function Code 0x10)
 */
typedef struct {
    uint8_t function_code;          // Always 0x10
    uint16_t starting_address;      // Starting register address
    uint16_t quantity_of_registers; // Number of registers to write
    uint8_t byte_count;             // Number of data bytes to follow
    uint16_t *register_values;      // Register values
} modbus_write_multiple_registers_req_t;

/**
 * @brief Write Multiple Registers Response (Function Code 0x10)
 */
typedef struct {
    uint8_t function_code;          // Always 0x10
    uint16_t starting_address;      // Echo of request
    uint16_t quantity_of_registers; // Echo of request
} modbus_write_multiple_registers_resp_t;

/**
 * @brief Modbus Exception Response
 */
typedef struct {
    uint8_t function_code;          // Original function code + 0x80
    uint8_t exception_code;         // Exception code
} modbus_exception_resp_t;

//=============================================================================
// THREADSAFE DATA STORE
//=============================================================================

// Forward declarations
typedef struct modbus_data_store modbus_data_store_t;

/**
 * @brief Configuration for Modbus data store
 */
typedef struct {
    uint16_t coil_count;           // Number of coils (default: 10000)
    uint16_t discrete_input_count; // Number of discrete inputs (default: 10000)
    uint16_t holding_register_count; // Number of holding registers (default: 10000)
    uint16_t input_register_count; // Number of input registers (default: 10000)
    bool read_only_coils;          // Make coils read-only (default: false)
    bool read_only_holding_registers; // Make holding registers read-only (default: false)
} modbus_data_store_config_t;

/**
 * @brief Create a new threadsafe Modbus data store
 *
 * @param store Pointer to store the created data store
 * @param config Configuration (NULL for defaults)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_data_store_create(modbus_data_store_t **store, 
                                      const modbus_data_store_config_t *config);

/**
 * @brief Destroy a Modbus data store
 *
 * @param store The data store to destroy
 */
void modbus_data_store_destroy(modbus_data_store_t *store);

/**
 * @brief Read coils from the data store (thread-safe)
 *
 * @param store The data store
 * @param address Starting address
 * @param count Number of coils to read
 * @param values Output buffer for coil values (bits packed in bytes)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_data_store_read_coils(modbus_data_store_t *store,
                                          uint16_t address, uint16_t count,
                                          uint8_t *values);

/**
 * @brief Write coils to the data store (thread-safe)
 *
 * @param store The data store
 * @param address Starting address
 * @param count Number of coils to write
 * @param values Input buffer for coil values (bits packed in bytes)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_data_store_write_coils(modbus_data_store_t *store,
                                           uint16_t address, uint16_t count,
                                           const uint8_t *values);

/**
 * @brief Read discrete inputs from the data store (thread-safe)
 *
 * @param store The data store
 * @param address Starting address
 * @param count Number of inputs to read
 * @param values Output buffer for input values (bits packed in bytes)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_data_store_read_discrete_inputs(modbus_data_store_t *store,
                                                    uint16_t address, uint16_t count,
                                                    uint8_t *values);

/**
 * @brief Read holding registers from the data store (thread-safe)
 *
 * @param store The data store
 * @param address Starting address
 * @param count Number of registers to read
 * @param values Output buffer for register values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_data_store_read_holding_registers(modbus_data_store_t *store,
                                                      uint16_t address, uint16_t count,
                                                      uint16_t *values);

/**
 * @brief Write holding registers to the data store (thread-safe)
 *
 * @param store The data store
 * @param address Starting address
 * @param count Number of registers to write
 * @param values Input buffer for register values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_data_store_write_holding_registers(modbus_data_store_t *store,
                                                       uint16_t address, uint16_t count,
                                                       const uint16_t *values);

/**
 * @brief Read input registers from the data store (thread-safe)
 *
 * @param store The data store
 * @param address Starting address
 * @param count Number of registers to read
 * @param values Output buffer for register values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_data_store_read_input_registers(modbus_data_store_t *store,
                                                    uint16_t address, uint16_t count,
                                                    uint16_t *values);

//=============================================================================
// MODBUS SERVER
//=============================================================================

// Forward declarations
typedef struct modbus_server modbus_server_t;

/**
 * @brief Configuration for Modbus server
 */
typedef struct {
    const char *bind_host;         // Host to bind to (NULL for all interfaces)
    int bind_port;                 // Port to bind to (default: 502)
    modbus_data_store_t *data_store; // Data store to use
    uint8_t unit_id;               // Unit identifier (slave address)
    size_t max_connections;        // Maximum concurrent connections
} modbus_server_config_t;

/**
 * @brief Create and start a Modbus TCP server
 *
 * @param loop The event loop to use
 * @param server Pointer to store the created server
 * @param config Server configuration
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_server_create(ev_loop *loop, modbus_server_t **server,
                                  const modbus_server_config_t *config);

/**
 * @brief Stop and destroy a Modbus server
 *
 * @param server The server to destroy
 */
void modbus_server_destroy(modbus_server_t *server);

//=============================================================================
// MODBUS CLIENT
//=============================================================================

// Forward declarations
typedef struct modbus_client modbus_client_t;

/**
 * @brief Configuration for Modbus client
 */
typedef struct {
    const char *host;              // Server host
    int port;                      // Server port (default: 502)
    uint8_t unit_id;               // Unit identifier (slave address)
    uint32_t timeout_ms;           // Request timeout in milliseconds
} modbus_client_config_t;

/**
 * @brief Create a Modbus TCP client
 *
 * @param loop The event loop to use
 * @param client Pointer to store the created client
 * @param config Client configuration
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_client_create(ev_loop *loop, modbus_client_t **client,
                                  const modbus_client_config_t *config);

/**
 * @brief Destroy a Modbus client
 *
 * @param client The client to destroy
 */
void modbus_client_destroy(modbus_client_t *client);

/**
 * @brief Read coils from a Modbus server (synchronous)
 *
 * @param client The client
 * @param address Starting address
 * @param count Number of coils to read
 * @param values Output buffer for coil values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_client_read_coils(modbus_client_t *client,
                                      uint16_t address, uint16_t count,
                                      uint8_t *values);

/**
 * @brief Read holding registers from a Modbus server (synchronous)
 *
 * @param client The client
 * @param address Starting address
 * @param count Number of registers to read
 * @param values Output buffer for register values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_client_read_holding_registers(modbus_client_t *client,
                                                  uint16_t address, uint16_t count,
                                                  uint16_t *values);

/**
 * @brief Write single coil to a Modbus server (synchronous)
 *
 * @param client The client
 * @param address Coil address
 * @param value Coil value (true/false)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_client_write_single_coil(modbus_client_t *client,
                                             uint16_t address, bool value);

/**
 * @brief Write single register to a Modbus server (synchronous)
 *
 * @param client The client
 * @param address Register address
 * @param value Register value
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_client_write_single_register(modbus_client_t *client,
                                                 uint16_t address, uint16_t value);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Convert Modbus error code to human-readable string
 *
 * @param err The error code
 * @return String description of the error
 */
const char* modbus_err_string(modbus_err_t err);

/**
 * @brief Calculate number of bytes needed to pack bit values
 *
 * @param bit_count Number of bits
 * @return Number of bytes needed
 */
static inline size_t modbus_bits_to_bytes(size_t bit_count) {
    return (bit_count + 7) / 8;
}

/**
 * @brief Pack bit values into bytes
 *
 * @param bits Array of bit values (0 or 1)
 * @param bit_count Number of bits
 * @param bytes Output buffer for packed bytes
 */
void modbus_pack_bits(const uint8_t *bits, size_t bit_count, uint8_t *bytes);

/**
 * @brief Unpack bit values from bytes
 *
 * @param bytes Input buffer of packed bytes
 * @param bit_count Number of bits to unpack
 * @param bits Output array for bit values (0 or 1)
 */
void modbus_unpack_bits(const uint8_t *bytes, size_t bit_count, uint8_t *bits);

//=============================================================================
// PROTOCOL ENCODE/DECODE FUNCTIONS
//=============================================================================

/**
 * @brief Encode MBAP header
 */
modbus_err_t modbus_mbap_header_encode(buf *dest, const modbus_mbap_header_t *header);

/**
 * @brief Decode MBAP header
 */
modbus_err_t modbus_mbap_header_decode(modbus_mbap_header_t *header, buf *src);

/**
 * @brief Encode read holding registers request
 */
modbus_err_t modbus_read_holding_registers_req_encode(buf *dest, 
                                                     const modbus_read_holding_registers_req_t *req);

/**
 * @brief Decode read holding registers request
 */
modbus_err_t modbus_read_holding_registers_req_decode(modbus_read_holding_registers_req_t *req, 
                                                     buf *src);

/**
 * @brief Encode read holding registers response
 */
modbus_err_t modbus_read_holding_registers_resp_encode(buf *dest, 
                                                      const modbus_read_holding_registers_resp_t *resp);

/**
 * @brief Process a complete Modbus TCP request and generate response
 *
 * @param data_store The data store to use
 * @param request_buf Input buffer containing the request
 * @param response_buf Output buffer for the response
 * @param unit_id Expected unit identifier
 * @return MODBUS_OK on success, error code on failure
 */
modbus_err_t modbus_process_request(modbus_data_store_t *data_store,
                                   buf *request_buf, buf **response_buf,
                                   uint8_t unit_id);