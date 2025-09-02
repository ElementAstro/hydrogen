# HydrogenPackageManagers.cmake - Advanced package manager detection and configuration
# This module provides comprehensive package manager detection, configuration, and integration
# for vcpkg, Conan, and fallback mechanisms.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_PACKAGE_MANAGERS_INCLUDED)
    return()
endif()
set(HYDROGEN_PACKAGE_MANAGERS_INCLUDED TRUE)

# =============================================================================
# Global Package Manager State
# =============================================================================

# Initialize global variables
set(HYDROGEN_PACKAGE_MANAGERS_DETECTED FALSE CACHE INTERNAL "Package managers detection completed")
set(HYDROGEN_ACTIVE_PACKAGE_MANAGER "none" CACHE STRING "Currently active package manager")
set(HYDROGEN_VCPKG_ROOT "" CACHE PATH "vcpkg installation root")
set(HYDROGEN_CONAN_PROFILE "" CACHE STRING "Active Conan profile")

# =============================================================================
# vcpkg Detection and Configuration
# =============================================================================

function(hydrogen_detect_vcpkg)
    set(vcpkg_found FALSE)
    set(vcpkg_root "")
    set(vcpkg_toolchain "")
    
    # Method 1: Check CMAKE_TOOLCHAIN_FILE
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        get_filename_component(toolchain_name "${CMAKE_TOOLCHAIN_FILE}" NAME)
        if(toolchain_name STREQUAL "vcpkg.cmake")
            set(vcpkg_found TRUE)
            get_filename_component(vcpkg_root "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
            get_filename_component(vcpkg_root "${vcpkg_root}" DIRECTORY)
            get_filename_component(vcpkg_root "${vcpkg_root}" DIRECTORY)
            set(vcpkg_toolchain "${CMAKE_TOOLCHAIN_FILE}")
            message(STATUS "Hydrogen: vcpkg detected via CMAKE_TOOLCHAIN_FILE")
        endif()
    endif()
    
    # Method 2: Check VCPKG_ROOT environment variable
    if(NOT vcpkg_found AND DEFINED ENV{VCPKG_ROOT})
        set(env_vcpkg_root "$ENV{VCPKG_ROOT}")
        if(EXISTS "${env_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
            set(vcpkg_found TRUE)
            set(vcpkg_root "${env_vcpkg_root}")
            set(vcpkg_toolchain "${vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
            message(STATUS "Hydrogen: vcpkg detected via VCPKG_ROOT environment variable")
        endif()
    endif()
    
    # Method 3: Check for vcpkg.json in project root
    if(NOT vcpkg_found AND EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
        # Look for vcpkg in common locations
        set(common_vcpkg_paths
            "C:/vcpkg"
            "C:/tools/vcpkg"
            "C:/dev/vcpkg"
            "/usr/local/vcpkg"
            "/opt/vcpkg"
            "${CMAKE_SOURCE_DIR}/../vcpkg"
            "${CMAKE_SOURCE_DIR}/../../vcpkg"
        )
        
        foreach(path ${common_vcpkg_paths})
            if(EXISTS "${path}/scripts/buildsystems/vcpkg.cmake")
                set(vcpkg_found TRUE)
                set(vcpkg_root "${path}")
                set(vcpkg_toolchain "${path}/scripts/buildsystems/vcpkg.cmake")
                message(STATUS "Hydrogen: vcpkg detected at ${path} (vcpkg.json found)")
                break()
            endif()
        endforeach()
    endif()
    
    # Set global variables
    if(vcpkg_found)
        set(HYDROGEN_VCPKG_AVAILABLE TRUE PARENT_SCOPE)
        set(HYDROGEN_VCPKG_AVAILABLE TRUE CACHE BOOL "vcpkg is available" FORCE)
        set(HYDROGEN_VCPKG_ROOT "${vcpkg_root}" PARENT_SCOPE)
        set(HYDROGEN_VCPKG_ROOT "${vcpkg_root}" CACHE PATH "vcpkg root directory" FORCE)
        set(HYDROGEN_VCPKG_TOOLCHAIN "${vcpkg_toolchain}" PARENT_SCOPE)

        # Check for manifest mode
        if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
            set(HYDROGEN_VCPKG_MANIFEST_MODE TRUE PARENT_SCOPE)
            set(HYDROGEN_VCPKG_MANIFEST_MODE TRUE CACHE BOOL "vcpkg manifest mode" FORCE)
            message(STATUS "Hydrogen: vcpkg manifest mode detected")
        else()
            set(HYDROGEN_VCPKG_MANIFEST_MODE FALSE PARENT_SCOPE)
            set(HYDROGEN_VCPKG_MANIFEST_MODE FALSE CACHE BOOL "vcpkg manifest mode" FORCE)
        endif()
        
        # Detect triplet
        if(DEFINED VCPKG_TARGET_TRIPLET)
            set(HYDROGEN_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}" PARENT_SCOPE)
        else()
            # Auto-detect triplet based on platform
            if(WIN32)
                if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                    set(HYDROGEN_VCPKG_TRIPLET "x64-windows" PARENT_SCOPE)
                else()
                    set(HYDROGEN_VCPKG_TRIPLET "x86-windows" PARENT_SCOPE)
                endif()
            elseif(APPLE)
                set(HYDROGEN_VCPKG_TRIPLET "x64-osx" PARENT_SCOPE)
            else()
                set(HYDROGEN_VCPKG_TRIPLET "x64-linux" PARENT_SCOPE)
            endif()
        endif()
        
        message(STATUS "Hydrogen: vcpkg triplet: ${HYDROGEN_VCPKG_TRIPLET}")
    else()
        set(HYDROGEN_VCPKG_AVAILABLE FALSE PARENT_SCOPE)
        set(HYDROGEN_VCPKG_ROOT "" PARENT_SCOPE)
        set(HYDROGEN_VCPKG_TOOLCHAIN "" PARENT_SCOPE)
        set(HYDROGEN_VCPKG_MANIFEST_MODE FALSE PARENT_SCOPE)
    endif()
endfunction()

# =============================================================================
# Conan Detection and Configuration
# =============================================================================

function(hydrogen_detect_conan)
    set(conan_found FALSE)
    set(conan_toolchain "")
    set(conan_deps "")
    
    # Method 1: Check for conan_toolchain.cmake in build directory
    if(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
        set(conan_found TRUE)
        set(conan_toolchain "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
        message(STATUS "Hydrogen: Conan detected via conan_toolchain.cmake")
        
        # Include the toolchain if not already included
        if(NOT CONAN_TOOLCHAIN_INCLUDED)
            include("${conan_toolchain}")
            set(CONAN_TOOLCHAIN_INCLUDED TRUE CACHE INTERNAL "Conan toolchain included")
        endif()
    endif()
    
    # Method 2: Check for conandeps.cmake (legacy)
    if(NOT conan_found AND EXISTS "${CMAKE_BINARY_DIR}/conandeps.cmake")
        set(conan_found TRUE)
        set(conan_deps "${CMAKE_BINARY_DIR}/conandeps.cmake")
        message(STATUS "Hydrogen: Conan detected via conandeps.cmake (legacy mode)")
        
        # Include the deps file
        include("${conan_deps}")
    endif()
    
    # Method 3: Check for conanfile.txt or conanfile.py
    if(NOT conan_found)
        if(EXISTS "${CMAKE_SOURCE_DIR}/conanfile.txt" OR EXISTS "${CMAKE_SOURCE_DIR}/conanfile.py")
            # Check if conan command is available
            find_program(CONAN_COMMAND conan)
            if(CONAN_COMMAND)
                set(conan_found TRUE)
                message(STATUS "Hydrogen: Conan detected via conanfile (conan command available)")
                
                # Try to detect if conan install was run
                if(EXISTS "${CMAKE_BINARY_DIR}/conaninfo.txt" OR 
                   EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
                    message(STATUS "Hydrogen: Conan build files detected")
                endif()
            else()
                message(WARNING "Hydrogen: conanfile found but conan command not available")
            endif()
        endif()
    endif()
    
    # Set global variables
    if(conan_found)
        set(HYDROGEN_CONAN_AVAILABLE TRUE PARENT_SCOPE)
        set(HYDROGEN_CONAN_TOOLCHAIN "${conan_toolchain}" PARENT_SCOPE)
        set(HYDROGEN_CONAN_DEPS "${conan_deps}" PARENT_SCOPE)
        
        # Detect Conan version
        if(CONAN_COMMAND)
            execute_process(
                COMMAND ${CONAN_COMMAND} --version
                OUTPUT_VARIABLE conan_version_output
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            
            if(conan_version_output MATCHES "Conan version ([0-9]+\\.[0-9]+)")
                set(HYDROGEN_CONAN_VERSION "${CMAKE_MATCH_1}" PARENT_SCOPE)
                message(STATUS "Hydrogen: Conan version: ${CMAKE_MATCH_1}")
            endif()
        endif()
        
        # Check for profile
        if(EXISTS "${CMAKE_BINARY_DIR}/conaninfo.txt")
            file(READ "${CMAKE_BINARY_DIR}/conaninfo.txt" conaninfo_content)
            if(conaninfo_content MATCHES "\\[settings\\][^\\[]*arch=([^\n\r]+)")
                set(HYDROGEN_CONAN_ARCH "${CMAKE_MATCH_1}" PARENT_SCOPE)
            endif()
            if(conaninfo_content MATCHES "\\[settings\\][^\\[]*build_type=([^\n\r]+)")
                set(HYDROGEN_CONAN_BUILD_TYPE "${CMAKE_MATCH_1}" PARENT_SCOPE)
            endif()
        endif()
    else()
        set(HYDROGEN_CONAN_AVAILABLE FALSE PARENT_SCOPE)
        set(HYDROGEN_CONAN_TOOLCHAIN "" PARENT_SCOPE)
        set(HYDROGEN_CONAN_DEPS "" PARENT_SCOPE)
    endif()
endfunction()

# =============================================================================
# Package Manager Priority and Selection
# =============================================================================

function(hydrogen_select_primary_package_manager)
    set(primary_manager "none")

    # Check for HYDROGEN_DISABLE_VCPKG environment variable or CMake option
    if(DEFINED ENV{HYDROGEN_DISABLE_VCPKG} OR HYDROGEN_DISABLE_VCPKG)
        message(STATUS "Hydrogen: vcpkg disabled by user configuration")
        set(primary_manager "none")
    # Priority order: vcpkg (if manifest mode) > conan > vcpkg (classic) > none
    elseif(HYDROGEN_VCPKG_AVAILABLE AND HYDROGEN_VCPKG_MANIFEST_MODE)
        set(primary_manager "vcpkg")
        message(STATUS "Hydrogen: Selected vcpkg as primary package manager (manifest mode)")
    elseif(HYDROGEN_CONAN_AVAILABLE)
        set(primary_manager "conan")
        message(STATUS "Hydrogen: Selected Conan as primary package manager")
    elseif(HYDROGEN_VCPKG_AVAILABLE)
        set(primary_manager "vcpkg")
        message(STATUS "Hydrogen: Selected vcpkg as primary package manager (classic mode)")
    else()
        message(STATUS "Hydrogen: No package manager detected, using FetchContent fallback")
    endif()

    set(HYDROGEN_PRIMARY_PACKAGE_MANAGER "${primary_manager}" PARENT_SCOPE)
    set(HYDROGEN_PRIMARY_PACKAGE_MANAGER "${primary_manager}" CACHE STRING "Primary package manager" FORCE)
endfunction()

# =============================================================================
# Package Manager Configuration
# =============================================================================

function(hydrogen_configure_package_managers)
    message(STATUS "Hydrogen: Configuring package managers...")
    
    # Configure vcpkg if available
    if(HYDROGEN_VCPKG_AVAILABLE)
        # Set vcpkg feature flags
        set(VCPKG_FEATURE_FLAGS "versions,manifests" CACHE STRING "vcpkg feature flags")
        
        # Configure vcpkg overlay ports if they exist
        if(EXISTS "${CMAKE_SOURCE_DIR}/ports")
            set(VCPKG_OVERLAY_PORTS "${CMAKE_SOURCE_DIR}/ports" CACHE PATH "vcpkg overlay ports")
            message(STATUS "Hydrogen: vcpkg overlay ports configured: ${CMAKE_SOURCE_DIR}/ports")
        endif()
        
        # Configure vcpkg overlay triplets if they exist
        if(EXISTS "${CMAKE_SOURCE_DIR}/triplets")
            set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/triplets" CACHE PATH "vcpkg overlay triplets")
            message(STATUS "Hydrogen: vcpkg overlay triplets configured: ${CMAKE_SOURCE_DIR}/triplets")
        endif()
    endif()
    
    # Configure Conan if available
    if(HYDROGEN_CONAN_AVAILABLE)
        # Set Conan CMake integration variables
        set(CONAN_CMAKE_SILENT_OUTPUT TRUE CACHE BOOL "Silence Conan CMake output")
        
        # Configure Conan generators path
        if(EXISTS "${CMAKE_BINARY_DIR}/conan")
            list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/conan")
            message(STATUS "Hydrogen: Added Conan generators to CMAKE_PREFIX_PATH")
        endif()
    endif()
    
    message(STATUS "Hydrogen: Package manager configuration complete")
endfunction()

# =============================================================================
# Main Detection Function
# =============================================================================

function(hydrogen_detect_all_package_managers)
    if(HYDROGEN_PACKAGE_MANAGERS_DETECTED)
        return()
    endif()
    
    message(STATUS "Hydrogen: Detecting package managers...")
    
    # Detect individual package managers
    hydrogen_detect_vcpkg()
    hydrogen_detect_conan()
    
    # Select primary package manager
    hydrogen_select_primary_package_manager()
    
    # Configure package managers
    hydrogen_configure_package_managers()
    
    # Print summary
    message(STATUS "Hydrogen: Package Manager Detection Summary:")
    message(STATUS "  vcpkg available: ${HYDROGEN_VCPKG_AVAILABLE}")
    if(HYDROGEN_VCPKG_AVAILABLE)
        message(STATUS "    Root: ${HYDROGEN_VCPKG_ROOT}")
        message(STATUS "    Manifest mode: ${HYDROGEN_VCPKG_MANIFEST_MODE}")
        message(STATUS "    Triplet: ${HYDROGEN_VCPKG_TRIPLET}")
    endif()
    
    message(STATUS "  Conan available: ${HYDROGEN_CONAN_AVAILABLE}")
    if(HYDROGEN_CONAN_AVAILABLE AND HYDROGEN_CONAN_VERSION)
        message(STATUS "    Version: ${HYDROGEN_CONAN_VERSION}")
    endif()
    
    message(STATUS "  Primary package manager: ${HYDROGEN_PRIMARY_PACKAGE_MANAGER}")
    
    # Mark detection as complete
    set(HYDROGEN_PACKAGE_MANAGERS_DETECTED TRUE CACHE INTERNAL "Package managers detection completed")
endfunction()

# =============================================================================
# Automatic Detection on Include
# =============================================================================

# Automatically detect package managers when this module is included
hydrogen_detect_all_package_managers()
