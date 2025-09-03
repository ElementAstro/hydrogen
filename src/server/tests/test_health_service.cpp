#include <gtest/gtest.h>
#include "hydrogen/server/services/health_service.h"
#include <memory>
#include <chrono>

using namespace hydrogen::server::services;

class HealthServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create health service instance
        HealthServiceFactory factory;
        std::unordered_map<std::string, std::string> config;
        config["serviceName"] = "TestHealthService";

        auto service = factory.createService("HealthService", config);
        service_ = std::unique_ptr<IHealthService>(dynamic_cast<IHealthService*>(service.release()));
        ASSERT_NE(service_, nullptr);

        // Initialize the service
        ASSERT_TRUE(service_->initialize());
        ASSERT_TRUE(service_->start());
    }

    void TearDown() override {
        if (service_) {
            service_->stop();
        }
    }

    std::unique_ptr<IHealthService> service_;
};

TEST_F(HealthServiceTest, ServiceInitialization) {
    // Test basic service functionality
    EXPECT_TRUE(service_->isSystemHealthy());

    // Get overall health status
    auto healthStatus = service_->getOverallHealthStatus();
    EXPECT_NE(healthStatus, HealthStatus::UNKNOWN);

    // Get health summary
    auto summary = service_->getHealthSummary();
    EXPECT_FALSE(summary.empty());
}

TEST_F(HealthServiceTest, HealthCheckRegistration) {
    // Create health check configuration
    HealthCheckConfig config;
    config.checkId = "test_component";
    config.checkName = "Test Component Check";
    config.component = "test_component";
    config.interval = std::chrono::seconds(30);
    config.timeout = std::chrono::seconds(5);
    config.retryAttempts = 3;
    config.retryDelay = std::chrono::milliseconds(1000);
    config.enabled = true;

    // Define a health check function
    auto healthCheckFunction = []() -> HealthCheckResult {
        HealthCheckResult result;
        result.checkId = "test_component";
        result.status = HealthStatus::HEALTHY;
        result.message = "Component is healthy";
        result.timestamp = std::chrono::system_clock::now();
        result.executionTime = std::chrono::milliseconds(10);
        return result;
    };

    bool registered = service_->registerHealthCheck(config, healthCheckFunction);
    EXPECT_TRUE(registered);

    // Execute the health check
    auto result = service_->executeHealthCheck("test_component");
    EXPECT_EQ(result.checkId, "test_component");
    EXPECT_EQ(result.status, HealthStatus::HEALTHY);
    EXPECT_EQ(result.message, "Component is healthy");

    // Unregister the health check
    bool unregistered = service_->unregisterHealthCheck("test_component");
    EXPECT_TRUE(unregistered);
}

TEST_F(HealthServiceTest, OverallHealthStatus) {
    // Get overall health status
    auto healthStatus = service_->getOverallHealthStatus();
    EXPECT_NE(healthStatus, HealthStatus::UNKNOWN);

    // Get component health status
    auto componentStatus = service_->getComponentHealthStatus();
    EXPECT_GE(componentStatus.size(), 0);

    // Check if system is healthy
    bool isHealthy = service_->isSystemHealthy();
    (void)isHealthy; // Suppress unused variable warning
}

TEST_F(HealthServiceTest, SystemMetrics) {
    // Get system metrics
    auto metrics = service_->getSystemMetrics();
    EXPECT_GE(metrics.uptime.count(), 0);

    // Start metrics collection
    bool started = service_->startSystemMetricsCollection(std::chrono::seconds(1));
    EXPECT_TRUE(started);

    // Get metrics history
    auto history = service_->getSystemMetricsHistory(std::chrono::minutes(1));
    EXPECT_GE(history.size(), 0);

    // Stop metrics collection
    bool stopped = service_->stopSystemMetricsCollection();
    EXPECT_TRUE(stopped);
}

TEST_F(HealthServiceTest, HealthReporting) {
    // Get health summary
    auto summary = service_->getHealthSummary();
    EXPECT_FALSE(summary.empty());

    // Get component health status
    auto componentStatus = service_->getComponentHealthStatus();
    EXPECT_GE(componentStatus.size(), 0);

    // Generate health report
    bool reportGenerated = service_->generateHealthReport("./test_data/health_report.json");
    EXPECT_TRUE(reportGenerated);
}
