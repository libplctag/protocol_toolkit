# CMakeLists.txt for protocol_toolkit
cmake_minimum_required(VERSION 3.10)

# Protocol toolkit utilities
set(PROTOCOL_TOOLKIT_SOURCES
    utils/buf.c
    utils/buf.h
    utils/log.c
    utils/log.h
)

# Buffer utility library
add_library(buf STATIC
    utils/buf.c
    utils/buf.h
)

# Logging utility library  
add_library(log STATIC
    utils/log.c
    utils/log.h
)

# Add subdirectories
add_subdirectory(ev_loop)

# Include directories
target_include_directories(buf PUBLIC utils)
target_include_directories(log PUBLIC utils)

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(buf PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_compile_options(log PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(buf PRIVATE /W3)
    target_compile_options(log PRIVATE /W3)
endif()

# Export targets
set_target_properties(buf PROPERTIES
    PUBLIC_HEADER "utils/buf.h"
)

set_target_properties(log PROPERTIES
    PUBLIC_HEADER "utils/log.h"
)

message(STATUS "Protocol toolkit configured:")
message(STATUS "  - buf: Buffer utilities")
message(STATUS "  - log: Logging utilities")
message(STATUS "  - ev_loop: Event loop (see subdirectory)")