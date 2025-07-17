#include <modbus.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_sock.h>
#include <ptk_mem.h>
#include <ptk_buf.h>
#include <ptk_array.h>
#include <string.h>

//=============================================================================
// MODBUS TCP HEADER CONSTANTS
//=============================================================================

#define MODBUS_TCP_HEADER_SIZE 6
#define MODBUS_TCP_PROTOCOL_ID 0x0000
#define MODBUS_MAX_PDU_SIZE 253
#define MODBUS_TCP_MAX_ADU_SIZE (MODBUS_TCP_HEADER_SIZE + MODBUS_MAX_PDU_SIZE)

//=============================================================================
// PRIVATE FUNCTIONS
//=============================================================================

extern struct modbus_connection_t {
    ptk_sock *socket;
    ptk_address_t addr;
    uint8_t unit_id;
    bool is_server;
    uint16_t transaction_id;
};

/**
 * @brief Get next transaction ID for a connection
 *
 * @param conn Modbus connection
 * @return Next transaction ID
 */
static uint16_t get_next_transaction_id(modbus_connection_t *conn) {
    if (!conn->is_server) {
        return ++conn->transaction_id;
    }
    return 0;
}

/**
 * @brief Generic PDU destructor
 *
 * @param ptr Pointer to PDU to destroy
 */
static void modbus_pdu_destructor(void *ptr) {
    debug("destroying modbus PDU");
    modbus_pdu_base_t *pdu = (modbus_pdu_base_t *)ptr;
    if (!pdu) return;

    switch (pdu->pdu_type) {
        case MODBUS_READ_COILS_RESP_TYPE: {
            modbus_read_coils_resp_t *resp = (modbus_read_coils_resp_t *)pdu;
            if (resp->coil_status) {
                ptk_free(&resp->coil_status);
            }
            break;
        }
        case MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE: {
            modbus_read_discrete_inputs_resp_t *resp = (modbus_read_discrete_inputs_resp_t *)pdu;
            if (resp->input_status) {
                ptk_free(&resp->input_status);
            }
            break;
        }
        case MODBUS_READ_HOLDING_REGISTERS_RESP_TYPE: {
            modbus_read_holding_registers_resp_t *resp = (modbus_read_holding_registers_resp_t *)pdu;
            if (resp->register_values) {
                ptk_free(&resp->register_values);
            }
            break;
        }
        case MODBUS_READ_INPUT_REGISTERS_RESP_TYPE: {
            modbus_read_input_registers_resp_t *resp = (modbus_read_input_registers_resp_t *)pdu;
            if (resp->register_values) {
                ptk_free(&resp->register_values);
            }
            break;
        }
        case MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE: {
            modbus_write_multiple_coils_req_t *req = (modbus_write_multiple_coils_req_t *)pdu;
            if (req->output_values) {
                ptk_free(&req->output_values);
            }
            break;
        }
        case MODBUS_WRITE_MULTIPLE_REGISTERS_REQ_TYPE: {
            modbus_write_multiple_registers_req_t *req = (modbus_write_multiple_registers_req_t *)pdu;
            if (req->register_values) {
                ptk_free(&req->register_values);
            }
            break;
        }
    }
}

/**
 * @brief Serialize a PDU to buffer
 *
 * @param pdu PDU to serialize
 * @param buf Buffer to write to
 * @return PTK_OK on success, error code on failure
 */
static ptk_err_t modbus_pdu_serialize(modbus_pdu_base_t *pdu, ptk_buf *buf) {
    if (!pdu || !buf) {
        warn("null parameters for PDU serialize");
        return PTK_ERR_INVALID_PARAM;
    }

    debug("serializing PDU type %zu", pdu->pdu_type);

    switch (pdu->pdu_type) {
        case MODBUS_READ_COILS_REQ_TYPE: {
            modbus_read_coils_req_t *req = (modbus_read_coils_req_t *)pdu;
            return ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 3,
                PTK_BUF_TYPE_U8, req->function_code,
                PTK_BUF_TYPE_U16, req->starting_address,
                PTK_BUF_TYPE_U16, req->quantity_of_coils);
        }
        case MODBUS_READ_COILS_RESP_TYPE: {
            modbus_read_coils_resp_t *resp = (modbus_read_coils_resp_t *)pdu;
            uint8_t byte_count = (uint8_t)((modbus_bit_array_len(resp->coil_status) + 7) / 8);
            ptk_err_t err = ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 2,
                PTK_BUF_TYPE_U8, resp->function_code,
                PTK_BUF_TYPE_U8, byte_count);
            if (err != PTK_OK) return err;
            
            uint8_t *bytes;
            size_t byte_len;
            err = modbus_bit_array_to_bytes(resp->coil_status, &bytes, &byte_len);
            if (err != PTK_OK) return err;
            
            for (size_t i = 0; i < byte_len; i++) {
                err = ptk_buf_set_u8(buf, bytes[i]);
                if (err != PTK_OK) {
                    ptk_free(&bytes);
                    return err;
                }
            }
            ptk_free(&bytes);
            return PTK_OK;
        }
        case MODBUS_READ_HOLDING_REGISTERS_REQ_TYPE: {
            modbus_read_holding_registers_req_t *req = (modbus_read_holding_registers_req_t *)pdu;
            return ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 3,
                PTK_BUF_TYPE_U8, req->function_code,
                PTK_BUF_TYPE_U16, req->starting_address,
                PTK_BUF_TYPE_U16, req->quantity_of_registers);
        }
        case MODBUS_WRITE_SINGLE_COIL_REQ_TYPE: {
            modbus_write_single_coil_req_t *req = (modbus_write_single_coil_req_t *)pdu;
            return ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 3,
                PTK_BUF_TYPE_U8, req->function_code,
                PTK_BUF_TYPE_U16, req->output_address,
                PTK_BUF_TYPE_U16, req->output_value);
        }
        case MODBUS_WRITE_SINGLE_REGISTER_REQ_TYPE: {
            modbus_write_single_register_req_t *req = (modbus_write_single_register_req_t *)pdu;
            return ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 3,
                PTK_BUF_TYPE_U8, req->function_code,
                PTK_BUF_TYPE_U16, req->register_address,
                PTK_BUF_TYPE_U16, req->register_value);
        }
        case MODBUS_EXCEPTION_RESP_TYPE: {
            modbus_exception_resp_t *resp = (modbus_exception_resp_t *)pdu;
            return ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 2,
                PTK_BUF_TYPE_U8, resp->exception_function_code,
                PTK_BUF_TYPE_U8, resp->exception_code);
        }
        default:
            warn("unsupported PDU type %zu for serialization", pdu->pdu_type);
            return PTK_ERR_UNSUPPORTED;
    }
}

/**
 * @brief Deserialize a PDU from buffer
 *
 * @param buf Buffer to read from
 * @param conn Connection context
 * @return Valid PDU on success, NULL on failure
 */
static modbus_pdu_base_t *modbus_pdu_deserialize(ptk_buf *buf, modbus_connection_t *conn) {
    if (!buf || !conn) {
        warn("null parameters for PDU deserialize");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    uint8_t function_code;
    ptk_err_t err = ptk_buf_deserialize_impl(buf, true, PTK_BUF_BIG_ENDIAN, 1, 
        PTK_BUF_TYPE_U8, &function_code);
    if (err != PTK_OK) {
        warn("failed to peek function code");
        ptk_set_err(err);
        return NULL;
    }

    debug("deserializing PDU with function code 0x%02x", function_code);

    if (function_code & 0x80) {
        modbus_exception_resp_t *resp = ptk_alloc(sizeof(modbus_exception_resp_t), modbus_pdu_destructor);
        if (!resp) {
            error("failed to allocate exception response");
            return NULL;
        }
        
        resp->base.conn = conn;
        resp->base.pdu_type = MODBUS_EXCEPTION_RESP_TYPE;
        
        err = ptk_buf_deserialize_impl(buf, false, PTK_BUF_BIG_ENDIAN, 2,
            PTK_BUF_TYPE_U8, &resp->exception_function_code,
            PTK_BUF_TYPE_U8, &resp->exception_code);
        
        if (err != PTK_OK) {
            ptk_free(&resp);
            warn("failed to deserialize exception response");
            ptk_set_err(err);
            return NULL;
        }
        
        return (modbus_pdu_base_t *)resp;
    }

    switch (function_code) {
        case MODBUS_FC_READ_COILS: {
            if (ptk_buf_get_len(buf) >= 5) {
                modbus_read_coils_req_t *req = ptk_alloc(sizeof(modbus_read_coils_req_t), modbus_pdu_destructor);
                if (!req) {
                    error("failed to allocate read coils request");
                    return NULL;
                }
                
                req->base.conn = conn;
                req->base.pdu_type = MODBUS_READ_COILS_REQ_TYPE;
                
                err = ptk_buf_deserialize_impl(buf, false, PTK_BUF_BIG_ENDIAN, 3,
                    PTK_BUF_TYPE_U8, &req->function_code,
                    PTK_BUF_TYPE_U16, &req->starting_address,
                    PTK_BUF_TYPE_U16, &req->quantity_of_coils);
                
                if (err != PTK_OK) {
                    ptk_free(&req);
                    warn("failed to deserialize read coils request");
                    ptk_set_err(err);
                    return NULL;
                }
                
                return (modbus_pdu_base_t *)req;
            } else {
                modbus_read_coils_resp_t *resp = ptk_alloc(sizeof(modbus_read_coils_resp_t), modbus_pdu_destructor);
                if (!resp) {
                    error("failed to allocate read coils response");
                    return NULL;
                }
                
                resp->base.conn = conn;
                resp->base.pdu_type = MODBUS_READ_COILS_RESP_TYPE;
                
                uint8_t byte_count;
                err = ptk_buf_deserialize_impl(buf, false, PTK_BUF_BIG_ENDIAN, 2,
                    PTK_BUF_TYPE_U8, &resp->function_code,
                    PTK_BUF_TYPE_U8, &byte_count);
                
                if (err != PTK_OK) {
                    ptk_free(&resp);
                    warn("failed to deserialize read coils response header");
                    ptk_set_err(err);
                    return NULL;
                }
                
                uint8_t *bytes = ptk_alloc(byte_count, NULL);
                if (!bytes) {
                    ptk_free(&resp);
                    error("failed to allocate coil data buffer");
                    return NULL;
                }
                
                for (uint8_t i = 0; i < byte_count; i++) {
                    bytes[i] = ptk_buf_get_u8(buf);
                    if (ptk_get_err() != PTK_OK) {
                        ptk_free(&bytes);
                        ptk_free(&resp);
                        warn("failed to read coil data");
                        return NULL;
                    }
                }
                
                size_t num_bits = byte_count * 8;
                err = modbus_bit_array_from_bytes(bytes, num_bits, &resp->coil_status);
                ptk_free(&bytes);
                
                if (err != PTK_OK) {
                    ptk_free(&resp);
                    warn("failed to create coil bit array");
                    ptk_set_err(err);
                    return NULL;
                }
                
                return (modbus_pdu_base_t *)resp;
            }
        }
        default:
            warn("unsupported function code 0x%02x", function_code);
            ptk_set_err(PTK_ERR_UNSUPPORTED);
            return NULL;
    }
}

/**
 * @brief Create a read coils response with initialized bit array
 *
 * @param conn Connection context
 * @param num_coils Number of coils to allocate
 * @return Valid response PDU on success, NULL on failure
 */
static modbus_read_coils_resp_t *create_read_coils_response(modbus_connection_t *conn, size_t num_coils) {
    modbus_read_coils_resp_t *resp = ptk_alloc(sizeof(modbus_read_coils_resp_t), modbus_pdu_destructor);
    if (!resp) {
        error("failed to allocate read coils response");
        return NULL;
    }
    
    resp->base.conn = conn;
    resp->base.pdu_type = MODBUS_READ_COILS_RESP_TYPE;
    resp->function_code = MODBUS_FC_READ_COILS;
    
    resp->coil_status = modbus_bit_array_create(num_coils > 0 ? num_coils : 1);
    if (!resp->coil_status) {
        ptk_free(&resp);
        error("failed to create coil status bit array");
        return NULL;
    }
    
    return resp;
}

/**
 * @brief Create a read discrete inputs response with initialized bit array
 *
 * @param conn Connection context
 * @param num_inputs Number of inputs to allocate
 * @return Valid response PDU on success, NULL on failure
 */
static modbus_read_discrete_inputs_resp_t *create_read_discrete_inputs_response(modbus_connection_t *conn, size_t num_inputs) {
    modbus_read_discrete_inputs_resp_t *resp = ptk_alloc(sizeof(modbus_read_discrete_inputs_resp_t), modbus_pdu_destructor);
    if (!resp) {
        error("failed to allocate read discrete inputs response");
        return NULL;
    }
    
    resp->base.conn = conn;
    resp->base.pdu_type = MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE;
    resp->function_code = MODBUS_FC_READ_DISCRETE_INPUTS;
    
    resp->input_status = modbus_bit_array_create(num_inputs > 0 ? num_inputs : 1);
    if (!resp->input_status) {
        ptk_free(&resp);
        error("failed to create input status bit array");
        return NULL;
    }
    
    return resp;
}

/**
 * @brief Create a write multiple coils request with initialized bit array
 *
 * @param conn Connection context
 * @param num_coils Number of coils to allocate
 * @return Valid request PDU on success, NULL on failure
 */
static modbus_write_multiple_coils_req_t *create_write_multiple_coils_request(modbus_connection_t *conn, size_t num_coils) {
    modbus_write_multiple_coils_req_t *req = ptk_alloc(sizeof(modbus_write_multiple_coils_req_t), modbus_pdu_destructor);
    if (!req) {
        error("failed to allocate write multiple coils request");
        return NULL;
    }
    
    req->base.conn = conn;
    req->base.pdu_type = MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE;
    req->function_code = MODBUS_FC_WRITE_MULTIPLE_COILS;
    
    req->output_values = modbus_bit_array_create(num_coils > 0 ? num_coils : 1);
    if (!req->output_values) {
        ptk_free(&req);
        error("failed to create output values bit array");
        return NULL;
    }
    
    return req;
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

/**
 * @brief Send a Modbus PDU
 *
 * @param pdu Pointer to PDU to send (will be freed and nulled)
 * @param timeout_ms Timeout in milliseconds
 * @return Response PDU for requests, NULL for responses or errors
 */
modbus_pdu_base_t *modbus_pdu_send(modbus_pdu_base_t **pdu, ptk_duration_ms timeout_ms) {
    info("sending modbus PDU");
    
    if (!pdu || !*pdu) {
        warn("null PDU pointer");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    modbus_pdu_base_t *pdu_to_send = *pdu;
    modbus_connection_t *conn = pdu_to_send->conn;
    
    if (!conn) {
        warn("PDU has no associated connection");
        ptk_free(pdu);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    // Determine if this is a request (expects response) or response (no response expected)
    bool is_request = !MODBUS_PDU_IS_RESPONSE(pdu_to_send->pdu_type);

    ptk_buf *buf = ptk_buf_alloc(MODBUS_TCP_MAX_ADU_SIZE);
    if (!buf) {
        error("failed to allocate send buffer");
        ptk_free(pdu);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }

    uint16_t transaction_id = get_next_transaction_id(conn);
    uint16_t protocol_id = MODBUS_TCP_PROTOCOL_ID;
    uint16_t length = 1 + MODBUS_MAX_PDU_SIZE;
    uint8_t unit_id = conn->unit_id;

    ptk_err_t err = ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 4,
        PTK_BUF_TYPE_U16, transaction_id,
        PTK_BUF_TYPE_U16, protocol_id,
        PTK_BUF_TYPE_U16, length,
        PTK_BUF_TYPE_U8, unit_id);

    if (err != PTK_OK) {
        ptk_free(&buf);
        ptk_free(pdu);
        warn("failed to serialize TCP header");
        ptk_set_err(err);
        return NULL;
    }

    ptk_buf_size_t header_end = ptk_buf_get_end(buf);
    
    err = modbus_pdu_serialize(pdu_to_send, buf);
    if (err != PTK_OK) {
        ptk_free(&buf);
        ptk_free(pdu);
        warn("failed to serialize PDU");
        ptk_set_err(err);
        return NULL;
    }

    ptk_buf_size_t total_len = ptk_buf_get_end(buf);
    ptk_buf_size_t pdu_len = total_len - header_end;
    uint16_t actual_length = pdu_len + 1;

    ptk_buf_set_end(buf, header_end - 3);
    err = ptk_buf_serialize_impl(buf, PTK_BUF_BIG_ENDIAN, 1,
        PTK_BUF_TYPE_U16, actual_length);
    
    if (err != PTK_OK) {
        ptk_free(&buf);
        ptk_free(pdu);
        warn("failed to update length field");
        ptk_set_err(err);
        return NULL;
    }

    ptk_buf_set_end(buf, total_len);
    ptk_buf_set_start(buf, 0);

    // Wrap buf in a ptk_buf_array_t for sending
    ptk_buf_array_t *send_array = ptk_buf_array_create(1, NULL);
    ptk_buf_array_set(send_array, 0, *buf);
    err = ptk_tcp_socket_send(conn->socket, send_array, timeout_ms);
    ptk_free(&send_array);
    ptk_free(&buf);

    if (err != PTK_OK) {
        ptk_free(pdu);
        warn("failed to send TCP data");
        ptk_set_err(err);
        return NULL;
    }

    debug("successfully sent modbus PDU");
    
    // Free the sent PDU
    ptk_free(pdu);
    
    // If this was a response, we're done
    if (!is_request) {
        ptk_set_err(PTK_OK);
        return NULL;
    }
    
    // For requests, wait for and return the response
    modbus_pdu_u response = modbus_pdu_recv(conn, timeout_ms);
    if (!response.base) {
        warn("failed to receive response PDU");
        return NULL;
    }
    
    debug("successfully received response PDU");
    ptk_set_err(PTK_OK);
    return response.base;
}

/**
 * @brief Receive a Modbus PDU from a connection
 *
 * @param conn Connection to receive from
 * @param timeout_ms Timeout in milliseconds
 * @return Valid PDU union on success, zeroed union on failure (check ptk_get_err())
 */
modbus_pdu_u modbus_pdu_recv(modbus_connection_t *conn, ptk_duration_ms timeout_ms) {
    info("receiving modbus PDU");
    modbus_pdu_u result = {0};
    
    if (!conn) {
        warn("null connection");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return result;
    }

    ptk_buf *buf = ptk_buf_alloc(MODBUS_TCP_MAX_ADU_SIZE);
    if (!buf) {
        error("failed to allocate receive buffer");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return result;
    }

    ptk_buf *recv_buf = ptk_tcp_socket_recv(conn->socket, false, timeout_ms);
    ptk_err_t err = (recv_buf != NULL) ? PTK_OK : ptk_get_err();
    if (recv_buf) {
        // Copy received data into buf
        memcpy(buf->data, recv_buf->data, recv_buf->end);
        buf->end = recv_buf->end;
        ptk_free(&recv_buf);
    }
    if (err != PTK_OK) {
        ptk_free(&buf);
        warn("failed to receive TCP data");
        ptk_set_err(err);
        return result;
    }

    if (ptk_buf_get_len(buf) < MODBUS_TCP_HEADER_SIZE) {
        ptk_free(&buf);
        warn("received data too short for TCP header");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return result;
    }

    uint16_t transaction_id, protocol_id, length;
    uint8_t unit_id;
    
    err = ptk_buf_deserialize_impl(buf, false, PTK_BUF_BIG_ENDIAN, 4,
        PTK_BUF_TYPE_U16, &transaction_id,
        PTK_BUF_TYPE_U16, &protocol_id,
        PTK_BUF_TYPE_U16, &length,
        PTK_BUF_TYPE_U8, &unit_id);

    if (err != PTK_OK) {
        ptk_free(&buf);
        warn("failed to deserialize TCP header");
        ptk_set_err(err);
        return result;
    }

    if (protocol_id != MODBUS_TCP_PROTOCOL_ID) {
        ptk_free(&buf);
        warn("invalid protocol ID: 0x%04x", protocol_id);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return result;
    }

    debug("received transaction_id=%u, length=%u, unit_id=%u", 
          transaction_id, length, unit_id);

    modbus_pdu_base_t *pdu = modbus_pdu_deserialize(buf, conn);
    ptk_free(&buf);

    if (!pdu) {
        warn("failed to deserialize PDU");
        return result;
    }

    result.base = pdu;
    debug("successfully received modbus PDU");
    return result;
}

/**
 * @brief Create a PDU from a type identifier
 *
 * @param conn Connection context
 * @param pdu_type PDU type identifier
 * @return Valid PDU on success, NULL on failure (check ptk_get_err())
 */
modbus_pdu_base_t *modbus_pdu_create_from_type(modbus_connection_t *conn, size_t pdu_type) {
    info("creating PDU of type %zu", pdu_type);
    
    if (!conn) {
        warn("null connection");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    switch (pdu_type) {
        case MODBUS_READ_COILS_REQ_TYPE: {
            modbus_read_coils_req_t *req = ptk_alloc(sizeof(modbus_read_coils_req_t), modbus_pdu_destructor);
            if (!req) {
                error("failed to allocate read coils request");
                return NULL;
            }
            req->base.conn = conn;
            req->base.pdu_type = pdu_type;
            req->function_code = MODBUS_FC_READ_COILS;
            return (modbus_pdu_base_t *)req;
        }
        case MODBUS_WRITE_SINGLE_COIL_REQ_TYPE: {
            modbus_write_single_coil_req_t *req = ptk_alloc(sizeof(modbus_write_single_coil_req_t), modbus_pdu_destructor);
            if (!req) {
                error("failed to allocate write single coil request");
                return NULL;
            }
            req->base.conn = conn;
            req->base.pdu_type = pdu_type;
            req->function_code = MODBUS_FC_WRITE_SINGLE_COIL;
            return (modbus_pdu_base_t *)req;
        }
        case MODBUS_WRITE_SINGLE_REGISTER_REQ_TYPE: {
            modbus_write_single_register_req_t *req = ptk_alloc(sizeof(modbus_write_single_register_req_t), modbus_pdu_destructor);
            if (!req) {
                error("failed to allocate write single register request");
                return NULL;
            }
            req->base.conn = conn;
            req->base.pdu_type = pdu_type;
            req->function_code = MODBUS_FC_WRITE_SINGLE_REGISTER;
            return (modbus_pdu_base_t *)req;
        }
        case MODBUS_READ_COILS_RESP_TYPE: {
            modbus_read_coils_resp_t *resp = create_read_coils_response(conn, 1);
            return (modbus_pdu_base_t *)resp;
        }
        case MODBUS_READ_DISCRETE_INPUTS_RESP_TYPE: {
            modbus_read_discrete_inputs_resp_t *resp = create_read_discrete_inputs_response(conn, 1);
            return (modbus_pdu_base_t *)resp;
        }
        case MODBUS_WRITE_MULTIPLE_COILS_REQ_TYPE: {
            modbus_write_multiple_coils_req_t *req = create_write_multiple_coils_request(conn, 1);
            return (modbus_pdu_base_t *)req;
        }
        default:
            warn("unsupported PDU type %zu", pdu_type);
            ptk_set_err(PTK_ERR_UNSUPPORTED);
            return NULL;
    }
}