# Hydrogen Device Component
# This component provides concrete device implementations for astronomical equipment

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenCompiler.cmake)

# =============================================================================
# Target Definition
# =============================================================================

# Define the device library
add_library(astrocomm_device STATIC
    src/device.cpp
    src/websocket_device.cpp
    src/telescope.cpp
    ${CMAKE_SOURCE_DIR}/src/device/device_base.cpp
    ${CMAKE_SOURCE_DIR}/src/common/message.cpp
    ${CMAKE_SOURCE_DIR}/src/common/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/device/camera.cpp
    ${CMAKE_SOURCE_DIR}/src/device/focuser.cpp
    ${CMAKE_SOURCE_DIR}/src/device/rotator.cpp
    ${CMAKE_SOURCE_DIR}/src/device/solver.cpp
    ${CMAKE_SOURCE_DIR}/src/device/switch.cpp
    ${CMAKE_SOURCE_DIR}/src/device/guider.cpp
)

# =============================================================================
# Modern CMake Configuration
# =============================================================================

# Apply modern CMake best practices
hydrogen_apply_modern_cmake(astrocomm_device
    NAMESPACE AstroComm
    EXPORT_NAME Device
)

# Apply standard Hydrogen target configuration
hydrogen_configure_target_standard(astrocomm_device)

# Configure platform-specific settings
hydrogen_configure_platform_target(astrocomm_device)

# =============================================================================
# Dependencies
# =============================================================================

# Link system dependencies
hydrogen_link_system_dependencies(astrocomm_device)

# Core dependencies
target_link_libraries(astrocomm_device
    PUBLIC
        AstroComm::Core
    PRIVATE
        Threads::Threads
)

# Boost dependencies for networking (required for device communication)
if(TARGET Boost::system)
    target_link_libraries(astrocomm_device PRIVATE
        Boost::system
        Boost::thread
    )
else()
    message(WARNING "Hydrogen: Device component requires Boost libraries for networking")
endif()

# Optional SSL support for secure device connections
if(HYDROGEN_HAS_SSL AND TARGET OpenSSL::SSL)
    target_link_libraries(astrocomm_device PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_compile_definitions(astrocomm_device PRIVATE HYDROGEN_DEVICE_SSL_SUPPORT)
endif()

# =============================================================================
# Feature Configuration
# =============================================================================

# Configure device features based on available dependencies
if(HYDROGEN_HAS_SSL)
    target_compile_definitions(astrocomm_device PUBLIC HYDROGEN_DEVICE_HAS_SSL)
endif()

if(HYDROGEN_HAS_COMPRESSION)
    target_compile_definitions(astrocomm_device PUBLIC HYDROGEN_DEVICE_COMPRESSION_SUPPORT)
endif()

# Configure device-specific features
target_compile_definitions(astrocomm_device PRIVATE
    HYDROGEN_DEVICE_WEBSOCKET_ENABLED
    HYDROGEN_DEVICE_TELESCOPE_SUPPORT
    HYDROGEN_DEVICE_CAMERA_SUPPORT
    HYDROGEN_DEVICE_FOCUSER_SUPPORT
    HYDROGEN_DEVICE_ROTATOR_SUPPORT
    HYDROGEN_DEVICE_SOLVER_SUPPORT
    HYDROGEN_DEVICE_SWITCH_SUPPORT
    HYDROGEN_DEVICE_GUIDER_SUPPORT
)

# =============================================================================
# Installation and Export
# =============================================================================

# Headers will be installed by the main export system
# Individual component exports are handled by the main CMakeLists.txt

# Install headers with proper structure
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# =============================================================================
# Package Configuration
# =============================================================================

include(CMakePackageConfigHelpers)

# Create config file if template exists, otherwise create a basic one
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommDeviceConfig.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommDeviceConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/AstroCommDeviceConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
    )
else()
    # Create a basic config file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AstroCommDeviceConfig.cmake"
        "# AstroComm Device Component Configuration\n"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(AstroCommCore)\n"
        "find_dependency(Boost COMPONENTS system thread)\n"
        "find_dependency(Threads)\n"
        "if(HYDROGEN_HAS_SSL)\n"
        "    find_dependency(OpenSSL)\n"
        "endif()\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/AstroCommDeviceTargets.cmake\")\n"
    )
endif()

# Create version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommDeviceConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommDeviceConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommDeviceConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
)

# =============================================================================
# Component Summary
# =============================================================================

message(STATUS "Hydrogen: Device component configured")
message(STATUS "  - Target: astrocomm_device (AstroComm::Device)")
message(STATUS "  - Type: Static library")
message(STATUS "  - Dependencies: Core, Boost, Threads")
message(STATUS "  - Device types: Telescope, Camera, Focuser, Rotator, Solver, Switch, Guider")
if(HYDROGEN_HAS_SSL)
    message(STATUS "  - SSL support: Enabled")
else()
    message(STATUS "  - SSL support: Disabled")
endif()
if(TARGET Boost::system)
    message(STATUS "  - Networking: Boost (found)")
else()
    message(STATUS "  - Networking: Boost (not found)")
endif()
