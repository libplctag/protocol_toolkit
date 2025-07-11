# EtherNet/IP Device Discovery Example CMakeLists.txt

# Add executable
add_executable(ethernetip_find_devices
    ethernetip_find_devices.c
)

# Link with protocol toolkit library
target_link_libraries(ethernetip_find_devices
    protocol_toolkit
)

# Include directories
target_include_directories(ethernetip_find_devices PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
)

# Set output directory
set_target_properties(ethernetip_find_devices PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
