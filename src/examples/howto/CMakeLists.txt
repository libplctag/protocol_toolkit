cmake_minimum_required(VERSION 3.16)

project(protocol_toolkit_howto)

# Add CRC utility library
add_library(crc_utils STATIC
    crc.c
    crc.h
)

# Add arithmetic protocol library  
add_library(arithmetic_protocol STATIC
    arithmetic_protocol.c
    arithmetic_protocol.h
)

target_link_libraries(arithmetic_protocol 
    PUBLIC 
        crc_utils 
        ptk::ptk
)

# Server executable
add_executable(arithmetic_server
    server.c
)

target_link_libraries(arithmetic_server
    PRIVATE
        arithmetic_protocol
        ptk::ptk
        m
)

# Client executable
add_executable(arithmetic_client
    client.c
)

target_link_libraries(arithmetic_client
    PRIVATE
        arithmetic_protocol  
        ptk::ptk
        m
)

# Compiler flags for debug builds
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Installation
install(TARGETS arithmetic_server arithmetic_client
    RUNTIME DESTINATION bin
)

install(FILES howto.md
    DESTINATION share/doc/protocol_toolkit/examples
)