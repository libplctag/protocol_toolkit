# Modbus Client CMakeLists.txt

# Create modbus client executable
add_executable(modbus_client
    modbus_client_main.c
    modbus_client.c
    ../protocol/modbus_data_store.c
    ../protocol/modbus_protocol.c
)

# Include directories
target_include_directories(modbus_client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
        ../protocol
)

# Link libraries
target_link_libraries(modbus_client
    protocol_toolkit
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(modbus_client ws2_32)
elseif(UNIX)
    target_link_libraries(modbus_client pthread)
    if(PLATFORM_LIBS)
        target_link_libraries(modbus_client ${PLATFORM_LIBS})
    endif()
endif()

# Set target properties
set_target_properties(modbus_client PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)