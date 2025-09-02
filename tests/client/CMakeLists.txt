# Client Component Tests
# Tests for the refactored client components

# Include Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Test executable for refactored components
add_executable(test_refactored_components
    test_refactored_components.cpp
)

# Comprehensive test executable with extensive coverage
add_executable(test_comprehensive_coverage
    test_comprehensive_coverage.cpp
)

# Link libraries for basic tests
target_link_libraries(test_refactored_components
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        Hydrogen::Common
        Hydrogen::Client
        Hydrogen::Core
        test_utils
)

# Link libraries for comprehensive tests
target_link_libraries(test_comprehensive_coverage
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        Hydrogen::Common
        Hydrogen::Client
        Hydrogen::Core
        test_utils
)

# Include directories for basic tests
target_include_directories(test_refactored_components
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/client
        ${CMAKE_SOURCE_DIR}/src/client_component/include
        ${CMAKE_SOURCE_DIR}/src/core/include
)

# Include directories for comprehensive tests
target_include_directories(test_comprehensive_coverage
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/client
        ${CMAKE_SOURCE_DIR}/src/client_component/include
        ${CMAKE_SOURCE_DIR}/src/core/include
)

# Register tests with CTest
gtest_discover_tests(test_refactored_components
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

gtest_discover_tests(test_comprehensive_coverage
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Add to the client_tests target
add_custom_target(client_tests
    DEPENDS test_refactored_components test_comprehensive_coverage
    COMMENT "Building client component tests"
)

message(STATUS "Hydrogen: Client tests configured")
message(STATUS "  - Target: test_refactored_components (basic tests)")
message(STATUS "  - Target: test_comprehensive_coverage (comprehensive tests)")
message(STATUS "  - Framework: Google Test")
