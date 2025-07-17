#include "../include/ethernetip.h"
#include <ptk_mem.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <string.h>
#include <stdio.h>

//=============================================================================
// PROTOCOL CONSTANTS
//=============================================================================

// Vendor ID Registry - Most common vendors
static const struct {
    uint16_t vendor_id;
    const char *vendor_name;
} CIP_VENDORS[] = {
    {1, "Rockwell Automation/Allen-Bradley"},
    {47, "OMRON Corporation"},
    {8, "Molex Incorporated"},
    {26, "Festo SE & Co KG"},
    {29, "OPTO 22"},
    {40, "WAGO Corporation"},
    {108, "Beckhoff Automation"},
    {252, "OMRON Software Co., Ltd."},
    {678, "Cognex Corporation"},
    {808, "SICK AG"},
    {1988, "Unitronics (1989) (RG) LTD"},
    {0, NULL}  // End marker
};

// Device Type Registry - Common device types
static const struct {
    uint16_t device_type;
    const char *device_name;
} CIP_DEVICE_TYPES[] = {
    {0x00, "Generic Device"},
    {0x02, "AC Drive"},
    {0x0C, "Communications Adapter"},
    {0x0E, "Programmable Logic Controller"},
    {0x10, "Position Controller"},
    {0x13, "DC Drive"},
    {0x18, "Human-Machine Interface"},
    {0x25, "CIP Motion Drive"},
    {0x2C, "Managed Switch"},
    {0, NULL}  // End marker
};

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

const char *eip_vendor_id_to_string(uint16_t vendor_id) {
    for (int i = 0; CIP_VENDORS[i].vendor_name != NULL; i++) {
        if (CIP_VENDORS[i].vendor_id == vendor_id) {
            return CIP_VENDORS[i].vendor_name;
        }
    }
    return NULL;
}

const char *eip_device_type_to_string(uint16_t device_type) {
    for (int i = 0; CIP_DEVICE_TYPES[i].device_name != NULL; i++) {
        if (CIP_DEVICE_TYPES[i].device_type == device_type) {
            return CIP_DEVICE_TYPES[i].device_name;
        }
    }
    return NULL;
}

const char *eip_device_state_to_string(uint8_t state) {
    switch (state) {
        case EIP_DEVICE_STATE_NONEXISTENT:
            return "Nonexistent";
        case EIP_DEVICE_STATE_SELF_TESTING:
            return "Self Testing";
        case EIP_DEVICE_STATE_STANDBY:
            return "Standby";
        case EIP_DEVICE_STATE_OPERATIONAL:
            return "Operational";
        case EIP_DEVICE_STATE_MAJOR_FAULT:
            return "Major Recoverable Fault";
        case EIP_DEVICE_STATE_CONFIGURATION:
            return "Configuration";
        case EIP_DEVICE_STATE_WAITING_FOR_RESET:
            return "Waiting for Reset";
        default:
            return "Unknown";
    }
}

//=============================================================================
// CIP SEGMENT OPERATIONS
//=============================================================================

static ptk_err_t cip_segment_serialize_port(ptk_buf *buf, const cip_port_segment_t *seg) {
    return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                           (uint8_t)seg->segment_type,
                           (uint8_t)seg->port_number);
}

static ptk_err_t cip_segment_serialize_class(ptk_buf *buf, const cip_class_segment_t *seg) {
    if (seg->class_id <= 0xFF) {
        return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                               (uint8_t)seg->segment_type,
                               (uint8_t)seg->class_id);
    } else {
        // Extended format
        return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                               (uint8_t)(seg->segment_type | 0x01), // Extended bit
                               (uint8_t)0,                          // Reserved
                               (uint16_t)seg->class_id);
    }
}

static ptk_err_t cip_segment_serialize_instance(ptk_buf *buf, const cip_instance_segment_t *seg) {
    if (seg->instance_id <= 0xFF) {
        return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                               (uint8_t)seg->segment_type,
                               (uint8_t)seg->instance_id);
    } else {
        // Extended format
        return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                               (uint8_t)(seg->segment_type | 0x01), // Extended bit
                               (uint8_t)0,                          // Reserved
                               (uint16_t)seg->instance_id);
    }
}

static ptk_err_t cip_segment_serialize_member(ptk_buf *buf, const cip_member_segment_t *seg) {
    if (seg->member_id <= 0xFF) {
        return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                               (uint8_t)seg->segment_type,
                               (uint8_t)seg->member_id);
    } else {
        // Extended format
        return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                               (uint8_t)(seg->segment_type | 0x01), // Extended bit
                               (uint8_t)0,                          // Reserved
                               (uint16_t)seg->member_id);
    }
}

static ptk_err_t cip_segment_serialize_symbolic(ptk_buf *buf, const cip_symbolic_segment_t *seg) {
    if (!seg->symbol_name || seg->symbol_length == 0) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    uint8_t segment_byte = (uint8_t)seg->segment_type | (uint8_t)(seg->symbol_length & 0x1F);
    
    ptk_err_t err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, segment_byte);
    if (err != PTK_OK) return err;
    
    // Write symbol name
    for (size_t i = 0; i < seg->symbol_length; i++) {
        err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, (uint8_t)seg->symbol_name[i]);
        if (err != PTK_OK) return err;
    }
    
    // Pad to even number of bytes
    if (seg->symbol_length % 2 == 1) {
        err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, (uint8_t)0);
    }
    
    return err;
}

static ptk_err_t cip_segment_serialize_data(ptk_buf *buf, const cip_data_segment_t *seg) {
    if (!seg->data || seg->data_length == 0) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    uint8_t segment_byte = (uint8_t)seg->segment_type | (uint8_t)(seg->data_length & 0x1F);
    
    ptk_err_t err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, segment_byte);
    if (err != PTK_OK) return err;
    
    // Write data
    for (size_t i = 0; i < seg->data_length; i++) {
        err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, seg->data[i]);
        if (err != PTK_OK) return err;
    }
    
    // Pad to even number of bytes
    if (seg->data_length % 2 == 1) {
        err = ptk_buf_serialize(buf, PTK_BUF_NATIVE_ENDIAN, (uint8_t)0);
    }
    
    return err;
}

ptk_err_t cip_segment_serialize(ptk_buf *buf, const cip_segment_u *segment) {
    if (!buf || !segment) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    switch (segment->segment_type) {
        case CIP_SEGMENT_TYPE_PORT:
            return cip_segment_serialize_port(buf, &segment->port);
            
        case CIP_SEGMENT_TYPE_LOGICAL_CLASS:
            return cip_segment_serialize_class(buf, &segment->class);
            
        case CIP_SEGMENT_TYPE_LOGICAL_INSTANCE:
            return cip_segment_serialize_instance(buf, &segment->instance);
            
        case CIP_SEGMENT_TYPE_LOGICAL_MEMBER:
            return cip_segment_serialize_member(buf, &segment->member);
            
        case CIP_SEGMENT_TYPE_SYMBOLIC:
            return cip_segment_serialize_symbolic(buf, &segment->symbolic);
            
        case CIP_SEGMENT_TYPE_DATA:
            return cip_segment_serialize_data(buf, &segment->data);
            
        default:
            return PTK_ERR_INVALID_ARGUMENT;
    }
}

ptk_err_t cip_segment_array_serialize(ptk_buf *buf, const cip_segment_array_t *segments) {
    if (!buf || !segments) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    size_t count = cip_segment_array_len(segments);
    for (size_t i = 0; i < count; i++) {
        const cip_segment_u *segment = cip_segment_array_get(segments, i);
        if (!segment) {
            return PTK_ERR_INVALID_ARGUMENT;
        }
        
        ptk_err_t err = cip_segment_serialize(buf, segment);
        if (err != PTK_OK) {
            return err;
        }
    }
    
    return PTK_OK;
}

//=============================================================================
// CIP PATH PARSING
//=============================================================================

static ptk_err_t parse_path_component(const char **path_str, cip_segment_u *segment) {
    const char *start = *path_str;
    const char *equals = strchr(start, '=');
    const char *comma = strchr(start, ',');
    const char *slash = strchr(start, '/');
    
    // Find the end of this component
    const char *end = NULL;
    if (comma && (!slash || comma < slash)) {
        end = comma;
    } else if (slash) {
        end = slash;
    } else {
        end = start + strlen(start);
    }
    
    if (equals && equals < end) {
        // Named format: "Class=1" or "Instance=2"
        size_t name_len = equals - start;
        char name[32];
        if (name_len >= sizeof(name)) {
            return PTK_ERR_INVALID_ARGUMENT;
        }
        
        strncpy(name, start, name_len);
        name[name_len] = '\0';
        
        uint32_t value = (uint32_t)strtoul(equals + 1, NULL, 0);
        
        if (strcasecmp(name, "class") == 0) {
            segment->segment_type = CIP_SEGMENT_TYPE_LOGICAL_CLASS;
            segment->class.segment_type = CIP_SEGMENT_TYPE_LOGICAL_CLASS;
            segment->class.class_id = value;
        } else if (strcasecmp(name, "instance") == 0) {
            segment->segment_type = CIP_SEGMENT_TYPE_LOGICAL_INSTANCE;
            segment->instance.segment_type = CIP_SEGMENT_TYPE_LOGICAL_INSTANCE;
            segment->instance.instance_id = value;
        } else if (strcasecmp(name, "attribute") == 0 || strcasecmp(name, "member") == 0) {
            segment->segment_type = CIP_SEGMENT_TYPE_LOGICAL_MEMBER;
            segment->member.segment_type = CIP_SEGMENT_TYPE_LOGICAL_MEMBER;
            segment->member.member_id = value;
        } else {
            return PTK_ERR_INVALID_ARGUMENT;
        }
    } else {
        // Numeric format: assume it's a class/instance/member sequence
        uint32_t value = (uint32_t)strtoul(start, NULL, 0);
        
        // Simple heuristic: first number is class, second is instance, third is member
        // This is a simplified parser - real implementation would need more context
        segment->segment_type = CIP_SEGMENT_TYPE_LOGICAL_CLASS;
        segment->class.segment_type = CIP_SEGMENT_TYPE_LOGICAL_CLASS;
        segment->class.class_id = value;
    }
    
    // Advance past this component
    if (*end == ',' || *end == '/') {
        *path_str = end + 1;
    } else {
        *path_str = end;
    }
    
    return PTK_OK;
}

ptk_err_t cip_ioi_path_pdu_create_from_string(cip_segment_array_t *path, const char *path_string) {
    if (!path || !path_string) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Clear the path array
    cip_segment_array_clear(path);
    
    const char *current = path_string;
    while (*current && *current != '\0') {
        // Skip whitespace
        while (*current == ' ' || *current == '\t') {
            current++;
        }
        
        if (*current == '\0') {
            break;
        }
        
        cip_segment_u segment;
        memset(&segment, 0, sizeof(segment));
        
        ptk_err_t err = parse_path_component(&current, &segment);
        if (err != PTK_OK) {
            return err;
        }
        
        err = cip_segment_array_push(path, segment);
        if (err != PTK_OK) {
            return err;
        }
    }
    
    return PTK_OK;
}

//=============================================================================
// PDU SERIALIZATION FUNCTIONS
//=============================================================================

ptk_err_t eip_list_identity_req_serialize(ptk_buf *buf, eip_list_identity_req_t *req) {
    if (!buf || !req) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // ListIdentity request is just an EIP header with no data
    return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN,
                           (uint16_t)0x0063,  // ListIdentity command
                           (uint16_t)0,       // Length (no data)
                           (uint32_t)0,       // Session Handle (0 for unregistered)
                           (uint32_t)0,       // Status
                           (uint64_t)1000,    // Sender Context
                           (uint32_t)0);      // Options
}

ptk_err_t eip_list_identity_resp_serialize(ptk_buf *buf, eip_list_identity_resp_t *resp) {
    // Response serialization is typically handled by servers
    // For now, return not implemented
    return PTK_ERR_NOT_IMPLEMENTED;
}