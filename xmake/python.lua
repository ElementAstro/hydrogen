-- Hydrogen XMake Python Bindings Configuration
-- This file defines Python bindings for the Hydrogen project

-- Only build Python bindings if enabled
if has_config("python_bindings") then

target("pyhydrogen")
    set_kind("shared")
    set_languages("cxx17")
    
    -- Python binding sources
    add_files("src/python/bindings.cpp")
    add_files("src/python/py_dome.cpp")
    add_files("src/python/py_observing_conditions.cpp")
    add_files("src/python/py_error_handling.cpp")
    add_files("src/python/py_type_safety.cpp")
    add_files("src/python/py_documentation.cpp")
    add_files("src/python/py_camera.cpp")
    add_files("src/python/py_filter_wheel.cpp")
    add_files("src/python/py_focuser.cpp")
    add_files("src/python/py_guider.cpp")
    add_files("src/python/py_rotator.cpp")
    add_files("src/python/py_solver.cpp")
    add_files("src/python/py_switch.cpp")
    
    add_includedirs("src")
    add_packages("pybind11")
    add_deps("hydrogen_device", "hydrogen_client", "hydrogen_server", "hydrogen_core")
    
    -- Configure as Python module
    set_extension(".pyd")
    set_prefixname("")
    
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
    
    -- Install to Python site-packages (this would need platform-specific handling)
    after_build(function (target)
        print("Python module built: " .. target:targetfile())
        print("Note: Manual installation to Python site-packages may be required")
    end)
target_end()

end -- python_bindings
