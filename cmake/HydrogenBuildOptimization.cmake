# HydrogenBuildOptimization.cmake - Advanced build optimization system
# This module provides comprehensive build optimizations for performance,
# memory usage, and compilation speed.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_BUILD_OPTIMIZATION_INCLUDED)
    return()
endif()
set(HYDROGEN_BUILD_OPTIMIZATION_INCLUDED TRUE)

# =============================================================================
# Build Performance Optimization
# =============================================================================

# Configure parallel compilation
function(hydrogen_configure_parallel_build)
    # Detect number of CPU cores
    cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    
    if(NOT CPU_CORES)
        set(CPU_CORES 4) # Fallback
    endif()
    
    # Use 75% of available cores for parallel builds
    math(EXPR PARALLEL_JOBS "${CPU_CORES} * 3 / 4")
    if(PARALLEL_JOBS LESS 1)
        set(PARALLEL_JOBS 1)
    endif()
    
    # Set parallel build properties
    set_property(GLOBAL PROPERTY JOB_POOLS compile=${PARALLEL_JOBS} link=2)
    set(CMAKE_JOB_POOL_COMPILE compile PARENT_SCOPE)
    set(CMAKE_JOB_POOL_LINK link PARENT_SCOPE)
    
    message(STATUS "Hydrogen: Configured for ${PARALLEL_JOBS} parallel compilation jobs (${CPU_CORES} cores detected)")
endfunction()

# Configure compiler-specific optimizations
function(hydrogen_configure_compiler_optimizations)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        hydrogen_configure_gcc_optimizations()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        hydrogen_configure_clang_optimizations()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        hydrogen_configure_msvc_optimizations()
    endif()
endfunction()

# GCC-specific optimizations
function(hydrogen_configure_gcc_optimizations)
    # Enable fast compilation
    add_compile_options(-pipe)
    
    # Optimize for current architecture in Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-march=native -mtune=native)
    endif()
    
    # Enable link-time optimization if requested
    if(HYDROGEN_ENABLE_LTO)
        add_compile_options(-flto)
        add_link_options(-flto)
    endif()
    
    # Memory optimization for large builds
    add_compile_options(-fno-keep-inline-dllexport)
    
    # Faster debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-Og -g1) # Optimize for debugging, minimal debug info
    endif()
    
    message(STATUS "Hydrogen: Applied GCC-specific optimizations")
endfunction()

# Clang-specific optimizations
function(hydrogen_configure_clang_optimizations)
    # Enable fast compilation
    add_compile_options(-pipe)
    
    # Optimize for current architecture in Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-march=native -mtune=native)
    endif()
    
    # Enable link-time optimization if requested
    if(HYDROGEN_ENABLE_LTO)
        add_compile_options(-flto=thin)
        add_link_options(-flto=thin)
    endif()
    
    # Faster debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-Og -g1)
    endif()
    
    # Enable colored diagnostics
    add_compile_options(-fcolor-diagnostics)
    
    message(STATUS "Hydrogen: Applied Clang-specific optimizations")
endfunction()

# MSVC-specific optimizations
function(hydrogen_configure_msvc_optimizations)
    # Enable multi-processor compilation
    add_compile_options(/MP)
    
    # Optimize for speed in Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /Ob2 /Oi /Ot /Oy /GL)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif()
    
    # Faster debug builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /Zi)
    endif()
    
    # Reduce memory usage during compilation
    add_compile_options(/bigobj)
    
    # Enable function-level linking
    add_compile_options(/Gy)
    
    message(STATUS "Hydrogen: Applied MSVC-specific optimizations")
endfunction()

# =============================================================================
# Memory Optimization
# =============================================================================

# Configure memory-efficient compilation
function(hydrogen_configure_memory_optimization)
    # Limit memory usage during compilation
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Limit memory usage for GCC
        add_compile_options(--param=ggc-min-expand=20 --param=ggc-min-heapsize=32768)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Limit memory usage for Clang
        add_compile_options(-mllvm -polly-vectorizer=stripmine)
    endif()
    
    # Configure linker memory optimization
    if(UNIX AND NOT APPLE)
        add_link_options(-Wl,--no-keep-memory -Wl,--reduce-memory-overheads)
    endif()
    
    message(STATUS "Hydrogen: Applied memory optimization settings")
endfunction()

# =============================================================================
# Cache Optimization
# =============================================================================

# Configure ccache if available
function(hydrogen_configure_ccache)
    find_program(CCACHE_PROGRAM ccache)
    
    if(CCACHE_PROGRAM)
        # Configure ccache settings
        execute_process(COMMAND ${CCACHE_PROGRAM} --set-config=max_size=1G)
        execute_process(COMMAND ${CCACHE_PROGRAM} --set-config=compression=true)
        execute_process(COMMAND ${CCACHE_PROGRAM} --set-config=compression_level=6)
        execute_process(COMMAND ${CCACHE_PROGRAM} --set-config=sloppiness=pch_defines,time_macros)
        
        # Set compiler launcher
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)
        
        message(STATUS "Hydrogen: ccache enabled for faster rebuilds")
    else()
        message(STATUS "Hydrogen: ccache not found, compilation caching disabled")
    endif()
endfunction()

# Configure sccache if available (alternative to ccache)
function(hydrogen_configure_sccache)
    find_program(SCCACHE_PROGRAM sccache)
    
    if(SCCACHE_PROGRAM AND NOT CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} PARENT_SCOPE)
        set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM} PARENT_SCOPE)
        
        message(STATUS "Hydrogen: sccache enabled for faster rebuilds")
    endif()
endfunction()

# =============================================================================
# Precompiled Headers Optimization
# =============================================================================

# Configure precompiled headers for faster compilation
function(hydrogen_configure_precompiled_headers target_name)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        # Create precompiled header with common includes
        target_precompile_headers(${target_name} PRIVATE
            <memory>
            <string>
            <vector>
            <unordered_map>
            <functional>
            <chrono>
            <thread>
            <mutex>
            <atomic>
            <future>
            <iostream>
            <sstream>
            <fstream>
            <algorithm>
            <numeric>
            <random>
            <nlohmann/json.hpp>
        )
        
        message(STATUS "Hydrogen: Precompiled headers configured for ${target_name}")
    endif()
endfunction()

# =============================================================================
# Unity Build Optimization
# =============================================================================

# Configure unity builds for faster compilation
function(hydrogen_configure_unity_build target_name)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16" AND HYDROGEN_ENABLE_UNITY_BUILD)
        set_target_properties(${target_name} PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE 8
        )
        
        message(STATUS "Hydrogen: Unity build enabled for ${target_name}")
    endif()
endfunction()

# =============================================================================
# Link Time Optimization
# =============================================================================

# Configure Link Time Optimization
function(hydrogen_configure_lto target_name)
    if(HYDROGEN_ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
        include(CheckIPOSupported)
        check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
        
        if(lto_supported)
            set_target_properties(${target_name} PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION TRUE
            )
            message(STATUS "Hydrogen: LTO enabled for ${target_name}")
        else()
            message(WARNING "Hydrogen: LTO requested but not supported for ${target_name}: ${lto_error}")
        endif()
    endif()
endfunction()

# =============================================================================
# Target-Specific Optimizations
# =============================================================================

# Apply all optimizations to a target
function(hydrogen_optimize_target target_name)
    # Apply precompiled headers
    hydrogen_configure_precompiled_headers(${target_name})
    
    # Apply unity build if enabled
    hydrogen_configure_unity_build(${target_name})
    
    # Apply LTO if enabled
    hydrogen_configure_lto(${target_name})
    
    # Set target-specific compiler flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(${target_name} PRIVATE
            NDEBUG
            HYDROGEN_OPTIMIZED_BUILD
        )
    endif()
    
    # Enable position-independent code for shared libraries
    if(BUILD_SHARED_LIBS)
        set_target_properties(${target_name} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
        )
    endif()
    
    message(STATUS "Hydrogen: Applied optimizations to target ${target_name}")
endfunction()

# =============================================================================
# Build System Initialization
# =============================================================================

# Initialize all build optimizations
function(hydrogen_initialize_build_optimizations)
    message(STATUS "Hydrogen: Initializing build optimizations...")
    
    # Configure parallel builds
    hydrogen_configure_parallel_build()
    
    # Configure compiler optimizations
    hydrogen_configure_compiler_optimizations()
    
    # Configure memory optimization
    hydrogen_configure_memory_optimization()
    
    # Configure compilation caching
    hydrogen_configure_ccache()
    hydrogen_configure_sccache()
    
    # Set global optimization flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DHYDROGEN_RELEASE_BUILD" PARENT_SCOPE)
    endif()
    
    message(STATUS "Hydrogen: Build optimizations initialized")
endfunction()

# =============================================================================
# Build Performance Monitoring
# =============================================================================

# Monitor build performance
function(hydrogen_monitor_build_performance)
    # Record build start time
    string(TIMESTAMP BUILD_START_TIME "%s")
    set(HYDROGEN_BUILD_START_TIME ${BUILD_START_TIME} CACHE INTERNAL "Build start time")
    
    # Create build performance report
    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/BuildPerformanceReport.cmake.in
        ${CMAKE_BINARY_DIR}/BuildPerformanceReport.cmake
        @ONLY
    )
    
    # Add custom target for performance report
    add_custom_target(build-performance-report
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/BuildPerformanceReport.cmake
        COMMENT "Generating build performance report"
    )
endfunction()

# =============================================================================
# Optimization Options
# =============================================================================

# Define optimization options
option(HYDROGEN_ENABLE_UNITY_BUILD "Enable unity builds for faster compilation" OFF)
option(HYDROGEN_ENABLE_BUILD_OPTIMIZATIONS "Enable comprehensive build optimizations" ON)
option(HYDROGEN_ENABLE_CCACHE "Enable ccache for compilation caching" ON)
option(HYDROGEN_MONITOR_BUILD_PERFORMANCE "Monitor and report build performance" OFF)

# Apply optimizations if enabled
if(HYDROGEN_ENABLE_BUILD_OPTIMIZATIONS)
    hydrogen_initialize_build_optimizations()
endif()

if(HYDROGEN_MONITOR_BUILD_PERFORMANCE)
    hydrogen_monitor_build_performance()
endif()
