# Protocol Toolkit Library Tests CMakeLists.txt
# Build tests for the core protocol toolkit library

# Test for allocator functionality - temporarily disabled (uses old allocator)
# add_executable(test_ptk_alloc
#     test_ptk_alloc.c
# )
#
# target_include_directories(test_ptk_alloc
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/src/include
# )
#
# target_link_libraries(test_ptk_alloc
#     protocol_toolkit
# )

# Test for buffer functionality - temporarily disabled (uses old allocator)
# add_executable(test_ptk_buf
#     test_ptk_buf.c
# )
#
# target_include_directories(test_ptk_buf
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/src/include
# )
#
# target_link_libraries(test_ptk_buf
#     protocol_toolkit
# )

# Add tests to CTest
enable_testing()

add_test(NAME ptk_alloc_test COMMAND test_ptk_alloc)
add_test(NAME ptk_buf_test COMMAND test_ptk_buf)

# Set test properties
set_tests_properties(ptk_alloc_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(ptk_buf_test PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Optional: Add a target to run all library tests
add_custom_target(run_lib_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_ptk_alloc test_ptk_buf
    COMMENT "Running protocol toolkit library tests"
)
