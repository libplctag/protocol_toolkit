# Simplified PDL Schema for Direct Struct Generation

## Overview

This simplified schema generates C structs with direct field access instead of field-indexed vtables. Key features:

- **Direct field access**: `msg->field_name = value`
- **Discriminated unions** for variant types with `payload_type` enum + `payload_value` union
- **Message type constants** as first field in every struct
- **Safe array accessors** with bounds checking
- **Arena allocators** for message lifetime management

## Generated Structure Pattern

```c
// Global message type enum
typedef enum {
    MESSAGE_TYPE_FOO = 1,
    MESSAGE_TYPE_BAR = 2,
} message_type_t;

// Generated message struct
typedef struct {
    const int message_type;     // Always first field
    // ... visible fields as direct struct members
    field_type_array_t *array_field;  // Arrays as pointers to safe arrays
} message_name_t;

// Constructor/destructor
ptk_err message_name_create(ptk_allocator_t *alloc, message_name_t **msg);
void message_name_dispose(ptk_allocator_t *alloc, message_name_t *msg);

// Encode/decode
ptk_err message_name_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const message_name_t *msg);
ptk_err message_name_decode(ptk_allocator_t *alloc, message_name_t **msg, ptk_buf_t *buf);

// Safe array accessors
ptk_err message_name_get_array_field_element(const message_name_t *msg, size_t index, field_type *value);
ptk_err message_name_set_array_field_element(message_name_t *msg, size_t index, field_type value);
size_t message_name_get_array_field_count(const message_name_t *msg);
```

## Schema Syntax (Simplified)

### Basic Types
```pdl
// Primitive types with byte order
def u16_be = { type: u16, byte_order: [1, 0] }
def u32_le = { type: u32, byte_order: [0, 1, 2, 3] }

// Constants
def MY_CONSTANT = { type: u16_be, const: 0x1234 }
```

### Messages
```pdl
def message_name = {
    type: message,
    match: ($.parent_field == SOME_VALUE),  // Optional discriminator
    fields: [
        field1: u16_be,
        field2: u32_le,
        array_field: u8[field1],           // Simple array sizing
        complex_array: {                   // Complex array sizing
            type: u16[],
            length: {
                decode: (field1 / 2),
                encode: (complex_array.count)
            }
        }
    ]
}
```

### Discriminated Unions
```pdl
def parent_message = {
    type: message,
    fields: [
        discriminator: u8,
        payload: [          // Variant field - generates union
            message_type_a,
            message_type_b,
            message_type_c
        ]
    ]
}
```

**Generated Union:**
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

### Bit Flags (Individual Bit Fields)

Bit flags extract individual bits from a source field as boolean values:

```pdl
def control_register = {
    type: message,
    fields: [
        _control_bits: u16_le,                  // Source field
        start_bit: { type: bit, source_field: _control_bits, source_bit: 0 },
        stop_bit: { type: bit, source_field: _control_bits, source_bit: 1 },
        reset_bit: { type: bit, source_field: _control_bits, source_bit: 2 }
    ]
}
```

**Generated:**
```c
typedef struct control_register_t {
    const int message_type;
    u16 _control_bits;      // Source field
    bool start_bit;         // Virtual bit field
    bool stop_bit;          // Virtual bit field  
    bool reset_bit;         // Virtual bit field
} control_register_t;

// Generated accessors maintain synchronization
static inline void control_register_set_start_bit(control_register_t *msg, bool value);
static inline bool control_register_get_start_bit(const control_register_t *msg);
```

### Bit-Packed Arrays with Container Types

Bit arrays are defined with a specific container type that determines the allocation granularity:

```pdl
// Define bit array types with different container sizes
def u8_based_bit_array = { type: bit[], container_type: u8 }
def u16_based_bit_array = { type: bit[], container_type: u16 }
def u32_based_bit_array = { type: bit[], container_type: u32 }

def coil_message = {
    type: message,
    fields: [
        byte_count: u8,                               // Container count
        coils: u8_based_bit_array[byte_count]         // Bit array with u8 containers
    ]
}
```

**Generated:**
```c
typedef struct {
    const int message_type;
    u8 byte_count;
    u8_bit_array_t *coils;  // Bit array with u8 containers
} coil_message_t;

// Safe accessors for bit arrays - bit-level access
ptk_err coil_message_get_coils_element(const coil_message_t *msg, size_t bit_index, bool *value);
ptk_err coil_message_set_coils_element(coil_message_t *msg, size_t bit_index, bool value);
size_t coil_message_get_coils_count(const coil_message_t *msg);

// Container-level access for direct byte manipulation
ptk_err coil_message_get_coils_container(const coil_message_t *msg, size_t container_index, u8 *value);
ptk_err coil_message_set_coils_container(coil_message_t *msg, size_t container_index, u8 value);
size_t coil_message_get_coils_container_count(const coil_message_t *msg);
```

**Key Benefits:**
- **Container awareness**: Matches wire format exactly (Modbus coils are u8-packed)
- **Efficient access**: Can manipulate whole containers or individual bits
- **Protocol alignment**: u16 containers for 16-bit word-aligned protocols, etc.
- **Length clarity**: `byte_count` refers to container count, not bit count

### Import and Namespaces
```pdl
import common_types = "common/types.pdl"

def my_message = {
    type: message,
    fields: [
        header: common_types.standard_header,
        data: u8[header.length]
    ]
}
```

## Field Attributes Reference

| Attribute | Description | Example |
|-----------|-------------|---------|
| `type` | Field type | `u16`, `message`, `u8[]`, `[msg_a, msg_b]` |
| `byte_order` | Byte order array | `[0, 1]` (little), `[1, 0]` (big) |
| `const` | Constant value | `0x1234` |
| `length` | Array sizing | `{ decode: expr, encode: expr }` |
| `match` | Discriminator | `($.field == VALUE)` |
| `container_type` | Bit array container | `u8`, `u16_le`, `u32_le` |
| `source_field` | Bit flag source | `field_name` |
| `source_bit` | Bit flag position | `0`, `1`, `2`, etc. |

## Simplifications from Original Schema

1. **Removed vtables**: Direct struct access instead of function pointers
2. **Removed field indexing**: No dynamic offset tracking needed
3. **Removed encoded_data storage**: Structs hold decoded values
4. **Simplified expressions**: Only basic operators for length calculations
5. **Removed visibility/representation**: All fields are concrete and visible
6. **Single allocator per message**: No complex memory hierarchies
7. **Container-type bit arrays**: Bit arrays use underlying container arrays with generated accessors

## Array Sizing Shortcuts

- **Constant**: `u8[256]`
- **Field reference**: `u8[length_field]` 
- **Complex**: Use `length: { decode: expr, encode: expr }`

## Usage Example

```c
// Create message
read_coils_response_t *response;
read_coils_response_create(arena_alloc, &response);

// Set fields directly
response->byte_count = 3;

// Allocate and set bit array
bit_array_resize(response->coil_status, 20);  // 20 coils
read_coils_response_set_coils_element(response, 5, true);   // Coil 5 ON
read_coils_response_set_coils_element(response, 10, false); // Coil 10 OFF

// Encode to buffer
ptk_buf_t *send_buf;
ptk_buf_create(malloc_alloc, &send_buf, 1024);
read_coils_response_encode(arena_alloc, send_buf, response);

// Cleanup
alloc_reset(arena_alloc);  // Frees all arena-allocated memory
ptk_buf_dispose(malloc_alloc, send_buf);
```

This simplified approach provides the benefits of direct struct access while maintaining type safety and memory management through allocators and safe array accessors.