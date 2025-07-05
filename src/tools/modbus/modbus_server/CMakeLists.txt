# Modbus Server CMakeLists.txt

# Create modbus server executable
add_executable(modbus_server
    modbus_server_main.c
    modbus_server.c
    ../protocol/modbus_data_store.c
    ../protocol/modbus_protocol.c
)

# Include directories
target_include_directories(modbus_server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
        ../protocol
)

# Link libraries
target_link_libraries(modbus_server
    protocol_toolkit
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(modbus_server ws2_32)
elseif(UNIX)
    target_link_libraries(modbus_server pthread)
    if(PLATFORM_LIBS)
        target_link_libraries(modbus_server ${PLATFORM_LIBS})
    endif()
endif()

# Set target properties
set_target_properties(modbus_server PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)