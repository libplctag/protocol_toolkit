# PTK-Gen: Protocol Toolkit Code Generator Design

## Overview

PTK-Gen is a code generator that transforms Protocol Definition Language (PDL) files into type-safe C code for parsing and generating binary protocol messages. It generates direct struct access patterns with safe array handling and bit field manipulation for industrial communication protocols.

## Key Features

- **Direct Struct Access**: Generated structs with direct field access (no vtables)
- **Bounds Safety**: Bounds-checked array accessors using PTK_ARRAY patterns
- **Bit Fields**: Individual bit access and multi-bit field extraction from raw data
- **Memory Safety**: Allocator-based memory management with clear ownership
- **Protocol Agnostic**: Support for different endianness, container types, and bit patterns
- **DRY Principles**: Minimal repetition through semantic types and response linking

## PDL Language Design

### Basic Type System

#### Primitive Types
```pdl
// Basic types with explicit byte ordering
def u8 = { type: u8 }
def u16_be = { type: u16, byte_order: [1, 0] }
def u16_le = { type: u16, byte_order: [0, 1] }
def u32_be = { type: u32, byte_order: [3, 2, 1, 0] }
def u32_le = { type: u32, byte_order: [0, 1, 2, 3] }
```

#### Semantic Types for Clarity
```pdl
// Define semantic meaning for same underlying types
def modbus_register = u16_be
def modbus_address = u16_be
def modbus_quantity = u16_be
def tcp_port = u16_be
```

#### Constants
```pdl
def MODBUS_FC_READ_COILS = { type: u8, const: 0x01 }
def MODBUS_FC_READ_HOLDING_REGISTERS = { type: u8, const: 0x03 }
def EIP_SEND_RR_DATA = { type: u16_le, const: 0x006F }
```

#### Array Types with Custom Byte Ordering
```pdl
// Arrays can have custom byte ordering for complex wire formats
def plc5_string = { 
    type: u8[82], 
    byte_order: [1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30, 33, 32, 35, 34, 37, 36, 39, 38, 41, 40, 43, 42, 45, 44, 47, 46, 49, 48, 51, 50, 53, 52, 55, 54, 57, 56, 59, 58, 61, 60, 63, 62, 65, 64, 67, 66, 69, 68, 71, 70, 73, 72, 75, 74, 77, 76, 79, 78, 81, 80]
}

def siemens_word_array = {
    type: u8[20],
    byte_order: [1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16, 19, 18]
}

// Validation rules:
// - For type: uN, byte_order length must equal N/8
// - For type: u8[N], byte_order length must equal N
// - No shortcuts or patterns - explicit arrays only
```

### Array Types

#### Simple Arrays with PTK_ARRAY Generation
```pdl
// These generate separate PTK_ARRAY_DECLARE statements
def modbus_register_array = modbus_register[]
def modbus_address_array = modbus_address[]
def tcp_port_array = tcp_port[]
```

#### Bit Arrays with Container Types
```pdl
// Container type determines allocation granularity and wire format
def modbus_coil_array = { type: bit[], container_type: u8 }
def modbus_discrete_input_array = { type: bit[], container_type: u8 }
def rockwell_bit_array = { type: bit[], container_type: u32_le }
def omron_bit_array = { type: bit[], container_type: u16_le }
```

### Message Definitions

#### Basic Message Structure
```pdl
def message_name = {
    type: message,
    match: (condition_expression),     // Optional discriminator
    response_to: request_type,         // Optional response linking
    fields: [
        field1: type_reference,
        field2: { type: u8, hidden: true },
        field3: array_type[size_expression]
    ]
}
```

#### Request/Response Linking
```pdl
// Request (no response_to field)
def modbus_read_coils_req = {
    type: message,
    match: (function_code == MODBUS_FC_READ_COILS),
    fields: [
        function_code: u8,
        starting_address: modbus_address,
        quantity_of_coils: modbus_quantity
    ]
}

// Response (with response_to field)
def modbus_read_coils_resp = {
    type: message,
    match: (function_code == MODBUS_FC_READ_COILS),
    response_to: modbus_read_coils_req,
    fields: [
        function_code: u8,
        coil_status: modbus_coil_array[$.request.quantity_of_coils]
    ]
}

// Exception response (responds to any request)
def modbus_exception_resp = {
    type: message,
    match: (function_code >= 0x81 && function_code <= 0x90),
    response_to: ALL,
    fields: [
        exception_function_code: u8,
        exception_code: u8
    ]
}
```

### Bit Field Extraction

#### Individual Bit Flags
```pdl
def status_register_pdu = {
    type: message,
    fields: [
        _status_bits: { type: u8, hidden: true },
        ready_flag: { type: bit, container: _status_bits, bits: [0] },
        error_flag: { type: bit, container: _status_bits, bits: [1] },
        busy_flag: { type: bit, container: _status_bits, bits: [2] },
        enabled_flag: { type: bit, container: _status_bits, bits: [4] }
    ]
}
```

#### Multi-bit Fields (Contiguous and Discontiguous)
```pdl
def control_register_pdu = {
    type: message,
    fields: [
        _control_word: { type: u16_be, hidden: true },
        mode: { type: bit_field, container: _control_word, bits: [2, 1, 0] },        // 3-bit field
        speed: { type: bit_field, container: _control_word, bits: [7, 6, 5, 4] },    // 4-bit field
        priority: { type: bit_field, container: _control_word, bits: [15, 10, 8] },  // discontiguous!
        enable_flag: { type: bit, container: _control_word, bits: [14] }
    ]
}
```

### Array Sizing

#### Simple Array Sizing
```pdl
def simple_arrays = {
    type: message,
    fields: [
        length: u16_le,
        data: u8[length],                    // Direct field reference
        fixed_data: u8[256],                 // Constant size
        item_count: u16_le,
        headers: message_header[item_count]  // Direct field reference
    ]
}
```

#### Complex Array Sizing with Length Expressions
```pdl
def complex_arrays = {
    type: message,
    fields: [
        total_length: u16_le,
        header_size: u8,
        payload: {
            type: u8[],
            length: {
                decode: (total_length - header_size - 2),  // 2 = sizeof(u16_le)
                encode: (payload.count)
            }
        }
    ]
}
```

### Discriminated Unions

#### Variant Fields
```pdl
def parent_message = {
    type: message,
    fields: [
        discriminator: u8,
        payload: [
            message_type_a,
            message_type_b,
            message_type_c
        ]
    ]
}
```

### Import System

#### Namespace Imports
```pdl
import eip_common = "eip_common.pdl"
import modbus_types = "modbus/types.pdl"

def my_message = {
    type: message,
    fields: [
        header: eip_common.eip_header,
        modbus_data: modbus_types.holding_reg,
        local_field: u32_le
    ]
}
```

### Field References

#### Parent Field Access
```pdl
def nested_msg = {
    type: message,
    match: ($.header.command == SOME_COMMAND),
    response_to: parent_request,
    fields: [
        sub_length: u16_le,
        sub_data: {
            type: u8[],
            length: {
                decode: ($.request.total_length - sub_length),
                encode: (sub_data.count)
            }
        }
    ]
}
```

### Expression Language

#### Supported Operators
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Logical: `&&`, `||`, `!`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Ternary: `condition ? true_value : false_value`
- Grouping: `(expression)`

#### Field References in Expressions
- `field_name` - Reference to field in current message
- `$.parent_field` - Reference to parent message field
- `$.request.field_name` - Reference to request field from response
- `array_field.count` - Get array element count
- `bit_array.container_count` - Get container count for bit arrays

## Code Generation Patterns

### Generated Struct Patterns

#### Basic Message Struct
```c
typedef struct message_name_t {
    const int message_type;     // Always first field
    // ... visible fields as direct struct members
    type_array_t *array_field;  // Arrays as pointers to PTK_ARRAY types
} message_name_t;
```

#### PTK_ARRAY Generation
```c
// From modbus_register_array definition
PTK_ARRAY_DECLARE(modbus_register, uint16_t);

// From tcp_port_array definition  
PTK_ARRAY_DECLARE(tcp_port, uint16_t);
```

#### Bit Field Structs
```c
typedef struct status_register_pdu_t {
    const int message_type;
    // _status_bits hidden - only exists in raw/encoded data
    bool ready_flag;
    bool error_flag;
    bool busy_flag;
    bool enabled_flag;
} status_register_pdu_t;
```

#### Discriminated Union Structs
```c
typedef enum {
    PARENT_MESSAGE_PAYLOAD_TYPE_MESSAGE_TYPE_A,
    PARENT_MESSAGE_PAYLOAD_TYPE_MESSAGE_TYPE_B,
    PARENT_MESSAGE_PAYLOAD_TYPE_MESSAGE_TYPE_C,
} parent_message_payload_type_t;

typedef struct {
    parent_message_payload_type_t payload_type;
    union {
        message_type_a_t *message_type_a;
        message_type_b_t *message_type_b;
        message_type_c_t *message_type_c;
    } payload_value;
} parent_message_payload_t;

typedef struct {
    const int message_type;
    u8 discriminator;
    parent_message_payload_t payload;
} parent_message_t;
```

### Generated Function Patterns

#### Constructor/Destructor
```c
ptk_err message_name_create(ptk_allocator_t *alloc, message_name_t **msg);
void message_name_dispose(ptk_allocator_t *alloc, message_name_t *msg);
```

#### Encode/Decode
```c
ptk_err message_name_encode(ptk_allocator_t *alloc, ptk_buf *buf, const message_name_t *msg);
ptk_err message_name_decode(ptk_allocator_t *alloc, message_name_t **msg, ptk_buf *buf);
```

#### Bit Array Accessors
```c
// Bit-level access
ptk_err message_name_get_array_field_element(const message_name_t *msg, size_t bit_index, bool *value);
ptk_err message_name_set_array_field_element(message_name_t *msg, size_t bit_index, bool value);
size_t message_name_get_array_field_count(const message_name_t *msg);

// Container-level access for bulk operations
ptk_err message_name_get_array_field_container(const message_name_t *msg, size_t container_index, u8 *value);
ptk_err message_name_set_array_field_container(message_name_t *msg, size_t container_index, u8 value);
size_t message_name_get_array_field_container_count(const message_name_t *msg);
```

### Bit Field Encoding/Decoding

#### Single Bit Fields
```c
// During decode: extract from hidden raw data
static ptk_err status_register_pdu_decode_fields(status_register_pdu_t *msg, uint8_t raw_status_bits) {
    msg->ready_flag = (raw_status_bits >> 0) & 0x01;
    msg->error_flag = (raw_status_bits >> 1) & 0x01;
    msg->busy_flag = (raw_status_bits >> 2) & 0x01;
    msg->enabled_flag = (raw_status_bits >> 4) & 0x01;
    return PTK_OK;
}

// During encode: reconstruct hidden raw data
static uint8_t status_register_pdu_encode_fields(const status_register_pdu_t *msg) {
    uint8_t raw_status_bits = 0;
    if (msg->ready_flag) raw_status_bits |= (1 << 0);
    if (msg->error_flag) raw_status_bits |= (1 << 1);
    if (msg->busy_flag) raw_status_bits |= (1 << 2);
    if (msg->enabled_flag) raw_status_bits |= (1 << 4);
    return raw_status_bits;
}
```

#### Multi-bit and Discontiguous Fields
```c
// During decode: extract discontiguous bits [15, 10, 8] into priority field
static ptk_err control_register_pdu_decode_priority(control_register_pdu_t *msg, uint16_t raw_control_word) {
    msg->priority = ((raw_control_word >> 15) & 0x01) << 2 |
                   ((raw_control_word >> 10) & 0x01) << 1 |
                   ((raw_control_word >> 8) & 0x01) << 0;
    return PTK_OK;
}

// During encode: spread priority bits into [15, 10, 8]
static void control_register_pdu_encode_priority(const control_register_pdu_t *msg, uint16_t *raw_control_word) {
    if (msg->priority & 0x04) *raw_control_word |= (1 << 15);
    if (msg->priority & 0x02) *raw_control_word |= (1 << 10);
    if (msg->priority & 0x01) *raw_control_word |= (1 << 8);
}
```

### Request/Response Handling

#### Response Type Determination
```c
typedef enum {
    MODBUS_RESPONSE_NONE,
    MODBUS_RESPONSE_NORMAL,
    MODBUS_RESPONSE_EXCEPTION
} modbus_response_type_t;

// Generated lookup table
static const modbus_response_mapping_t response_table[] = {
    { MODBUS_READ_COILS_REQ_TYPE, MODBUS_READ_COILS_RESP_TYPE },
    { MODBUS_WRITE_SINGLE_COIL_REQ_TYPE, MODBUS_WRITE_SINGLE_COIL_RESP_TYPE },
    { ALL_REQUEST_TYPES, MODBUS_EXCEPTION_RESP_TYPE },
};

modbus_response_type_t modbus_get_expected_response_type(uint8_t function_code) {
    // Generated switch based on response_to: relationships
}
```

#### PDU Send Function
```c
modbus_pdu_base_t *modbus_pdu_send(modbus_pdu_base_t **pdu, ptk_duration_ms timeout_ms) {
    // Take ownership of PDU
    modbus_pdu_base_t *request = *pdu;
    *pdu = NULL;
    
    // Send request
    // ...
    
    // Check if response expected based on response_to: mappings
    modbus_response_type_t response_type = modbus_get_expected_response_type(request_function_code);
    if (response_type == MODBUS_RESPONSE_NONE) {
        return NULL;
    }
    
    // Receive and match response
    modbus_pdu_u response = modbus_pdu_recv(conn, timeout_ms);
    return response.base;
}
```

## Field Attributes Reference

| Attribute | Description | Example |
|-----------|-------------|---------|
| `type` | Field type | `u16`, `message`, `u8[]`, `u8[82]`, `[msg_a, msg_b]` |
| `byte_order` | Byte order array for primitives/arrays | `[0, 1]` (little), `[1, 0]` (big), `[1,0,3,2,...]` (custom) |
| `const` | Constant value | `0x1234` |
| `hidden` | Hide from generated struct | `true`, `false` |
| `length` | Complex array sizing | `{ decode: expr, encode: expr }` |
| `match` | Message discriminator | `(field == VALUE)` |
| `response_to` | Response linking | `request_type`, `ALL` |
| `container_type` | Bit array container | `u8`, `u16_le`, `u32_le` |
| `container` | Bit field source | `field_name` |
| `bits` | Bit positions | `[0]`, `[2, 1, 0]`, `[15, 10, 8]` |

## Modbus Protocol Example

### Complete Modbus PDL Definition
```pdl
// Basic types
def modbus_register = u16_be
def modbus_address = u16_be
def modbus_quantity = u16_be

// Array types
def modbus_register_array = modbus_register[]
def modbus_coil_array = { type: bit[], container_type: u8 }
def modbus_discrete_input_array = { type: bit[], container_type: u8 }

// Constants
def MODBUS_FC_READ_COILS = { type: u8, const: 0x01 }
def MODBUS_FC_READ_DISCRETE_INPUTS = { type: u8, const: 0x02 }
def MODBUS_FC_READ_HOLDING_REGISTERS = { type: u8, const: 0x03 }
def MODBUS_FC_READ_INPUT_REGISTERS = { type: u8, const: 0x04 }
def MODBUS_FC_WRITE_SINGLE_COIL = { type: u8, const: 0x05 }
def MODBUS_FC_WRITE_SINGLE_REGISTER = { type: u8, const: 0x06 }

// Read Coils
def modbus_read_coils_req = {
    type: message,
    match: (function_code == MODBUS_FC_READ_COILS),
    fields: [
        function_code: u8,
        starting_address: modbus_address,
        quantity_of_coils: modbus_quantity
    ]
}

def modbus_read_coils_resp = {
    type: message,
    match: (function_code == MODBUS_FC_READ_COILS),
    response_to: modbus_read_coils_req,
    fields: [
        function_code: u8,
        coil_status: modbus_coil_array[$.request.quantity_of_coils]
    ]
}

// Read Holding Registers
def modbus_read_holding_registers_req = {
    type: message,
    match: (function_code == MODBUS_FC_READ_HOLDING_REGISTERS),
    fields: [
        function_code: u8,
        starting_address: modbus_address,
        quantity_of_registers: modbus_quantity
    ]
}

def modbus_read_holding_registers_resp = {
    type: message,
    match: (function_code == MODBUS_FC_READ_HOLDING_REGISTERS),
    response_to: modbus_read_holding_registers_req,
    fields: [
        function_code: u8,
        register_values: modbus_register_array[$.request.quantity_of_registers]
    ]
}

// Write Single Coil
def modbus_write_single_coil_req = {
    type: message,
    match: (function_code == MODBUS_FC_WRITE_SINGLE_COIL),
    fields: [
        function_code: u8,
        output_address: modbus_address,
        output_value: u16_be
    ]
}

def modbus_write_single_coil_resp = {
    type: message,
    match: (function_code == MODBUS_FC_WRITE_SINGLE_COIL),
    response_to: modbus_write_single_coil_req,
    fields: [
        function_code: u8,
        output_address: modbus_address,
        output_value: u16_be
    ]
}

// Exception Response
def modbus_exception_resp = {
    type: message,
    match: (function_code >= 0x81 && function_code <= 0x90),
    response_to: ALL,
    fields: [
        exception_function_code: u8,
        exception_code: u8
    ]
}

// Root PDU Union
def modbus_pdu = {
    type: message,
    fields: [
        pdu_data: [
            modbus_read_coils_req,
            modbus_read_coils_resp,
            modbus_read_holding_registers_req,
            modbus_read_holding_registers_resp,
            modbus_write_single_coil_req,
            modbus_write_single_coil_resp,
            modbus_exception_resp
        ]
    ]
}
```

### Generated C Code Structure
```c
// Generated constants
#define MODBUS_FC_READ_COILS 0x01
#define MODBUS_FC_READ_HOLDING_REGISTERS 0x03
// ...

// Generated array types
PTK_ARRAY_DECLARE(modbus_register, uint16_t);
PTK_ARRAY_DECLARE(modbus_address, uint16_t);
PTK_ARRAY_DECLARE(modbus_quantity, uint16_t);

// Generated message types
typedef enum {
    MODBUS_READ_COILS_REQ_TYPE = __LINE__,
    MODBUS_READ_COILS_RESP_TYPE = (MODBUS_PDU_TYPE_RESPONSE_FLAG | __LINE__),
    // ...
} modbus_message_type_t;

// Generated structs
typedef struct modbus_read_coils_req_t {
    modbus_pdu_base_t base;
    uint8_t function_code;
    uint16_t starting_address;
    uint16_t quantity_of_coils;
} modbus_read_coils_req_t;

typedef struct modbus_read_coils_resp_t {
    modbus_pdu_base_t base;
    uint8_t function_code;
    modbus_bit_array_t *coil_status;
} modbus_read_coils_resp_t;

// Generated union
typedef union modbus_pdu_u {
    modbus_pdu_base_t *base;
    struct modbus_read_coils_req_t *read_coils_req;
    struct modbus_read_coils_resp_t *read_coils_resp;
    // ...
} modbus_pdu_u;
```

## Implementation Strategy

### Parser Implementation
- Use Lark parser generator for robust PDL parsing
- Python 3.8+ implementation for broad compatibility
- Separate lexical analysis and semantic analysis phases

### Code Generation
- Jinja2 templates for C code generation
- Separate templates for headers (.h) and implementation (.c)
- Template inheritance for common patterns

### Type System
- Static type checking during parse phase
- Semantic type validation (array sizing, bit field ranges)
- Cross-reference validation for response_to: relationships

### Memory Management
- Arena allocator patterns for message lifetime management
- Clear ownership semantics with double-pointer parameters
- Automatic cleanup on dispose functions

## Benefits

### For Protocol Developers
- **Declarative**: Describe protocol structure, not implementation details
- **Type Safe**: Compiler catches array bounds and type errors
- **Maintainable**: Changes to protocol require only PDL updates
- **Readable**: Self-documenting protocol definitions

### For Application Developers  
- **Direct Access**: `msg->field_name = value` syntax
- **Safe Arrays**: Bounds checking on all array operations
- **Clean Types**: Boolean flags instead of raw bit manipulation
- **Memory Safe**: Allocator-based lifetime management

### For Generated Code
- **Efficient**: Direct struct access with no vtable overhead
- **Compact**: Only visible fields in structs
- **Standard**: Uses existing PTK_ARRAY infrastructure
- **Portable**: Pure C with no external dependencies
- **Protocol Accurate**: Handles complex byte arrangements like PLC5 word-swapped strings

## Array Byte Ordering Examples

### Industrial Protocol String Handling

#### Rockwell PLC5 Word-Swapped Strings
```pdl
// PLC5 stores strings as 16-bit words in big-endian order
// Bytes are arranged as [1,0,3,2,5,4,...] for 82-character strings
def plc5_string = { 
    type: u8[82], 
    byte_order: [1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30, 33, 32, 35, 34, 37, 36, 39, 38, 41, 40, 43, 42, 45, 44, 47, 46, 49, 48, 51, 50, 53, 52, 55, 54, 57, 56, 59, 58, 61, 60, 63, 62, 65, 64, 67, 66, 69, 68, 71, 70, 73, 72, 75, 74, 77, 76, 79, 78, 81, 80]
}

def plc5_tag_read_resp = {
    type: message,
    response_to: plc5_tag_read_req,
    fields: [
        tag_type: u16_le,
        element_count: u16_le,
        tag_name: plc5_string    // Automatically handles byte swapping
    ]
}
```

#### Generated Code for Array Byte Ordering
```c
typedef struct plc5_tag_read_resp_t {
    const int message_type;
    uint16_t tag_type;
    uint16_t element_count;
    uint8_t tag_name[82];  // Application sees logical byte order
} plc5_tag_read_resp_t;

// Generated encode function applies byte_order mapping
static ptk_err plc5_tag_read_resp_encode_tag_name(ptk_buf *buf, const uint8_t *tag_name) {
    uint8_t wire_order[82];
    
    // Apply byte_order: wire_order[i] = tag_name[byte_order[i]]
    wire_order[0] = tag_name[1];   // byte_order[0] = 1
    wire_order[1] = tag_name[0];   // byte_order[1] = 0
    wire_order[2] = tag_name[3];   // byte_order[2] = 3
    wire_order[3] = tag_name[2];   // byte_order[3] = 2
    // ... continue for all 82 bytes
    
    return ptk_buf_append(buf, wire_order, 82);
}

// Generated decode function reverses byte_order mapping  
static ptk_err plc5_tag_read_resp_decode_tag_name(ptk_buf *buf, uint8_t *tag_name) {
    uint8_t wire_data[82];
    ptk_err err = ptk_buf_consume(buf, wire_data, 82);
    if (err != PTK_OK) return err;
    
    // Apply reverse mapping: tag_name[byte_order[i]] = wire_data[i]
    tag_name[1] = wire_data[0];    // byte_order[0] = 1 -> tag_name[1] = wire_data[0]
    tag_name[0] = wire_data[1];    // byte_order[1] = 0 -> tag_name[0] = wire_data[1]  
    tag_name[3] = wire_data[2];    // byte_order[2] = 3 -> tag_name[3] = wire_data[2]
    tag_name[2] = wire_data[3];    // byte_order[3] = 2 -> tag_name[2] = wire_data[3]
    // ... continue for all 82 bytes
    
    return PTK_OK;
}
```

#### Usage Example
```c
// Application code sees normal string bytes in logical order
plc5_tag_read_resp_t *resp;
plc5_tag_read_resp_create(alloc, &resp);

// Set string data in normal byte order
strcpy((char*)resp->tag_name, "MAIN:MyTag.Value");

// Encode automatically applies PLC5 word-swapping to wire format
ptk_buf *send_buf;
plc5_tag_read_resp_encode(alloc, send_buf, resp);

// Wire data has bytes arranged as [1,0,3,2,5,4,...] per PLC5 format
```

### Validation Rules

#### Byte Order Length Validation
```pdl
// VALID: Primitive types - byte_order length equals type size in bytes
def u16_be = { type: u16, byte_order: [1, 0] }           // OK: 2 elements for u16
def u32_le = { type: u32, byte_order: [0, 1, 2, 3] }     // OK: 4 elements for u32

// VALID: Array types - byte_order length equals array length  
def custom_array = { type: u8[10], byte_order: [1, 0, 3, 2, 5, 4, 7, 6, 9, 8] }  // OK: 10 elements

// INVALID: Mismatched lengths
def bad_u16 = { type: u16, byte_order: [1, 0, 3, 2] }    // ERROR: 4 elements for u16
def bad_array = { type: u8[5], byte_order: [1, 0, 3] }   // ERROR: 3 elements for u8[5]
```

#### Helper Script Approach
For complex patterns, use external tooling:

```python
# generate_plc5_pattern.py - Not part of PDL syntax
def generate_plc5_byte_order(length):
    """Generate PLC5 word-swap pattern for given length."""
    order = []
    for i in range(0, length, 2):
        if i + 1 < length:
            order.extend([i + 1, i])  # Swap each 16-bit word
        else:
            order.append(i)           # Handle odd byte at end
    return order

# Generate pattern and copy into PDL definition
print(f"byte_order: {generate_plc5_byte_order(82)}")
```

This approach maintains KISS and DRY principles while handling the most complex industrial protocol byte arrangements through explicit, validated definitions.