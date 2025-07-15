cmake_minimum_required(VERSION 3.18)
project(protocol_toolkit)

# Determine if this is the main project or a subproject
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PTK_IS_MAIN_PROJECT TRUE)
else()
    set(PTK_IS_MAIN_PROJECT FALSE)
endif()

# Options
option(PTK_BUILD_EXAMPLES "Build Protocol Toolkit examples" ${PTK_IS_MAIN_PROJECT})
option(PTK_BUILD_TESTS "Build Protocol Toolkit tests" ${PTK_IS_MAIN_PROJECT})
option(PTK_INSTALL "Install Protocol Toolkit files" ${PTK_IS_MAIN_PROJECT})
option(PTK_STATIC_BUILD "Build static libraries only" OFF)
option(PTK_ENABLE_STATIC_ANALYSIS "Enable static analysis tools" OFF)
option(PTK_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(PTK_ENABLE_MSAN "Enable MemorySanitizer" OFF)
option(PTK_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(PTK_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    # GCC and Clang options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    # MSVC options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3")
    set(CMAKE_C_FLAGS_DEBUG "/Zi /Od /DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

#=============================================================================
# SANITIZERS AND STATIC ANALYSIS
#=============================================================================

# Sanitizers are disabled for static builds as they typically don't work well together
if(NOT PTK_STATIC_BUILD)
    # Sanitizer configuration
    set(SANITIZER_FLAGS "")
    set(SANITIZER_LINK_FLAGS "")
    
    # AddressSanitizer
    if(PTK_ENABLE_ASAN)
        if(PTK_ENABLE_MSAN OR PTK_ENABLE_TSAN)
            message(FATAL_ERROR "AddressSanitizer cannot be used with MemorySanitizer or ThreadSanitizer")
        endif()
        set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=address")
        message(STATUS "AddressSanitizer enabled")
    endif()
    
    # MemorySanitizer
    if(PTK_ENABLE_MSAN)
        if(PTK_ENABLE_ASAN OR PTK_ENABLE_TSAN)
            message(FATAL_ERROR "MemorySanitizer cannot be used with AddressSanitizer or ThreadSanitizer")
        endif()
        if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
            message(FATAL_ERROR "MemorySanitizer requires Clang")
        endif()
        set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=memory -fno-omit-frame-pointer")
        set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=memory")
        message(STATUS "MemorySanitizer enabled")
    endif()
    
    # UndefinedBehaviorSanitizer
    if(PTK_ENABLE_UBSAN)
        set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=undefined")
        set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=undefined")
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    endif()
    
    # ThreadSanitizer
    if(PTK_ENABLE_TSAN)
        if(PTK_ENABLE_ASAN OR PTK_ENABLE_MSAN)
            message(FATAL_ERROR "ThreadSanitizer cannot be used with AddressSanitizer or MemorySanitizer")
        endif()
        set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=thread")
        set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=thread")
        message(STATUS "ThreadSanitizer enabled")
    endif()
    
    # Apply sanitizer flags
    if(SANITIZER_FLAGS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS}")
    endif()
else()
    if(PTK_ENABLE_ASAN OR PTK_ENABLE_MSAN OR PTK_ENABLE_UBSAN OR PTK_ENABLE_TSAN)
        message(WARNING "Sanitizers are disabled for static builds")
    endif()
endif()

# Static Analysis Tools
if(PTK_ENABLE_STATIC_ANALYSIS)
    # Find cppcheck
    find_program(CPPCHECK_EXECUTABLE cppcheck)
    if(CPPCHECK_EXECUTABLE)
        message(STATUS "Found cppcheck: ${CPPCHECK_EXECUTABLE}")
        
        # Create cppcheck target
        add_custom_target(cppcheck
            COMMAND ${CPPCHECK_EXECUTABLE}
                --enable=all
                --std=c11
                --verbose
                --quiet
                --xml
                --xml-version=2
                --suppress=missingIncludeSystem
                --suppress=unusedFunction
                --suppress=unmatchedSuppression
                src/
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running cppcheck static analysis"
        )
    else()
        message(WARNING "cppcheck not found. Static analysis will be limited.")
    endif()
    
    # Find scan-build (Clang Static Analyzer)
    find_program(SCAN_BUILD_EXECUTABLE scan-build)
    if(SCAN_BUILD_EXECUTABLE)
        message(STATUS "Found scan-build: ${SCAN_BUILD_EXECUTABLE}")
        
        # Create scan-build target
        add_custom_target(scan-build
            COMMAND ${SCAN_BUILD_EXECUTABLE}
                --status-bugs
                --use-cc=${CMAKE_C_COMPILER}
                -o ${CMAKE_BINARY_DIR}/scan-build-reports
                make -C ${CMAKE_BINARY_DIR}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running Clang Static Analyzer"
        )
    else()
        message(WARNING "scan-build not found. Install clang-tools for static analysis.")
    endif()
    
    # Combined static analysis target
    if(CPPCHECK_EXECUTABLE OR SCAN_BUILD_EXECUTABLE)
        add_custom_target(static-analysis)
        if(CPPCHECK_EXECUTABLE)
            add_dependencies(static-analysis cppcheck)
        endif()
        if(SCAN_BUILD_EXECUTABLE)
            add_dependencies(static-analysis scan-build)
        endif()
    endif()
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 and later
    if(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif()
elseif(APPLE)
    # macOS-specific settings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
elseif(UNIX)
    # Linux/BSD-specific settings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

    # Find required libraries
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
        set(PLATFORM_LIBS ${PLATFORM_LIBS} ${RT_LIBRARY})
    endif()
endif()

#=============================================================================
# PROTOCOL TOOLKIT LIBRARY
#=============================================================================

# Add protocol toolkit library
add_subdirectory(src/lib)

#=============================================================================
# EXAMPLES
#=============================================================================

# Add examples (only if requested)
if(PTK_BUILD_EXAMPLES)
    add_subdirectory(src/examples)
endif()

# Add tests (only if requested)
if(PTK_BUILD_TESTS)
    add_subdirectory(src/tests)
endif()

#=============================================================================
# INSTALLATION
#=============================================================================

if(PTK_INSTALL)
    include(CMakePackageConfigHelpers)
    
    # Install the main library target
    install(TARGETS ptk
        EXPORT PTKTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
    
    # Install headers
    install(FILES
        src/include/ptk.h
        src/include/ptk_alloc.h
        src/include/ptk_array.h
        src/include/ptk_atomic.h
        src/include/ptk_buf.h
        src/include/ptk_config.h
        src/include/ptk_err.h
        src/include/ptk_log.h
        src/include/ptk_os_thread.h
        src/include/ptk_shared.h
        src/include/ptk_sock.h
        src/include/ptk_threadlet.h
        src/include/ptk_utils.h
        DESTINATION include
    )
    
    # Install example executables if built
    if(TARGET arithmetic_server)
        install(TARGETS arithmetic_server arithmetic_client
            RUNTIME DESTINATION bin
        )
    endif()

    # Generate and install CMake config files
    write_basic_package_version_file(
        PTKConfigVersion.cmake
        VERSION 1.0.0
        COMPATIBILITY AnyNewerVersion
    )

    install(EXPORT PTKTargets
        FILE PTKTargets.cmake
        NAMESPACE ptk::
        DESTINATION lib/cmake/PTK
    )

    configure_file(cmake/PTKConfig.cmake.in PTKConfig.cmake @ONLY)
    install(FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/PTKConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/PTKConfigVersion.cmake
        DESTINATION lib/cmake/PTK
    )

    # Install documentation
    install(FILES
        how-to-use-protocol-toolkit.md
        DESTINATION share/doc/protocol_toolkit
    )

    # Install example documentation if it exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/examples/howto/howto.md)
        install(FILES
            src/examples/howto/howto.md
            DESTINATION share/doc/protocol_toolkit/examples
        )
    endif()
endif()

#=============================================================================
# TESTING
#=============================================================================

# Add test targets
# add_custom_target(test_sockets
#     COMMENT "Testing socket implementation"
#     DEPENDS test_tcp_echo_abort test_udp_echo_abort
# )

#=============================================================================
# INFORMATION
#=============================================================================

# Only print detailed information if this is the main project
if(PTK_IS_MAIN_PROJECT)
    # Print build information
    message(STATUS "Building Protocol Toolkit")
    message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
    message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")

    # Usage instructions
    message(STATUS "")
    message(STATUS "Build instructions:")
    message(STATUS "  mkdir build && cd build")
    message(STATUS "  cmake ..")
    message(STATUS "  make")
    message(STATUS "")
    message(STATUS "Libraries built:")
    message(STATUS "  - protocol_toolkit: Core utilities (buf, err, log, socket, thread, utils)")
    message(STATUS "")
    
    if(PTK_BUILD_EXAMPLES)
        message(STATUS "Examples:")
        message(STATUS "  - ethernetip: EtherNet/IP device discovery tool")
        message(STATUS "  - modbus: Implementation guide and documentation")
        message(STATUS "  - howto: Arithmetic protocol tutorial (client/server with CRC)")
        message(STATUS "")
    endif()
    
    if(PTK_BUILD_TESTS)
        message(STATUS "Tests available:")
        message(STATUS "  - test_tcp_echo_abort: TCP socket abort functionality")
        message(STATUS "  - test_udp_echo_abort: UDP socket abort functionality")
        message(STATUS "")
    endif()
else()
    message(STATUS "Protocol Toolkit - included as subproject")
endif()