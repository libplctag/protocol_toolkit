#include "ethernetip_defs.h"
#include <stdlib.h>
#include <string.h>

//=============================================================================
// EIP ENCAPSULATION HEADER FUNCTIONS
//=============================================================================

static buf_err_t eip_encap_header_decode(eip_encap_header *header, buf *src) {
    return buf_decode(src, false, "< u16 u16 u32 u32 u64 u32", &header->command, &header->length, &header->session_handle,
                      &header->status, &header->sender_context, &header->options);
}

static buf_err_t eip_encap_header_encode(buf *dest, const eip_encap_header *header) {
    return buf_encode(dest, true, "< u16 u16 u32 u32 u64 u32", header->command, header->length, header->session_handle,
                      header->status, header->sender_context, header->options);
}

//=============================================================================
// CPF ITEM HEADER FUNCTIONS
//=============================================================================

static buf_err_t eip_cpf_item_header_decode(eip_cpf_item_header *header, buf *src) {
    return buf_decode(src, false, "< u16 u16", &header->type_id, &header->length);
}

static buf_err_t eip_cpf_item_header_encode(buf *dest, const eip_cpf_item_header *header) {
    return buf_encode(dest, true, "< u16 u16", header->type_id, header->length);
}

//=============================================================================
// LIST IDENTITY REQUEST FUNCTIONS
//=============================================================================

buf_err_t eip_list_identity_request_decode(eip_list_identity_request **header, buf *src) {
    trace("Decoding EIP List Identity Request");

    if(!header || !src) {
        error("NULL pointer passed to eip_list_identity_request_decode");
        return BUF_ERR_NULL_PTR;
    }

    *header = calloc(1, sizeof(eip_list_identity_request));
    if(!*header) {
        error("Failed to allocate memory for List Identity Request");
        return BUF_ERR_NO_RESOURCES;
    }

    buf_err_t result = eip_encap_header_decode(*header, src);
    if(result != BUF_OK) {
        error("Failed to decode encapsulation header");
        free(*header);
        *header = NULL;
        return result;
    }

    // Validate command
    if((*header)->command != EIP_LIST_IDENTITY) {
        error("Invalid command for List Identity Request: 0x%04X", (*header)->command);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    trace("Successfully decoded List Identity Request");
    return BUF_OK;
}

buf_err_t eip_list_identity_request_encode(buf *dest, eip_list_identity_request *header) {
    trace("Encoding EIP List Identity Request");

    if(!dest || !header) {
        error("NULL pointer passed to eip_list_identity_request_encode");
        return BUF_ERR_NULL_PTR;
    }

    // Set command and length for List Identity Request
    header->command = EIP_LIST_IDENTITY;
    header->length = 0;  // No data payload
    header->session_handle = 0;
    header->status = 0;
    header->sender_context = 0;
    header->options = 0;

    buf_err_t result = eip_encap_header_encode(dest, header);
    if(result != BUF_OK) {
        error("Failed to encode encapsulation header");
        return result;
    }

    trace("Successfully encoded List Identity Request");
    return BUF_OK;
}

void eip_list_identity_request_dispose(eip_list_identity_request *header) {
    if(header) { free(header); }
}

void eip_list_identity_request_log_impl(const char *function, int line, ev_log_level level,
                                        const eip_list_identity_request *header) {
    if(!header) {
        ev_log_impl(function, line, level, "List Identity Request: NULL");
        return;
    }

    ev_log_impl(function, line, level, "List Identity Request: cmd=0x%04X, len=%u, session=0x%08X, status=0x%08X",
                header->command, header->length, header->session_handle, header->status);
}

//=============================================================================
// NULL ADDRESS ITEM FUNCTIONS
//=============================================================================

buf_err_t eip_cpf_null_address_item_decode(eip_cpf_null_address_item **header, buf *src) {
    trace("Decoding CPF NULL Address Item");

    if(!header || !src) {
        error("NULL pointer passed to eip_cpf_null_address_item_decode");
        return BUF_ERR_NULL_PTR;
    }

    *header = calloc(1, sizeof(eip_cpf_null_address_item));
    if(!*header) {
        error("Failed to allocate memory for NULL Address Item");
        return BUF_ERR_NO_RESOURCES;
    }

    buf_err_t result = eip_cpf_item_header_decode(*header, src);
    if(result != BUF_OK) {
        error("Failed to decode CPF item header");
        free(*header);
        *header = NULL;
        return result;
    }

    // Validate type and length
    if((*header)->type_id != CPF_TYPE_ID_NULL_ADDRESS) {
        error("Invalid type ID for NULL Address Item: 0x%04X", (*header)->type_id);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    if((*header)->length != 0) {
        error("Invalid length for NULL Address Item: %u (expected 0)", (*header)->length);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    trace("Successfully decoded CPF NULL Address Item");
    return BUF_OK;
}

buf_err_t eip_cpf_null_address_item_encode(buf *dest, eip_cpf_null_address_item *header) {
    trace("Encoding CPF NULL Address Item");

    if(!dest || !header) {
        error("NULL pointer passed to eip_cpf_null_address_item_encode");
        return BUF_ERR_NULL_PTR;
    }

    header->type_id = CPF_TYPE_ID_NULL_ADDRESS;
    header->length = 0;

    buf_err_t result = eip_cpf_item_header_encode(dest, header);
    if(result != BUF_OK) {
        error("Failed to encode CPF item header");
        return result;
    }

    trace("Successfully encoded CPF NULL Address Item");
    return BUF_OK;
}

void eip_cpf_null_address_item_dispose(eip_cpf_null_address_item *header) {
    if(header) { free(header); }
}

void eip_cpf_null_address_item_log_impl(const char *function, int line, ev_log_level level,
                                        const eip_cpf_null_address_item *header) {
    if(!header) {
        ev_log_impl(function, line, level, "CPF NULL Address Item: NULL");
        return;
    }

    ev_log_impl(function, line, level, "CPF NULL Address Item: type_id=0x%04X, length=%u", header->type_id, header->length);
}

//=============================================================================
// CONNECTED ADDRESS ITEM FUNCTIONS
//=============================================================================

buf_err_t eip_cpf_connected_address_item_decode(eip_cpf_connected_address_item **header, buf *src) {
    trace("Decoding CPF Connected Address Item");

    if(!header || !src) {
        error("NULL pointer passed to eip_cpf_connected_address_item_decode");
        return BUF_ERR_NULL_PTR;
    }

    *header = calloc(1, sizeof(eip_cpf_connected_address_item));
    if(!*header) {
        error("Failed to allocate memory for Connected Address Item");
        return BUF_ERR_NO_RESOURCES;
    }

    buf_err_t result = eip_cpf_item_header_decode(&(*header)->header, src);
    if(result != BUF_OK) {
        error("Failed to decode CPF item header");
        free(*header);
        *header = NULL;
        return result;
    }

    // Validate type and length
    if((*header)->header.type_id != CPF_TYPE_ID_CONNECTED_ADDR) {
        error("Invalid type ID for Connected Address Item: 0x%04X", (*header)->header.type_id);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    if((*header)->header.length != 4) {
        error("Invalid length for Connected Address Item: %u (expected 4)", (*header)->header.length);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    // Decode connection ID
    result = buf_decode(src, false, "< u32", &(*header)->connection_id);
    if(result != BUF_OK) {
        error("Failed to decode connection ID");
        free(*header);
        *header = NULL;
        return result;
    }

    trace("Successfully decoded CPF Connected Address Item");
    return BUF_OK;
}

buf_err_t eip_cpf_connected_address_item_encode(buf *dest, eip_cpf_connected_address_item *header) {
    trace("Encoding CPF Connected Address Item");

    if(!dest || !header) {
        error("NULL pointer passed to eip_cpf_connected_address_item_encode");
        return BUF_ERR_NULL_PTR;
    }

    header->header.type_id = CPF_TYPE_ID_CONNECTED_ADDR;
    header->header.length = 4;

    buf_err_t result = eip_cpf_item_header_encode(dest, &header->header);
    if(result != BUF_OK) {
        error("Failed to encode CPF item header");
        return result;
    }

    result = buf_encode(dest, true, "< u32", header->connection_id);
    if(result != BUF_OK) {
        error("Failed to encode connection ID");
        return result;
    }

    trace("Successfully encoded CPF Connected Address Item");
    return BUF_OK;
}

void eip_cpf_connected_address_item_dispose(eip_cpf_connected_address_item *header) {
    if(header) { free(header); }
}

void eip_cpf_connected_address_item_log_impl(const char *function, int line, ev_log_level level,
                                             const eip_cpf_connected_address_item *header) {
    if(!header) {
        ev_log_impl(function, line, level, "CPF Connected Address Item: NULL");
        return;
    }

    ev_log_impl(function, line, level, "CPF Connected Address Item: type_id=0x%04X, length=%u, connection_id=0x%08X",
                header->header.type_id, header->header.length, header->connection_id);
}

//=============================================================================
// CIP IDENTITY ITEM FUNCTIONS
//=============================================================================

buf_err_t eip_cpf_cip_identity_item_decode(eip_cpf_cip_identity_item **header, buf *src) {
    trace("Decoding CPF CIP Identity Item");

    if(!header || !src) {
        error("NULL pointer passed to eip_cpf_cip_identity_item_decode");
        return BUF_ERR_NULL_PTR;
    }

    *header = calloc(1, sizeof(eip_cpf_cip_identity_item));
    if(!*header) {
        error("Failed to allocate memory for CIP Identity Item");
        return BUF_ERR_NO_RESOURCES;
    }

    buf_err_t result = eip_cpf_item_header_decode(&(*header)->header, src);
    if(result != BUF_OK) {
        error("Failed to decode CPF item header");
        free(*header);
        *header = NULL;
        return result;
    }

    // Validate type
    if((*header)->header.type_id != CPF_TYPE_ID_CIP_IDENTITY) {
        error("Invalid type ID for CIP Identity Item: 0x%04X", (*header)->header.type_id);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    // Decode fixed fields
    result = buf_decode(src, false, "< u16 u16 u16 u8 u8 u16 u32 u8", &(*header)->vendor_id, &(*header)->device_type,
                        &(*header)->product_code, &(*header)->major_revision, &(*header)->minor_revision, &(*header)->status,
                        &(*header)->serial_number, &(*header)->product_name_length);
    if(result != BUF_OK) {
        error("Failed to decode CIP Identity fixed fields");
        free(*header);
        *header = NULL;
        return result;
    }

    // Decode variable-length product name
    if((*header)->product_name_length > 0) {
        (*header)->product_name = calloc((*header)->product_name_length + 1, sizeof(uint8_t));
        if(!(*header)->product_name) {
            error("Failed to allocate memory for product name");
            free(*header);
            *header = NULL;
            return BUF_ERR_NO_RESOURCES;
        }

        for(int i = 0; i < (*header)->product_name_length; i++) {
            result = buf_decode(src, false, "u8", &(*header)->product_name[i]);
            if(result != BUF_OK) {
                error("Failed to decode product name byte %d", i);
                free((*header)->product_name);
                free(*header);
                *header = NULL;
                return result;
            }
        }
        (*header)->product_name[(*header)->product_name_length] = '\0';
    }

    trace("Successfully decoded CPF CIP Identity Item");
    return BUF_OK;
}

buf_err_t eip_cpf_cip_identity_item_encode(buf *dest, eip_cpf_cip_identity_item *header) {
    trace("Encoding CPF CIP Identity Item");

    if(!dest || !header) {
        error("NULL pointer passed to eip_cpf_cip_identity_item_encode");
        return BUF_ERR_NULL_PTR;
    }

    header->header.type_id = CPF_TYPE_ID_CIP_IDENTITY;
    header->header.length = 15 + header->product_name_length;  // Fixed fields + product name

    buf_err_t result = eip_cpf_item_header_encode(dest, &header->header);
    if(result != BUF_OK) {
        error("Failed to encode CPF item header");
        return result;
    }

    // Encode fixed fields
    result = buf_encode(dest, true, "< u16 u16 u16 u8 u8 u16 u32 u8", header->vendor_id, header->device_type,
                        header->product_code, header->major_revision, header->minor_revision, header->status,
                        header->serial_number, header->product_name_length);
    if(result != BUF_OK) {
        error("Failed to encode CIP Identity fixed fields");
        return result;
    }

    // Encode variable-length product name
    if(header->product_name_length > 0 && header->product_name) {
        for(int i = 0; i < header->product_name_length; i++) {
            result = buf_encode(dest, true, "u8", header->product_name[i]);
            if(result != BUF_OK) {
                error("Failed to encode product name byte %d", i);
                return result;
            }
        }
    }

    trace("Successfully encoded CPF CIP Identity Item");
    return BUF_OK;
}

void eip_cpf_cip_identity_item_dispose(eip_cpf_cip_identity_item *header) {
    if(header) {
        if(header->product_name) { free(header->product_name); }
        free(header);
    }
}

void eip_cpf_cip_identity_item_log_impl(const char *function, int line, ev_log_level level,
                                        const eip_cpf_cip_identity_item *header) {
    if(!header) {
        ev_log_impl(function, line, level, "CPF CIP Identity Item: NULL");
        return;
    }

    ev_log_impl(function, line, level,
                "CPF CIP Identity Item: vendor_id=0x%04X, device_type=0x%04X, product_code=0x%04X, "
                "revision=%u.%u, status=0x%04X, serial=0x%08X, name='%s'",
                header->vendor_id, header->device_type, header->product_code, header->major_revision, header->minor_revision,
                header->status, header->serial_number, header->product_name ? (char *)header->product_name : "NULL");
}

//=============================================================================
// SOCKET ADDRESS ITEM FUNCTIONS
//=============================================================================

buf_err_t eip_cpf_socket_addr_item_decode(eip_cpf_socket_addr_item **header, buf *src) {
    trace("Decoding CPF Socket Address Item");

    if(!header || !src) {
        error("NULL pointer passed to eip_cpf_socket_addr_item_decode");
        return BUF_ERR_NULL_PTR;
    }

    *header = calloc(1, sizeof(eip_cpf_socket_addr_item));
    if(!*header) {
        error("Failed to allocate memory for Socket Address Item");
        return BUF_ERR_NO_RESOURCES;
    }

    buf_err_t result = eip_cpf_item_header_decode(&(*header)->header, src);
    if(result != BUF_OK) {
        error("Failed to decode CPF item header");
        free(*header);
        *header = NULL;
        return result;
    }

    // Validate type and length
    if((*header)->header.type_id != CPF_TYPE_ID_SOCKET_ADDR) {
        error("Invalid type ID for Socket Address Item: 0x%04X", (*header)->header.type_id);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    if((*header)->header.length != 16) {
        error("Invalid length for Socket Address Item: %u (expected 16)", (*header)->header.length);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    // Decode socket address (big-endian except sin_zero)
    result = buf_decode(src, false, "> u16 u16 u32 < u64", &(*header)->sin_family, &(*header)->sin_port, &(*header)->sin_addr,
                        &(*header)->sin_zero);
    if(result != BUF_OK) {
        error("Failed to decode socket address");
        free(*header);
        *header = NULL;
        return result;
    }

    trace("Successfully decoded CPF Socket Address Item");
    return BUF_OK;
}

buf_err_t eip_cpf_socket_addr_item_encode(buf *dest, eip_cpf_socket_addr_item *header) {
    trace("Encoding CPF Socket Address Item");

    if(!dest || !header) {
        error("NULL pointer passed to eip_cpf_socket_addr_item_encode");
        return BUF_ERR_NULL_PTR;
    }

    header->header.type_id = CPF_TYPE_ID_SOCKET_ADDR;
    header->header.length = 16;

    buf_err_t result = eip_cpf_item_header_encode(dest, &header->header);
    if(result != BUF_OK) {
        error("Failed to encode CPF item header");
        return result;
    }

    // Encode socket address (big-endian except sin_zero)
    result =
        buf_encode(dest, true, "> u16 u16 u32 < u64", header->sin_family, header->sin_port, header->sin_addr, header->sin_zero);
    if(result != BUF_OK) {
        error("Failed to encode socket address");
        return result;
    }

    trace("Successfully encoded CPF Socket Address Item");
    return BUF_OK;
}

void eip_cpf_socket_addr_item_dispose(eip_cpf_socket_addr_item *header) {
    if(header) { free(header); }
}

void eip_cpf_socket_addr_item_log_impl(const char *function, int line, ev_log_level level,
                                       const eip_cpf_socket_addr_item *header) {
    if(!header) {
        ev_log_impl(function, line, level, "CPF Socket Address Item: NULL");
        return;
    }

    // Convert address to readable format
    uint8_t *addr_bytes = (uint8_t *)&header->sin_addr;
    ev_log_impl(function, line, level, "CPF Socket Address Item: family=%u, port=%u, addr=%u.%u.%u.%u", header->sin_family,
                header->sin_port, addr_bytes[0], addr_bytes[1], addr_bytes[2], addr_bytes[3]);
}

//=============================================================================
// LIST IDENTITY RESPONSE FUNCTIONS (Complex structure with variable CPF items)
//=============================================================================

buf_err_t eip_list_identity_response_decode(eip_list_identity_response **header, buf *src) {
    trace("Decoding EIP List Identity Response");

    if(!header || !src) {
        error("NULL pointer passed to eip_list_identity_response_decode");
        return BUF_ERR_NULL_PTR;
    }

    *header = calloc(1, sizeof(eip_list_identity_response));
    if(!*header) {
        error("Failed to allocate memory for List Identity Response");
        return BUF_ERR_NO_RESOURCES;
    }

    buf_err_t result = eip_encap_header_decode(&(*header)->header, src);
    if(result != BUF_OK) {
        error("Failed to decode encapsulation header");
        free(*header);
        *header = NULL;
        return result;
    }

    // Validate command
    if((*header)->header.command != EIP_LIST_IDENTITY) {
        error("Invalid command for List Identity Response: 0x%04X", (*header)->header.command);
        free(*header);
        *header = NULL;
        return BUF_ERR_BAD_FORMAT;
    }

    // Decode item count
    result = buf_decode(src, false, "< u16", &(*header)->item_count);
    if(result != BUF_OK) {
        error("Failed to decode item count");
        free(*header);
        *header = NULL;
        return result;
    }

    // Allocate array for CPF items
    if((*header)->item_count > 0) {
        (*header)->items = calloc((*header)->item_count, sizeof(struct eip_cpf_item_header *));
        if(!(*header)->items) {
            error("Failed to allocate memory for CPF items array");
            free(*header);
            *header = NULL;
            return BUF_ERR_NO_RESOURCES;
        }

        // Parse each CPF item based on its type
        for(int i = 0; i < (*header)->item_count; i++) {
            // Peek at the type ID to determine which parser to use
            uint16_t type_id;
            result = buf_decode(src, true, "< u16", &type_id);  // Peek, don't advance cursor
            if(result != BUF_OK) {
                error("Failed to peek CPF item type ID");
                // Cleanup previously allocated items
                for(int j = 0; j < i; j++) { free((*header)->items[j]); }
                free((*header)->items);
                free(*header);
                *header = NULL;
                return result;
            }

            // Parse based on type ID
            switch(type_id) {
                case CPF_TYPE_ID_NULL_ADDRESS:
                    result = eip_cpf_null_address_item_decode((eip_cpf_null_address_item **)&(*header)->items[i], src);
                    break;
                case CPF_TYPE_ID_CIP_IDENTITY:
                    result = eip_cpf_cip_identity_item_decode((eip_cpf_cip_identity_item **)&(*header)->items[i], src);
                    break;
                case CPF_TYPE_ID_SOCKET_ADDR:
                    result = eip_cpf_socket_addr_item_decode((eip_cpf_socket_addr_item **)&(*header)->items[i], src);
                    break;
                case CPF_TYPE_ID_CONNECTED_ADDR:
                    result = eip_cpf_connected_address_item_decode((eip_cpf_connected_address_item **)&(*header)->items[i], src);
                    break;
                default:
                    error("Unknown CPF item type ID: 0x%04X", type_id);
                    result = BUF_ERR_BAD_FORMAT;
                    break;
            }

            if(result != BUF_OK) {
                error("Failed to decode CPF item %d", i);
                // Cleanup previously allocated items
                for(int j = 0; j < i; j++) { free((*header)->items[j]); }
                free((*header)->items);
                free(*header);
                *header = NULL;
                return result;
            }
        }
    }

    trace("Successfully decoded List Identity Response with %u items", (*header)->item_count);
    return BUF_OK;
}

buf_err_t eip_list_identity_response_encode(buf *dest, eip_list_identity_response *header) {
    trace("Encoding EIP List Identity Response");

    if(!dest || !header) {
        error("NULL pointer passed to eip_list_identity_response_encode");
        return BUF_ERR_NULL_PTR;
    }

    // Calculate total length
    uint16_t total_length = 2;  // item_count field
    for(int i = 0; i < header->item_count; i++) {
        if(header->items[i]) {
            total_length += 4 + header->items[i]->length;  // header + data
        }
    }

    header->header.command = EIP_LIST_IDENTITY;
    header->header.length = total_length;
    header->header.status = 0;

    buf_err_t result = eip_encap_header_encode(dest, &header->header);
    if(result != BUF_OK) {
        error("Failed to encode encapsulation header");
        return result;
    }

    result = buf_encode(dest, true, "< u16", header->item_count);
    if(result != BUF_OK) {
        error("Failed to encode item count");
        return result;
    }

    // Encode each CPF item based on its type
    for(int i = 0; i < header->item_count; i++) {
        if(!header->items[i]) {
            error("NULL CPF item at index %d", i);
            return BUF_ERR_NULL_PTR;
        }

        uint16_t type_id = header->items[i]->type_id;
        switch(type_id) {
            case CPF_TYPE_ID_NULL_ADDRESS:
                result = eip_cpf_null_address_item_encode(dest, (eip_cpf_null_address_item *)header->items[i]);
                break;
            case CPF_TYPE_ID_CIP_IDENTITY:
                result = eip_cpf_cip_identity_item_encode(dest, (eip_cpf_cip_identity_item *)header->items[i]);
                break;
            case CPF_TYPE_ID_SOCKET_ADDR:
                result = eip_cpf_socket_addr_item_encode(dest, (eip_cpf_socket_addr_item *)header->items[i]);
                break;
            case CPF_TYPE_ID_CONNECTED_ADDR:
                result = eip_cpf_connected_address_item_encode(dest, (eip_cpf_connected_address_item *)header->items[i]);
                break;
            default: error("Unknown CPF item type ID: 0x%04X", type_id); return BUF_ERR_BAD_FORMAT;
        }

        if(result != BUF_OK) {
            error("Failed to encode CPF item %d", i);
            return result;
        }
    }

    trace("Successfully encoded List Identity Response");
    return BUF_OK;
}

void eip_list_identity_response_dispose(eip_list_identity_response *header) {
    if(header) {
        if(header->items) {
            for(int i = 0; i < header->item_count; i++) {
                if(header->items[i]) {
                    // We need to dispose based on type, but since all items start with eip_cpf_item_header,
                    // we can safely free them all the same way for now
                    free(header->items[i]);
                }
            }
            free(header->items);
        }
        free(header);
    }
}

void eip_list_identity_response_log_impl(const char *function, int line, ev_log_level level,
                                         const eip_list_identity_response *header) {
    if(!header) {
        ev_log_impl(function, line, level, "List Identity Response: NULL");
        return;
    }

    ev_log_impl(function, line, level, "List Identity Response: cmd=0x%04X, len=%u, session=0x%08X, status=0x%08X, items=%u",
                header->header.command, header->header.length, header->header.session_handle, header->header.status,
                header->item_count);
}
