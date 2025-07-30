# macOS Platform Implementation
# This file defines the Protocol Toolkit library for macOS using kevent

# Source files for the library
set(PTK_MACOS_SOURCES
    src/protocol_toolkit_macos.c
)

# Create static library (STATIC is explicit to ensure no shared library is built)
add_library(protocol_toolkit_macos STATIC ${PTK_MACOS_SOURCES})

# Set library properties
set_target_properties(protocol_toolkit_macos PROPERTIES
    OUTPUT_NAME "protocol_toolkit_macos"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME protocol_toolkit_macos
    # Ensure this is always a static library
    LIBRARY_OUTPUT_DIRECTORY ""  # Disable shared library output
)

# Include directories for the library
target_include_directories(protocol_toolkit_macos PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Platform-specific settings for macOS
if(APPLE)
    target_compile_definitions(protocol_toolkit_macos PRIVATE
        _DARWIN_C_SOURCE
    )
endif()

# Create an alias for consistent naming
add_library(ProtocolToolkit::macos ALIAS protocol_toolkit_macos)

# Installation (only when root project)
if(PTK_IS_ROOT_PROJECT)
    install(TARGETS protocol_toolkit_macos
        EXPORT ProtocolToolkitTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )

    install(FILES include/protocol_toolkit.h
        DESTINATION include
    )

    # Package configuration
    include(CMakePackageConfigHelpers)

    # Create config file
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProtocolToolkitConfig.cmake.in"
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
