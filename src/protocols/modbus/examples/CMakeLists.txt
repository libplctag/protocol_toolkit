# CMakeLists.txt for Modbus examples
cmake_minimum_required(VERSION 3.10)

# Include directories
include_directories(../../../protocol_toolkit/utils)
include_directories(../../../protocol_toolkit/ev_loop)

# Modbus Server
add_executable(modbus_server
    modbus_server/modbus_server.c
)

target_link_libraries(modbus_server
    modbus
    ev_loop
    buf
    log
)

# Platform-specific libraries for server
if(WIN32)
    target_link_libraries(modbus_server ws2_32)
elseif(UNIX)
    target_link_libraries(modbus_server pthread)
    
    # Find required libraries for Linux
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_library(RT_LIBRARY rt)
        if(RT_LIBRARY)
            target_link_libraries(modbus_server ${RT_LIBRARY})
        endif()
    endif()
endif()

# Modbus Client
add_executable(modbus_client
    modbus_client/modbus_client.c
)

target_link_libraries(modbus_client
    modbus
    ev_loop
    buf
    log
)

# Platform-specific libraries for client
if(WIN32)
    target_link_libraries(modbus_client ws2_32)
elseif(UNIX)
    target_link_libraries(modbus_client pthread)
    
    # Find required libraries for Linux
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_library(RT_LIBRARY rt)
        if(RT_LIBRARY)
            target_link_libraries(modbus_client ${RT_LIBRARY})
        endif()
    endif()
endif()

# Set output directory
set_target_properties(modbus_server modbus_client
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "Modbus examples configured:")
message(STATUS "  - modbus_server")
message(STATUS "  - modbus_client")