# Protocol Toolkit

**WARNING** This is note even close to being usable!!!  DO NOT USE.

A C library designed to make industrial network protocol implementations **simple**, **clear**, and **bulletproof**.

## Project Goals

The Protocol Toolkit is built around three core principles:

### **Simple**
- **Global managers** for common resources (timers, sockets, memory)
- **Minimal APIs** with just the functions you need
- **Single-protocol focus** - optimized for applications implementing one protocol
- **No complex abstractions** - direct, straightforward interfaces
- **Simplified resource handling** with allocators that support destructors
- **Separation of long-term vs. ephemeral data** for optimal memory management

### **Clear**
- **Type-safe serialization** using Protocol Description Language (PDL)
- **Structured protocol definitions** that are easy to read and maintain
- **Consistent error handling** throughout the library
- **Comprehensive documentation** with real-world examples
- **Cross-platform support** for common tasks with unified APIs
- **Built-in serialization/deserialization** that handles endianness and validation

### **Bulletproof**
- **Memory safety** with built-in buffer management and automatic cleanup
- **Robust error handling** with clear error codes and recovery paths
- **Timeout management** for all network operations
- **Extensive testing** to ensure reliability in industrial environments
- **Thread-safe primitives** for low-level concurrent operations
- **Platform abstraction** that works consistently across operating systems

## Why Protocol Toolkit?

Industrial network protocols are complex, but implementing them shouldn't be. Traditional approaches often result in:
- Fragile, hard-to-maintain code
- Memory leaks and buffer overflows
- Inconsistent error handling
- Poor performance under load

Protocol Toolkit solves these problems by providing:
- **Pre-built, tested components** for common protocol needs
- **Type-safe message serialization** that eliminates parsing errors
- **Unified event handling** for timers, sockets, and protocol events
- **Clear patterns** that make protocol code predictable and maintainable

## Supported Protocols

Currently supported protocols:
- **Modbus TCP/RTU** - Complete implementation with client and server
- **EtherNet/IP** - Device discovery and basic CIP messaging
- **More protocols planned** based on community needs

## Quick Start

```c
#include "ptk_timer.h"
#include "ptk_modbus.h"

// Initialize the toolkit
ptk_timer_init();
ptk_modbus_init();

// Create a simple Modbus client
ptk_modbus_client_t client = ptk_modbus_client_create("192.168.1.100", 502);

// Read holding registers with automatic timeout
uint16_t values[10];
int result = ptk_modbus_read_holding_registers(client, 0, 10, values);

if (result == 0) {
    printf("Successfully read %d registers\n", 10);
} else {
    printf("Read failed: %s\n", ptk_modbus_error_string(result));
}

// Cleanup is automatic
ptk_modbus_client_destroy(client);
```

## Key Features

### **Protocol-Agnostic Components**
- **Global timer system** - Simple, efficient timers for timeouts and periodic tasks
- **Smart memory management** - Allocators with destructors for automatic cleanup
- **Event framework** - Unified handling of socket and timer events
- **Error handling** - Consistent error codes and recovery mechanisms
- **Cross-platform threading** - Mutexes, condition variables, and thread primitives

### **Protocol-Specific APIs**
- **Modbus** - Complete client/server with all function codes
- **EtherNet/IP** - Device discovery, explicit messaging, and I/O
- **Type-safe parsing** - PDL-generated serialization code

### **Development Tools**
- **Protocol Description Language** - Generate serialization code from simple definitions
- **Code generator** - Automatic creation of type-safe protocol handlers
- **Testing utilities** - Built-in support for protocol testing and validation

### **Cross-Platform Support**
- **Logging system** - Unified logging with configurable levels and outputs
- **Buffer management** - Efficient, safe buffer operations with bounds checking
- **Serialization/deserialization** - Automatic handling of endianness and data types
- **Threading primitives** - Mutexes, condition variables, and thread-safe operations
- **Platform abstraction** - Windows, Linux, macOS, and embedded systems support

## Architecture

The toolkit is designed with clear separation of concerns:

```
Application Layer
    ├── Protocol APIs (Modbus, EtherNet/IP, etc.)
    └── Protocol-Specific Logic

Protocol Toolkit Core
    ├── Timer System (ptk_timer.h)
    ├── Memory Management (ptk_mem.h)
    ├── Buffer Management (ptk_buf.h)
    ├── Event Framework (ptk_event.h)
    ├── Logging System (ptk_log.h)
    ├── Threading Support (ptk_thread.h)
    ├── Serialization (ptk_serialize.h)
    └── Error Handling (ptk_error.h)

System Layer
    ├── Socket Operations
    ├── Threading Primitives
    └── Platform Abstraction
```

## Examples

### Timer Usage
```c
// Create a timeout for a protocol request
ptk_timer_t timeout = ptk_timer_create_oneshot(5000, handle_timeout, &request);

// Create a keepalive timer
ptk_timer_t keepalive = ptk_timer_create_repeating(30000, send_keepalive, connection);

// Cancel when no longer needed
ptk_timer_cancel(timeout);
```

### Type-Safe Serialization
```c
// Define protocol structure in PDL
struct ModbusRequest {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t quantity;
};

// Generated serialization code handles endianness and validation
ptk_buf_t buffer = ptk_buf_create(256);
modbus_request_serialize(&request, buffer);
```

### Resource Management with Destructors


### Cross-Platform Threading
```c
// Create mutex for thread-safe operations
ptk_mutex_t mutex = ptk_mutex_create();

// Use condition variables for coordination
ptk_cond_t cond = ptk_cond_create();

// Thread-safe logging
ptk_log_info("Connection established to %s:%d", host, port);
ptk_log_error("Failed to read register %d: %s", reg, ptk_error_string(err));

// Cleanup is automatic
ptk_mutex_destroy(mutex);
ptk_cond_destroy(cond);
```

### Buffer Management
```c
// Create buffer with bounds checking
ptk_buf_t buf = ptk_buf_create(1024);

// Safe operations with automatic bounds checking
ptk_buf_write_uint16_be(buf, transaction_id);  // Big-endian
ptk_buf_write_uint16_le(buf, register_count);  // Little-endian
ptk_buf_write_bytes(buf, data, data_len);

// Automatic cleanup
ptk_buf_destroy(buf);
```

## Building

```bash
# Clone the repository
git clone https://github.com/your-org/protocol_toolkit.git
cd protocol_toolkit

# Build with CMake
mkdir build
cd build
cmake ..
make

# Run tests
make test

# Install
make install
```

## Version Management

This project uses an automated versioning system with semantic versioning (major.minor.patch). See [VERSIONING.md](VERSIONING.md) for complete details.

### Quick Version Commands

```bash
# Get current version
./scripts/version_manager.sh get

# Set major/minor version (resets patch to 0)
./scripts/version_manager.sh set-major-minor 2 0

# Increment patch version
./scripts/version_manager.sh increment-patch

# Update all project files with current version
./scripts/version_manager.sh update-files
```

### Release Process

1. **Development**: Work on `dev` branch
2. **Staging**: Merge to `prerelease` branch for testing
3. **Validation**: Automated tests run and create PR to `release` branch
4. **Release**: Manual merge to `release` branch triggers automatic release creation
5. **Versioning**: Patch version auto-increments for next development cycle

## Status

**Current Status**: Alpha Development

The Protocol Toolkit is under active development. Current focus areas:
- ✅ Core timer and memory management APIs
- ✅ Modbus TCP/RTU implementation
- ✅ EtherNet/IP device discovery
- 🔄 Smart allocators with destructors
- 🔄 Cross-platform threading primitives
- 🔄 Type-safe serialization system
- 🔄 Unified logging system
- 🔄 Enhanced buffer management
- 🔄 Comprehensive error handling
- 📋 Complete EtherNet/IP implementation
- 📋 Additional protocol support

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

Key areas where we need help:
- Protocol implementations
- Cross-platform testing and validation
- Documentation and examples
- Performance optimization
- Platform-specific optimizations

## License

Protocol Toolkit is dual-licensed:
- **Mozilla Public License 2.0** (MPL-2.0) - See [LICENSE-1.MPL](LICENSE-1.MPL)
- **GNU Lesser General Public License 2.1** (LGPL-2.1) - See [LICENSE-2.LGPL](LICENSE-2.LGPL)

Choose the license that best fits your project's needs.

## Used By

- **libplctag** - Popular PLC communication library
- Your project here - let us know if you're using Protocol Toolkit!

---

*Making industrial network protocols simple, clear, and bulletproof.*
