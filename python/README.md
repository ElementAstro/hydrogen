# Hydrogen Python Bindings

## Overview

The Hydrogen Python bindings provide complete access to the Hydrogen astronomical device control system from Python, including the revolutionary **Automatic ASCOM/INDI Compatibility System**. This allows you to control astronomical devices through Python while automatically providing ASCOM and INDI protocol support without any additional code.

## 🚀 Key Features

- **Complete Device Support**: Camera, Telescope, Focuser, Dome, ObservingConditions, SafetyMonitor, CoverCalibrator
- **Automatic ASCOM/INDI Compatibility**: Zero-code protocol support for all devices
- **Transparent Protocol Access**: Same device accessible through multiple protocols simultaneously
- **Real-time Synchronization**: Property changes propagate across all protocols automatically
- **Thread-Safe Operations**: Full concurrent access support
- **Pythonic API**: Clean, intuitive Python interface

## 📦 Installation

### Prerequisites

- Python 3.7 or later
- CMake 3.15 or later
- C++17 compatible compiler
- pybind11 (automatically downloaded)

### Quick Installation

```bash
# Clone the repository
git clone https://github.com/ElementAstro/Hydrogen.git
cd Hydrogen

# Build and install Python bindings
python python/setup.py install
```

### Manual Build

```bash
# Create build directory
mkdir build && cd build

# Configure with Python bindings
cmake -DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON ..

# Build
cmake --build . --target pyhydrogen

# The module will be in the build directory
```

## 🔧 Quick Start

### Basic Usage

```python
import pyhydrogen as hydrogen

# Initialize the automatic compatibility system
hydrogen.init_compatibility_system()

# Create devices with automatic ASCOM/INDI compatibility
camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294MM Pro")
telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX-L")
focuser = hydrogen.create_compatible_focuser("foc1", "ZWO", "EAF")

# Use devices normally - ASCOM/INDI support is automatic!
camera.set_property("coolerOn", True)
camera.set_property("targetTemperature", -10.0)
camera.start_exposure(60.0)  # 60-second exposure

# Cleanup
hydrogen.shutdown_compatibility_system()
```

### Complete Observatory Example

```python
import pyhydrogen as hydrogen
import time

# Initialize system
hydrogen.init_compatibility_system()

# Create complete observatory
devices = {
    'camera': hydrogen.create_compatible_camera("cam1", "ZWO", "ASI6200MM"),
    'telescope': hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX-L"),
    'focuser': hydrogen.create_compatible_focuser("foc1", "ZWO", "EAF"),
    'dome': hydrogen.create_compatible_dome("dome1", "Technical Innovations", "ProDome"),
    'weather': hydrogen.create_compatible_observing_conditions("weather1", "Boltwood", "Cloud Sensor")
}

# Setup dome slaving
devices['dome'].slave_to_telescope(True)

# Setup weather monitoring
weather = devices['weather']
weather.set_safety_threshold(hydrogen.SensorType.WIND_SPEED, 0, 15)  # Max 15 m/s
weather.enable_safety_monitoring(True)

# Run observation sequence
def run_observation():
    # Check weather
    weather.refresh()
    if not weather.is_safe_for_observing():
        print("Weather unsafe for observing")
        return
    
    # Open observatory
    devices['dome'].open_shutter()
    devices['telescope'].unpark()
    
    # Setup imaging
    camera = devices['camera']
    camera.set_property("coolerOn", True)
    camera.set_property("targetTemperature", -10.0)
    
    # Take exposures
    for i in range(5):
        print(f"Taking exposure {i+1}/5...")
        camera.start_exposure(300.0)  # 5-minute exposure
        time.sleep(305)  # Wait for completion
        
        # Check weather between exposures
        weather.refresh()
        if not weather.is_safe_for_observing():
            print("Weather turned unsafe, stopping")
            break
    
    # Close observatory
    devices['dome'].close_shutter()
    devices['telescope'].park()
    camera.set_property("coolerOn", False)

# Run the observation
run_observation()

# Cleanup
hydrogen.shutdown_compatibility_system()
```

## 🌐 Protocol Transparency

The automatic compatibility system allows the same device to be accessed through multiple protocols simultaneously:

```python
# Get device bridge for protocol-specific access
manager = hydrogen.get_integration_manager()
bridge = manager.get_device_bridge("cam1")

# Internal protocol (direct device access)
bridge.set_property("coolerOn", True, hydrogen.ProtocolType.INTERNAL)
temp = bridge.get_property("temperature", hydrogen.ProtocolType.INTERNAL)

# ASCOM protocol (automatically mapped properties)
bridge.set_property("CoolerOn", True, hydrogen.ProtocolType.ASCOM)
ascom_temp = bridge.get_property("CCDTemperature", hydrogen.ProtocolType.ASCOM)

# INDI protocol (automatically mapped properties)  
bridge.set_property("CCD_COOLER", True, hydrogen.ProtocolType.INDI)
indi_temp = bridge.get_property("CCD_TEMPERATURE", hydrogen.ProtocolType.INDI)

# All three temperatures are the same - protocols stay synchronized!
assert temp == ascom_temp == indi_temp
```

## 📡 Device Types

### Camera

```python
camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294MM Pro")

# Basic operations
camera.set_property("coolerOn", True)
camera.set_property("targetTemperature", -10.0)
camera.set_property("binning", 2)
camera.set_property("gain", 100)

# Imaging
camera.start_exposure(60.0)  # 60-second exposure
camera.abort_exposure()      # Abort if needed
camera.save_image("image.fits")
```

### Telescope

```python
telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX-L")

# Coordinates
telescope.set_property("targetRA", 5.59)    # 5h 35m (M42)
telescope.set_property("targetDec", -5.39)  # -5° 23'
telescope.slew_to_target()

# Tracking and parking
telescope.set_property("tracking", True)
telescope.park()
telescope.unpark()
```

### Focuser

```python
focuser = hydrogen.create_compatible_focuser("foc1", "ZWO", "EAF")

# Position control
current_pos = focuser.get_property("position")
focuser.set_property("targetPosition", current_pos + 100)
focuser.move_relative(50)  # Move 50 steps outward

# Temperature compensation
focuser.set_property("tempComp", True)
```

### Dome

```python
dome = hydrogen.create_compatible_dome("dome1", "Technical Innovations", "ProDome")

# Shutter control
dome.open_shutter()
dome.close_shutter()

# Azimuth control
dome.slew_to_azimuth(180.0)  # Point south
dome.find_home()
dome.park()

# Telescope slaving
dome.slave_to_telescope(True)
```

### Observing Conditions (Weather Station)

```python
weather = hydrogen.create_compatible_observing_conditions("weather1", "Boltwood", "Cloud Sensor")

# Get readings
weather.refresh()
temp = weather.get_temperature()
humidity = weather.get_humidity()
wind_speed = weather.get_wind_speed()
cloud_cover = weather.get_cloud_cover()

# Safety monitoring
weather.set_safety_threshold(hydrogen.SensorType.WIND_SPEED, 0, 15)
weather.enable_safety_monitoring(True)
is_safe = weather.is_safe_for_observing()
```

## 📊 System Monitoring

```python
# Get compatibility system statistics
stats = hydrogen.get_compatibility_statistics()
print(f"Total devices: {stats.total_devices}")
print(f"ASCOM-enabled devices: {stats.ascom_devices}")
print(f"INDI-enabled devices: {stats.indi_devices}")
print(f"System uptime: {stats.uptime.total_seconds():.1f} seconds")

# Device type breakdown
for device_type, count in stats.device_type_count.items():
    print(f"{device_type}: {count}")
```

## 🔧 Advanced Configuration

### Custom Bridge Configuration

```python
# Create custom bridge configuration
config = hydrogen.BridgeConfiguration("MyCamera", "Custom Camera Description")
config.enable_ascom = True
config.enable_indi = True
config.indi_port = 7625
config.custom_properties["manufacturer"] = "Custom Corp"
config.custom_properties["model"] = "SuperCam Pro"

# Create device with custom configuration
camera = hydrogen.Camera("cam1", "Custom", "SuperCam")
bridge = hydrogen.create_bridge_with_config(camera, config)
```

### Event Callbacks

```python
# Weather change callback
def on_weather_change(condition):
    print(f"Weather changed to: {condition}")
    if condition == hydrogen.WeatherCondition.UNSAFE:
        emergency_shutdown()

weather.set_weather_change_callback(on_weather_change)

# Dome slewing callback
def on_dome_slewing(state, azimuth):
    print(f"Dome {state} at azimuth {azimuth:.1f}°")

dome.set_slewing_callback(on_dome_slewing)
```

## 🧪 Testing

```bash
# Test installation
python python/setup.py test

# Run examples
python examples/python/compatibility_example.py
python examples/python/complete_observatory_example.py
```

## 🐛 Troubleshooting

### Common Issues

**Import Error**: `ModuleNotFoundError: No module named 'pyhydrogen'`
- Solution: Ensure the module is built and installed correctly
- Run: `python python/setup.py install`

**ASCOM devices not appearing**: 
- Ensure ASCOM Platform is installed on Windows
- Check Windows registry for device entries
- Verify COM registration is enabled

**INDI clients cannot connect**:
- Check firewall settings for INDI ports (default 7624+)
- Verify INDI server is started
- Check device names match client expectations

### Debug Logging

```python
# Enable detailed logging
hydrogen.set_log_level("debug")

# This will show detailed information about:
# - Property mappings and translations
# - Protocol bridge operations  
# - Device communication
# - Error conditions
```

## 📚 Examples

See the `examples/python/` directory for complete working examples:

- `compatibility_example.py` - Basic compatibility demonstration
- `complete_observatory_example.py` - Full observatory automation
- `weather_monitoring.py` - Weather station integration
- `dome_control.py` - Dome automation examples

## 🤝 Contributing

Contributions are welcome! Please see the main project README for contribution guidelines.

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🆘 Support

- **Documentation**: See `docs/automatic_compatibility_system.md`
- **Issues**: Report bugs on GitHub Issues
- **Discussions**: Join GitHub Discussions for questions

---

**The Hydrogen Python bindings bring the power of automatic ASCOM/INDI compatibility to Python, making astronomical device control easier than ever before!** 🌟
