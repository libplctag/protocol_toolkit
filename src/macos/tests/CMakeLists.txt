cmake_minimum_required(VERSION 3.16)

# Simple test for basic functionality
add_executable(test_basic_functionality test_basic_functionality.c)
target_link_libraries(test_basic_functionality protocol_toolkit_macos)

# Test for state machine functionality
add_executable(test_state_machine test_state_machine.c)
target_link_libraries(test_state_machine protocol_toolkit_macos)

# Add tests to CTest
add_test(NAME BasicFunctionality COMMAND test_basic_functionality)
add_test(NAME StateMachine COMMAND test_state_machine)

# Set test properties
set_tests_properties(BasicFunctionality PROPERTIES
    TIMEOUT 10
    PASS_REGULAR_EXPRESSION "All tests passed"
)

set_tests_properties(StateMachine PROPERTIES
    TIMEOUT 10
    PASS_REGULAR_EXPRESSION "All tests passed"
)
