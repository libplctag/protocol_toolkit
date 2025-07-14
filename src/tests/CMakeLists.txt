# Protocol Toolkit Tests

# Basic tests
add_executable(test_ptk_config test_ptk_config.c)
target_link_libraries(test_ptk_config ptk::ptk)

# add_executable(test_ptk_threadlet test_ptk_threadlet.c)
# target_link_libraries(test_ptk_threadlet ptk::ptk)

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

# Type-safe serialization test (disabled - missing source file)
# add_executable(test_type_safe_serialize test_type_safe_serialize.c)
# target_link_libraries(test_type_safe_serialize ptk::ptk)

# Enable CTest and add all test executables
enable_testing()

# Add tests with Valgrind on Linux if available
if(UNIX AND NOT APPLE)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        add_test(NAME test_ptk_config COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_config)
        # add_test(NAME test_ptk_threadlet COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_threadlet)
        add_test(NAME test_ptk_os_thread COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_os_thread)
        add_test(NAME test_ptk_mem COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_mem)
        add_test(NAME test_ptk_array COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_array)
        add_test(NAME test_ptk_atomic COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_atomic)
        add_test(NAME test_ptk_buf COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_buf)
        add_test(NAME test_ptk_err COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_err)
        add_test(NAME test_ptk_log COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_log)
        add_test(NAME test_ptk_shared COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_shared)
        add_test(NAME test_ptk_utils COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_utils)
        add_test(NAME test_ptk_sock COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_sock)
        add_test(NAME test_ptk_sock_udp COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_ptk_sock_udp)
        # add_test(NAME test_type_safe_serialize COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 test_type_safe_serialize)
    else()
        add_test(NAME test_ptk_config COMMAND test_ptk_config)
        # add_test(NAME test_ptk_threadlet COMMAND test_ptk_threadlet)
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
        # add_test(NAME test_type_safe_serialize COMMAND test_type_safe_serialize)
    endif()
else()
    add_test(NAME test_ptk_config COMMAND test_ptk_config)
    # add_test(NAME test_ptk_threadlet COMMAND test_ptk_threadlet)
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
    # add_test(NAME test_type_safe_serialize COMMAND test_type_safe_serialize)
endif()