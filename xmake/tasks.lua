-- Hydrogen XMake Custom Tasks Configuration
-- This file defines custom tasks and utilities for the Hydrogen project

-- =============================================================================
-- Build Tasks
-- =============================================================================

task("clean-all")
    set_menu({
        usage = "xmake clean-all",
        description = "Clean all build artifacts including cache",
        options = {}
    })
    
    on_run(function ()
        os.exec("xmake clean")
        os.rmdir(".xmake")
        print("All build artifacts cleaned.")
    end)
task_end()

task("build-libs")
    set_menu({
        usage = "xmake build-libs",
        description = "Build only library targets",
        options = {}
    })
    
    on_run(function ()
        local libraries = {
            "hydrogen_common", "hydrogen_core", "hydrogen_server", 
            "hydrogen_client", "hydrogen_device", "hydrogen"
        }
        
        for _, lib in ipairs(libraries) do
            print("Building " .. lib .. "...")
            os.exec("xmake build " .. lib)
        end
    end)
task_end()

task("build-apps")
    set_menu({
        usage = "xmake build-apps",
        description = "Build only application targets",
        options = {}
    })
    
    on_run(function ()
        if has_config("examples") then
            local applications = {
                "astro_server", "astro_client", "astro_telescope", "astro_camera",
                "astro_focuser", "astro_guider", "astro_rotator", "astro_solver", "astro_switch"
            }
            
            for _, app in ipairs(applications) do
                print("Building " .. app .. "...")
                os.exec("xmake build " .. app)
            end
        else
            print("Examples are not enabled. Use 'xmake config --examples=y' to enable applications.")
        end
    end)
task_end()

-- =============================================================================
-- Development Tasks
-- =============================================================================

task("show-config")
    set_menu({
        usage = "xmake show-config",
        description = "Show current build configuration",
        options = {}
    })
    
    on_run(function ()
        print("=== Hydrogen Build Configuration ===")
        print("Project: Hydrogen " .. (get_config("version") or "1.0.0"))
        print("Mode: " .. (get_config("mode") or "release"))
        print("Platform: " .. (get_config("plat") or "unknown"))
        print("Architecture: " .. (get_config("arch") or "unknown"))
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
        print("==========================================")
    end)
task_end()

-- =============================================================================
-- Test Tasks
-- =============================================================================

task("test")
    set_menu({
        usage = "xmake test",
        description = "Run all tests",
        options = {}
    })
    
    on_run(function ()
        if has_config("tests") then
            local test_targets = {
                "core_tests", "client_tests", "server_tests", "device_tests",
                "integration_tests", "comprehensive_tests", "performance_tests", "protocol_tests"
            }
            
            for _, test in ipairs(test_targets) do
                print("Running " .. test .. "...")
                os.exec("xmake run " .. test)
            end
        else
            print("Tests are not enabled. Use 'xmake config --tests=y' to enable tests.")
        end
    end)
task_end()

-- =============================================================================
-- CI/CD Tasks
-- =============================================================================

task("ci-build")
    set_menu({
        usage = "xmake ci-build",
        description = "Build for CI/CD environment",
        options = {}
    })
    
    on_run(function ()
        print("Running CI build...")
        
        -- Configure for CI
        os.exec("xmake config --tests=y --examples=y --warnings_as_errors=y")
        
        -- Build all targets
        os.exec("xmake")
        
        -- Run tests
        if has_config("tests") then
            os.exec("xmake test")
        end
        
        print("CI build completed successfully")
    end)
task_end()

task("release")
    set_menu({
        usage = "xmake release",
        description = "Build release version",
        options = {}
    })
    
    on_run(function ()
        print("Building release version...")
        
        -- Configure for release
        os.exec("xmake config --mode=release --lto=y --tests=n --examples=y")
        
        -- Build all targets
        os.exec("xmake")
        
        print("Release build completed successfully")
    end)
task_end()
