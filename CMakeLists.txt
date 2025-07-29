cmake_minimum_required(VERSION 3.16)
project(protocol_toolkit VERSION 4.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Platform detection
if(APPLE)
    set(PTK_PLATFORM "macos")
    set(PTK_PLATFORM_DIR "include/macos")
    set(PTK_PLATFORM_SOURCES "")
elseif(UNIX)
    set(PTK_PLATFORM "linux")
    set(PTK_PLATFORM_DIR "lib/linux")
    set(PTK_PLATFORM_SOURCES "src/lib/linux/protocol_toolkit_linux.c")
    find_package(Threads REQUIRED)
else()
    set(PTK_PLATFORM "generic")
    set(PTK_PLATFORM_DIR "include")
    set(PTK_PLATFORM_SOURCES "")
endif()

message(STATUS "Building for platform: ${PTK_PLATFORM}")

# ========================================================================
# PROTOCOL TOOLKIT LIBRARY
# ========================================================================

# Create the main library target
add_library(protocol_toolkit INTERFACE)

# Set include directories based on platform
target_include_directories(protocol_toolkit INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/${PTK_PLATFORM_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Platform-specific configuration
if(PTK_PLATFORM STREQUAL "linux")
    # Linux requires pthread
    target_link_libraries(protocol_toolkit INTERFACE Threads::Threads)

    # Add the implementation source for Linux
    add_library(protocol_toolkit_impl STATIC ${PTK_PLATFORM_SOURCES})
    target_include_directories(protocol_toolkit_impl PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/${PTK_PLATFORM_DIR}>
    )
    target_link_libraries(protocol_toolkit_impl PUBLIC Threads::Threads)
    target_link_libraries(protocol_toolkit INTERFACE protocol_toolkit_impl)

elseif(PTK_PLATFORM STREQUAL "macos")
    # macOS requires system frameworks
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(protocol_toolkit INTERFACE ${FOUNDATION_FRAMEWORK})
endif()

# Compiler-specific flags
target_compile_options(protocol_toolkit INTERFACE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# ========================================================================
# EXAMPLES
# ========================================================================

# Function to add an example
function(add_ptk_example name source_file)
    add_executable(${name} ${source_file})
    target_link_libraries(${name} PRIVATE protocol_toolkit)

    # Platform-specific example configuration
    if(PTK_PLATFORM STREQUAL "linux")
        target_link_libraries(${name} PRIVATE protocol_toolkit_impl)
    endif()

    # Set output directory
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
    )
endfunction()

# Add all examples
add_ptk_example(simple_tcp_protothread "src/examples/simple_tcp_protothread.c")
add_ptk_example(tcp_client_protothread_example "src/examples/tcp_client_protothread_example.c")
add_ptk_example(embedded_pattern_example "src/examples/embedded_pattern_example.c")

# Platform-specific examples
if(PTK_PLATFORM STREQUAL "linux")
    add_ptk_example(linux_example "src/examples/linux_example.c")
endif()

# ========================================================================
# TESTS
# ========================================================================

# Function to add a test
function(add_ptk_test name source_file)
    add_executable(${name} ${source_file})
    target_link_libraries(${name} PRIVATE protocol_toolkit)

    # Platform-specific test configuration
    if(PTK_PLATFORM STREQUAL "linux")
        target_link_libraries(${name} PRIVATE protocol_toolkit_impl)
    endif()

    # Set output directory
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    # Register with CTest
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Add all tests
add_ptk_test(test_basic_functionality "src/tests/test_basic_functionality.c")
add_ptk_test(test_protothread_macros "src/tests/test_protothread_macros.c")

# ========================================================================
# CUSTOM TARGETS
# ========================================================================

# Run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_basic_functionality test_protothread_macros
    COMMENT "Running all Protocol Toolkit tests"
)

# Run all examples
add_custom_target(run_examples
    COMMENT "Running all Protocol Toolkit examples"
)

# Add example dependencies
add_dependencies(run_examples
    simple_tcp_protothread
    tcp_client_protothread_example
    embedded_pattern_example
)

if(PTK_PLATFORM STREQUAL "linux")
    add_dependencies(run_examples linux_example)
endif()

# ========================================================================
# BUILD SUMMARY
# ========================================================================

message(STATUS "")
message(STATUS "========================================================================")
message(STATUS "Protocol Toolkit Build Configuration")
message(STATUS "========================================================================")
message(STATUS "Platform: ${PTK_PLATFORM}")
message(STATUS "Include Directory: ${PTK_PLATFORM_DIR}")
message(STATUS "")
message(STATUS "Available targets:")
message(STATUS "  run_tests                   - Run all tests using CTest")
message(STATUS "  run_examples                - Build all examples")
message(STATUS "  test_basic_functionality    - Basic functionality tests")
message(STATUS "  test_protothread_macros     - Protothread macro tests")
message(STATUS "  simple_tcp_protothread      - Simple TCP example")
message(STATUS "  tcp_client_protothread_example - TCP client example")
message(STATUS "  embedded_pattern_example   - Embedded pattern demo")
if(PTK_PLATFORM STREQUAL "linux")
    message(STATUS "  protocol_toolkit_impl      - Linux implementation")
    message(STATUS "  linux_example              - Linux-specific example")
endif()
message(STATUS "")
message(STATUS "Build commands:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make run_tests")
message(STATUS "  make run_examples")
message(STATUS "========================================================================")
message(STATUS "")
