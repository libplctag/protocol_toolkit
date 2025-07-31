# Tests for Protocol Toolkit

# Test executables
add_executable(test_basic_functionality test_basic_functionality.c)
target_include_directories(test_basic_functionality PRIVATE ${PTK_COMMON_INCLUDE_DIR} ${PTK_PLATFORM_INCLUDE_DIR})
target_link_libraries(test_basic_functionality ProtocolToolkit::platform)

add_executable(test_state_machine test_state_machine.c)
target_include_directories(test_state_machine PRIVATE ${PTK_COMMON_INCLUDE_DIR} ${PTK_PLATFORM_INCLUDE_DIR})
target_link_libraries(test_state_machine ProtocolToolkit::platform)

# Set runtime output directory for tests
set_target_properties(test_basic_functionality test_state_machine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests
)

# Add tests to CTest
add_test(NAME BasicFunctionality
    COMMAND test_basic_functionality
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests
)

add_test(NAME StateMachine
    COMMAND test_state_machine
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests
)

# Set test properties
set_tests_properties(BasicFunctionality PROPERTIES
    TIMEOUT 10
    PASS_REGULAR_EXPRESSION "All tests passed"
    LABELS "unit"
)

set_tests_properties(StateMachine PROPERTIES
    TIMEOUT 10
    PASS_REGULAR_EXPRESSION "All tests passed"
    LABELS "unit"
)

# Create a custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS test_basic_functionality test_state_machine
    COMMENT "Running all Protocol Toolkit tests"
)

# Install test executables (optional, for debugging)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    install(TARGETS test_basic_functionality test_state_machine
        RUNTIME DESTINATION bin/tests
    )
endif()
