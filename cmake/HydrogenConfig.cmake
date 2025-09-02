# HydrogenConfig.cmake - Comprehensive configuration system for build options and features
# This module provides a centralized configuration system for managing build options,
# feature toggles, and conditional compilation across all components.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_CONFIG_INCLUDED)
    return()
endif()
set(HYDROGEN_CONFIG_INCLUDED TRUE)

# =============================================================================
# Project Configuration
# =============================================================================

# Set project-wide defaults
set(HYDROGEN_DEFAULT_CXX_STANDARD 17 CACHE STRING "Default C++ standard for all targets")
set(HYDROGEN_DEFAULT_BUILD_TYPE "Release" CACHE STRING "Default build type")

# Ensure build type is set
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Hydrogen: Setting build type to '${HYDROGEN_DEFAULT_BUILD_TYPE}' as none was specified")
    set(CMAKE_BUILD_TYPE "${HYDROGEN_DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# =============================================================================
# Build Options
# =============================================================================

# Core build options
option(HYDROGEN_BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)
option(HYDROGEN_BUILD_TESTS "Build unit tests" ON)
option(HYDROGEN_BUILD_EXAMPLES "Build example applications" ON)
option(HYDROGEN_BUILD_DOCS "Build documentation" OFF)
option(HYDROGEN_BUILD_BENCHMARKS "Build performance benchmarks" OFF)

# Development options
option(HYDROGEN_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(HYDROGEN_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(HYDROGEN_ENABLE_SANITIZERS "Enable runtime sanitizers in Debug builds" OFF)
option(HYDROGEN_ENABLE_COVERAGE "Enable code coverage analysis" OFF)
option(HYDROGEN_ENABLE_LTO "Enable Link Time Optimization in Release builds" OFF)

# Feature options
option(HYDROGEN_ENABLE_PYTHON_BINDINGS "Build Python bindings" OFF)
option(HYDROGEN_ENABLE_SSL "Enable SSL/TLS support" ON)
option(HYDROGEN_ENABLE_COMPRESSION "Enable compression support" ON)
option(HYDROGEN_ENABLE_LOGGING "Enable detailed logging" ON)
option(HYDROGEN_ENABLE_PROFILING "Enable profiling support" OFF)

# Package manager preferences
option(HYDROGEN_PREFER_VCPKG "Prefer vcpkg over other package managers" OFF)
option(HYDROGEN_PREFER_CONAN "Prefer Conan over other package managers" OFF)
option(HYDROGEN_DISABLE_VCPKG "Disable vcpkg package manager completely" OFF)
option(HYDROGEN_ALLOW_FETCHCONTENT "Allow FetchContent fallback for missing packages" ON)

# =============================================================================
# Feature Detection and Configuration
# =============================================================================

# Define available features with their dependencies
function(hydrogen_configure_features)
    message(STATUS "Hydrogen: Configuring features...")
    
    # SSL Feature
    if(HYDROGEN_ENABLE_SSL)
        find_package(OpenSSL QUIET)
        if(OpenSSL_FOUND)
            set(HYDROGEN_HAS_SSL TRUE CACHE BOOL "SSL support available")
            message(STATUS "Hydrogen: SSL support enabled (OpenSSL ${OPENSSL_VERSION})")
        else()
            set(HYDROGEN_HAS_SSL FALSE CACHE BOOL "SSL support available")
            message(WARNING "Hydrogen: SSL support requested but OpenSSL not found")
            if(HYDROGEN_WARNINGS_AS_ERRORS)
                message(FATAL_ERROR "Hydrogen: SSL support required but not available")
            endif()
        endif()
    else()
        set(HYDROGEN_HAS_SSL FALSE CACHE BOOL "SSL support available")
        message(STATUS "Hydrogen: SSL support disabled")
    endif()
    
    # Compression Feature
    if(HYDROGEN_ENABLE_COMPRESSION)
        find_package(ZLIB QUIET)
        if(ZLIB_FOUND)
            set(HYDROGEN_HAS_COMPRESSION TRUE CACHE BOOL "Compression support available")
            message(STATUS "Hydrogen: Compression support enabled")
        else()
            set(HYDROGEN_HAS_COMPRESSION FALSE CACHE BOOL "Compression support available")
            message(WARNING "Hydrogen: Compression support requested but ZLIB not found")
        endif()
    else()
        set(HYDROGEN_HAS_COMPRESSION FALSE CACHE BOOL "Compression support available")
        message(STATUS "Hydrogen: Compression support disabled")
    endif()
    
    # Python Bindings Feature
    if(HYDROGEN_ENABLE_PYTHON_BINDINGS)
        find_package(Python COMPONENTS Interpreter Development QUIET)
        find_package(pybind11 QUIET)
        if(Python_FOUND AND pybind11_FOUND)
            set(HYDROGEN_HAS_PYTHON_BINDINGS TRUE CACHE BOOL "Python bindings available")
            message(STATUS "Hydrogen: Python bindings enabled (Python ${Python_VERSION})")
        else()
            set(HYDROGEN_HAS_PYTHON_BINDINGS FALSE CACHE BOOL "Python bindings available")
            message(WARNING "Hydrogen: Python bindings requested but dependencies not found")
            if(NOT Python_FOUND)
                message(STATUS "  - Python interpreter/development files not found")
            endif()
            if(NOT pybind11_FOUND)
                message(STATUS "  - pybind11 not found")
            endif()
        endif()
    else()
        set(HYDROGEN_HAS_PYTHON_BINDINGS FALSE CACHE BOOL "Python bindings available")
        message(STATUS "Hydrogen: Python bindings disabled")
    endif()
    
    # Testing Feature
    if(HYDROGEN_BUILD_TESTS)
        find_package(GTest QUIET)
        if(GTest_FOUND)
            set(HYDROGEN_HAS_TESTING TRUE CACHE BOOL "Testing framework available")
            enable_testing()
            message(STATUS "Hydrogen: Testing enabled (Google Test)")
        else()
            set(HYDROGEN_HAS_TESTING FALSE CACHE BOOL "Testing framework available")
            message(WARNING "Hydrogen: Testing requested but Google Test not found")
        endif()
    else()
        set(HYDROGEN_HAS_TESTING FALSE CACHE BOOL "Testing framework available")
        message(STATUS "Hydrogen: Testing disabled")
    endif()
endfunction()

# =============================================================================
# Compiler Configuration
# =============================================================================

function(hydrogen_configure_compiler)
    message(STATUS "Hydrogen: Configuring compiler settings...")
    
    # Set C++ standard
    set(CMAKE_CXX_STANDARD ${HYDROGEN_DEFAULT_CXX_STANDARD} PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    
    # Position Independent Code
    set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
    
    # Export compile commands for IDEs
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON PARENT_SCOPE)
    
    # Configure warnings
    if(HYDROGEN_ENABLE_WARNINGS)
        if(MSVC)
            set(HYDROGEN_WARNING_FLAGS "/W4" CACHE STRING "Warning flags for MSVC")
            if(HYDROGEN_WARNINGS_AS_ERRORS)
                string(APPEND HYDROGEN_WARNING_FLAGS " /WX")
            endif()
        else()
            set(HYDROGEN_WARNING_FLAGS "-Wall -Wextra -Wpedantic" CACHE STRING "Warning flags for GCC/Clang")
            if(HYDROGEN_WARNINGS_AS_ERRORS)
                string(APPEND HYDROGEN_WARNING_FLAGS " -Werror")
            endif()
        endif()
        message(STATUS "Hydrogen: Compiler warnings enabled: ${HYDROGEN_WARNING_FLAGS}")
    endif()
    
    # Configure sanitizers for Debug builds
    if(HYDROGEN_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(HYDROGEN_SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer" CACHE STRING "Sanitizer flags")
            message(STATUS "Hydrogen: Sanitizers enabled for Debug builds")
        else()
            message(WARNING "Hydrogen: Sanitizers requested but not supported by current compiler")
        endif()
    endif()
    
    # Configure Link Time Optimization for Release builds
    if(HYDROGEN_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
        include(CheckIPOSupported)
        check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
        if(lto_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE PARENT_SCOPE)
            message(STATUS "Hydrogen: Link Time Optimization enabled for Release builds")
        else()
            message(WARNING "Hydrogen: LTO requested but not supported: ${lto_error}")
        endif()
    endif()
    
    # Configure coverage
    if(HYDROGEN_ENABLE_COVERAGE)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(HYDROGEN_COVERAGE_FLAGS "--coverage -fprofile-arcs -ftest-coverage" CACHE STRING "Coverage flags")
            message(STATUS "Hydrogen: Code coverage enabled")
        else()
            message(WARNING "Hydrogen: Coverage requested but not supported by current compiler")
        endif()
    endif()
endfunction()

# =============================================================================
# Target Configuration Helpers
# =============================================================================

# Apply standard configuration to a target
function(hydrogen_configure_target_standard target_name)
    # Apply C++ standard
    target_compile_features(${target_name} PUBLIC cxx_std_${HYDROGEN_DEFAULT_CXX_STANDARD})
    
    # Apply warning flags
    if(HYDROGEN_ENABLE_WARNINGS AND HYDROGEN_WARNING_FLAGS)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE ${HYDROGEN_WARNING_FLAGS})
        else()
            separate_arguments(warning_flags UNIX_COMMAND "${HYDROGEN_WARNING_FLAGS}")
            target_compile_options(${target_name} PRIVATE ${warning_flags})
        endif()
    endif()
    
    # Apply sanitizer flags for Debug builds
    if(HYDROGEN_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND HYDROGEN_SANITIZER_FLAGS)
        separate_arguments(sanitizer_flags UNIX_COMMAND "${HYDROGEN_SANITIZER_FLAGS}")
        target_compile_options(${target_name} PRIVATE ${sanitizer_flags})
        target_link_options(${target_name} PRIVATE ${sanitizer_flags})
    endif()
    
    # Apply coverage flags
    if(HYDROGEN_ENABLE_COVERAGE AND HYDROGEN_COVERAGE_FLAGS)
        separate_arguments(coverage_flags UNIX_COMMAND "${HYDROGEN_COVERAGE_FLAGS}")
        target_compile_options(${target_name} PRIVATE ${coverage_flags})
        target_link_options(${target_name} PRIVATE ${coverage_flags})
    endif()
    
    # Set target properties
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD ${HYDROGEN_DEFAULT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )
endfunction()

# =============================================================================
# Configuration Summary
# =============================================================================

function(hydrogen_print_configuration_summary)
    message(STATUS "")
    message(STATUS "=== Hydrogen Build Configuration Summary ===")
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "C++ standard: ${HYDROGEN_DEFAULT_CXX_STANDARD}")
    message(STATUS "Shared libraries: ${HYDROGEN_BUILD_SHARED_LIBS}")
    message(STATUS "")
    message(STATUS "Features:")
    message(STATUS "  Tests: ${HYDROGEN_BUILD_TESTS}")
    message(STATUS "  Examples: ${HYDROGEN_BUILD_EXAMPLES}")
    message(STATUS "  Documentation: ${HYDROGEN_BUILD_DOCS}")
    message(STATUS "  Python bindings: ${HYDROGEN_ENABLE_PYTHON_BINDINGS}")
    message(STATUS "  SSL support: ${HYDROGEN_ENABLE_SSL}")
    message(STATUS "  Compression: ${HYDROGEN_ENABLE_COMPRESSION}")
    message(STATUS "")
    message(STATUS "Development:")
    message(STATUS "  Warnings: ${HYDROGEN_ENABLE_WARNINGS}")
    message(STATUS "  Warnings as errors: ${HYDROGEN_WARNINGS_AS_ERRORS}")
    message(STATUS "  Sanitizers: ${HYDROGEN_ENABLE_SANITIZERS}")
    message(STATUS "  Coverage: ${HYDROGEN_ENABLE_COVERAGE}")
    message(STATUS "  LTO: ${HYDROGEN_ENABLE_LTO}")
    message(STATUS "")
    message(STATUS "Package Management:")
    message(STATUS "  Primary manager: ${HYDROGEN_PRIMARY_PACKAGE_MANAGER}")
    message(STATUS "  FetchContent fallback: ${HYDROGEN_ALLOW_FETCHCONTENT}")
    message(STATUS "===========================================")
    message(STATUS "")
endfunction()

# =============================================================================
# Main Configuration Function
# =============================================================================

function(hydrogen_configure_build)
    message(STATUS "Hydrogen: Configuring build system...")
    
    # Configure compiler settings
    hydrogen_configure_compiler()
    
    # Configure features
    hydrogen_configure_features()
    
    # Print configuration summary
    hydrogen_print_configuration_summary()
    
    message(STATUS "Hydrogen: Build configuration complete")
endfunction()
