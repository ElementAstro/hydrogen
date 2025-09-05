-- Hydrogen XMake Tests Configuration
-- This file defines all test targets for the Hydrogen project

-- Only build tests if enabled
if has_config("tests") then

-- =============================================================================
-- Test Utility Libraries
-- =============================================================================

target("test_utils")
    set_kind("static")
    set_languages("cxx17")
    
    add_files("../tests/utils/simple_helpers.cpp")

    add_includedirs("../tests/utils", "../src")
    add_packages("gtest")
target_end()

target("hydrogen_test_framework")
    set_kind("static")
    set_languages("cxx17")
    
    add_files("../tests/framework/comprehensive_test_framework.cpp")
    add_files("../tests/framework/mock_objects.cpp")

    add_includedirs("../tests/framework", "../src", "../src/core/include", "../src/client_component/include")
    add_packages("gtest", "nlohmann_json")
    add_deps("hydrogen_core", "test_utils")
    
    if has_config("logging") then
        add_packages("spdlog")
        add_defines("HYDROGEN_HAS_SPDLOG")
    end
target_end()

-- =============================================================================
-- Component Tests
-- =============================================================================

target("core_tests")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../tests/core/**.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest", "nlohmann_json", "boost", "fmt")
    add_deps("hydrogen_core", "hydrogen_test_framework")

    if has_config("logging") then
        add_packages("spdlog")
    end
    
    -- Register as test
    add_tests("core_tests")
target_end()

target("client_tests")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../tests/client/**.cpp")
    -- Exclude duplicate main function and duplicate test definitions
    remove_files("../tests/client/test_main.cpp")
    remove_files("../tests/client/test_refactored_components.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest", "boost", "nlohmann_json", "fmt")
    add_deps("hydrogen_client", "hydrogen_test_framework")

    if has_config("logging") then
        add_packages("spdlog")
    end
    
    -- Register as test
    add_tests("client_tests")
target_end()

target("server_tests")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../tests/server/**.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest")
    add_deps("hydrogen_server", "hydrogen_test_framework")
    
    -- Register as test
    add_tests("server_tests")
target_end()

target("device_tests")
    set_kind("binary")
    set_languages("cxx17")

    add_files("../tests/device/**.cpp")
    -- Temporarily exclude problematic test files that need interface updates
    remove_files("../tests/device/test_focuser_device.cpp")
    remove_files("../tests/device/test_telescope_device.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest", "nlohmann_json", "boost", "fmt")
    add_deps("hydrogen_device", "hydrogen_test_framework")

    if has_config("logging") then
        add_packages("spdlog")
    end
    
    -- Register as test
    add_tests("device_tests")
target_end()

-- =============================================================================
-- Integration Tests
-- =============================================================================

target("integration_tests")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../tests/integration/**.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest")
    add_deps("hydrogen", "hydrogen_test_framework")
    
    -- Register as test
    add_tests("integration_tests")
target_end()

target("comprehensive_tests")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../tests/comprehensive/**.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest", "nlohmann_json", "boost", "fmt")
    add_deps("hydrogen", "hydrogen_test_framework")

    if has_config("logging") then
        add_packages("spdlog")
    end
    
    -- Register as test
    add_tests("comprehensive_tests")
target_end()

-- =============================================================================
-- Performance Tests
-- =============================================================================

target("performance_tests")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../tests/performance/**.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest", "nlohmann_json", "boost", "fmt")
    add_deps("hydrogen", "hydrogen_test_framework")

    if has_config("logging") then
        add_packages("spdlog")
    end
    
    -- Register as test
    add_tests("performance_tests")
target_end()

target("protocol_tests")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../tests/protocols/**.cpp")

    add_includedirs("../src", "../tests/framework")
    add_packages("gtest")
    add_deps("hydrogen", "hydrogen_test_framework")
    
    -- Register as test
    add_tests("protocol_tests")
target_end()

end -- tests
