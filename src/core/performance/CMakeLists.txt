# Performance Optimization Components CMake Configuration

cmake_minimum_required(VERSION 3.16)

# Define the performance optimization library
set(PERFORMANCE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/performance/connection_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/performance/tcp_connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/performance/message_batcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/performance/memory_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/performance/serialization_optimizer.cpp
)

set(PERFORMANCE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/core/performance/connection_pool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/core/performance/tcp_connection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/core/performance/message_batcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/core/performance/memory_pool.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/core/performance/serialization_optimizer.h
)

# Create the performance optimization library
add_library(hydrogen_performance STATIC
    ${PERFORMANCE_SOURCES}
    ${PERFORMANCE_HEADERS}
)

# Set target properties
set_target_properties(hydrogen_performance PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(hydrogen_performance
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Link dependencies
target_link_libraries(hydrogen_performance
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
    PRIVATE
        # Add other dependencies as needed
)

# Compiler-specific options
if(MSVC)
    target_compile_options(hydrogen_performance PRIVATE
        /W4
        /WX
        /permissive-
    )
else()
    target_compile_options(hydrogen_performance PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

# Enable threading support
find_package(Threads REQUIRED)
target_link_libraries(hydrogen_performance PRIVATE Threads::Threads)

# Optional: Enable SSL support if requested
if(HYDROGEN_ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(hydrogen_performance PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(hydrogen_performance PRIVATE HYDROGEN_ENABLE_SSL)
endif()

# Optional: Enable compression support if requested
if(HYDROGEN_ENABLE_COMPRESSION)
    find_package(ZLIB REQUIRED)
    target_link_libraries(hydrogen_performance PRIVATE ZLIB::ZLIB)
    target_compile_definitions(hydrogen_performance PRIVATE HYDROGEN_ENABLE_COMPRESSION)
endif()

# Install targets
install(TARGETS hydrogen_performance
    EXPORT HydrogenPerformanceTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/core/performance/
    DESTINATION include/hydrogen/core/performance
    FILES_MATCHING PATTERN "*.h"
)

# Export targets
install(EXPORT HydrogenPerformanceTargets
    FILE HydrogenPerformanceTargets.cmake
    NAMESPACE Hydrogen::
    DESTINATION lib/cmake/Hydrogen
)

# Create package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    HydrogenPerformanceConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/HydrogenPerformanceConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HydrogenPerformanceConfig.cmake
    INSTALL_DESTINATION lib/cmake/Hydrogen
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HydrogenPerformanceConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HydrogenPerformanceConfigVersion.cmake
    DESTINATION lib/cmake/Hydrogen
)

# Testing support
if(HYDROGEN_BUILD_TESTS)
    enable_testing()
    
    # Find Google Test
    find_package(GTest REQUIRED)
    
    # Create test executables
    add_executable(test_connection_pool
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/core/performance/test_connection_pool.cpp
    )
    
    add_executable(test_message_batcher
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/core/performance/test_message_batcher.cpp
    )
    
    add_executable(test_memory_pool
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/core/performance/test_memory_pool.cpp
    )
    
    # Set test properties
    set_target_properties(test_connection_pool test_message_batcher test_memory_pool PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Link test dependencies
    target_link_libraries(test_connection_pool
        PRIVATE
            hydrogen_performance
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
    )
    
    target_link_libraries(test_message_batcher
        PRIVATE
            hydrogen_performance
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
    )
    
    target_link_libraries(test_memory_pool
        PRIVATE
            hydrogen_performance
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
    )
    
    # Include test directories
    target_include_directories(test_connection_pool
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    
    target_include_directories(test_message_batcher
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    
    target_include_directories(test_memory_pool
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    
    # Register tests with CTest
    add_test(NAME ConnectionPoolTests COMMAND test_connection_pool)
    add_test(NAME MessageBatcherTests COMMAND test_message_batcher)
    add_test(NAME MemoryPoolTests COMMAND test_memory_pool)
    
    # Set test properties
    set_tests_properties(ConnectionPoolTests MessageBatcherTests MemoryPoolTests PROPERTIES
        TIMEOUT 60
        LABELS "unit;performance;core"
    )
endif()

# Examples support
if(HYDROGEN_BUILD_EXAMPLES)
    add_executable(performance_optimization_demo
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/performance/performance_optimization_demo.cpp
    )
    
    set_target_properties(performance_optimization_demo PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    target_link_libraries(performance_optimization_demo
        PRIVATE
            hydrogen_performance
            nlohmann_json::nlohmann_json
            spdlog::spdlog
    )
    
    target_include_directories(performance_optimization_demo
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    
    # Install example
    install(TARGETS performance_optimization_demo
        RUNTIME DESTINATION bin/examples
    )
endif()

# Documentation
if(HYDROGEN_BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
        
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        
        add_custom_target(performance_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation for performance components"
            VERBATIM
        )
    endif()
endif()

# Performance benchmarks
if(HYDROGEN_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    
    add_executable(benchmark_performance
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/performance_benchmark.cpp
    )
    
    target_link_libraries(benchmark_performance
        PRIVATE
            hydrogen_performance
            benchmark::benchmark
            benchmark::benchmark_main
    )
    
    target_include_directories(benchmark_performance
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
endif()

# Static analysis support
if(HYDROGEN_ENABLE_STATIC_ANALYSIS)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        set_target_properties(hydrogen_performance PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,readability-*,performance-*,modernize-*"
        )
    endif()
    
    find_program(CPPCHECK_EXE NAMES "cppcheck")
    if(CPPCHECK_EXE)
        add_custom_target(cppcheck_performance
            COMMAND ${CPPCHECK_EXE}
                --enable=warning,performance,portability,information,missingInclude
                --std=c++17
                --library=std.cfg
                --template="[{severity}][{id}] {message} {callstack} \(On {file}:{line}\)"
                --verbose
                --quiet
                ${PERFORMANCE_SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running cppcheck on performance components"
        )
    endif()
endif()

# Code coverage support
if(HYDROGEN_ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(hydrogen_performance PRIVATE --coverage)
        target_link_libraries(hydrogen_performance PRIVATE --coverage)
    endif()
endif()

# Print configuration summary
message(STATUS "Hydrogen Performance Components Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SSL Support: ${HYDROGEN_ENABLE_SSL}")
message(STATUS "  Compression Support: ${HYDROGEN_ENABLE_COMPRESSION}")
message(STATUS "  Tests: ${HYDROGEN_BUILD_TESTS}")
message(STATUS "  Examples: ${HYDROGEN_BUILD_EXAMPLES}")
message(STATUS "  Documentation: ${HYDROGEN_BUILD_DOCS}")
message(STATUS "  Benchmarks: ${HYDROGEN_BUILD_BENCHMARKS}")
message(STATUS "  Static Analysis: ${HYDROGEN_ENABLE_STATIC_ANALYSIS}")
message(STATUS "  Code Coverage: ${HYDROGEN_ENABLE_COVERAGE}")
