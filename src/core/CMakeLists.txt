# Hydrogen Core Component
# This component provides the fundamental shared functionality for the Hydrogen library

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenCompiler.cmake)

# =============================================================================
# Target Definition
# =============================================================================

# Define the core library
add_library(astrocomm_core STATIC
    common/core.cpp
    common/utils.cpp
    common/message.cpp
    common/message_queue.cpp
    common/error_recovery.cpp
    common/device_interface.cpp
    ${CMAKE_SOURCE_DIR}/src/common/logger.cpp
)

# =============================================================================
# Modern CMake Configuration
# =============================================================================

# Apply modern CMake best practices
hydrogen_apply_modern_cmake(astrocomm_core
    NAMESPACE AstroComm
    EXPORT_NAME Core
)

# Apply standard Hydrogen target configuration
hydrogen_configure_target_standard(astrocomm_core)

# Configure platform-specific settings
hydrogen_configure_platform_target(astrocomm_core)

# =============================================================================
# Dependencies
# =============================================================================

# Link system dependencies
hydrogen_link_system_dependencies(astrocomm_core)

# Link required dependencies
target_link_libraries(astrocomm_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        Threads::Threads
)

# Optional spdlog dependency (if available)
if(TARGET spdlog::spdlog)
    target_link_libraries(astrocomm_core PUBLIC spdlog::spdlog)
    target_compile_definitions(astrocomm_core PUBLIC HYDROGEN_HAS_SPDLOG)
endif()

# Conditional Boost dependencies
if(HYDROGEN_HAS_BOOST_REGEX)
    target_link_libraries(astrocomm_core PRIVATE Boost::regex)
    target_compile_definitions(astrocomm_core PRIVATE HYDROGEN_USE_BOOST_REGEX)
endif()

# =============================================================================
# Feature Configuration
# =============================================================================

# Configure logging features
if(HYDROGEN_ENABLE_LOGGING)
    target_compile_definitions(astrocomm_core PUBLIC HYDROGEN_ENABLE_LOGGING)
endif()

# Configure SSL support
if(HYDROGEN_HAS_SSL)
    target_compile_definitions(astrocomm_core PUBLIC HYDROGEN_HAS_SSL)
endif()

# Configure compression support
if(HYDROGEN_HAS_COMPRESSION)
    target_compile_definitions(astrocomm_core PUBLIC HYDROGEN_HAS_COMPRESSION)
endif()

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
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommCoreConfig.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommCoreConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/AstroCommCoreConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
    )
else()
    # Create a basic config file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AstroCommCoreConfig.cmake"
        "# AstroComm Core Component Configuration\n"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(nlohmann_json)\n"
        "find_dependency(Threads)\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/AstroCommCoreTargets.cmake\")\n"
    )
endif()

# Create version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommCoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommCoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommCoreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
)

# =============================================================================
# Component Summary
# =============================================================================

message(STATUS "Hydrogen: Core component configured")
message(STATUS "  - Target: astrocomm_core (AstroComm::Core)")
message(STATUS "  - Type: Static library")
message(STATUS "  - Dependencies: nlohmann_json, Threads")
if(TARGET spdlog::spdlog)
    message(STATUS "  - Optional: spdlog (found)")
else()
    message(STATUS "  - Optional: spdlog (not found)")
endif()
