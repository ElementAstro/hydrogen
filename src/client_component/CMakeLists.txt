# Hydrogen Client Component
# This component provides client-side functionality for connecting to device servers

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenCompiler.cmake)

# =============================================================================
# Target Definition
# =============================================================================

# Define the client library
add_library(astrocomm_client STATIC
    src/client.cpp
    src/device_client.cpp
)

# =============================================================================
# Modern CMake Configuration
# =============================================================================

# Apply modern CMake best practices
hydrogen_apply_modern_cmake(astrocomm_client
    NAMESPACE AstroComm
    EXPORT_NAME Client
)

# Apply standard Hydrogen target configuration
hydrogen_configure_target_standard(astrocomm_client)

# Configure platform-specific settings
hydrogen_configure_platform_target(astrocomm_client)

# =============================================================================
# Dependencies
# =============================================================================

# Link system dependencies
hydrogen_link_system_dependencies(astrocomm_client)

# Core dependencies
target_link_libraries(astrocomm_client
    PUBLIC
        AstroComm::Core
    PRIVATE
        Threads::Threads
)

# Boost dependencies for networking (required for client)
if(TARGET Boost::system)
    target_link_libraries(astrocomm_client PRIVATE
        Boost::system
        Boost::thread
    )
else()
    message(WARNING "Hydrogen: Client component requires Boost libraries for networking")
endif()

# Optional SSL support for secure connections
if(HYDROGEN_HAS_SSL AND TARGET OpenSSL::SSL)
    target_link_libraries(astrocomm_client PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_compile_definitions(astrocomm_client PRIVATE HYDROGEN_CLIENT_SSL_SUPPORT)
endif()

# =============================================================================
# Feature Configuration
# =============================================================================

# Configure client features based on available dependencies
if(HYDROGEN_HAS_SSL)
    target_compile_definitions(astrocomm_client PUBLIC HYDROGEN_CLIENT_HAS_SSL)
endif()

if(HYDROGEN_HAS_COMPRESSION)
    target_compile_definitions(astrocomm_client PUBLIC HYDROGEN_CLIENT_COMPRESSION_SUPPORT)
endif()

# Configure networking features
target_compile_definitions(astrocomm_client PRIVATE HYDROGEN_CLIENT_NETWORKING_ENABLED)

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
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommClientConfig.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommClientConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/AstroCommClientConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
    )
else()
    # Create a basic config file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AstroCommClientConfig.cmake"
        "# AstroComm Client Component Configuration\n"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(AstroCommCore)\n"
        "find_dependency(Boost COMPONENTS system thread)\n"
        "find_dependency(Threads)\n"
        "if(HYDROGEN_HAS_SSL)\n"
        "    find_dependency(OpenSSL)\n"
        "endif()\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/AstroCommClientTargets.cmake\")\n"
    )
endif()

# Create version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommClientConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommClientConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommClientConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
)

# =============================================================================
# Component Summary
# =============================================================================

message(STATUS "Hydrogen: Client component configured")
message(STATUS "  - Target: astrocomm_client (AstroComm::Client)")
message(STATUS "  - Type: Static library")
message(STATUS "  - Dependencies: Core, Boost, Threads")
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
