#include "ethernet_ip_example.h"
#include <stdlib.h>
#include <string.h>

/**
 * Implementation of EtherNet/IP custom types and PDUs
 */

/* Implement basic PDUs */
PTK_IMPLEMENT_PDU(enip_header)

/* Implement custom PDUs */
PTK_IMPLEMENT_PDU_CUSTOM(cip_request)
PTK_IMPLEMENT_PDU_CUSTOM(cip_response)
PTK_IMPLEMENT_PDU_CUSTOM(forward_open_request)
PTK_IMPLEMENT_PDU_CUSTOM(identity_response)

/**
 * CIP Path Implementation
 */
ptk_status_t cip_path_serialize(ptk_slice_bytes_t* slice, const cip_path_t* path, ptk_endian_t endian) {
    if (!slice || !path) return PTK_ERROR_INVALID_PARAM;
    
    // Write path size (in 16-bit words)
    *slice = ptk_write_uint8(*slice, path->path_size);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    // Write path data
    if (path->path_size > 0 && path->path_data) {
        size_t byte_size = path->path_size * 2; // Convert words to bytes
        ptk_slice_bytes_t path_slice = ptk_slice_bytes_make(path->path_data, byte_size);
        *slice = ptk_write_bytes(*slice, path_slice);
        if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Pad to even byte boundary if path size is odd
    if (path->path_size % 2 != 0) {
        *slice = ptk_write_uint8(*slice, 0x00); // Padding byte
        if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    }
    
    return PTK_OK;
}

ptk_status_t cip_path_deserialize(ptk_slice_bytes_t* slice, cip_path_t* path, ptk_endian_t endian) {
    if (!slice || !path) return PTK_ERROR_INVALID_PARAM;
    
    // Read path size
    path->path_size = ptk_read_uint8(slice);
    
    if (path->path_size > 0) {
        size_t byte_size = path->path_size * 2;
        
        // Ensure we have enough capacity
        if (path->capacity < byte_size) {
            return PTK_ERROR_BUFFER_TOO_SMALL;
        }
        
        // Read path data
        for (size_t i = 0; i < byte_size; i++) {
            path->path_data[i] = ptk_read_uint8(slice);
        }
        
        // Skip padding byte if path size is odd
        if (path->path_size % 2 != 0) {
            ptk_read_uint8(slice); // Consume padding
        }
    }
    
    return PTK_OK;
}

size_t cip_path_size(const cip_path_t* path) {
    if (!path) return 0;
    
    size_t size = 1; // path_size byte
    size += path->path_size * 2; // path data in bytes
    
    // Add padding if odd path size
    if (path->path_size % 2 != 0) {
        size += 1;
    }
    
    return size;
}

void cip_path_init(cip_path_t* path, size_t capacity) {
    if (!path) return;
    
    path->path_size = 0;
    path->capacity = capacity;
    path->path_data = (capacity > 0) ? malloc(capacity) : NULL;
}

void cip_path_destroy(cip_path_t* path) {
    if (!path) return;
    
    free(path->path_data);
    path->path_data = NULL;
    path->path_size = 0;
    path->capacity = 0;
}

void cip_path_print(const cip_path_t* path) {
    if (!path) {
        printf("NULL path");
        return;
    }
    
    printf("CIP_Path { size: %u, data: ", path->path_size);
    if (path->path_data && path->path_size > 0) {
        for (size_t i = 0; i < path->path_size * 2; i++) {
            printf("%02X ", path->path_data[i]);
        }
    } else {
        printf("(empty)");
    }
    printf("}");
}

/**
 * CIP Data Implementation
 */
ptk_status_t cip_data_serialize(ptk_slice_bytes_t* slice, const cip_data_t* data, ptk_endian_t endian) {
    if (!slice || !data) return PTK_ERROR_INVALID_PARAM;
    
    // Write data length
    *slice = ptk_write_uint16(*slice, data->data_length, endian);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    // Write data
    if (data->data_length > 0 && data->data) {
        ptk_slice_bytes_t data_slice = ptk_slice_bytes_make(data->data, data->data_length);
        *slice = ptk_write_bytes(*slice, data_slice);
        if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    }
    
    return PTK_OK;
}

ptk_status_t cip_data_deserialize(ptk_slice_bytes_t* slice, cip_data_t* data, ptk_endian_t endian) {
    if (!slice || !data) return PTK_ERROR_INVALID_PARAM;
    
    // Read data length
    data->data_length = ptk_read_uint16(slice, endian);
    
    if (data->data_length > 0) {
        // Ensure we have enough capacity
        if (data->capacity < data->data_length) {
            return PTK_ERROR_BUFFER_TOO_SMALL;
        }
        
        // Read data
        for (size_t i = 0; i < data->data_length; i++) {
            data->data[i] = ptk_read_uint8(slice);
        }
    }
    
    return PTK_OK;
}

size_t cip_data_size(const cip_data_t* data) {
    if (!data) return 0;
    return 2 + data->data_length; // length field + data
}

void cip_data_init(cip_data_t* data, size_t capacity) {
    if (!data) return;
    
    data->data_length = 0;
    data->capacity = capacity;
    data->data = (capacity > 0) ? malloc(capacity) : NULL;
}

void cip_data_destroy(cip_data_t* data) {
    if (!data) return;
    
    free(data->data);
    data->data = NULL;
    data->data_length = 0;
    data->capacity = 0;
}

void cip_data_print(const cip_data_t* data) {
    if (!data) {
        printf("NULL data");
        return;
    }
    
    printf("CIP_Data { length: %u, data: ", data->data_length);
    if (data->data && data->data_length > 0) {
        for (size_t i = 0; i < data->data_length; i++) {
            printf("%02X ", data->data[i]);
        }
    } else {
        printf("(empty)");
    }
    printf("}");
}

/**
 * Identity Object Implementation
 */
ptk_status_t identity_object_serialize(ptk_slice_bytes_t* slice, const identity_object_t* obj, ptk_endian_t endian) {
    if (!slice || !obj) return PTK_ERROR_INVALID_PARAM;
    
    *slice = ptk_write_uint16(*slice, obj->vendor_id, endian);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    *slice = ptk_write_uint16(*slice, obj->device_type, endian);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    *slice = ptk_write_uint16(*slice, obj->product_code, endian);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    *slice = ptk_write_uint8(*slice, obj->major_revision);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    *slice = ptk_write_uint8(*slice, obj->minor_revision);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    *slice = ptk_write_uint16(*slice, obj->status, endian);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    *slice = ptk_write_uint32(*slice, obj->serial_number, endian);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    *slice = ptk_write_uint8(*slice, obj->product_name_length);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    // Write product name
    ptk_slice_bytes_t name_slice = ptk_slice_bytes_make((uint8_t*)obj->product_name, obj->product_name_length);
    *slice = ptk_write_bytes(*slice, name_slice);
    if (ptk_slice_bytes_is_empty(*slice)) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    return PTK_OK;
}

ptk_status_t identity_object_deserialize(ptk_slice_bytes_t* slice, identity_object_t* obj, ptk_endian_t endian) {
    if (!slice || !obj) return PTK_ERROR_INVALID_PARAM;
    
    obj->vendor_id = ptk_read_uint16(slice, endian);
    obj->device_type = ptk_read_uint16(slice, endian);
    obj->product_code = ptk_read_uint16(slice, endian);
    obj->major_revision = ptk_read_uint8(slice);
    obj->minor_revision = ptk_read_uint8(slice);
    obj->status = ptk_read_uint16(slice, endian);
    obj->serial_number = ptk_read_uint32(slice, endian);
    obj->product_name_length = ptk_read_uint8(slice);
    
    // Read product name (limited by buffer size)
    size_t copy_len = (obj->product_name_length < sizeof(obj->product_name)) ? 
                      obj->product_name_length : sizeof(obj->product_name) - 1;
    
    for (size_t i = 0; i < copy_len; i++) {
        obj->product_name[i] = ptk_read_uint8(slice);
    }
    obj->product_name[copy_len] = '\0'; // Null terminate
    
    // Skip any remaining product name bytes if buffer was too small
    for (size_t i = copy_len; i < obj->product_name_length; i++) {
        ptk_read_uint8(slice);
    }
    
    return PTK_OK;
}

size_t identity_object_size(const identity_object_t* obj) {
    if (!obj) return 0;
    return 13 + obj->product_name_length; // Fixed fields + variable name
}

void identity_object_init(identity_object_t* obj) {
    if (!obj) return;
    memset(obj, 0, sizeof(*obj));
}

void identity_object_destroy(identity_object_t* obj) {
    if (!obj) return;
    // Nothing to free for this simple object
}

void identity_object_print(const identity_object_t* obj) {
    if (!obj) {
        printf("NULL identity");
        return;
    }
    
    printf("Identity {\n");
    printf("  vendor_id: 0x%04X\n", obj->vendor_id);
    printf("  device_type: 0x%04X\n", obj->device_type);
    printf("  product_code: 0x%04X\n", obj->product_code);
    printf("  revision: %u.%u\n", obj->major_revision, obj->minor_revision);
    printf("  status: 0x%04X\n", obj->status);
    printf("  serial_number: %u\n", obj->serial_number);
    printf("  product_name: \"%.*s\"\n", obj->product_name_length, obj->product_name);
    printf("}");
}

/**
 * Helper Functions
 */
ptk_status_t cip_path_build_simple(cip_path_t* path, uint16_t class_id, uint16_t instance_id, uint16_t attribute_id) {
    if (!path || !path->path_data) return PTK_ERROR_INVALID_PARAM;
    
    // Build a simple 3-segment path: class/instance/attribute
    // Each segment is 2 bytes (segment type + value)
    if (path->capacity < 6) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    uint8_t* data = path->path_data;
    
    // Class segment (0x20 = logical class, 8-bit)
    data[0] = 0x20;
    data[1] = (uint8_t)class_id;
    
    // Instance segment (0x24 = logical instance, 8-bit)  
    data[2] = 0x24;
    data[3] = (uint8_t)instance_id;
    
    // Attribute segment (0x30 = logical attribute, 8-bit)
    data[4] = 0x30;
    data[5] = (uint8_t)attribute_id;
    
    path->path_size = 3; // 3 words
    
    return PTK_OK;
}

ptk_status_t cip_data_from_bytes(cip_data_t* data, const uint8_t* bytes, size_t length) {
    if (!data || !bytes) return PTK_ERROR_INVALID_PARAM;
    if (length > data->capacity) return PTK_ERROR_BUFFER_TOO_SMALL;
    
    memcpy(data->data, bytes, length);
    data->data_length = length;
    
    return PTK_OK;
}

/**
 * Demo function showing EtherNet/IP usage
 */
void demonstrate_ethernet_ip(void) {
    printf("=== EtherNet/IP PDU System Demonstration ===\n\n");
    
    // Create a buffer for serialization
    uint8_t buffer[1024];
    ptk_slice_bytes_t slice = ptk_slice_bytes_make(buffer, sizeof(buffer));
    
    // Example 1: EtherNet/IP Header
    printf("1. EtherNet/IP Header:\n");
    enip_header_t enip_hdr;
    enip_header_init(&enip_hdr);
    enip_hdr.command = ENIP_CMD_SEND_RR_DATA;
    enip_hdr.length = 24; // Example length
    enip_hdr.session_handle = 0x12345678;
    enip_hdr.status = ENIP_STATUS_SUCCESS;
    enip_hdr.sender_context = 0x1122334455667788ULL;
    enip_hdr.options = 0;
    
    enip_header_print(&enip_hdr);
    printf("Size: %zu bytes\n\n", enip_header_size(&enip_hdr));
    
    // Example 2: CIP Request with custom path
    printf("2. CIP Request (Get Attribute All for Identity Object):\n");
    cip_request_t cip_req;
    cip_request_init(&cip_req);
    cip_req.service = CIP_SERVICE_GET_ATTRIBUTE_ALL;
    
    // Initialize path and data
    cip_path_init(&cip_req.path, 32);
    cip_data_init(&cip_req.data, 64);
    
    // Build path for Identity Object (Class 1, Instance 1)
    cip_path_build_simple(&cip_req.path, 0x01, 0x01, 0x00);
    
    cip_request_print(&cip_req);
    printf("Size: %zu bytes\n", cip_request_size(&cip_req));
    
    // Serialize the request
    ptk_slice_bytes_t write_slice = slice;
    ptk_status_t status = cip_request_serialize(&write_slice, &cip_req, PTK_ENDIAN_LITTLE);
    printf("Serialization: %s\n\n", (status == PTK_OK) ? "SUCCESS" : "FAILED");
    
    // Example 3: Identity Object Response
    printf("3. Identity Object Response:\n");
    identity_response_t identity_resp;
    identity_response_init(&identity_resp);
    
    identity_resp.identity.vendor_id = 0x001D; // Rockwell Automation
    identity_resp.identity.device_type = 0x002B; // Generic EtherNet/IP Device
    identity_resp.identity.product_code = 0x0001;
    identity_resp.identity.major_revision = 1;
    identity_resp.identity.minor_revision = 0;
    identity_resp.identity.status = 0x0060; // Configured state
    identity_resp.identity.serial_number = 0x12345678;
    identity_resp.identity.product_name_length = 12;
    strcpy(identity_resp.identity.product_name, "Test Device");
    
    identity_response_print(&identity_resp);
    printf("Size: %zu bytes\n\n", identity_response_size(&identity_resp));
    
    // Cleanup
    cip_path_destroy(&cip_req.path);
    cip_data_destroy(&cip_req.data);
    identity_response_destroy(&identity_resp);
    
    printf("=== End EtherNet/IP Demonstration ===\n");
}