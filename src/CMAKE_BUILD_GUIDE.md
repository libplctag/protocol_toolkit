# Protocol Toolkit CMake Build System

## Overview

The Protocol Toolkit uses CMake as its build system to provide cross-platform compilation, testing, and packaging. This document describes how to build, test, and use the library.

## Project Structure

```
src/
├── CMakeLists.txt           # Main CMake configuration
├── macos/                   # macOS-specific implementation
│   ├── CMakeLists.txt       # macOS platform configuration
│   ├── include/
│   │   └── protocol_toolkit.h
│   ├── src/
│   │   └── protocol_toolkit_macos.c
│   └── cmake/
│       └── ProtocolToolkitConfig.cmake.in
├── examples/                # Example applications
│   ├── CMakeLists.txt
│   └── example_tcp_client.c
└── tests/                   # Test suite
    ├── CMakeLists.txt
    ├── test_basic_functionality.c
    └── test_state_machine.c
```

## Build Process

### Prerequisites

- CMake 3.16 or later
- C99-compatible compiler (Clang, GCC)
- macOS (for the current implementation)

### Basic Build

```bash
# Navigate to the source directory
cd src/

# Create and enter build directory
mkdir build && cd build

# Configure the project
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --parallel

# Run tests
ctest --output-on-failure
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_EXAMPLES` | `ON` | Build example applications |
| `BUILD_TESTS` | `ON` | Build test suite |
| `ENABLE_MACOS` | `ON` | Enable macOS kevent implementation |
| `CMAKE_BUILD_TYPE` | `Release` | Build type (Debug, Release, RelWithDebInfo, MinSizeRel) |

Example with custom options:
```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr/local
```

## Output Structure

All build artifacts are organized in the `build/` directory:

```
build/
├── bin/
│   ├── examples/
│   │   └── example_tcp_client     # Example executable
│   └── tests/
│       ├── test_basic_functionality # Test executables
│       └── test_state_machine
└── lib/
    └── libprotocol_toolkit_macos.a    # Static library
```

## Testing with CTest

The project uses CTest for automated testing:

```bash
# Run all tests
ctest

# Run tests with verbose output
ctest --verbose

# Run tests with output on failure
ctest --output-on-failure

# Run specific test
ctest -R BasicFunctionality

# Run tests with specific label
ctest -L unit
```

### Test Categories

- **unit**: Unit tests for individual components
- Tests are automatically discovered and configured
- Timeout: 10 seconds per test
- Pass criteria: "All tests passed" in output

## Custom Targets

Several custom targets are available for convenience:

```bash
# Run all tests (equivalent to ctest)
cmake --build . --target run_tests

# Build and show example location
cmake --build . --target run_examples
```

## Installation

Install the library and headers system-wide:

```bash
# Install to default location (/usr/local)
sudo cmake --install .

# Install to custom location
cmake --install . --prefix /opt/protocol_toolkit
```

Installed files:
- `lib/libprotocol_toolkit_macos.a` - Static library
- `include/protocol_toolkit.h` - Header file
- `bin/examples/example_tcp_client` - Example application
- `lib/cmake/ProtocolToolkit/` - CMake config files

## Using the Library

### In CMake Projects

```cmake
find_package(ProtocolToolkit REQUIRED)
target_link_libraries(your_target ProtocolToolkit::protocol_toolkit_macos)
```

### Manual Compilation

```bash
gcc -I/usr/local/include \
    -L/usr/local/lib \
    -lprotocol_toolkit_macos \
    your_source.c -o your_program
```

## Development Workflow

### Adding New Tests

1. Create test file in `src/tests/`
2. Add executable and test in `src/tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_new_feature test_new_feature.c)
   target_link_libraries(test_new_feature ProtocolToolkit::macos)
   add_test(NAME NewFeature COMMAND test_new_feature)
   ```

### Adding New Examples

1. Create example file in `src/examples/`
2. Add executable in `src/examples/CMakeLists.txt`:
   ```cmake
   add_executable(example_new_feature example_new_feature.c)
   target_link_libraries(example_new_feature ProtocolToolkit::macos)
   ```

### Platform Implementations

To add a new platform:

1. Create platform directory (e.g., `linux/`, `windows/`)
2. Implement platform-specific source files
3. Create platform-specific `CMakeLists.txt`
4. Update main `CMakeLists.txt` with platform detection

## Continuous Integration

The CMake configuration supports CI/CD workflows:

```yaml
# Example GitHub Actions workflow
- name: Configure CMake
  run: cmake -B build -DCMAKE_BUILD_TYPE=Release

- name: Build
  run: cmake --build build --parallel

- name: Test
  run: cd build && ctest --output-on-failure
```

## Troubleshooting

### Common Issues

1. **Missing CMake**: Install with `brew install cmake` on macOS
2. **Compiler warnings**: Use `-DCMAKE_C_FLAGS="-Wno-unused-parameter"` to suppress
3. **Test failures**: Run with `ctest --verbose` to see detailed output
4. **Build artifacts in source**: Always build in separate `build/` directory

### Clean Build

```bash
# Remove build directory completely
rm -rf build/

# Reconfigure and rebuild
mkdir build && cd build
cmake .. && cmake --build .
```

## Configuration Files

### CMake Cache Variables

View current configuration:
```bash
cmake -L .        # List cache variables
cmake -LA .       # List all variables including advanced
ccmake .          # Interactive configuration (if available)
```

### Generator Selection

```bash
# Use Ninja (faster builds)
cmake .. -G Ninja

# Use Xcode (macOS)
cmake .. -G Xcode

# Use Unix Makefiles (default)
cmake .. -G "Unix Makefiles"
```

This CMake build system provides a robust, portable foundation for the Protocol Toolkit while maintaining the zero-allocation design principles and embedded-friendly characteristics of the library.
