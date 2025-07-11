# Test for ptk_buf serialization/deserialization
add_executable(test_ptk_buf
    test_ptk_buf.c
)

target_link_libraries(test_ptk_buf
    protocol_toolkit
)

target_include_directories(test_ptk_buf PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}
)
# Socket tests for protocol toolkit - temporarily disabled (use old allocator)

# Test 1: TCP Echo Server/Client with Abort
# add_executable(test_tcp_echo_abort
#     test_tcp_echo_abort.c
# )
#
# target_link_libraries(test_tcp_echo_abort
#     protocol_toolkit
#     pthread
# )
#
# target_include_directories(test_tcp_echo_abort PRIVATE
#     ${CMAKE_SOURCE_DIR}/src/include
# )

# Test 2: UDP Echo Server/Client with Abort
# add_executable(test_udp_echo_abort
#     test_udp_echo_abort.c
# )
#
# target_link_libraries(test_udp_echo_abort
#     protocol_toolkit
#     pthread
# )
#
# target_include_directories(test_udp_echo_abort PRIVATE
#     ${CMAKE_SOURCE_DIR}/src/include
# )

# Test 3: Type-Safe Serialization Test
add_executable(test_type_safe_serialize
    test_type_safe_serialize.c
)

target_link_libraries(test_type_safe_serialize
    protocol_toolkit
)

target_include_directories(test_type_safe_serialize PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}
)

# Demo 4: Type-Safe Serialization Demo - temporarily disabled (uses old allocator)
# add_executable(demo_type_safe_serialize
#     demo_type_safe_serialize.c
# )
#
# target_link_libraries(demo_type_safe_serialize
#     protocol_toolkit
# )
#
# target_include_directories(demo_type_safe_serialize PRIVATE
#     ${CMAKE_SOURCE_DIR}/src/include
#     ${CMAKE_SOURCE_DIR}
# )

# Test 5: Parent-Child Allocation Test
add_executable(test_parent_child_alloc
    test_parent_child_alloc.c
)

target_link_libraries(test_parent_child_alloc
    protocol_toolkit
)

target_include_directories(test_parent_child_alloc PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}
)