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

            return std::make_unique<astrocomm::device::Telescope>(deviceId, manufacturer, model);
        });

    // Register camera factory
    registry.registerDeviceType("camera",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "ZWO");
            std::string model = config.value("model", "ASI294MC");

            return std::make_unique<astrocomm::device::Camera>(deviceId, manufacturer, model);
        });

    // Register focuser factory
    registry.registerDeviceType("focuser",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "ZWO");
            std::string model = config.value("model", "EAF");

            return std::make_unique<astrocomm::device::Focuser>(deviceId, manufacturer, model);
        });

    // Register filter wheel factory
    registry.registerDeviceType("filter_wheel",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "ZWO");
            std::string model = config.value("model", "EFW");

            return std::make_unique<astrocomm::device::FilterWheel>(deviceId, manufacturer, model);
        });

    // Register guider factory
    registry.registerDeviceType("guider",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "PHD2");
            std::string model = config.value("model", "Guider");

            return std::make_unique<astrocomm::device::Guider>(deviceId, manufacturer, model);
        });

    // Register rotator factory
    registry.registerDeviceType("rotator",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Pegasus");
            std::string model = config.value("model", "FocusCube");

            return std::make_unique<astrocomm::device::Rotator>(deviceId, manufacturer, model);
        });

    // Register switch factory
    registry.registerDeviceType("switch",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Pegasus");
            std::string model = config.value("model", "UPB");

            return std::make_unique<astrocomm::device::Switch>(deviceId, manufacturer, model);
        });

    // Register dome factory
    registry.registerDeviceType("dome",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Generic");
            std::string model = config.value("model", "Dome");

            return std::make_unique<astrocomm::device::Dome>(deviceId, manufacturer, model);
        });

    // Register cover calibrator factory
    registry.registerDeviceType("cover_calibrator",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Generic");
            std::string model = config.value("model", "CoverCalibrator");

            return std::make_unique<astrocomm::device::CoverCalibrator>(deviceId, manufacturer, model);
        });

    // Register observing conditions factory
    registry.registerDeviceType("observing_conditions",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Generic");
            std::string model = config.value("model", "WeatherStation");

            return std::make_unique<astrocomm::device::ObservingConditions>(deviceId, manufacturer, model);
        });

    // Register safety monitor factory
    registry.registerDeviceType("safety_monitor",
        [](const std::string& deviceId, const nlohmann::json& config) -> std::unique_ptr<astrocomm::core::IDevice> {
            std::string manufacturer = config.value("manufacturer", "Generic");
            std::string model = config.value("model", "SafetyMonitor");

            return std::make_unique<astrocomm::device::SafetyMonitor>(deviceId, manufacturer, model);
        });
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
