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

# Guider device tests
add_executable(test_guider_device
    test_guider_device.cpp
)

target_link_libraries(test_guider_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Rotator device tests
add_executable(test_rotator_device
    test_rotator_device.cpp
)

target_link_libraries(test_rotator_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Switch device tests
add_executable(test_switch_device
    test_switch_device.cpp
)

target_link_libraries(test_switch_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

# Dome device tests
add_executable(test_dome_device
    test_dome_device.cpp
)

target_link_libraries(test_dome_device
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

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
    test_guider_device
    test_rotator_device
    test_switch_device
    test_dome_device
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

# Optional: Add device interface tests
add_executable(test_device_interfaces
    test_device_interfaces.cpp
)

target_link_libraries(test_device_interfaces
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

target_compile_features(test_device_interfaces PRIVATE cxx_std_17)

gtest_discover_tests(test_device_interfaces
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "device"
        TIMEOUT 60
)

# Add device interfaces test to the main target
add_dependencies(device_tests test_device_interfaces)

# Optional: Add device behavior tests
add_executable(test_device_behaviors
    test_device_behaviors.cpp
)

target_link_libraries(test_device_behaviors
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

target_compile_features(test_device_behaviors PRIVATE cxx_std_17)

gtest_discover_tests(test_device_behaviors
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "device"
        TIMEOUT 60
)

# Add device behaviors test to the main target
add_dependencies(device_tests test_device_behaviors)

# Optional: Add device communication tests
add_executable(test_device_communication
    test_device_communication.cpp
)

target_link_libraries(test_device_communication
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        hydrogen_device
        hydrogen_common
        test_utils
)

target_compile_features(test_device_communication PRIVATE cxx_std_17)

gtest_discover_tests(test_device_communication
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        LABELS "device"
        TIMEOUT 60
)

# Add device communication test to the main target
add_dependencies(device_tests test_device_communication)
