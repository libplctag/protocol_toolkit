# Modbus TCP Client

add_executable(modbus_client
    modbus_client.c
)

target_include_directories(modbus_client PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/include
)

target_link_libraries(modbus_client
    modbus
    protocol_toolkit
)
