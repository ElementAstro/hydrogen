#include <astrocomm/device.h>

namespace astrocomm {
namespace device {

void initialize() {
    // Initialize the core component first
    astrocomm::core::initialize();
    
    // Register built-in device types
    registerBuiltinDeviceTypes();
}

void cleanup() {
    // Clean up device-specific resources
    
    // Clean up the core component
    astrocomm::core::cleanup();
}

std::string getVersion() {
    return "1.0.0";
}

void registerBuiltinDeviceTypes() {
    auto& registry = astrocomm::core::DeviceRegistry::getInstance();
    
    // Register telescope factory
    registry.registerDeviceType("telescope", 
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Generic");
            std::string model = config.value("model", "Telescope");
            
            return std::make_unique<Telescope>(deviceId, manufacturer, model);
        });
    
    // Additional device types can be registered here as they are implemented
    // registry.registerDeviceType("camera", ...);
    // registry.registerDeviceType("focuser", ...);
    // registry.registerDeviceType("filter_wheel", ...);
}

std::unique_ptr<Telescope> createTelescope(const std::string& deviceId,
                                         const std::string& manufacturer,
                                         const std::string& model) {
    return std::make_unique<Telescope>(deviceId, manufacturer, model);
}

std::unique_ptr<astrocomm::core::IDevice> createDevice(const std::string& deviceType,
                                                     const std::string& deviceId,
                                                     const nlohmann::json& config) {
    auto& registry = astrocomm::core::DeviceRegistry::getInstance();
    return registry.createDevice(deviceType, deviceId, config);
}

} // namespace device
} // namespace astrocomm
