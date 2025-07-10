#ifndef MODBUS_INTERNAL_H
#define MODBUS_INTERNAL_H

#include "modbus.h"

//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Initialize the base PDU structure
 * @param base Base PDU structure to initialize
 * @param pdu_type Type identifier for this PDU
 * @param serialize_fn Serialization function pointer
 * @param deserialize_fn Deserialization function pointer
 */
void modbus_pdu_base_init(modbus_pdu_base_t *base,
                         size_t pdu_type,
                         ptk_err (*serialize_fn)(ptk_buf *buf, ptk_serializable_t *obj),
                         ptk_err (*deserialize_fn)(ptk_buf *buf, ptk_serializable_t *obj));

/**
 * @brief Dispatch PDU deserialization based on function code
 * @param buf Buffer to deserialize from (uses peek to examine function code)
 * @param mbap MBAP header to populate with the correct PDU
 * @return PTK_OK on success, error code on failure
 */
ptk_err modbus_dispatch_pdu_deserializer(ptk_buf *buf, modbus_mbap_t *mbap);

/**
 * @brief Get PDU type constant from function code and request/response flag
 * @param function_code Modbus function code
 * @param is_request True for request PDUs, false for responses
 * @return PDU type constant, or 0 if invalid
 */
size_t modbus_get_pdu_type_from_function_code(uint8_t function_code, bool is_request);

/**
 * @brief Validate Modbus address and quantity ranges
 * @param address Starting address
 * @param quantity Number of items
 * @param max_address Maximum valid address
 * @param max_quantity Maximum valid quantity
 * @return PTK_OK if valid, PTK_ERR_INVALID_PARAM if invalid
 */
ptk_err modbus_validate_request_params(uint16_t address, uint16_t quantity,
                                      uint16_t max_address, uint16_t max_quantity);

/**
 * @brief Convert boolean value to Modbus coil format (0x0000 or 0xFF00)
 * @param value Boolean value
 * @return Modbus coil value
 */
uint16_t modbus_bool_to_coil_value(bool value);

/**
 * @brief Convert Modbus coil format to boolean
 * @param coil_value Modbus coil value
 * @return Boolean value (true for 0xFF00, false for 0x0000)
 */
bool modbus_coil_value_to_bool(uint16_t coil_value);

#endif // MODBUS_INTERNAL_H
