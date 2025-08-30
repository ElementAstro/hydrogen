#include <gtest/gtest.h>
#include "astrocomm/server/services/health_service.h"
#include <memory>
#include <chrono>
#include <thread>

using namespace astrocomm::server::services;

class HealthServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create health service instance
        service_ = HealthServiceFactory::createService("TestHealthService");
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
    EXPECT_TRUE(service_->isInitialized());
    EXPECT_TRUE(service_->isRunning());
    EXPECT_EQ(service_->getName(), "TestHealthService");
}

TEST_F(HealthServiceTest, HealthCheckRegistration) {
    std::string componentId = "test_component";
    
    // Register a health check
    auto healthCheckFunction = []() -> HealthCheck {
        HealthCheck check;
        check.componentId = "test_component";
        check.status = HealthStatus::HEALTHY;
        check.message = "Component is healthy";
        return check;
    };
    
    bool registered = service_->registerHealthCheck(componentId, healthCheckFunction);
    EXPECT_TRUE(registered);
    
    // Perform the health check
    bool performed = service_->performHealthCheck(componentId);
    EXPECT_TRUE(performed);
    
    // Get the health check result
    auto healthCheck = service_->getHealthCheck(componentId);
    EXPECT_EQ(healthCheck.componentId, componentId);
    EXPECT_EQ(healthCheck.status, HealthStatus::HEALTHY);
    EXPECT_EQ(healthCheck.message, "Component is healthy");
    
    // Unregister the health check
    bool unregistered = service_->unregisterHealthCheck(componentId);
    EXPECT_TRUE(unregistered);
}

TEST_F(HealthServiceTest, OverallHealthStatus) {
    // Initially should be unknown (no health checks)
    auto initialStatus = service_->getOverallHealth();
    EXPECT_EQ(initialStatus, HealthStatus::UNKNOWN);
    
    // Register a healthy component
    service_->registerHealthCheck("healthy_component", []() -> HealthCheck {
        HealthCheck check;
        check.componentId = "healthy_component";
        check.status = HealthStatus::HEALTHY;
        check.message = "All good";
        return check;
    });
    
    // Register a warning component
    service_->registerHealthCheck("warning_component", []() -> HealthCheck {
        HealthCheck check;
        check.componentId = "warning_component";
        check.status = HealthStatus::WARNING;
        check.message = "Minor issue";
        return check;
    });
    
    // Perform all health checks
    service_->performAllHealthChecks();
    
    // Overall status should be WARNING (worst status)
    auto overallStatus = service_->getOverallHealth();
    EXPECT_EQ(overallStatus, HealthStatus::WARNING);
}

TEST_F(HealthServiceTest, GetAllHealthChecks) {
    // Register multiple health checks
    service_->registerHealthCheck("component1", []() -> HealthCheck {
        HealthCheck check;
        check.componentId = "component1";
        check.status = HealthStatus::HEALTHY;
        check.message = "Component 1 OK";
        return check;
    });
    
    service_->registerHealthCheck("component2", []() -> HealthCheck {
        HealthCheck check;
        check.componentId = "component2";
        check.status = HealthStatus::WARNING;
        check.message = "Component 2 has warnings";
        return check;
    });
    
    // Perform all health checks
    service_->performAllHealthChecks();
    
    // Get all health checks
    auto allChecks = service_->getAllHealthChecks();
    EXPECT_EQ(allChecks.size(), 2);
    
    // Verify components are present
    bool found1 = false, found2 = false;
    for (const auto& check : allChecks) {
        if (check.componentId == "component1") {
            EXPECT_EQ(check.status, HealthStatus::HEALTHY);
            found1 = true;
        } else if (check.componentId == "component2") {
            EXPECT_EQ(check.status, HealthStatus::WARNING);
            found2 = true;
        }
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(HealthServiceTest, SystemMetrics) {
    // Update system metrics
    service_->updateSystemMetrics();
    
    // Get system metrics
    auto metrics = service_->getSystemMetrics();
    
    // Verify metrics are populated
    EXPECT_GE(metrics.cpuUsage, 0.0);
    EXPECT_GT(metrics.memoryUsage, 0);
    EXPECT_GT(metrics.uptime, 0);
}

TEST_F(HealthServiceTest, HealthCheckInterval) {
    // Set health check interval
    auto interval = std::chrono::seconds(10);
    service_->setHealthCheckInterval(interval);
    
    // Verify interval is set
    auto retrievedInterval = service_->getHealthCheckInterval();
    EXPECT_EQ(retrievedInterval, interval);
}

TEST_F(HealthServiceTest, HealthAlerts) {
    // Register a critical component
    service_->registerHealthCheck("critical_component", []() -> HealthCheck {
        HealthCheck check;
        check.componentId = "critical_component";
        check.status = HealthStatus::CRITICAL;
        check.message = "Critical failure";
        return check;
    });
    
    // Perform health check to trigger alert
    service_->performHealthCheck("critical_component");
    
    // Wait a bit for alert processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get active alerts
    auto alerts = service_->getActiveAlerts();
    EXPECT_GE(alerts.size(), 1);
    
    // Find the critical alert
    bool foundCriticalAlert = false;
    std::string alertId;
    for (const auto& alert : alerts) {
        if (alert.componentId == "critical_component" && alert.severity == AlertSeverity::CRITICAL) {
            foundCriticalAlert = true;
            alertId = alert.id;
            break;
        }
    }
    EXPECT_TRUE(foundCriticalAlert);
    
    // Acknowledge the alert
    if (!alertId.empty()) {
        bool acknowledged = service_->acknowledgeAlert(alertId);
        EXPECT_TRUE(acknowledged);
    }
}

TEST_F(HealthServiceTest, ServiceRestart) {
    // Stop the service
    EXPECT_TRUE(service_->stop());
    EXPECT_FALSE(service_->isRunning());
    
    // Restart the service
    EXPECT_TRUE(service_->restart());
    EXPECT_TRUE(service_->isRunning());
}

TEST_F(HealthServiceTest, InvalidOperations) {
    // Try to perform health check on non-existent component
    bool result = service_->performHealthCheck("non_existent_component");
    EXPECT_FALSE(result);
    
    // Try to unregister non-existent health check
    bool unregistered = service_->unregisterHealthCheck("non_existent_component");
    EXPECT_FALSE(unregistered);
    
    // Try to acknowledge non-existent alert
    bool acknowledged = service_->acknowledgeAlert("invalid_alert_id");
    EXPECT_FALSE(acknowledged);
    
    // Try to clear non-existent alert
    bool cleared = service_->clearAlert("invalid_alert_id");
    EXPECT_FALSE(cleared);
}
