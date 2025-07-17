# Protocol Toolkit Tests
cmake_minimum_required(VERSION 3.10)
project(ProtocolToolkitTests C)

# Common settings for all test executables
function(add_ptk_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE ptk::ptk)
    target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/include")
endfunction()

# Individual test executables
add_ptk_test(test_ptk_array test_ptk_array_comprehensive.c)
add_ptk_test(test_ptk_atomic test_ptk_atomic_comprehensive.c)
add_ptk_test(test_ptk_buf test_ptk_buf_comprehensive.c)
add_ptk_test(test_ptk_config test_ptk_config_comprehensive.c)
add_ptk_test(test_ptk_err test_ptk_err_comprehensive.c)
add_ptk_test(test_ptk_log test_ptk_log_comprehensive.c)
add_ptk_test(test_ptk_mem test_ptk_mem.c)
add_ptk_test(test_ptk_os_thread test_ptk_os_thread_comprehensive.c)
add_ptk_test(test_ptk_sock test_ptk_sock_comprehensive.c)
add_ptk_test(test_ptk_utils test_ptk_utils_comprehensive.c)

# Add individual tests to CTest
add_test(NAME ptk_array COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_array)
add_test(NAME ptk_atomic COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_atomic)
add_test(NAME ptk_buf COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_buf)
add_test(NAME ptk_config COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_config)
add_test(NAME ptk_err_t COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_err)
add_test(NAME ptk_log COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_log)
add_test(NAME ptk_mem COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_mem)
add_test(NAME ptk_os_thread COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_os_thread)
add_test(NAME ptk_sock COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_sock)
add_test(NAME ptk_utils COMMAND ${CMAKE_BINARY_DIR}/bin/test_ptk_utils)

# Add test with Valgrind on Linux if available
if(UNIX AND NOT APPLE)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        # Add individual valgrind tests
        add_test(NAME ptk_array_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_array)
        add_test(NAME ptk_atomic_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_atomic)
        add_test(NAME ptk_buf_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_buf)
        add_test(NAME ptk_config_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_config)
        add_test(NAME ptk_err_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_err)
        add_test(NAME ptk_log_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_log)
        add_test(NAME ptk_mem_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_mem)
        add_test(NAME ptk_os_thread_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_os_thread)
        add_test(NAME ptk_sock_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_sock)
        add_test(NAME ptk_utils_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 ${CMAKE_BINARY_DIR}/bin/test_ptk_utils)
    endif()
endif()