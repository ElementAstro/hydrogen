-- Hydrogen XMake Libraries Configuration
-- This file defines all library targets for the Hydrogen project

-- =============================================================================
-- Common Library
-- =============================================================================

target("hydrogen_common")
    set_kind("static")
    set_languages("cxx17")
    
    -- Source files
    add_files("../src/common/message.cpp")
    add_files("../src/common/message_queue.cpp")
    add_files("../src/common/utils.cpp")
    add_files("../src/common/logger.cpp")
    add_files("../src/common/error_recovery.cpp")
    
    -- Include directories
    add_includedirs("../src", {public = true})

    -- Dependencies
    add_packages("nlohmann_json", "fmt")

    if has_config("logging") then
        add_packages("spdlog")
        add_defines("HYDROGEN_HAS_SPDLOG", {public = true})
    end

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end

    -- Export as Hydrogen::Common
    add_headerfiles("../src/common/*.h", {prefixdir = "Hydrogen/common"})
target_end()

-- =============================================================================
-- Core Library
-- =============================================================================

target("hydrogen_core")
    set_kind("static")
    set_languages("cxx17")
    
    -- Source files
    add_files("../src/core/common/core.cpp")
    add_files("../src/core/common/utils.cpp")
    add_files("../src/core/common/message.cpp")
    add_files("../src/core/common/message_queue.cpp")
    add_files("../src/core/common/error_recovery.cpp")
    add_files("../src/core/common/device_interface.cpp")
    add_files("../src/core/src/message_transformer.cpp")
    add_files("../src/core/src/protocol_converters.cpp")
    add_files("../src/core/src/message_validator.cpp")
    add_files("../src/core/src/grpc_streaming_client.cpp")
    add_files("../src/core/src/zmq_queue_client.cpp")
    add_files("../src/core/src/websocket_error_handler.cpp")
    add_files("../src/core/src/protocol_error_mapper.cpp")
    add_files("../src/core/src/unified_device_client.cpp")
    add_files("../src/core/src/unified_connection_manager.cpp")
    add_files("../src/core/src/client_configuration.cpp")
    add_files("../src/core/src/unified_websocket_error_handler.cpp")
    add_files("../src/common/logger.cpp")
    
    -- Include directories
    add_includedirs("../src", "../src/core/include", {public = true})

    -- Dependencies
    add_packages("nlohmann_json", "boost")
    add_deps("hydrogen_common")

    if has_config("logging") then
        add_packages("spdlog")
        add_defines("HYDROGEN_HAS_SPDLOG", {public = true})
    end

    -- Feature-specific configuration
    if has_config("ssl") then
        add_defines("HYDROGEN_HAS_SSL", {public = true})
    end

    if has_config("compression") then
        add_defines("HYDROGEN_HAS_COMPRESSION", {public = true})
    end

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end

    -- Export headers
    add_headerfiles("../src/core/include/**.h", {prefixdir = "Hydrogen/core"})
target_end()

-- =============================================================================
-- Server Library
-- =============================================================================

target("hydrogen_server")
    set_kind("static")
    set_languages("cxx17")
    
    -- Core infrastructure sources
    add_files("../src/server/src/core/protocol_handler.cpp")
    add_files("../src/server/src/core/service_registry.cpp")

    -- Service layer sources
    add_files("../src/server/src/services/device_service_impl.cpp")
    add_files("../src/server/src/services/auth_service_impl.cpp")
    add_files("../src/server/src/services/communication_service_impl.cpp")
    add_files("../src/server/src/services/health_service_impl.cpp")

    -- Repository layer sources
    add_files("../src/server/src/repositories/device_repository_impl.cpp")
    add_files("../src/server/src/repositories/user_repository_impl.cpp")
    add_files("../src/server/src/repositories/config_repository_impl.cpp")

    -- Infrastructure sources
    add_files("../src/server/src/infrastructure/config_manager_impl.cpp")
    add_files("../src/server/src/infrastructure/logging_impl.cpp")
    add_files("../src/server/src/infrastructure/error_handler_impl.cpp")

    -- Protocol implementation sources
    add_files("../src/server/src/protocols/http/http_server_impl.cpp")
    add_files("../src/server/src/protocols/grpc/grpc_server_impl.cpp")
    add_files("../src/server/src/protocols/mqtt/mqtt_broker_impl.cpp")
    add_files("../src/server/src/protocols/zmq/zmq_server_impl.cpp")

    -- Include directories
    add_includedirs("../src", "../src/server/include", {public = true})

    -- Dependencies
    add_packages("nlohmann_json", "boost", "openssl")
    add_deps("hydrogen_core", "hydrogen_common")

    if has_config("logging") then
        add_packages("spdlog")
        add_defines("HYDROGEN_HAS_SPDLOG", {public = true})
    end

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end

    -- Export headers
    add_headerfiles("../src/server/include/**.h", {prefixdir = "Hydrogen/server"})
target_end()

-- =============================================================================
-- Client Library
-- =============================================================================

target("hydrogen_client")
    set_kind("static")
    set_languages("cxx17")

    -- Source files from client_component (original files)
    add_files("../src/client_component/src/client.cpp")
    add_files("../src/client_component/src/device_client.cpp")

    -- Refactored client component files
    add_files("../src/client/connection_manager.cpp")
    add_files("../src/client/message_processor.cpp")
    add_files("../src/client/device_manager.cpp")
    add_files("../src/client/command_executor.cpp")
    add_files("../src/client/subscription_manager.cpp")
    add_files("../src/client/message_queue_manager.cpp")
    add_files("../src/client/device_client_refactored.cpp")

    -- Include directories
    add_includedirs("../src", "../src/client_component/include", {public = true})

    -- Dependencies
    add_packages("nlohmann_json", "boost")
    add_deps("hydrogen_core", "hydrogen_common")

    if has_config("logging") then
        add_packages("spdlog")
        add_defines("HYDROGEN_HAS_SPDLOG", {public = true})
    end

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end

    -- Export headers
    add_headerfiles("../src/client_component/include/**.h", {prefixdir = "Hydrogen/client"})
target_end()

-- =============================================================================
-- Device Library
-- =============================================================================

target("hydrogen_device")
    set_kind("static")
    set_languages("cxx17")
    
    -- Device implementation sources
    add_files("../src/device/device_base.cpp")
    add_files("../src/device/device_registry.cpp")
    add_files("../src/device/camera.cpp")
    add_files("../src/device/telescope.cpp")
    add_files("../src/device/focuser.cpp")
    add_files("../src/device/guider.cpp")
    add_files("../src/device/rotator.cpp")
    add_files("../src/device/solver.cpp")
    add_files("../src/device/switch.cpp")
    add_files("../src/device/dome.cpp")
    add_files("../src/device/filter_wheel.cpp")
    add_files("../src/device/cover_calibrator.cpp")
    add_files("../src/device/observing_conditions.cpp")
    add_files("../src/device/safety_monitor.cpp")

    -- Device core sources (modern device base and infrastructure)
    add_files("../src/device/core/modern_device_base.cpp")
    -- add_files("../src/device/core/device_manager.cpp")  -- Disabled due to massive syntax errors (100+ errors)
    -- add_files("../src/device/core/communication_manager.cpp")  -- Disabled due to missing members and macro conflicts
    -- add_files("../src/device/core/config_manager.cpp")  -- Disabled due to syntax errors
    -- add_files("../src/device/core/state_manager.cpp")  -- Disabled due to syntax errors
    -- add_files("../src/device/core/enhanced_device_base.cpp")  -- Disabled due to missing dependencies
    -- add_files("../src/device/core/async_operation.cpp")  -- Disabled due to template/method issues
    add_files("../src/device/core/multi_protocol_communication_manager.cpp")

    -- Device component sources
    add_files("../src/device_component/src/**.cpp")

    -- Device behavior sources
    -- add_files("../src/device/behaviors/device_behavior.cpp")  -- Temporarily disabled due to compilation issues
    -- add_files("../src/device/behaviors/temperature_control_behavior_stub.cpp")  -- Temporarily disabled due to compilation issues

    -- Include directories
    add_includedirs("../src", "../src/device_component/include", "../src/device/core", "../src/device/behaviors", {public = true})

    -- Dependencies
    add_packages("nlohmann_json", "boost")
    add_deps("hydrogen_core", "hydrogen_common")

    if has_config("logging") then
        add_packages("spdlog")
        add_defines("HYDROGEN_HAS_SPDLOG", {public = true})
    end

    -- Enable WebSocket support (Boost.Beast is included with Boost)
    add_defines("HYDROGEN_HAS_WEBSOCKETS", {public = true})

    -- Platform-specific settings
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end

    -- Export headers
    add_headerfiles("../src/device/**.h", {prefixdir = "Hydrogen/device"})
    add_headerfiles("../src/device_component/include/**.h", {prefixdir = "Hydrogen/device"})
target_end()

-- =============================================================================
-- Convenience Library
-- =============================================================================

target("hydrogen")
    set_kind("static")
    set_languages("cxx17")
    
    -- This is a convenience target that links all components
    add_deps("hydrogen_common", "hydrogen_core", "hydrogen_server", "hydrogen_client", "hydrogen_device")
    
    -- Include directories
    add_includedirs("../src", {public = true})
target_end()
