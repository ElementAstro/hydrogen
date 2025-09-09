# TCP Communication Protocol CMake Configuration

# Define the TCP communicator library
set(TCP_COMMUNICATOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/enhanced_tcp_communicator.cpp
)

set(TCP_COMMUNICATOR_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/hydrogen/core/communication/protocols/tcp_communicator.h
)

# Create the TCP communicator library
add_library(hydrogen_tcp_communicator STATIC
    ${TCP_COMMUNICATOR_SOURCES}
    ${TCP_COMMUNICATOR_HEADERS}
)

# Set target properties
set_target_properties(hydrogen_tcp_communicator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(hydrogen_tcp_communicator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(hydrogen_tcp_communicator
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        hydrogen_performance_optimization
        hydrogen_device_communicator
    PRIVATE
        $<$<PLATFORM_ID:Windows>:ws2_32>
        $<$<PLATFORM_ID:Windows>:wsock32>
)

# Platform-specific configurations
if(WIN32)
    target_compile_definitions(hydrogen_tcp_communicator PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0601  # Windows 7 or later
    )
endif()

# Compiler-specific configurations
if(MSVC)
    target_compile_options(hydrogen_tcp_communicator PRIVATE
        /W4
        /WX-  # Don't treat warnings as errors for now
        /permissive-
    )
else()
    target_compile_options(hydrogen_tcp_communicator PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Export the target
set_property(TARGET hydrogen_tcp_communicator PROPERTY EXPORT_NAME TcpCommunicator)

# Add to the hydrogen_core library if it exists
if(TARGET hydrogen_core)
    target_link_libraries(hydrogen_core PUBLIC hydrogen_tcp_communicator)
endif()

# Install configuration
install(TARGETS hydrogen_tcp_communicator
    EXPORT HydrogenTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES ${TCP_COMMUNICATOR_HEADERS}
    DESTINATION include/hydrogen/core/communication/protocols
)

# Add example if building examples
if(HYDROGEN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Add tests if building tests
if(HYDROGEN_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Create a separate example target
if(HYDROGEN_BUILD_EXAMPLES)
    add_executable(tcp_communication_demo
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../examples/communication/tcp_communication_demo.cpp
    )
    
    target_link_libraries(tcp_communication_demo
        PRIVATE
            hydrogen_tcp_communicator
            nlohmann_json::nlohmann_json
            spdlog::spdlog
    )
    
    set_target_properties(tcp_communication_demo PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
    
    # Install example
    install(TARGETS tcp_communication_demo
        RUNTIME DESTINATION examples
    )
endif()

# Create test target
if(HYDROGEN_BUILD_TESTS)
    add_executable(test_tcp_communicator
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../tests/core/communication/test_tcp_communicator.cpp
    )
    
    target_link_libraries(test_tcp_communicator
        PRIVATE
            hydrogen_tcp_communicator
            gtest::gtest
            gtest::gtest_main
            gmock::gmock
            nlohmann_json::nlohmann_json
            spdlog::spdlog
    )
    
    set_target_properties(test_tcp_communicator PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    
    # Add to test suite
    add_test(NAME TcpCommunicatorTest COMMAND test_tcp_communicator)
    
    # Install test
    install(TARGETS test_tcp_communicator
        RUNTIME DESTINATION tests
    )
endif()

# Documentation
if(HYDROGEN_BUILD_DOCS)
    set(TCP_COMMUNICATOR_DOC_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/PERFORMANCE.md
        ${CMAKE_CURRENT_SOURCE_DIR}/EXAMPLES.md
    )
    
    install(FILES ${TCP_COMMUNICATOR_DOC_FILES}
        DESTINATION docs/communication/protocols
        OPTIONAL
    )
endif()

# Package configuration
set(TCP_COMMUNICATOR_VERSION_MAJOR 1)
set(TCP_COMMUNICATOR_VERSION_MINOR 0)
set(TCP_COMMUNICATOR_VERSION_PATCH 0)
set(TCP_COMMUNICATOR_VERSION 
    "${TCP_COMMUNICATOR_VERSION_MAJOR}.${TCP_COMMUNICATOR_VERSION_MINOR}.${TCP_COMMUNICATOR_VERSION_PATCH}")

# Generate version header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_communicator_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/tcp_communicator_version.h
    @ONLY
)

target_include_directories(hydrogen_tcp_communicator
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tcp_communicator_version.h
    DESTINATION include/hydrogen/core/communication/protocols
)

# Summary
message(STATUS "TCP Communicator Configuration:")
message(STATUS "  Version: ${TCP_COMMUNICATOR_VERSION}")
message(STATUS "  Sources: ${TCP_COMMUNICATOR_SOURCES}")
message(STATUS "  Headers: ${TCP_COMMUNICATOR_HEADERS}")
message(STATUS "  Build Examples: ${HYDROGEN_BUILD_EXAMPLES}")
message(STATUS "  Build Tests: ${HYDROGEN_BUILD_TESTS}")
message(STATUS "  Build Documentation: ${HYDROGEN_BUILD_DOCS}")

# Feature summary
include(FeatureSummary)
set_package_properties(nlohmann_json PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "JSON library for message serialization"
    PURPOSE "Required for TCP message serialization and deserialization"
)

set_package_properties(spdlog PROPERTIES
    TYPE REQUIRED
    DESCRIPTION "Fast C++ logging library"
    PURPOSE "Required for TCP communicator logging and debugging"
)

feature_summary(WHAT ALL)
