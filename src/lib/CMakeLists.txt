# Protocol Toolkit Library CMakeLists.txt
# Build the core protocol toolkit library

# Create protocol toolkit library
add_library(protocol_toolkit
    ptk_atomic.c
    ptk_buf.c
    ptk_err.c
    ptk_log.c
    ptk_socket.c
    ptk_thread.c
    ptk_utils.c
)

# Set target properties
target_include_directories(protocol_toolkit
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/include
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(protocol_toolkit ws2_32)
elseif(UNIX)
    target_link_libraries(protocol_toolkit pthread)
    if(PLATFORM_LIBS)
        target_link_libraries(protocol_toolkit ${PLATFORM_LIBS})
    endif()
endif()

# Set library properties
set_target_properties(protocol_toolkit PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)