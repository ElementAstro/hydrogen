# HydrogenFeatures.cmake - Comprehensive feature toggle and conditional compilation system
# This module provides a centralized feature toggle system that allows selective
# compilation of components and features based on available dependencies and user preferences.

cmake_minimum_required(VERSION 3.15)

# Include guard
if(HYDROGEN_FEATURES_INCLUDED)
    return()
endif()
set(HYDROGEN_FEATURES_INCLUDED TRUE)

# =============================================================================
# Feature Definition Framework
# =============================================================================

# Define a feature with automatic dependency checking and configuration
function(hydrogen_define_feature feature_name)
    set(options ENABLED_BY_DEFAULT REQUIRED)
    set(oneValueArgs DESCRIPTION CATEGORY)
    set(multiValueArgs REQUIRED_PACKAGES OPTIONAL_PACKAGES CONFLICTS_WITH ENABLES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Set default description if not provided
    if(NOT ARG_DESCRIPTION)
        set(ARG_DESCRIPTION "Enable ${feature_name} feature")
    endif()
    
    # Set default category
    if(NOT ARG_CATEGORY)
        set(ARG_CATEGORY "General")
    endif()
    
    # Create the option
    if(ARG_ENABLED_BY_DEFAULT)
        option(HYDROGEN_ENABLE_${feature_name} "${ARG_DESCRIPTION}" ON)
    else()
        option(HYDROGEN_ENABLE_${feature_name} "${ARG_DESCRIPTION}" OFF)
    endif()
    
    # Store feature metadata
    set(HYDROGEN_FEATURE_${feature_name}_DESCRIPTION "${ARG_DESCRIPTION}" CACHE INTERNAL "Feature description")
    set(HYDROGEN_FEATURE_${feature_name}_CATEGORY "${ARG_CATEGORY}" CACHE INTERNAL "Feature category")
    set(HYDROGEN_FEATURE_${feature_name}_REQUIRED_PACKAGES "${ARG_REQUIRED_PACKAGES}" CACHE INTERNAL "Required packages")
    set(HYDROGEN_FEATURE_${feature_name}_OPTIONAL_PACKAGES "${ARG_OPTIONAL_PACKAGES}" CACHE INTERNAL "Optional packages")
    set(HYDROGEN_FEATURE_${feature_name}_CONFLICTS_WITH "${ARG_CONFLICTS_WITH}" CACHE INTERNAL "Conflicting features")
    set(HYDROGEN_FEATURE_${feature_name}_ENABLES "${ARG_ENABLES}" CACHE INTERNAL "Features enabled by this feature")
    
    # Add to global feature list
    get_property(feature_list GLOBAL PROPERTY HYDROGEN_ALL_FEATURES)
    if(NOT feature_list)
        set(feature_list "")
    endif()
    list(APPEND feature_list ${feature_name})
    set_property(GLOBAL PROPERTY HYDROGEN_ALL_FEATURES "${feature_list}")
    
    # Check if feature should be enabled
    if(HYDROGEN_ENABLE_${feature_name})
        hydrogen_validate_feature(${feature_name})
    endif()
endfunction()

# Validate a feature's dependencies and conflicts
function(hydrogen_validate_feature feature_name)
    set(feature_available TRUE)
    set(missing_packages "")
    
    # Get feature metadata
    get_property(required_packages CACHE HYDROGEN_FEATURE_${feature_name}_REQUIRED_PACKAGES PROPERTY VALUE)
    get_property(conflicts_with CACHE HYDROGEN_FEATURE_${feature_name}_CONFLICTS_WITH PROPERTY VALUE)
    
    # Check required packages
    foreach(package ${required_packages})
        if(NOT ${package}_FOUND AND NOT TARGET ${package})
            set(feature_available FALSE)
            list(APPEND missing_packages ${package})
        endif()
    endforeach()
    
    # Check for conflicts
    foreach(conflict ${conflicts_with})
        if(HYDROGEN_ENABLE_${conflict})
            set(feature_available FALSE)
            message(WARNING "Hydrogen: Feature ${feature_name} conflicts with enabled feature ${conflict}")
        endif()
    endforeach()
    
    # Update feature status
    if(NOT feature_available)
        set(HYDROGEN_ENABLE_${feature_name} OFF CACHE BOOL "${HYDROGEN_FEATURE_${feature_name}_DESCRIPTION}" FORCE)
        if(missing_packages)
            message(WARNING "Hydrogen: Feature ${feature_name} disabled - missing packages: ${missing_packages}")
        endif()
    else()
        # Enable dependent features
        get_property(enables CACHE HYDROGEN_FEATURE_${feature_name}_ENABLES PROPERTY VALUE)
        foreach(dependent ${enables})
            if(NOT HYDROGEN_ENABLE_${dependent})
                message(STATUS "Hydrogen: Enabling dependent feature ${dependent} (required by ${feature_name})")
                set(HYDROGEN_ENABLE_${dependent} ON CACHE BOOL "Auto-enabled by ${feature_name}" FORCE)
            endif()
        endforeach()
    endif()
    
    # Set global variable for easy checking
    set(HYDROGEN_HAS_${feature_name} ${HYDROGEN_ENABLE_${feature_name}} CACHE BOOL "Feature ${feature_name} available" FORCE)
    
    if(HYDROGEN_ENABLE_${feature_name})
        message(STATUS "Hydrogen: Feature ${feature_name} enabled")
    else()
        message(STATUS "Hydrogen: Feature ${feature_name} disabled")
    endif()
endfunction()

# =============================================================================
# Core Feature Definitions
# =============================================================================

# Define all Hydrogen features
function(hydrogen_define_all_features)
    message(STATUS "Hydrogen: Defining features...")
    
    # Core features
    hydrogen_define_feature(SSL
        DESCRIPTION "Enable SSL/TLS support for secure communications"
        CATEGORY "Security"
        REQUIRED_PACKAGES OpenSSL
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(COMPRESSION
        DESCRIPTION "Enable data compression support"
        CATEGORY "Performance"
        REQUIRED_PACKAGES ZLIB
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(LOGGING
        DESCRIPTION "Enable detailed logging support"
        CATEGORY "Debugging"
        OPTIONAL_PACKAGES spdlog
        ENABLED_BY_DEFAULT
    )
    
    # Development features
    hydrogen_define_feature(TESTING
        DESCRIPTION "Build unit tests and testing framework"
        CATEGORY "Development"
        REQUIRED_PACKAGES GTest
    )
    
    hydrogen_define_feature(BENCHMARKS
        DESCRIPTION "Build performance benchmarks"
        CATEGORY "Development"
        REQUIRED_PACKAGES benchmark
        CONFLICTS_WITH TESTING
    )
    
    hydrogen_define_feature(EXAMPLES
        DESCRIPTION "Build example applications"
        CATEGORY "Development"
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(DOCUMENTATION
        DESCRIPTION "Build API documentation"
        CATEGORY "Development"
        REQUIRED_PACKAGES Doxygen
    )
    
    # Language bindings
    hydrogen_define_feature(PYTHON_BINDINGS
        DESCRIPTION "Build Python language bindings"
        CATEGORY "Bindings"
        REQUIRED_PACKAGES pybind11 Python
    )
    
    # Advanced features
    hydrogen_define_feature(PROFILING
        DESCRIPTION "Enable profiling and performance analysis"
        CATEGORY "Performance"
        CONFLICTS_WITH TESTING
    )
    
    hydrogen_define_feature(WEBSOCKETS
        DESCRIPTION "Enable WebSocket communication support"
        CATEGORY "Networking"
        REQUIRED_PACKAGES Boost
        ENABLES SSL
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(HTTP_SERVER
        DESCRIPTION "Enable HTTP server functionality"
        CATEGORY "Networking"
        REQUIRED_PACKAGES Crow
        ENABLES SSL WEBSOCKETS
        ENABLED_BY_DEFAULT
    )

    hydrogen_define_feature(MQTT_SUPPORT
        DESCRIPTION "Enable MQTT broker/client support"
        CATEGORY "Networking"
        REQUIRED_PACKAGES Mongoose
        ENABLES SSL
        ENABLED_BY_DEFAULT
    )

    hydrogen_define_feature(GRPC_SUPPORT
        DESCRIPTION "Enable gRPC communication support"
        CATEGORY "Networking"
        REQUIRED_PACKAGES gRPC
        ENABLES SSL
        ENABLED_BY_DEFAULT
    )

    hydrogen_define_feature(ZEROMQ_SUPPORT
        DESCRIPTION "Enable ZeroMQ communication support"
        CATEGORY "Networking"
        REQUIRED_PACKAGES cppzmq libzmq
        ENABLED_BY_DEFAULT
    )
    
    # Device-specific features
    hydrogen_define_feature(TELESCOPE_SUPPORT
        DESCRIPTION "Enable telescope device support"
        CATEGORY "Devices"
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(CAMERA_SUPPORT
        DESCRIPTION "Enable camera device support"
        CATEGORY "Devices"
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(FOCUSER_SUPPORT
        DESCRIPTION "Enable focuser device support"
        CATEGORY "Devices"
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(ROTATOR_SUPPORT
        DESCRIPTION "Enable rotator device support"
        CATEGORY "Devices"
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(SOLVER_SUPPORT
        DESCRIPTION "Enable plate solver support"
        CATEGORY "Devices"
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(SWITCH_SUPPORT
        DESCRIPTION "Enable switch device support"
        CATEGORY "Devices"
        ENABLED_BY_DEFAULT
    )
    
    hydrogen_define_feature(GUIDER_SUPPORT
        DESCRIPTION "Enable guider device support"
        CATEGORY "Devices"
        ENABLED_BY_DEFAULT
    )
    
    message(STATUS "Hydrogen: Feature definitions complete")
endfunction()

# =============================================================================
# Feature Configuration
# =============================================================================

# Configure all features based on current settings
function(hydrogen_configure_all_features)
    message(STATUS "Hydrogen: Configuring features...")
    
    # Get all defined features
    get_property(all_features GLOBAL PROPERTY HYDROGEN_ALL_FEATURES)
    
    # Validate each enabled feature
    foreach(feature ${all_features})
        if(HYDROGEN_ENABLE_${feature})
            hydrogen_validate_feature(${feature})
        endif()
    endforeach()
    
    message(STATUS "Hydrogen: Feature configuration complete")
endfunction()

# =============================================================================
# Feature Reporting
# =============================================================================

# Generate a comprehensive feature report
function(hydrogen_generate_feature_report)
    message(STATUS "")
    message(STATUS "=== Hydrogen Feature Report ===")
    
    # Get all defined features
    get_property(all_features GLOBAL PROPERTY HYDROGEN_ALL_FEATURES)
    
    # Group features by category
    set(categories "")
    foreach(feature ${all_features})
        get_property(category CACHE HYDROGEN_FEATURE_${feature}_CATEGORY PROPERTY VALUE)
        list(APPEND categories ${category})
    endforeach()
    list(REMOVE_DUPLICATES categories)
    list(SORT categories)
    
    # Report by category
    foreach(category ${categories})
        message(STATUS "")
        message(STATUS "${category} Features:")
        foreach(feature ${all_features})
            get_property(feature_category CACHE HYDROGEN_FEATURE_${feature}_CATEGORY PROPERTY VALUE)
            if(feature_category STREQUAL category)
                get_property(description CACHE HYDROGEN_FEATURE_${feature}_DESCRIPTION PROPERTY VALUE)
                if(HYDROGEN_ENABLE_${feature})
                    message(STATUS "  ✓ ${feature}: ${description}")
                else()
                    message(STATUS "  ✗ ${feature}: ${description}")
                endif()
            endif()
        endforeach()
    endforeach()
    
    message(STATUS "")
    message(STATUS "===============================")
    message(STATUS "")
endfunction()

# =============================================================================
# Target Feature Application
# =============================================================================

# Apply feature-based compile definitions to a target
function(hydrogen_apply_feature_definitions target_name)
    # Get all defined features
    get_property(all_features GLOBAL PROPERTY HYDROGEN_ALL_FEATURES)
    
    # Apply definitions for enabled features
    foreach(feature ${all_features})
        if(HYDROGEN_ENABLE_${feature})
            target_compile_definitions(${target_name} PUBLIC HYDROGEN_HAS_${feature})
        endif()
    endforeach()
    
    # Apply legacy definitions for backward compatibility
    if(HYDROGEN_ENABLE_SSL)
        target_compile_definitions(${target_name} PUBLIC HYDROGEN_SSL_SUPPORT)
    endif()
    
    if(HYDROGEN_ENABLE_COMPRESSION)
        target_compile_definitions(${target_name} PUBLIC HYDROGEN_COMPRESSION_SUPPORT)
    endif()
    
    if(HYDROGEN_ENABLE_LOGGING)
        target_compile_definitions(${target_name} PUBLIC HYDROGEN_LOGGING_ENABLED)
    endif()
endfunction()

# =============================================================================
# Main Feature Management Function
# =============================================================================

# Initialize and configure the complete feature system
function(hydrogen_initialize_features)
    message(STATUS "Hydrogen: Initializing feature system...")
    
    # Define all features
    hydrogen_define_all_features()
    
    # Configure features based on dependencies
    hydrogen_configure_all_features()
    
    # Generate feature report
    hydrogen_generate_feature_report()
    
    message(STATUS "Hydrogen: Feature system initialized")
endfunction()
