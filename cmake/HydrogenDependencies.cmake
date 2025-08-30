# HydrogenDependencies.cmake - Unified dependency management system
# This module provides a unified interface for managing dependencies across
# different package managers (vcpkg, Conan) with FetchContent fallback.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_DEPENDENCIES_INCLUDED)
    return()
endif()
set(HYDROGEN_DEPENDENCIES_INCLUDED TRUE)

# Include required modules
include(FetchContent)
include(${CMAKE_CURRENT_LIST_DIR}/HydrogenPackageManagers.cmake)

# =============================================================================
# Dependency Configuration
# =============================================================================

# Define all project dependencies with their fallback information
function(hydrogen_define_dependencies)
    # Core dependencies - using list format to avoid semicolon parsing issues
    set(HYDROGEN_CORE_DEPS "")
    list(APPEND HYDROGEN_CORE_DEPS "Boost|boost|https://github.com/boostorg/boost.git|boost-1.84.0")
    list(APPEND HYDROGEN_CORE_DEPS "OpenSSL|openssl|https://github.com/openssl/openssl.git|openssl-3.2.0")
    list(APPEND HYDROGEN_CORE_DEPS "nlohmann_json|nlohmann_json|https://github.com/nlohmann/json.git|v3.11.3")
    list(APPEND HYDROGEN_CORE_DEPS "Threads||")

    # Web framework dependencies
    set(HYDROGEN_WEB_DEPS "")
    list(APPEND HYDROGEN_WEB_DEPS "asio|asio|https://github.com/chriskohlhoff/asio.git|asio-1-18-2")
    list(APPEND HYDROGEN_WEB_DEPS "Crow|crow|https://github.com/CrowCpp/Crow.git|v1.2.0")

    # Optional dependencies
    set(HYDROGEN_OPTIONAL_DEPS "")
    list(APPEND HYDROGEN_OPTIONAL_DEPS "ZLIB|zlib|https://github.com/madler/zlib.git|v1.3.1")
    list(APPEND HYDROGEN_OPTIONAL_DEPS "GTest|gtest|https://github.com/google/googletest.git|v1.14.0")
    list(APPEND HYDROGEN_OPTIONAL_DEPS "pybind11|pybind11|https://github.com/pybind/pybind11.git|v2.11.1")
    list(APPEND HYDROGEN_OPTIONAL_DEPS "benchmark|benchmark|https://github.com/google/benchmark.git|v1.8.3")

    # Communication protocol dependencies
    set(HYDROGEN_PROTOCOL_DEPS "")
    list(APPEND HYDROGEN_PROTOCOL_DEPS "gRPC|grpc|https://github.com/grpc/grpc.git|v1.60.0")
    list(APPEND HYDROGEN_PROTOCOL_DEPS "cppzmq|cppzmq|https://github.com/zeromq/cppzmq.git|v4.10.0")
    list(APPEND HYDROGEN_PROTOCOL_DEPS "libzmq|zeromq|https://github.com/zeromq/libzmq.git|v4.3.5")
    list(APPEND HYDROGEN_PROTOCOL_DEPS "mosquitto|mosquitto|https://github.com/eclipse/mosquitto.git|v2.0.18")
    
    # Store in parent scope
    set(HYDROGEN_CORE_DEPS "${HYDROGEN_CORE_DEPS}" PARENT_SCOPE)
    set(HYDROGEN_WEB_DEPS "${HYDROGEN_WEB_DEPS}" PARENT_SCOPE)
    set(HYDROGEN_OPTIONAL_DEPS "${HYDROGEN_OPTIONAL_DEPS}" PARENT_SCOPE)
endfunction()

# =============================================================================
# Package Manager Specific Dependency Resolution
# =============================================================================

# Find dependency using vcpkg
function(hydrogen_find_with_vcpkg package_name package_config)
    list(GET package_config 0 cmake_name)
    list(GET package_config 1 vcpkg_name)
    
    if(vcpkg_name)
        message(STATUS "Hydrogen: Looking for ${cmake_name} via vcpkg (${vcpkg_name})")
        find_package(${cmake_name} CONFIG QUIET)
        
        if(${cmake_name}_FOUND)
            message(STATUS "Hydrogen: Found ${cmake_name} via vcpkg")
            set(${package_name}_FOUND TRUE PARENT_SCOPE)
            return()
        endif()
    endif()
    
    set(${package_name}_FOUND FALSE PARENT_SCOPE)
endfunction()

# Find dependency using Conan
function(hydrogen_find_with_conan package_name package_config)
    list(GET package_config 0 cmake_name)
    list(GET package_config 1 conan_name)
    
    if(conan_name)
        message(STATUS "Hydrogen: Looking for ${cmake_name} via Conan (${conan_name})")
        
        # Try to find the package
        find_package(${cmake_name} CONFIG QUIET)
        
        if(${cmake_name}_FOUND)
            message(STATUS "Hydrogen: Found ${cmake_name} via Conan")
            set(${package_name}_FOUND TRUE PARENT_SCOPE)
            return()
        endif()
        
        # For Conan, also try the conan-generated names
        string(TOLOWER ${conan_name} conan_lower)
        find_package(${conan_lower} CONFIG QUIET)
        
        if(${conan_lower}_FOUND)
            message(STATUS "Hydrogen: Found ${conan_lower} via Conan")
            set(${package_name}_FOUND TRUE PARENT_SCOPE)
            return()
        endif()
    endif()
    
    set(${package_name}_FOUND FALSE PARENT_SCOPE)
endfunction()

# Find dependency using FetchContent
function(hydrogen_find_with_fetchcontent package_name package_config)
    list(GET package_config 0 cmake_name)
    list(LENGTH package_config config_length)

    if(config_length GREATER 2)
        list(GET package_config 2 git_url)
        list(GET package_config 3 git_tag)

        if(git_url AND git_tag)
            message(STATUS "Hydrogen: Using FetchContent for ${cmake_name}")

            # Set up cache directory
            if(NOT DEFINED FETCHCONTENT_BASE_DIR)
                set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cmake-cache" CACHE PATH "Base directory for FetchContent downloads")
            endif()

            # Create cache directory if it doesn't exist
            if(NOT EXISTS "${FETCHCONTENT_BASE_DIR}")
                file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")
            endif()

            # Configure FetchContent
            string(TOLOWER ${cmake_name} package_lower)
            set(cache_dir "${FETCHCONTENT_BASE_DIR}/${package_lower}-${git_tag}")

            FetchContent_Declare(
                ${package_lower}
                GIT_REPOSITORY ${git_url}
                GIT_TAG ${git_tag}
                SOURCE_DIR ${cache_dir}
            )

            FetchContent_MakeAvailable(${package_lower})
            set(${package_name}_FOUND TRUE PARENT_SCOPE)
            return()
        endif()
    endif()

    set(${package_name}_FOUND FALSE PARENT_SCOPE)
endfunction()

# =============================================================================
# Unified Dependency Resolution
# =============================================================================

# Find a dependency using the best available method
function(hydrogen_find_dependency package_name)
    set(options REQUIRED QUIET)
    set(oneValueArgs VERSION)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Get dependency configuration
    hydrogen_define_dependencies()
    
    # Find the package configuration
    set(package_config "")
    foreach(dep_list ${HYDROGEN_CORE_DEPS} ${HYDROGEN_WEB_DEPS} ${HYDROGEN_OPTIONAL_DEPS} ${HYDROGEN_PROTOCOL_DEPS})
        string(REPLACE "|" ";" dep_config "${dep_list}")
        list(GET dep_config 0 dep_name)
        if(dep_name STREQUAL package_name)
            set(package_config "${dep_config}")
            break()
        endif()
    endforeach()
    
    if(NOT package_config)
        message(WARNING "Hydrogen: Unknown dependency: ${package_name}")
        set(${package_name}_FOUND FALSE PARENT_SCOPE)
        return()
    endif()
    
    # Try package managers in order of preference
    set(package_found FALSE)
    
    # Method 1: Try primary package manager
    if(HYDROGEN_PRIMARY_PACKAGE_MANAGER STREQUAL "vcpkg")
        hydrogen_find_with_vcpkg(${package_name} "${package_config}")
        if(${package_name}_FOUND)
            set(package_found TRUE)
        endif()
    elseif(HYDROGEN_PRIMARY_PACKAGE_MANAGER STREQUAL "conan")
        hydrogen_find_with_conan(${package_name} "${package_config}")
        if(${package_name}_FOUND)
            set(package_found TRUE)
        endif()
    endif()
    
    # Method 2: Try secondary package manager
    if(NOT package_found)
        if(HYDROGEN_VCPKG_AVAILABLE AND HYDROGEN_PRIMARY_PACKAGE_MANAGER STREQUAL "conan")
            hydrogen_find_with_vcpkg(${package_name} "${package_config}")
            if(${package_name}_FOUND)
                set(package_found TRUE)
            endif()
        elseif(HYDROGEN_CONAN_AVAILABLE AND HYDROGEN_PRIMARY_PACKAGE_MANAGER STREQUAL "vcpkg")
            hydrogen_find_with_conan(${package_name} "${package_config}")
            if(${package_name}_FOUND)
                set(package_found TRUE)
            endif()
        endif()
    endif()
    
    # Method 3: Try standard find_package
    if(NOT package_found)
        list(GET package_config 0 cmake_name)
        message(STATUS "Hydrogen: Trying standard find_package for ${cmake_name}")

        set(find_args ${cmake_name})
        if(ARG_VERSION)
            list(APPEND find_args ${ARG_VERSION})
        endif()
        if(ARG_COMPONENTS)
            list(APPEND find_args COMPONENTS ${ARG_COMPONENTS})
        endif()
        if(ARG_QUIET)
            list(APPEND find_args QUIET)
        endif()

        # Special handling for Boost to ensure components are found
        if(cmake_name STREQUAL "Boost" AND ARG_COMPONENTS)
            find_package(Boost REQUIRED COMPONENTS ${ARG_COMPONENTS})
        else()
            find_package(${find_args})
        endif()

        if(${cmake_name}_FOUND)
            set(${package_name}_FOUND TRUE PARENT_SCOPE)
            set(package_found TRUE)
        endif()
    endif()
    
    # Method 4: Use FetchContent fallback
    if(NOT package_found AND HYDROGEN_ALLOW_FETCHCONTENT)
        hydrogen_find_with_fetchcontent(${package_name} "${package_config}")
        if(${package_name}_FOUND)
            set(package_found TRUE)
        endif()
    endif()
    
    # Handle required packages
    if(NOT package_found AND ARG_REQUIRED)
        message(FATAL_ERROR "Hydrogen: Required dependency ${package_name} not found")
    endif()
    
    # Propagate result to parent scope
    set(${package_name}_FOUND ${package_found} PARENT_SCOPE)
    
    if(package_found)
        message(STATUS "Hydrogen: Successfully resolved dependency: ${package_name}")
    else()
        message(STATUS "Hydrogen: Could not resolve dependency: ${package_name}")
    endif()
endfunction()

# =============================================================================
# Convenience Functions
# =============================================================================

# Find all core dependencies
function(hydrogen_find_core_dependencies)
    message(STATUS "Hydrogen: Resolving core dependencies...")
    
    hydrogen_find_dependency(Boost REQUIRED COMPONENTS system filesystem thread)
    hydrogen_find_dependency(OpenSSL REQUIRED)
    hydrogen_find_dependency(nlohmann_json REQUIRED)
    hydrogen_find_dependency(Threads REQUIRED)
    
    message(STATUS "Hydrogen: Core dependencies resolved")
endfunction()

# Find web framework dependencies
function(hydrogen_find_web_dependencies)
    message(STATUS "Hydrogen: Resolving web framework dependencies...")

    hydrogen_find_dependency(asio REQUIRED)
    hydrogen_find_dependency(Crow REQUIRED)

    message(STATUS "Hydrogen: Web framework dependencies resolved")
endfunction()

# Find optional dependencies based on features
function(hydrogen_find_optional_dependencies)
    message(STATUS "Hydrogen: Resolving optional dependencies...")

    if(HYDROGEN_ENABLE_COMPRESSION)
        hydrogen_find_dependency(ZLIB)
    endif()

    if(HYDROGEN_BUILD_TESTS)
        hydrogen_find_dependency(GTest)
    endif()

    if(HYDROGEN_ENABLE_PYTHON_BINDINGS)
        hydrogen_find_dependency(pybind11)
    endif()

    if(HYDROGEN_BUILD_BENCHMARKS)
        hydrogen_find_dependency(benchmark)
    endif()

    message(STATUS "Hydrogen: Optional dependencies resolved")
endfunction()

# Find protocol dependencies based on features
function(hydrogen_find_protocol_dependencies)
    message(STATUS "Hydrogen: Resolving protocol dependencies...")

    if(HYDROGEN_HAS_GRPC_SUPPORT)
        hydrogen_find_dependency(gRPC)
    endif()

    if(HYDROGEN_HAS_ZEROMQ_SUPPORT)
        hydrogen_find_dependency(cppzmq)
        hydrogen_find_dependency(libzmq)
    endif()

    if(HYDROGEN_HAS_MQTT_SUPPORT)
        hydrogen_find_dependency(mosquitto)
    endif()

    message(STATUS "Hydrogen: Protocol dependencies resolved")
endfunction()

# Find all dependencies
function(hydrogen_find_all_dependencies)
    message(STATUS "Hydrogen: Starting dependency resolution...")

    hydrogen_find_core_dependencies()
    hydrogen_find_web_dependencies()
    hydrogen_find_optional_dependencies()
    hydrogen_find_protocol_dependencies()

    message(STATUS "Hydrogen: All dependencies resolved")
endfunction()
