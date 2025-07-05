/**
 * @file modbus.c
 * @brief Complete Modbus protocol implementation
 */

#include "modbus.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

//=============================================================================
// THREADSAFE DATA STORE IMPLEMENTATION
//=============================================================================

/**
 * @brief Internal data store structure
 */
struct modbus_data_store {
    // Mutexes for thread safety
    ev_mutex *coils_mutex;
    ev_mutex *discrete_inputs_mutex;
    ev_mutex *holding_registers_mutex;
    ev_mutex *input_registers_mutex;
    
    // Data arrays
    uint8_t *coils;                    // Packed bits (1 bit per coil)
    uint8_t *discrete_inputs;          // Packed bits (1 bit per input)
    uint16_t *holding_registers;       // 16-bit registers
    uint16_t *input_registers;         // 16-bit registers
    
    // Configuration
    uint16_t coil_count;
    uint16_t discrete_input_count;
    uint16_t holding_register_count;
    uint16_t input_register_count;
    bool read_only_coils;
    bool read_only_holding_registers;
};

modbus_err_t modbus_data_store_create(modbus_data_store_t **store, 
                                      const modbus_data_store_config_t *config) {
    if (!store) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    *store = calloc(1, sizeof(modbus_data_store_t));
    if (!*store) {
        error("Failed to allocate data store");
        return MODBUS_ERR_NO_RESOURCES;
    }
    
    modbus_data_store_t *ds = *store;
    
    // Apply configuration or defaults
    if (config) {
        ds->coil_count = config->coil_count ? config->coil_count : MODBUS_DEFAULT_COIL_COUNT;
        ds->discrete_input_count = config->discrete_input_count ? config->discrete_input_count : MODBUS_DEFAULT_DISCRETE_INPUT_COUNT;
        ds->holding_register_count = config->holding_register_count ? config->holding_register_count : MODBUS_DEFAULT_HOLDING_REG_COUNT;
        ds->input_register_count = config->input_register_count ? config->input_register_count : MODBUS_DEFAULT_INPUT_REG_COUNT;
        ds->read_only_coils = config->read_only_coils;
        ds->read_only_holding_registers = config->read_only_holding_registers;
    } else {
        ds->coil_count = MODBUS_DEFAULT_COIL_COUNT;
        ds->discrete_input_count = MODBUS_DEFAULT_DISCRETE_INPUT_COUNT;
        ds->holding_register_count = MODBUS_DEFAULT_HOLDING_REG_COUNT;
        ds->input_register_count = MODBUS_DEFAULT_INPUT_REG_COUNT;
    }
    
    // Create mutexes
    if (ev_mutex_create(&ds->coils_mutex) != EV_OK ||
        ev_mutex_create(&ds->discrete_inputs_mutex) != EV_OK ||
        ev_mutex_create(&ds->holding_registers_mutex) != EV_OK ||
        ev_mutex_create(&ds->input_registers_mutex) != EV_OK) {
        error("Failed to create mutexes for data store");
        modbus_data_store_destroy(ds);
        *store = NULL;
        return MODBUS_ERR_NO_RESOURCES;
    }
    
    // Allocate data arrays
    size_t coil_bytes = modbus_bits_to_bytes(ds->coil_count);
    size_t input_bytes = modbus_bits_to_bytes(ds->discrete_input_count);
    
    ds->coils = calloc(coil_bytes, 1);
    ds->discrete_inputs = calloc(input_bytes, 1);
    ds->holding_registers = calloc(ds->holding_register_count, sizeof(uint16_t));
    ds->input_registers = calloc(ds->input_register_count, sizeof(uint16_t));
    
    if (!ds->coils || !ds->discrete_inputs || !ds->holding_registers || !ds->input_registers) {
        error("Failed to allocate data arrays for data store");
        modbus_data_store_destroy(ds);
        *store = NULL;
        return MODBUS_ERR_NO_RESOURCES;
    }
    
    info("Created Modbus data store: %u coils, %u discrete inputs, %u holding registers, %u input registers",
         ds->coil_count, ds->discrete_input_count, ds->holding_register_count, ds->input_register_count);
    
    return MODBUS_OK;
}

void modbus_data_store_destroy(modbus_data_store_t *store) {
    if (!store) {
        return;
    }
    
    // Destroy mutexes
    if (store->coils_mutex) ev_mutex_destroy(store->coils_mutex);
    if (store->discrete_inputs_mutex) ev_mutex_destroy(store->discrete_inputs_mutex);
    if (store->holding_registers_mutex) ev_mutex_destroy(store->holding_registers_mutex);
    if (store->input_registers_mutex) ev_mutex_destroy(store->input_registers_mutex);
    
    // Free data arrays
    free(store->coils);
    free(store->discrete_inputs);
    free(store->holding_registers);
    free(store->input_registers);
    
    free(store);
}

modbus_err_t modbus_data_store_read_coils(modbus_data_store_t *store,
                                          uint16_t address, uint16_t count,
                                          uint8_t *values) {
    if (!store || !values) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    if (address >= store->coil_count || address + count > store->coil_count) {
        return MODBUS_ERR_ILLEGAL_DATA_ADDRESS;
    }
    
    if (count == 0 || count > MODBUS_MAX_COILS) {
        return MODBUS_ERR_ILLEGAL_DATA_VALUE;
    }
    
    ev_err mutex_result = ev_mutex_wait_lock(store->coils_mutex, THREAD_WAIT_FOREVER);
    if (mutex_result != EV_OK) {
        error("Failed to lock coils mutex: %d", mutex_result);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }
    
    // Extract bits from packed storage
    size_t byte_count = modbus_bits_to_bytes(count);
    memset(values, 0, byte_count);
    
    for (uint16_t i = 0; i < count; i++) {
        uint16_t bit_addr = address + i;
        uint16_t byte_index = bit_addr / 8;
        uint8_t bit_index = bit_addr % 8;
        
        if (store->coils[byte_index] & (1 << bit_index)) {
            values[i / 8] |= (1 << (i % 8));
        }
    }
    
    ev_mutex_unlock(store->coils_mutex);
    return MODBUS_OK;
}

modbus_err_t modbus_data_store_write_coils(modbus_data_store_t *store,
                                           uint16_t address, uint16_t count,
                                           const uint8_t *values) {
    if (!store || !values) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    if (store->read_only_coils) {
        return MODBUS_ERR_ILLEGAL_FUNCTION;
    }
    
    if (address >= store->coil_count || address + count > store->coil_count) {
        return MODBUS_ERR_ILLEGAL_DATA_ADDRESS;
    }
    
    if (count == 0 || count > MODBUS_MAX_COILS) {
        return MODBUS_ERR_ILLEGAL_DATA_VALUE;
    }
    
    ev_err mutex_result = ev_mutex_wait_lock(store->coils_mutex, THREAD_WAIT_FOREVER);
    if (mutex_result != EV_OK) {
        error("Failed to lock coils mutex: %d", mutex_result);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }
    
    // Write bits to packed storage
    for (uint16_t i = 0; i < count; i++) {
        uint16_t bit_addr = address + i;
        uint16_t byte_index = bit_addr / 8;
        uint8_t bit_index = bit_addr % 8;
        
        if (values[i / 8] & (1 << (i % 8))) {
            store->coils[byte_index] |= (1 << bit_index);
        } else {
            store->coils[byte_index] &= ~(1 << bit_index);
        }
    }
    
    ev_mutex_unlock(store->coils_mutex);
    return MODBUS_OK;
}

modbus_err_t modbus_data_store_read_discrete_inputs(modbus_data_store_t *store,
                                                    uint16_t address, uint16_t count,
                                                    uint8_t *values) {
    if (!store || !values) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    if (address >= store->discrete_input_count || address + count > store->discrete_input_count) {
        return MODBUS_ERR_ILLEGAL_DATA_ADDRESS;
    }
    
    if (count == 0 || count > MODBUS_MAX_COILS) {
        return MODBUS_ERR_ILLEGAL_DATA_VALUE;
    }
    
    ev_err mutex_result = ev_mutex_wait_lock(store->discrete_inputs_mutex, THREAD_WAIT_FOREVER);
    if (mutex_result != EV_OK) {
        error("Failed to lock discrete inputs mutex: %d", mutex_result);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }
    
    // Extract bits from packed storage
    size_t byte_count = modbus_bits_to_bytes(count);
    memset(values, 0, byte_count);
    
    for (uint16_t i = 0; i < count; i++) {
        uint16_t bit_addr = address + i;
        uint16_t byte_index = bit_addr / 8;
        uint8_t bit_index = bit_addr % 8;
        
        if (store->discrete_inputs[byte_index] & (1 << bit_index)) {
            values[i / 8] |= (1 << (i % 8));
        }
    }
    
    ev_mutex_unlock(store->discrete_inputs_mutex);
    return MODBUS_OK;
}

modbus_err_t modbus_data_store_read_holding_registers(modbus_data_store_t *store,
                                                      uint16_t address, uint16_t count,
                                                      uint16_t *values) {
    if (!store || !values) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    if (address >= store->holding_register_count || address + count > store->holding_register_count) {
        return MODBUS_ERR_ILLEGAL_DATA_ADDRESS;
    }
    
    if (count == 0 || count > MODBUS_MAX_REGISTERS) {
        return MODBUS_ERR_ILLEGAL_DATA_VALUE;
    }
    
    ev_err mutex_result = ev_mutex_wait_lock(store->holding_registers_mutex, THREAD_WAIT_FOREVER);
    if (mutex_result != EV_OK) {
        error("Failed to lock holding registers mutex: %d", mutex_result);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }
    
    memcpy(values, &store->holding_registers[address], count * sizeof(uint16_t));
    
    ev_mutex_unlock(store->holding_registers_mutex);
    return MODBUS_OK;
}

modbus_err_t modbus_data_store_write_holding_registers(modbus_data_store_t *store,
                                                       uint16_t address, uint16_t count,
                                                       const uint16_t *values) {
    if (!store || !values) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    if (store->read_only_holding_registers) {
        return MODBUS_ERR_ILLEGAL_FUNCTION;
    }
    
    if (address >= store->holding_register_count || address + count > store->holding_register_count) {
        return MODBUS_ERR_ILLEGAL_DATA_ADDRESS;
    }
    
    if (count == 0 || count > MODBUS_MAX_REGISTERS) {
        return MODBUS_ERR_ILLEGAL_DATA_VALUE;
    }
    
    ev_err mutex_result = ev_mutex_wait_lock(store->holding_registers_mutex, THREAD_WAIT_FOREVER);
    if (mutex_result != EV_OK) {
        error("Failed to lock holding registers mutex: %d", mutex_result);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }
    
    memcpy(&store->holding_registers[address], values, count * sizeof(uint16_t));
    
    ev_mutex_unlock(store->holding_registers_mutex);
    return MODBUS_OK;
}

modbus_err_t modbus_data_store_read_input_registers(modbus_data_store_t *store,
                                                    uint16_t address, uint16_t count,
                                                    uint16_t *values) {
    if (!store || !values) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    if (address >= store->input_register_count || address + count > store->input_register_count) {
        return MODBUS_ERR_ILLEGAL_DATA_ADDRESS;
    }
    
    if (count == 0 || count > MODBUS_MAX_REGISTERS) {
        return MODBUS_ERR_ILLEGAL_DATA_VALUE;
    }
    
    ev_err mutex_result = ev_mutex_wait_lock(store->input_registers_mutex, THREAD_WAIT_FOREVER);
    if (mutex_result != EV_OK) {
        error("Failed to lock input registers mutex: %d", mutex_result);
        return MODBUS_ERR_SERVER_DEVICE_FAILURE;
    }
    
    memcpy(values, &store->input_registers[address], count * sizeof(uint16_t));
    
    ev_mutex_unlock(store->input_registers_mutex);
    return MODBUS_OK;
}

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

const char* modbus_err_string(modbus_err_t err) {
    switch (err) {
        case MODBUS_OK: return "Success";
        case MODBUS_ERR_NULL_PTR: return "Null pointer";
        case MODBUS_ERR_NO_RESOURCES: return "No resources";
        case MODBUS_ERR_INVALID_PARAM: return "Invalid parameter";
        case MODBUS_ERR_ILLEGAL_FUNCTION: return "Illegal function code";
        case MODBUS_ERR_ILLEGAL_DATA_ADDRESS: return "Illegal data address";
        case MODBUS_ERR_ILLEGAL_DATA_VALUE: return "Illegal data value";
        case MODBUS_ERR_SERVER_DEVICE_FAILURE: return "Server device failure";
        case MODBUS_ERR_CRC_MISMATCH: return "CRC mismatch";
        case MODBUS_ERR_BUFFER_TOO_SMALL: return "Buffer too small";
        case MODBUS_ERR_TIMEOUT: return "Timeout";
        case MODBUS_ERR_CONNECTION_FAILED: return "Connection failed";
        case MODBUS_ERR_PARSE_ERROR: return "Parse error";
        default: return "Unknown error";
    }
}

void modbus_pack_bits(const uint8_t *bits, size_t bit_count, uint8_t *bytes) {
    if (!bits || !bytes) return;
    
    size_t byte_count = modbus_bits_to_bytes(bit_count);
    memset(bytes, 0, byte_count);
    
    for (size_t i = 0; i < bit_count; i++) {
        if (bits[i]) {
            bytes[i / 8] |= (1 << (i % 8));
        }
    }
}

void modbus_unpack_bits(const uint8_t *bytes, size_t bit_count, uint8_t *bits) {
    if (!bytes || !bits) return;
    
    for (size_t i = 0; i < bit_count; i++) {
        bits[i] = (bytes[i / 8] & (1 << (i % 8))) ? 1 : 0;
    }
}

//=============================================================================
// PROTOCOL ENCODE/DECODE FUNCTIONS
//=============================================================================

modbus_err_t modbus_mbap_header_encode(buf *dest, const modbus_mbap_header_t *header) {
    if (!dest || !header) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    buf_err_t result = buf_encode(dest, true, "> u16 u16 u16 u8", 
                                  header->transaction_id, header->protocol_id, 
                                  header->length, header->unit_id);
    
    return (result == BUF_OK) ? MODBUS_OK : MODBUS_ERR_BUFFER_TOO_SMALL;
}

modbus_err_t modbus_mbap_header_decode(modbus_mbap_header_t *header, buf *src) {
    if (!header || !src) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    buf_err_t result = buf_decode(src, false, "> u16 u16 u16 u8", 
                                  &header->transaction_id, &header->protocol_id, 
                                  &header->length, &header->unit_id);
    
    if (result != BUF_OK) {
        return MODBUS_ERR_PARSE_ERROR;
    }
    
    return MODBUS_OK;
}

modbus_err_t modbus_read_holding_registers_req_encode(buf *dest, 
                                                     const modbus_read_holding_registers_req_t *req) {
    if (!dest || !req) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    buf_err_t result = buf_encode(dest, true, "> u8 u16 u16", 
                                  req->function_code, req->starting_address, 
                                  req->quantity_of_registers);
    
    return (result == BUF_OK) ? MODBUS_OK : MODBUS_ERR_BUFFER_TOO_SMALL;
}

modbus_err_t modbus_read_holding_registers_req_decode(modbus_read_holding_registers_req_t *req, 
                                                     buf *src) {
    if (!req || !src) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    buf_err_t result = buf_decode(src, false, "> u8 u16 u16", 
                                  &req->function_code, &req->starting_address, 
                                  &req->quantity_of_registers);
    
    if (result != BUF_OK) {
        return MODBUS_ERR_PARSE_ERROR;
    }
    
    return MODBUS_OK;
}

modbus_err_t modbus_read_holding_registers_resp_encode(buf *dest, 
                                                      const modbus_read_holding_registers_resp_t *resp) {
    if (!dest || !resp) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    buf_err_t result = buf_encode(dest, true, "> u8 u8", resp->function_code, resp->byte_count);
    if (result != BUF_OK) {
        return MODBUS_ERR_BUFFER_TOO_SMALL;
    }
    
    // Write register values  
    uint16_t register_count = resp->byte_count / 2;
    for (uint16_t i = 0; i < register_count; i++) {
        result = buf_encode(dest, true, "> u16", resp->register_values[i]);
        if (result != BUF_OK) {
            return MODBUS_ERR_BUFFER_TOO_SMALL;
        }
    }
    
    return MODBUS_OK;
}

//=============================================================================
// REQUEST PROCESSING
//=============================================================================

static modbus_err_t create_exception_response(buf **response_buf, uint8_t function_code, uint8_t exception_code) {
    buf_err_t buf_result = buf_alloc(response_buf, 2);
    if (buf_result != BUF_OK) {
        return MODBUS_ERR_NO_RESOURCES;
    }
    
    buf_encode(*response_buf, false, "> u8 u8", function_code | 0x80, exception_code);
    
    return MODBUS_OK;
}

static modbus_err_t process_read_holding_registers(modbus_data_store_t *data_store,
                                                  buf *request_buf, buf **response_buf) {
    modbus_read_holding_registers_req_t req;
    modbus_err_t result = modbus_read_holding_registers_req_decode(&req, request_buf);
    if (result != MODBUS_OK) {
        return create_exception_response(response_buf, MODBUS_FC_READ_HOLDING_REGISTERS, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    uint16_t address = req.starting_address;
    uint16_t count = req.quantity_of_registers;
    
    // Validate request
    if (count == 0 || count > MODBUS_MAX_REGISTERS) {
        return create_exception_response(response_buf, MODBUS_FC_READ_HOLDING_REGISTERS, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    // Allocate temporary buffer for register values
    uint16_t *values = malloc(count * sizeof(uint16_t));
    if (!values) {
        return create_exception_response(response_buf, MODBUS_FC_READ_HOLDING_REGISTERS, MODBUS_EX_SLAVE_DEVICE_FAILURE);
    }
    
    // Read from data store
    result = modbus_data_store_read_holding_registers(data_store, address, count, values);
    if (result != MODBUS_OK) {
        free(values);
        uint8_t exception_code = (result == MODBUS_ERR_ILLEGAL_DATA_ADDRESS) ? 
                                MODBUS_EX_ILLEGAL_DATA_ADDRESS : MODBUS_EX_SLAVE_DEVICE_FAILURE;
        return create_exception_response(response_buf, MODBUS_FC_READ_HOLDING_REGISTERS, exception_code);
    }
    
    // Create response
    uint8_t byte_count = count * 2;
    buf_err_t buf_result = buf_alloc(response_buf, 2 + byte_count);
    if (buf_result != BUF_OK) {
        free(values);
        return create_exception_response(response_buf, MODBUS_FC_READ_HOLDING_REGISTERS, MODBUS_EX_SLAVE_DEVICE_FAILURE);
    }
    
    buf_encode(*response_buf, false, "> u8 u8", MODBUS_FC_READ_HOLDING_REGISTERS, byte_count);
    
    for (uint16_t i = 0; i < count; i++) {
        buf_encode(*response_buf, false, "> u16", values[i]);
    }
    
    free(values);
    return MODBUS_OK;
}

static modbus_err_t process_read_coils(modbus_data_store_t *data_store,
                                      buf *request_buf, buf **response_buf) {
    modbus_read_coils_req_t req;
    
    // Decode request
    uint8_t function_code;
    uint16_t starting_address, quantity_of_coils;
    
    buf_err_t buf_result = buf_decode(request_buf, false, "> u8 u16 u16", 
                                      &function_code, &starting_address, &quantity_of_coils);
    
    if (buf_result != BUF_OK) {
        return create_exception_response(response_buf, MODBUS_FC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    // Validate request
    if (quantity_of_coils == 0 || quantity_of_coils > MODBUS_MAX_COILS) {
        return create_exception_response(response_buf, MODBUS_FC_READ_COILS, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    // Allocate temporary buffer for coil values
    uint8_t byte_count = modbus_bits_to_bytes(quantity_of_coils);
    uint8_t *values = malloc(byte_count);
    if (!values) {
        return create_exception_response(response_buf, MODBUS_FC_READ_COILS, MODBUS_EX_SLAVE_DEVICE_FAILURE);
    }
    
    // Read from data store
    modbus_err_t result = modbus_data_store_read_coils(data_store, starting_address, quantity_of_coils, values);
    if (result != MODBUS_OK) {
        free(values);
        uint8_t exception_code = (result == MODBUS_ERR_ILLEGAL_DATA_ADDRESS) ? 
                                MODBUS_EX_ILLEGAL_DATA_ADDRESS : MODBUS_EX_SLAVE_DEVICE_FAILURE;
        return create_exception_response(response_buf, MODBUS_FC_READ_COILS, exception_code);
    }
    
    // Create response
    buf_result = buf_alloc(response_buf, 2 + byte_count);
    if (buf_result != BUF_OK) {
        free(values);
        return create_exception_response(response_buf, MODBUS_FC_READ_COILS, MODBUS_EX_SLAVE_DEVICE_FAILURE);
    }
    
    buf_encode(*response_buf, false, "> u8 u8", MODBUS_FC_READ_COILS, byte_count);
    
    // Write coil values byte by byte
    for (size_t i = 0; i < byte_count; i++) {
        buf_encode(*response_buf, false, "u8", values[i]);
    }
    
    free(values);
    return MODBUS_OK;
}

static modbus_err_t process_write_single_coil(modbus_data_store_t *data_store,
                                             buf *request_buf, buf **response_buf) {
    // Decode request
    uint8_t function_code;
    uint16_t output_address, output_value;
    
    buf_err_t buf_result = buf_decode(request_buf, false, "> u8 u16 u16", 
                                      &function_code, &output_address, &output_value);
    
    if (buf_result != BUF_OK) {
        return create_exception_response(response_buf, MODBUS_FC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    // Validate coil value
    if (output_value != MODBUS_COIL_ON && output_value != MODBUS_COIL_OFF) {
        return create_exception_response(response_buf, MODBUS_FC_WRITE_SINGLE_COIL, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    // Write to data store
    uint8_t coil_value = (output_value == MODBUS_COIL_ON) ? 1 : 0;
    modbus_err_t result = modbus_data_store_write_coils(data_store, output_address, 1, &coil_value);
    if (result != MODBUS_OK) {
        uint8_t exception_code = (result == MODBUS_ERR_ILLEGAL_DATA_ADDRESS) ? 
                                MODBUS_EX_ILLEGAL_DATA_ADDRESS : 
                                (result == MODBUS_ERR_ILLEGAL_FUNCTION) ?
                                MODBUS_EX_ILLEGAL_FUNCTION : MODBUS_EX_SLAVE_DEVICE_FAILURE;
        return create_exception_response(response_buf, MODBUS_FC_WRITE_SINGLE_COIL, exception_code);
    }
    
    // Create response (echo of request)
    buf_result = buf_alloc(response_buf, 5);
    if (buf_result != BUF_OK) {
        return create_exception_response(response_buf, MODBUS_FC_WRITE_SINGLE_COIL, MODBUS_EX_SLAVE_DEVICE_FAILURE);
    }
    
    buf_encode(*response_buf, false, "> u8 u16 u16", MODBUS_FC_WRITE_SINGLE_COIL, output_address, output_value);
    
    return MODBUS_OK;
}

static modbus_err_t process_write_single_register(modbus_data_store_t *data_store,
                                                 buf *request_buf, buf **response_buf) {
    // Decode request
    uint8_t function_code;
    uint16_t register_address, register_value;
    
    buf_err_t buf_result = buf_decode(request_buf, false, "> u8 u16 u16", 
                                      &function_code, &register_address, &register_value);
    
    if (buf_result != BUF_OK) {
        return create_exception_response(response_buf, MODBUS_FC_WRITE_SINGLE_REGISTER, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    // Write to data store
    modbus_err_t result = modbus_data_store_write_holding_registers(data_store, register_address, 1, &register_value);
    if (result != MODBUS_OK) {
        uint8_t exception_code = (result == MODBUS_ERR_ILLEGAL_DATA_ADDRESS) ? 
                                MODBUS_EX_ILLEGAL_DATA_ADDRESS :
                                (result == MODBUS_ERR_ILLEGAL_FUNCTION) ?
                                MODBUS_EX_ILLEGAL_FUNCTION : MODBUS_EX_SLAVE_DEVICE_FAILURE;
        return create_exception_response(response_buf, MODBUS_FC_WRITE_SINGLE_REGISTER, exception_code);
    }
    
    // Create response (echo of request)
    buf_result = buf_alloc(response_buf, 5);
    if (buf_result != BUF_OK) {
        return create_exception_response(response_buf, MODBUS_FC_WRITE_SINGLE_REGISTER, MODBUS_EX_SLAVE_DEVICE_FAILURE);
    }
    
    buf_encode(*response_buf, false, "> u8 u16 u16", MODBUS_FC_WRITE_SINGLE_REGISTER, register_address, register_value);
    
    return MODBUS_OK;
}

modbus_err_t modbus_process_request(modbus_data_store_t *data_store,
                                   buf *request_buf, buf **response_buf,
                                   uint8_t unit_id) {
    if (!data_store || !request_buf || !response_buf) {
        return MODBUS_ERR_NULL_PTR;
    }
    
    *response_buf = NULL;
    
    // Decode MBAP header
    modbus_mbap_header_t mbap_header;
    modbus_err_t result = modbus_mbap_header_decode(&mbap_header, request_buf);
    if (result != MODBUS_OK) {
        return result;
    }
    
    // Check unit ID
    if (mbap_header.unit_id != unit_id) {
        // Silently ignore request for different unit
        return MODBUS_OK;
    }
    
    // Read function code
    uint8_t function_code;
    if (buf_decode(request_buf, false, "u8", &function_code) != BUF_OK) {
        return create_exception_response(response_buf, 0, MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    
    // Process based on function code
    buf *pdu_response = NULL;
    switch (function_code) {
        case MODBUS_FC_READ_COILS:
            // Reset buffer position to read the function code again
            buf_set_cursor(request_buf, buf_get_cursor(request_buf) - 1);
            result = process_read_coils(data_store, request_buf, &pdu_response);
            break;
            
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            // Reset buffer position to read the function code again
            buf_set_cursor(request_buf, buf_get_cursor(request_buf) - 1);
            result = process_read_holding_registers(data_store, request_buf, &pdu_response);
            break;
            
        case MODBUS_FC_WRITE_SINGLE_COIL:
            // Reset buffer position to read the function code again
            buf_set_cursor(request_buf, buf_get_cursor(request_buf) - 1);
            result = process_write_single_coil(data_store, request_buf, &pdu_response);
            break;
            
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            // Reset buffer position to read the function code again
            buf_set_cursor(request_buf, buf_get_cursor(request_buf) - 1);
            result = process_write_single_register(data_store, request_buf, &pdu_response);
            break;
            
        default:
            result = create_exception_response(&pdu_response, function_code, MODBUS_EX_ILLEGAL_FUNCTION);
            break;
    }
    
    if (result != MODBUS_OK || !pdu_response) {
        return result;
    }
    
    // Create full response with MBAP header
    size_t pdu_length = pdu_response->len;
    buf_err_t buf_result = buf_alloc(response_buf, 7 + pdu_length);
    if (buf_result != BUF_OK) {
        buf_free(pdu_response);
        return MODBUS_ERR_NO_RESOURCES;
    }
    
    // Write MBAP header
    modbus_mbap_header_t response_header = mbap_header;
    response_header.length = pdu_length + 1; // +1 for unit ID
    modbus_mbap_header_encode(*response_buf, &response_header);
    
    // Write PDU - copy byte by byte
    buf_set_cursor(pdu_response, 0);
    for (size_t i = 0; i < pdu_length; i++) {
        buf_encode(*response_buf, false, "u8", pdu_response->data[i]);
    }
    
    buf_free(pdu_response);
    return MODBUS_OK;
}