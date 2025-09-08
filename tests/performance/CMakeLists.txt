# Hydrogen Performance Tests
# Performance benchmarks and stress tests

cmake_minimum_required(VERSION 3.15)

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)

# Find required testing dependencies
find_package(GTest REQUIRED)
include(GoogleTest)

# Performance tests executable
add_executable(performance_tests
    test_performance_benchmarks.cpp
    placeholder_test.cpp
)

# Apply modern CMake configuration
hydrogen_apply_modern_cmake(performance_tests)

# Configure target with standard settings
hydrogen_configure_target_standard(performance_tests)

# Link dependencies
target_link_libraries(performance_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Hydrogen::Core
    Hydrogen::Server
    Hydrogen::Client
    Hydrogen::Device
    hydrogen_test_framework
)

# Link system dependencies
hydrogen_link_system_dependencies(performance_tests)

# Apply feature definitions
if(COMMAND hydrogen_apply_feature_definitions)
    hydrogen_apply_feature_definitions(performance_tests)
endif()

# Register tests with CTest (longer timeout for performance tests)
gtest_discover_tests(performance_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "performance;benchmark"
        TIMEOUT 300
)

message(STATUS "Hydrogen: Performance tests configured")
