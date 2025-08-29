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
        astrocomm_core
)

# Register tests with CTest
gtest_discover_tests(core_simple_tests)
gtest_discover_tests(core_message_basic_tests)

# Create a target for all core tests
add_custom_target(core_tests
    DEPENDS
        core_simple_tests
        core_message_basic_tests
    COMMENT "Building all core tests"
)
