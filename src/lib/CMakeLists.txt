# PTK - Protocol Toolkit Library Build Configuration
cmake_minimum_required(VERSION 3.10)

# Core platform-independent sources
set(PTK_CORE_SOURCES
    ptk.c
    ptk_scratch.c
    ptk_serialization.c
    ptk_connection.c
    ptk_event.c
)

# Platform detection and source selection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux - Use epoll for optimal performance
    set(PTK_PLATFORM_SOURCES
        posix/ptk_socket_posix.c
        posix/ptk_time_posix.c
        linux/ptk_epoll.c
    )
    set(PTK_PLATFORM "Linux")
    set(PTK_PLATFORM_LIBS pthread)
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS - Use kqueue (placeholder for future implementation)
    set(PTK_PLATFORM_SOURCES
        posix/ptk_socket_posix.c
        posix/ptk_time_posix.c
        # TODO: Add macos/ptk_kqueue.c when implemented
    )
    set(PTK_PLATFORM "macOS")
    set(PTK_PLATFORM_LIBS pthread)
    
elseif(WIN32)
    # Windows - Use IOCP (placeholder for future implementation)
    set(PTK_PLATFORM_SOURCES
        # TODO: Add windows/ptk_socket_win.c when implemented
        # TODO: Add windows/ptk_time_win.c when implemented
        # TODO: Add windows/ptk_iocp.c when implemented
    )
    set(PTK_PLATFORM "Windows")
    set(PTK_PLATFORM_LIBS ws2_32 winmm)
    
elseif(UNIX)
    # Generic POSIX fallback
    set(PTK_PLATFORM_SOURCES
        posix/ptk_socket_posix.c
        posix/ptk_time_posix.c
        # TODO: Add posix/ptk_select.c for platforms without epoll/kqueue
    )
    set(PTK_PLATFORM "POSIX")
    set(PTK_PLATFORM_LIBS pthread)
    
else()
    message(WARNING "Unknown platform, using POSIX fallback")
    set(PTK_PLATFORM_SOURCES
        posix/ptk_socket_posix.c
        posix/ptk_time_posix.c
    )
    set(PTK_PLATFORM "Unknown")
    set(PTK_PLATFORM_LIBS pthread)
endif()

# Create the PTK library
add_library(ptk STATIC ${PTK_CORE_SOURCES} ${PTK_PLATFORM_SOURCES})

# Set target properties
set_target_properties(ptk PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Include directories
target_include_directories(ptk
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific compile definitions
target_compile_definitions(ptk PRIVATE
    PTK_PLATFORM_${PTK_PLATFORM}=1
)

# Linux-specific definitions
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(ptk PRIVATE
        PTK_HAS_EPOLL=1
        _GNU_SOURCE=1  # For epoll extensions
    )
endif()

# Platform-specific link libraries
if(PTK_PLATFORM_LIBS)
    target_link_libraries(ptk ${PTK_PLATFORM_LIBS})
endif()

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ptk PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
    
    # Release optimizations
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(ptk PRIVATE
            -O2
            -DNDEBUG
        )
    endif()
    
    # Debug options
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(ptk PRIVATE
            -g
            -O0
            -DDEBUG
        )
    endif()
endif()

# Optional: Enable additional warnings for development
option(PTK_ENABLE_EXTRA_WARNINGS "Enable extra compiler warnings" OFF)
if(PTK_ENABLE_EXTRA_WARNINGS AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang"))
    target_compile_options(ptk PRIVATE
        -Wpedantic
        -Wcast-align
        -Wcast-qual
        -Wconversion
        -Wformat=2
        -Wmissing-prototypes
        -Wredundant-decls
        -Wshadow
        -Wstrict-prototypes
        -Wwrite-strings
    )
endif()

# Optional: Enable Link Time Optimization for size-critical builds
option(PTK_ENABLE_LTO "Enable Link Time Optimization" OFF)
if(PTK_ENABLE_LTO)
    set_property(TARGET ptk PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(ptk PRIVATE -flto)
        target_link_options(ptk PRIVATE -flto)
    endif()
endif()

# Optional: Enable dead code elimination
option(PTK_ENABLE_DCE "Enable dead code elimination" OFF)
if(PTK_ENABLE_DCE AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang"))
    target_compile_options(ptk PRIVATE -ffunction-sections -fdata-sections)
    if(NOT WIN32)
        target_link_options(ptk PRIVATE -Wl,--gc-sections)
    endif()
endif()

# Display configuration information
message(STATUS "PTK Library Configuration:")
message(STATUS "  Platform: ${PTK_PLATFORM}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "  Core Sources: ${PTK_CORE_SOURCES}")
message(STATUS "  Platform Sources: ${PTK_PLATFORM_SOURCES}")
if(PTK_PLATFORM_LIBS)
    message(STATUS "  Platform Libraries: ${PTK_PLATFORM_LIBS}")
endif()

# Installation support (optional)
if(PTK_INSTALL)
    install(TARGETS ptk
        EXPORT ptkTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )
    
    install(EXPORT ptkTargets
        FILE ptkTargets.cmake
        NAMESPACE ptk::
        DESTINATION lib/cmake/ptk
    )
endif()