# Integration Tests
# Tests for component interactions and end-to-end functionality

# Include Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Integration test executable
add_executable(test_client_integration
    test_client_integration.cpp
)

# Link libraries
target_link_libraries(test_client_integration
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        AstroComm::Client
        AstroComm::Core
        test_utils
)

# Include directories
target_include_directories(test_client_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/client
        ${CMAKE_SOURCE_DIR}/src/client_component/include
        ${CMAKE_SOURCE_DIR}/src/core/include
)

# Register test with CTest
gtest_discover_tests(test_client_integration
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Add to the integration_tests target
add_custom_target(integration_tests
    DEPENDS test_client_integration
    COMMENT "Building integration tests"
)

message(STATUS "Hydrogen: Integration tests configured")
message(STATUS "  - Target: test_client_integration")
message(STATUS "  - Framework: Google Test")
