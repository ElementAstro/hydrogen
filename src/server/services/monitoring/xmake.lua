-- Device Monitoring Service XMake Configuration

-- Add required packages
add_requires("nlohmann_json", "spdlog")

-- Optional packages based on build options
if has_config("ssl") then
    add_requires("openssl")
end

if has_config("compression") then
    add_requires("zlib")
end

if has_config("tests") then
    add_requires("gtest")
end

-- Define the monitoring service target
target("hydrogen_monitoring_service")
    set_kind("static")
    set_languages("cxx17")
    
    -- Source files
    add_files("../src/services/device_monitoring_service_impl.cpp")
    
    -- Header files
    add_headerfiles(
        "../include/hydrogen/server/services/device_monitoring_service.h",
        "../include/hydrogen/server/services/monitoring_data_structures.h",
        "../src/services/device_monitoring_service_impl.h"
    )
    
    -- Include directories
    add_includedirs(
        "../include",
        "../src",
        "../../core/include",
        {public = true}
    )
    
    -- Required packages
    add_packages("nlohmann_json", "spdlog")
    
    -- Optional SSL support
    if has_config("ssl") then
        add_packages("openssl")
        add_defines("HYDROGEN_ENABLE_SSL")
    end
    
    -- Optional compression support
    if has_config("compression") then
        add_packages("zlib")
        add_defines("HYDROGEN_ENABLE_COMPRESSION")
    end
    
    -- Compiler flags
    add_cxxflags("-Wall", "-Wextra", "-Werror", "-pedantic")
    
    -- MSVC specific flags
    if is_plat("windows") then
        add_cxxflags("/W4", "/WX", "/permissive-")
    end
    
    -- Threading support
    if is_plat("linux", "macosx") then
        add_syslinks("pthread")
    end
    
    -- Debug configuration
    if is_mode("debug") then
        add_defines("DEBUG")
        set_symbols("debug")
        set_optimize("none")
        
        -- Code coverage support
        if has_config("coverage") then
            add_cxxflags("--coverage")
            add_ldflags("--coverage")
        end
    end
    
    -- Release configuration
    if is_mode("release") then
        add_defines("NDEBUG")
        set_symbols("hidden")
        set_optimize("fastest")
        set_strip("all")
    end
    
    -- Install configuration
    on_install(function (target)
        -- Install headers
        os.cp(path.join(target:scriptdir(), "../include/hydrogen/server/services/*.h"), 
              path.join(target:installdir(), "include/hydrogen/server/services/"))
        
        -- Install library
        os.cp(target:targetfile(), path.join(target:installdir(), "lib/"))
    end)

-- Test target
if has_config("tests") then
    target("test_monitoring_service")
        set_kind("binary")
        set_languages("cxx17")
        set_default(false)
        
        -- Test source files
        add_files("../../../tests/server/services/test_device_monitoring_service.cpp")
        
        -- Include directories
        add_includedirs(
            "../include",
            "../src",
            "../../core/include"
        )
        
        -- Dependencies
        add_deps("hydrogen_monitoring_service")
        add_packages("gtest", "nlohmann_json", "spdlog")
        
        -- Test runner
        on_run(function (target)
            os.exec(target:targetfile())
        end)
        
        -- Test configuration
        after_build(function (target)
            print("Running monitoring service tests...")
            os.exec(target:targetfile())
        end)
end

-- Example target
if has_config("examples") then
    target("monitoring_service_example")
        set_kind("binary")
        set_languages("cxx17")
        set_default(false)
        
        -- Example source
        add_files("examples/monitoring_example.cpp")
        
        -- Include directories
        add_includedirs("../include")
        
        -- Dependencies
        add_deps("hydrogen_monitoring_service")
        add_packages("nlohmann_json", "spdlog")
        
        -- Install example
        on_install(function (target)
            os.cp(target:targetfile(), path.join(target:installdir(), "bin/examples/"))
        end)
end

-- Benchmark target
if has_config("benchmarks") then
    add_requires("benchmark")
    
    target("benchmark_monitoring_service")
        set_kind("binary")
        set_languages("cxx17")
        set_default(false)
        
        -- Benchmark source
        add_files("benchmarks/monitoring_service_benchmark.cpp")
        
        -- Include directories
        add_includedirs("../include")
        
        -- Dependencies
        add_deps("hydrogen_monitoring_service")
        add_packages("benchmark", "nlohmann_json", "spdlog")
        
        -- Benchmark runner
        on_run(function (target)
            os.exec(target:targetfile())
        end)
end

-- Documentation target
if has_config("docs") then
    target("monitoring_docs")
        set_kind("phony")
        set_default(false)
        
        on_build(function (target)
            -- Check if doxygen is available
            if os.iorunv("doxygen", {"--version"}) then
                print("Generating monitoring service documentation...")
                os.cd(target:scriptdir())
                os.exec("doxygen Doxyfile")
            else
                print("Doxygen not found, skipping documentation generation")
            end
        end)
end

-- Static analysis targets
if has_config("static_analysis") then
    target("clang_tidy_monitoring")
        set_kind("phony")
        set_default(false)
        
        on_build(function (target)
            local sourcefiles = {
                "../src/services/device_monitoring_service_impl.cpp"
            }
            
            for _, file in ipairs(sourcefiles) do
                print("Running clang-tidy on " .. file)
                os.execv("clang-tidy", {
                    file,
                    "-checks=-*,readability-*,performance-*,modernize-*",
                    "--",
                    "-std=c++17",
                    "-I../include",
                    "-I../src"
                })
            end
        end)
    
    target("cppcheck_monitoring")
        set_kind("phony")
        set_default(false)
        
        on_build(function (target)
            print("Running cppcheck on monitoring service...")
            os.execv("cppcheck", {
                "--enable=warning,performance,portability,information,missingInclude",
                "--std=c++17",
                "--library=std.cfg",
                "--template=[{severity}][{id}] {message} {callstack} (On {file}:{line})",
                "--verbose",
                "--quiet",
                "../src/services/device_monitoring_service_impl.cpp"
            })
        end)
end

-- Custom build rules
rule("monitoring_service_build")
    on_build_file(function (target, sourcefile)
        print("Building monitoring service file: " .. sourcefile)
    end)

-- Add custom build rule to target
target("hydrogen_monitoring_service")
    add_rules("monitoring_service_build")

-- Package configuration
package("hydrogen_monitoring_service")
    set_homepage("https://github.com/hydrogen-project/hydrogen")
    set_description("Hydrogen Device Monitoring Service")
    set_license("MIT")
    
    add_deps("nlohmann_json", "spdlog")
    
    on_install(function (package)
        local configs = {}
        
        if package:config("ssl") then
            table.insert(configs, "--ssl=y")
        end
        
        if package:config("compression") then
            table.insert(configs, "--compression=y")
        end
        
        if package:config("tests") then
            table.insert(configs, "--tests=y")
        end
        
        if package:config("examples") then
            table.insert(configs, "--examples=y")
        end
        
        import("package.tools.xmake").install(package, configs)
    end)

-- Configuration options
option("ssl")
    set_default(false)
    set_showmenu(true)
    set_description("Enable SSL support")

option("compression")
    set_default(false)
    set_showmenu(true)
    set_description("Enable compression support")

option("tests")
    set_default(false)
    set_showmenu(true)
    set_description("Build tests")

option("examples")
    set_default(false)
    set_showmenu(true)
    set_description("Build examples")

option("benchmarks")
    set_default(false)
    set_showmenu(true)
    set_description("Build benchmarks")

option("docs")
    set_default(false)
    set_showmenu(true)
    set_description("Generate documentation")

option("static_analysis")
    set_default(false)
    set_showmenu(true)
    set_description("Enable static analysis tools")

option("coverage")
    set_default(false)
    set_showmenu(true)
    set_description("Enable code coverage")

-- Build configuration summary
after_build(function (target)
    print("")
    print("Hydrogen Monitoring Service Build Summary:")
    print("  Target: " .. target:name())
    print("  Kind: " .. target:kind())
    print("  Language: " .. target:get("languages"))
    print("  SSL Support: " .. (has_config("ssl") and "enabled" or "disabled"))
    print("  Compression Support: " .. (has_config("compression") and "enabled" or "disabled"))
    print("  Tests: " .. (has_config("tests") and "enabled" or "disabled"))
    print("  Examples: " .. (has_config("examples") and "enabled" or "disabled"))
    print("  Documentation: " .. (has_config("docs") and "enabled" or "disabled"))
    print("  Static Analysis: " .. (has_config("static_analysis") and "enabled" or "disabled"))
    print("  Code Coverage: " .. (has_config("coverage") and "enabled" or "disabled"))
    print("")
end)
