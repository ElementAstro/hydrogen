# HydrogenUtils.cmake - Centralized utilities for the Hydrogen project build system
# This module provides utilities for dependency management, package manager detection,
# and build configuration following modern CMake best practices.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_UTILS_INCLUDED)
    return()
endif()
set(HYDROGEN_UTILS_INCLUDED TRUE)

# =============================================================================
# Package Manager Detection
# =============================================================================

# Detect available package managers and set global variables
function(hydrogen_detect_package_managers)
    # Initialize detection results
    set(HYDROGEN_VCPKG_AVAILABLE FALSE PARENT_SCOPE)
    set(HYDROGEN_CONAN_AVAILABLE FALSE PARENT_SCOPE)
    set(HYDROGEN_PACKAGE_MANAGER "none" PARENT_SCOPE)
    
    # Check for vcpkg
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
        set(HYDROGEN_VCPKG_AVAILABLE TRUE PARENT_SCOPE)
        set(HYDROGEN_PACKAGE_MANAGER "vcpkg" PARENT_SCOPE)
        message(STATUS "Hydrogen: vcpkg detected via toolchain file")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(HYDROGEN_VCPKG_AVAILABLE TRUE PARENT_SCOPE)
        if(HYDROGEN_PACKAGE_MANAGER STREQUAL "none")
            set(HYDROGEN_PACKAGE_MANAGER "vcpkg" PARENT_SCOPE)
        endif()
        message(STATUS "Hydrogen: vcpkg detected via VCPKG_ROOT environment variable")
    endif()
    
    # Check for conan
    if(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
        set(HYDROGEN_CONAN_AVAILABLE TRUE PARENT_SCOPE)
        if(HYDROGEN_PACKAGE_MANAGER STREQUAL "none")
            set(HYDROGEN_PACKAGE_MANAGER "conan" PARENT_SCOPE)
        endif()
        message(STATUS "Hydrogen: Conan detected via toolchain file")
    elseif(EXISTS "${CMAKE_BINARY_DIR}/conandeps.cmake")
        set(HYDROGEN_CONAN_AVAILABLE TRUE PARENT_SCOPE)
        if(HYDROGEN_PACKAGE_MANAGER STREQUAL "none")
            set(HYDROGEN_PACKAGE_MANAGER "conan" PARENT_SCOPE)
        endif()
        message(STATUS "Hydrogen: Conan detected via conandeps.cmake")
    endif()
    
    message(STATUS "Hydrogen: Primary package manager: ${HYDROGEN_PACKAGE_MANAGER}")
endfunction()

# =============================================================================
# Dependency Management
# =============================================================================

# Enhanced find_package wrapper with fallback support
function(hydrogen_find_package package_name)
    set(options REQUIRED QUIET)
    set(oneValueArgs VERSION FALLBACK_URL FALLBACK_TAG FALLBACK_DIR)
    set(multiValueArgs COMPONENTS CONFIG_NAMES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Store original arguments for fallback
    set(original_args ${ARGN})
    
    # Try to find package using standard find_package
    set(find_args ${package_name})
    if(ARG_VERSION)
        list(APPEND find_args ${ARG_VERSION})
    endif()
    if(ARG_COMPONENTS)
        list(APPEND find_args COMPONENTS ${ARG_COMPONENTS})
    endif()
    if(ARG_QUIET)
        list(APPEND find_args QUIET)
    endif()
    
    # Try CONFIG mode first, then MODULE mode
    find_package(${find_args} CONFIG QUIET)
    if(NOT ${package_name}_FOUND)
        find_package(${find_args} MODULE QUIET)
    endif()
    
    # If package not found and fallback is available, use FetchContent
    if(NOT ${package_name}_FOUND AND ARG_FALLBACK_URL)
        message(STATUS "Hydrogen: ${package_name} not found via package manager, using FetchContent fallback")
        
        include(FetchContent)
        
        # Set up cache directory if not already set
        if(NOT DEFINED FETCHCONTENT_BASE_DIR)
            set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cmake-cache" CACHE PATH "Base directory for FetchContent downloads")
        endif()
        
        # Configure FetchContent declaration
        set(fetch_args ${package_name})
        if(ARG_FALLBACK_URL)
            list(APPEND fetch_args GIT_REPOSITORY ${ARG_FALLBACK_URL})
        endif()
        if(ARG_FALLBACK_TAG)
            list(APPEND fetch_args GIT_TAG ${ARG_FALLBACK_TAG})
        endif()
        if(ARG_FALLBACK_DIR)
            list(APPEND fetch_args SOURCE_DIR ${ARG_FALLBACK_DIR})
        endif()
        
        FetchContent_Declare(${fetch_args})
        FetchContent_MakeAvailable(${package_name})
        
        # Mark as found for parent scope
        set(${package_name}_FOUND TRUE PARENT_SCOPE)
    elseif(NOT ${package_name}_FOUND AND ARG_REQUIRED)
        message(FATAL_ERROR "Hydrogen: Required package ${package_name} not found and no fallback provided")
    endif()
    
    # Propagate found status to parent scope
    set(${package_name}_FOUND ${${package_name}_FOUND} PARENT_SCOPE)
endfunction()

# =============================================================================
# Target Configuration Utilities
# =============================================================================

# Configure a target with modern CMake best practices
function(hydrogen_configure_target target_name)
    set(options INTERFACE STATIC SHARED)
    set(oneValueArgs CXX_STANDARD EXPORT_NAME ALIAS_NAME)
    set(multiValueArgs PUBLIC_HEADERS PRIVATE_HEADERS SOURCES DEPENDENCIES PUBLIC_DEPENDENCIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Set C++ standard
    if(ARG_CXX_STANDARD)
        set_target_properties(${target_name} PROPERTIES
            CXX_STANDARD ${ARG_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )
    endif()
    
    # Set position independent code for static libraries
    if(ARG_STATIC OR (NOT ARG_INTERFACE AND NOT ARG_SHARED))
        set_target_properties(${target_name} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
        )
    endif()
    
    # Configure include directories
    if(ARG_PUBLIC_HEADERS)
        target_include_directories(${target_name}
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
        )
    endif()
    
    if(ARG_PRIVATE_HEADERS)
        target_include_directories(${target_name}
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
    endif()
    
    # Link dependencies
    if(ARG_PUBLIC_DEPENDENCIES)
        target_link_libraries(${target_name} PUBLIC ${ARG_PUBLIC_DEPENDENCIES})
    endif()
    
    if(ARG_DEPENDENCIES)
        target_link_libraries(${target_name} PRIVATE ${ARG_DEPENDENCIES})
    endif()
    
    # Set export name
    if(ARG_EXPORT_NAME)
        set_target_properties(${target_name} PROPERTIES EXPORT_NAME ${ARG_EXPORT_NAME})
    endif()
    
    # Create alias
    if(ARG_ALIAS_NAME)
        add_library(${ARG_ALIAS_NAME} ALIAS ${target_name})
    endif()
endfunction()

# =============================================================================
# Feature Management
# =============================================================================

# Define a feature with automatic dependency checking
function(hydrogen_define_feature feature_name description)
    set(options ENABLED_BY_DEFAULT)
    set(oneValueArgs)
    set(multiValueArgs REQUIRED_PACKAGES OPTIONAL_PACKAGES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create option
    if(ARG_ENABLED_BY_DEFAULT)
        option(HYDROGEN_ENABLE_${feature_name} "${description}" ON)
    else()
        option(HYDROGEN_ENABLE_${feature_name} "${description}" OFF)
    endif()
    
    # Check dependencies if feature is enabled
    if(HYDROGEN_ENABLE_${feature_name})
        set(feature_available TRUE)
        
        # Check required packages
        foreach(package ${ARG_REQUIRED_PACKAGES})
            find_package(${package} QUIET)
            if(NOT ${package}_FOUND)
                set(feature_available FALSE)
                message(WARNING "Hydrogen: Feature ${feature_name} disabled - required package ${package} not found")
                break()
            endif()
        endforeach()
        
        # Update feature status
        if(NOT feature_available)
            set(HYDROGEN_ENABLE_${feature_name} OFF CACHE BOOL "${description}" FORCE)
        endif()
    endif()
    
    # Set global variable for easy checking
    set(HYDROGEN_FEATURE_${feature_name} ${HYDROGEN_ENABLE_${feature_name}} PARENT_SCOPE)
    
    if(HYDROGEN_ENABLE_${feature_name})
        message(STATUS "Hydrogen: Feature ${feature_name} enabled")
    else()
        message(STATUS "Hydrogen: Feature ${feature_name} disabled")
    endif()
endfunction()

# =============================================================================
# Installation Utilities
# =============================================================================

# Install a target with proper export configuration
function(hydrogen_install_target target_name export_name)
    set(options)
    set(oneValueArgs NAMESPACE)
    set(multiValueArgs HEADERS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    
    # Install target
    install(TARGETS ${target_name}
        EXPORT ${export_name}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    
    # Install headers
    if(ARG_HEADERS)
        foreach(header_path ${ARG_HEADERS})
            if(IS_DIRECTORY "${header_path}")
                install(DIRECTORY "${header_path}/"
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.hxx"
                )
            else()
                install(FILES "${header_path}"
                    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                )
            endif()
        endforeach()
    endif()
    
    # Install export targets
    install(EXPORT ${export_name}Targets
        FILE ${export_name}Targets.cmake
        NAMESPACE ${ARG_NAMESPACE}
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${export_name}
    )
endfunction()

# =============================================================================
# Initialization
# =============================================================================

# Initialize the Hydrogen build system
function(hydrogen_initialize)
    message(STATUS "Hydrogen: Initializing build system")
    
    # Detect package managers
    hydrogen_detect_package_managers()
    
    # Set up common directories
    set(HYDROGEN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" PARENT_SCOPE)
    set(HYDROGEN_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
    
    # Include additional modules
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    
    message(STATUS "Hydrogen: Build system initialized")
endfunction()
