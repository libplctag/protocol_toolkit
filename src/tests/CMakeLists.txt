# Socket tests for protocol toolkit

# Test 1: TCP Echo Server/Client with Abort
add_executable(test_tcp_echo_abort
    test_tcp_echo_abort.c
)

target_link_libraries(test_tcp_echo_abort
    protocol_toolkit
    pthread
)

target_include_directories(test_tcp_echo_abort PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
)

# Test 2: UDP Echo Server/Client with Abort  
add_executable(test_udp_echo_abort
    test_udp_echo_abort.c
)

target_link_libraries(test_udp_echo_abort
    protocol_toolkit
    pthread
)

target_include_directories(test_udp_echo_abort PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
)