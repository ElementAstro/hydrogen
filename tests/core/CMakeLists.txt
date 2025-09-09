# Core component tests

# Simple working message tests
add_executable(core_simple_tests
    test_simple.cpp
)

target_link_libraries(core_simple_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
)

# Basic message functionality tests
add_executable(core_message_basic_tests
    test_message_basic.cpp
)

target_link_libraries(core_message_basic_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        test_utils
        hydrogen_core
)

# Mock STDIO communicator tests
add_executable(core_stdio_mock_tests
    test_stdio_communicator_mock.cpp
)

target_link_libraries(core_stdio_mock_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        hydrogen_core
        hydrogen_common
        nlohmann_json::nlohmann_json
)

# Device lifecycle tests
add_executable(core_device_lifecycle_tests
    device/test_device_lifecycle.cpp
)

target_link_libraries(core_device_lifecycle_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        hydrogen_core
        hydrogen_common
        nlohmann_json::nlohmann_json
)

# Set test properties for device lifecycle tests
set_target_properties(core_device_lifecycle_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories for device lifecycle tests
target_include_directories(core_device_lifecycle_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core/include
        ${CMAKE_SOURCE_DIR}/src/common/include
)

# Register tests with CTest
gtest_discover_tests(core_simple_tests)
gtest_discover_tests(core_message_basic_tests)
gtest_discover_tests(core_stdio_mock_tests)
gtest_discover_tests(core_device_lifecycle_tests)

# Set test properties for device lifecycle tests (after gtest_discover_tests)
# Note: Properties will be set after test discovery

# Create a target for all core tests
add_custom_target(core_tests
    DEPENDS
        core_simple_tests
        core_message_basic_tests
        core_stdio_mock_tests
        core_device_lifecycle_tests
    COMMENT "Building all core tests"
)

# Create a specific target for core device tests
add_custom_target(core_device_tests
    DEPENDS
        core_device_lifecycle_tests
    COMMENT "Building core device-specific tests"
)
