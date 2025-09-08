#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

#include "hydrogen/server/services/device_monitoring_service.h"
#include "../../../src/server/src/services/device_monitoring_service_impl.h"

using namespace hydrogen::server::services;
using namespace std::chrono_literals;

class DeviceMonitoringServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<DeviceMonitoringServiceImpl>();
    }
    
    void TearDown() override {
        if (service_ && service_->isRunning()) {
            service_->stop();
        }
        service_.reset();
    }
    
    std::unique_ptr<DeviceMonitoringServiceImpl> service_;
};

TEST_F(DeviceMonitoringServiceTest, ServiceLifecycle) {
    // Test initial state
    EXPECT_FALSE(service_->isRunning());
    
    // Test start
    EXPECT_TRUE(service_->start());
    EXPECT_TRUE(service_->isRunning());
    
    // Test stop
    EXPECT_TRUE(service_->stop());
    EXPECT_FALSE(service_->isRunning());
}

TEST_F(DeviceMonitoringServiceTest, Configuration) {
    json config = {
        {"metricsInterval", 500},
        {"healthCheckInterval", 2000},
        {"maxSubscriptions", 100}
    };
    
    service_->setConfiguration(config);
    
    json retrievedConfig = service_->getConfiguration();
    EXPECT_EQ(retrievedConfig["metricsInterval"], 500);
    EXPECT_EQ(retrievedConfig["healthCheckInterval"], 2000);
    EXPECT_EQ(retrievedConfig["maxSubscriptions"], 100);
}

TEST_F(DeviceMonitoringServiceTest, DeviceMetricsSubscription) {
    ASSERT_TRUE(service_->start());
    
    bool callbackCalled = false;
    json receivedMetrics;
    
    MetricsSubscription subscription;
    subscription.metrics = {"responseTime", "throughput"};
    subscription.interval = 100ms;
    subscription.includeTimestamp = true;
    
    auto callback = [&](const json& metrics) {
        callbackCalled = true;
        receivedMetrics = metrics;
    };
    
    std::string subscriptionId = service_->subscribeToDeviceMetrics(
        "test_device_001", subscription, callback);
    
    EXPECT_FALSE(subscriptionId.empty());
    EXPECT_TRUE(subscriptionId.find("sub_") == 0);
    
    // Wait a bit for potential callback
    std::this_thread::sleep_for(200ms);
    
    // Unsubscribe
    EXPECT_TRUE(service_->unsubscribeFromMetrics(subscriptionId));
    
    // Try to unsubscribe again (should fail)
    EXPECT_FALSE(service_->unsubscribeFromMetrics(subscriptionId));
}

TEST_F(DeviceMonitoringServiceTest, SystemMetricsSubscription) {
    ASSERT_TRUE(service_->start());
    
    bool callbackCalled = false;
    json receivedMetrics;
    
    SystemMetricsSubscription subscription;
    subscription.metricTypes = {"performance", "health"};
    subscription.interval = 100ms;
    subscription.includeDeviceBreakdown = true;
    
    auto callback = [&](const json& metrics) {
        callbackCalled = true;
        receivedMetrics = metrics;
    };
    
    std::string subscriptionId = service_->subscribeToSystemMetrics(subscription, callback);
    
    EXPECT_FALSE(subscriptionId.empty());
    EXPECT_TRUE(subscriptionId.find("sub_") == 0);
    
    // Wait a bit for potential callback
    std::this_thread::sleep_for(200ms);
    
    // Unsubscribe
    EXPECT_TRUE(service_->unsubscribeFromMetrics(subscriptionId));
}

TEST_F(DeviceMonitoringServiceTest, InvalidDeviceId) {
    ASSERT_TRUE(service_->start());
    
    MetricsSubscription subscription;
    auto callback = [](const json&) {};
    
    // Test empty device ID
    std::string subscriptionId = service_->subscribeToDeviceMetrics("", subscription, callback);
    EXPECT_TRUE(subscriptionId.empty());
    
    // Test very long device ID
    std::string longDeviceId(300, 'a');
    subscriptionId = service_->subscribeToDeviceMetrics(longDeviceId, subscription, callback);
    EXPECT_TRUE(subscriptionId.empty());
}

TEST_F(DeviceMonitoringServiceTest, NullCallback) {
    ASSERT_TRUE(service_->start());
    
    MetricsSubscription subscription;
    
    // Test null callback for device metrics
    std::string subscriptionId = service_->subscribeToDeviceMetrics(
        "test_device", subscription, nullptr);
    EXPECT_TRUE(subscriptionId.empty());
    
    // Test null callback for system metrics
    SystemMetricsSubscription systemSubscription;
    subscriptionId = service_->subscribeToSystemMetrics(systemSubscription, nullptr);
    EXPECT_TRUE(subscriptionId.empty());
}

TEST_F(DeviceMonitoringServiceTest, GetDevicePerformanceMetrics) {
    ASSERT_TRUE(service_->start());
    
    TimeRange timeRange = TimeRange::lastHour();
    MetricsAggregation aggregation;
    aggregation.type = MetricsAggregation::Type::AVERAGE;
    aggregation.window = 60000ms;
    
    DevicePerformanceMetrics metrics = service_->getDevicePerformanceMetrics(
        "test_device_001", timeRange, aggregation);
    
    // Since this is a test with no real device, we expect default/empty metrics
    EXPECT_EQ(metrics.deviceId, "");
}

TEST_F(DeviceMonitoringServiceTest, GetSystemPerformanceMetrics) {
    ASSERT_TRUE(service_->start());
    
    TimeRange timeRange = TimeRange::lastHour();
    MetricsAggregation aggregation;
    aggregation.type = MetricsAggregation::Type::AVERAGE;
    
    SystemPerformanceMetrics metrics = service_->getSystemPerformanceMetrics(timeRange, aggregation);
    
    // Since this is a test with no real system data, we expect default metrics
    EXPECT_EQ(metrics.totalDevices, 0);
}

TEST_F(DeviceMonitoringServiceTest, GetPerformanceAlerts) {
    ASSERT_TRUE(service_->start());
    
    AlertFilter filter;
    filter.activeOnly = true;
    filter.deviceIds = {"test_device_001"};
    
    std::vector<PerformanceAlert> alerts = service_->getPerformanceAlerts("test_device_001", filter);
    
    // Since this is a test environment, we expect no alerts
    EXPECT_TRUE(alerts.empty());
}

TEST_F(DeviceMonitoringServiceTest, MultipleSubscriptions) {
    ASSERT_TRUE(service_->start());
    
    MetricsSubscription subscription;
    subscription.metrics = {"responseTime"};
    subscription.interval = 100ms;
    
    auto callback = [](const json&) {};
    
    // Create multiple subscriptions
    std::vector<std::string> subscriptionIds;
    for (int i = 0; i < 5; ++i) {
        std::string deviceId = "test_device_" + std::to_string(i);
        std::string subscriptionId = service_->subscribeToDeviceMetrics(deviceId, subscription, callback);
        EXPECT_FALSE(subscriptionId.empty());
        subscriptionIds.push_back(subscriptionId);
    }
    
    // Verify all subscriptions are unique
    std::set<std::string> uniqueIds(subscriptionIds.begin(), subscriptionIds.end());
    EXPECT_EQ(uniqueIds.size(), subscriptionIds.size());
    
    // Unsubscribe from all
    for (const auto& subscriptionId : subscriptionIds) {
        EXPECT_TRUE(service_->unsubscribeFromMetrics(subscriptionId));
    }
}

TEST_F(DeviceMonitoringServiceTest, ServiceRestartability) {
    // Start service
    EXPECT_TRUE(service_->start());
    EXPECT_TRUE(service_->isRunning());
    
    // Stop service
    EXPECT_TRUE(service_->stop());
    EXPECT_FALSE(service_->isRunning());
    
    // Restart service
    EXPECT_TRUE(service_->start());
    EXPECT_TRUE(service_->isRunning());
    
    // Stop again
    EXPECT_TRUE(service_->stop());
    EXPECT_FALSE(service_->isRunning());
}

// Test data structure serialization
TEST(MonitoringDataStructuresTest, TimeSeriesPointSerialization) {
    TimeSeriesPoint point;
    point.timestamp = std::chrono::system_clock::now();
    point.value = 42.5;
    point.metadata = json{{"source", "test"}};
    
    json j = point.toJson();
    TimeSeriesPoint deserializedPoint = TimeSeriesPoint::fromJson(j);
    
    EXPECT_EQ(point.value, deserializedPoint.value);
    EXPECT_EQ(point.metadata, deserializedPoint.metadata);
}

TEST(MonitoringDataStructuresTest, ResponseTimeMetricsSerialization) {
    ResponseTimeMetrics metrics;
    metrics.averageMs = 50.0;
    metrics.p95Ms = 95.0;
    metrics.totalRequests = 1000;
    
    json j = metrics.toJson();
    ResponseTimeMetrics deserializedMetrics = ResponseTimeMetrics::fromJson(j);
    
    EXPECT_EQ(metrics.averageMs, deserializedMetrics.averageMs);
    EXPECT_EQ(metrics.p95Ms, deserializedMetrics.p95Ms);
    EXPECT_EQ(metrics.totalRequests, deserializedMetrics.totalRequests);
}

TEST(MonitoringDataStructuresTest, HealthIndicatorSerialization) {
    HealthIndicator indicator;
    indicator.name = "cpu_usage";
    indicator.description = "CPU usage percentage";
    indicator.status = HealthStatus::HEALTHY;
    indicator.value = 25.5;
    indicator.threshold = 80.0;
    indicator.unit = "%";
    indicator.lastCheck = std::chrono::system_clock::now();
    
    json j = indicator.toJson();
    HealthIndicator deserializedIndicator = HealthIndicator::fromJson(j);
    
    EXPECT_EQ(indicator.name, deserializedIndicator.name);
    EXPECT_EQ(indicator.description, deserializedIndicator.description);
    EXPECT_EQ(indicator.status, deserializedIndicator.status);
    EXPECT_EQ(indicator.value, deserializedIndicator.value);
    EXPECT_EQ(indicator.threshold, deserializedIndicator.threshold);
    EXPECT_EQ(indicator.unit, deserializedIndicator.unit);
}
