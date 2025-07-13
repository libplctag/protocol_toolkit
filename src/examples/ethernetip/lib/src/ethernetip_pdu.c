#include "../include/ethernetip.h"
#include <ptk_alloc.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_utils.h>
#include <string.h>

//=============================================================================
// INTERNAL DECLARATIONS
//=============================================================================

// Forward declarations for internal functions
extern ptk_sock *eip_connection_get_socket(eip_connection_t *conn);
extern ptk_buf *eip_connection_get_buffer(eip_connection_t *conn);
extern uint32_t eip_connection_get_session_handle(eip_connection_t *conn);
extern uint64_t eip_connection_get_next_sender_context(eip_connection_t *conn);
extern bool eip_connection_is_udp(eip_connection_t *conn);
extern bool eip_connection_is_session_registered(eip_connection_t *conn);

//=============================================================================
// PROTOCOL CONSTANTS
//=============================================================================

#define EIP_LIST_IDENTITY_CMD 0x0063
#define EIP_UNCONNECTED_SEND_CMD 0x006F
#define EIP_HEADER_SIZE 24

// CPF Item Types
#define CPF_TYPE_NULL 0x0000
#define CPF_TYPE_CIP_IDENTITY 0x000C
#define CPF_TYPE_UNCONNECTED_DATA 0x00B2

//=============================================================================
// INTERNAL PDU STRUCTURES
//=============================================================================

typedef struct {
    uint16_t command;         // EIP command
    uint16_t length;          // Length of data following header
    uint32_t session_handle;  // Session identifier
    uint32_t status;          // Status code
    uint64_t sender_context;  // Context data
    uint32_t options;         // Options flags
} eip_encap_header_t;

//=============================================================================
// PDU BASE OPERATIONS
//=============================================================================

static ptk_err eip_pdu_base_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    eip_pdu_base_t *pdu = (eip_pdu_base_t *)obj;
    
    // EIP PDUs serialize themselves based on their specific type
    switch (pdu->pdu_type) {
        case EIP_LIST_IDENTITY_REQ_TYPE:
            return eip_list_identity_req_serialize(buf, (eip_list_identity_req_t *)pdu);
            
        case EIP_LIST_IDENTITY_RESP_TYPE:
            return eip_list_identity_resp_serialize(buf, (eip_list_identity_resp_t *)pdu);
            
        default:
            return PTK_ERR_INVALID_ARGUMENT;
    }
}

static ptk_err eip_pdu_base_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    // Deserialization is handled by specific PDU parsers
    return PTK_ERR_NOT_IMPLEMENTED;
}

static void eip_pdu_destructor(void *ptr) {
    eip_pdu_base_t *pdu = (eip_pdu_base_t *)ptr;
    if (pdu && pdu->conn) {
        // Don't free the connection - it's owned by the caller
        pdu->conn = NULL;
    }
}

//=============================================================================
// LIST IDENTITY REQUEST PDU
//=============================================================================

//=============================================================================
// LIST IDENTITY RESPONSE PDU
//=============================================================================

static ptk_err parse_cip_identity_item(ptk_buf *buffer, eip_list_identity_resp_t *resp, uint16_t item_length) {
    if (!buffer || !resp || item_length < 34) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Parse encapsulation version
    uint16_t encap_version;
    ptk_err err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &encap_version);
    if (err != PTK_OK) return err;
    
    // Parse socket address (big-endian for network fields)
    int16_t sin_family;
    uint16_t sin_port;
    err = ptk_buf_deserialize(buffer, false, PTK_BUF_BIG_ENDIAN, &sin_family, &sin_port);
    if (err != PTK_OK) return err;
    
    resp->port = sin_port;
    
    // Parse IP address bytes
    uint8_t ip_bytes[4];
    for (int i = 0; i < 4; i++) {
        err = ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &ip_bytes[i]);
        if (err != PTK_OK) return err;
    }
    
    // Convert IP to string
    snprintf(resp->ip_address, sizeof(resp->ip_address), "%u.%u.%u.%u",
             ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    
    // Skip sin_zero padding (8 bytes)
    for (int i = 0; i < 8; i++) {
        uint8_t padding;
        err = ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &padding);
        if (err != PTK_OK) return err;
    }
    
    // Parse device identity fields (little-endian)
    err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN,
                             &resp->vendor_id,
                             &resp->device_type,
                             &resp->product_code,
                             &resp->revision_major,
                             &resp->revision_minor);
    if (err != PTK_OK) return err;
    
    // Calculate remaining bytes for optional fields
    size_t parsed_bytes = 2 + 2 + 2 + 4 + 8 + 2 + 2 + 2 + 1 + 1; // 26 bytes
    size_t remaining_bytes = item_length - parsed_bytes;
    
    // Parse status and serial number if present
    if (remaining_bytes >= 6) {
        uint16_t status_word;
        err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN,
                                 &status_word,
                                 &resp->serial_number);
        if (err != PTK_OK) return err;
        
        // Parse status bits into app-friendly bools
        resp->owned = (status_word & 0x0001) != 0;
        resp->configured = ((status_word & 0x000C) >> 2) != 0;
        resp->minor_recoverable_fault = (status_word & 0x0100) != 0;
        resp->minor_unrecoverable_fault = (status_word & 0x0200) != 0;
        resp->major_recoverable_fault = (status_word & 0x0400) != 0;
        resp->major_unrecoverable_fault = (status_word & 0x0800) != 0;
        
        remaining_bytes -= 6;
    }
    
    // Parse product name (SHORT_STRING) if present
    if (remaining_bytes > 0) {
        uint8_t name_length;
        err = ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &name_length);
        if (err == PTK_OK && name_length > 0 && name_length < remaining_bytes && name_length < 63) {
            for (uint8_t i = 0; i < name_length; i++) {
                uint8_t c;
                err = ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &c);
                if (err != PTK_OK) break;
                
                // Validate printable ASCII
                if (c >= 0x20 && c <= 0x7E) {
                    resp->product_name[i] = (char)c;
                } else {
                    resp->product_name[i] = '?';
                }
            }
            resp->product_name[name_length] = '\0';
            remaining_bytes -= (name_length + 1);
        }
    }
    
    // Parse device state if present
    if (remaining_bytes >= 1) {
        uint8_t state;
        err = ptk_buf_deserialize(buffer, false, PTK_BUF_NATIVE_ENDIAN, &state);
        if (err == PTK_OK) {
            // Parse state into app-friendly bools
            resp->self_test_in_progress = (state == 0x01);
            resp->standby = (state == 0x02);
            resp->operational = (state == 0x03);
            resp->major_fault = (state == 0x04);
            resp->configuration_mode = (state == 0x05);
            resp->waiting_for_reset = (state == 0x06);
        }
    }
    
    // Set discovery timestamp
    resp->discovery_timestamp_ms = ptk_now_ms();
    
    return PTK_OK;
}

ptk_err eip_list_identity_resp_deserialize(ptk_buf *buf, eip_list_identity_resp_t *resp) {
    if (!buf || !resp) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Parse EIP header
    eip_encap_header_t header;
    ptk_err err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN,
                                     &header.command,
                                     &header.length,
                                     &header.session_handle,
                                     &header.status,
                                     &header.sender_context,
                                     &header.options);
    if (err != PTK_OK) return err;
    
    if (header.command != EIP_LIST_IDENTITY_CMD || header.status != 0) {
        return PTK_ERR_PROTOCOL_ERROR;
    }
    
    // Parse CPF data if present
    if (header.length > 0) {
        uint16_t item_count;
        err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &item_count);
        if (err != PTK_OK) return err;
        
        // Look for CIP Identity item
        for (uint16_t i = 0; i < item_count; i++) {
            uint16_t type_id, item_length;
            err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &type_id, &item_length);
            if (err != PTK_OK) return err;
            
            if (type_id == CPF_TYPE_CIP_IDENTITY && item_length >= 34) {
                return parse_cip_identity_item(buf, resp, item_length);
            } else {
                // Skip unknown item
                size_t current_pos = ptk_buf_get_start(buf);
                ptk_buf_set_start(buf, current_pos + item_length);
            }
        }
    }
    
    return PTK_ERR_PROTOCOL_ERROR; // No identity item found
}

ptk_err eip_list_identity_resp_serialize(ptk_buf *buf, eip_list_identity_resp_t *resp) {
    // Response serialization is typically not needed for client implementations
    return PTK_ERR_NOT_IMPLEMENTED;
}

//=============================================================================
// PDU FACTORY FUNCTIONS
//=============================================================================

eip_pdu_base_t *eip_pdu_create_from_type(eip_connection_t *conn, size_t pdu_type) {
    if (!conn) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    eip_pdu_base_t *pdu = NULL;
    
    switch (pdu_type) {
        case EIP_LIST_IDENTITY_REQ_TYPE: {
            eip_list_identity_req_t *req = ptk_alloc(sizeof(eip_list_identity_req_t), eip_pdu_destructor);
            if (!req) {
                ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
                return NULL;
            }
            
            memset(req, 0, sizeof(*req));
            req->base.base.serialize = eip_pdu_base_serialize;
            req->base.base.deserialize = eip_pdu_base_deserialize;
            req->base.pdu_type = pdu_type;
            req->base.conn = conn;
            req->response_time_range_ms = 500; // Default 500ms response window
            
            pdu = &req->base;
            break;
        }
        
        case EIP_LIST_IDENTITY_RESP_TYPE: {
            eip_list_identity_resp_t *resp = ptk_alloc(sizeof(eip_list_identity_resp_t), eip_pdu_destructor);
            if (!resp) {
                ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
                return NULL;
            }
            
            memset(resp, 0, sizeof(*resp));
            resp->base.base.serialize = eip_pdu_base_serialize;
            resp->base.base.deserialize = eip_pdu_base_deserialize;
            resp->base.pdu_type = pdu_type;
            resp->base.conn = conn;
            
            pdu = &resp->base;
            break;
        }
        
        default:
            ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
            return NULL;
    }
    
    return pdu;
}

//=============================================================================
// PDU SEND/RECEIVE FUNCTIONS
//=============================================================================

eip_pdu_u eip_pdu_recv(eip_connection_t *conn, ptk_duration_ms timeout_ms) {
    eip_pdu_u result = {0};
    
    if (!conn) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return result;
    }
    
    ptk_sock *socket = eip_connection_get_socket(conn);
    ptk_buf *buffer = eip_connection_get_buffer(conn);
    
    if (!socket || !buffer) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return result;
    }
    
    // Reset buffer for receiving
    ptk_buf_set_start(buffer, 0);
    ptk_buf_set_end(buffer, 0);
    
    ptk_err err;
    if (eip_connection_is_udp(conn)) {
        ptk_address_t sender_addr;
        err = ptk_udp_socket_recv_from(socket, buffer, &sender_addr, timeout_ms);
    } else {
        err = ptk_tcp_socket_recv(socket, buffer, timeout_ms);
    }
    
    if (err != PTK_OK) {
        ptk_set_err(err);
        return result;
    }
    
    // Peek at command to determine PDU type
    uint16_t command;
    size_t saved_start = ptk_buf_get_start(buffer);
    err = ptk_buf_deserialize(buffer, false, PTK_BUF_LITTLE_ENDIAN, &command);
    ptk_buf_set_start(buffer, saved_start); // Reset position
    
    if (err != PTK_OK) {
        ptk_set_err(err);
        return result;
    }
    
    // Create appropriate PDU based on command
    switch (command) {
        case EIP_LIST_IDENTITY_CMD: {
            eip_list_identity_resp_t *resp = (eip_list_identity_resp_t *)
                eip_pdu_create_from_type(conn, EIP_LIST_IDENTITY_RESP_TYPE);
            
            if (resp) {
                err = eip_list_identity_resp_deserialize(buffer, resp);
                if (err == PTK_OK) {
                    result.list_identity_resp = resp;
                    result.base = &resp->base;
                } else {
                    ptk_free(&resp);
                    ptk_set_err(err);
                }
            }
            break;
        }
        
        default:
            ptk_set_err(PTK_ERR_PROTOCOL_ERROR);
            break;
    }
    
    return result;
}

eip_pdu_base_t *eip_pdu_send(eip_pdu_base_t **pdu, ptk_duration_ms timeout_ms) {
    if (!pdu || !*pdu) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    eip_pdu_base_t *request = *pdu;
    eip_connection_t *conn = request->conn;
    
    if (!conn) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    ptk_sock *socket = eip_connection_get_socket(conn);
    ptk_buf *buffer = eip_connection_get_buffer(conn);
    
    if (!socket || !buffer) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    // Reset buffer for sending
    ptk_buf_set_start(buffer, 0);
    ptk_buf_set_end(buffer, 0);
    
    // Serialize the PDU
    ptk_err err = request->base.serialize(buffer, &request->base);
    if (err != PTK_OK) {
        ptk_set_err(err);
        ptk_free(pdu); // Free the request PDU
        return NULL;
    }
    
    // Send the PDU
    if (eip_connection_is_udp(conn)) {
        // For UDP (ListIdentity), send to broadcast or specific host
        ptk_address_t target_addr;
        err = ptk_address_create(&target_addr, "255.255.255.255", 44818);
        if (err == PTK_OK) {
            err = ptk_udp_socket_send_to(socket, buffer, &target_addr, true, timeout_ms);
        }
    } else {
        err = ptk_tcp_socket_send(socket, buffer, timeout_ms);
    }
    
    if (err != PTK_OK) {
        ptk_set_err(err);
        ptk_free(pdu); // Free the request PDU
        return NULL;
    }
    
    // Free the request PDU after sending
    ptk_free(pdu);
    
    // Check if this PDU type expects a response
    bool expects_response = !EIP_PDU_IS_RESPONSE(request->pdu_type);
    
    if (!expects_response) {
        return NULL; // No response expected
    }
    
    // Receive response
    eip_pdu_u response = eip_pdu_recv(conn, timeout_ms);
    if (!response.base) {
        return NULL; // Error already set by eip_pdu_recv
    }
    
    return response.base;
}