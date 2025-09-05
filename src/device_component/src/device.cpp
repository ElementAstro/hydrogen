#include <hydrogen/device.h>

// Main device implementations (from src/device/) - temporarily disabled
// #include "../../../device/camera.h"
// #include "../../../device/focuser.h"

// TODO: Adapter classes will be added here when we implement camera/focuser integration

namespace hydrogen {
namespace device {

void initialize() {
    try {
        // Initialize the core component first
        hydrogen::core::initialize();

        // Register built-in device types
        registerBuiltinDeviceTypes();

        // Log successful initialization
        // TODO: Add proper logging when available

    } catch (const std::exception& e) {
        // TODO: Add proper error logging
        throw std::runtime_error("Failed to initialize device component: " + std::string(e.what()));
    }
}

void cleanup() {
    try {
        // Get registry instance and stop all devices
        auto& registry = hydrogen::core::DeviceRegistry::getInstance();

        // TODO: Add method to stop all devices in registry
        // registry.stopAllDevices();

        // Clean up device-specific resources

        // Clean up the core component
        hydrogen::core::cleanup();

    } catch (const std::exception& e) {
        // TODO: Add proper error logging
        // Continue cleanup even if errors occur
    }
}

std::string getVersion() {
    return "1.0.0";
}

void registerBuiltinDeviceTypes() {
    try {
        auto& registry = hydrogen::core::DeviceRegistry::getInstance();

        // Register telescope factory with enhanced error handling
        registry.registerDeviceType("telescope",
            [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<hydrogen::core::IDevice> {
                try {
                    // Validate device ID
                    if (deviceId.empty()) {
                        throw std::invalid_argument("Device ID cannot be empty");
                    }

                    // Extract configuration with defaults
                    std::string manufacturer = config.value("manufacturer", "Generic");
                    std::string model = config.value("model", "Telescope");

                    // Validate configuration
                    if (manufacturer.empty()) manufacturer = "Generic";
                    if (model.empty()) model = "Telescope";

                    auto telescope = std::make_unique<hydrogen::device::Telescope>(deviceId, manufacturer, model);

                    // Apply additional configuration if provided
                    if (config.contains("observer_latitude") && config.contains("observer_longitude")) {
                        double lat = config["observer_latitude"];
                        double lon = config["observer_longitude"];
                        telescope->setObserverLocation(lat, lon);
                    }

                    if (config.contains("slew_rate")) {
                        int rate = config["slew_rate"];
                        if (rate >= 0 && rate <= 9) {
                            telescope->setSlewRate(rate);
                        }
                    }

                    return telescope;

                } catch (const std::exception& e) {
                    // TODO: Add proper logging
                    throw std::runtime_error("Failed to create telescope device '" + deviceId + "': " + e.what());
                }
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

    } catch (const std::exception& e) {
        // TODO: Add proper logging
        throw std::runtime_error("Failed to register built-in device types: " + std::string(e.what()));
    }
}

std::unique_ptr<Telescope> createTelescope(const std::string& deviceId,
                                         const std::string& manufacturer,
                                         const std::string& model) {
    try {
        // Validate parameters
        if (deviceId.empty()) {
            throw std::invalid_argument("Device ID cannot be empty");
        }

        std::string validManufacturer = manufacturer.empty() ? "Generic" : manufacturer;
        std::string validModel = model.empty() ? "Telescope" : model;

        return std::make_unique<Telescope>(deviceId, validManufacturer, validModel);

    } catch (const std::exception& e) {
        // TODO: Add proper logging
        throw std::runtime_error("Failed to create telescope: " + std::string(e.what()));
    }
}

std::unique_ptr<hydrogen::core::IDevice> createDevice(const std::string& deviceType,
                                                     const std::string& deviceId,
                                                     const nlohmann::json& config) {
    try {
        // Validate parameters
        if (deviceType.empty()) {
            throw std::invalid_argument("Device type cannot be empty");
        }
        if (deviceId.empty()) {
            throw std::invalid_argument("Device ID cannot be empty");
        }

        auto& registry = hydrogen::core::DeviceRegistry::getInstance();
        auto device = registry.createDevice(deviceType, deviceId, config);

        if (!device) {
            throw std::runtime_error("Failed to create device of type '" + deviceType + "'");
        }

        return device;

    } catch (const std::exception& e) {
        // TODO: Add proper logging
        throw std::runtime_error("Failed to create device '" + deviceId + "' of type '" + deviceType + "': " + std::string(e.what()));
    }
}

} // namespace device
} // namespace hydrogen
