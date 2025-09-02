#include <hydrogen/core/enhanced_device_manager.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::core;

int main() {
    std::cout << "Enhanced Device Manager Example" << std::endl;
    
    auto manager = EnhancedDeviceManagerFactory::createManagerWithHealthMonitoring();
    
    // Create device group
    std::string groupId = manager->createDeviceGroup("Test Sensors", "Example sensor group");
    
    // Register some devices
    DeviceInfo sensor1;
    sensor1.deviceId = "sensor_001";
    sensor1.deviceType = "temperature_sensor";
    sensor1.deviceName = "Temperature Sensor 1";
    manager->registerDevice(sensor1);
    
    manager->addDeviceToGroup("sensor_001", groupId);
    
    std::cout << "Device manager running with health monitoring..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    auto stats = manager->getEnhancedStatistics();
    std::cout << "Health checks performed: " << stats["healthChecksPerformed"] << std::endl;
    
    return 0;
}
