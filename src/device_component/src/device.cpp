#include <hydrogen/device.h>

// Main device implementations (from src/device/) - temporarily disabled
// #include "../../../device/camera.h"
// #include "../../../device/focuser.h"

// TODO: Adapter classes will be added here when we implement camera/focuser integration

namespace hydrogen {
namespace device {

void initialize() {
    // Initialize the core component first
    hydrogen::core::initialize();
    
    // Register built-in device types
    registerBuiltinDeviceTypes();
}

void cleanup() {
    // Clean up device-specific resources
    
    // Clean up the core component
    hydrogen::core::cleanup();
}

std::string getVersion() {
    return "1.0.0";
}

void registerBuiltinDeviceTypes() {
    auto& registry = hydrogen::core::DeviceRegistry::getInstance();

    // Register telescope factory
    registry.registerDeviceType("telescope",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<hydrogen::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Generic");
            std::string model = config.value("model", "Telescope");

            return std::make_unique<hydrogen::device::Telescope>(deviceId, manufacturer, model);
        });

    // TODO: Register camera factory (temporarily disabled due to interface mismatch)
    // registry.registerDeviceType("camera", ...);

    // TODO: Register focuser factory (temporarily disabled due to interface mismatch)
    // registry.registerDeviceType("focuser", ...);

    // TODO: Register other device types (temporarily disabled - classes don't exist yet)
    // registry.registerDeviceType("filter_wheel", ...);
    // registry.registerDeviceType("guider", ...);
    // registry.registerDeviceType("rotator", ...);
    // registry.registerDeviceType("switch", ...);
    // registry.registerDeviceType("dome", ...);
    // registry.registerDeviceType("cover_calibrator", ...);
    // registry.registerDeviceType("observing_conditions", ...);
    // registry.registerDeviceType("safety_monitor", ...);
}

std::unique_ptr<Telescope> createTelescope(const std::string& deviceId,
                                         const std::string& manufacturer,
                                         const std::string& model) {
    return std::make_unique<Telescope>(deviceId, manufacturer, model);
}

std::unique_ptr<hydrogen::core::IDevice> createDevice(const std::string& deviceType,
                                                     const std::string& deviceId,
                                                     const nlohmann::json& config) {
    auto& registry = hydrogen::core::DeviceRegistry::getInstance();
    return registry.createDevice(deviceType, deviceId, config);
}

} // namespace device
} // namespace hydrogen
