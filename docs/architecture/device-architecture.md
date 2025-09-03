# Device Architecture Guide

## Overview

Hydrogen's device architecture has been completely redesigned using a **modular, component-based approach** that maximizes code reuse, maintainability, and extensibility. The new architecture moves away from traditional inheritance hierarchies to a composition-based system with reusable behaviors and components.

## Architecture Principles

### 1. Composition Over Inheritance
- **Modular Behaviors**: Device capabilities implemented as composable behavior components
- **Reusable Components**: Core functionality shared across different device types
- **Dynamic Configuration**: Runtime composition and configuration of device behaviors

### 2. Component-Based Design
- **Core Components**: Communication, state management, and configuration as separate components
- **Behavior Components**: Specialized behaviors like movement, temperature control, imaging
- **Manager Components**: High-level coordination and integration components

### 3. Interface-Driven Development
- **Clear Interfaces**: Well-defined interfaces for all components and behaviors
- **Testability**: Easy mocking and testing through interface abstraction
- **Extensibility**: Simple addition of new behaviors and components

## Core Architecture Components

### Device Manager
Central coordinator that integrates all core components:

```cpp
class DeviceManager {
private:
    std::unique_ptr<CommunicationManager> communicationManager_;
    std::unique_ptr<StateManager> stateManager_;
    std::unique_ptr<ConfigManager> configManager_;
    std::vector<std::unique_ptr<DeviceBehavior>> behaviors_;

public:
    // Component access
    CommunicationManager& getCommunicationManager();
    StateManager& getStateManager();
    ConfigManager& getConfigManager();
    
    // Behavior management
    void addBehavior(std::unique_ptr<DeviceBehavior> behavior);
    void removeBehavior(const std::string& behaviorId);
    DeviceBehavior* getBehavior(const std::string& behaviorId);
    
    // Lifecycle management
    bool initialize();
    bool start();
    void stop();
    void shutdown();
};
```

### Communication Manager
Handles all communication protocols and connection management:

```cpp
class CommunicationManager {
public:
    // Connection management
    bool connect(const std::string& host, int port);
    void disconnect();
    bool isConnected() const;
    
    // Message handling
    bool sendMessage(const json& message);
    void setMessageHandler(std::function<void(const json&)> handler);
    
    // Multi-protocol support
    bool configureProtocol(CommunicationProtocol protocol, const json& config);
    bool sendMessage(const json& message, CommunicationProtocol protocol);
    
    // Connection monitoring
    ConnectionState getConnectionState() const;
    ConnectionStatistics getStatistics() const;
};
```

### State Manager
Thread-safe property and state management:

```cpp
class StateManager {
public:
    // Property management
    void setProperty(const std::string& name, const json& value);
    json getProperty(const std::string& name) const;
    bool hasProperty(const std::string& name) const;
    
    // Property validation
    void setPropertyValidator(const std::string& name, PropertyValidator validator);
    bool validateProperty(const std::string& name, const json& value) const;
    
    // Change notifications
    void addPropertyListener(const std::string& name, PropertyChangeListener listener);
    void removePropertyListener(const std::string& name, const std::string& listenerId);
    
    // Bulk operations
    void setProperties(const json& properties);
    json getAllProperties() const;
};
```

### Configuration Manager
Comprehensive configuration management with validation:

```cpp
class ConfigManager {
public:
    // Configuration management
    void defineConfiguration(const std::string& name, const ConfigDefinition& definition);
    void setConfiguration(const std::string& name, const json& value);
    json getConfiguration(const std::string& name) const;
    
    // Presets
    void savePreset(const std::string& name, const json& config);
    json loadPreset(const std::string& name) const;
    std::vector<std::string> getPresetNames() const;
    
    // Validation
    bool validateConfiguration(const std::string& name, const json& value) const;
    std::vector<std::string> getValidationErrors(const std::string& name, const json& value) const;
};
```

## Behavior System

### Base Device Behavior
All device behaviors inherit from the base `DeviceBehavior` class:

```cpp
class DeviceBehavior {
protected:
    std::string behaviorId_;
    DeviceManager* deviceManager_;
    
public:
    DeviceBehavior(const std::string& id) : behaviorId_(id) {}
    virtual ~DeviceBehavior() = default;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void shutdown() = 0;
    
    // Command handling
    virtual json handleCommand(const std::string& command, const json& params) = 0;
    
    // Status reporting
    virtual json getStatus() const = 0;
    virtual bool isHealthy() const = 0;
    
    // Configuration
    virtual void configure(const json& config) = 0;
    virtual json getConfiguration() const = 0;
};
```

### Movable Behavior
Provides generic movement control for focusers, filter wheels, rotators, etc.:

```cpp
class MovableBehavior : public DeviceBehavior {
private:
    int currentPosition_ = 0;
    int targetPosition_ = 0;
    bool isMoving_ = false;
    MovementLimits limits_;
    
public:
    // Position control
    void moveToPosition(int position);
    void moveRelative(int steps);
    void stopMovement();
    
    // Position queries
    int getCurrentPosition() const { return currentPosition_; }
    int getTargetPosition() const { return targetPosition_; }
    bool isMoving() const { return isMoving_; }
    
    // Calibration
    void findHome();
    void setZeroPosition();
    void calibrateRange();
    
    // Limits and validation
    void setMovementLimits(const MovementLimits& limits);
    bool isPositionValid(int position) const;
};
```

### Temperature Control Behavior
Provides PID-based temperature control for cameras, focusers, etc.:

```cpp
class TemperatureControlBehavior : public DeviceBehavior {
private:
    double currentTemperature_ = 0.0;
    double targetTemperature_ = 0.0;
    bool coolerEnabled_ = false;
    PIDController pidController_;
    
public:
    // Temperature control
    void setTargetTemperature(double temperature);
    void enableCooler(bool enable);
    bool isCoolerEnabled() const { return coolerEnabled_; }
    
    // Temperature monitoring
    double getCurrentTemperature() const { return currentTemperature_; }
    double getTargetTemperature() const { return targetTemperature_; }
    double getCoolerPower() const;
    
    // Control parameters
    void setPIDParameters(double kp, double ki, double kd);
    void setTemperatureLimits(double minTemp, double maxTemp);
    
    // Status
    bool isTemperatureStable() const;
    TemperatureControlStatus getControlStatus() const;
};
```

## Device Implementation Examples

### Modern Focuser Implementation

```cpp
class ModernFocuser : public ModernDeviceBase {
private:
    std::unique_ptr<MovableBehavior> movableBehavior_;
    std::unique_ptr<TemperatureControlBehavior> tempBehavior_;
    
public:
    ModernFocuser(const std::string& id, const std::string& manufacturer, const std::string& model)
        : ModernDeviceBase(id, manufacturer, model) {
        
        // Add movable behavior for position control
        movableBehavior_ = std::make_unique<MovableBehavior>("movement");
        addBehavior(std::move(movableBehavior_));
        
        // Add temperature behavior for compensation
        tempBehavior_ = std::make_unique<TemperatureControlBehavior>("temperature");
        addBehavior(std::move(tempBehavior_));
    }
    
    // Focuser-specific interface
    void moveToPosition(int position) {
        getBehavior<MovableBehavior>("movement")->moveToPosition(position);
    }
    
    void enableTemperatureCompensation(bool enable) {
        getBehavior<TemperatureControlBehavior>("temperature")->enableCooler(enable);
    }
};
```

### Modern Camera Implementation

```cpp
class ModernCamera : public ModernDeviceBase {
private:
    std::unique_ptr<ImagingBehavior> imagingBehavior_;
    std::unique_ptr<TemperatureControlBehavior> coolingBehavior_;
    std::unique_ptr<FilterWheelBehavior> filterBehavior_;
    
public:
    ModernCamera(const std::string& id, const std::string& manufacturer, const std::string& model)
        : ModernDeviceBase(id, manufacturer, model) {
        
        // Add imaging behavior
        imagingBehavior_ = std::make_unique<ImagingBehavior>("imaging");
        addBehavior(std::move(imagingBehavior_));
        
        // Add cooling behavior
        coolingBehavior_ = std::make_unique<TemperatureControlBehavior>("cooling");
        addBehavior(std::move(coolingBehavior_));
        
        // Add filter wheel behavior (if present)
        if (hasFilterWheel()) {
            filterBehavior_ = std::make_unique<FilterWheelBehavior>("filter");
            addBehavior(std::move(filterBehavior_));
        }
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

## Factory Pattern for Device Creation

### Device Factory
Centralized device creation with automatic behavior composition:

```cpp
class DeviceFactory {
public:
    // Generic device creation
    template<typename DeviceType>
    static std::unique_ptr<DeviceType> createDevice(
        const std::string& id,
        const std::string& manufacturer,
        const std::string& model,
        const json& config = {}) {
        
        auto device = std::make_unique<DeviceType>(id, manufacturer, model);
        
        // Apply configuration
        if (!config.empty()) {
            device->configure(config);
        }
        
        // Initialize device
        if (!device->initialize()) {
            throw std::runtime_error("Failed to initialize device: " + id);
        }
        
        return device;
    }
    
    // Specialized creation methods
    static std::unique_ptr<ModernFocuser> createFocuser(
        const std::string& id,
        const std::string& manufacturer,
        const std::string& model);
        
    static std::unique_ptr<ModernCamera> createCamera(
        const std::string& id,
        const std::string& manufacturer,
        const std::string& model);
};
```

### Configuration-Driven Creation

```cpp
// Create devices from configuration
json deviceConfig = {
    {"type", "focuser"},
    {"id", "focuser_001"},
    {"manufacturer", "ZWO"},
    {"model", "EAF"},
    {"behaviors", {
        {"movement", {
            {"min_position", 0},
            {"max_position", 50000},
            {"step_size", 1}
        }},
        {"temperature", {
            {"compensation_enabled", true},
            {"compensation_factor", 2.5}
        }}
    }}
};

auto focuser = DeviceFactory::createFromConfig(deviceConfig);
```

## Behavior Composition Examples

### Runtime Behavior Addition

```cpp
// Start with basic device
auto device = std::make_unique<ModernDeviceBase>("dev1", "Generic", "Device");

// Add behaviors dynamically
if (supportsMovement) {
    auto movableBehavior = std::make_unique<MovableBehavior>("movement");
    movableBehavior->setMovementLimits({0, 10000, 1});
    device->addBehavior(std::move(movableBehavior));
}

if (supportsTemperatureControl) {
    auto tempBehavior = std::make_unique<TemperatureControlBehavior>("temperature");
    tempBehavior->setPIDParameters(1.0, 0.1, 0.01);
    device->addBehavior(std::move(tempBehavior));
}
```

### Behavior Interaction

```cpp
// Behaviors can interact with each other through the device manager
class AdvancedFocuserBehavior : public DeviceBehavior {
public:
    json handleCommand(const std::string& command, const json& params) override {
        if (command == "auto_focus") {
            // Get temperature behavior for compensation
            auto tempBehavior = deviceManager_->getBehavior<TemperatureControlBehavior>("temperature");
            double currentTemp = tempBehavior->getCurrentTemperature();
            
            // Get movement behavior for positioning
            auto moveBehavior = deviceManager_->getBehavior<MovableBehavior>("movement");
            
            // Perform temperature-compensated auto-focus
            return performAutoFocus(currentTemp, moveBehavior);
        }
        
        return json{{"error", "Unknown command"}};
    }
};
```

## Testing and Mocking

### Behavior Testing

```cpp
// Test individual behaviors in isolation
TEST(MovableBehaviorTest, BasicMovement) {
    auto behavior = std::make_unique<MovableBehavior>("test_movement");
    
    // Mock device manager
    auto mockManager = std::make_unique<MockDeviceManager>();
    behavior->setDeviceManager(mockManager.get());
    
    // Test movement
    behavior->moveToPosition(1000);
    EXPECT_EQ(behavior->getTargetPosition(), 1000);
    
    // Simulate movement completion
    behavior->onMovementComplete();
    EXPECT_EQ(behavior->getCurrentPosition(), 1000);
    EXPECT_FALSE(behavior->isMoving());
}
```

### Device Integration Testing

```cpp
// Test complete device with all behaviors
TEST(ModernFocuserTest, FullIntegration) {
    auto focuser = DeviceFactory::createFocuser("test_focuser", "Test", "Focuser");
    
    // Test initialization
    EXPECT_TRUE(focuser->initialize());
    EXPECT_TRUE(focuser->start());
    
    // Test movement behavior
    focuser->moveToPosition(5000);
    EXPECT_EQ(focuser->getTargetPosition(), 5000);
    
    // Test temperature behavior
    focuser->enableTemperatureCompensation(true);
    EXPECT_TRUE(focuser->isTemperatureCompensationEnabled());
}
```

## Best Practices

### Design Guidelines

1. **Single Responsibility**: Each behavior should have a single, well-defined responsibility
2. **Loose Coupling**: Behaviors should interact through well-defined interfaces
3. **High Cohesion**: Related functionality should be grouped together in behaviors
4. **Composition**: Prefer composition over inheritance for device capabilities

### Implementation Best Practices

1. **Use Smart Pointers**: Manage behavior lifetimes with smart pointers
2. **Thread Safety**: Ensure thread-safe access to shared state
3. **Error Handling**: Implement comprehensive error handling and recovery
4. **Configuration**: Make behaviors configurable through JSON configuration
5. **Testing**: Write unit tests for individual behaviors and integration tests for devices

### Performance Considerations

1. **Lazy Initialization**: Initialize behaviors only when needed
2. **Resource Management**: Properly manage system resources (threads, connections, etc.)
3. **Caching**: Cache frequently accessed properties and calculations
4. **Async Operations**: Use asynchronous operations for long-running tasks

## Migration from Legacy Architecture

### Legacy to Modern Migration

```cpp
// Legacy inheritance-based focuser
class LegacyFocuser : public DeviceBase {
    // All functionality mixed together
};

// Modern composition-based focuser
class ModernFocuser : public ModernDeviceBase {
    // Functionality separated into behaviors
    std::unique_ptr<MovableBehavior> movableBehavior_;
    std::unique_ptr<TemperatureControlBehavior> tempBehavior_;
};
```

### Migration Steps

1. **Identify Behaviors**: Extract distinct behaviors from legacy devices
2. **Create Behavior Classes**: Implement behavior classes following the interface
3. **Update Device Classes**: Modify devices to use composition instead of inheritance
4. **Test Thoroughly**: Ensure all functionality is preserved during migration
5. **Update Documentation**: Document the new architecture and usage patterns

## Examples and References

See the following files for complete implementation examples:

- `src/device/core/modern_device_base.h/cpp` - Base device implementation
- `src/device/behaviors/` - Behavior implementations
- `src/device/README.md` - Detailed architecture documentation
- `examples/device/` - Example device implementations
- `tests/device/` - Comprehensive device and behavior tests
