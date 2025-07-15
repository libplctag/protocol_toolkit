# Protocol Toolkit Tests

# Test Harness - Main test runner
add_executable(test_harness 
    test_harness.c
    test_ptk_array_comprehensive.c
    test_ptk_atomic_comprehensive.c
    test_ptk_buf_comprehensive.c
    test_ptk_config_comprehensive.c
    test_ptk_err_comprehensive.c
    test_ptk_log_comprehensive.c
    test_ptk_mem.c
    test_ptk_os_thread_comprehensive.c
    test_ptk_sock_comprehensive.c
    test_ptk_utils_comprehensive.c
)
target_link_libraries(test_harness ptk::ptk)

# Basic tests (legacy)
add_executable(test_ptk_config test_ptk_config.c)
target_link_libraries(test_ptk_config ptk::ptk)

add_executable(test_ptk_os_thread test_ptk_os_thread.c)
target_link_libraries(test_ptk_os_thread ptk::ptk)

# Core library tests
add_executable(test_ptk_mem test_ptk_mem.c)
target_link_libraries(test_ptk_mem ptk::ptk)

add_executable(test_ptk_array test_ptk_array.c)
target_link_libraries(test_ptk_array ptk::ptk)

add_executable(test_ptk_atomic test_ptk_atomic.c)
target_link_libraries(test_ptk_atomic ptk::ptk)

add_executable(test_ptk_buf test_ptk_buf.c)
target_link_libraries(test_ptk_buf ptk::ptk)

add_executable(test_ptk_err test_ptk_err.c)
target_link_libraries(test_ptk_err ptk::ptk)

add_executable(test_ptk_log test_ptk_log.c)
target_link_libraries(test_ptk_log ptk::ptk)

add_executable(test_ptk_shared test_ptk_shared.c)
target_link_libraries(test_ptk_shared ptk::ptk)

add_executable(test_ptk_utils test_ptk_utils.c)
target_link_libraries(test_ptk_utils ptk::ptk)

add_executable(test_ptk_sock test_ptk_sock.c)
target_link_libraries(test_ptk_sock ptk::ptk)

add_executable(test_ptk_sock_udp test_ptk_sock_udp.c)
target_link_libraries(test_ptk_sock_udp ptk::ptk)

# Comprehensive tests (individual executables)
add_executable(test_ptk_array_comprehensive test_ptk_array_comprehensive.c)
target_link_libraries(test_ptk_array_comprehensive ptk::ptk)

add_executable(test_ptk_atomic_comprehensive test_ptk_atomic_comprehensive.c)
target_link_libraries(test_ptk_atomic_comprehensive ptk::ptk)

add_executable(test_ptk_buf_comprehensive test_ptk_buf_comprehensive.c)
target_link_libraries(test_ptk_buf_comprehensive ptk::ptk)

add_executable(test_ptk_config_comprehensive test_ptk_config_comprehensive.c)
target_link_libraries(test_ptk_config_comprehensive ptk::ptk)

add_executable(test_ptk_err_comprehensive test_ptk_err_comprehensive.c)
target_link_libraries(test_ptk_err_comprehensive ptk::ptk)

add_executable(test_ptk_log_comprehensive test_ptk_log_comprehensive.c)
target_link_libraries(test_ptk_log_comprehensive ptk::ptk)

add_executable(test_ptk_os_thread_comprehensive test_ptk_os_thread_comprehensive.c)
target_link_libraries(test_ptk_os_thread_comprehensive ptk::ptk)

add_executable(test_ptk_sock_comprehensive test_ptk_sock_comprehensive.c)
target_link_libraries(test_ptk_sock_comprehensive ptk::ptk)

add_executable(test_ptk_utils_comprehensive test_ptk_utils_comprehensive.c)
target_link_libraries(test_ptk_utils_comprehensive ptk::ptk)

# Type-safe serialization test (disabled - missing source file)
# add_executable(test_type_safe_serialize test_type_safe_serialize.c)
# target_link_libraries(test_type_safe_serialize ptk::ptk)

# Enable CTest and add all test executables
enable_testing()

# Add the main test harness as the primary test
add_test(NAME test_harness_all COMMAND test_harness)

# Add tests with Valgrind on Linux if available
if(UNIX AND NOT APPLE)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        # Primary comprehensive test with Valgrind
        add_test(NAME test_harness_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_harness)
        
        # Individual comprehensive tests with Valgrind
        add_test(NAME test_ptk_array_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_array_comprehensive)
        add_test(NAME test_ptk_atomic_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_atomic_comprehensive)
        add_test(NAME test_ptk_buf_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_buf_comprehensive)
        add_test(NAME test_ptk_config_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_config_comprehensive)
        add_test(NAME test_ptk_err_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_err_comprehensive)
        add_test(NAME test_ptk_log_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_log_comprehensive)
        add_test(NAME test_ptk_os_thread_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_os_thread_comprehensive)
        add_test(NAME test_ptk_sock_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_sock_comprehensive)
        add_test(NAME test_ptk_utils_comprehensive_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_utils_comprehensive)
        
        # Legacy tests with Valgrind
        add_test(NAME test_ptk_config_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_config)
        add_test(NAME test_ptk_os_thread_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_os_thread)
        add_test(NAME test_ptk_mem_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_mem)
        add_test(NAME test_ptk_array_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_array)
        add_test(NAME test_ptk_atomic_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_atomic)
        add_test(NAME test_ptk_buf_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_buf)
        add_test(NAME test_ptk_err_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_err)
        add_test(NAME test_ptk_log_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_log)
        add_test(NAME test_ptk_shared_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_shared)
        add_test(NAME test_ptk_utils_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_utils)
        add_test(NAME test_ptk_sock_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_sock)
        add_test(NAME test_ptk_sock_udp_valgrind COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_sock_udp)
    else()
        # Individual comprehensive tests without Valgrind
        add_test(NAME test_ptk_array_comprehensive COMMAND test_ptk_array_comprehensive)
        add_test(NAME test_ptk_atomic_comprehensive COMMAND test_ptk_atomic_comprehensive)
        add_test(NAME test_ptk_buf_comprehensive COMMAND test_ptk_buf_comprehensive)
        add_test(NAME test_ptk_config_comprehensive COMMAND test_ptk_config_comprehensive)
        add_test(NAME test_ptk_err_comprehensive COMMAND test_ptk_err_comprehensive)
        add_test(NAME test_ptk_log_comprehensive COMMAND test_ptk_log_comprehensive)
        add_test(NAME test_ptk_os_thread_comprehensive COMMAND test_ptk_os_thread_comprehensive)
        add_test(NAME test_ptk_sock_comprehensive COMMAND test_ptk_sock_comprehensive)
        add_test(NAME test_ptk_utils_comprehensive COMMAND test_ptk_utils_comprehensive)
        
        # Legacy tests without Valgrind
        add_test(NAME test_ptk_config COMMAND test_ptk_config)
        add_test(NAME test_ptk_os_thread COMMAND test_ptk_os_thread)
        add_test(NAME test_ptk_mem COMMAND test_ptk_mem)
        add_test(NAME test_ptk_array COMMAND test_ptk_array)
        add_test(NAME test_ptk_atomic COMMAND test_ptk_atomic)
        add_test(NAME test_ptk_buf COMMAND test_ptk_buf)
        add_test(NAME test_ptk_err COMMAND test_ptk_err)
        add_test(NAME test_ptk_log COMMAND test_ptk_log)
        add_test(NAME test_ptk_shared COMMAND test_ptk_shared)
        add_test(NAME test_ptk_utils COMMAND test_ptk_utils)
        add_test(NAME test_ptk_sock COMMAND test_ptk_sock)
        add_test(NAME test_ptk_sock_udp COMMAND test_ptk_sock_udp)
    endif()
else()
    # Individual comprehensive tests for non-Linux platforms
    add_test(NAME test_ptk_array_comprehensive COMMAND test_ptk_array_comprehensive)
    add_test(NAME test_ptk_atomic_comprehensive COMMAND test_ptk_atomic_comprehensive)
    add_test(NAME test_ptk_buf_comprehensive COMMAND test_ptk_buf_comprehensive)
    add_test(NAME test_ptk_config_comprehensive COMMAND test_ptk_config_comprehensive)
    add_test(NAME test_ptk_err_comprehensive COMMAND test_ptk_err_comprehensive)
    add_test(NAME test_ptk_log_comprehensive COMMAND test_ptk_log_comprehensive)
    add_test(NAME test_ptk_os_thread_comprehensive COMMAND test_ptk_os_thread_comprehensive)
    add_test(NAME test_ptk_sock_comprehensive COMMAND test_ptk_sock_comprehensive)
    add_test(NAME test_ptk_utils_comprehensive COMMAND test_ptk_utils_comprehensive)
    
    # Legacy tests for non-Linux platforms
    add_test(NAME test_ptk_config COMMAND test_ptk_config)
    add_test(NAME test_ptk_os_thread COMMAND test_ptk_os_thread)
    add_test(NAME test_ptk_mem COMMAND test_ptk_mem)
    add_test(NAME test_ptk_array COMMAND test_ptk_array)
    add_test(NAME test_ptk_atomic COMMAND test_ptk_atomic)
    add_test(NAME test_ptk_buf COMMAND test_ptk_buf)
    add_test(NAME test_ptk_err COMMAND test_ptk_err)
    add_test(NAME test_ptk_log COMMAND test_ptk_log)
    add_test(NAME test_ptk_shared COMMAND test_ptk_shared)
    add_test(NAME test_ptk_utils COMMAND test_ptk_utils)
    add_test(NAME test_ptk_sock COMMAND test_ptk_sock)
    add_test(NAME test_ptk_sock_udp COMMAND test_ptk_sock_udp)
endif()