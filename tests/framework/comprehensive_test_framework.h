#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <random>
#include <fstream>
#include <filesystem>

namespace hydrogen {
namespace testing {

using json = nlohmann::json;

/**
 * @brief Test configuration and settings
 */
struct TestConfig {
    bool enablePerformanceTesting = false;
    bool enableIntegrationTesting = false;
    bool enableStressTesting = false;
    bool enableConcurrencyTesting = false;
    bool enableNetworkTesting = false;
    
    std::chrono::milliseconds defaultTimeout{5000};
    std::chrono::milliseconds performanceTimeout{30000};
    std::chrono::milliseconds integrationTimeout{60000};
    
    size_t stressTestIterations = 1000;
    size_t concurrencyThreadCount = 4;
    
    std::string testDataDirectory = "test_data";
    std::string tempDirectory = "temp_test";
    std::string logDirectory = "test_logs";
    
    // Network testing settings
    std::string testServerHost = "localhost";
    uint16_t testServerPort = 8080;
    bool useRealNetwork = false;
    
    // Performance thresholds
    std::chrono::milliseconds maxResponseTime{1000};
    size_t maxMemoryUsageMB = 100;
    double maxCpuUsagePercent = 80.0;
};

/**
 * @brief Test result and metrics collection
 */
struct TestMetrics {
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
    std::chrono::milliseconds duration{0};
    
    size_t memoryUsageBytes = 0;
    double cpuUsagePercent = 0.0;
    size_t networkBytesTransferred = 0;
    
    size_t operationsPerformed = 0;
    size_t errorsEncountered = 0;
    
    json customMetrics;
    
    void startMeasurement();
    void stopMeasurement();
    json toJson() const;
};

/**
 * @brief Enhanced test fixture base class
 */
class ComprehensiveTestFixture : public ::testing::Test {
public:
    ComprehensiveTestFixture();
    virtual ~ComprehensiveTestFixture();

protected:
    void SetUp() override;
    void TearDown() override;
    
    // Configuration
    TestConfig& getConfig() { return config_; }
    const TestConfig& getConfig() const { return config_; }
    
    // Metrics
    TestMetrics& getMetrics() { return metrics_; }
    const TestMetrics& getMetrics() const { return metrics_; }
    
    // Test utilities
    std::string generateTestId() const;
    std::string createTempFile(const std::string& content = "");
    std::string createTempDirectory();
    void cleanupTempResources();
    
    // Timing utilities
    void startTimer();
    void stopTimer();
    std::chrono::milliseconds getElapsedTime() const;
    
    // Assertion helpers
    void expectWithinTimeout(std::function<bool()> condition, 
                           std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
    void expectEventually(std::function<bool()> condition, 
                         std::chrono::milliseconds timeout = std::chrono::milliseconds{10000},
                         std::chrono::milliseconds interval = std::chrono::milliseconds{100});
    
    // Performance testing
    void measurePerformance(std::function<void()> operation, const std::string& operationName = "");
    void benchmarkOperation(std::function<void()> operation, size_t iterations, const std::string& name = "");
    
    // Concurrency testing
    void runConcurrentTest(std::function<void(int)> testFunction, size_t threadCount = 4);
    void runStressTest(std::function<void(int)> testFunction, size_t iterations = 1000);
    
    // Data generation
    json generateTestData(const std::string& schema = "");
    std::vector<uint8_t> generateRandomData(size_t size);
    std::string generateRandomString(size_t length);
    
    // Network testing helpers
    bool isNetworkAvailable() const;
    bool canConnectToTestServer() const;
    void skipIfNetworkUnavailable() const;
    
    // Logging and reporting
    void logTestInfo(const std::string& message);
    void logTestWarning(const std::string& message);
    void logTestError(const std::string& message);
    void saveTestReport(const std::string& filename = "");

private:
    TestConfig config_;
    TestMetrics metrics_;
    
    std::vector<std::string> tempFiles_;
    std::vector<std::string> tempDirectories_;
    
    std::chrono::high_resolution_clock::time_point timerStart_;
    mutable std::mutex timerMutex_;
    
    mutable std::mt19937 randomGenerator_;
    
    void initializeTestEnvironment();
    void loadTestConfiguration();
    void setupLogging();
};

/**
 * @brief Mock factory for creating test doubles
 */
class MockFactory {
public:
    // Device mocks
    static std::unique_ptr<class MockDevice> createMockDevice(const std::string& deviceId = "");
    static std::unique_ptr<class MockDeviceManager> createMockDeviceManager();
    
    // Communication mocks
    static std::unique_ptr<class MockWebSocketClient> createMockWebSocketClient();
    static std::unique_ptr<class MockWebSocketServer> createMockWebSocketServer();
    static std::unique_ptr<class MockMessageProcessor> createMockMessageProcessor();
    
    // Protocol mocks
    static std::unique_ptr<class MockGrpcClient> createMockGrpcClient();
    static std::unique_ptr<class MockMqttClient> createMockMqttClient();
    static std::unique_ptr<class MockZmqClient> createMockZmqClient();
    
    // Configuration for mocks
    static void configureMockDefaults();
    static void resetAllMocks();
};

/**
 * @brief Test data manager for handling test datasets
 */
class TestDataManager {
public:
    static TestDataManager& getInstance();
    
    // Data loading
    json loadTestData(const std::string& filename);
    void saveTestData(const std::string& filename, const json& data);
    
    // Schema validation
    bool validateTestData(const json& data, const std::string& schemaFile);
    json generateDataFromSchema(const std::string& schemaFile);
    
    // Test datasets
    json getDeviceTestData(const std::string& deviceType = "");
    json getMessageTestData(const std::string& messageType = "");
    json getConfigurationTestData(const std::string& configType = "");
    
    // Data cleanup
    void cleanupTestData();
    void archiveTestResults(const std::string& testSuite);

private:
    TestDataManager() = default;
    std::string testDataPath_;
    std::mutex dataMutex_;
    
    void initializeDataDirectory();
};

/**
 * @brief Performance testing utilities
 */
class PerformanceTester {
public:
    struct BenchmarkResult {
        std::string name;
        size_t iterations;
        std::chrono::milliseconds totalTime;
        std::chrono::microseconds averageTime;
        std::chrono::microseconds minTime;
        std::chrono::microseconds maxTime;
        double operationsPerSecond;
        json customMetrics;
        
        json toJson() const;
    };
    
    static BenchmarkResult benchmark(const std::string& name,
                                   std::function<void()> operation,
                                   size_t iterations = 1000);
    
    static BenchmarkResult benchmarkWithWarmup(const std::string& name,
                                              std::function<void()> operation,
                                              size_t warmupIterations = 100,
                                              size_t benchmarkIterations = 1000);
    
    static void comparePerformance(const std::vector<std::pair<std::string, std::function<void()>>>& operations,
                                 size_t iterations = 1000);
    
    // Memory profiling
    static size_t getCurrentMemoryUsage();
    static size_t measureMemoryUsage(std::function<void()> operation);
    
    // CPU profiling
    static double getCurrentCpuUsage();
    static double measureCpuUsage(std::function<void()> operation, std::chrono::milliseconds duration);
};

/**
 * @brief Integration testing utilities
 */
class IntegrationTester {
public:
    // Server management
    static bool startTestServer(uint16_t port = 8080);
    static void stopTestServer();
    static bool isTestServerRunning();
    
    // Client-server testing
    static void runClientServerTest(std::function<void()> serverSetup,
                                  std::function<void()> clientTest,
                                  std::function<void()> cleanup = nullptr);
    
    // Multi-component testing
    static void runMultiComponentTest(const std::vector<std::function<void()>>& componentTests);
    
    // End-to-end testing
    static void runEndToEndTest(const std::string& testScenario,
                              const json& testConfiguration);
    
    // Network simulation
    static void simulateNetworkLatency(std::chrono::milliseconds latency);
    static void simulateNetworkPacketLoss(double lossPercentage);
    static void resetNetworkSimulation();

private:
    static std::atomic<bool> testServerRunning_;
    static std::thread testServerThread_;
    static uint16_t testServerPort_;
};

/**
 * @brief Test suite manager for organizing and running tests
 */
class TestSuiteManager {
public:
    enum class TestType {
        UNIT,
        INTEGRATION,
        PERFORMANCE,
        STRESS,
        REGRESSION,
        SMOKE
    };
    
    static void registerTestSuite(const std::string& name, TestType type);
    static void runTestSuite(const std::string& name);
    static void runTestsByType(TestType type);
    static void runAllTests();
    
    // Test filtering
    static void setTestFilter(const std::string& filter);
    static void excludeTests(const std::vector<std::string>& testNames);
    
    // Reporting
    static json generateTestReport();
    static void saveTestReport(const std::string& filename);
    static void printTestSummary();

private:
    static std::map<std::string, TestType> registeredSuites_;
    static std::string currentFilter_;
    static std::vector<std::string> excludedTests_;
};

/**
 * @brief Convenience macros for comprehensive testing
 */
#define HYDROGEN_TEST_FIXTURE(TestClass) \
    class TestClass : public hydrogen::testing::ComprehensiveTestFixture

#define HYDROGEN_PERFORMANCE_TEST(TestClass, TestName) \
    TEST_F(TestClass, TestName) { \
        if (!getConfig().enablePerformanceTesting) { \
            GTEST_SKIP() << "Performance testing disabled"; \
        } \
        startTimer(); \
        /* Test implementation */ \
        stopTimer(); \
        measurePerformance([this]() { /* Test code */ }, #TestName); \
    }

#define HYDROGEN_INTEGRATION_TEST(TestClass, TestName) \
    TEST_F(TestClass, TestName) { \
        if (!getConfig().enableIntegrationTesting) { \
            GTEST_SKIP() << "Integration testing disabled"; \
        } \
        skipIfNetworkUnavailable(); \
        /* Test implementation */ \
    }

#define HYDROGEN_STRESS_TEST(TestClass, TestName) \
    TEST_F(TestClass, TestName) { \
        if (!getConfig().enableStressTesting) { \
            GTEST_SKIP() << "Stress testing disabled"; \
        } \
        runStressTest([this](int iteration) { \
            /* Test implementation */ \
        }, getConfig().stressTestIterations); \
    }

#define EXPECT_WITHIN_TIMEOUT(condition, timeout) \
    expectWithinTimeout([&]() { return condition; }, timeout)

#define EXPECT_EVENTUALLY(condition) \
    expectEventually([&]() { return condition; })

#define BENCHMARK_OPERATION(operation, name) \
    measurePerformance([&]() { operation; }, name)

} // namespace testing
} // namespace hydrogen
