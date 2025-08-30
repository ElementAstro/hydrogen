#!/usr/bin/env python3
"""
Setup script for Hydrogen Python bindings

This script provides an easy way to build and install the Hydrogen Python
bindings with automatic ASCOM/INDI compatibility features.
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path

def run_command(cmd, cwd=None, check=True):
    """Run a command and return the result"""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, check=check, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)
    return result

def find_cmake():
    """Find CMake executable"""
    cmake_names = ['cmake', 'cmake3']
    for name in cmake_names:
        if shutil.which(name):
            return name
    raise RuntimeError("CMake not found. Please install CMake.")

def build_hydrogen_python():
    """Build Hydrogen Python bindings"""
    print("🚀 Building Hydrogen Python Bindings")
    print("=" * 50)
    
    # Get project root directory
    project_root = Path(__file__).parent.parent
    build_dir = project_root / "build"
    
    print(f"Project root: {project_root}")
    print(f"Build directory: {build_dir}")
    
    # Create build directory
    build_dir.mkdir(exist_ok=True)
    
    # Find CMake
    cmake = find_cmake()
    print(f"Using CMake: {cmake}")
    
    # Configure with Python bindings enabled
    print("\n📋 Configuring build...")
    configure_cmd = [
        cmake,
        "-DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON",
        "-DHYDROGEN_BUILD_EXAMPLES=ON",
        "-DHYDROGEN_BUILD_TESTS=ON",
        "-DCMAKE_BUILD_TYPE=Release",
        str(project_root)
    ]
    
    run_command(configure_cmd, cwd=build_dir)
    
    # Build the project
    print("\n🔨 Building project...")
    build_cmd = [cmake, "--build", ".", "--config", "Release", "--target", "pyhydrogen"]
    run_command(build_cmd, cwd=build_dir)
    
    # Find the built module
    module_patterns = [
        "pyhydrogen*.so",      # Linux
        "pyhydrogen*.pyd",     # Windows
        "pyhydrogen*.dylib"    # macOS
    ]
    
    module_file = None
    for pattern in module_patterns:
        matches = list(build_dir.glob(f"**/{pattern}"))
        if matches:
            module_file = matches[0]
            break
    
    if module_file:
        print(f"✅ Built Python module: {module_file}")
        return module_file
    else:
        raise RuntimeError("Failed to find built Python module")

def install_module(module_file):
    """Install the Python module"""
    print("\n📦 Installing Python module...")
    
    # Get Python site-packages directory
    import site
    site_packages = site.getsitepackages()[0]
    
    # Copy module to site-packages
    dest_file = Path(site_packages) / module_file.name
    shutil.copy2(module_file, dest_file)
    
    print(f"✅ Installed module to: {dest_file}")

def test_installation():
    """Test the installation"""
    print("\n🧪 Testing installation...")
    
    try:
        import pyhydrogen as hydrogen
        print("✅ Successfully imported pyhydrogen")
        
        # Test basic functionality
        hydrogen.set_log_level("info")
        print("✅ Basic functionality test passed")
        
        # Test compatibility system
        hydrogen.init_compatibility_system(
            enable_auto_discovery=False,
            enable_ascom=True,
            enable_indi=True
        )
        print("✅ Compatibility system initialization test passed")
        
        # Get statistics
        stats = hydrogen.get_compatibility_statistics()
        print(f"✅ Statistics test passed: {stats.total_devices} devices")
        
        # Shutdown
        hydrogen.shutdown_compatibility_system()
        print("✅ Shutdown test passed")
        
        print("🎉 All tests passed!")
        return True
        
    except Exception as e:
        print(f"❌ Test failed: {e}")
        return False

def show_usage_examples():
    """Show usage examples"""
    print("\n📚 Usage Examples")
    print("=" * 30)
    
    print("""
🔧 Basic Device Creation:
    import pyhydrogen as hydrogen
    
    # Initialize compatibility system
    hydrogen.init_compatibility_system()
    
    # Create devices with automatic ASCOM/INDI compatibility
    camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294")
    telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX")
    dome = hydrogen.create_compatible_dome("dome1", "Generic", "Dome")
    
🌤️ Weather Monitoring:
    weather = hydrogen.create_compatible_observing_conditions("weather1", "Boltwood", "Cloud Sensor")
    weather.refresh()
    
    temp = weather.get_temperature()
    humidity = weather.get_humidity()
    is_safe = weather.is_safe_for_observing()
    
🔄 Protocol Transparency:
    # Same device accessible through multiple protocols
    bridge = hydrogen.get_integration_manager().get_device_bridge("cam1")
    
    # Internal protocol
    bridge.set_property("coolerOn", True, hydrogen.ProtocolType.INTERNAL)
    
    # ASCOM protocol (automatically mapped)
    bridge.set_property("CoolerOn", True, hydrogen.ProtocolType.ASCOM)
    
    # INDI protocol (automatically mapped)
    bridge.set_property("CCD_COOLER", True, hydrogen.ProtocolType.INDI)

📊 System Monitoring:
    stats = hydrogen.get_compatibility_statistics()
    print(f"Total devices: {stats.total_devices}")
    print(f"ASCOM devices: {stats.ascom_devices}")
    print(f"INDI devices: {stats.indi_devices}")

🧹 Cleanup:
    hydrogen.shutdown_compatibility_system()
""")

def main():
    """Main function"""
    print("🌟 Hydrogen Python Bindings Setup")
    print("Building with automatic ASCOM/INDI compatibility")
    print()
    
    if len(sys.argv) > 1:
        command = sys.argv[1].lower()
    else:
        command = "build"
    
    try:
        if command in ["build", "install"]:
            # Build the module
            module_file = build_hydrogen_python()
            
            if command == "install":
                # Install the module
                install_module(module_file)
                
                # Test installation
                if test_installation():
                    show_usage_examples()
                else:
                    sys.exit(1)
            else:
                print(f"\n✅ Build completed successfully!")
                print(f"Module location: {module_file}")
                print(f"\nTo install, run: python setup.py install")
                
        elif command == "test":
            # Test existing installation
            if test_installation():
                print("✅ Installation test passed")
            else:
                sys.exit(1)
                
        elif command == "examples":
            show_usage_examples()
            
        elif command == "clean":
            # Clean build directory
            project_root = Path(__file__).parent.parent
            build_dir = project_root / "build"
            
            if build_dir.exists():
                shutil.rmtree(build_dir)
                print(f"✅ Cleaned build directory: {build_dir}")
            else:
                print("Build directory doesn't exist")
                
        else:
            print(f"Unknown command: {command}")
            print("Available commands:")
            print("  build    - Build the Python bindings")
            print("  install  - Build and install the Python bindings")
            print("  test     - Test existing installation")
            print("  examples - Show usage examples")
            print("  clean    - Clean build directory")
            sys.exit(1)
            
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
