# CMakeLists.txt for ev_loop library

cmake_minimum_required(VERSION 3.10)

# Event loop library
set(EV_LOOP_SOURCES
    ev_loop.c
    ev_loop.h
    ev_loop_platform.h
    ev_loop_threading.c
)

# Add platform-specific source files based on the target platform
if(WIN32)
    list(APPEND EV_LOOP_SOURCES ev_loop_iocp.c)
    message(STATUS "Using Windows IOCP implementation for ev_loop")
elseif(APPLE OR CMAKE_SYSTEM_NAME MATCHES "Darwin")
    list(APPEND EV_LOOP_SOURCES ev_loop_kqueue.c)
    message(STATUS "Using macOS kqueue implementation for ev_loop")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND EV_LOOP_SOURCES ev_loop_epoll.c)
    message(STATUS "Using Linux epoll implementation for ev_loop")
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD")
    list(APPEND EV_LOOP_SOURCES ev_loop_kqueue.c)
    message(STATUS "Using BSD kqueue implementation for ev_loop")
else()
    # Default to kqueue for unknown Unix-like systems
    list(APPEND EV_LOOP_SOURCES ev_loop_kqueue.c)
    message(WARNING "Unknown platform, defaulting to kqueue implementation for ev_loop")
endif()

# Create the ev_loop static library
add_library(ev_loop STATIC ${EV_LOOP_SOURCES})

# Link dependencies
target_link_libraries(ev_loop buf log)

# Platform-specific libraries for ev_loop
if(WIN32)
    target_link_libraries(ev_loop ws2_32)
elseif(UNIX)
    target_link_libraries(ev_loop pthread)
    
    # Find required libraries for Linux
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_library(RT_LIBRARY rt)
        if(RT_LIBRARY)
            target_link_libraries(ev_loop ${RT_LIBRARY})
        endif()
    endif()
endif()

# Include directories
target_include_directories(ev_loop PUBLIC .)
target_include_directories(ev_loop PRIVATE ../utils)

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ev_loop PRIVATE -Wall -Wextra -Wno-unused-parameter)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(ev_loop PRIVATE /W3)
endif()

# Export the target for use by parent CMakeLists
set_target_properties(ev_loop PROPERTIES
    PUBLIC_HEADER "ev_loop.h;ev_loop_platform.h"
)

message(STATUS "ev_loop library configured with sources: ${EV_LOOP_SOURCES}")