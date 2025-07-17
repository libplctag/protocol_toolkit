# Protocol Toolkit Library CMakeLists.txt
# Build the core protocol toolkit library

# Core sources (platform-independent)
set(PTK_CORE_SOURCES
    ptk.c
    posix/mem.c
    ptk_buf.c
    ptk_err.c
    ptk_log.c
    ptk_utils.c
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
        posix/thread.c
        posix/network_list.c
        linux/address.c
        linux/sock.c
    )
    message(STATUS "Building PTK for Linux with platform-specific sources: ${PTK_PLATFORM_SOURCES}")

elseif(CMAKE_SYSTEM_NAME MATCHES "BSD|FreeBSD|OpenBSD|NetBSD")
    list(APPEND PTK_PLATFORM_SOURCES
        posix/atomic_operations.c
        posix/thread.c
        posix/network_list.c
        # BSD-specific files would go in bsd/ directory
    )
    message(STATUS "Building PTK for BSD variant: ${CMAKE_SYSTEM_NAME}")
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND PTK_PLATFORM_SOURCES
        posix/atomic_operations.c
        posix/thread.c
        posix/network_list.c
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
# Set default symbol visibility to hidden for GCC/Clang
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set_target_properties(ptk PROPERTIES
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()

# Set default symbol visibility to hidden for MSVC (Windows)
if(MSVC)
    set_target_properties(ptk PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )
endif()
endif()