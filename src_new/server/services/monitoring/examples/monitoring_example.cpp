#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "hydrogen/server/services/device_monitoring_service.h"
#include "src/server/src/services/device_monitoring_service_impl.h"

using namespace hydrogen::server::services;
using namespace std::chrono_literals;
using json = nlohmann::json;

// Global flag for graceful shutdown
std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    running.store(false);
}

class MonitoringExample {
public:
    MonitoringExample() {
        // Set up logging
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        
        // Create monitoring service
        monitoringService_ = std::make_unique<DeviceMonitoringServiceImpl>();
        
        // Configure the service
        json config = {
            {"metricsInterval", 1000},        // 1 second
            {"healthCheckInterval", 5000},    // 5 seconds
            {"alertCheckInterval", 2000},     // 2 seconds
            {"maxSubscriptions", 100},
            {"metricsCollector", {
                {"maxHistorySize", 1000},
                {"metricsRetention", 3600000}  // 1 hour
            }},
            {"healthMonitor", {
                {"healthCheckTimeout", 10000},
                {"healthScoreThreshold", 0.8}
            }}
        };
        
        monitoringService_->setConfiguration(config);
    }
    
    ~MonitoringExample() {
        if (monitoringService_ && monitoringService_->isRunning()) {
            monitoringService_->stop();
        }
    }
    
    void run() {
        std::cout << "=== Hydrogen Device Monitoring Service Example ===" << std::endl;
        std::cout << "This example demonstrates the real-time monitoring capabilities." << std::endl;
        std::cout << "Press Ctrl+C to stop the example." << std::endl << std::endl;
        
        // Start the monitoring service
        if (!monitoringService_->start()) {
            std::cerr << "Failed to start monitoring service!" << std::endl;
            return;
        }
        
        std::cout << "✓ Monitoring service started successfully" << std::endl;
        
        // Demonstrate device metrics subscription
        demonstrateDeviceMetricsSubscription();
        
        // Demonstrate system metrics subscription
        demonstrateSystemMetricsSubscription();
        
        // Demonstrate performance metrics retrieval
        demonstratePerformanceMetricsRetrieval();
        
        // Demonstrate health monitoring
        demonstrateHealthMonitoring();
        
        // Main loop - keep the example running
        std::cout << "\n--- Monitoring Service Running ---" << std::endl;
        std::cout << "Real-time metrics and health data will be displayed below:" << std::endl;
        
        auto startTime = std::chrono::steady_clock::now();
        while (running.load()) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);
            
            // Display periodic status updates
            if (elapsed.count() % 10 == 0 && elapsed.count() > 0) {
                displayServiceStatus(elapsed.count());
            }
            
            std::this_thread::sleep_for(1s);
        }
        
        // Cleanup
        cleanup();
        std::cout << "\n✓ Example completed successfully" << std::endl;
    }

private:
    std::unique_ptr<DeviceMonitoringServiceImpl> monitoringService_;
    std::vector<std::string> subscriptionIds_;
    
    void demonstrateDeviceMetricsSubscription() {
        std::cout << "\n--- Device Metrics Subscription Demo ---" << std::endl;
        
        // Create metrics subscription
        MetricsSubscription subscription;
        subscription.metrics = {"responseTime", "throughput", "errorRate"};
        subscription.interval = 2000ms;  // 2 seconds
        subscription.includeTimestamp = true;
        subscription.includeMetadata = true;
        
        // Set up callback for device metrics
        auto deviceMetricsCallback = [](const json& metrics) {
            std::cout << "📊 Device Metrics Received: " << std::endl;
            std::cout << "   Timestamp: " << metrics.value("timestamp", "N/A") << std::endl;
            std::cout << "   Metrics: " << metrics.dump(2) << std::endl;
        };
        
        // Subscribe to metrics for multiple test devices
        std::vector<std::string> testDevices = {
            "device_001", "device_002", "device_003"
        };
        
        for (const auto& deviceId : testDevices) {
            std::string subscriptionId = monitoringService_->subscribeToDeviceMetrics(
                deviceId, subscription, deviceMetricsCallback);
            
            if (!subscriptionId.empty()) {
                subscriptionIds_.push_back(subscriptionId);
                std::cout << "✓ Subscribed to metrics for device: " << deviceId 
                         << " (ID: " << subscriptionId << ")" << std::endl;
            } else {
                std::cout << "✗ Failed to subscribe to metrics for device: " << deviceId << std::endl;
            }
        }
    }
    
    void demonstrateSystemMetricsSubscription() {
        std::cout << "\n--- System Metrics Subscription Demo ---" << std::endl;
        
        // Create system metrics subscription
        SystemMetricsSubscription systemSubscription;
        systemSubscription.metricTypes = {"performance", "health", "resources"};
        systemSubscription.interval = 5000ms;  // 5 seconds
        systemSubscription.includeDeviceBreakdown = true;
        systemSubscription.includeProtocolBreakdown = true;
        
        // Set up callback for system metrics
        auto systemMetricsCallback = [](const json& metrics) {
            std::cout << "🖥️  System Metrics Received: " << std::endl;
            std::cout << "   Total Devices: " << metrics.value("totalDevices", 0) << std::endl;
            std::cout << "   Active Devices: " << metrics.value("activeDevices", 0) << std::endl;
            std::cout << "   System Health Score: " << metrics.value("systemHealthScore", 0.0) << std::endl;
            std::cout << "   Full Data: " << metrics.dump(2) << std::endl;
        };
        
        // Subscribe to system metrics
        std::string systemSubscriptionId = monitoringService_->subscribeToSystemMetrics(
            systemSubscription, systemMetricsCallback);
        
        if (!systemSubscriptionId.empty()) {
            subscriptionIds_.push_back(systemSubscriptionId);
            std::cout << "✓ Subscribed to system metrics (ID: " << systemSubscriptionId << ")" << std::endl;
        } else {
            std::cout << "✗ Failed to subscribe to system metrics" << std::endl;
        }
    }
    
    void demonstratePerformanceMetricsRetrieval() {
        std::cout << "\n--- Performance Metrics Retrieval Demo ---" << std::endl;
        
        // Wait a moment for some metrics to be collected
        std::this_thread::sleep_for(3s);
        
        // Retrieve device performance metrics
        TimeRange timeRange = TimeRange::lastHour();
        MetricsAggregation aggregation;
        aggregation.type = MetricsAggregation::Type::AVERAGE;
        aggregation.window = 60000ms;  // 1 minute window
        
        DevicePerformanceMetrics deviceMetrics = monitoringService_->getDevicePerformanceMetrics(
            "device_001", timeRange, aggregation);
        
        std::cout << "📈 Device Performance Metrics for device_001:" << std::endl;
        std::cout << "   Device ID: " << deviceMetrics.deviceId << std::endl;
        std::cout << "   Response Time (avg): " << deviceMetrics.responseTime.averageMs << "ms" << std::endl;
        std::cout << "   Throughput (req/s): " << deviceMetrics.throughput.requestsPerSecond << std::endl;
        std::cout << "   Error Rate: " << deviceMetrics.errors.errorPercentage << "%" << std::endl;
        
        // Retrieve system performance metrics
        SystemPerformanceMetrics systemMetrics = monitoringService_->getSystemPerformanceMetrics(
            timeRange, aggregation);
        
        std::cout << "🌐 System Performance Metrics:" << std::endl;
        std::cout << "   Total Devices: " << systemMetrics.totalDevices << std::endl;
        std::cout << "   Active Devices: " << systemMetrics.activeDevices << std::endl;
        std::cout << "   System Health Score: " << systemMetrics.systemHealthScore << std::endl;
    }
    
    void demonstrateHealthMonitoring() {
        std::cout << "\n--- Health Monitoring Demo ---" << std::endl;
        
        // Get device health report
        HealthReportOptions healthOptions;
        healthOptions.includeHistory = false;
        healthOptions.includeRecommendations = true;
        healthOptions.includeDiagnostics = false;
        
        DeviceHealthReport deviceHealth = monitoringService_->getDeviceHealthReport(
            "device_001", healthOptions);
        
        std::cout << "🏥 Device Health Report for device_001:" << std::endl;
        std::cout << "   Overall Health: " << static_cast<int>(deviceHealth.overallHealth) << std::endl;
        std::cout << "   Health Indicators: " << deviceHealth.indicators.size() << std::endl;
        
        for (const auto& indicator : deviceHealth.indicators) {
            std::cout << "   - " << indicator.name << ": " << indicator.value 
                     << " " << indicator.unit << " (Status: " << static_cast<int>(indicator.status) << ")" << std::endl;
        }
        
        // Get system health report
        SystemHealthReport systemHealth = monitoringService_->getSystemHealthReport(healthOptions);
        
        std::cout << "🏥 System Health Report:" << std::endl;
        std::cout << "   Overall Health: " << static_cast<int>(systemHealth.overallHealth) << std::endl;
        std::cout << "   System Indicators: " << systemHealth.indicators.size() << std::endl;
        std::cout << "   Device Reports: " << systemHealth.deviceReports.size() << std::endl;
    }
    
    void displayServiceStatus(int elapsedSeconds) {
        std::cout << "\n⏱️  Service Status Update (Running for " << elapsedSeconds << " seconds):" << std::endl;
        std::cout << "   Service Running: " << (monitoringService_->isRunning() ? "Yes" : "No") << std::endl;
        std::cout << "   Active Subscriptions: " << subscriptionIds_.size() << std::endl;
        
        // Get current configuration
        json config = monitoringService_->getConfiguration();
        std::cout << "   Metrics Interval: " << config.value("metricsInterval", 0) << "ms" << std::endl;
        std::cout << "   Health Check Interval: " << config.value("healthCheckInterval", 0) << "ms" << std::endl;
        
        // Get performance alerts
        AlertFilter alertFilter;
        alertFilter.activeOnly = true;
        
        auto alerts = monitoringService_->getPerformanceAlerts("", alertFilter);
        std::cout << "   Active Alerts: " << alerts.size() << std::endl;
    }
    
    void cleanup() {
        std::cout << "\n--- Cleaning Up ---" << std::endl;
        
        // Unsubscribe from all metrics
        for (const auto& subscriptionId : subscriptionIds_) {
            if (monitoringService_->unsubscribeFromMetrics(subscriptionId)) {
                std::cout << "✓ Unsubscribed from: " << subscriptionId << std::endl;
            } else {
                std::cout << "✗ Failed to unsubscribe from: " << subscriptionId << std::endl;
            }
        }
        
        subscriptionIds_.clear();
        
        // Stop the monitoring service
        if (monitoringService_->stop()) {
            std::cout << "✓ Monitoring service stopped successfully" << std::endl;
        } else {
            std::cout << "✗ Failed to stop monitoring service" << std::endl;
        }
    }
};

int main() {
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        MonitoringExample example;
        example.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
