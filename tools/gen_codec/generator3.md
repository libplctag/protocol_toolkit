# Protocol Description Language v3 (PDL3) Generator Specification

A comprehensive specification for generating industrial protocol encode/decode functions from PDL protocol definitions using the protocol toolkit APIs.

## Overview

The PDL3 generator reads protocol definition files written in PDL syntax and produces C header and source files with complete encode/decode functions using the protocol toolkit's buffer, error handling, and logging APIs.

## Schema and Syntax

### Field Definition

Fields can be defined in two forms:

**Shorthand Form:**
```pdl
command: u16
length: u8
```

**Structured Form:**
```pdl
command: {
  type: u16,
  validate: command >= 0x0065 && command <= 0x0070,
  post_decode: validate_command
}

session_handle: {
  type: u32,
  const: 0,  // For requests
  pre_encode: generate_session_handle
}
```

### Type Aliases

Define reusable type patterns, byte orderings, and constants:

```pdl
aliases: {
  u16_le: { type: u16, byte_order_map: [0, 1] },
  u16_be: { type: u16, byte_order_map: [1, 0] },
  u32_be: { type: u32, byte_order_map: [3, 2, 1, 0] },

  // Constant aliases for self-documenting protocols
  modbus_protocol_version: { type: u16_be, const: 0 },
  func_read_coils: { type: u8, const: 0x01 },
  func_read_discrete_inputs: { type: u8, const: 0x02 },
  func_read_holding_registers: { type: u8, const: 0x03 },
  func_read_input_registers: { type: u8, const: 0x04 },
  func_write_single_coil: { type: u8, const: 0x05 },
  func_write_single_register: { type: u8, const: 0x06 },
  func_write_multiple_coils: { type: u8, const: 0x0F },
  func_write_multiple_registers: { type: u8, const: 0x10 }
}
```

**Generated Constants:**
The generator creates `#define` entries in the generated header for all constant aliases. The constant names are converted to uppercase with underscores:

```c
// Generated constants from aliases (names converted to uppercase)
#define MODBUS_PROTOCOL_VERSION 0
#define FUNC_READ_COILS 0x01
#define FUNC_READ_DISCRETE_INPUTS 0x02
#define FUNC_READ_HOLDING_REGISTERS 0x03
#define FUNC_READ_INPUT_REGISTERS 0x04
#define FUNC_WRITE_SINGLE_COIL 0x05
#define FUNC_WRITE_SINGLE_REGISTER 0x06
#define FUNC_WRITE_MULTIPLE_COILS 0x0F
#define FUNC_WRITE_MULTIPLE_REGISTERS 0x10
```

**Constant Naming Rules:**
- Alias name `modbus_protocol_version` becomes `#define MODBUS_PROTOCOL_VERSION`
- Alias name `func_read_coils` becomes `#define FUNC_READ_COILS`
- All underscores are preserved, letters converted to uppercase

**Usage in Generated Code:**
These constants replace raw numbers throughout the generated code:
- Validation: `if (field != FUNC_READ_COILS)`
- Encoding: `ptk_buf_produce_u8(buf, FUNC_READ_COILS)`
- Error messages: `error("Expected %u, got %u", FUNC_READ_COILS, actual_value)`

This provides better readability, maintainability, and self-documenting code.

### Discriminated Unions

Handle protocol variants using `type_choice` with arrays:

```pdl
ModbusTCPRequest: {
  header: MBAPHeader,
  pdu: {
    type_choice: [
      ReadCoilsRequest,
      ReadDiscreteInputsRequest,
      ReadHoldingRegistersRequest,
      ReadInputRegistersRequest,
      WriteSingleCoilRequest,
      WriteSingleRegisterRequest,
      WriteMultipleCoilsRequest,
      WriteMultipleRegistersRequest
    ],
    default: UnknownRequest
  }
}
```

The messages in the choice list are attempted one at a time until one passes decode.  If none pass, then the default is used.

### Variable-Length Arrays

```pdl
WriteMultipleCoilsRequest: {
  function_code: func_write_multiple_coils,
  address: u16_be,
  quantity: u16_be,
  byte_count: u8,
  data: u8[byte_count]
}
```

### Field Constraints and Validation

- **Validation constraints**: `field: { type: type_name, validate: constraint_expression }`
- **Constant values**: `field: constant_alias` or `field: { type: type_name, const: value }`
- **Auto-sizing**: `field: { type: type_name, auto_size: "target_field" }`
- **Size relationships**: `field: type_name[size_field]`

### Function Hooks

- **pre_decode**: Called before field is decoded
- **post_decode**: Called after field is decoded (can validate/modify)
- **pre_encode**: Called before field is encoded (can validate/modify)
- **post_encode**: Called after field is encoded

## Generator Inputs and Outputs

### Input

**Primary Input:**
- **Protocol Definition File**: `.pdl` file containing protocol specification
- **File Format**: PDL syntax as specified above

**Example Input File:**
```
/Users/kyle/Projects/protocol_toolkit/tools/gen_codec/modbus.pdl
```

### Outputs

The generator produces three output files:

#### 1. Generated Header (`protocol_generated.h`)
Contains:
- Generated type definitions (structs, enums)
- Function declarations for encode/decode/validate operations
- Auto-generated size calculation macros

#### 2. Generated Source (`protocol_generated.c`)
Contains:
- Implementation of all encode/decode functions
- Validation logic implementation
- Buffer manipulation using ptk_buf_t APIs

#### 3. User Functions Header (`protocol_user.h`)
Contains:
- Declarations for user-implemented hook functions
- Function signatures for custom validation/encoding

## Generated Function Signatures

All generated functions follow consistent patterns using the protocol toolkit APIs.

### Allocator Interface

All generated functions accept a pluggable allocator to enable custom memory management strategies. The allocator interface is defined in `ptk_alloc.h`:

```c
#include "ptk_alloc.h"

// Generic allocator interface:
typedef struct ptk_allocator {
    const ptk_allocator_vtable_t *vtable;  // Function pointer table
    void *context;                         // Allocator-specific context
} ptk_allocator_t;

// All allocators use this same structure, hiding implementation details
// behind the vtable and context pointer.

// Three allocator implementations provided:
// 1. PTK_SYSTEM_ALLOCATOR - Standard malloc/free (reset is no-op)
// 2. Debug allocator - Tracks all allocations, detects leaks, provides stats
// 3. Arena allocator - Fast allocation with simple bulk cleanup

// Initialization functions return generic ptk_allocator_t:
ptk_err ptk_debug_allocator_init(ptk_allocator_t *allocator);
ptk_err ptk_arena_allocator_init(ptk_allocator_t *allocator, void *memory, size_t size);
```

**Example: Arena Allocation for Modbus Coil Processing**
```c
void example_modbus_coil_processing() {
    // Allocate arena for up to 2000 coils (worst case)
    static uint8_t arena_memory[8192];  // 2000 bools + overhead
    ptk_allocator_t alloc;
    ptk_arena_allocator_init(&alloc, arena_memory, sizeof(arena_memory));

    // Decode Modbus message using arena allocator
    modbus_tcp_request_t request;
    ptk_err err = modbus_tcp_request_decode(&alloc, &request, buffer, context);

    // Process coils - all allocations use arena
    if (err == PTK_OK) {
        // Work with decoded data - arrays allocated from arena
        process_coil_data(&request);
    }

    // Simple cleanup - reset arena (no individual frees needed)
    ptk_reset(&alloc);
    ptk_arena_allocator_destroy(&alloc);
}
```

**Example: Debug Allocation for Leak Detection**
```c
void example_debug_allocation() {
    ptk_allocator_t alloc;
    ptk_debug_allocator_init(&alloc);

    // Decode and process protocol messages
    modbus_tcp_request_t request;
    ptk_err err = modbus_tcp_request_decode(&alloc, &request, buffer, context);

    // Check for memory leaks
    if (ptk_debug_allocator_has_leaks(&alloc)) {
        ptk_debug_allocator_report(&alloc);  // Print leak details
    }

    // Get memory usage statistics
    ptk_alloc_stats_t stats;
    ptk_get_stats(&alloc, &stats);
    printf("Peak memory usage: %zu bytes\n", stats.peak_allocated);
    printf("Total allocations: %zu\n", stats.total_allocations);
    printf("Total frees: %zu\n", stats.total_frees);
    printf("Active allocations: %zu\n", stats.active_allocations);

    // Clean up any remaining allocations
    ptk_reset(&alloc);
    ptk_debug_allocator_destroy(&alloc);
}
```

### Core Function Types

#### Decode Functions
```c
ptk_err <message_name>_decode(const ptk_allocator_t *alloc, <message_type>_t *msg, ptk_buf_t *src, void *context);
```

#### Encode Functions
```c
ptk_err <message_name>_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const <message_type>_t *msg, void *context);
```

#### Validation Functions
```c
ptk_err <message_name>_validate(const ptk_allocator_t *alloc, const <message_type>_t *msg, void *context);
```

#### Disposal Functions
```c
void <message_name>_dispose(const ptk_allocator_t *alloc, <message_type>_t *msg, void *context);
```

### Example Function Signatures (from modbus.pdl)

```c
// MBAPHeader functions
ptk_err mbap_header_decode(const ptk_allocator_t *alloc, mbap_header_t *header, ptk_buf_t *src, void *context);
ptk_err mbap_header_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const mbap_header_t *header, void *context);
ptk_err mbap_header_validate(const ptk_allocator_t *alloc, const mbap_header_t *header, void *context);
void mbap_header_dispose(const ptk_allocator_t *alloc, mbap_header_t *header, void *context);

// ReadBitsRequest functions
ptk_err read_bits_request_decode(const ptk_allocator_t *alloc, read_bits_request_t *req, ptk_buf_t *src, void *context);
ptk_err read_bits_request_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const read_bits_request_t *req, void *context);
ptk_err read_bits_request_validate(const ptk_allocator_t *alloc, const read_bits_request_t *req, void *context);
void read_bits_request_dispose(const ptk_allocator_t *alloc, read_bits_request_t *req, void *context);

// ModbusTCPRequest (discriminated union)
ptk_err modbus_tcp_request_decode(const ptk_allocator_t *alloc, modbus_tcp_request_t *req, ptk_buf_t *src, void *context);
ptk_err modbus_tcp_request_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const modbus_tcp_request_t *req, void *context);
ptk_err modbus_tcp_request_validate(const ptk_allocator_t *alloc, const modbus_tcp_request_t *req, void *context);
void modbus_tcp_request_dispose(const ptk_allocator_t *alloc, modbus_tcp_request_t *req, void *context);

// WriteCoilsResponse functions
ptk_err write_coils_response_decode(const ptk_allocator_t *alloc, write_coils_response_t *resp, ptk_buf_t *src, void *context);
ptk_err write_coils_response_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const write_coils_response_t *resp, void *context);
ptk_err write_coils_response_validate(const ptk_allocator_t *alloc, const write_coils_response_t *resp, void *context);
void write_coils_response_dispose(const ptk_allocator_t *alloc, write_coils_response_t *resp, void *context);

// ExceptionResponse functions
ptk_err exception_response_decode(const ptk_allocator_t *alloc, exception_response_t *ex, ptk_buf_t *src, void *context);
ptk_err exception_response_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const exception_response_t *ex, void *context);
ptk_err exception_response_validate(const ptk_allocator_t *alloc, const exception_response_t *ex, void *context);
void exception_response_dispose(const ptk_allocator_t *alloc, exception_response_t *ex, void *context);
```

### User-Implemented Function Signatures

User function names are generated **exactly as written** in the protocol specification, without any message or field name prefixes. This provides clean, reusable function names that can work across multiple contexts.

```c
// Hook functions (declared in protocol_user.h, implemented by user)
// Function names match exactly what's specified in the .pdl file
ptk_err validate_command(const ptk_allocator_t *alloc, u8 *command, const void *msg, void *context);
ptk_err generate_session_handle(const ptk_allocator_t *alloc, u32 *handle, const void *msg, void *context);
ptk_err validate_function_code(const ptk_allocator_t *alloc, u8 *function_code, const void *msg, void *context);
```

**Example Protocol Specification:**
```pdl
command: {
  type: u16_le,
  post_decode: validate_command  // Exact name used in generated code
}

session_handle: {
  type: u32_le,
  pre_encode: generate_session_handle  // Exact name used in generated code
}

function_code: {
  type: u8,
  validate: function_code >= 0x01 && function_code <= 0x17,
  post_decode: validate_function_code  // Exact name used in generated code
}
```

## Type-Safe Array API

The generator uses a macro-based type-safe array API for all variable-length arrays within message structures. This complements `ptk_buf_t` (which remains the primary buffer management API) by providing safe handling of protocol message arrays. This provides significant safety improvements over raw pointer/length pairs.

### Array Type Definition

```c
#include "ptk_array.h"

// Automatically generates for each element type T:
typedef struct T_array {
    size_t len;        // Number of elements (not bytes)
    T *elements;       // Typed pointer to elements
} T_array_t;

// Plus complete API: T_array_init, T_array_dispose, T_array_resize, etc.
```

### Safety Benefits

1. **Type Safety**: Cannot mix different element types or confuse byte counts with element counts
2. **Bounds Checking**: All array access validates indices against actual length
3. **Memory Management**: Automatic allocation/deallocation with proper cleanup
4. **Consistency**: Same API pattern for all array types eliminates errors
5. **Length Tracking**: Length is always accurate and travels with the array
6. **Null Safety**: Validates pointers before dereferencing
7. **Overflow Protection**: Validates allocation sizes before memory operations
8. **Error Propagation**: All operations return ptk_err codes for proper error handling

### Array vs Buffer Usage

- **`ptk_buf_t`**: Primary buffer management for encode/decode operations, network I/O
- **Type-safe arrays**: Message structure members for variable-length protocol data
- **Generated arrays**: Based on protocol specification array field types

### Generated Array Types

Array types are generated for each unique element type found in the protocol:
- `u8_array_t` - Byte arrays for protocol data
- `u16_array_t` - 16-bit arrays for register data
- `u32_array_t` - 32-bit arrays for extended data
- `f32_array_t` - Floating point arrays
- Alias-based arrays (e.g., `u16_be_array_t` if `u16_be` alias is used in arrays)

## Type Alias Generation

The generator creates C typedefs for all aliases defined in the protocol specification. This allows clean, readable generated code that matches the protocol definition.

### Alias Processing Rules

**Simple Type Aliases:**
```pdl
aliases: {
  u16_be: { type: u16, byte_order_map: [1, 0] }
  u32_le: { type: u32, byte_order_map: [0, 1, 2, 3] }
}
```

**Generated Typedefs:**
```c
// Standard type aliases for consistency (always generated)
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;

// Protocol-specific type aliases
typedef u16 u16_be;  // Big-endian 16-bit (byte order handled in encode/decode)
typedef u32 u32_le;  // Little-endian 32-bit (byte order handled in encode/decode)
```

**Array Type Generation:**
When types are used in array contexts, array types are generated:
```c
// Standard array types (always generated for basic types)
PTK_ARRAY_DECLARE(u8)      // Creates u8_array_t
PTK_ARRAY_DECLARE(u16)     // Creates u16_array_t

// If u16_be is used in arrays: u16_be[count]
PTK_ARRAY_DECLARE(u16_be)  // Creates u16_be_array_t
```

### Limitations and Edge Cases

**1. Complex Aliases with Structured Types:**
```pdl
aliases: {
  complex_string: {
    type: struct
    fields: {
      length: u16_le
      data: u8[length]
    }
  }
}
```
This generates a complete struct type rather than a simple typedef:
```c
typedef struct complex_string {
    u16_le length;
    u8_array_t data;
} complex_string_t;
```

**2. Array Field Aliases:**
Array fields work well with aliases for element types:
```pdl
data: u16_be[count]  // Works: generates u16_be_array_t
```

But not for array types themselves:
```pdl
aliases: {
  byte_array: u8[]  // Problematic: array type alias
}
data: byte_array  // Cannot directly typedef an array type
```

**3. Byte Order in Arrays:**
Array elements inherit byte order from their element type:
```c
// u16_be_array_t elements are encoded/decoded with big-endian byte order
// Handled per-element during array encode/decode operations
```

## Generated Code Examples

### Example 1: Simple Field Decoding (MBAPHeader)

**Protocol Specification (modbus.pdl):**
```pdl
aliases: {
  u16_be: { type: u16, byte_order_map: [1, 0] },
  u32_be: { type: u32, byte_order_map: [3, 2, 1, 0] },

  // Constants as aliases
  modbus_protocol_version: { type: u16_be, const: 0 }
}

MBAPHeader: {
  transaction_id: u16_be,
  protocol_id: modbus_protocol_version,
  length: u16_be,
  unit_id: u8
}
```

**Generated Type Definition:**
```c
#include "ptk_buf.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include "ptk_array.h"

// Standard type aliases for consistency
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;

// Generated typedefs for protocol aliases
typedef u16 u16_be;
typedef u32 u32_be;

// Generated constants from aliases (converted to uppercase)
#define MODBUS_PROTOCOL_VERSION 0

typedef struct mbap_header {
    u16_be transaction_id;
    u16_be protocol_id;     // Will be validated against MODBUS_PROTOCOL_VERSION
    u16_be length;
    u8 unit_id;
} mbap_header_t;
```

**Generated Decode Function:**
```c
ptk_err mbap_header_decode(const ptk_allocator_t *alloc, mbap_header_t *header, ptk_buf_t *src, void *context) {
    ptk_err err = PTK_OK;

    debug("Decoding MBAP header");

    // Decode transaction_id (u16_be - big endian from byte_order_map)
    if ((err = ptk_buf_consume_u16(src, &header->transaction_id, PTK_BUF_BIG_ENDIAN, false)) != PTK_OK) {
        error("Failed to decode transaction_id: %s", ptk_err_to_string(err));
        return err;
    }
    trace("transaction_id = 0x%04X", header->transaction_id);

    // Decode protocol_id (u16_be with const constraint)
    if ((err = ptk_buf_consume_u16(src, &header->protocol_id, PTK_BUF_BIG_ENDIAN, false)) != PTK_OK) {
        error("Failed to decode protocol_id: %s", ptk_err_to_string(err));
        return err;
    }
    trace("protocol_id = 0x%04X", header->protocol_id);

    // Validate const constraint: protocol_id == MODBUS_PROTOCOL_VERSION
    if (header->protocol_id != MODBUS_PROTOCOL_VERSION) {
        error("protocol_id validation failed: expected %u, got %u", MODBUS_PROTOCOL_VERSION, header->protocol_id);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Decode length (u16_be)
    if ((err = ptk_buf_consume_u16(src, &header->length, PTK_BUF_BIG_ENDIAN, false)) != PTK_OK) {
        error("Failed to decode length: %s", ptk_err_to_string(err));
        return err;
    }
    trace("length = %u", header->length);

    // Decode unit_id (u8)
    if ((err = ptk_buf_consume_u8(src, &header->unit_id, false)) != PTK_OK) {
        error("Failed to decode unit_id: %s", ptk_err_to_string(err));
        return err;
    }
    trace("unit_id = 0x%02X", header->unit_id);

    debug("MBAP header decoded successfully");
    return PTK_OK;
}
```

**Generated Encode Function:**
```c
ptk_err mbap_header_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const mbap_header_t *header, void *context) {
    ptk_err err = PTK_OK;

    debug("Encoding MBAP header");

    // Encode transaction_id (u16_be)
    if ((err = ptk_buf_produce_u16(dst, header->transaction_id, PTK_BUF_BIG_ENDIAN)) != PTK_OK) {
        error("Failed to encode transaction_id: %s", ptk_err_to_string(err));
        return err;
    }
    trace("Encoded transaction_id = 0x%04X", header->transaction_id);

    // Encode protocol_id (constant) - force const value MODBUS_PROTOCOL_VERSION
    if ((err = ptk_buf_produce_u16(dst, MODBUS_PROTOCOL_VERSION, PTK_BUF_BIG_ENDIAN)) != PTK_OK) {
        error("Failed to encode protocol_id: %s", ptk_err_to_string(err));
        return err;
    }
    trace("Encoded protocol_id = %u", MODBUS_PROTOCOL_VERSION);

    // Encode length (u16_be) - auto_size will be calculated by parent
    if ((err = ptk_buf_produce_u16(dst, header->length, PTK_BUF_BIG_ENDIAN)) != PTK_OK) {
        error("Failed to encode length: %s", ptk_err_to_string(err));
        return err;
    }
    trace("Encoded length = %u", header->length);

    // Encode unit_id (u8)
    if ((err = ptk_buf_produce_u8(dst, header->unit_id)) != PTK_OK) {
        error("Failed to encode unit_id: %s", ptk_err_to_string(err));
        return err;
    }
    trace("Encoded unit_id = 0x%02X", header->unit_id);

    debug("MBAP header encoded successfully");
    return PTK_OK;
}
```

### Example 2: Variable-Length Array (WriteCoilsRequest)

**Protocol Specification:**
```pdl
aliases: {
  u16_be: { type: u16, byte_order_map: [1, 0] },

  // Function code constants
  func_write_coils: { type: u8, const: 0x0F }
}

WriteCoilsRequest: {
  function_code: func_write_coils,
  address: u16_be,
  quantity: u16_be,
  byte_count: u8,
  data: u8[byte_count]
}
```

**Generated Type Definition:**
```c
// Generated constants from aliases (converted to uppercase)
#define FUNC_WRITE_COILS 0x0F

typedef struct write_coils_request {
    u8 function_code;                // Will be validated against FUNC_WRITE_COILS
    u16_be address;
    u16_be quantity;
    u8 byte_count;                   // Auto-calculated from data.len
    u8_array_t data;                 // Type-safe array
} write_coils_request_t;
```

**Generated Decode Function:**
```c
ptk_err write_coils_request_decode(const ptk_allocator_t *alloc, write_coils_request_t *req, ptk_buf_t *src, void *context) {
    ptk_err err = PTK_OK;

    debug("Decoding WriteCoilsRequest");

    // Decode function_code (u8 with constraint)
    if ((err = ptk_buf_consume_u8(src, &req->function_code, false)) != PTK_OK) {
        error("Failed to decode function_code: %s", ptk_err_to_string(err));
        return err;
    }
    trace("function_code = 0x%02X", req->function_code);

    // Validate constraint: function_code == FUNC_WRITE_COILS
    if (req->function_code != FUNC_WRITE_COILS) {
        error("function_code validation failed: expected 0x%02X, got 0x%02X", FUNC_WRITE_COILS, req->function_code);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Decode address (u16_be)
    if ((err = ptk_buf_consume_u16(src, &req->address, PTK_BUF_BIG_ENDIAN, false)) != PTK_OK) {
        error("Failed to decode address: %s", ptk_err_to_string(err));
        return err;
    }
    trace("address = 0x%04X", req->address);

    // Decode quantity (u16_be with constraints)
    if ((err = ptk_buf_consume_u16(src, &req->quantity, PTK_BUF_BIG_ENDIAN, false)) != PTK_OK) {
        error("Failed to decode quantity: %s", ptk_err_to_string(err));
        return err;
    }
    trace("quantity = %u", req->quantity);

    // Validate quantity constraints: >= 1 && <= 1968
    if (req->quantity < 1 || req->quantity > 1968) {
        error("quantity validation failed: %u not in range [1, 1968]", req->quantity);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Decode byte_count (u8)
    if ((err = ptk_buf_consume_u8(src, &req->byte_count, false)) != PTK_OK) {
        error("Failed to decode byte_count: %s", ptk_err_to_string(err));
        return err;
    }
    trace("byte_count = %u", req->byte_count);

    // Decode variable-length data array
    req->data_length = req->byte_count;
    if (req->data_length > 0) {
        // Check if enough data available
        size_t available_len;
        if ((err = ptk_buf_len(&available_len, src)) != PTK_OK) {
            error("Failed to get buffer length: %s", ptk_err_to_string(err));
            return err;
        }

        if (available_len < req->data_length) {
            error("Insufficient data: need %zu bytes, have %zu", req->data_length, available_len);
            return PTK_ERR_BUFFER_TOO_SMALL;
        }

        // Allocate memory for data using provided allocator
        req->data = ptk_alloc(alloc, req->data_length);
        if (!req->data) {
            error("Failed to allocate %zu bytes for data", req->data_length);
            return PTK_ERR_NO_RESOURCES;
        }

        // Read data bytes
        for (size_t i = 0; i < req->data_length; i++) {
            if ((err = ptk_buf_consume_u8(src, &req->data[i], false)) != PTK_OK) {
                error("Failed to decode data[%zu]: %s", i, ptk_err_to_string(err));
                ptk_free(alloc, req->data);
                req->data = NULL;
                return err;
            }
        }
        trace("Decoded %zu data bytes", req->data_length);
    } else {
        req->data = NULL;
    }

    // Validate array length constraint: len(data) == byte_count
    if (req->data_length != req->byte_count) {
        error("data length validation failed: expected %u, got %zu", req->byte_count, req->data_length);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    debug("WriteCoilsRequest decoded successfully");
    return PTK_OK;
}
```

**Generated Dispose Function:**
```c
void write_coils_request_dispose(write_coils_request_t *req, void *context) {
    if (req) {
        debug("Disposing WriteCoilsRequest");
        u8_array_dispose(&req->data);  // Type-safe disposal
    }
}
```

**Generated Encode Function:**
```c
ptk_err write_coils_request_encode(ptk_buf_t *dst, const write_coils_request_t *req, void *context) {
    ptk_err err = PTK_OK;

    debug("Encoding WriteCoilsRequest");

    // Validate array consistency
    if (!u8_array_is_valid(&req->data)) {
        error("Invalid data array in WriteCoilsRequest");
        return PTK_ERR_INVALID_PARAM;
    }

    // Auto-calculate byte_count from array length
    u8 calculated_byte_count = (u8)req->data.len;
    if (req->data.len > 255) {
        error("Data array too large: %zu bytes > 255", req->data.len);
        return PTK_ERR_INVALID_PARAM;
    }

    // Encode function_code
    if ((err = ptk_buf_produce_u8(dst, req->function_code)) != PTK_OK) {
        error("Failed to encode function_code: %s", ptk_err_to_string(err));
        return err;
    }

    // Encode address
    if ((err = ptk_buf_produce_u16(dst, req->address, PTK_BUF_BIG_ENDIAN)) != PTK_OK) {
        error("Failed to encode address: %s", ptk_err_to_string(err));
        return err;
    }

    // Encode quantity
    if ((err = ptk_buf_produce_u16(dst, req->quantity, PTK_BUF_BIG_ENDIAN)) != PTK_OK) {
        error("Failed to encode quantity: %s", ptk_err_to_string(err));
        return err;
    }

    // Encode byte_count (auto-calculated)
    if ((err = ptk_buf_produce_u8(dst, calculated_byte_count)) != PTK_OK) {
        error("Failed to encode byte_count: %s", ptk_err_to_string(err));
        return err;
    }

    // Encode data array
    for (size_t i = 0; i < req->data.len; i++) {
        if ((err = ptk_buf_produce_u8(dst, req->data.elements[i])) != PTK_OK) {
            error("Failed to encode data[%zu]: %s", i, ptk_err_to_string(err));
            return err;
        }
    }

    debug("WriteCoilsRequest encoded successfully");
    return PTK_OK;
}
```

### Example 3: Discriminated Union (ModbusTCPRequest)

**Protocol Specification:**
```pdl
aliases: {
  u16_be: { type: u16, byte_order_map: [1, 0] }

  // Function code constants for discrimination
  func_read_coils: { type: u8, const: 0x01 }
  func_read_discrete: { type: u8, const: 0x02 }
  func_read_holding: { type: u8, const: 0x03 }
  func_read_input: { type: u8, const: 0x04 }
  func_write_coil: { type: u8, const: 0x05 }
  func_write_register: { type: u8, const: 0x06 }
  func_write_coils: { type: u8, const: 0x0F }
  func_write_registers: { type: u8, const: 0x10 }
}

// Individual request types with constant fields for discrimination
ReadCoilsRequest: {
  function_code: func_read_coils;
  address: u16_be;
  quantity: u16_be;
}

ReadDiscreteRequest: {
  function_code: func_read_discrete;
  address: u16_be;
  quantity: u16_be;
}

WriteCoilRequest: {
  function_code: func_write_coil;
  address: u16_be;
  value: u16_be;
}

WriteCoilsRequest: {
  function_code: func_write_coils;
  address: u16_be;
  quantity: u16_be;
  byte_count: u8;
  data: u8[byte_count];
}

// Main discriminated union
ModbusTCPRequest: {
  header: MBAPHeader;
  pdu: {
    type_choice: [
      ReadCoilsRequest,
      ReadDiscreteRequest,
      WriteCoilRequest,
      WriteCoilsRequest
    ],
    default: UnknownRequest
  };
}

// Default/error case
UnknownRequest: {
  function_code: u8;
  data: u8[];
}
```

### Example 3: Discriminated Union (ModbusTCPRequest)

**Generated Type Definition:**
```c
// Additional constant aliases for discrimination
typedef u8 func_read_coils;
typedef u8 func_read_discrete;
typedef u8 func_write_coil;
typedef u8 func_write_coils;

typedef enum modbus_tcp_request_pdu_type {
    MODBUS_TCP_REQUEST_PDU_READ_COILS_REQUEST,
    MODBUS_TCP_REQUEST_PDU_READ_DISCRETE_REQUEST,
    MODBUS_TCP_REQUEST_PDU_WRITE_COIL_REQUEST,
    MODBUS_TCP_REQUEST_PDU_WRITE_COILS_REQUEST,
    MODBUS_TCP_REQUEST_PDU_UNKNOWN_REQUEST  // Default case
} modbus_tcp_request_pdu_type_t;

typedef struct read_coils_request {
    func_read_coils function_code;  // Constant field for discrimination
    u16_be address;
    u16_be quantity;
} read_coils_request_t;

typedef struct write_coil_request {
    func_write_coil function_code;  // Constant field for discrimination
    u16_be address;
    u16_be value;
} write_coil_request_t;

typedef struct unknown_request {
    u8 function_code;
    u8_array_t data;
} unknown_request_t;

typedef struct modbus_tcp_request {
    mbap_header_t header;
    modbus_tcp_request_pdu_type_t pdu_type;
    union {
        read_coils_request_t read_coils_request;
        read_discrete_request_t read_discrete_request;
        write_coil_request_t write_coil_request;
        write_coils_request_t write_coils_request;
        unknown_request_t unknown_request;  // Default case
    } pdu;
} modbus_tcp_request_t;
```

**Generated Decode Function:**
```c
ptk_err modbus_tcp_request_decode(modbus_tcp_request_t *req, ptk_buf_t *src, void *context) {
    ptk_err err = PTK_OK;

    debug("Decoding ModbusTCPRequest");

    // Decode header first
    if ((err = mbap_header_decode(&req->header, src, context)) != PTK_OK) {
        error("Failed to decode MBAP header: %s", ptk_err_to_string(err));
        return err;
    }

    // Peek at function_code to determine PDU type
    uint8_t function_code;
    if ((err = ptk_buf_consume_u8(src, &function_code, true)) != PTK_OK) {
        error("Failed to peek function_code: %s", ptk_err_to_string(err));
        return err;
    }
    trace("Peeked function_code = 0x%02X", function_code);

    // Determine PDU type based on function_code constants
    if (function_code == 0x01) {  // func_read_coils constant
        req->pdu_type = MODBUS_TCP_REQUEST_PDU_READ_COILS_REQUEST;
        debug("Detected ReadCoilsRequest (function_code 0x%02X)", function_code);
        if ((err = read_coils_request_decode(&req->pdu.read_coils_request, src, context)) != PTK_OK) {
            error("Failed to decode ReadCoilsRequest: %s", ptk_err_to_string(err));
            return err;
        }
    } else if (function_code == 0x02) {  // func_read_discrete constant
        req->pdu_type = MODBUS_TCP_REQUEST_PDU_READ_DISCRETE_REQUEST;
        debug("Detected ReadDiscreteRequest (function_code 0x%02X)", function_code);
        if ((err = read_discrete_request_decode(&req->pdu.read_discrete_request, src, context)) != PTK_OK) {
            error("Failed to decode ReadDiscreteRequest: %s", ptk_err_to_string(err));
            return err;
        }
    } else if (function_code == 0x05) {  // func_write_coil constant
        req->pdu_type = MODBUS_TCP_REQUEST_PDU_WRITE_COIL_REQUEST;
        debug("Detected WriteCoilRequest (function_code 0x%02X)", function_code);
        if ((err = write_coil_request_decode(&req->pdu.write_coil_request, src, context)) != PTK_OK) {
            error("Failed to decode WriteCoilRequest: %s", ptk_err_to_string(err));
            return err;
        }
    } else if (function_code == 0x0F) {  // func_write_coils constant
        req->pdu_type = MODBUS_TCP_REQUEST_PDU_WRITE_COILS_REQUEST;
        debug("Detected WriteCoilsRequest (function_code 0x%02X)", function_code);
        if ((err = write_coils_request_decode(&req->pdu.write_coils_request, src, context)) != PTK_OK) {
            error("Failed to decode WriteCoilsRequest: %s", ptk_err_to_string(err));
            return err;
        }
    } else {
        // Default case - unknown function code
        req->pdu_type = MODBUS_TCP_REQUEST_PDU_UNKNOWN_REQUEST;
        debug("Unknown function_code: 0x%02X, using default handler", function_code);
        if ((err = unknown_request_decode(&req->pdu.unknown_request, src, context)) != PTK_OK) {
            error("Failed to decode UnknownRequest: %s", ptk_err_to_string(err));
            return err;
        }
    }

    debug("ModbusTCPRequest decoded successfully");
    return PTK_OK;
}
```

## Generated File Structure

### protocol_generated.h
```c
#ifndef MODBUS_GENERATED_H
#define MODBUS_GENERATED_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ptk_buf.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include "ptk_array.h"

// Generated typedefs for protocol aliases
typedef uint16_t u16_be;
typedef uint32_t u32_be;

// Generate array types for standard and alias types used in arrays
PTK_ARRAY_DECLARE(u8)      // Creates u8_array_t
PTK_ARRAY_DECLARE(u16_be)  // Creates u16_be_array_t if used

// Forward declarations
typedef struct mbap_header mbap_header_t;
typedef struct read_bits_request read_bits_request_t;
typedef struct write_coils_request write_coils_request_t;
// ... more typedefs

// Generated structure definitions
struct mbap_header {
    u16_be transaction_id;    // Uses generated typedef
    u16_be protocol_id;       // Uses generated typedef
    u16_be length;            // Uses generated typedef
    u8 unit_id;
};

struct write_coils_request {
    u8 function_code;
    u16_be address;           // Uses generated typedef
    u16_be quantity;          // Uses generated typedef
    u8 byte_count;
    u8_array_t data;          // Type-safe array
};

// ... more structures

// Function declarations
ptk_err mbap_header_decode(const ptk_allocator_t *alloc, mbap_header_t *header, ptk_buf_t *src, void *context);
ptk_err mbap_header_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const mbap_header_t *header, void *context);
ptk_err mbap_header_validate(const ptk_allocator_t *alloc, const mbap_header_t *header, void *context);
void mbap_header_dispose(const ptk_allocator_t *alloc, mbap_header_t *header, void *context);

ptk_err write_coils_request_decode(const ptk_allocator_t *alloc, write_coils_request_t *req, ptk_buf_t *src, void *context);
ptk_err write_coils_request_encode(const ptk_allocator_t *alloc, ptk_buf_t *dst, const write_coils_request_t *req, void *context);
ptk_err write_coils_request_validate(const ptk_allocator_t *alloc, const write_coils_request_t *req, void *context);
void write_coils_request_dispose(const ptk_allocator_t *alloc, write_coils_request_t *req, void *context);

// ... more function declarations

#endif // MODBUS_GENERATED_H
```

### protocol_user.h
```c
#ifndef MODBUS_USER_H
#define MODBUS_USER_H

#include "modbus_generated.h"

/*
 * USER-IMPLEMENTED FUNCTIONS
 * Implement these functions to provide custom behavior.
 */

// Hook functions for validation and custom processing
ptk_err validate_function_code(const ptk_allocator_t *alloc, u8 *function_code, const void *msg, void *context);
ptk_err validate_address_range(const ptk_allocator_t *alloc, u16 *address, const void *msg, void *context);

#endif // MODBUS_USER_H
```

## EBNF Grammar

The PDL3 syntax uses an LL(1) compatible grammar to ensure predictable parsing. The complete grammar specification is available in [`pdl_grammar.ebnf`](pdl_grammar.ebnf).

## Implementation Requirements

The Python generator program must:

1. **Parse JSON-like Syntax**: Handle the complete PDL3 syntax with comma-separated attributes in curly braces
2. **Generate Constants**: Create `#define` entries for all constant aliases, converting names to uppercase
3. **Generate Type Definitions**: Create appropriate C structures and enums
4. **Generate Buffer Operations**: Use ptk_buf_t APIs for all data manipulation
5. **Generate Error Handling**: Use ptk_err codes and ptk_log macros for diagnostics
6. **Handle Memory Management**: Use pluggable allocator for all dynamic allocations (first parameter in all functions)
7. **Validate Constraints**: Generate runtime validation using named constants instead of raw numbers
8. **Support Hooks**: Generate calls to user-provided functions using exact names from specification (with allocator parameter)
9. **Handle Unions**: Generate sequential type choice handling - try alternatives in order until one passes validation
10. **Auto-sizing**: Calculate and set auto-sized fields during encoding
11. **Function Naming**: Use exact function names as specified in protocol definition without modification
12. **Array Safety**: Generate type-safe arrays using ptk_array.h macros for all variable-length data
13. **Constant Usage**: Use generated constants throughout code instead of raw numbers for maintainability
14. **Allocator Integration**: Pass allocator to all array operations and user hook functions for consistent memory management

The specification provides sufficient detail to implement a complete protocol code generator that produces industrial-strength, memory-safe protocol handling code.