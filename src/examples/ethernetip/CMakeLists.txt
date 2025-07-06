# EtherNet/IP Example Tools

# Add tag server if it has a CMakeLists.txt
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tag_server/CMakeLists.txt)
    add_subdirectory(tag_server)
endif()

# ethernetip_find_devices executable
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ethernetip_find_devices/ethernetip_find_devices.c)
    add_executable(ethernetip_find_devices
        ethernetip_find_devices/ethernetip_find_devices.c
    )

    target_link_libraries(ethernetip_find_devices
        protocol_toolkit
    )

    target_include_directories(ethernetip_find_devices PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tag_server/protocol
    )
endif()