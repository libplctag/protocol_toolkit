#include "modbus_internal.h"
#include <ptk_log.h>

//=============================================================================
// INTERNAL HELPER FUNCTION IMPLEMENTATIONS
//=============================================================================

void modbus_pdu_base_init(modbus_pdu_base_t *base, size_t pdu_type,
                          ptk_err (*serialize_fn)(ptk_buf *buf, ptk_serializable_t *obj),
                          ptk_err (*deserialize_fn)(ptk_buf *buf, ptk_serializable_t *obj)) {
    if(!base) { return; }

    base->pdu_type = pdu_type;
    base->buf_base.serialize = serialize_fn;
    base->buf_base.deserialize = deserialize_fn;
}

size_t modbus_get_pdu_type_from_function_code(uint8_t function_code, bool is_request) {
    // Handle exception responses first
    if(function_code >= 0x80) { return MODBUS_EXCEPTION_RESP_TYPE; }

    switch(function_code) {
        case MODBUS_FUNC_READ_COILS: return is_request ? MODBUS_READ_COILS_REQ_TYPE : MODBUS_READ_COILS_RESP_TYPE;

        case MODBUS_FUNC_READ_DISCRETE_INPUTS:
            return is_request ? MODBUS_READ_DISCRETE_INPUTS_REQ_TYPE : MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE;

        case MODBUS_FUNC_READ_HOLDING_REGISTERS:
            return is_request ? MODBUS_READ_HOLDING_REGISTERS_REQ_TYPE : MODBUS_READ_HOLDING_REGISTERS_RESP_TYPE;

        case MODBUS_FUNC_READ_INPUT_REGISTERS:
            return is_request ? MODBUS_READ_INPUT_REGISTERS_REQ_TYPE : MODBUS_READ_INPUT_REGISTERS_RESP_TYPE;

        case MODBUS_FUNC_WRITE_SINGLE_COIL:
            return is_request ? MODBUS_WRITE_SINGLE_COIL_REQ_TYPE : MODBUS_WRITE_SINGLE_COIL_RESP_TYPE;

        case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
            return is_request ? MODBUS_WRITE_SINGLE_REGISTER_REQ_TYPE : MODBUS_WRITE_SINGLE_REGISTER_RESP_TYPE;

        case MODBUS_FUNC_WRITE_MULTIPLE_COILS:
            return is_request ? MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE : MODBUS_WRITE_MULTIPLE_COILS_RESP_TYPE;

        case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
            return is_request ? MODBUS_WRITE_MULTIPLE_REGISTERS_REQ_TYPE : MODBUS_WRITE_MULTIPLE_REGISTERS_RESP_TYPE;

        default: return 0;  // Invalid function code
    }
}

ptk_err modbus_dispatch_pdu_deserializer(ptk_buf *buf, modbus_mbap_t *mbap) {
    if(!buf || !mbap) { return PTK_ERR_NULL_PTR; }

    // Peek at the function code to determine PDU type
    uint8_t function_code;
    ptk_err err = ptk_buf_deserialize(buf, true, PTK_BUF_NATIVE_ENDIAN, function_code);
    if(err != PTK_OK) {
        error("Failed to peek function code from buffer");
        return err;
    }

    // Determine if this is a request or response based on context
    // For now, we'll assume responses based on function code >= 0x80 (exceptions)
    // or use additional context if available
    bool is_request = true;  // This would need more context in real implementation

    // Handle exception responses
    if(function_code >= 0x80) {
        mbap->pdu.exception_resp = modbus_exception_resp_create(mbap);
        if(!mbap->pdu.exception_resp) { return PTK_ERR_NO_RESOURCES; }
        return modbus_exception_resp_deserialize(buf, &mbap->pdu.exception_resp->base.buf_base);
    }

    // Dispatch to appropriate deserializer based on function code
    switch(function_code) {
        case MODBUS_FUNC_READ_COILS:
            if(is_request) {
                mbap->pdu.read_coils_req = modbus_read_coils_req_create(mbap);
                if(!mbap->pdu.read_coils_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_coils_req_deserialize(buf, &mbap->pdu.read_coils_req->base.buf_base);
            } else {
                mbap->pdu.read_coils_resp = modbus_read_coils_resp_create(mbap);
                if(!mbap->pdu.read_coils_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_coils_resp_deserialize(buf, &mbap->pdu.read_coils_resp->base.buf_base);
            }

        case MODBUS_FUNC_READ_DISCRETE_INPUTS:
            if(is_request) {
                mbap->pdu.read_discrete_inputs_req = modbus_read_discrete_inputs_req_create(mbap);
                if(!mbap->pdu.read_discrete_inputs_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_discrete_inputs_req_deserialize(buf, &mbap->pdu.read_discrete_inputs_req->base.buf_base);
            } else {
                mbap->pdu.read_discrete_inputs_resp = modbus_read_discrete_inputs_resp_create(mbap);
                if(!mbap->pdu.read_discrete_inputs_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_discrete_inputs_resp_deserialize(buf, &mbap->pdu.read_discrete_inputs_resp->base.buf_base);
            }

        case MODBUS_FUNC_READ_HOLDING_REGISTERS:
            if(is_request) {
                mbap->pdu.read_holding_registers_req = modbus_read_holding_registers_req_create(mbap);
                if(!mbap->pdu.read_holding_registers_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_holding_registers_req_deserialize(buf, &mbap->pdu.read_holding_registers_req->base.buf_base);
            } else {
                mbap->pdu.read_holding_registers_resp = modbus_read_holding_registers_resp_create(mbap);
                if(!mbap->pdu.read_holding_registers_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_holding_registers_resp_deserialize(buf, &mbap->pdu.read_holding_registers_resp->base.buf_base);
            }

        case MODBUS_FUNC_READ_INPUT_REGISTERS:
            if(is_request) {
                mbap->pdu.read_input_registers_req = modbus_read_input_registers_req_create(mbap);
                if(!mbap->pdu.read_input_registers_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_input_registers_req_deserialize(buf, &mbap->pdu.read_input_registers_req->base.buf_base);
            } else {
                mbap->pdu.read_input_registers_resp = modbus_read_input_registers_resp_create(mbap);
                if(!mbap->pdu.read_input_registers_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_read_input_registers_resp_deserialize(buf, &mbap->pdu.read_input_registers_resp->base.buf_base);
            }

        case MODBUS_FUNC_WRITE_SINGLE_COIL:
            if(is_request) {
                mbap->pdu.write_single_coil_req = modbus_write_single_coil_req_create(mbap);
                if(!mbap->pdu.write_single_coil_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_single_coil_req_deserialize(buf, &mbap->pdu.write_single_coil_req->base.buf_base);
            } else {
                mbap->pdu.write_single_coil_resp = modbus_write_single_coil_resp_create(mbap);
                if(!mbap->pdu.write_single_coil_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_single_coil_resp_deserialize(buf, &mbap->pdu.write_single_coil_resp->base.buf_base);
            }

        case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
            if(is_request) {
                mbap->pdu.write_single_register_req = modbus_write_single_register_req_create(mbap);
                if(!mbap->pdu.write_single_register_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_single_register_req_deserialize(buf, &mbap->pdu.write_single_register_req->base.buf_base);
            } else {
                mbap->pdu.write_single_register_resp = modbus_write_single_register_resp_create(mbap);
                if(!mbap->pdu.write_single_register_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_single_register_resp_deserialize(buf, &mbap->pdu.write_single_register_resp->base.buf_base);
            }

        case MODBUS_FUNC_WRITE_MULTIPLE_COILS:
            if(is_request) {
                mbap->pdu.write_multiple_coils_req = modbus_write_multiple_coils_req_create(mbap);
                if(!mbap->pdu.write_multiple_coils_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_multiple_coils_req_deserialize(buf, &mbap->pdu.write_multiple_coils_req->base.buf_base);
            } else {
                mbap->pdu.write_multiple_coils_resp = modbus_write_multiple_coils_resp_create(mbap);
                if(!mbap->pdu.write_multiple_coils_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_multiple_coils_resp_deserialize(buf, &mbap->pdu.write_multiple_coils_resp->base.buf_base);
            }

        case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
            if(is_request) {
                mbap->pdu.write_multiple_registers_req = modbus_write_multiple_registers_req_create(mbap);
                if(!mbap->pdu.write_multiple_registers_req) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_multiple_registers_req_deserialize(buf,
                                                                       &mbap->pdu.write_multiple_registers_req->base.buf_base);
            } else {
                mbap->pdu.write_multiple_registers_resp = modbus_write_multiple_registers_resp_create(mbap);
                if(!mbap->pdu.write_multiple_registers_resp) { return PTK_ERR_NO_RESOURCES; }
                return modbus_write_multiple_registers_resp_deserialize(buf,
                                                                        &mbap->pdu.write_multiple_registers_resp->base.buf_base);
            }

        default: error("Unknown Modbus function code: 0x%02X", function_code); return PTK_ERR_INVALID_PARAM;
    }
}

ptk_err modbus_validate_request_params(uint16_t address, uint16_t quantity, uint16_t max_address, uint16_t max_quantity) {
    if(quantity == 0 || quantity > max_quantity) {
        error("Invalid quantity: %u (max: %u)", quantity, max_quantity);
        return PTK_ERR_INVALID_PARAM;
    }

    if(address > max_address || (address + quantity - 1) > max_address) {
        error("Invalid address range: %u-%u (max: %u)", address, address + quantity - 1, max_address);
        return PTK_ERR_INVALID_PARAM;
    }

    return PTK_OK;
}

uint16_t modbus_bool_to_coil_value(bool value) { return value ? 0xFF00 : 0x0000; }

bool modbus_coil_value_to_bool(uint16_t coil_value) { return coil_value == 0xFF00; }

//=============================================================================
// MBAP HEADER IMPLEMENTATION
//=============================================================================

ptk_err modbus_mbap_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_mbap_t *mbap = (modbus_mbap_t *)obj;

    // Serialize MBAP header fields in big-endian format (Modbus standard)
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, ptk_buf_byte_swap_u16(mbap->transaction_id),
                                    ptk_buf_byte_swap_u16(mbap->protocol_id), ptk_buf_byte_swap_u16(mbap->length), mbap->unit_id);

    if(err != PTK_OK) {
        error("Failed to serialize MBAP header");
        return err;
    }

    // Serialize the PDU based on which union member is active
    // This would need additional logic to determine which PDU is active
    // For now, we'll add a TODO comment
    // TODO: Implement PDU serialization dispatch

    return PTK_OK;
}

ptk_err modbus_mbap_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    if(!buf || !obj) { return PTK_ERR_NULL_PTR; }

    modbus_mbap_t *mbap = (modbus_mbap_t *)obj;

    // Deserialize MBAP header fields from big-endian format
    uint16_t transaction_id, protocol_id, length;
    uint8_t unit_id;

    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_NATIVE_ENDIAN, &transaction_id, &protocol_id, &length, &unit_id);

    if(err != PTK_OK) {
        error("Failed to deserialize MBAP header");
        return err;
    }

    // Convert from big-endian
    mbap->transaction_id = ptk_buf_byte_swap_u16(transaction_id);
    mbap->protocol_id = ptk_buf_byte_swap_u16(protocol_id);
    mbap->length = ptk_buf_byte_swap_u16(length);
    mbap->unit_id = unit_id;

    // Validate protocol ID
    if(mbap->protocol_id != 0) {
        error("Invalid Modbus protocol ID: %u (expected 0)", mbap->protocol_id);
        return PTK_ERR_INVALID_PARAM;
    }

    // Dispatch PDU deserialization
    return modbus_dispatch_pdu_deserializer(buf, mbap);
}

modbus_mbap_t *modbus_mbap_create(void *parent) {
    modbus_mbap_t *mbap = ptk_alloc(parent, sizeof(modbus_mbap_t), modbus_mbap_destructor);
    if(!mbap) {
        error("Failed to allocate MBAP structure");
        return NULL;
    }

    // Initialize the base structure
    modbus_pdu_base_init(&mbap->base, MODBUS_MBAP_TYPE, modbus_mbap_serialize, modbus_mbap_deserialize);

    // Initialize MBAP fields
    mbap->transaction_id = 0;
    mbap->protocol_id = 0;
    mbap->length = 0;
    mbap->unit_id = 0;

    // Initialize union to NULL
    memset(&mbap->pdu, 0, sizeof(mbap->pdu));

    debug("Created MBAP structure");
    return mbap;
}

void modbus_mbap_destructor(void *ptr) {
    if(!ptr) { return; }

    modbus_mbap_t *mbap = (modbus_mbap_t *)ptr;
    debug("Destroying MBAP structure");

    // PDU cleanup is handled automatically by parent-child allocation
    // since PDUs are allocated as children of the MBAP
}

//=============================================================================
// UTILITY FUNCTION IMPLEMENTATIONS
//=============================================================================

ptk_err modbus_pack_coils(const modbus_coil_array_t *coils, modbus_byte_array_t *packed_bytes) {
    if(!coils || !packed_bytes) { return PTK_ERR_NULL_PTR; }

    size_t num_coils = modbus_coil_array_len(coils);
    if(num_coils == 0) { return PTK_OK; }

    // Calculate number of bytes needed
    size_t num_bytes = (num_coils + 7) / 8;

    // Resize the byte array
    ptk_err err = modbus_byte_array_resize(packed_bytes, num_bytes);
    if(err != PTK_OK) { return err; }

    // Pack coils into bytes
    for(size_t i = 0; i < num_coils; i++) {
        bool coil_value;
        err = modbus_coil_array_get(coils, i, &coil_value);
        if(err != PTK_OK) { return err; }

        size_t byte_index = i / 8;
        size_t bit_index = i % 8;

        uint8_t byte_value;
        err = modbus_byte_array_get(packed_bytes, byte_index, &byte_value);
        if(err != PTK_OK) { return err; }

        if(coil_value) {
            byte_value |= (1 << bit_index);
        } else {
            byte_value &= ~(1 << bit_index);
        }

        err = modbus_byte_array_set(packed_bytes, byte_index, byte_value);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

ptk_err modbus_unpack_coils(const modbus_byte_array_t *packed_bytes, uint16_t quantity, modbus_coil_array_t *coils) {
    if(!packed_bytes || !coils) { return PTK_ERR_NULL_PTR; }

    if(quantity == 0) { return PTK_OK; }

    // Resize the coil array
    ptk_err err = modbus_coil_array_resize(coils, quantity);
    if(err != PTK_OK) { return err; }

    // Unpack bytes into coils
    for(uint16_t i = 0; i < quantity; i++) {
        size_t byte_index = i / 8;
        size_t bit_index = i % 8;

        uint8_t byte_value;
        err = modbus_byte_array_get(packed_bytes, byte_index, &byte_value);
        if(err != PTK_OK) { return err; }

        bool coil_value = (byte_value & (1 << bit_index)) != 0;

        err = modbus_coil_array_set(coils, i, coil_value);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

ptk_err modbus_validate_address_range(uint16_t address, uint16_t quantity, uint16_t max_address) {
    return modbus_validate_request_params(address, quantity, max_address, quantity);
}

ptk_err modbus_validate_quantity(uint16_t quantity, uint16_t max_quantity) {
    if(quantity == 0 || quantity > max_quantity) {
        error("Invalid quantity: %u (max: %u)", quantity, max_quantity);
        return PTK_ERR_INVALID_PARAM;
    }
    return PTK_OK;
}
