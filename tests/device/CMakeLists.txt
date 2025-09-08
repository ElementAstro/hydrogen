# Device Tests
# Tests for all device types including telescope, camera, focuser, guider, rotator, etc.

cmake_minimum_required(VERSION 3.10)

# Find required testing dependencies
find_package(GTest REQUIRED)
include(GoogleTest)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/device
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/common
)

# Device base tests
add_executable(test_device_base
    test_device_base.cpp
)

target_link_libraries(test_device_base
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Camera device tests
add_executable(test_camera_device
    test_camera_device.cpp
)

target_link_libraries(test_camera_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Telescope device tests
add_executable(test_telescope_device
    test_telescope_device.cpp
)

target_link_libraries(test_telescope_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Focuser device tests
add_executable(test_focuser_device
    test_focuser_device.cpp
)

target_link_libraries(test_focuser_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Filter wheel device tests
add_executable(test_filter_wheel_device
    test_filter_wheel_device.cpp
)

target_link_libraries(test_filter_wheel_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Note: Some device test files are not yet implemented
# Uncomment and add test files as they are created:
# - test_guider_device.cpp
# - test_rotator_device.cpp
# - test_switch_device.cpp
# - test_dome_device.cpp

# Device registry tests
add_executable(test_device_registry
    test_device_registry.cpp
)

target_link_libraries(test_device_registry
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Set C++ standard for all test targets
set(DEVICE_TEST_TARGETS
    test_device_base
    test_camera_device
    test_telescope_device
    test_focuser_device
    test_filter_wheel_device
    test_device_registry
)

foreach(target ${DEVICE_TEST_TARGETS})
    target_compile_features(${target} PRIVATE cxx_std_17)
    
    # Add compiler flags
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
    
    # Register tests with CTest
    gtest_discover_tests(${target}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        PROPERTIES
            LABELS "device"
            TIMEOUT 120
    )
endforeach()

# Create a target for all device tests
add_custom_target(device_tests
    DEPENDS ${DEVICE_TEST_TARGETS}
    COMMENT "Building all device tests"
)

# Note: Additional device test files not yet implemented
# Uncomment and add test files as they are created:
# - test_device_interfaces.cpp
# - test_device_behaviors.cpp
# - test_device_communication.cpp
