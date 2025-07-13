# Protocol Toolkit Library CMakeLists.txt
# Build the core protocol toolkit library

# Core sources (platform-independent)
set(PTK_CORE_SOURCES
    ptk_alloc.c
    ptk_buf.c
    ptk_err.c
    ptk_log.c
    ptk_utils.c
    ptk_shared.c
    ptk_config.c
)

# Platform-specific sources
set(PTK_PLATFORM_SOURCES "")

# Detect platform and add appropriate sources
if(WIN32)
    list(APPEND PTK_PLATFORM_SOURCES
        windows/atomic_operations.c
        windows/os_thread_win.c
    )
    message(STATUS "Building PTK for Windows")
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND PTK_PLATFORM_SOURCES
        posix/atomic_operations.c
        posix/os_thread_posix.c
        posix/network_list.c
        posix/socket.c
        linux/address.c
        linux/linux_event_loop.c
        linux/linux_integration.c
        linux/socket_integration.c
        linux/threadlet_api.c
        linux/threadlet_core.c
        linux/threadlet_scheduler.c
    )
    message(STATUS "Building PTK for Linux")
    
elseif(CMAKE_SYSTEM_NAME MATCHES "BSD|FreeBSD|OpenBSD|NetBSD")
    list(APPEND PTK_PLATFORM_SOURCES
        posix/atomic_operations.c
        posix/os_thread_posix.c
        posix/network_list.c
        posix/socket.c
        # BSD-specific files would go in bsd/ directory
    )
    message(STATUS "Building PTK for BSD variant: ${CMAKE_SYSTEM_NAME}")
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND PTK_PLATFORM_SOURCES
        posix/atomic_operations.c
        posix/os_thread_posix.c
        posix/network_list.c
        posix/socket.c
        # macOS-specific files would go here
    )
    message(STATUS "Building PTK for macOS")
    
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Verify all platform sources exist
foreach(source_file ${PTK_PLATFORM_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        message(WARNING "Platform source file does not exist: ${source_file}")
        list(REMOVE_ITEM PTK_PLATFORM_SOURCES ${source_file})
    endif()
endforeach()

# Create protocol toolkit library with all sources
add_library(ptk ${PTK_CORE_SOURCES} ${PTK_PLATFORM_SOURCES})

# Create alias for namespaced usage
add_library(ptk::ptk ALIAS ptk)

# Set target properties
target_include_directories(ptk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

# Link platform-specific libraries
if(WIN32)
    target_link_libraries(ptk PUBLIC ws2_32)
elseif(UNIX)
    target_link_libraries(ptk PUBLIC pthread)
    if(PLATFORM_LIBS)
        target_link_libraries(ptk PUBLIC ${PLATFORM_LIBS})
    endif()
endif()

# Set library properties
set_target_properties(ptk PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    OUTPUT_NAME ptk
)

# Add tests subdirectory (only if this is part of a main project build)
if(PTK_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test)
    add_subdirectory(test)
endif()