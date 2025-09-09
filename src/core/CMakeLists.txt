# Hydrogen Core Component
# This component provides the fundamental shared functionality for the Hydrogen library

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenCompiler.cmake)

# =============================================================================
# Target Definition
# =============================================================================

# Define the core library
add_library(hydrogen_core STATIC
    # Infrastructure
    src/infrastructure/core.cpp
    src/infrastructure/utils.cpp
    src/infrastructure/error_recovery.cpp
    src/infrastructure/performance_metrics.cpp
    src/infrastructure/discovery.cpp

    # Messaging
    src/messaging/message.cpp
    src/messaging/message_queue.cpp
    src/messaging/message_transformer.cpp
    src/messaging/message_validator.cpp

    # Device Management
    src/device/device_interface.cpp
    src/device/device_health.cpp
    src/device/device_lifecycle.cpp
    src/device/device_plugin.cpp

    # Configuration
    src/configuration/client_configuration.cpp
    src/configuration/stdio_config_manager.cpp
    src/configuration/fifo_config_manager.cpp

    # Communication - Protocols
    src/communication/protocols/stdio_communicator.cpp
    src/communication/protocols/fifo_communicator.cpp
    src/communication/protocols/grpc_communicator.cpp
    src/communication/protocols/mqtt_communicator.cpp
    # src/communication/protocols/tcp_communicator.cpp   # Temporarily disabled due to complex inheritance issues
    src/communication/protocols/zmq_communicator.cpp
    # src/communication/protocols/websocket_error_handler.cpp  # Temporarily disabled due to build issues
    # src/communication/protocols/unified_websocket_error_handler.cpp  # Temporarily disabled due to build issues

    # Communication - Infrastructure
    # src/communication/infrastructure/multi_protocol_communicator.cpp  # Temporarily disabled due to TCP communicator interface issues
    src/communication/infrastructure/protocol_communicator_factory.cpp
    src/communication/infrastructure/protocol_converters.cpp
    src/communication/infrastructure/protocol_error_mapper.cpp

    # Communication - Clients
    src/communication/clients/grpc_streaming_client.cpp
    # src/communication/clients/zmq_queue_client.cpp  # Temporarily disabled due to complex header issues

    # Communication - Connection Management
    # src/communication/connection/unified_device_client.cpp  # Temporarily disabled due to spdlog guard issues
    # src/communication/connection/unified_connection_manager.cpp  # Temporarily disabled due to complex header issues

    # Logging
    src/logging/stdio_logger.cpp
    src/logging/fifo_logger.cpp

    # External dependencies
    ${CMAKE_SOURCE_DIR}/src/common/logger.cpp
)

# =============================================================================
# Modern CMake Configuration
# =============================================================================

# Apply modern CMake best practices
hydrogen_apply_modern_cmake(hydrogen_core
    NAMESPACE Hydrogen
    EXPORT_NAME Core
)

# Apply standard Hydrogen target configuration
hydrogen_configure_target_standard(hydrogen_core)

# Configure platform-specific settings
hydrogen_configure_platform_target(hydrogen_core)

# =============================================================================
# Dependencies
# =============================================================================

# Link system dependencies
hydrogen_link_system_dependencies(hydrogen_core)

# Link required dependencies
target_link_libraries(hydrogen_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        Threads::Threads
)

# Optional spdlog dependency is inherited from hydrogen_common
# No need to link it again here to avoid duplicate symbols

# Conditional Boost dependencies
if(HYDROGEN_HAS_BOOST_REGEX)
    target_link_libraries(hydrogen_core PRIVATE Boost::regex)
    target_compile_definitions(hydrogen_core PRIVATE HYDROGEN_USE_BOOST_REGEX)
endif()

# =============================================================================
# Feature Configuration
# =============================================================================

# Configure logging features
if(HYDROGEN_ENABLE_LOGGING)
    target_compile_definitions(hydrogen_core PUBLIC HYDROGEN_ENABLE_LOGGING)
endif()

# Configure SSL support
if(HYDROGEN_HAS_SSL)
    target_compile_definitions(hydrogen_core PUBLIC HYDROGEN_HAS_SSL)
endif()

# Configure compression support
if(HYDROGEN_HAS_COMPRESSION)
    target_compile_definitions(hydrogen_core PUBLIC HYDROGEN_HAS_COMPRESSION)
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
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/HydrogenCoreConfig.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/HydrogenCoreConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/HydrogenCoreConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hydrogen
    )
else()
    # Create a basic config file
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/HydrogenCoreConfig.cmake"
        "# Hydrogen Core Component Configuration\n"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(nlohmann_json)\n"
        "find_dependency(Threads)\n"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/HydrogenCoreTargets.cmake\")\n"
    )
endif()

# Create version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HydrogenCoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HydrogenCoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/HydrogenCoreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Hydrogen
)

# =============================================================================
# Component Summary
# =============================================================================

message(STATUS "Hydrogen: Core component configured")
message(STATUS "  - Target: hydrogen_core (Hydrogen::Core)")
message(STATUS "  - Type: Static library")
message(STATUS "  - Dependencies: nlohmann_json, Threads")
message(STATUS "  - Optional: spdlog (inherited from Common)")
