
add_executable(test_ptk_config test_ptk_config.c)
target_link_libraries(test_ptk_config protocol_toolkit)
target_include_directories(test_ptk_config PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})

add_executable(test_ptk_threadlet test_ptk_threadlet.c)
add_executable(test_ptk_os_thread test_ptk_os_thread.c)

target_link_libraries(test_ptk_threadlet protocol_toolkit)
target_link_libraries(test_ptk_os_thread protocol_toolkit)

target_include_directories(test_ptk_threadlet PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_os_thread PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_link_libraries(test_ptk_buf
target_include_directories(test_ptk_buf PRIVATE
 

add_executable(test_ptk_alloc test_ptk_alloc.c)
add_executable(test_ptk_array test_ptk_array.c)
add_executable(test_ptk_atomic test_ptk_atomic.c)
add_executable(test_ptk_buf test_ptk_buf.c)
add_executable(test_ptk_err test_ptk_err.c)
add_executable(test_ptk_log test_ptk_log.c)
add_executable(test_ptk_shared test_ptk_shared.c)
add_executable(test_ptk_utils test_ptk_utils.c)

target_link_libraries(test_ptk_alloc protocol_toolkit)
target_link_libraries(test_ptk_array protocol_toolkit)
target_link_libraries(test_ptk_atomic protocol_toolkit)
target_link_libraries(test_ptk_buf protocol_toolkit)
target_link_libraries(test_ptk_err protocol_toolkit)
target_link_libraries(test_ptk_log protocol_toolkit)
target_link_libraries(test_ptk_shared protocol_toolkit)
target_link_libraries(test_ptk_utils protocol_toolkit)

target_include_directories(test_ptk_alloc PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_array PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_atomic PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_buf PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_err PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_log PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_shared PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_ptk_utils PRIVATE ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR})
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
