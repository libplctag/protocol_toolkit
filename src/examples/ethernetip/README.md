# EtherNet/IP Protocol Implementation

A clean, idiomatic C API for EtherNet/IP protocol communication following Protocol Toolkit (PTK) design patterns.

## Design Philosophy

This implementation follows the same design patterns as the Modbus reference implementation:

- **Memory Management**: Parent-child allocation with automatic cleanup
- **Type Safety**: Union-based PDU dispatch with compile-time type checking
- **Protocol Abstraction**: Hide wire format details, expose only application-level data
- **Extensibility**: Easy to add new CIP objects and message types

## Key Features

### 1. Clean Public API
- No protocol internals exposed (encapsulation, CPF, raw wire formats)
- Application-friendly data structures with host byte order
- Type-safe PDU unions for message dispatch
- Automatic session management

### 2. Ergonomic CIP Path Construction
```c
// Method 1: Parse from string
cip_ioi_path_pdu_t *path;
cip_ioi_path_pdu_create_from_string(parent, "1,0", &path);

// Method 2: Build programmatically
cip_ioi_path_pdu_t *path = cip_ioi_path_pdu_create(parent);
cip_ioi_path_pdu_add_port(path, 1);        // Backplane
cip_ioi_path_pdu_add_port(path, 0);        // Slot 0
cip_ioi_path_pdu_add_class(path, 0x01);    // Identity Object
cip_ioi_path_pdu_add_instance(path, 0x01); // Instance 1
```

### 3. Device Discovery
```c
eip_discovery_result_t *result;
eip_discover_devices(parent, NULL, 5000, &result);

for (size_t i = 0; i < result->device_count; i++) {
    eip_identity_response_t *device = eip_identity_array_get(result->devices, i);
    printf("Device: %s at %s\n", device->product_name, device->ip_address);
}
```

### 4. Connection and Messaging
```c
// Connect with automatic session management
eip_connection *conn = eip_client_connect_tcp(parent, "192.168.1.100", 44818, cip_path);

// Type-safe message receiving
eip_apu_base_t *apu;
eip_apu_recv(conn, &apu, timeout);

// Dispatch based on message type
switch (apu->apu_type) {
    case EIP_LIST_IDENTITY_RESP_TYPE:
        handle_list_identity_response((eip_list_identity_resp_t*)apu);
        break;
    // ... other message types
}
```

## CIP Path Segment Types

The API supports all standard CIP segment types through type-specific structures:

- **Port Segments**: Route to backplane slots, communication ports
- **Class Segments**: Identify CIP object classes
- **Instance Segments**: Identify specific instances of a class
- **Member Segments**: Access attributes/members of an instance
- **Connection Segments**: For connected messaging
- **Element Segments**: Array element access
- **Symbolic Segments**: Tag names and symbolic references
- **Data Segments**: Raw data for complex routing

All segments are stored in a type-safe union and managed through a PDU-based API.

## Memory Management

Following PTK patterns:
- Create a parent allocator for each operation scope
- All child allocations (buffers, arrays, connections) are freed automatically
- No manual memory management required

```c
void *parent = ptk_alloc_create_parent();
// ... use API functions with parent ...
ptk_alloc_free(parent); // Frees everything automatically
```

## Error Handling

All functions return `ptk_err_t` status codes:
- `PTK_OK`: Success
- `PTK_ERR_*`: Various error conditions with descriptive names

## Thread Safety

- Connections are not thread-safe (single-threaded per connection)
- Discovery operations can be run concurrently
- Multiple parent allocators can be used simultaneously

## Example Usage

See `ethernetip_usage_example.c` for comprehensive examples demonstrating:
- Device discovery
- CIP path construction
- TCP messaging
- Connection management

## Integration

Include the header and link with Protocol Toolkit:

```c
#include "lib/include/ethernetip.h"
```

The implementation handles all EtherNet/IP protocol details internally while providing a clean, application-focused API surface.
