-- Hydrogen - Modern astronomical device communication protocol and framework
-- Simplified Modular XMake build configuration
-- Version: 1.0.0

-- Set minimum xmake version
set_xmakever("2.8.0")

-- Project definition
set_project("Hydrogen")
set_version("1.0.0")

-- Language and standards
set_languages("cxx17")
set_warnings("all")

-- Build modes
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.minsizerel")

-- Default build mode
if not is_mode("debug", "release", "releasedbg", "minsizerel") then
    set_defaultmode("release")
end

-- =============================================================================
-- Build Options (from xmake/options.lua)
-- =============================================================================

-- Core build options
option("shared")
    set_default(false)
    set_showmenu(true)
    set_description("Build shared libraries instead of static")
option_end()

option("tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build unit tests")
option_end()

option("examples")
    set_default(true)
    set_showmenu(true)
    set_description("Build example applications")
option_end()

option("python_bindings")
    set_default(false)
    set_showmenu(true)
    set_description("Build Python bindings")
option_end()

-- Feature options
option("ssl")
    set_default(true)
    set_showmenu(true)
    set_description("Enable SSL/TLS support")
option_end()

option("compression")
    set_default(true)
    set_showmenu(true)
    set_description("Enable compression support")
option_end()

option("logging")
    set_default(true)
    set_showmenu(true)
    set_description("Enable detailed logging")
option_end()

option("base64")
    set_default(false)
    set_showmenu(true)
    set_description("Enable base64 encoding support (legacy)")
option_end()

-- Development options
option("warnings_as_errors")
    set_default(false)
    set_showmenu(true)
    set_description("Treat warnings as errors")
option_end()

option("sanitizers")
    set_default(false)
    set_showmenu(true)
    set_description("Enable runtime sanitizers in Debug builds")
option_end()

option("lto")
    set_default(false)
    set_showmenu(true)
    set_description("Enable Link Time Optimization in Release builds")
option_end()

-- =============================================================================
-- Global Configuration (from xmake/config.lua)
-- =============================================================================

-- Set global compile features
add_cxxflags("-std=c++17")
set_policy("build.warning", true)

-- Platform-specific settings
if is_plat("windows") then
    add_defines("WIN32_LEAN_AND_MEAN", "_WIN32_WINNT=0x0601")
    add_syslinks("ws2_32")
elseif is_plat("linux") then
    add_syslinks("pthread", "dl")
elseif is_plat("macosx") then
    add_syslinks("pthread")
end

-- Build configuration specific settings
if is_mode("debug") then
    add_defines("DEBUG", "_DEBUG")
    set_symbols("debug")
    set_optimize("none")
    
    -- Enable sanitizers in debug mode if requested
    if has_config("sanitizers") then
        if is_plat("linux", "macosx") then
            add_cxxflags("-fsanitize=address,undefined", "-fno-omit-frame-pointer")
            add_ldflags("-fsanitize=address,undefined")
        end
    end
elseif is_mode("release") then
    add_defines("NDEBUG")
    set_symbols("hidden")
    set_optimize("fastest")
    
    -- Enable LTO if requested
    if has_config("lto") then
        set_policy("build.optimization.lto", true)
    end
end

-- Warning configuration
if has_config("warnings_as_errors") then
    set_warnings("all", "error")
else
    set_warnings("all")
end

-- =============================================================================
-- Dependencies (from xmake/dependencies.lua)
-- =============================================================================

-- Core dependencies
add_requires("nlohmann_json")
add_requires("fmt")

-- Optional dependencies
if has_config("logging") then
    add_requires("spdlog", {optional = true})
end

if has_config("ssl") then
    add_requires("openssl", {optional = true})
end

if has_config("compression") then
    add_requires("zlib", {optional = true})
end

if has_config("tests") then
    add_requires("gtest", {optional = true})
end

if has_config("python_bindings") then
    add_requires("pybind11", {optional = true})
end

-- Boost libraries
add_requires("boost", {optional = true, configs = {
    system = true,
    asio = true,
    algorithm = true,
    smart_ptr = true,
    type_traits = true,
    utility = true,
    shared = false
}})

-- =============================================================================
-- Include Modular Target Definitions
-- =============================================================================

-- Include library targets
includes("xmake/libraries.lua")

-- Include application targets  
includes("xmake/applications.lua")

-- Include test targets
includes("xmake/tests.lua")

-- Include Python bindings
includes("xmake/python.lua")

-- Include custom tasks
includes("xmake/tasks.lua")

-- =============================================================================
-- Build Summary
-- =============================================================================

-- Print build configuration summary
after_load(function ()
    print("")
    print("=== Hydrogen XMake Modular Build Configuration ===")
    print("Project: Hydrogen " .. (get_config("version") or "1.0.0"))
    print("Mode: " .. (get_config("mode") or "release"))
    print("Architecture: " .. (get_config("arch") or "unknown"))
    print("Platform: " .. (get_config("plat") or "unknown"))
    print("")
    print("Features:")
    print("  Examples: " .. tostring(has_config("examples")))
    print("  Tests: " .. tostring(has_config("tests")))
    print("  Python bindings: " .. tostring(has_config("python_bindings")))
    print("  SSL support: " .. tostring(has_config("ssl")))
    print("  Compression: " .. tostring(has_config("compression")))
    print("  Logging: " .. tostring(has_config("logging")))
    print("  Base64 support: " .. tostring(has_config("base64")))
    print("")
    print("Development:")
    print("  Warnings as errors: " .. tostring(has_config("warnings_as_errors")))
    print("  Sanitizers: " .. tostring(has_config("sanitizers")))
    print("  LTO: " .. tostring(has_config("lto")))
    print("  Shared libraries: " .. tostring(has_config("shared")))
    print("====================================================")
    print("")
end)
