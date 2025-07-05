# CMakeLists.txt for EtherNet/IP protocol
cmake_minimum_required(VERSION 3.10)

# EtherNet/IP protocol library
add_library(ethernetip STATIC
    protocol/ethernetip_defs.c
    protocol/ethernetip_defs.h
)

# Include directories
target_include_directories(ethernetip PUBLIC protocol)
target_include_directories(ethernetip PRIVATE ../../protocol_toolkit/utils)
target_include_directories(ethernetip PRIVATE ../../protocol_toolkit/ev_loop)

# Link dependencies
target_link_libraries(ethernetip buf log)

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ethernetip PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ethernetip PRIVATE /W3)
endif()

# Export the target
set_target_properties(ethernetip PROPERTIES
    PUBLIC_HEADER "protocol/ethernetip_defs.h"
)

# Example applications subdirectory
add_subdirectory(examples)

message(STATUS "EtherNet/IP protocol library configured")