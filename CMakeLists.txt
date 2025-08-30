# Hydrogen - Modern astronomical device communication protocol and framework
# Modern CMake build system with modular architecture and package manager support

cmake_minimum_required(VERSION 3.15)

# Project definition
project(Hydrogen
    VERSION 1.0.0
    DESCRIPTION "Modern astronomical device communication protocol and framework"
    HOMEPAGE_URL "https://github.com/hydrogen-project/hydrogen"
    LANGUAGES CXX
)

# =============================================================================
# Build System Initialization
# =============================================================================

# Add cmake modules to path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include Hydrogen build system modules
include(HydrogenUtils)
include(HydrogenPackageManagers)
include(HydrogenConfig)
include(HydrogenCompiler)
include(HydrogenDependencies)
include(HydrogenVersions)
include(HydrogenFeatures)
include(HydrogenExport)

# Initialize the Hydrogen build system
hydrogen_initialize()

# Initialize feature system
hydrogen_initialize_features()

# Configure build system
hydrogen_configure_build()

# =============================================================================
# Backward Compatibility Options
# =============================================================================

# Map old option names to new ones for backward compatibility
if(DEFINED BUILD_PYTHON_BINDINGS)
    set(HYDROGEN_ENABLE_PYTHON_BINDINGS ${BUILD_PYTHON_BINDINGS} CACHE BOOL "Build Python bindings" FORCE)
endif()

if(DEFINED BUILD_TESTS)
    set(HYDROGEN_BUILD_TESTS ${BUILD_TESTS} CACHE BOOL "Build unit tests" FORCE)
endif()

# =============================================================================
# Dependency Resolution
# =============================================================================

# Find all dependencies using the unified system
hydrogen_find_all_dependencies()

# Perform version management and conflict resolution
hydrogen_manage_versions()

# =============================================================================
# Legacy cpp-base64 Support (Temporary)
# =============================================================================

# Handle cpp-base64 dependency (this is a legacy dependency that should be replaced)
# For now, we maintain backward compatibility
set(CPP_BASE64_PATH "" CACHE PATH "Path to cpp-base64 library (optional)")
if(NOT CPP_BASE64_PATH STREQUAL "")
    message(STATUS "Hydrogen: Using cpp-base64 from: ${CPP_BASE64_PATH}")
    set(cpp-base64_SOURCE_DIR "${CPP_BASE64_PATH}")
    # Verify the path has the needed files
    if(NOT EXISTS "${cpp-base64_SOURCE_DIR}/base64.cpp" OR NOT EXISTS "${cpp-base64_SOURCE_DIR}/base64.h")
        message(FATAL_ERROR "Hydrogen: Invalid cpp-base64 path: ${CPP_BASE64_PATH}. base64.cpp or base64.h not found.")
    endif()
else()
    # Use FetchContent as fallback for cpp-base64
    include(FetchContent)
    if(NOT DEFINED FETCHCONTENT_BASE_DIR)
        set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cmake-cache" CACHE PATH "Base directory for FetchContent downloads")
    endif()

    set(CPP_BASE64_CACHE "${FETCHCONTENT_BASE_DIR}/cpp-base64-master")

    if(EXISTS "${CPP_BASE64_CACHE}/base64.cpp" AND EXISTS "${CPP_BASE64_CACHE}/base64.h")
        message(STATUS "Hydrogen: Using cached cpp-base64 at: ${CPP_BASE64_CACHE}")
        set(cpp-base64_SOURCE_DIR "${CPP_BASE64_CACHE}")
    else()
        message(STATUS "Hydrogen: Fetching cpp-base64 from GitHub")
        FetchContent_Declare(
            cpp-base64
            GIT_REPOSITORY https://github.com/ReneNyffenegger/cpp-base64.git
            GIT_TAG master
            SOURCE_DIR "${CPP_BASE64_CACHE}"
        )
        FetchContent_MakeAvailable(cpp-base64)
    endif()
endif()

# =============================================================================
# Global Include Directories (Legacy Support)
# =============================================================================

# Add global include directories for backward compatibility
# Note: Modern CMake prefers target-specific includes, but we maintain this for compatibility
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${cpp-base64_SOURCE_DIR}
)

# =============================================================================
# Component Building
# =============================================================================

# Add component subdirectories in dependency order
message(STATUS "Hydrogen: Building components...")
add_subdirectory(src/common)
add_subdirectory(src/core)
add_subdirectory(src/server)
add_subdirectory(src/client_component)
add_subdirectory(src/device_component)

# Add applications
add_subdirectory(src/apps)

# =============================================================================
# Testing
# =============================================================================

# Add tests if enabled
if(HYDROGEN_BUILD_TESTS)
    message(STATUS "Hydrogen: Enabling testing...")
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Convenience Libraries (Backward Compatibility)
# =============================================================================

# Create a convenience library that includes all components (for backward compatibility)
add_library(astrocomm INTERFACE)
target_link_libraries(astrocomm INTERFACE
    AstroComm::Common
    AstroComm::Core
    AstroComm::Server
    AstroComm::Client
    AstroComm::Device
)

# Create modern Hydrogen namespace alias
add_library(Hydrogen::Hydrogen ALIAS astrocomm)

# =============================================================================
# Python Bindings
# =============================================================================

# Python bindings (optional)
if(HYDROGEN_ENABLE_PYTHON_BINDINGS)
    message(STATUS "Hydrogen: Building Python bindings...")

    # Find Python interpreter
    find_package(Python COMPONENTS Interpreter Development REQUIRED)
    message(STATUS "Hydrogen: Found Python: ${Python_VERSION}")

    # pybind11 should already be found by the dependency system
    if(pybind11_FOUND)
        # Python module with comprehensive bindings and 100% API parity
        pybind11_add_module(pyhydrogen
            src/python/bindings.cpp
            src/python/py_dome.cpp
            src/python/py_observing_conditions.cpp
            src/python/py_error_handling.cpp
            src/python/py_type_safety.cpp
            src/python/py_documentation.cpp
        )
        target_link_libraries(pyhydrogen PRIVATE
            AstroComm::Device
            AstroComm::Client
            AstroComm::Server
            AstroComm::Core
        )

        # Apply modern CMake configuration
        hydrogen_configure_target_standard(pyhydrogen)

        # Install Python module
        install(TARGETS pyhydrogen
            LIBRARY DESTINATION ${Python_SITEARCH}
        )

        # Backward compatibility alias
        add_library(pyastrocomm ALIAS pyhydrogen)
    else()
        message(WARNING "Hydrogen: Python bindings requested but pybind11 not found")
    endif()
endif()

# =============================================================================
# Installation and Packaging
# =============================================================================

# Include standard installation directories
include(GNUInstallDirs)

# =============================================================================
# Export and Installation Setup
# =============================================================================

# Export all targets using the new export system
hydrogen_export_target(astrocomm_common EXPORT_NAME Common COMPONENT Development)
hydrogen_export_target(astrocomm_core EXPORT_NAME Core COMPONENT Development)
hydrogen_export_target(astrocomm_server EXPORT_NAME Server COMPONENT Development)
hydrogen_export_target(astrocomm_client EXPORT_NAME Client COMPONENT Development)
hydrogen_export_target(astrocomm_device EXPORT_NAME Device COMPONENT Development)
hydrogen_export_target(astrocomm EXPORT_NAME AstroComm COMPONENT Runtime)

# Setup comprehensive package exports
hydrogen_setup_exports()

# =============================================================================
# Build Summary
# =============================================================================

message(STATUS "")
message(STATUS "=== Hydrogen Build Summary ===")
message(STATUS "Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Package manager: ${HYDROGEN_PRIMARY_PACKAGE_MANAGER}")
message(STATUS "Components: Core, Server, Client, Device")
message(STATUS "Applications: ${HYDROGEN_BUILD_EXAMPLES}")
message(STATUS "Tests: ${HYDROGEN_BUILD_TESTS}")
message(STATUS "Python bindings: ${HYDROGEN_ENABLE_PYTHON_BINDINGS}")
message(STATUS "SSL support: ${HYDROGEN_HAS_SSL}")
message(STATUS "Compression: ${HYDROGEN_HAS_COMPRESSION}")
message(STATUS "==============================")
message(STATUS "")
