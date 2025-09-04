-- Hydrogen XMake Applications Configuration
-- This file defines all application targets for the Hydrogen project

-- Only build applications if examples are enabled
if has_config("examples") then

-- =============================================================================
-- Server Applications
-- =============================================================================

target("astro_server")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../src/apps/simple_server.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_core")
    add_packages("fmt")

    if has_config("logging") then
        add_packages("spdlog")
    end
    
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

-- =============================================================================
-- Client Applications
-- =============================================================================

target("astro_client")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../src/apps/client_app.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_client", "hydrogen_core")
    add_packages("fmt", "nlohmann_json", "boost")

    if has_config("logging") then
        add_packages("spdlog")
    end
    
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

-- =============================================================================
-- Device Applications
-- =============================================================================

target("astro_telescope")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../src/apps/telescope_device.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_device", "hydrogen_core")
    add_packages("fmt", "boost", "nlohmann_json")

    if has_config("logging") then
        add_packages("spdlog")
    end

    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

target("astro_camera")
    set_kind("binary")
    set_languages("cxx17")
    
    add_files("../src/apps/camera_device.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_device", "hydrogen_core")
    add_packages("fmt", "boost", "nlohmann_json")

    if has_config("logging") then
        add_packages("spdlog")
    end

    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

target("astro_focuser")
    set_kind("binary")
    set_languages("cxx17")

    add_files("../src/apps/focuser_device.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_device", "hydrogen_core")
    add_packages("fmt", "boost", "nlohmann_json")

    if has_config("logging") then
        add_packages("spdlog")
    end

    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

target("astro_guider")
    set_kind("binary")
    set_languages("cxx17")

    add_files("../src/apps/guider_device.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_device", "hydrogen_core")
    add_packages("fmt", "boost", "nlohmann_json")

    if has_config("logging") then
        add_packages("spdlog")
    end

    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

target("astro_rotator")
    set_kind("binary")
    set_languages("cxx17")

    add_files("../src/apps/rotator_device.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_device", "hydrogen_core")
    add_packages("fmt", "boost", "nlohmann_json")

    if has_config("logging") then
        add_packages("spdlog")
    end

    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

target("astro_solver")
    set_kind("binary")
    set_languages("cxx17")

    add_files("../src/apps/solver_device.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_device", "hydrogen_core")
    add_packages("fmt", "boost", "nlohmann_json")

    if has_config("logging") then
        add_packages("spdlog")
    end

    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

target("astro_switch")
    set_kind("binary")
    set_languages("cxx17")

    add_files("../src/apps/switch_device.cpp")

    add_includedirs("../src")
    add_deps("hydrogen_device", "hydrogen_core")
    add_packages("fmt", "boost", "nlohmann_json")

    if has_config("logging") then
        add_packages("spdlog")
    end

    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
target_end()

end -- examples
