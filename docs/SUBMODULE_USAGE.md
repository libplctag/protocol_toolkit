# Using Protocol Toolkit as a Git Submodule

This document describes how to use Protocol Toolkit as a Git submodule or linked repository in your project.

## Changes Made for Submodule Support

The following changes have been made to ensure Protocol Toolkit works correctly when included as a submodule:

### 1. **Main Project Detection**
- Added automatic detection of whether PTK is the main project or a subproject
- Uses `CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR` test

### 2. **Conditional Building**
- **PTK_BUILD_EXAMPLES**: Controls whether examples are built (default: only in main project)
- **PTK_BUILD_TESTS**: Controls whether tests are built (default: only in main project)  
- **PTK_INSTALL**: Controls whether installation rules are processed (default: only in main project)

### 3. **Target-Based Include Directories**
- Replaced global `include_directories()` with target-specific `target_include_directories()`
- Uses generator expressions for build vs install interface paths
- Eliminates pollution of parent project include paths

### 4. **Namespaced Targets**
- Main library target renamed from `protocol_toolkit` to `ptk`
- Added alias `ptk::ptk` for namespaced usage
- Prevents target name conflicts with parent projects

### 5. **Fixed Path References**
- Replaced all `CMAKE_SOURCE_DIR` with `CMAKE_CURRENT_SOURCE_DIR` where appropriate
- Uses relative paths from current source directory
- Works correctly regardless of project hierarchy

### 6. **CMake Package Configuration**
- Added proper CMake config files for `find_package(PTK)` support
- Includes version compatibility and dependency handling
- Supports both installed and build-tree usage

## Using as a Submodule

### 1. **Add as Submodule**
```bash
# Add Protocol Toolkit as a submodule
git submodule add https://github.com/libplctag/protocol_toolkit.git external/protocol_toolkit
git submodule update --init --recursive
```

### 2. **CMakeLists.txt Integration**

#### Option A: Direct Integration (Recommended)
```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.18)
project(my_project)

# Add Protocol Toolkit as subdirectory
add_subdirectory(external/protocol_toolkit)

# Create your executable
add_executable(my_app main.c)

# Link against Protocol Toolkit
target_link_libraries(my_app PRIVATE ptk::ptk)
```

#### Option B: With Custom Options
```cmake
# Your project's CMakeLists.txt  
cmake_minimum_required(VERSION 3.18)
project(my_project)

# Optionally configure PTK options before adding subdirectory
set(PTK_BUILD_EXAMPLES OFF CACHE BOOL "Don't build PTK examples")
set(PTK_BUILD_TESTS OFF CACHE BOOL "Don't build PTK tests")

# Add Protocol Toolkit
add_subdirectory(external/protocol_toolkit)

# Use in your project
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE ptk::ptk)
```

### 3. **Using the API**
```c
// Your source code
#include <ptk.h>
#include <ptk_sock.h>
#include <ptk_buf.h>
// ... other PTK headers as needed

int main() {
    // Initialize PTK
    ptk_err_t err = ptk_startup();
    if (err != PTK_OK) {
        return 1;
    }
    
    // Use PTK APIs...
    
    // Cleanup
    ptk_shutdown();
    return 0;
}
```

## Available Targets

When Protocol Toolkit is included as a submodule, the following targets are available:

- **`ptk::ptk`**: Main Protocol Toolkit library (preferred, namespaced)
- **`ptk`**: Main Protocol Toolkit library (alternative name)

## Conditional Features

### Examples (PTK_BUILD_EXAMPLES)
When enabled, builds example programs:
- `arithmetic_server`: Example arithmetic protocol server
- `arithmetic_client`: Example arithmetic protocol client
- `modbus_*`: Modbus protocol examples (if available)

### Tests (PTK_BUILD_TESTS)  
When enabled, builds test suite:
- All `test_ptk_*` test executables
- CTest integration with Valgrind support on Linux

### Installation (PTK_INSTALL)
When enabled, provides installation rules:
- Headers installed to `include/`
- Library installed to `lib/`
- CMake config files for `find_package(PTK)`

## Best Practices

### 1. **Target Usage**
Always use the namespaced target:
```cmake
# Good
target_link_libraries(my_app PRIVATE ptk::ptk)

# Avoid  
target_link_libraries(my_app PRIVATE ptk)
```

### 2. **Option Configuration**
Set PTK options before adding the subdirectory:
```cmake
# Configure before add_subdirectory
set(PTK_BUILD_EXAMPLES OFF)
add_subdirectory(external/protocol_toolkit)
```

### 3. **Include Headers**
No need for manual include directories - use the target:
```cmake
# This is sufficient - no manual include_directories needed
target_link_libraries(my_app PRIVATE ptk::ptk)
```

### 4. **Platform Libraries**
Platform-specific libraries (pthread, ws2_32, etc.) are automatically linked through the `ptk::ptk` target.

## Troubleshooting

### Build Errors
If you encounter build errors:

1. **Check CMake Version**: Ensure you're using CMake 3.18 or later
2. **Clean Build**: Delete build directory and rebuild
3. **Option Conflicts**: Ensure PTK options are set before `add_subdirectory()`

### Include Path Issues
If headers aren't found:

1. **Use Target**: Link against `ptk::ptk` instead of manual include paths
2. **Check Build Order**: Ensure PTK is added before your targets that use it

### Target Not Found
If `ptk::ptk` target isn't available:

1. **Check Subdirectory**: Ensure `add_subdirectory(external/protocol_toolkit)` is called
2. **Check Path**: Verify the submodule path is correct
3. **Submodule Init**: Run `git submodule update --init --recursive`

## Migration from Standalone Build

If migrating from standalone Protocol Toolkit usage:

### Before (Standalone)
```cmake
find_package(PTK REQUIRED)
target_link_libraries(my_app PRIVATE ${PTK_LIBRARIES})
target_include_directories(my_app PRIVATE ${PTK_INCLUDE_DIRS})
```

### After (Submodule)
```cmake
add_subdirectory(external/protocol_toolkit)
target_link_libraries(my_app PRIVATE ptk::ptk)
```

## Example Project Structure

```
my_project/
├── CMakeLists.txt
├── src/
│   └── main.c
├── external/
│   └── protocol_toolkit/          # Git submodule
│       ├── CMakeLists.txt
│       ├── src/
│       │   ├── include/
│       │   └── lib/
│       └── ...
└── build/                         # Build directory
```

This setup provides a clean, self-contained build that doesn't interfere with your main project while providing all the benefits of Protocol Toolkit.