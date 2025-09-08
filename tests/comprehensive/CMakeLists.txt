# Hydrogen Comprehensive Tests
# End-to-end integration and system tests

cmake_minimum_required(VERSION 3.15)

# Include Hydrogen build system utilities
include(${CMAKE_SOURCE_DIR}/cmake/HydrogenUtils.cmake)

# Find required testing dependencies
find_package(GTest REQUIRED)
include(GoogleTest)

# Comprehensive tests executable
add_executable(comprehensive_tests
    placeholder_test.cpp
)

# Apply modern CMake configuration
hydrogen_apply_modern_cmake(comprehensive_tests)

# Configure target with standard settings
hydrogen_configure_target_standard(comprehensive_tests)

# Link dependencies
target_link_libraries(comprehensive_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Hydrogen::Core
    Hydrogen::Server
    Hydrogen::Client
    Hydrogen::Device
    hydrogen_test_framework
)

# Link system dependencies
hydrogen_link_system_dependencies(comprehensive_tests)

# Apply feature definitions
if(COMMAND hydrogen_apply_feature_definitions)
    hydrogen_apply_feature_definitions(comprehensive_tests)
endif()

# Register tests with CTest (longer timeout for comprehensive tests)
gtest_discover_tests(comprehensive_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "comprehensive;integration;system"
        TIMEOUT 600
)

message(STATUS "Hydrogen: Comprehensive tests configured")
