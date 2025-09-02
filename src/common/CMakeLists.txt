# Hydrogen Common Library
# Shared utilities and message classes

cmake_minimum_required(VERSION 3.10)

# Create the common library
add_library(hydrogen_common STATIC
    message.cpp
    message_queue.cpp
    utils.cpp
    logger.cpp
    error_recovery.cpp
)

# Set target properties
set_target_properties(hydrogen_common PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(hydrogen_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find required dependencies
find_package(nlohmann_json REQUIRED)

# Find optional logging dependencies
find_package(spdlog QUIET)
find_package(fmt QUIET)

# Link dependencies
target_link_libraries(hydrogen_common
    PUBLIC
        nlohmann_json::nlohmann_json
)

# Link optional logging dependencies if available
if(spdlog_FOUND)
    target_link_libraries(hydrogen_common PUBLIC spdlog::spdlog)
    target_compile_definitions(hydrogen_common PUBLIC HYDROGEN_HAS_SPDLOG)
endif()

if(fmt_FOUND)
    target_link_libraries(hydrogen_common PUBLIC fmt::fmt)
    target_compile_definitions(hydrogen_common PUBLIC HYDROGEN_HAS_FMT)
endif()

# Create alias for modern CMake usage
add_library(Hydrogen::Common ALIAS hydrogen_common)

message(STATUS "Hydrogen: Common library configured")
message(STATUS "  - Target: hydrogen_common (Hydrogen::Common)")
message(STATUS "  - Type: Static library")
message(STATUS "  - Dependencies: nlohmann_json, spdlog, fmt")
