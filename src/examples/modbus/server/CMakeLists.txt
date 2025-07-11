# Modbus TCP Server

add_executable(modbus_server
    modbus_server.c
)

target_include_directories(modbus_server PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/include
)

target_link_libraries(modbus_server
    modbus
    protocol_toolkit
)
