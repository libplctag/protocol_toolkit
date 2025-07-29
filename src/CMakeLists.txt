cmake_minimum_required(VERSION 3.16)
project(ProtocolToolkit
    VERSION 1.0.0
    DESCRIPTION "Protocol Toolkit - Event-driven framework for industrial network protocols"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Configure output directories to build/
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Configure object file output
set(CMAKE_OBJECT_PATH_MAX 260)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Build options
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" ON)
option(ENABLE_MACOS "Build macOS kevent implementation" ON)

# Platform detection
if(APPLE AND ENABLE_MACOS)
    set(PLATFORM_DIR "macos")
    set(PLATFORM_NAME "macOS")
else()
    message(FATAL_ERROR "No supported platform implementation available")
endif()

# Include the platform-specific implementation
add_subdirectory(${PLATFORM_DIR})

# Build examples if requested
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Build tests if requested
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Protocol Toolkit Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Platform: ${PLATFORM_NAME}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  C Flags: ${CMAKE_C_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "  Debug flags: ${CMAKE_C_FLAGS_DEBUG}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "  Release flags: ${CMAKE_C_FLAGS_RELEASE}")
endif()
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Output directories:")
message(STATUS "    Binaries: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "    Libraries: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
message(STATUS "")
