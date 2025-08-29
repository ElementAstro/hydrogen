# HydrogenCompiler.cmake - Compiler and platform configuration utilities
# This module provides utilities for consistent compiler settings, platform-specific
# configurations, and modern CMake best practices across all targets.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_COMPILER_INCLUDED)
    return()
endif()
set(HYDROGEN_COMPILER_INCLUDED TRUE)

# =============================================================================
# Platform Detection
# =============================================================================

# Detect platform and architecture
function(hydrogen_detect_platform)
    # Operating System Detection
    if(WIN32)
        set(HYDROGEN_PLATFORM "Windows" PARENT_SCOPE)
        if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
            set(HYDROGEN_PLATFORM_VARIANT "UWP" PARENT_SCOPE)
        else()
            set(HYDROGEN_PLATFORM_VARIANT "Desktop" PARENT_SCOPE)
        endif()
    elseif(APPLE)
        set(HYDROGEN_PLATFORM "Apple" PARENT_SCOPE)
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(HYDROGEN_PLATFORM_VARIANT "macOS" PARENT_SCOPE)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
            set(HYDROGEN_PLATFORM_VARIANT "iOS" PARENT_SCOPE)
        else()
            set(HYDROGEN_PLATFORM_VARIANT "Unknown" PARENT_SCOPE)
        endif()
    elseif(UNIX)
        set(HYDROGEN_PLATFORM "Unix" PARENT_SCOPE)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(HYDROGEN_PLATFORM_VARIANT "Linux" PARENT_SCOPE)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
            set(HYDROGEN_PLATFORM_VARIANT "FreeBSD" PARENT_SCOPE)
        else()
            set(HYDROGEN_PLATFORM_VARIANT "Other" PARENT_SCOPE)
        endif()
    else()
        set(HYDROGEN_PLATFORM "Unknown" PARENT_SCOPE)
        set(HYDROGEN_PLATFORM_VARIANT "Unknown" PARENT_SCOPE)
    endif()
    
    # Architecture Detection
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(HYDROGEN_ARCHITECTURE "x64" PARENT_SCOPE)
    else()
        set(HYDROGEN_ARCHITECTURE "x86" PARENT_SCOPE)
    endif()
    
    # Processor Architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
        set(HYDROGEN_PROCESSOR "ARM64" PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(HYDROGEN_PROCESSOR "ARM" PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)")
        set(HYDROGEN_PROCESSOR "x64" PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i386|i686)")
        set(HYDROGEN_PROCESSOR "x86" PARENT_SCOPE)
    else()
        set(HYDROGEN_PROCESSOR "Unknown" PARENT_SCOPE)
    endif()
    
    message(STATUS "Hydrogen: Platform: ${HYDROGEN_PLATFORM} ${HYDROGEN_PLATFORM_VARIANT}")
    message(STATUS "Hydrogen: Architecture: ${HYDROGEN_ARCHITECTURE} (${HYDROGEN_PROCESSOR})")
endfunction()

# =============================================================================
# Compiler Detection and Configuration
# =============================================================================

# Detect compiler and version
function(hydrogen_detect_compiler)
    set(HYDROGEN_COMPILER_NAME "Unknown" PARENT_SCOPE)
    set(HYDROGEN_COMPILER_VERSION "Unknown" PARENT_SCOPE)
    
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(HYDROGEN_COMPILER_NAME "MSVC" PARENT_SCOPE)
        set(HYDROGEN_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}" PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(HYDROGEN_COMPILER_NAME "GCC" PARENT_SCOPE)
        set(HYDROGEN_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}" PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
            set(HYDROGEN_COMPILER_NAME "Apple Clang" PARENT_SCOPE)
        else()
            set(HYDROGEN_COMPILER_NAME "Clang" PARENT_SCOPE)
        endif()
        set(HYDROGEN_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}" PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
        set(HYDROGEN_COMPILER_NAME "Intel" PARENT_SCOPE)
        set(HYDROGEN_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}" PARENT_SCOPE)
    endif()
    
    message(STATUS "Hydrogen: Compiler: ${HYDROGEN_COMPILER_NAME} ${HYDROGEN_COMPILER_VERSION}")
endfunction()

# =============================================================================
# Compiler-Specific Flags
# =============================================================================

# Get compiler-specific warning flags
function(hydrogen_get_warning_flags output_var level)
    set(flags "")
    
    if(level STREQUAL "BASIC")
        if(MSVC)
            set(flags "/W3")
        else()
            set(flags "-Wall")
        endif()
    elseif(level STREQUAL "EXTRA")
        if(MSVC)
            set(flags "/W4")
        else()
            set(flags "-Wall -Wextra")
        endif()
    elseif(level STREQUAL "PEDANTIC")
        if(MSVC)
            set(flags "/W4 /permissive-")
        else()
            set(flags "-Wall -Wextra -Wpedantic")
        endif()
    elseif(level STREQUAL "ALL")
        if(MSVC)
            set(flags "/Wall")
        else()
            set(flags "-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion")
        endif()
    endif()
    
    set(${output_var} "${flags}" PARENT_SCOPE)
endfunction()

# Get compiler-specific optimization flags
function(hydrogen_get_optimization_flags output_var level)
    set(flags "")
    
    if(level STREQUAL "DEBUG")
        if(MSVC)
            set(flags "/Od /Zi")
        else()
            set(flags "-O0 -g")
        endif()
    elseif(level STREQUAL "RELEASE")
        if(MSVC)
            set(flags "/O2 /DNDEBUG")
        else()
            set(flags "-O3 -DNDEBUG")
        endif()
    elseif(level STREQUAL "SIZE")
        if(MSVC)
            set(flags "/O1 /DNDEBUG")
        else()
            set(flags "-Os -DNDEBUG")
        endif()
    elseif(level STREQUAL "RELWITHDEBINFO")
        if(MSVC)
            set(flags "/O2 /Zi /DNDEBUG")
        else()
            set(flags "-O2 -g -DNDEBUG")
        endif()
    endif()
    
    set(${output_var} "${flags}" PARENT_SCOPE)
endfunction()

# =============================================================================
# Platform-Specific Configuration
# =============================================================================

# Configure platform-specific settings for a target
function(hydrogen_configure_platform_target target_name)
    # Windows-specific configuration
    if(WIN32)
        # Set Windows version
        target_compile_definitions(${target_name} PRIVATE
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
        
        # Link Windows libraries
        if(HYDROGEN_PLATFORM_VARIANT STREQUAL "Desktop")
            target_link_libraries(${target_name} PRIVATE ws2_32 mswsock)
        endif()
        
        # Set runtime library
        if(MSVC)
            set_property(TARGET ${target_name} PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            )
        endif()
    endif()
    
    # Linux-specific configuration
    if(HYDROGEN_PLATFORM_VARIANT STREQUAL "Linux")
        # Link pthread
        find_package(Threads REQUIRED)
        target_link_libraries(${target_name} PRIVATE Threads::Threads)
        
        # Link dl for dynamic loading
        target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
    endif()
    
    # macOS-specific configuration
    if(HYDROGEN_PLATFORM_VARIANT STREQUAL "macOS")
        # Set deployment target
        if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
            set_target_properties(${target_name} PROPERTIES
                OSX_DEPLOYMENT_TARGET "10.15"
            )
        endif()
        
        # Link system frameworks
        target_link_libraries(${target_name} PRIVATE
            "-framework Foundation"
            "-framework CoreFoundation"
        )
    endif()
endfunction()

# =============================================================================
# Modern CMake Best Practices
# =============================================================================

# Apply modern CMake best practices to a target
function(hydrogen_apply_modern_cmake target_name)
    set(options INTERFACE_TARGET)
    set(oneValueArgs NAMESPACE EXPORT_NAME)
    set(multiValueArgs)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Set modern target properties
    if(NOT ARG_INTERFACE_TARGET)
        set_target_properties(${target_name} PROPERTIES
            CXX_STANDARD ${HYDROGEN_DEFAULT_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
        )
    endif()
    
    # Set export name if provided
    if(ARG_EXPORT_NAME)
        set_target_properties(${target_name} PROPERTIES
            EXPORT_NAME ${ARG_EXPORT_NAME}
        )
    endif()
    
    # Create namespace alias if provided
    if(ARG_NAMESPACE)
        if(ARG_EXPORT_NAME)
            set(alias_name "${ARG_NAMESPACE}::${ARG_EXPORT_NAME}")
        else()
            set(alias_name "${ARG_NAMESPACE}::${target_name}")
        endif()
        
        if(ARG_INTERFACE_TARGET)
            add_library(${alias_name} ALIAS ${target_name})
        else()
            get_target_property(target_type ${target_name} TYPE)
            if(target_type STREQUAL "EXECUTABLE")
                add_executable(${alias_name} ALIAS ${target_name})
            else()
                add_library(${alias_name} ALIAS ${target_name})
            endif()
        endif()
    endif()
    
    # Configure include directories with generator expressions
    if(NOT ARG_INTERFACE_TARGET)
        target_include_directories(${target_name} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )
    endif()
endfunction()

# =============================================================================
# Dependency Linking Helpers
# =============================================================================

# Link system dependencies based on platform
function(hydrogen_link_system_dependencies target_name)
    # Threading support
    find_package(Threads REQUIRED)
    target_link_libraries(${target_name} PRIVATE Threads::Threads)
    
    # Platform-specific system libraries
    if(WIN32)
        target_link_libraries(${target_name} PRIVATE ws2_32 mswsock)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
        # Link rt library for clock functions on older systems
        include(CheckLibraryExists)
        check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME)
        if(HAVE_CLOCK_GETTIME)
            target_link_libraries(${target_name} PRIVATE rt)
        endif()
    endif()
endfunction()

# =============================================================================
# Initialization
# =============================================================================

# Initialize compiler and platform detection
function(hydrogen_initialize_compiler)
    message(STATUS "Hydrogen: Initializing compiler and platform configuration...")
    
    # Detect platform and compiler
    hydrogen_detect_platform()
    hydrogen_detect_compiler()
    
    # Set global compile features
    set(CMAKE_CXX_STANDARD ${HYDROGEN_DEFAULT_CXX_STANDARD} PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)
    
    # Enable position independent code globally
    set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
    
    # Export compile commands for IDEs
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON PARENT_SCOPE)
    
    message(STATUS "Hydrogen: Compiler and platform configuration complete")
endfunction()

# Automatically initialize when included
hydrogen_initialize_compiler()
