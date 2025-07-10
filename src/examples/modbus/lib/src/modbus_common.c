#include "../include/modbus.h"

//=============================================================================
// COMMON UTILITIES AND VALIDATION
//=============================================================================

size_t modbus_get_pdu_type_from_function_code(uint8_t function_code, bool is_request) {
    // Handle exception responses (function code with 0x80 bit set)
    if(function_code & 0x80) { return MODBUS_EXCEPTION_RESP_TYPE; }

    switch(function_code) {
        case MODBUS_FC_READ_COILS: return is_request ? MODBUS_READ_COILS_REQ_TYPE : MODBUS_READ_COILS_RESP_TYPE;

        case MODBUS_FC_READ_DISCRETE_INPUTS:
            return is_request ? MODBUS_READ_DISCRETE_INPUTS_REQ_TYPE : MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE;

        case MODBUS_FC_READ_HOLDING_REGISTERS:
            return is_request ? MODBUS_READ_HOLDING_REGISTERS_REQ_TYPE : MODBUS_READ_HOLDING_REGISTERS_RESP_TYPE;

        case MODBUS_FC_READ_INPUT_REGISTERS:
            return is_request ? MODBUS_READ_INPUT_REGISTERS_REQ_TYPE : MODBUS_READ_INPUT_REGISTERS_RESP_TYPE;

        case MODBUS_FC_WRITE_SINGLE_COIL:
            return is_request ? MODBUS_WRITE_SINGLE_COIL_REQ_TYPE : MODBUS_WRITE_SINGLE_COIL_RESP_TYPE;

        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            return is_request ? MODBUS_WRITE_SINGLE_REGISTER_REQ_TYPE : MODBUS_WRITE_SINGLE_REGISTER_RESP_TYPE;

        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            return is_request ? MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE : MODBUS_WRITE_MULTIPLE_COILS_RESP_TYPE;

        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            return is_request ? MODBUS_WRITE_MULTIPLE_REGISTERS_REQ_TYPE : MODBUS_WRITE_MULTIPLE_REGISTERS_RESP_TYPE;

        default: return 0;  // Unknown function code
    }
}

ptk_err modbus_validate_address_range(uint16_t address, uint16_t quantity, uint16_t max_address) {
    // Check for overflow in address + quantity
    if(address > max_address) { return PTK_ERR_OUT_OF_BOUNDS; }

    if(quantity == 0) { return PTK_ERR_INVALID_PARAM; }

    // Check if address + quantity - 1 would exceed max_address
    if(address + quantity - 1 > max_address) { return PTK_ERR_OUT_OF_BOUNDS; }

    return PTK_OK;
}

ptk_err modbus_validate_quantity(uint16_t quantity, uint16_t max_quantity) {
    if(quantity == 0 || quantity > max_quantity) { return PTK_ERR_INVALID_PARAM; }
    return PTK_OK;
}

//=============================================================================
// PDU BASE INITIALIZATION
//=============================================================================

static ptk_err modbus_pdu_base_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    // PDU base doesn't serialize anything - this is handled by derived types
    (void)buf;
    (void)obj;
    return PTK_OK;
}

static ptk_err modbus_pdu_base_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    // PDU base doesn't deserialize anything - this is handled by derived types
    (void)buf;
    (void)obj;
    return PTK_OK;
}

void modbus_pdu_base_init(modbus_pdu_base_t *base, size_t pdu_type) {
    if(!base) { return; }

    base->buf_base.serialize = modbus_pdu_base_serialize;
    base->buf_base.deserialize = modbus_pdu_base_deserialize;
    base->pdu_type = pdu_type;
}
