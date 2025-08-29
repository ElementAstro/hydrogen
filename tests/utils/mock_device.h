#pragma once

#include <astrocomm/core/device_interface.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace astrocomm {
namespace test {

/**
 * @brief Mock device implementation for testing
 */
class MockDevice : public astrocomm::core::IDevice {
public:
    MOCK_METHOD(std::string, getDeviceId, (), (const, override));
    MOCK_METHOD(std::string, getDeviceType, (), (const, override));
    MOCK_METHOD(nlohmann::json, getDeviceInfo, (), (const, override));
    MOCK_METHOD(void, setProperty, (const std::string&, const nlohmann::json&), (override));
    MOCK_METHOD(nlohmann::json, getProperty, (const std::string&), (const, override));
    MOCK_METHOD(nlohmann::json, getAllProperties, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, getCapabilities, (), (const, override));
    MOCK_METHOD(bool, hasCapability, (const std::string&), (const, override));
    MOCK_METHOD(void, addCapability, (const std::string&), (override));
    MOCK_METHOD(void, removeCapability, (const std::string&), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    MOCK_METHOD(nlohmann::json, getStatus, (), (const, override));
    MOCK_METHOD(bool, configure, (const nlohmann::json&), (override));
    MOCK_METHOD(nlohmann::json, getConfiguration, (), (const, override));
    MOCK_METHOD(bool, connect, (const std::string&, uint16_t), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
};

/**
 * @brief Simple test device implementation
 */
class TestDevice : public astrocomm::core::DeviceBase {
public:
    TestDevice(const std::string& deviceId, const std::string& deviceType,
               const std::string& manufacturer, const std::string& model)
        : DeviceBase(deviceId, deviceType, manufacturer, model) {
        initializeProperties();
    }
    
    bool connect(const std::string& host, uint16_t port) override {
        host_ = host;
        port_ = port;
        connected_ = true;
        return true;
    }
    
    void disconnect() override {
        connected_ = false;
        running_ = false;
    }
    
    bool start() override {
        if (!connected_) return false;
        running_ = true;
        return true;
    }
    
    void stop() override {
        running_ = false;
    }
    
    // Test helpers
    std::string getHost() const { return host_; }
    uint16_t getPort() const { return port_; }
    
    // Expose protected methods for testing
    using DeviceBase::handleCommandMessage;
    using DeviceBase::sendResponse;
    using DeviceBase::sendEvent;
    using DeviceBase::sendPropertyChangedEvent;
    
private:
    std::string host_;
    uint16_t port_ = 0;
};

/**
 * @brief Factory for creating test devices
 */
class TestDeviceFactory {
public:
    static std::unique_ptr<TestDevice> createTelescope(const std::string& deviceId = "test_telescope") {
        return std::make_unique<TestDevice>(deviceId, "telescope", "Test Manufacturer", "Test Telescope v1.0");
    }
    
    static std::unique_ptr<TestDevice> createCamera(const std::string& deviceId = "test_camera") {
        return std::make_unique<TestDevice>(deviceId, "camera", "Test Manufacturer", "Test Camera v1.0");
    }
    
    static std::unique_ptr<TestDevice> createFocuser(const std::string& deviceId = "test_focuser") {
        return std::make_unique<TestDevice>(deviceId, "focuser", "Test Manufacturer", "Test Focuser v1.0");
    }
    
    static std::unique_ptr<TestDevice> createGeneric(const std::string& deviceId, const std::string& deviceType) {
        return std::make_unique<TestDevice>(deviceId, deviceType, "Test Manufacturer", "Test Device v1.0");
    }
};

} // namespace test
} // namespace astrocomm
