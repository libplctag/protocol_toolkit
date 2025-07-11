cmake_minimum_required(VERSION 3.15)

# Create modbus server executable
add_executable(modbus_server
    modbus_server.c
)

# Include directories
target_include_directories(modbus_server PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../src/include  # Protocol Toolkit headers
    ${CMAKE_SOURCE_DIR}/..  # For modbus.h
)

# Link libraries
target_link_libraries(modbus_server PRIVATE
    protocol_toolkit
)

# Compiler settings
target_compile_features(modbus_server PRIVATE c_std_11)
target_compile_options(modbus_server PRIVATE
    -Wall
    -Wextra
    -Werror
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# Install target
install(TARGETS modbus_server
    RUNTIME DESTINATION bin
)