# Protocol Definition Language (PDL) Generator v1

A Domain Specific Language (DSL) for generating protocol encode/decode functions from structured definitions that resemble C structs but provide rich annotations for protocol-specific behavior.

## Overview

The Protocol Definition Language (PDL) allows you to define network protocols in a declarative manner. The generator reads PDL files and produces C header and source files with complete encode/decode functions using the protocol toolkit's `ptk_buf.h` API.

## Basic Syntax

### Protocol Declaration
```pdl
protocol EtherNetIP {
    name: "EtherNet/IP",
    version: "1.0",
    default_port: 44818,
    endianness: little,
    transport: tcp
}
```

### Simple Message Definition
```pdl
message EIPHeader {
    uint16_le command;
    uint16_le length @auto_size(payload);
    uint32_le session_handle;
    uint32_le status;
    uint64_le sender_context;
    uint32_le options;
}
```

### Data Types with Endianness
```pdl
// Explicit endianness in type names
uint8_t     // 8-bit (no endianness)
uint16_le   // 16-bit little endian
uint32_be   // 32-bit big endian
uint64_le   // 64-bit little endian
int16_le    // Signed 16-bit little endian
float32_be  // 32-bit float big endian
float64_le  // 64-bit double little endian
```

## Advanced Features

### Variable-Length Arrays
```pdl
message CPFPacket {
    uint32_le interface_handle;
    uint16_le router_timeout;
    uint16_le item_count;
    CPFItem items[item_count];  // Array sized by previous field
}
```

### Discriminated Unions (Switch Statements)
```pdl
message CPFItem {
    uint16_le item_type;
    uint16_le item_length;

    @switch(item_type) {
        case 0x0000: {  // Null Address Item
            @validate(item_length == 0);
        }
        case 0x00A1: {  // Connected Address Item
            @validate(item_length == 4);
            uint32_le connection_id;
        }
        case 0x00B1: {  // Connected Data Item
            uint16_le conn_seq;
            uint8_t payload[item_length - @sizeof(conn_seq)];
        }
        default: {
            uint8_t data[item_length];
        }
    }
}
```

### Conditional Fields
```pdl
message EIPMessage {
    uint16_le command;
    uint16_le length;
    uint32_le session_handle;
    // ... other header fields ...

    @switch(command) {
        case 0x0065: {  // Register Session
            @when(session_handle == 0) RegisterSessionRequest payload;
            @when(session_handle != 0) RegisterSessionResponse payload;
        }
        case 0x0066: {  // Unregister Session
            @validate(length == 0);  // No payload
        }
        case 0x006F: {  // Send RR Data
            CPFPacket payload @validate(payload.item_count >= 2);
        }
    }
}
```

### Alignment and Byte Ordering
```pdl
message CIPMessage {
    uint8_t service;
    uint8_t path_size;      // Size in 16-bit words
    uint16_le path[path_size] @align(2);  // Array of 16-bit words, aligned
    uint8_t service_data[] @remaining_bytes;
}

// Type aliases for complex byte ordering
type_aliases {
    alias plc5_str = message {
        u16_le len;
        u8 data[82] @byte_order(1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14,17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30,33,32,35,34,37,36,39,38,41,40,43,42,45,44,47,46,49,48,51,50,53,52,55,54,57,56,59,58,61,60,63,62,65,64,67,66,69,68,71,70,73,72,75,74,77,76,79,78,81,80);
    };

    alias f32_plc5 = f32 @byte_order(2, 3, 0, 1);
}
```

### Validation and Function Calls
```pdl
message ValidatedMessage {
    uint16_le command @validate(command >= 0x0065 && command <= 0x0070);
    uint8_t version = 1;
    uint8_t flags @call_post_decode(validate_flags);
    uint16_le length @validate(length >= 0 && length <= 65535);
    uint32_le session_handle @call_pre_encode(generate_session_if_zero);
}

message CustomField {
    uint16_le field_type;
    uint16_le field_length;
    uint8_t custom_data[field_length] @call_to_encode(encode_custom_data) @call_to_decode(decode_custom_data);
}
```

## Annotation Reference

### Core Annotations
- `@auto_size(field)` - Automatically calculate size of referenced field
- `@validate(c_expression)` - Simple C validation expressions
- `@when(condition)` - Conditional fields
- `@byte_order(pattern)` - Custom byte ordering for fields
- `@align(bytes)` - Align field to N-byte boundary

### Function Call Annotations
- `@call_post_decode(func)` - Called after field is decoded (validate/modify)
- `@call_pre_encode(func)` - Called before field is encoded (validate/modify)
- `@call_to_decode(func)` - Custom decode function (replaces standard decoding)
- `@call_to_encode(func)` - Custom encode function (replaces standard encoding)

### Expression Helpers
- `@sizeof(field)` - Size of field in bytes
- `@offset_of(field)` - Offset of field from message start
- `@remaining_bytes` - Use all remaining bytes in buffer

### Assignment Operators
- `=` - Constant value (validate on decode, set on encode)
- `?=` - Default value (use if not set)

## Service Definitions

```pdl
service SessionManager {
    request RegisterSession {
        command: 0x0065,
        session_handle: 0,
        payload: {
            uint16_le version @const(1);
            uint16_le options @default(0);
        }
    }

    response RegisterSession {
        command: 0x0065,
        status: 0,
        session_handle: @generated,
        payload: {
            uint16_le version @const(1);
            uint16_le options;
        }
    }
}
```

## Type Aliases

```pdl
type_aliases {
    // Standard endianness aliases
    alias i32_le = i32 @byte_order(0, 1, 2, 3);
    alias i32_be = i32 @byte_order(3, 2, 1, 0);
    alias f32_le = f32 @byte_order(0, 1, 2, 3);
    alias f32_be = f32 @byte_order(3, 2, 1, 0);

    // PLC5's weird byte orders
    alias f32_plc5 = f32 @byte_order(2, 3, 0, 1);
    alias i32_plc5 = i32 @byte_order(2, 3, 0, 1);

    // Complex structures
    alias plc5_str = message {
        u16_le len @validate(len <= 82);
        u8 data[82] @byte_order(1,0,3,2,5,4,7,6,9,8,11,10,13,12,15,14,17,16,19,18,21,20,23,22,25,24,27,26,29,28,31,30,33,32,35,34,37,36,39,38,41,40,43,42,45,44,47,46,49,48,51,50,53,52,55,54,57,56,59,58,61,60,63,62,65,64,67,66,69,68,71,70,73,72,75,74,77,76,79,78,81,80);
    };

    // Standard length-prefixed string
    alias length_string = message {
        uint16_le length;
        uint8_t data[length];
    };
}

## Data Type Registry

```pdl
datatype_registry {
    BOOL: { code: 0x00C1, size: 1, type: "uint8_t" },
    SINT: { code: 0x00C2, size: 1, type: "int8_t" },
    INT: { code: 0x00C3, size: 2, type: "int16_le" },
    DINT: { code: 0x00C4, size: 4, type: "int32_le" },
    REAL: { code: 0x00CA, size: 4, type: "float32_le" },
    STRING: {
        code: 0x00D0,
        size: variable,
        structure: {
            uint32_le length;
            uint8_t data[length];
        }
    }
}
```

## Error Handling

```pdl
error_codes {
    // Protocol-specific errors
    INVALID_COMMAND: 0x01,
    INVALID_LENGTH: 0x02,
    INVALID_SESSION: 0x64,

    // Generic errors
    PARSE_ERROR: 0x100,
    VALIDATION_ERROR: 0x101,
    BUFFER_OVERFLOW: 0x102
}
```

## Generated Code Structure

The generator creates two header files to clearly separate generated code from user-defined functions.

### Generated Header (protocol_generated.h)
```c
#ifndef PROTOCOL_GENERATED_H
#define PROTOCOL_GENERATED_H

#include <stdint.h>
#include <stdbool.h>
#include "ptk_buf.h"
#include "ptk_err.h"

// Generated type definitions
typedef struct {
    uint16_t command;
    uint16_t length;
    uint32_t session_handle;
    uint32_t status;
    uint64_t sender_context;
    uint32_t options;
} eip_header_t;

typedef struct {
    uint16_t len;
    uint8_t data[82];
} plc5_str_t;

// Generated function declarations - all take context parameter
extern ptk_err eip_header_decode(eip_header_t *header, ptk_buf *src, void *context);
extern ptk_err eip_header_encode(ptk_buf *dst, const eip_header_t *header, void *context);
extern ptk_err eip_header_validate(const eip_header_t *header, void *context);
extern void eip_header_dispose(eip_header_t *header, void *context);

extern ptk_err plc5_str_decode(plc5_str_t *str, ptk_buf *src, void *context);
extern ptk_err plc5_str_encode(ptk_buf *dst, const plc5_str_t *str, void *context);
extern ptk_err plc5_str_to_cstring(char *dest, size_t dest_size, const plc5_str_t *src, void *context);
extern ptk_err plc5_str_from_cstring(plc5_str_t *dest, const char *src, void *context);

#endif
```

### User Functions Header (protocol_user.h)
```c
#ifndef PROTOCOL_USER_H
#define PROTOCOL_USER_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol_generated.h"

/*
 * USER-DEFINED FUNCTIONS
 * You must implement these functions for the protocol to work correctly.
 * All functions receive a context parameter for stateful operations.
 */

// Post-decode functions - called after field is decoded (can validate/modify)
extern ptk_err validate_session_handle(uint32_t *session_handle, const eip_header_t *msg, void *context);
extern ptk_err validate_flags(uint32_t *flags, const eip_header_t *msg, void *context);

// Pre-encode functions - called before field is encoded (can validate/modify)
extern ptk_err generate_session_if_zero(uint32_t *session_handle, const eip_header_t *msg, void *context);
extern ptk_err generate_connection_id(uint32_t *connection_id, const connection_msg_t *msg, void *context);

// Custom encode/decode functions - complete control over encoding/decoding
extern ptk_err encode_custom_data(ptk_buf *dst, const uint8_t *data, const custom_field_t *msg, void *context);
extern ptk_err decode_custom_data(uint8_t *data, ptk_buf *src, const custom_field_t *msg, void *context);

#endif
```

## Usage

```bash
# Generate protocol code
generator.py protocol_definitions.pdl

# Produces:
# protocol_generated.h    - Generated types and functions
# protocol_generated.c    - Generated implementations
# protocol_user.h        - User function declarations (must implement)
```

The generated code depends only on:
- `src/include/ptk_buf.h`
- `src/include/ptk_err.h`
- Standard C headers (`stdint.h`, `stdbool.h`)

## Key Features

1. **Protocol Agnostic** - Can define any reasonably simple protocol
2. **Rich Validation** - Built-in and custom validation functions
3. **Expression-Based Sizing** - Use expressions like `field_length - @sizeof(header)` for complex size calculations
4. **Simple C Validation** - Use familiar C expressions for validation: `@validate(length >= 0 && length <= 1024)`
5. **Phase-Specific Functions** - Clear distinction between post-decode, pre-encode, and custom encode/decode functions
6. **Assignment Operators** - Use `=` for constants and `?=` for defaults
7. **Custom Byte Ordering** - Support for arbitrary byte orders like PLC5
8. **Type Aliases** - Reusable type definitions for complex structures
9. **Conditional Encoding** - Use `@when(condition)` for conditional fields
10. **Context Support** - All functions accept context parameter for stateful operations
11. **Clear Separation** - Generated vs user-defined functions in separate headers
12. **Memory Safe** - Generated code uses safe buffer operations