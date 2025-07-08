# Protocol Description Language v2 (PDL2) - CUE-Based Code Generator

A streamlined protocol definition language using CUE-inspired syntax for generating industrial protocol encode/decode functions with the protocol toolkit APIs.

## Overview

PDL2 provides a declarative approach to defining binary protocols using familiar CUE syntax patterns. The generator produces C header and source files with complete encode/decode functions using the protocol toolkit's `ptk_buf.h` API.

## Core Concepts

### Field Definitions

Fields can be defined in two forms:

**Shorthand Form:**
```cue
command: uint16
length: uint8
```

**Structured Form:**
```cue
command: {
  type: uint16
  validate: command >= 0x0065 && command <= 0x0070
  post_decode: validate_command
}

session_handle: {
  type: uint32
  const: 0  // For requests
  pre_encode: generate_session_handle
}
```

### Type Aliases

Define reusable type patterns and byte orderings:

```cue
aliases: {
  u16_le: { type: uint16, byte_order_map: [0, 1] }
  u16_be: { type: uint16, byte_order_map: [1, 0] }
  u32_le: { type: uint32, byte_order_map: [0, 1, 2, 3] }

  // PLC5 specific byte orders
  f32_plc5: {
    type: float32
    byte_order_map: [2, 3, 0, 1]
  }

  // Complex byte swapping for PLC5 strings
  plc5_str_data: {
    type: [uint8; 82]
    byte_order_map: [
      1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14,
      17,16, 19,18, 21,20, 23,22, 25,24, 27,26, 29,28,
      31,30, 33,32, 35,34, 37,36, 39,38, 41,40, 43,42,
      45,44, 47,46, 49,48, 51,50, 53,52, 55,54, 57,56,
      59,58, 61,60, 63,62, 65,64, 67,66, 69,68, 71,70,
      73,72, 75,74, 77,76, 79,78, 81,80
    ]
  }

  // Standard string patterns
  cip_string: {
    type: struct
    fields: {
      length: u16_le
      data: [...uint8] & len(data) == length
    }
  }

  logix_string: {
    type: struct
    fields: {
      length: u32_le
      data: [...uint8] & len(data) == length
    }
  }
}
```

### Discriminated Unions

Handle protocol variants using `oneof` constructs:

```cue
EIPMessage: {
  header: EIPHeader
  payload: {
    type: oneof {
      RegisterSession    | header.command == 0x0065
      UnregisterSession  | header.command == 0x0066
      SendRRData        | header.command == 0x006F
      SendUnitData      | header.command == 0x0070
    }
  }
}

CPFItem: {
  item_type: u16_le
  item_length: {
    type: u16_le
    auto_size: "item_data"
  }
  item_data: {
    type: oneof {
      NullAddress      | item_type == 0x0000
      ConnectedAddress | item_type == 0x00A1
      ConnectedData    | item_type == 0x00B1
      UnconnectedData  | item_type == 0x00B2
    }
  }
}
```

### Variable-Length Arrays

```cue
CPFPacket: {
  interface_handle: u32_le
  timeout: u16_le
  item_count: u16_le
  items: [...CPFItem] & len(items) == item_count
}

CIPPath: {
  path_size: uint8  // Size in 16-bit words
  path_segments: [...uint8] & len(path_segments) == path_size * 2
}
```

### Field Constraints and Validation

```cue
EIPHeader: {
  command: {
    type: u16_le
    validate: command >= 0x0065 && command <= 0x0070
  }

  length: {
    type: u16_le
    auto_size: "payload"
    validate: length <= 65535
  }

  session_handle: {
    type: u32_le
    validate: session_handle != 0  // Except for register session
    post_decode: check_session_exists
  }

  status: u32_le
  sender_context: uint64
  options: u32_le
}
```

### Function Hooks

```cue
CIPRequest: {
  service: {
    type: uint8
    validate: service <= 0x7F  // Ensure request bit clear
    post_decode: validate_service_supported
  }

  path_size: uint8
  request_path: {
    type: [...uint8]
    size_from: path_size
    validate: len(request_path) == path_size * 2
    post_decode: parse_cip_path
  }

  service_data: {
    type: [...uint8]
    remaining_bytes: true
    pre_encode: encode_service_specific_data
    post_decode: decode_service_specific_data
  }
}
```

## Complete Protocol Example: EtherNet/IP with CIP

```cue
// ethernetip.cue
package ethernetip

aliases: {
  u8:  { type: uint8 }
  u16: { type: uint16, byte_order_map: [0, 1] }  // Little endian
  u32: { type: uint32, byte_order_map: [0, 1, 2, 3] }
  u64: { type: uint64, byte_order_map: [0, 1, 2, 3, 4, 5, 6, 7] }
}

// EtherNet/IP Encapsulation Layer
EIPHeader: {
  command: {
    type: u16
    validate: command >= 0x0001 && command <= 0x0070
  }
  length: {
    type: u16
    auto_size: "data"
  }
  session_handle: {
    type: u32
    post_decode: validate_session_handle
  }
  status: u32
  sender_context: u64
  options: u32
}

EIPMessage: {
  header: EIPHeader
  data: {
    type: oneof {
      RegisterSessionData    | header.command == 0x0065
      UnregisterSessionData  | header.command == 0x0066 && header.length == 0
      SendRRData            | header.command == 0x006F
      SendUnitData          | header.command == 0x0070
    }
  }
}

RegisterSessionData: {
  protocol_version: {
    type: u16
    const: 1
  }
  options: {
    type: u16
    const: 0
  }
}

UnregisterSessionData: {
  // Empty payload - validated by length == 0 constraint
}

// Common Packet Format Layer
SendRRData: {
  interface_handle: {
    type: u32
    const: 0
  }
  timeout: u16
  item_count: {
    type: u16
    validate: item_count >= 2
  }
  items: [...CPFItem] & len(items) == item_count
}

SendUnitData: {
  interface_handle: u32
  timeout: u16
  item_count: {
    type: u16
    validate: item_count >= 2
  }
  items: [...CPFItem] & len(items) == item_count
}

CPFItem: {
  item_type: u16
  item_length: {
    type: u16
    auto_size: "item_data"
  }
  item_data: {
    type: oneof {
      NullAddressItem        | item_type == 0x0000
      ConnectedAddressItem   | item_type == 0x00A1
      ConnectedDataItem      | item_type == 0x00B1
      UnconnectedDataItem    | item_type == 0x00B2
    }
  }
}

NullAddressItem: {
  // Empty - length must be 0
}

ConnectedAddressItem: {
  connection_id: u32
}

ConnectedDataItem: {
  sequence_number: u16
  data: {
    type: [...uint8]
    remaining_bytes: true
  }
}

UnconnectedDataItem: {
  data: {
    type: [...uint8]
    remaining_bytes: true
  }
}

// CIP Message Layer
CIPRequest: {
  service: {
    type: u8
    validate: service <= 0x7F
  }
  path_size: u8  // Size in 16-bit words
  request_path: {
    type: [...uint8]
    size_from: path_size
    validate: len(request_path) == path_size * 2
    post_decode: validate_path_alignment
  }
  service_data: {
    type: oneof {
      ReadTagRequest           | service == 0x4C
      WriteTagRequest          | service == 0x4D
      ReadTagFragmentedRequest | service == 0x52
      WriteTagFragmentedRequest| service == 0x53
      ForwardOpenRequest       | service == 0x54
      ForwardCloseRequest      | service == 0x4E
      MultiServiceRequest      | service == 0x0A
      GetAttributesAllRequest  | service == 0x01
    }
  }
}

CIPResponse: {
  service: {
    type: u8
    validate: (service & 0x80) != 0  // Response bit set
  }
  reserved: {
    type: u8
    const: 0
  }
  general_status: u8
  additional_status_size: u8  // Size in 16-bit words
  additional_status: {
    type: [...u16]
    size_from: additional_status_size
  }
  response_data: {
    type: [...uint8]
    remaining_bytes: true
  }
}

// CIP Service Definitions
ReadTagRequest: {
  element_count: {
    type: u16
    const: 1
  }
}

WriteTagRequest: {
  data_type: u16
  element_count: u16
  data: {
    type: [...uint8]
    remaining_bytes: true
  }
}

ReadTagFragmentedRequest: {
  element_count: u16
  byte_offset: u32
}

WriteTagFragmentedRequest: {
  data_type: u16
  element_count: u16
  byte_offset: u32
  data: {
    type: [...uint8]
    remaining_bytes: true
  }
}

ForwardOpenRequest: {
  secs_per_tick: u8
  timeout_ticks: u8
  server_conn_id: {
    type: u32
    const: 0
  }
  client_conn_id: {
    type: u32
    pre_encode: generate_connection_id
  }
  conn_serial_number: u16
  orig_vendor_id: u16
  orig_serial_number: u32
  conn_timeout_multiplier: u8
  reserved: {
    type: [uint8; 3]
    const: [0, 0, 0]
  }
  client_to_server_rpi: u32
  client_to_server_params: u16
  server_to_client_rpi: u32
  server_to_client_params: u16
  transport_class: {
    type: u8
    const: 0xA3
  }
  connection_path_size: u8
  connection_path: {
    type: [...uint8]
    size_from: connection_path_size
    validate: len(connection_path) == connection_path_size * 2
  }
}

MultiServiceRequest: {
  service_count: u16
  service_offsets: {
    type: [...u16]
    size_from: service_count
  }
  service_data: {
    type: [...uint8]
    remaining_bytes: true
  }
}

GetAttributesAllRequest: {
  // No additional data for Get Attributes All
}

// CIP Data Types
CIPDataType: {
  type_code: u16
  data: {
    type: oneof {
      BoolData     | type_code == 0xC1
      SintData     | type_code == 0xC2
      IntData      | type_code == 0xC3
      DintData     | type_code == 0xC4
      RealData     | type_code == 0xCA
      StringData   | type_code == 0xD0
      StructData   | type_code == 0xA2
      ArrayData    | type_code == 0xA3
    }
  }
}

BoolData: {
  value: u8
}

SintData: {
  value: uint8
}

IntData: {
  value: u16
}

DintData: {
  value: u32
}

RealData: {
  value: {
    type: float32
    byte_order_map: [0, 1, 2, 3]
  }
}

StringData: {
  length: u32
  data: {
    type: [...uint8]
    size_from: length
  }
}

StructData: {
  structure_handle: u16
  member_count: u16
  members: [...StructMember] & len(members) == member_count
}

StructMember: {
  data_type: u16
  offset: u16
  name_length: u8
  name: {
    type: [...uint8]
    size_from: name_length
  }
}

ArrayData: {
  data_type: u16
  dimensions: u8
  dimension_sizes: {
    type: [...u32]
    size_from: dimensions
  }
  data: {
    type: [...uint8]
    remaining_bytes: true
  }
}
```

## OMRON vs Rockwell CIP Extensions

```cue
// OMRON-specific extensions
OMRONExtensions: {
  // Variable Object for tag discovery
  VariableObject: {
    variable_type: u16
    array_dimensions: u8
    array_sizes: {
      type: [...u32]
      size_from: array_dimensions
    }
    variable_name: {
      type: [...uint8]
      remaining_bytes: true
      post_decode: decode_utf8_string
    }
  }

  // Simple Data Segment for large transfers
  SimpleDataSegment: {
    segment_type: {
      type: u8
      const: 0x80
    }
    segment_length: {
      type: u8
      const: 0x03
    }
    offset: u32
    size: u16
  }

  // OMRON timestamp with nanosecond precision
  OMRONTimestamp: {
    type: u64
    post_decode: convert_omron_nanoseconds
    pre_encode: convert_to_omron_nanoseconds
  }
}

// Rockwell-specific extensions
RockwellExtensions: {
  // Instance Attribute List for batch operations
  InstanceAttributeList: {
    attribute_count: u16
    attribute_ids: {
      type: [...u16]
      size_from: attribute_count
    }
  }

  // Template Object for UDT discovery
  TemplateObject: {
    template_instance: u16
    template_definition: {
      type: [...uint8]
      remaining_bytes: true
      post_decode: decode_template_structure
    }
  }

  // Logix-specific string format
  LogixString: {
    length: u32
    data: {
      type: [...uint8]
      size_from: length
      validate: length <= 82  // Logix string limit
    }
  }
}
```

## Generated Code Structure

The generator produces clean separation between generated and user code:

### Generated Header (protocol_generated.h)
```c
#ifndef PROTOCOL_GENERATED_H
#define PROTOCOL_GENERATED_H

#include "ptk_buf.h"
#include "ptk_err.h"

// Generated type definitions
typedef struct eip_header {
    uint16_t command;
    uint16_t length;
    uint32_t session_handle;
    uint32_t status;
    uint64_t sender_context;
    uint32_t options;
} eip_header_t;

// Generated function declarations
ptk_err eip_header_decode(eip_header_t *header, ptk_buf *buf, void *context);
ptk_err eip_header_encode(ptk_buf *buf, const eip_header_t *header, void *context);
ptk_err eip_header_validate(const eip_header_t *header, void *context);

#endif
```

### User Functions Header (protocol_user.h)
```c
#ifndef PROTOCOL_USER_H
#define PROTOCOL_USER_H

#include "protocol_generated.h"

// User-implemented validation functions
ptk_err validate_session_handle(uint32_t *session_handle, const eip_header_t *msg, void *context);
ptk_err generate_connection_id(uint32_t *conn_id, const forward_open_request_t *msg, void *context);
ptk_err validate_service_supported(uint8_t *service, const cip_request_t *msg, void *context);

#endif
```

## Key Features

1. **CUE-Inspired Syntax** - Familiar, clean syntax with powerful constraints
2. **Type Safety** - Strong typing with validation at multiple levels
3. **Vendor Flexibility** - Support for both OMRON and Rockwell CIP extensions
4. **Memory Safety** - Generated code uses safe buffer operations
5. **Constraint Validation** - Rich constraint language for protocol compliance
6. **Function Hooks** - Clean separation of generated vs user-defined behavior
7. **Byte Order Control** - Precise control over field byte ordering
8. **Performance** - Optimized generated code with minimal overhead

## Usage

```bash
# Generate protocol implementation
pdl2-gen ethernetip.cue

# Produces:
# ethernetip_generated.h    - Type definitions and function declarations
# ethernetip_generated.c    - Generated encode/decode implementations
# ethernetip_user.h         - User function declarations to implement
```

The generated code integrates seamlessly with the protocol toolkit APIs and provides industrial-strength protocol handling with comprehensive error reporting and validation.