# Linux Platform Implementation
# This file defines the Protocol Toolkit library for Linux using epoll

# Generate the unified header for Linux
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/protocol_toolkit_impl.h.in" PLATFORM_IMPL_CONTENT)

# Create build include directory at project root level
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../../include")

# Generate the unified header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/protocol_toolkit.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/../../include/protocol_toolkit.h"
    @ONLY
)

# Source files for the library
set(PTK_LINUX_SOURCES
    src/protocol_toolkit_linux.c
)

# Create static library (STATIC is explicit to ensure no shared library is built)
add_library(protocol_toolkit STATIC ${PTK_LINUX_SOURCES})

# Set library properties
set_target_properties(protocol_toolkit PROPERTIES
    OUTPUT_NAME "protocol_toolkit"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME protocol_toolkit
    # Ensure this is always a static library
    LIBRARY_OUTPUT_DIRECTORY ""  # Disable shared library output
)

# Include directories for the library
target_include_directories(protocol_toolkit PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../../include>
    $<INSTALL_INTERFACE:include>
)

# Platform-specific settings for Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(protocol_toolkit PRIVATE
        _GNU_SOURCE
    )
endif()

# Create aliases for consistent naming
add_library(ProtocolToolkit::linux ALIAS protocol_toolkit)
add_library(ProtocolToolkit::platform ALIAS protocol_toolkit)

# Installation (only when root project)
if(PTK_IS_ROOT_PROJECT)
    install(TARGETS protocol_toolkit
        EXPORT ProtocolToolkitTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/../include/protocol_toolkit.h"
        DESTINATION include
    )

    # Package configuration
    include(CMakePackageConfigHelpers)

    # Create config file
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ProtocolToolkitConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ProtocolToolkitConfig.cmake"
        INSTALL_DESTINATION lib/cmake/ProtocolToolkit
    )

    # Create version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ProtocolToolkitConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    # Install config files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ProtocolToolkitConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ProtocolToolkitConfigVersion.cmake"
        DESTINATION lib/cmake/ProtocolToolkit
    )

    # Export targets
    install(EXPORT ProtocolToolkitTargets
        FILE ProtocolToolkitTargets.cmake
        NAMESPACE ProtocolToolkit::
        DESTINATION lib/cmake/ProtocolToolkit
    )

    # Add export to build tree
    export(EXPORT ProtocolToolkitTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/ProtocolToolkitTargets.cmake"
        NAMESPACE ProtocolToolkit::
    )
endif()