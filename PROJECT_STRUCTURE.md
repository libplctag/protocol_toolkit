# Protocol Toolkit Project Structure

## Overview

The Protocol Toolkit has been reorganized into a clean, platform-aware structure with proper separation of concerns.

## Directory Structure

```
protocol_toolkit/
├── CMakeLists.txt              # Main CMake configuration
├── src/
│   ├── examples/               # Platform-agnostic examples
│   │   ├── simple_tcp_protothread.c
│   │   ├── tcp_client_protothread_example.c
│   │   ├── embedded_pattern_example.c
│   │   └── linux_example.c    # Platform-specific example
│   ├── tests/                  # Test suite
│   │   ├── test_basic_functionality.c
│   │   └── test_protothread_macros.c
│   ├── include/                # Generic/fallback headers
│   │   ├── protocol_toolkit.h
│   │   └── macos/              # macOS-specific implementation
│   │       └── protocol_toolkit.h
│   └── lib/                    # Platform-specific implementations
│       └── linux/              # Linux-specific implementation
│           ├── protocol_toolkit.h
│           ├── protocol_toolkit_linux.c
│           └── README.md
└── examples/                   # Old examples directory (to be removed)
```

## Building

### macOS
```bash
mkdir build && cd build
cmake ..
make
```

### Linux
```bash
mkdir build && cd build
cmake ..
make
```

## Available Targets

### Libraries
- `protocol_toolkit` - Main interface library (header-only for macOS, links impl for Linux)
- `protocol_toolkit_impl` - Linux implementation library (Linux only)

### Examples
- `simple_tcp_protothread` - Simple TCP client using protothreads
- `tcp_client_protothread_example` - More comprehensive TCP example
- `embedded_pattern_example` - Demonstrates embedded protothread pattern
- `linux_example` - Linux-specific example (Linux only)

### Tests
- `test_basic_functionality` - Basic API functionality tests
- `test_protothread_macros` - Protothread macro and embedded pattern tests

### Convenience Targets
- `run_tests` - Run all tests using CTest
- `run_examples` - Build all examples

## Platform Differences

### Headers
- **macOS**: `src/include/macos/protocol_toolkit.h` (uses GCD)
- **Linux**: `src/lib/linux/protocol_toolkit.h` (uses epoll)
- **Generic**: `src/include/protocol_toolkit.h` (step-based protothreads)

### Implementation
- **macOS**: Header-only (built on GCD/Foundation)
- **Linux**: Requires `protocol_toolkit_linux.c` compilation
- **Generic**: Header-only with basic functionality

### Example Usage

All examples automatically include the correct platform header:

```c
#ifdef __APPLE__
    #include "../include/macos/protocol_toolkit.h"
#elif __linux__
    #include "../lib/linux/protocol_toolkit.h"
#else
    #include "../include/protocol_toolkit.h"
#endif
```

## Testing

```bash
# Build and run all tests
make run_tests

# Or run individual tests
./build/tests/test_basic_functionality
./build/tests/test_protothread_macros

# Using CTest
cd build && ctest --output-on-failure
```

## Cross-Platform Compatibility

The same application code works identically across platforms:

```c
typedef struct {
    ptk_pt_t pt;        // Must be first field
    ptk_handle_t socket;
    ptk_buffer_t buffer;
    uint8_t data[1024];
} my_app_t;

void my_protothread(ptk_pt_t *pt) {
    my_app_t *app = (my_app_t *)pt;

    PT_BEGIN(pt);
    PTK_PT_TCP_CONNECT(pt, app->socket, "server.com", 80);
    PTK_PT_TCP_SEND(pt, app->socket, &app->buffer);
    PTK_PT_TCP_RECEIVE(pt, app->socket, &app->buffer);
    PT_END(pt);
}
```

Only the include changes - the API is identical!

## Key Features

✅ **Zero Code Changes** - Same source works on all platforms
✅ **Platform Optimized** - GCD on macOS, epoll on Linux
✅ **Embedded Pattern** - Type-safe protothread context casting
✅ **Comprehensive Tests** - Automated testing of all functionality
✅ **CMake Integration** - Modern CMake with proper target exports
✅ **Clean Structure** - Logical separation of examples, tests, and implementation
