# EtherNet/IP Protocol Specification

**Based on Analysis of libplctag AB Server Implementation**

This document describes the EtherNet/IP protocol implementation requirements for developing a new server using the protocol toolkit APIs. The analysis is based on the reference implementation in `/Users/kyle/Projects/libplctag.worktrees/main/src/tests/ab_server/src`.

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Protocol Stack Architecture](#protocol-stack-architecture)
3. [EtherNet/IP Layer](#ethernetip-layer)
4. [Common Packet Format (CPF) Layer](#common-packet-format-cpf-layer)
5. [Common Industrial Protocol (CIP) Layer](#common-industrial-protocol-cip-layer)
6. [Data Types and Tag Management](#data-types-and-tag-management)
7. [Protocol Flow Patterns](#protocol-flow-patterns)
8. [Implementation Requirements](#implementation-requirements)
9. [Questions for Clarification](#questions-for-clarification)

## Protocol Overview

EtherNet/IP is an industrial networking standard that encapsulates Common Industrial Protocol (CIP) messages within standard Ethernet and TCP/IP packets. The protocol operates on **TCP port 44818** and supports both connected and unconnected messaging patterns.

### Key Characteristics
- **Transport**: TCP/IP over Ethernet
- **Default Port**: 44818
- **Byte Order**: Little-endian throughout
- **Session-based**: Requires session establishment before messaging
- **Multi-layered**: EIP → CPF → CIP → Application Data

## Protocol Stack Architecture

```
┌─────────────────────────────────────┐
│           Application Layer         │
│        (Tag Read/Write/etc.)        │
├─────────────────────────────────────┤
│     Common Industrial Protocol      │
│             (CIP Layer)             │
├─────────────────────────────────────┤
│      Common Packet Format          │
│           (CPF Layer)               │
├─────────────────────────────────────┤
│          EtherNet/IP                │
│           (EIP Layer)               │
├─────────────────────────────────────┤
│             TCP/IP                  │
└─────────────────────────────────────┘
```

## EtherNet/IP Layer

The EtherNet/IP layer provides session management and basic packet routing.

### EIP Header Structure

**Size**: 24 bytes (fixed)

```c
typedef struct {
    uint16_t command;          // EIP command type
    uint16_t length;           // Length of data following header
    uint32_t session_handle;   // Session identifier
    uint32_t status;           // Status/error code
    uint64_t sender_context;   // Client context data (8 bytes)
    uint32_t options;          // Command options
} eip_header_t;
```

**Field Details**:
- **command**: Service command identifier
- **length**: Byte count of data following the EIP header
- **session_handle**: Unique session identifier (0 for Register Session)
- **status**: Error code (0 = success)
- **sender_context**: Client-provided context data (echoed in response)
- **options**: Command-specific option flags

### EIP Commands

| Command | Code | Description |
|---------|------|-------------|
| Register Session | 0x0065 | Establish a new session |
| Unregister Session | 0x0066 | Terminate an existing session |
| Unconnected Send | 0x006F | Send unconnected data |
| Connected Send | 0x0070 | Send connected data |

### Session Management

#### Register Session Flow
1. **Client Request**:
   - command = 0x0065
   - session_handle = 0
   - data = `[version:uint16][options:uint16]`
   - version must be 1

2. **Server Response**:
   - session_handle = generated unique value (non-zero)
   - status = 0 (success)
   - data = `[version:uint16][options:uint16]`

#### Unregister Session Flow
1. **Client Request**:
   - command = 0x0066
   - session_handle = valid session handle

2. **Server Response**:
   - Same session_handle
   - status = 0 (success)

### EIP Status Codes
- **0x00**: Success
- **0x01**: Invalid command
- **0x02**: Invalid length
- **0x64**: Invalid session handle
- **0x65**: Invalid data

## Common Packet Format (CPF) Layer

The CPF layer wraps CIP data and provides addressing information for both connected and unconnected messaging.

### Unconnected CPF Structure

**Size**: 16 bytes (fixed header)

```c
typedef struct {
    uint32_t interface_handle;    // Interface handle (usually 0)
    uint16_t router_timeout;      // Router timeout in seconds
    uint16_t item_count;          // Number of items (always 2)
    uint16_t item_addr_type;      // Address item type
    uint16_t item_addr_length;    // Address item length (usually 0)
    uint16_t item_data_type;      // Data item type
    uint16_t item_data_length;    // Data item length
} cpf_unconnected_t;
```

### Connected CPF Structure

**Size**: 22 bytes (fixed header)

```c
typedef struct {
    uint32_t interface_handle;    // Interface handle
    uint16_t router_timeout;      // Router timeout
    uint16_t item_count;          // Number of items (always 2)
    uint16_t item_addr_type;      // Address item type (0x00A1)
    uint16_t item_addr_length;    // Address item length (always 4)
    uint32_t conn_id;             // Connection ID
    uint16_t item_data_type;      // Data item type (0x00B1)
    uint16_t item_data_length;    // Data item length
    uint16_t conn_seq;            // Connection sequence number
} cpf_connected_t;
```

### CPF Item Types

| Type | Code | Description |
|------|------|-------------|
| NULL Address Item | 0x0000 | No address data |
| Connected Address Item | 0x00A1 | Connected message address |
| Connected Data Item | 0x00B1 | Connected message data |
| Unconnected Data Item | 0x00B2 | Unconnected message data |

## Common Industrial Protocol (CIP) Layer

The CIP layer provides the actual industrial protocol services for tag access and connection management.

### CIP Service Codes

| Service | Code | Description |
|---------|------|-------------|
| Multiple Service Packet | 0x0A | Multiple services in one packet |
| PCCC Execute | 0x4B | Execute PCCC command |
| Read Named Tag | 0x4C | Read tag data |
| Write Named Tag | 0x4D | Write tag data |
| Forward Close | 0x4E | Close connection |
| Read Named Tag Fragment | 0x52 | Read large tag data |
| Write Named Tag Fragment | 0x53 | Write large tag data |
| Forward Open | 0x54 | Open connection (16-bit params) |
| Forward Open Extended | 0x5B | Open connection (32-bit params) |

### CIP Request Structure

```c
typedef struct {
    uint8_t service;              // Service code
    uint8_t request_path_size;    // Path size in 16-bit words
    uint8_t request_path[];       // Padded to even number of bytes
    uint8_t request_data[];       // Service-specific data
} cip_request_t;
```

### CIP Response Structure

```c
typedef struct {
    uint8_t service;              // Service code | 0x80 (response bit)
    uint8_t reserved;             // Reserved (0)
    uint8_t general_status;       // General status code
    uint8_t additional_status_size; // Additional status size in 16-bit words
    uint16_t additional_status[];   // Additional status data
    uint8_t response_data[];        // Service-specific response data
} cip_response_t;
```

### CIP Status Codes

| Status | Code | Description |
|--------|------|-------------|
| Success | 0x00 | Service succeeded |
| Extended Error | 0x01 | Extended error information |
| Invalid Parameter | 0x03 | Invalid parameter in request |
| Path Segment Error | 0x04 | Error in path segment |
| Path Destination Unknown | 0x05 | Path destination not found |
| Fragmentation Required | 0x06 | Data too large, use fragmentation |
| Unsupported Service | 0x08 | Service not supported |
| Insufficient Data | 0x13 | Not enough data provided |
| Too Much Data | 0x15 | Too much data provided |

### Forward Open Connection Process

#### Forward Open Request (0x54 - 16-bit parameters)

```c
typedef struct {
    uint8_t secs_per_tick;               // Timing base
    uint8_t timeout_ticks;               // Connection timeout
    uint32_t server_conn_id;             // Server connection ID (0 in request)
    uint32_t client_conn_id;             // Client connection ID
    uint16_t conn_serial_number;         // Connection serial number
    uint16_t orig_vendor_id;             // Originator vendor ID
    uint32_t orig_serial_number;         // Originator serial number
    uint8_t conn_timeout_multiplier;     // Timeout multiplier
    uint8_t reserved[3];                 // Reserved bytes
    uint32_t client_to_server_rpi;       // Client to server RPI (μs)
    uint16_t client_to_server_conn_params; // Connection parameters
    uint32_t server_to_client_rpi;       // Server to client RPI (μs)
    uint16_t server_to_client_conn_params; // Connection parameters
    uint8_t transport_class;             // Transport class (0xA3)
    uint8_t path_size;                   // Path size in 16-bit words
    uint8_t path[];                      // Connection path (padded)
} forward_open_request_t;
```

#### Forward Open Extended Request (0x5B - 32-bit parameters)

Same structure as above but with 32-bit connection parameter fields:
- `uint32_t client_to_server_conn_params`
- `uint32_t server_to_client_conn_params`

#### Connection Parameter Format

**16-bit Format**:
- Bits 0-8: Packet size (0=508 bytes, 1=4002 bytes)
- Bit 9: Variable length packet
- Bits 10-15: Reserved

**32-bit Format**:
- Bits 0-15: Packet size in bytes
- Bits 16-31: Reserved

## Data Types and Tag Management

### CIP Data Types

| Type | Code | Size | Description |
|------|------|------|-------------|
| BOOL | 0x00C1 | 1 byte | Boolean value |
| SINT | 0x00C2 | 1 byte | Signed 8-bit integer |
| INT | 0x00C3 | 2 bytes | Signed 16-bit integer |
| DINT | 0x00C4 | 4 bytes | Signed 32-bit integer |
| LINT | 0x00C5 | 8 bytes | Signed 64-bit integer |
| REAL | 0x00CA | 4 bytes | IEEE 754 single precision |
| LREAL | 0x00CB | 8 bytes | IEEE 754 double precision |
| STRING | 0x00D0 | 88 bytes | String structure |

### PCCC Data Types (Legacy Support)

| Type | Code | Size | Description |
|------|------|------|-------------|
| INT | 0x89 | 2 bytes | 16-bit signed integer |
| DINT | 0x91 | 4 bytes | 32-bit signed integer |
| REAL | 0x8A | 4 bytes | IEEE 754 single precision |
| STRING | 0x8D | 82 bytes | String with count |

### Tag Definition Structure

```c
typedef struct {
    char *name;                   // Tag name
    uint16_t data_type;          // CIP or PCCC data type
    size_t element_size;         // Size of one element
    size_t element_count;        // Total number of elements
    size_t num_dimensions;       // Number of array dimensions (0-3)
    size_t dimensions[3];        // Array dimensions
    uint8_t *data;              // Tag data storage
} tag_definition_t;
```

### Tag Access Patterns

#### Read Named Tag Request
```c
typedef struct {
    uint8_t service;              // 0x4C
    uint8_t request_path_size;    // Path size in words
    uint8_t request_path[];       // Symbol path (padded)
    uint16_t element_count;       // Number of elements to read
} read_tag_request_t;
```

#### Write Named Tag Request
```c
typedef struct {
    uint8_t service;              // 0x4D
    uint8_t request_path_size;    // Path size in words
    uint8_t request_path[];       // Symbol path (padded)
    uint16_t data_type;           // CIP data type
    uint16_t element_count;       // Number of elements
    uint8_t data[];              // Tag data
} write_tag_request_t;
```

## Protocol Flow Patterns

### Complete Unconnected Transaction

```
1. TCP Connection Established
2. EIP Register Session
   Client → Server: [EIP Header][version=1, options=0]
   Server → Client: [EIP Header][version=1, options=0, session_handle]

3. Unconnected Send with CIP Request
   Client → Server: [EIP Header][CPF Unconnected][CIP Request]
   Server → Client: [EIP Header][CPF Unconnected][CIP Response]

4. EIP Unregister Session
   Client → Server: [EIP Header][empty data]
   Server → Client: [EIP Header][empty data]

5. TCP Connection Closed
```

### Connected Transaction

```
1. Session Establishment (same as unconnected)

2. Forward Open
   Client → Server: [EIP Header][CPF Unconnected][Forward Open Request]
   Server → Client: [EIP Header][CPF Unconnected][Forward Open Response]

3. Connected Data Exchange
   Client → Server: [EIP Header][CPF Connected][CIP Request]
   Server → Client: [EIP Header][CPF Connected][CIP Response]

4. Forward Close
   Client → Server: [EIP Header][CPF Unconnected][Forward Close Request]
   Server → Client: [EIP Header][CPF Unconnected][Forward Close Response]

5. Session Termination (same as unconnected)
```

## Implementation Requirements

### 1. Threading and Concurrency
- **One thread per TCP connection**
- **Thread-safe tag data access** (use mutexes)
- **Session handle generation** must be thread-safe
- **Connection ID generation** must be unique across threads

### 2. Memory Management
- **Safe buffer operations** to prevent overruns
- **Proper cleanup** on connection termination
- **Tag data persistence** across connections

### 3. Error Handling
- **Validate packet lengths** at each layer
- **Proper error responses** with appropriate status codes
- **Graceful handling** of malformed packets
- **Resource cleanup** on errors

### 4. Network Considerations
- **Little-endian byte order** throughout
- **Proper TCP socket management**
- **Connection timeout handling**
- **Large packet fragmentation** support

### 5. Protocol Compliance
- **16-bit word alignment** for CIP paths
- **Proper padding** for odd-length fields
- **Sequence number management** for connected messaging
- **Session validation** on all requests after registration

## Questions for Clarification

### Protocol Implementation Questions

1. **Session Handle Generation**: What algorithm should be used for generating unique session handles? Should they be sequential, random, or use a specific pattern?

2. **Connection ID Management**: How should connection IDs be managed across multiple client connections? Should there be a global pool or per-session allocation?

3. **Tag Data Persistence**: Should tag data persist across server restarts, or is in-memory storage sufficient for this implementation?

4. **Maximum Connections**: What should be the maximum number of concurrent client connections supported?

5. **Timeout Values**: What are the recommended timeout values for:
   - TCP socket operations
   - EIP session timeouts
   - CIP request processing
   - Connected message timeouts

### API Integration Questions

6. **Buffer Management**: The protocol toolkit uses `ptk_buf_t` structures. How should the protocol layers interface with this buffer system for:
   - Building response packets
   - Parsing request packets
   - Managing fragmented data

7. **Threading Model**: The analysis shows the reference uses one thread per connection. Should the new implementation:
   - Use the same model with `ptk_thread.h`
   - Use a thread pool approach
   - Use async/event-driven approach with `ptk_socket.h` timeouts

8. **Error Reporting**: How should protocol-level errors integrate with the `ptk_err` error system? Should there be:
   - Direct mapping of CIP errors to ptk_err codes
   - A separate protocol error system
   - Extended error information passing

### Configuration Questions

9. **PLC Type Support**: Which PLC types should the new implementation support initially:
   - ControlLogix only
   - Multiple types with configurable paths
   - Legacy PCCC support

10. **Tag Configuration**: How should tags be configured:
    - Command-line arguments like the reference
    - Configuration file
    - Runtime creation via protocol messages
    - Combination of approaches

### Performance Questions

11. **Packet Size Limits**: What are the practical limits for:
    - Maximum tag name length
    - Maximum array size for single read/write
    - Maximum packet size before fragmentation

12. **Memory Usage**: Are there constraints on:
    - Total tag data size
    - Per-connection memory usage
    - Buffer allocation strategies

### Security Questions

13. **Input Validation**: What level of input validation is required:
    - Basic packet structure validation
    - Path traversal protection
    - Resource exhaustion protection
    - Authentication/authorization (if any)

14. **Resource Limits**: Should there be limits on:
    - Connection rate
    - Request rate per connection
    - Tag access patterns
    - Memory allocation per client

### Testing Questions

15. **Compatibility Testing**: Should the implementation be tested against:
    - Specific Allen-Bradley clients
    - Open-source clients like libplctag
    - Protocol conformance test suites

16. **Interoperability**: Are there specific client implementations that must be supported, or should standard EtherNet/IP compliance be sufficient?

---

*This document was generated from analysis of the libplctag AB server implementation located at `/Users/kyle/Projects/libplctag.worktrees/main/src/tests/ab_server/src`. The questions above should be addressed before beginning implementation of the new EtherNet/IP server using the protocol toolkit APIs.*