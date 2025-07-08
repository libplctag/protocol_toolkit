# Modbus TCP Server - Multi-threaded

add_executable(modbus_server_threaded
    modbus_server_threaded.c
)

target_include_directories(modbus_server_threaded PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/include
)

target_link_libraries(modbus_server_threaded
    modbus
    protocol_toolkit
)

# Alias for backward compatibility
add_executable(modbus_server
    modbus_server_threaded.c
)

target_include_directories(modbus_server PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/include
)

target_link_libraries(modbus_server
    modbus
    protocol_toolkit
)
