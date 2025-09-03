# Examples Guide

## Overview

This guide provides an overview of all available examples in the Hydrogen project, organized by category and complexity level. Each example demonstrates specific features and usage patterns.

## Example Categories

### C++ Examples (`examples/cpp/`)

#### Unified Client Example
**File**: `examples/cpp/unified_client_example.cpp`
**Level**: Beginner
**Features**: Basic client usage, device discovery, command execution

```cpp
// Demonstrates basic unified client usage
#include <hydrogen/core/unified_device_client.h>

int main() {
    // Create and configure client
    ClientConnectionConfig config;
    config.host = "localhost";
    config.port = 8080;
    
    auto client = std::make_unique<UnifiedDeviceClient>(config);
    
    // Connect and discover devices
    if (client->connect()) {
        auto devices = client->discoverDevices();
        std::cout << "Found devices: " << devices.dump(2) << std::endl;
        
        // Execute commands
        auto result = client->executeCommand("camera_001", "get_status");
        std::cout << "Camera status: " << result.dump(2) << std::endl;
    }
    
    return 0;
}
```

#### Comprehensive Testing Example
**File**: `examples/cpp/comprehensive_testing_example.cpp`
**Level**: Advanced
**Features**: Testing framework usage, mock objects, performance testing

```cpp
// Demonstrates comprehensive testing capabilities
#include <hydrogen/testing/test_framework.h>
#include <hydrogen/testing/mock_device.h>

void demonstrateUnitTesting() {
    // Create mock device for testing
    auto mockCamera = std::make_unique<MockCamera>("test_camera");
    
    // Set up expectations
    EXPECT_CALL(*mockCamera, startExposure(60.0))
        .WillOnce(Return(true));
    
    // Test device functionality
    bool result = mockCamera->startExposure(60.0);
    ASSERT_TRUE(result);
}

void demonstratePerformanceTesting() {
    // Performance benchmark example
    auto client = createTestClient();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Execute 1000 commands
    for (int i = 0; i < 1000; ++i) {
        client->executeCommand("test_device", "ping");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "1000 commands executed in " << duration.count() << "ms" << std::endl;
}
```

### Device Examples (`examples/device/`)

#### CCD Camera Example
**Files**: `examples/device/ccd_camera.h`, `examples/device/ccd_camera.cpp`
**Level**: Intermediate
**Features**: Device implementation, behavior composition, ASCOM/INDI compatibility

```cpp
// Custom CCD camera implementation
class CCDCamera : public ModernDeviceBase {
private:
    std::unique_ptr<ImagingBehavior> imagingBehavior_;
    std::unique_ptr<TemperatureControlBehavior> coolingBehavior_;
    
public:
    CCDCamera(const std::string& id, const std::string& manufacturer, const std::string& model)
        : ModernDeviceBase(id, manufacturer, model) {
        
        // Add imaging capabilities
        imagingBehavior_ = std::make_unique<ImagingBehavior>("imaging");
        addBehavior(std::move(imagingBehavior_));
        
        // Add cooling capabilities
        coolingBehavior_ = std::make_unique<TemperatureControlBehavior>("cooling");
        addBehavior(std::move(coolingBehavior_));
    }
    
    // Camera-specific interface
    void startExposure(double duration) {
        getBehavior<ImagingBehavior>("imaging")->startExposure(duration);
    }
    
    void setCoolerTemperature(double temperature) {
        getBehavior<TemperatureControlBehavior>("cooling")->setTargetTemperature(temperature);
    }
};
```

#### Custom Rotator Example
**Files**: `examples/device/custom_rotator.h`, `examples/device/custom_rotator.cpp`
**Level**: Intermediate
**Features**: Rotator device, movement behavior, position control

```cpp
// Custom rotator implementation
class CustomRotator : public ModernDeviceBase {
private:
    std::unique_ptr<MovableBehavior> movableBehavior_;
    
public:
    CustomRotator(const std::string& id, const std::string& manufacturer, const std::string& model)
        : ModernDeviceBase(id, manufacturer, model) {
        
        // Add rotational movement
        movableBehavior_ = std::make_unique<MovableBehavior>("rotation");
        
        // Configure for 360-degree rotation
        MovementLimits limits;
        limits.minPosition = 0;
        limits.maxPosition = 36000; // 0.01 degree steps
        limits.stepSize = 1;
        movableBehavior_->setMovementLimits(limits);
        
        addBehavior(std::move(movableBehavior_));
    }
    
    void rotateToAngle(double angle) {
        int position = static_cast<int>(angle * 100); // Convert to 0.01 degree steps
        getBehavior<MovableBehavior>("rotation")->moveToPosition(position);
    }
};
```

### Python Examples (`examples/python/`)

#### Compatibility Example
**File**: `examples/python/compatibility_example.py`
**Level**: Beginner
**Features**: Basic Python usage, automatic ASCOM/INDI compatibility

```python
import pyhydrogen as hydrogen

def main():
    # Initialize compatibility system
    hydrogen.init_compatibility_system()
    
    try:
        # Create devices with automatic compatibility
        camera = hydrogen.create_compatible_camera("cam1", "ZWO", "ASI294MM Pro")
        telescope = hydrogen.create_compatible_telescope("tel1", "Celestron", "CGX-L")
        
        # Use devices - ASCOM/INDI support is automatic
        camera.set_property("coolerOn", True)
        camera.set_property("targetTemperature", -10.0)
        
        telescope.set_property("targetRA", 5.59)  # M42
        telescope.set_property("targetDec", -5.39)
        telescope.slew_to_target()
        
        # Start imaging
        camera.start_exposure(60.0)
        
        print("Devices created and configured successfully!")
        print("ASCOM and INDI compatibility is automatic!")
        
    finally:
        # Cleanup
        hydrogen.shutdown_compatibility_system()

if __name__ == "__main__":
    main()
```

#### Complete Observatory Example
**File**: `examples/python/complete_observatory_example.py`
**Level**: Advanced
**Features**: Full observatory automation, weather monitoring, dome control

```python
import pyhydrogen as hydrogen
import time

def run_observatory_sequence():
    # Initialize system
    hydrogen.init_compatibility_system()
    
    try:
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
        if run_observation_sequence(devices):
            print("Observatory sequence completed successfully!")
        else:
            print("Observatory sequence aborted due to weather or errors")
            
    finally:
        hydrogen.shutdown_compatibility_system()

def run_observation_sequence(devices):
    # Check weather
    weather = devices['weather']
    weather.refresh()
    if not weather.is_safe_for_observing():
        print("Weather unsafe for observing")
        return False
    
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
    
    return True
```

#### Device-Specific Examples

**Advanced Camera Example** (`examples/python/advanced_camera.py`)
- Advanced imaging techniques
- Cooling control and monitoring
- Filter wheel integration

**Focuser Example** (`examples/python/focuser_example.py`)
- Position control and movement
- Temperature compensation
- Auto-focus algorithms

**Telescope Example** (`examples/python/custom_telescope.py`)
- Coordinate systems and transformations
- Slewing and tracking
- Pier side management

**Filter Wheel Example** (`examples/python/filter_wheel_example.py`)
- Filter selection and management
- Position sensing and calibration
- Integration with imaging workflows

**Switch Example** (`examples/python/switch_example.py`)
- Multi-channel switch control
- Power management
- Safety interlocks

**Guider Example** (`examples/python/guider_example.py`)
- Guide star selection
- Autoguiding algorithms
- Dithering patterns

**Rotator Example** (`examples/python/rotator_example.py`)
- Angle control and positioning
- Sky angle calculations
- Mechanical backlash compensation

**Solver Example** (`examples/python/solver_example.py`)
- Plate solving integration
- Coordinate calibration
- Pointing model correction

## Running Examples

### Prerequisites

1. **Build Hydrogen**: Ensure Hydrogen is built with examples enabled
   ```bash
   cmake --preset default -DHYDROGEN_BUILD_EXAMPLES=ON
   cmake --build --preset default
   ```

2. **Python Bindings** (for Python examples): Enable Python bindings
   ```bash
   cmake --preset default -DHYDROGEN_ENABLE_PYTHON_BINDINGS=ON
   cmake --build --preset default
   ```

3. **Server Running**: Start a Hydrogen server for client examples
   ```bash
   ./build/src/server/hydrogen_server
   ```

### Running C++ Examples

```bash
# Run unified client example
./build/examples/cpp/unified_client_example

# Run comprehensive testing example
./build/examples/cpp/comprehensive_testing_example
```

### Running Python Examples

```bash
# Set Python path (if needed)
export PYTHONPATH=$PYTHONPATH:./build/src/python

# Run compatibility example
python examples/python/compatibility_example.py

# Run complete observatory example
python examples/python/complete_observatory_example.py
```

### Running Device Examples

```bash
# Run CCD camera example
./build/examples/device/ccd_camera_example

# Run custom rotator example
./build/examples/device/custom_rotator_example
```

## Example Modifications

### Customizing Examples

1. **Change Connection Settings**: Modify host/port in configuration
2. **Add Device Types**: Include additional device types in examples
3. **Modify Behaviors**: Customize device behaviors and properties
4. **Add Error Handling**: Enhance error handling and recovery

### Creating New Examples

1. **Copy Existing Example**: Start with a similar example
2. **Modify for Your Use Case**: Adapt to your specific requirements
3. **Add Documentation**: Document your example for others
4. **Submit Pull Request**: Share useful examples with the community

## Troubleshooting Examples

### Common Issues

**Connection Failures**: Ensure server is running and accessible
**Missing Dependencies**: Check that all required libraries are installed
**Python Import Errors**: Verify Python bindings are built and in path
**Device Not Found**: Check device IDs and server configuration

### Debug Mode

Enable debug logging for detailed information:

```cpp
// C++ examples
hydrogen::setLogLevel(LogLevel::DEBUG);
```

```python
# Python examples
hydrogen.set_log_level("debug")
```

## Contributing Examples

We welcome contributions of new examples! Please:

1. **Follow Coding Standards**: Use consistent style and formatting
2. **Add Documentation**: Include clear comments and documentation
3. **Test Thoroughly**: Ensure examples work correctly
4. **Submit Pull Request**: Follow the contribution guidelines

## Additional Resources

- **API Documentation**: Complete API reference in `docs/API_REFERENCE.md`
- **Architecture Guide**: System architecture in `docs/UNIFIED_ARCHITECTURE.md`
- **Device Guide**: Device architecture in `docs/DEVICE_ARCHITECTURE_GUIDE.md`
- **Troubleshooting**: Common issues in `docs/TROUBLESHOOTING.md`
