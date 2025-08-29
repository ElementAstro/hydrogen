# Hydrogen Server Component
# This component provides server-side functionality for device management and HTTP/WebSocket services

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenCompiler.cmake)

# =============================================================================
# Target Definition
# =============================================================================

# Define the server library
add_library(astrocomm_server STATIC
    src/server.cpp
    src/device_server.cpp
    src/device_manager.cpp
    src/auth_manager.cpp
)

# =============================================================================
# Modern CMake Configuration
# =============================================================================

# Apply modern CMake best practices
hydrogen_apply_modern_cmake(astrocomm_server
    NAMESPACE AstroComm
    EXPORT_NAME Server
)

# Apply standard Hydrogen target configuration
hydrogen_configure_target_standard(astrocomm_server)

# Configure platform-specific settings
hydrogen_configure_platform_target(astrocomm_server)

# =============================================================================
# Dependencies
# =============================================================================

# Link system dependencies
hydrogen_link_system_dependencies(astrocomm_server)

# Core dependencies
target_link_libraries(astrocomm_server
    PUBLIC
        AstroComm::Core
    PRIVATE
        Threads::Threads
)

# SSL dependencies (if available)
if(HYDROGEN_HAS_SSL AND TARGET OpenSSL::SSL)
    target_link_libraries(astrocomm_server PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_compile_definitions(astrocomm_server PRIVATE HYDROGEN_SERVER_HAS_SSL)
endif()

# Boost dependencies (required for server)
if(TARGET Boost::system)
    target_link_libraries(astrocomm_server PRIVATE
        Boost::system
        Boost::filesystem
        Boost::thread
    )
else()
    message(WARNING "Hydrogen: Server component requires Boost libraries")
endif()

# Crow web framework dependency
if(TARGET Crow::Crow)
    target_link_libraries(astrocomm_server PRIVATE Crow::Crow)
    target_compile_definitions(astrocomm_server PRIVATE HYDROGEN_HAS_CROW)
elseif(TARGET crow)
    target_link_libraries(astrocomm_server PRIVATE crow)
    target_compile_definitions(astrocomm_server PRIVATE HYDROGEN_HAS_CROW)
else()
    message(WARNING "Hydrogen: Server component requires Crow web framework")
endif()

# =============================================================================
# Feature Configuration
# =============================================================================

# Configure server features based on available dependencies
if(HYDROGEN_HAS_SSL)
    target_compile_definitions(astrocomm_server PUBLIC HYDROGEN_SERVER_SSL_SUPPORT)
endif()

if(HYDROGEN_HAS_COMPRESSION)
    target_compile_definitions(astrocomm_server PUBLIC HYDROGEN_SERVER_COMPRESSION_SUPPORT)
endif()

# Configure authentication features
target_compile_definitions(astrocomm_server PRIVATE HYDROGEN_SERVER_AUTH_ENABLED)

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
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommServerConfig.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/AstroCommServerConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/AstroCommServerConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
    )
else()
    # Create a basic config file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/AstroCommServerConfig.cmake"
        "# AstroComm Server Component Configuration\n"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(AstroCommCore)\n"
        "find_dependency(Boost COMPONENTS system filesystem thread)\n"
        "find_dependency(Threads)\n"
        "if(HYDROGEN_HAS_SSL)\n"
        "    find_dependency(OpenSSL)\n"
        "endif()\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/AstroCommServerTargets.cmake\")\n"
    )
endif()

# Create version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommServerConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommServerConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AstroCommServerConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AstroComm
)

# =============================================================================
# Component Summary
# =============================================================================

message(STATUS "Hydrogen: Server component configured")
message(STATUS "  - Target: astrocomm_server (AstroComm::Server)")
message(STATUS "  - Type: Static library")
message(STATUS "  - Dependencies: Core, Boost, Threads")
if(HYDROGEN_HAS_SSL)
    message(STATUS "  - SSL support: Enabled")
else()
    message(STATUS "  - SSL support: Disabled")
endif()
if(TARGET Crow::Crow OR TARGET crow)
    message(STATUS "  - Web framework: Crow (found)")
else()
    message(STATUS "  - Web framework: Crow (not found)")
endif()
