#include "modbus_generic_response.h"
#include <stdlib.h>
#include <string.h>

/**
 * Implementation of Multiple PDU Types in Modbus Response Frames
 */

/* Implement individual response PDUs */
PTK_IMPLEMENT_PDU_CUSTOM(modbus_read_coils_response)
PTK_IMPLEMENT_PDU_CUSTOM(modbus_read_holding_response)
PTK_IMPLEMENT_PDU(modbus_write_single_coil_response)
PTK_IMPLEMENT_PDU(modbus_write_single_register_response)

/* Implement generic frame types */
PTK_IMPLEMENT_PDU_CUSTOM(modbus_generic_response_frame_v1)
PTK_IMPLEMENT_PDU_CUSTOM(modbus_tagged_response_frame)
PTK_IMPLEMENT_PDU_CUSTOM(modbus_polymorphic_response_frame)

/**
 * Coil Data Implementation (for Read Coils response)
 */
ptk_status_t modbus_coil_data_serialize(ptk_slice_bytes_t* slice, const modbus_coil_data_t* data, ptk_endian_t endian) {
    if (!slice || !data) return PTK_ERROR_INVALID_PARAM;
    (void)endian; // Coil data is just bytes, no endianness
    
    // Write coil data bytes
    if (data->byte_count > 0 && data->coil_data) {
        ptk_slice_bytes_t coil_slice = ptk_slice_bytes_make(data->coil_data, data->byte_count);
        *slice = ptk_write_bytes(*slice, coil_slice);
        if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    }
    
    return PTK_OK;
}

ptk_status_t modbus_coil_data_deserialize(ptk_slice_bytes_t* slice, modbus_coil_data_t* data, ptk_endian_t endian) {
    if (!slice || !data) return PTK_ERROR_INVALID_PARAM;
    (void)endian; // Coil data is just bytes, no endianness
    
    // The byte_count should be set by the calling PDU
    if (data->byte_count > data->capacity) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    // Read coil data bytes
    for (uint8_t i = 0; i < data->byte_count; i++) {
        data->coil_data[i] = ptk_read_uint8(slice);
    }
    
    return PTK_OK;
}

size_t modbus_coil_data_size(const modbus_coil_data_t* data) {
    return data ? data->byte_count : 0;
}

void modbus_coil_data_init(modbus_coil_data_t* data, size_t capacity) {
    if (!data) return;
    data->byte_count = 0;
    data->capacity = capacity;
    data->coil_data = (capacity > 0) ? malloc(capacity) : NULL;
}

void modbus_coil_data_destroy(modbus_coil_data_t* data) {
    if (!data) return;
    free(data->coil_data);
    data->coil_data = NULL;
    data->byte_count = 0;
    data->capacity = 0;
}

void modbus_coil_data_print(const modbus_coil_data_t* data) {
    if (!data) {
        printf("NULL coil data");
        return;
    }
    
    printf("Coils[%u bytes]: { ", data->byte_count);
    for (uint8_t i = 0; i < data->byte_count; i++) {
        printf("0x%02X", data->coil_data[i]);
        if (i < data->byte_count - 1) printf(", ");
    }
    printf(" }");
}

/**
 * Method 1: Generic Response PDU with Manual Union Handling
 */
ptk_status_t modbus_generic_response_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_generic_response_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM;
    
    switch (pdu->response_type) {
        case MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_REGISTERS:
            return modbus_write_multiple_response_serialize(slice, &pdu->pdu.write_multiple_regs, endian);
            
        case MODBUS_RESPONSE_TYPE_READ_COILS:
            return modbus_read_coils_response_serialize(slice, &pdu->pdu.read_coils, endian);
            
        case MODBUS_RESPONSE_TYPE_READ_HOLDING_REGISTERS:
            return modbus_read_holding_response_serialize(slice, &pdu->pdu.read_holding, endian);
            
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_COIL:
            return modbus_write_single_coil_response_serialize(slice, &pdu->pdu.write_single_coil, endian);
            
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_REGISTER:
            return modbus_write_single_register_response_serialize(slice, &pdu->pdu.write_single_register, endian);
            
        case MODBUS_RESPONSE_TYPE_EXCEPTION:
            return modbus_exception_response_serialize(slice, &pdu->pdu.exception, endian);
            
        default:
            return PTK_ERROR_INVALID_PARAM;
    }
}

ptk_status_t modbus_generic_response_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_generic_response_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu || slice->len < 1) return PTK_ERROR_INVALID_PARAM;
    
    // Peek at function code to determine response type
    uint8_t function_code = slice->data[0];
    pdu->response_type = modbus_detect_response_type(function_code);
    
    switch (pdu->response_type) {
        case MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_REGISTERS:
            return modbus_write_multiple_response_deserialize(slice, &pdu->pdu.write_multiple_regs, endian);
            
        case MODBUS_RESPONSE_TYPE_READ_COILS:
            // Need to set up byte count from PDU for coil data
            if (slice->len >= 2) {
                pdu->pdu.read_coils.coil_data.byte_count = slice->data[1];
            }
            return modbus_read_coils_response_deserialize(slice, &pdu->pdu.read_coils, endian);
            
        case MODBUS_RESPONSE_TYPE_READ_HOLDING_REGISTERS:
            // Need to set up register count from byte count
            if (slice->len >= 2) {
                pdu->pdu.read_holding.register_data.count = slice->data[1] / 2;
            }
            return modbus_read_holding_response_deserialize(slice, &pdu->pdu.read_holding, endian);
            
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_COIL:
            return modbus_write_single_coil_response_deserialize(slice, &pdu->pdu.write_single_coil, endian);
            
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_REGISTER:
            return modbus_write_single_register_response_deserialize(slice, &pdu->pdu.write_single_register, endian);
            
        case MODBUS_RESPONSE_TYPE_EXCEPTION:
            return modbus_exception_response_deserialize(slice, &pdu->pdu.exception, endian);
            
        default:
            return PTK_ERROR_INVALID_PARAM;
    }
}

size_t modbus_generic_response_pdu_size(const modbus_generic_response_pdu_t* pdu) {
    if (!pdu) return 0;
    
    switch (pdu->response_type) {
        case MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_REGISTERS:
            return modbus_write_multiple_response_size(&pdu->pdu.write_multiple_regs);
        case MODBUS_RESPONSE_TYPE_READ_COILS:
            return modbus_read_coils_response_size(&pdu->pdu.read_coils);
        case MODBUS_RESPONSE_TYPE_READ_HOLDING_REGISTERS:
            return modbus_read_holding_response_size(&pdu->pdu.read_holding);
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_COIL:
            return modbus_write_single_coil_response_size(&pdu->pdu.write_single_coil);
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_REGISTER:
            return modbus_write_single_register_response_size(&pdu->pdu.write_single_register);
        case MODBUS_RESPONSE_TYPE_EXCEPTION:
            return modbus_exception_response_size(&pdu->pdu.exception);
        default:
            return 0;
    }
}

void modbus_generic_response_pdu_init(modbus_generic_response_pdu_t* pdu) {
    if (!pdu) return;
    memset(pdu, 0, sizeof(*pdu));
    pdu->response_type = MODBUS_RESPONSE_TYPE_UNKNOWN;
}

void modbus_generic_response_pdu_destroy(modbus_generic_response_pdu_t* pdu) {
    if (!pdu) return;
    
    switch (pdu->response_type) {
        case MODBUS_RESPONSE_TYPE_READ_COILS:
            modbus_read_coils_response_destroy(&pdu->pdu.read_coils);
            break;
        case MODBUS_RESPONSE_TYPE_READ_HOLDING_REGISTERS:
            modbus_read_holding_response_destroy(&pdu->pdu.read_holding);
            break;
        default:
            // Other types don't need cleanup
            break;
    }
    
    pdu->response_type = MODBUS_RESPONSE_TYPE_UNKNOWN;
}

void modbus_generic_response_pdu_print(const modbus_generic_response_pdu_t* pdu) {
    if (!pdu) {
        printf("NULL PDU");
        return;
    }
    
    printf("Generic Response (");
    switch (pdu->response_type) {
        case MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_REGISTERS:
            printf("Write Multiple Registers): ");
            modbus_write_multiple_response_print(&pdu->pdu.write_multiple_regs);
            break;
        case MODBUS_RESPONSE_TYPE_READ_COILS:
            printf("Read Coils): ");
            modbus_read_coils_response_print(&pdu->pdu.read_coils);
            break;
        case MODBUS_RESPONSE_TYPE_READ_HOLDING_REGISTERS:
            printf("Read Holding Registers): ");
            modbus_read_holding_response_print(&pdu->pdu.read_holding);
            break;
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_COIL:
            printf("Write Single Coil): ");
            modbus_write_single_coil_response_print(&pdu->pdu.write_single_coil);
            break;
        case MODBUS_RESPONSE_TYPE_WRITE_SINGLE_REGISTER:
            printf("Write Single Register): ");
            modbus_write_single_register_response_print(&pdu->pdu.write_single_register);
            break;
        case MODBUS_RESPONSE_TYPE_EXCEPTION:
            printf("Exception): ");
            modbus_exception_response_print(&pdu->pdu.exception);
            break;
        default:
            printf("Unknown): <invalid>");
            break;
    }
}

/**
 * Method 3: Tagged PDU Implementation
 */
ptk_status_t modbus_tagged_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_tagged_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM;
    
    switch (pdu->tag) {
        case MODBUS_PDU_TAG_WRITE_MULTIPLE_REGS:
            return modbus_write_multiple_response_serialize(slice, &pdu->data.write_multiple_regs, endian);
        case MODBUS_PDU_TAG_READ_COILS:
            return modbus_read_coils_response_serialize(slice, &pdu->data.read_coils, endian);
        case MODBUS_PDU_TAG_READ_HOLDING:
            return modbus_read_holding_response_serialize(slice, &pdu->data.read_holding, endian);
        case MODBUS_PDU_TAG_WRITE_SINGLE_COIL:
            return modbus_write_single_coil_response_serialize(slice, &pdu->data.write_single_coil, endian);
        case MODBUS_PDU_TAG_WRITE_SINGLE_REGISTER:
            return modbus_write_single_register_response_serialize(slice, &pdu->data.write_single_register, endian);
        default:
            if (pdu->tag & 0x80) { // Exception response
                return modbus_exception_response_serialize(slice, &pdu->data.exception, endian);
            }
            return PTK_ERROR_INVALID_PARAM;
    }
}

ptk_status_t modbus_tagged_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_tagged_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu || slice->len < 1) return PTK_ERROR_INVALID_PARAM;
    
    // Function code determines the tag
    uint8_t function_code = slice->data[0];
    pdu->tag = modbus_function_code_to_tag(function_code);
    
    return modbus_tagged_pdu_serialize(slice, pdu, endian); // Reuse serialize logic
}

size_t modbus_tagged_pdu_size(const modbus_tagged_pdu_t* pdu) {
    if (!pdu) return 0;
    
    switch (pdu->tag) {
        case MODBUS_PDU_TAG_WRITE_MULTIPLE_REGS:
            return modbus_write_multiple_response_size(&pdu->data.write_multiple_regs);
        case MODBUS_PDU_TAG_READ_COILS:
            return modbus_read_coils_response_size(&pdu->data.read_coils);
        case MODBUS_PDU_TAG_READ_HOLDING:
            return modbus_read_holding_response_size(&pdu->data.read_holding);
        case MODBUS_PDU_TAG_WRITE_SINGLE_COIL:
            return modbus_write_single_coil_response_size(&pdu->data.write_single_coil);
        case MODBUS_PDU_TAG_WRITE_SINGLE_REGISTER:
            return modbus_write_single_register_response_size(&pdu->data.write_single_register);
        default:
            if (pdu->tag & 0x80) { // Exception
                return modbus_exception_response_size(&pdu->data.exception);
            }
            return 0;
    }
}

void modbus_tagged_pdu_init(modbus_tagged_pdu_t* pdu) {
    if (!pdu) return;
    memset(pdu, 0, sizeof(*pdu));
}

void modbus_tagged_pdu_destroy(modbus_tagged_pdu_t* pdu) {
    if (!pdu) return;
    
    switch (pdu->tag) {
        case MODBUS_PDU_TAG_READ_COILS:
            modbus_read_coils_response_destroy(&pdu->data.read_coils);
            break;
        case MODBUS_PDU_TAG_READ_HOLDING:
            modbus_read_holding_response_destroy(&pdu->data.read_holding);
            break;
        default:
            break;
    }
}

void modbus_tagged_pdu_print(const modbus_tagged_pdu_t* pdu) {
    if (!pdu) {
        printf("NULL tagged PDU");
        return;
    }
    
    printf("Tagged PDU (tag=0x%02X): ", pdu->tag);
    // Use same print logic as generic response
    // ... (implementation similar to generic version)
}

/**
 * Method 4: Polymorphic PDU with VTables
 */

/* VTable implementations for different response types */
static ptk_status_t write_multiple_serialize(ptk_slice_bytes_t* slice, const void* pdu, ptk_endian_t endian) {
    return modbus_write_multiple_response_serialize(slice, (const modbus_write_multiple_response_t*)pdu, endian);
}

static ptk_status_t write_multiple_deserialize(ptk_slice_bytes_t* slice, void* pdu, ptk_endian_t endian) {
    return modbus_write_multiple_response_deserialize(slice, (modbus_write_multiple_response_t*)pdu, endian);
}

static size_t write_multiple_size(const void* pdu) {
    return modbus_write_multiple_response_size((const modbus_write_multiple_response_t*)pdu);
}

static void write_multiple_print(const void* pdu) {
    modbus_write_multiple_response_print((const modbus_write_multiple_response_t*)pdu);
}

const modbus_response_vtable_t modbus_write_multiple_vtable = {
    .serialize = write_multiple_serialize,
    .deserialize = write_multiple_deserialize,
    .size = write_multiple_size,
    .print = write_multiple_print,
    .name = "Write Multiple Registers Response"
};

// Similar implementations for other response types...

ptk_status_t modbus_polymorphic_pdu_serialize(ptk_slice_bytes_t* slice, const modbus_polymorphic_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu || !pdu->vtable || !pdu->pdu_data) return PTK_ERROR_INVALID_PARAM;
    
    return pdu->vtable->serialize(slice, pdu->pdu_data, endian);
}

ptk_status_t modbus_polymorphic_pdu_deserialize(ptk_slice_bytes_t* slice, modbus_polymorphic_pdu_t* pdu, ptk_endian_t endian) {
    if (!slice || !pdu || !pdu->vtable || !pdu->pdu_data) return PTK_ERROR_INVALID_PARAM;
    
    return pdu->vtable->deserialize(slice, pdu->pdu_data, endian);
}

size_t modbus_polymorphic_pdu_size(const modbus_polymorphic_pdu_t* pdu) {
    if (!pdu || !pdu->vtable || !pdu->pdu_data) return 0;
    
    return pdu->vtable->size(pdu->pdu_data);
}

void modbus_polymorphic_pdu_init(modbus_polymorphic_pdu_t* pdu) {
    if (!pdu) return;
    memset(pdu, 0, sizeof(*pdu));
}

void modbus_polymorphic_pdu_destroy(modbus_polymorphic_pdu_t* pdu) {
    if (!pdu) return;
    
    free(pdu->pdu_data);
    pdu->pdu_data = NULL;
    pdu->vtable = NULL;
    pdu->pdu_size = 0;
}

void modbus_polymorphic_pdu_print(const modbus_polymorphic_pdu_t* pdu) {
    if (!pdu || !pdu->vtable) {
        printf("NULL polymorphic PDU");
        return;
    }
    
    printf("Polymorphic PDU (%s): ", pdu->vtable->name);
    if (pdu->vtable->print && pdu->pdu_data) {
        pdu->vtable->print(pdu->pdu_data);
    }
}

/**
 * Helper Functions
 */
modbus_response_type_t modbus_detect_response_type(uint8_t function_code) {
    if (function_code & 0x80) {
        return MODBUS_RESPONSE_TYPE_EXCEPTION;
    }
    
    switch (function_code) {
        case MODBUS_FC_READ_COILS:
            return MODBUS_RESPONSE_TYPE_READ_COILS;
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            return MODBUS_RESPONSE_TYPE_READ_DISCRETE_INPUTS;
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            return MODBUS_RESPONSE_TYPE_READ_HOLDING_REGISTERS;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            return MODBUS_RESPONSE_TYPE_READ_INPUT_REGISTERS;
        case MODBUS_FC_WRITE_SINGLE_COIL:
            return MODBUS_RESPONSE_TYPE_WRITE_SINGLE_COIL;
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            return MODBUS_RESPONSE_TYPE_WRITE_SINGLE_REGISTER;
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            return MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_COILS;
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            return MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_REGISTERS;
        default:
            return MODBUS_RESPONSE_TYPE_UNKNOWN;
    }
}

modbus_pdu_tag_t modbus_function_code_to_tag(uint8_t function_code) {
    if (function_code & 0x80) {
        return MODBUS_PDU_TAG_EXCEPTION;
    }
    return (modbus_pdu_tag_t)function_code;
}

/**
 * Demonstration Function
 */
void demonstrate_modbus_multiple_pdus(void) {
    printf("=== Modbus Multiple PDU Types Demo ===\n\n");
    
    // Example 1: Generic response with switch-based dispatch
    printf("1. Generic Response Frame (Method 1):\n");
    printf("-------------------------------------\n");
    
    modbus_generic_response_frame_v1_t generic_frame;
    modbus_generic_response_frame_v1_init(&generic_frame);
    
    // Create MBAP header
    modbus_create_mbap_header(&generic_frame.mbap, 0x5678, 0x01, 5);
    
    // Create a Write Multiple Registers response
    generic_frame.response.response_type = MODBUS_RESPONSE_TYPE_WRITE_MULTIPLE_REGISTERS;
    modbus_create_write_multiple_response(&generic_frame.response.pdu.write_multiple_regs, 1000, 4);
    
    printf("Generic frame with Write Multiple response:\n");
    modbus_generic_response_frame_v1_print(&generic_frame);
    printf("Frame size: %zu bytes\n\n", modbus_generic_response_frame_v1_size(&generic_frame));
    
    // Example 2: Tagged PDU approach
    printf("2. Tagged Response Frame (Method 3):\n");
    printf("------------------------------------\n");
    
    modbus_tagged_response_frame_t tagged_frame;
    modbus_tagged_response_frame_init(&tagged_frame);
    
    // Create MBAP header
    modbus_create_mbap_header(&tagged_frame.mbap, 0x9ABC, 0x01, 2);
    
    // Create an exception response
    tagged_frame.pdu.tag = MODBUS_PDU_TAG_EXCEPTION;
    modbus_create_exception_response(&tagged_frame.pdu.data.exception, 
                                   MODBUS_FC_WRITE_MULTIPLE_REGISTERS, 
                                   MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    
    printf("Tagged frame with exception response:\n");
    modbus_tagged_response_frame_print(&tagged_frame);
    printf("Frame size: %zu bytes\n\n", modbus_tagged_response_frame_size(&tagged_frame));
    
    // Example 3: Polymorphic approach
    printf("3. Polymorphic Response Frame (Method 4):\n");
    printf("-----------------------------------------\n");
    
    modbus_polymorphic_response_frame_t poly_frame;
    modbus_polymorphic_response_frame_init(&poly_frame);
    
    // Create MBAP header
    modbus_create_mbap_header(&poly_frame.mbap, 0xDEF0, 0x01, 5);
    
    // Create polymorphic PDU with write multiple response
    modbus_write_multiple_response_t* write_resp = malloc(sizeof(modbus_write_multiple_response_t));
    modbus_write_multiple_response_init(write_resp);
    modbus_create_write_multiple_response(write_resp, 2000, 8);
    
    poly_frame.pdu.vtable = &modbus_write_multiple_vtable;
    poly_frame.pdu.pdu_data = write_resp;
    poly_frame.pdu.pdu_size = sizeof(modbus_write_multiple_response_t);
    
    printf("Polymorphic frame with write multiple response:\n");
    modbus_polymorphic_response_frame_print(&poly_frame);
    printf("Frame size: %zu bytes\n\n", modbus_polymorphic_response_frame_size(&poly_frame));
    
    // Cleanup
    modbus_generic_response_frame_v1_destroy(&generic_frame);
    modbus_tagged_response_frame_destroy(&tagged_frame);
    modbus_polymorphic_response_frame_destroy(&poly_frame);
    
    printf("=== End Multiple PDU Demo ===\n");
}