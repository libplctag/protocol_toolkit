# Modbus TCP Implementation Test

add_library(modbus STATIC
    lib/src/modbus_common.c
    lib/src/modbus_holding_registers.c
    lib/src/modbus_input_registers.c
    lib/src/modbus_discrete_inputs.c
    lib/src/modbus_coils.c
    lib/src/modbus_exceptions.c
    lib/src/modbus_connection.c
)

target_include_directories(modbus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/src
    ${PROJECT_SOURCE_DIR}/src/include
)

target_link_libraries(modbus
    protocol_toolkit
)

# Add subdirectories
# add_subdirectory(client)
# add_subdirectory(server)