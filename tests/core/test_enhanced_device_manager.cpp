#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <astrocomm/core/enhanced_device_manager.h>
#include <thread>
#include <chrono>

using namespace astrocomm::core;
using namespace testing;

class EnhancedDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        json config = {
            {"device_manager", {
                {"max_devices", 1000},
                {"health_check_interval", 30000}
            }}
        };
        
        manager_ = std::make_unique<EnhancedDeviceManager>(config);
    }

    void TearDown() override {
        if (manager_) {
            manager_->stopHealthMonitoring();
            manager_->stopDeviceDiscovery();
        }
    }

    std::unique_ptr<EnhancedDeviceManager> manager_;
};

TEST_F(EnhancedDeviceManagerTest, ManagerInitialization) {
    EXPECT_FALSE(manager_->isHealthMonitoringActive());
    EXPECT_FALSE(manager_->isDiscoveryActive());
    EXPECT_FALSE(manager_->isAutoRegistrationEnabled());
}

TEST_F(EnhancedDeviceManagerTest, HealthMonitoring) {
    EXPECT_TRUE(manager_->startHealthMonitoring());
    EXPECT_TRUE(manager_->isHealthMonitoringActive());
    
    manager_->stopHealthMonitoring();
    EXPECT_FALSE(manager_->isHealthMonitoringActive());
}

TEST_F(EnhancedDeviceManagerTest, DeviceHealthInfo) {
    // Register a device first
    DeviceInfo device;
    device.deviceId = "test_device";
    device.deviceType = "sensor";
    device.deviceName = "Test Sensor";
    EXPECT_TRUE(manager_->registerDevice(device));
    
    // Create health info
    DeviceHealthInfo health;
    health.deviceId = "test_device";
    health.status = HealthStatus::HEALTHY;
    health.statusMessage = "Device is operating normally";
    health.cpuUsage = 25.5;
    health.memoryUsage = 60.0;
    health.temperature = 45.0;
    
    EXPECT_TRUE(manager_->updateDeviceHealth("test_device", health));
    
    auto retrievedHealth = manager_->getDeviceHealth("test_device");
    EXPECT_EQ(retrievedHealth.deviceId, "test_device");
    EXPECT_EQ(retrievedHealth.status, HealthStatus::HEALTHY);
    EXPECT_EQ(retrievedHealth.cpuUsage, 25.5);
    EXPECT_TRUE(retrievedHealth.isHealthy());
    EXPECT_FALSE(retrievedHealth.requiresAttention());
}

TEST_F(EnhancedDeviceManagerTest, UnhealthyDevices) {
    // Register devices with different health statuses
    DeviceInfo device1;
    device1.deviceId = "healthy_device";
    device1.deviceType = "sensor";
    EXPECT_TRUE(manager_->registerDevice(device1));
    
    DeviceInfo device2;
    device2.deviceId = "warning_device";
    device2.deviceType = "sensor";
    EXPECT_TRUE(manager_->registerDevice(device2));
    
    DeviceInfo device3;
    device3.deviceId = "critical_device";
    device3.deviceType = "sensor";
    EXPECT_TRUE(manager_->registerDevice(device3));
    
    // Set health statuses
    DeviceHealthInfo healthyHealth;
    healthyHealth.deviceId = "healthy_device";
    healthyHealth.status = HealthStatus::HEALTHY;
    manager_->updateDeviceHealth("healthy_device", healthyHealth);
    
    DeviceHealthInfo warningHealth;
    warningHealth.deviceId = "warning_device";
    warningHealth.status = HealthStatus::WARNING;
    manager_->updateDeviceHealth("warning_device", warningHealth);
    
    DeviceHealthInfo criticalHealth;
    criticalHealth.deviceId = "critical_device";
    criticalHealth.status = HealthStatus::CRITICAL;
    manager_->updateDeviceHealth("critical_device", criticalHealth);
    
    auto unhealthyDevices = manager_->getUnhealthyDevices();
    EXPECT_EQ(unhealthyDevices.size(), 2); // warning and critical devices
    
    bool foundWarning = false, foundCritical = false;
    for (const auto& health : unhealthyDevices) {
        if (health.deviceId == "warning_device") foundWarning = true;
        if (health.deviceId == "critical_device") foundCritical = true;
    }
    EXPECT_TRUE(foundWarning);
    EXPECT_TRUE(foundCritical);
}

TEST_F(EnhancedDeviceManagerTest, DeviceGroups) {
    std::string groupId = manager_->createDeviceGroup("Test Group", "A test device group");
    EXPECT_FALSE(groupId.empty());
    
    auto group = manager_->getDeviceGroup(groupId);
    EXPECT_EQ(group.groupId, groupId);
    EXPECT_EQ(group.groupName, "Test Group");
    EXPECT_EQ(group.description, "A test device group");
    EXPECT_TRUE(group.deviceIds.empty());
    
    // Register devices and add to group
    DeviceInfo device1;
    device1.deviceId = "device1";
    device1.deviceType = "sensor";
    EXPECT_TRUE(manager_->registerDevice(device1));
    
    DeviceInfo device2;
    device2.deviceId = "device2";
    device2.deviceType = "actuator";
    EXPECT_TRUE(manager_->registerDevice(device2));
    
    EXPECT_TRUE(manager_->addDeviceToGroup("device1", groupId));
    EXPECT_TRUE(manager_->addDeviceToGroup("device2", groupId));
    
    group = manager_->getDeviceGroup(groupId);
    EXPECT_EQ(group.getDeviceCount(), 2);
    EXPECT_TRUE(group.containsDevice("device1"));
    EXPECT_TRUE(group.containsDevice("device2"));
    
    auto groupDevices = manager_->getGroupDevices(groupId);
    EXPECT_EQ(groupDevices.size(), 2);
    
    auto deviceGroups = manager_->getDeviceGroups("device1");
    EXPECT_EQ(deviceGroups.size(), 1);
    EXPECT_EQ(deviceGroups[0], groupId);
}

TEST_F(EnhancedDeviceManagerTest, DeviceGroupRemoval) {
    std::string groupId = manager_->createDeviceGroup("Test Group");
    
    DeviceInfo device;
    device.deviceId = "test_device";
    device.deviceType = "sensor";
    EXPECT_TRUE(manager_->registerDevice(device));
    EXPECT_TRUE(manager_->addDeviceToGroup("test_device", groupId));
    
    // Remove device from group
    EXPECT_TRUE(manager_->removeDeviceFromGroup("test_device", groupId));
    
    auto group = manager_->getDeviceGroup(groupId);
    EXPECT_EQ(group.getDeviceCount(), 0);
    EXPECT_FALSE(group.containsDevice("test_device"));
    
    // Delete group
    EXPECT_TRUE(manager_->deleteDeviceGroup(groupId));
    
    auto deletedGroup = manager_->getDeviceGroup(groupId);
    EXPECT_TRUE(deletedGroup.groupId.empty());
}

TEST_F(EnhancedDeviceManagerTest, ConfigurationTemplates) {
    json baseConfig = {
        {"sampling_rate", 1000},
        {"precision", "high"},
        {"enabled", true}
    };
    
    std::string templateId = manager_->createConfigTemplate("Sensor Template", "sensor", baseConfig);
    EXPECT_FALSE(templateId.empty());
    
    auto templateData = manager_->getConfigTemplate(templateId);
    EXPECT_EQ(templateData.templateId, templateId);
    EXPECT_EQ(templateData.templateName, "Sensor Template");
    EXPECT_EQ(templateData.deviceType, "sensor");
    EXPECT_EQ(templateData.baseConfiguration, baseConfig);
    
    // Test configuration generation
    std::unordered_map<std::string, json> variables = {
        {"sampling_rate", 2000}
    };
    
    json generatedConfig = templateData.generateConfiguration(variables);
    EXPECT_EQ(generatedConfig["sampling_rate"], 2000);
    EXPECT_EQ(generatedConfig["precision"], "high");
    EXPECT_EQ(generatedConfig["enabled"], true);
}

TEST_F(EnhancedDeviceManagerTest, DeviceRegistrationWithTemplate) {
    json baseConfig = {
        {"sampling_rate", 1000},
        {"precision", "high"}
    };
    
    std::string templateId = manager_->createConfigTemplate("Sensor Template", "sensor", baseConfig);
    
    std::unordered_map<std::string, json> variables = {
        {"sampling_rate", 500}
    };
    
    EXPECT_TRUE(manager_->registerDeviceWithTemplate("template_device", templateId, variables));
    
    auto device = manager_->getDevice("template_device");
    EXPECT_EQ(device.deviceId, "template_device");
    EXPECT_EQ(device.deviceType, "sensor");
    EXPECT_EQ(device.configuration["sampling_rate"], 500);
    EXPECT_EQ(device.configuration["precision"], "high");
}

TEST_F(EnhancedDeviceManagerTest, CompatibleTemplates) {
    // Create templates for different device types
    json sensorConfig = {{"type", "sensor"}};
    json actuatorConfig = {{"type", "actuator"}};
    
    std::string sensorTemplateId = manager_->createConfigTemplate("Sensor Template", "sensor", sensorConfig);
    std::string actuatorTemplateId = manager_->createConfigTemplate("Actuator Template", "actuator", actuatorConfig);
    
    // Register a sensor device
    DeviceInfo sensorDevice;
    sensorDevice.deviceId = "sensor_device";
    sensorDevice.deviceType = "sensor";
    sensorDevice.capabilities = {"temperature", "humidity"};
    EXPECT_TRUE(manager_->registerDevice(sensorDevice));
    
    auto compatibleTemplates = manager_->getCompatibleTemplates("sensor_device");
    EXPECT_EQ(compatibleTemplates.size(), 1);
    EXPECT_EQ(compatibleTemplates[0].templateId, sensorTemplateId);
}

TEST_F(EnhancedDeviceManagerTest, BulkOperations) {
    // Register multiple devices
    std::vector<std::string> deviceIds = {"device1", "device2", "device3"};
    
    for (const auto& deviceId : deviceIds) {
        DeviceInfo device;
        device.deviceId = deviceId;
        device.deviceType = "sensor";
        EXPECT_TRUE(manager_->registerDevice(device));
    }
    
    // Start bulk configuration update
    json newConfig = {{"new_setting", "value"}};
    std::string operationId = manager_->startBulkOperation("update_config", deviceIds, newConfig);
    EXPECT_FALSE(operationId.empty());
    
    // Check operation status
    json status = manager_->getBulkOperationStatus(operationId);
    EXPECT_EQ(status["operationId"], operationId);
    EXPECT_EQ(status["operationType"], "update_config");
    EXPECT_EQ(status["totalDevices"], 3);
    
    auto activeOperations = manager_->getActiveBulkOperations();
    EXPECT_FALSE(activeOperations.empty());
    
    // Wait a bit for operation to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Cancel operation
    EXPECT_TRUE(manager_->cancelBulkOperation(operationId));
}

TEST_F(EnhancedDeviceManagerTest, BulkRegistration) {
    std::vector<std::pair<std::string, json>> devices = {
        {"bulk_device1", json{{"deviceType", "sensor"}, {"deviceName", "Bulk Sensor 1"}}},
        {"bulk_device2", json{{"deviceType", "actuator"}, {"deviceName", "Bulk Actuator 1"}}},
        {"bulk_device3", json{{"deviceType", "sensor"}, {"deviceName", "Bulk Sensor 2"}}}
    };
    
    EXPECT_TRUE(manager_->bulkRegisterDevices(devices));
    
    // Verify devices were registered
    for (const auto& [deviceId, config] : devices) {
        auto device = manager_->getDevice(deviceId);
        EXPECT_EQ(device.deviceId, deviceId);
        EXPECT_FALSE(device.deviceType.empty());
    }
}

TEST_F(EnhancedDeviceManagerTest, DeviceDiscovery) {
    DeviceDiscoveryConfig config;
    config.enableUdpMulticast = true;
    config.enableMdns = true;
    config.discoveryInterval = std::chrono::milliseconds(1000);
    
    EXPECT_TRUE(manager_->startDeviceDiscovery(config));
    EXPECT_TRUE(manager_->isDiscoveryActive());
    
    manager_->stopDeviceDiscovery();
    EXPECT_FALSE(manager_->isDiscoveryActive());
}

TEST_F(EnhancedDeviceManagerTest, AutoRegistration) {
    EXPECT_TRUE(manager_->enableAutoRegistration(true));
    EXPECT_TRUE(manager_->isAutoRegistrationEnabled());
    
    EXPECT_TRUE(manager_->enableAutoRegistration(false));
    EXPECT_FALSE(manager_->isAutoRegistrationEnabled());
}

TEST_F(EnhancedDeviceManagerTest, DeviceSearch) {
    // Register devices with different types and capabilities
    DeviceInfo sensor1;
    sensor1.deviceId = "sensor1";
    sensor1.deviceType = "temperature_sensor";
    sensor1.capabilities = {"temperature", "humidity"};
    sensor1.tags = {"indoor", "critical"};
    EXPECT_TRUE(manager_->registerDevice(sensor1));
    
    DeviceInfo sensor2;
    sensor2.deviceId = "sensor2";
    sensor2.deviceType = "pressure_sensor";
    sensor2.capabilities = {"pressure"};
    sensor2.tags = {"outdoor"};
    EXPECT_TRUE(manager_->registerDevice(sensor2));
    
    DeviceInfo actuator1;
    actuator1.deviceId = "actuator1";
    actuator1.deviceType = "motor";
    actuator1.capabilities = {"rotation", "speed_control"};
    actuator1.tags = {"critical"};
    EXPECT_TRUE(manager_->registerDevice(actuator1));
    
    // Test search by type
    auto temperatureSensors = manager_->findDevicesByType("temperature_sensor");
    EXPECT_EQ(temperatureSensors.size(), 1);
    EXPECT_EQ(temperatureSensors[0].deviceId, "sensor1");
    
    // Test search by capability
    auto temperatureDevices = manager_->findDevicesByCapability("temperature");
    EXPECT_EQ(temperatureDevices.size(), 1);
    EXPECT_EQ(temperatureDevices[0].deviceId, "sensor1");
    
    // Test search by tag
    auto criticalDevices = manager_->findDevicesByTag("critical");
    EXPECT_EQ(criticalDevices.size(), 2);
    
    // Set health status and test search by health
    DeviceHealthInfo health;
    health.deviceId = "sensor1";
    health.status = HealthStatus::WARNING;
    manager_->updateDeviceHealth("sensor1", health);
    
    auto warningDevices = manager_->findDevicesByHealthStatus(HealthStatus::WARNING);
    EXPECT_EQ(warningDevices.size(), 1);
    EXPECT_EQ(warningDevices[0].deviceId, "sensor1");
}

TEST_F(EnhancedDeviceManagerTest, EnhancedStatistics) {
    json stats = manager_->getEnhancedStatistics();
    EXPECT_TRUE(stats.contains("healthChecksPerformed"));
    EXPECT_TRUE(stats.contains("devicesDiscovered"));
    EXPECT_TRUE(stats.contains("bulkOperationsCompleted"));
    EXPECT_TRUE(stats.contains("groupsCreated"));
    EXPECT_TRUE(stats.contains("templatesCreated"));
}

class MockHealthChangeHandler {
public:
    MOCK_METHOD(void, onHealthChange, (const std::string& deviceId, HealthStatus oldStatus, HealthStatus newStatus));
};

class MockDeviceDiscoveryHandler {
public:
    MOCK_METHOD(void, onDeviceDiscovery, (const DeviceInfo& device, bool discovered));
};

class MockGroupChangeHandler {
public:
    MOCK_METHOD(void, onGroupChange, (const std::string& groupId, const std::string& action));
};

class MockBulkOperationHandler {
public:
    MOCK_METHOD(void, onBulkOperation, (const std::string& operationId, const std::vector<std::string>& deviceIds, bool success));
};

TEST_F(EnhancedDeviceManagerTest, EventHandlers) {
    MockHealthChangeHandler healthHandler;
    MockDeviceDiscoveryHandler discoveryHandler;
    MockGroupChangeHandler groupHandler;
    MockBulkOperationHandler bulkHandler;
    
    manager_->setHealthChangeHandler([&healthHandler](const std::string& deviceId, HealthStatus oldStatus, HealthStatus newStatus) {
        healthHandler.onHealthChange(deviceId, oldStatus, newStatus);
    });
    
    manager_->setDeviceDiscoveryHandler([&discoveryHandler](const DeviceInfo& device, bool discovered) {
        discoveryHandler.onDeviceDiscovery(device, discovered);
    });
    
    manager_->setGroupChangeHandler([&groupHandler](const std::string& groupId, const std::string& action) {
        groupHandler.onGroupChange(groupId, action);
    });
    
    manager_->setBulkOperationHandler([&bulkHandler](const std::string& operationId, const std::vector<std::string>& deviceIds, bool success) {
        bulkHandler.onBulkOperation(operationId, deviceIds, success);
    });
    
    // Test group change handler
    EXPECT_CALL(groupHandler, onGroupChange(_, "created"))
        .Times(1);
    
    std::string groupId = manager_->createDeviceGroup("Test Group");
    EXPECT_FALSE(groupId.empty());
}

// Factory tests
class EnhancedDeviceManagerFactoryTest : public ::testing::Test {};

TEST_F(EnhancedDeviceManagerFactoryTest, CreateManager) {
    json config = {{"test", "value"}};
    auto manager = EnhancedDeviceManagerFactory::createManager(config);
    
    EXPECT_NE(manager, nullptr);
    EXPECT_FALSE(manager->isHealthMonitoringActive());
}

TEST_F(EnhancedDeviceManagerFactoryTest, CreateManagerWithHealthMonitoring) {
    auto manager = EnhancedDeviceManagerFactory::createManagerWithHealthMonitoring(
        json{}, std::chrono::milliseconds(10000));
    
    EXPECT_NE(manager, nullptr);
    EXPECT_TRUE(manager->isHealthMonitoringActive());
    
    manager->stopHealthMonitoring();
}

TEST_F(EnhancedDeviceManagerFactoryTest, CreateManagerWithDiscovery) {
    DeviceDiscoveryConfig discoveryConfig;
    discoveryConfig.discoveryInterval = std::chrono::milliseconds(5000);
    
    auto manager = EnhancedDeviceManagerFactory::createManagerWithDiscovery(json{}, discoveryConfig);
    
    EXPECT_NE(manager, nullptr);
    EXPECT_TRUE(manager->isDiscoveryActive());
    
    manager->stopDeviceDiscovery();
}
