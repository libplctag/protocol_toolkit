cmake_minimum_required(VERSION 3.18)
project(protocol_toolkit)

# Set C standard
set(CMAKE_C_STANDARD 11)
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
# PROTOCOL TOOLKIT LIBRARY
#=============================================================================

# Add protocol toolkit library
add_subdirectory(src/lib)

#=============================================================================
# EXAMPLES
#=============================================================================

# Add examples
# add_subdirectory(src/examples)
# add_subdirectory(tools/gen_codec/examples)

# Add tests
add_subdirectory(src/tests)

#=============================================================================
# INSTALLATION
#=============================================================================

# Install any targets that exist
# Note: modbus_server and modbus_client removed
# if(TARGET ethernetip_find_devices)
#     install(TARGETS ethernetip_find_devices
#         RUNTIME DESTINATION bin
#     )
# endif()

# if(TARGET ethernetip_tag_server)
#     install(TARGETS ethernetip_tag_server
#         RUNTIME DESTINATION bin
#     )
# endif()

install(FILES
    src/include/ptk_buf.h
    src/include/ptk_err.h
    src/include/ptk_log.h
    src/include/ptk_socket.h
    src/include/ptk_thread.h
    src/include/ptk_utils.h
    DESTINATION include
)

#=============================================================================
# TESTING
#=============================================================================

# Add test targets
# add_custom_target(test_sockets
#     COMMENT "Testing socket implementation"
#     DEPENDS test_tcp_echo_abort test_udp_echo_abort
# )

#=============================================================================
# INFORMATION
#=============================================================================

# Print build information
message(STATUS "Building Protocol Toolkit")
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
message(STATUS "Libraries built:")
message(STATUS "  - protocol_toolkit: Core utilities (buf, err, log, socket, thread, utils)")
message(STATUS "")
message(STATUS "Examples:")
message(STATUS "  - ethernetip: EtherNet/IP device discovery tool")
message(STATUS "  - modbus: Implementation guide and documentation")
message(STATUS "")
message(STATUS "Tests available:")
message(STATUS "  - test_tcp_echo_abort: TCP socket abort functionality")
message(STATUS "  - test_udp_echo_abort: UDP socket abort functionality")
message(STATUS "")