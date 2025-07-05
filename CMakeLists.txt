cmake_minimum_required(VERSION 3.10)
project(serdesgen)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    # GCC and Clang options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    # MSVC options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
    set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 and later
    if(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif()
elseif(APPLE)
    # macOS-specific settings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
elseif(UNIX)
    # Linux/BSD-specific settings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    
    # Find required libraries
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
        set(PLATFORM_LIBS ${PLATFORM_LIBS} ${RT_LIBRARY})
    endif()
endif()

#=============================================================================
# PROTOCOL TOOLKIT (Shared utilities)
#=============================================================================

# Include directories for all projects
include_directories(src/protocol_toolkit/utils)
include_directories(src/protocol_toolkit/ev_loop)

# Add protocol toolkit subdirectory
add_subdirectory(src/protocol_toolkit)

#=============================================================================
# PROTOCOL LIBRARIES
#=============================================================================

# Add protocol subdirectories
add_subdirectory(src/protocols/ethernetip)
add_subdirectory(src/protocols/modbus)

#=============================================================================
# TEST APPLICATIONS  
#=============================================================================

# Timer test executable
add_executable(test_timer
    test_timer.c
)

target_link_libraries(test_timer
    buf
    log
    ev_loop
)

if(WIN32)
    target_link_libraries(test_timer ws2_32)
elseif(UNIX)
    target_link_libraries(test_timer pthread)
    if(PLATFORM_LIBS)
        target_link_libraries(test_timer ${PLATFORM_LIBS})
    endif()
endif()

# UDP + Timer test executable
add_executable(test_udp_timer
    test_udp_timer.c
)

target_link_libraries(test_udp_timer
    buf
    log
    ev_loop
)

if(WIN32)
    target_link_libraries(test_udp_timer ws2_32)
elseif(UNIX)
    target_link_libraries(test_udp_timer pthread)
    if(PLATFORM_LIBS)
        target_link_libraries(test_udp_timer ${PLATFORM_LIBS})
    endif()
endif()

# EtherNet/IP unicast test executable
add_executable(test_ethernetip_unicast
    test_ethernetip_unicast.c
)

target_link_libraries(test_ethernetip_unicast
    buf
    log
    ev_loop
    ethernetip
)

if(WIN32)
    target_link_libraries(test_ethernetip_unicast ws2_32)
elseif(UNIX)
    target_link_libraries(test_ethernetip_unicast pthread)
    if(PLATFORM_LIBS)
        target_link_libraries(test_ethernetip_unicast ${PLATFORM_LIBS})
    endif()
endif()

# Modbus examples are built in src/protocols/modbus/examples

#=============================================================================
# INSTALLATION
#=============================================================================

install(TARGETS ethernetip_server ethernetip_client modbus_server modbus_client
    RUNTIME DESTINATION bin
)

install(FILES
    src/protocol_toolkit/utils/buf.h
    src/protocol_toolkit/utils/log.h
    src/protocol_toolkit/ev_loop/ev_loop.h
    src/protocol_toolkit/ev_loop/ev_loop_platform.h
    src/protocol_toolkit/ev_loop/ev_loop_common.h
    src/protocols/ethernetip/protocol/ethernetip_defs.h
    src/protocols/modbus/protocol/modbus.h
    DESTINATION include
)

#=============================================================================
# TESTING
#=============================================================================

# Add test targets
add_custom_target(test_ethernetip
    COMMENT "Testing EtherNet/IP implementation"
    DEPENDS ethernetip_server ethernetip_client
)

add_custom_target(test_modbus
    COMMENT "Testing Modbus implementation"
    DEPENDS modbus_server modbus_client
)

#=============================================================================
# INFORMATION
#=============================================================================

# Print build information
message(STATUS "Building serdesgen project with new modular structure")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")

# Usage instructions
message(STATUS "")
message(STATUS "Build instructions:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "")
message(STATUS "Run EtherNet/IP examples:")
message(STATUS "  ./bin/ethernetip_server")
message(STATUS "  ./bin/ethernetip_client [broadcast_interval] [discovery_time]")
message(STATUS "")
message(STATUS "Libraries built:")
message(STATUS "  - buf: Buffer utilities with endian-aware typedefs")
message(STATUS "  - log: Logging utilities")
message(STATUS "  - ev_loop: Cross-platform event loop (kqueue/epoll/IOCP)")
message(STATUS "  - ethernetip: EtherNet/IP protocol implementation")
message(STATUS "  - modbus: Modbus protocol implementation")
message(STATUS "")