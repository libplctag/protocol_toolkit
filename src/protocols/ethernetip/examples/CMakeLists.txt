# CMakeLists.txt for EtherNet/IP examples
cmake_minimum_required(VERSION 3.10)

# Include directories
include_directories(../../../protocol_toolkit/utils)
include_directories(../../../protocol_toolkit/ev_loop)

# EtherNet/IP Server
add_executable(ethernetip_server
    ethernetip_server/ethernetip_server.c
)

target_link_libraries(ethernetip_server
    ethernetip
    ev_loop
    buf
    log
)

# Platform-specific libraries for server
if(WIN32)
    target_link_libraries(ethernetip_server ws2_32)
elseif(UNIX)
    target_link_libraries(ethernetip_server pthread)
    
    # Find required libraries for Linux
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_library(RT_LIBRARY rt)
        if(RT_LIBRARY)
            target_link_libraries(ethernetip_server ${RT_LIBRARY})
        endif()
    endif()
endif()

# EtherNet/IP Client
add_executable(ethernetip_client
    ethernetip_client/ethernetip_client.c
)

target_link_libraries(ethernetip_client
    ethernetip
    ev_loop
    buf
    log
)

# Platform-specific libraries for client
if(WIN32)
    target_link_libraries(ethernetip_client ws2_32)
elseif(UNIX)
    target_link_libraries(ethernetip_client pthread)
    
    # Find required libraries for Linux
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        find_library(RT_LIBRARY rt)
        if(RT_LIBRARY)
            target_link_libraries(ethernetip_client ${RT_LIBRARY})
        endif()
    endif()
endif()

# Set output directory
set_target_properties(ethernetip_server ethernetip_client
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "EtherNet/IP examples configured:")
message(STATUS "  - ethernetip_server")
message(STATUS "  - ethernetip_client")