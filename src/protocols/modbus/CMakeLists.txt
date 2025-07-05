# CMakeLists.txt for Modbus protocol
cmake_minimum_required(VERSION 3.10)

# Modbus protocol library
add_library(modbus STATIC
    protocol/modbus_defs.c
    protocol/modbus_defs.h
)

# Include directories
target_include_directories(modbus PUBLIC protocol)
target_include_directories(modbus PRIVATE ../../protocol_toolkit/utils)
target_include_directories(modbus PRIVATE ../../protocol_toolkit/ev_loop)

# Link dependencies
target_link_libraries(modbus buf log ev_loop)

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(modbus PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(modbus PRIVATE /W3)
endif()

# Export the target
set_target_properties(modbus PROPERTIES
    PUBLIC_HEADER "protocol/modbus.h"
)

# Example applications subdirectory
add_subdirectory(examples)

message(STATUS "Modbus protocol library configured")