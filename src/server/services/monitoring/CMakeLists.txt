# Device Monitoring Service CMake Configuration

cmake_minimum_required(VERSION 3.16)

# Define the monitoring service library
set(MONITORING_SERVICE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/device_monitoring_service_impl.cpp
)

set(MONITORING_SERVICE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/server/services/device_monitoring_service.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/server/services/monitoring_data_structures.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/services/device_monitoring_service_impl.h
)

# Create the monitoring service library
add_library(hydrogen_monitoring_service STATIC
    ${MONITORING_SERVICE_SOURCES}
    ${MONITORING_SERVICE_HEADERS}
)

# Set target properties
set_target_properties(hydrogen_monitoring_service PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(hydrogen_monitoring_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
)

# Link dependencies
target_link_libraries(hydrogen_monitoring_service
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
    PRIVATE
        # Add other dependencies as needed
)

# Compiler-specific options
if(MSVC)
    target_compile_options(hydrogen_monitoring_service PRIVATE
        /W4
        /WX
        /permissive-
    )
else()
    target_compile_options(hydrogen_monitoring_service PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
    )
endif()

# Enable threading support
find_package(Threads REQUIRED)
target_link_libraries(hydrogen_monitoring_service PRIVATE Threads::Threads)

# Optional: Enable SSL support if requested
if(HYDROGEN_ENABLE_SSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(hydrogen_monitoring_service PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(hydrogen_monitoring_service PRIVATE HYDROGEN_ENABLE_SSL)
endif()

# Optional: Enable compression support if requested
if(HYDROGEN_ENABLE_COMPRESSION)
    find_package(ZLIB REQUIRED)
    target_link_libraries(hydrogen_monitoring_service PRIVATE ZLIB::ZLIB)
    target_compile_definitions(hydrogen_monitoring_service PRIVATE HYDROGEN_ENABLE_COMPRESSION)
endif()

# Install targets
install(TARGETS hydrogen_monitoring_service
    EXPORT HydrogenMonitoringTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/hydrogen/server/services/
    DESTINATION include/hydrogen/server/services
    FILES_MATCHING PATTERN "*.h"
)

# Export targets
install(EXPORT HydrogenMonitoringTargets
    FILE HydrogenMonitoringTargets.cmake
    NAMESPACE Hydrogen::
    DESTINATION lib/cmake/Hydrogen
)

# Create package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    HydrogenMonitoringConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/HydrogenMonitoringConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/HydrogenMonitoringConfig.cmake
    INSTALL_DESTINATION lib/cmake/Hydrogen
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/HydrogenMonitoringConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/HydrogenMonitoringConfigVersion.cmake
    DESTINATION lib/cmake/Hydrogen
)

# Testing support
if(HYDROGEN_BUILD_TESTS)
    enable_testing()
    
    # Find Google Test
    find_package(GTest REQUIRED)
    
    # Create test executable
    add_executable(test_monitoring_service
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../tests/server/services/test_device_monitoring_service.cpp
    )
    
    # Set test properties
    set_target_properties(test_monitoring_service PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Link test dependencies
    target_link_libraries(test_monitoring_service
        PRIVATE
            hydrogen_monitoring_service
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            GTest::gmock_main
    )
    
    # Include test directories
    target_include_directories(test_monitoring_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )
    
    # Register test with CTest
    add_test(NAME MonitoringServiceTests COMMAND test_monitoring_service)
    
    # Set test properties
    set_tests_properties(MonitoringServiceTests PROPERTIES
        TIMEOUT 60
        LABELS "unit;monitoring;server"
    )
endif()

# Examples support
if(HYDROGEN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Documentation
if(HYDROGEN_BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
        
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        
        add_custom_target(monitoring_docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation for monitoring service"
            VERBATIM
        )
    endif()
endif()

# Performance benchmarks
if(HYDROGEN_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    
    add_executable(benchmark_monitoring_service
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/monitoring_service_benchmark.cpp
    )
    
    target_link_libraries(benchmark_monitoring_service
        PRIVATE
            hydrogen_monitoring_service
            benchmark::benchmark
            benchmark::benchmark_main
    )
    
    target_include_directories(benchmark_monitoring_service
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
endif()

# Static analysis support
if(HYDROGEN_ENABLE_STATIC_ANALYSIS)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        set_target_properties(hydrogen_monitoring_service PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,readability-*,performance-*,modernize-*"
        )
    endif()
    
    find_program(CPPCHECK_EXE NAMES "cppcheck")
    if(CPPCHECK_EXE)
        add_custom_target(cppcheck_monitoring
            COMMAND ${CPPCHECK_EXE}
                --enable=warning,performance,portability,information,missingInclude
                --std=c++17
                --library=std.cfg
                --template="[{severity}][{id}] {message} {callstack} \(On {file}:{line}\)"
                --verbose
                --quiet
                ${MONITORING_SERVICE_SOURCES}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running cppcheck on monitoring service"
        )
    endif()
endif()

# Code coverage support
if(HYDROGEN_ENABLE_COVERAGE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(hydrogen_monitoring_service PRIVATE --coverage)
        target_link_libraries(hydrogen_monitoring_service PRIVATE --coverage)
    endif()
endif()

# Print configuration summary
message(STATUS "Hydrogen Monitoring Service Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SSL Support: ${HYDROGEN_ENABLE_SSL}")
message(STATUS "  Compression Support: ${HYDROGEN_ENABLE_COMPRESSION}")
message(STATUS "  Tests: ${HYDROGEN_BUILD_TESTS}")
message(STATUS "  Examples: ${HYDROGEN_BUILD_EXAMPLES}")
message(STATUS "  Documentation: ${HYDROGEN_BUILD_DOCS}")
message(STATUS "  Benchmarks: ${HYDROGEN_BUILD_BENCHMARKS}")
message(STATUS "  Static Analysis: ${HYDROGEN_ENABLE_STATIC_ANALYSIS}")
message(STATUS "  Code Coverage: ${HYDROGEN_ENABLE_COVERAGE}")
