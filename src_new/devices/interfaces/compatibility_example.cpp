#include "integration_manager.h"
#include "../camera.h"
#include "../telescope.h"
#include "../focuser.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::device;
using namespace hydrogen::device::interfaces;

/**
 * @brief Comprehensive example demonstrating automatic ASCOM/INDI compatibility
 * 
 * This example shows how the automatic compatibility system works without
 * requiring any changes to existing device implementations.
 */
class CompatibilityExample {
public:
    void runExample() {
        std::cout << "=== Hydrogen Automatic ASCOM/INDI Compatibility Example ===" << std::endl;
        
        // Initialize the integration manager
        initializeIntegrationManager();
        
        // Create and register devices
        createAndRegisterDevices();
        
        // Demonstrate transparent protocol access
        demonstrateProtocolAccess();
        
        // Demonstrate ASCOM client simulation
        demonstrateASCOMClient();
        
        // Demonstrate INDI client simulation
        demonstrateINDIClient();
        
        // Show statistics
        showStatistics();
        
        // Cleanup
        cleanup();
        
        std::cout << "=== Example completed ===" << std::endl;
    }

private:
    integration::AutomaticIntegrationManager* manager_;
    std::shared_ptr<Camera> camera_;
    std::shared_ptr<Telescope> telescope_;
    std::shared_ptr<Focuser> focuser_;
    
    void initializeIntegrationManager() {
        std::cout << "\n1. Initializing Integration Manager..." << std::endl;
        
        manager_ = &integration::AutomaticIntegrationManager::getInstance();
        
        // Configure integration
        integration::IntegrationConfiguration config;
        config.autoDiscovery = true;
        config.autoRegistration = true;
        config.enableASCOM = true;
        config.enableINDI = true;
        config.deviceNamePrefix = "HydrogenExample_";
        
        // Set device-specific configurations
        bridge::BridgeConfiguration cameraConfig("HydrogenExample_Camera", "Example Camera Device");
        cameraConfig.customProperties["manufacturer"] = "Hydrogen";
        cameraConfig.customProperties["model"] = "ExampleCam";
        config.deviceConfigs["camera1"] = cameraConfig;
        
        bridge::BridgeConfiguration telescopeConfig("HydrogenExample_Telescope", "Example Telescope Device");
        telescopeConfig.customProperties["manufacturer"] = "Hydrogen";
        telescopeConfig.customProperties["model"] = "ExampleScope";
        config.deviceConfigs["telescope1"] = telescopeConfig;
        
        bridge::BridgeConfiguration focuserConfig("HydrogenExample_Focuser", "Example Focuser Device");
        focuserConfig.customProperties["manufacturer"] = "Hydrogen";
        focuserConfig.customProperties["model"] = "ExampleFocus";
        config.deviceConfigs["focuser1"] = focuserConfig;
        
        manager_->initialize(config);
        manager_->start();
        
        // Add discovery callbacks
        manager_->addDeviceDiscoveryCallback([](const std::string& deviceId, std::shared_ptr<core::IDevice> device) {
            std::cout << "  Device discovered: " << deviceId << std::endl;
        });
        
        manager_->addDeviceRemovalCallback([](const std::string& deviceId) {
            std::cout << "  Device removed: " << deviceId << std::endl;
        });
        
        std::cout << "  Integration manager initialized and started" << std::endl;
    }
    
    void createAndRegisterDevices() {
        std::cout << "\n2. Creating and Registering Devices..." << std::endl;
        
        // Create devices using existing implementations
        camera_ = std::make_shared<Camera>("camera1", "Hydrogen", "ExampleCam");
        telescope_ = std::make_shared<Telescope>("telescope1", "Hydrogen", "ExampleScope");
        focuser_ = std::make_shared<Focuser>("focuser1", "Hydrogen", "ExampleFocus");
        
        // Initialize devices
        camera_->initializeDevice();
        camera_->startDevice();
        
        telescope_->initializeDevice();
        telescope_->startDevice();
        
        focuser_->initializeDevice();
        focuser_->startDevice();
        
        // Register devices - this automatically creates ASCOM/INDI bridges
        REGISTER_DEVICE_AUTO("camera1", camera_);
        REGISTER_DEVICE_AUTO("telescope1", telescope_);
        REGISTER_DEVICE_AUTO("focuser1", focuser_);
        
        std::cout << "  Devices registered with automatic ASCOM/INDI compatibility" << std::endl;
        
        // Wait for registration to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void demonstrateProtocolAccess() {
        std::cout << "\n3. Demonstrating Transparent Protocol Access..." << std::endl;
        
        try {
            // Access camera properties through different protocols
            std::cout << "  Camera Properties:" << std::endl;
            
            // Internal protocol access
            double internalTemp = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "temperature", double, bridge::ProtocolType::INTERNAL);
            std::cout << "    Internal temperature: " << internalTemp << "°C" << std::endl;
            
            // ASCOM protocol access (same property, different protocol)
            double ascomTemp = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "CCDTemperature", double, bridge::ProtocolType::ASCOM);
            std::cout << "    ASCOM temperature: " << ascomTemp << "°C" << std::endl;
            
            // INDI protocol access
            double indiTemp = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "CCD_TEMPERATURE", double, bridge::ProtocolType::INDI);
            std::cout << "    INDI temperature: " << indiTemp << "°C" << std::endl;
            
            // Set properties through different protocols
            SET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "coolerOn", true, bridge::ProtocolType::INTERNAL);
            std::cout << "    Cooler turned on via internal protocol" << std::endl;
            
            SET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "CoolerOn", true, bridge::ProtocolType::ASCOM);
            std::cout << "    Cooler confirmed via ASCOM protocol" << std::endl;
            
            // Telescope properties
            std::cout << "  Telescope Properties:" << std::endl;
            
            double ra = GET_DEVICE_PROPERTY_AUTO(Telescope, "telescope1", "rightAscension", double, bridge::ProtocolType::INTERNAL);
            double dec = GET_DEVICE_PROPERTY_AUTO(Telescope, "telescope1", "declination", double, bridge::ProtocolType::INTERNAL);
            std::cout << "    Current position: RA=" << ra << "h, Dec=" << dec << "°" << std::endl;
            
            // Focuser properties
            std::cout << "  Focuser Properties:" << std::endl;
            
            int position = GET_DEVICE_PROPERTY_AUTO(Focuser, "focuser1", "position", int, bridge::ProtocolType::INTERNAL);
            std::cout << "    Current position: " << position << " steps" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << std::endl;
        }
    }
    
    void demonstrateASCOMClient() {
        std::cout << "\n4. Simulating ASCOM Client Access..." << std::endl;
        
        try {
            // Simulate ASCOM client connecting to camera
            std::cout << "  ASCOM Client connecting to camera..." << std::endl;
            
            // Get ASCOM standard properties
            auto name = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "Name", std::string, bridge::ProtocolType::ASCOM);
            auto description = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "Description", std::string, bridge::ProtocolType::ASCOM);
            auto driverVersion = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "DriverVersion", std::string, bridge::ProtocolType::ASCOM);
            
            std::cout << "    Device Name: " << name << std::endl;
            std::cout << "    Description: " << description << std::endl;
            std::cout << "    Driver Version: " << driverVersion << std::endl;
            
            // Connect to device
            SET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "Connected", true, bridge::ProtocolType::ASCOM);
            bool connected = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "Connected", bool, bridge::ProtocolType::ASCOM);
            std::cout << "    Connected: " << (connected ? "Yes" : "No") << std::endl;
            
            // Start exposure using ASCOM method
            std::cout << "    Starting 5-second exposure..." << std::endl;
            INVOKE_DEVICE_METHOD_AUTO(Camera, void, "camera1", "StartExposure", bridge::ProtocolType::ASCOM, 5.0, true);
            
            // Check exposure status
            bool imageReady = GET_DEVICE_PROPERTY_AUTO(Camera, "camera1", "ImageReady", bool, bridge::ProtocolType::ASCOM);
            std::cout << "    Image Ready: " << (imageReady ? "Yes" : "No") << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << std::endl;
        }
    }
    
    void demonstrateINDIClient() {
        std::cout << "\n5. Simulating INDI Client Access..." << std::endl;
        
        try {
            // Simulate INDI client connecting to telescope
            std::cout << "  INDI Client connecting to telescope..." << std::endl;
            
            // Get INDI properties
            double currentRA = GET_DEVICE_PROPERTY_AUTO(Telescope, "telescope1", "EQUATORIAL_EOD_COORD", double, bridge::ProtocolType::INDI);
            double currentDec = GET_DEVICE_PROPERTY_AUTO(Telescope, "telescope1", "EQUATORIAL_EOD_COORD", double, bridge::ProtocolType::INDI);
            
            std::cout << "    Current coordinates: RA=" << currentRA << "h, Dec=" << currentDec << "°" << std::endl;
            
            // Set target coordinates
            SET_DEVICE_PROPERTY_AUTO(Telescope, "telescope1", "EQUATORIAL_EOD_COORD", 12.5, bridge::ProtocolType::INDI);
            std::cout << "    Target RA set to 12.5h" << std::endl;
            
            // Start slew
            std::cout << "    Starting slew to target..." << std::endl;
            INVOKE_DEVICE_METHOD_AUTO(Telescope, void, "telescope1", "slewToCoordinates", bridge::ProtocolType::INDI, 12.5, 45.0);
            
            // Check slewing status
            bool slewing = GET_DEVICE_PROPERTY_AUTO(Telescope, "telescope1", "TELESCOPE_MOTION_NS", bool, bridge::ProtocolType::INDI);
            std::cout << "    Slewing: " << (slewing ? "Yes" : "No") << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "    Error: " << e.what() << std::endl;
        }
    }
    
    void showStatistics() {
        std::cout << "\n6. Integration Statistics..." << std::endl;
        
        auto stats = manager_->getStatistics();
        
        std::cout << "  Total devices: " << stats.totalDevices << std::endl;
        std::cout << "  ASCOM-enabled devices: " << stats.ascomDevices << std::endl;
        std::cout << "  INDI-enabled devices: " << stats.indiDevices << std::endl;
        std::cout << "  Uptime: " << stats.uptime.count() << " ms" << std::endl;
        
        std::cout << "  Device types:" << std::endl;
        for (const auto& [type, count] : stats.deviceTypeCount) {
            std::cout << "    " << type << ": " << count << std::endl;
        }
        
        // Show registered devices
        auto deviceIds = manager_->getRegisteredDeviceIds();
        std::cout << "  Registered devices:" << std::endl;
        for (const auto& deviceId : deviceIds) {
            std::cout << "    " << deviceId << std::endl;
        }
    }
    
    void cleanup() {
        std::cout << "\n7. Cleaning up..." << std::endl;
        
        // Unregister devices
        UNREGISTER_DEVICE_AUTO("camera1");
        UNREGISTER_DEVICE_AUTO("telescope1");
        UNREGISTER_DEVICE_AUTO("focuser1");
        
        // Stop devices
        if (camera_) {
            camera_->stopDevice();
        }
        if (telescope_) {
            telescope_->stopDevice();
        }
        if (focuser_) {
            focuser_->stopDevice();
        }
        
        // Stop integration manager
        manager_->stop();
        
        std::cout << "  Cleanup completed" << std::endl;
    }
};

/**
 * @brief Main function demonstrating the automatic compatibility system
 */
int main() {
    try {
        CompatibilityExample example;
        example.runExample();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
