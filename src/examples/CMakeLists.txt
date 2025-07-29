# Examples for Protocol Toolkit

# TCP Client Example
add_executable(example_tcp_client example_tcp_client.c)
target_link_libraries(example_tcp_client ProtocolToolkit::macos)

# Set runtime output directory for examples
set_target_properties(example_tcp_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples
)

# Install examples
install(TARGETS example_tcp_client
    RUNTIME DESTINATION bin/examples
)

# Optional: Create a custom target to run examples
add_custom_target(run_examples
    COMMENT "Running Protocol Toolkit examples"
)

add_custom_command(TARGET run_examples POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Example binaries are located in: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples/"
    COMMAND ${CMAKE_COMMAND} -E echo "Run with: ./build/bin/examples/example_tcp_client"
)
