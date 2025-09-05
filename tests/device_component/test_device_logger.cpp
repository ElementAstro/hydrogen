#include <gtest/gtest.h>
#include <hydrogen/device/device_logger.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

using namespace hydrogen::device;

class DeviceLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset logger to default state
        DeviceLogger::getInstance().setLogLevel(LogLevel::INFO);
        DeviceLogger::getInstance().setConsoleLogging(false); // Disable for tests
        DeviceLogger::getInstance().setLogFile(""); // Disable file logging
        DeviceLogger::getInstance().setLogCallback(nullptr);
    }

    void TearDown() override {
        // Clean up any test files
        std::remove("test_log.txt");
    }
};

TEST_F(DeviceLoggerTest, LogLevels) {
    std::vector<std::string> loggedMessages;
    
    // Set up callback to capture messages
    DeviceLogger::getInstance().setLogCallback(
        [&loggedMessages](LogLevel level, const std::string& deviceId, const std::string& message) {
            loggedMessages.push_back(logLevelToString(level) + ":" + deviceId + ":" + message);
        });
    
    DeviceLogger::getInstance().setLogLevel(LogLevel::WARN);
    
    // These should be logged (WARN and above)
    DeviceLogger::getInstance().warn("device1", "Warning message");
    DeviceLogger::getInstance().error("device1", "Error message");
    DeviceLogger::getInstance().critical("device1", "Critical message");
    
    // These should NOT be logged (below WARN)
    DeviceLogger::getInstance().trace("device1", "Trace message");
    DeviceLogger::getInstance().debug("device1", "Debug message");
    DeviceLogger::getInstance().info("device1", "Info message");
    
    EXPECT_EQ(loggedMessages.size(), 3);
    EXPECT_THAT(loggedMessages[0], testing::HasSubstr("WARN:device1:Warning message"));
    EXPECT_THAT(loggedMessages[1], testing::HasSubstr("ERROR:device1:Error message"));
    EXPECT_THAT(loggedMessages[2], testing::HasSubstr("CRITICAL:device1:Critical message"));
}

TEST_F(DeviceLoggerTest, FileLogging) {
    const std::string logFile = "test_log.txt";
    DeviceLogger::getInstance().setLogFile(logFile);
    DeviceLogger::getInstance().setLogLevel(LogLevel::DEBUG);
    
    DeviceLogger::getInstance().info("test_device", "Test message 1");
    DeviceLogger::getInstance().error("test_device", "Test message 2");
    
    DeviceLogger::getInstance().flush();
    
    // Read the log file
    std::ifstream file(logFile);
    ASSERT_TRUE(file.is_open());
    
    std::string line1, line2;
    std::getline(file, line1);
    std::getline(file, line2);
    
    EXPECT_THAT(line1, testing::HasSubstr("[INFO]"));
    EXPECT_THAT(line1, testing::HasSubstr("[test_device]"));
    EXPECT_THAT(line1, testing::HasSubstr("Test message 1"));
    
    EXPECT_THAT(line2, testing::HasSubstr("[ERROR]"));
    EXPECT_THAT(line2, testing::HasSubstr("[test_device]"));
    EXPECT_THAT(line2, testing::HasSubstr("Test message 2"));
}

TEST_F(DeviceLoggerTest, ThreadSafety) {
    std::vector<std::string> loggedMessages;
    std::mutex messagesMutex;
    
    DeviceLogger::getInstance().setLogCallback(
        [&loggedMessages, &messagesMutex](LogLevel level, const std::string& deviceId, const std::string& message) {
            std::lock_guard<std::mutex> lock(messagesMutex);
            loggedMessages.push_back(deviceId + ":" + message);
        });
    
    DeviceLogger::getInstance().setLogLevel(LogLevel::DEBUG);
    
    const int numThreads = 10;
    const int messagesPerThread = 100;
    std::vector<std::thread> threads;
    
    // Create multiple threads that log concurrently
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([t, messagesPerThread]() {
            std::string deviceId = "device" + std::to_string(t);
            for (int i = 0; i < messagesPerThread; ++i) {
                DeviceLogger::getInstance().info(deviceId, "Message " + std::to_string(i));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check that all messages were logged
    std::lock_guard<std::mutex> lock(messagesMutex);
    EXPECT_EQ(loggedMessages.size(), numThreads * messagesPerThread);
    
    // Check that messages from each device are present
    for (int t = 0; t < numThreads; ++t) {
        std::string deviceId = "device" + std::to_string(t);
        int count = 0;
        for (const auto& msg : loggedMessages) {
            if (msg.find(deviceId) == 0) {
                count++;
            }
        }
        EXPECT_EQ(count, messagesPerThread);
    }
}

TEST_F(DeviceLoggerTest, MacroUsage) {
    std::vector<std::string> loggedMessages;
    
    DeviceLogger::getInstance().setLogCallback(
        [&loggedMessages](LogLevel level, const std::string& deviceId, const std::string& message) {
            loggedMessages.push_back(logLevelToString(level) + ":" + deviceId + ":" + message);
        });
    
    DeviceLogger::getInstance().setLogLevel(LogLevel::TRACE);
    
    // Test all logging macros
    DEVICE_LOG_TRACE("macro_device", "Trace message");
    DEVICE_LOG_DEBUG("macro_device", "Debug message");
    DEVICE_LOG_INFO("macro_device", "Info message");
    DEVICE_LOG_WARN("macro_device", "Warn message");
    DEVICE_LOG_ERROR("macro_device", "Error message");
    DEVICE_LOG_CRITICAL("macro_device", "Critical message");
    
    EXPECT_EQ(loggedMessages.size(), 6);
    EXPECT_THAT(loggedMessages[0], testing::HasSubstr("TRACE:macro_device:Trace message"));
    EXPECT_THAT(loggedMessages[1], testing::HasSubstr("DEBUG:macro_device:Debug message"));
    EXPECT_THAT(loggedMessages[2], testing::HasSubstr("INFO:macro_device:Info message"));
    EXPECT_THAT(loggedMessages[3], testing::HasSubstr("WARN:macro_device:Warn message"));
    EXPECT_THAT(loggedMessages[4], testing::HasSubstr("ERROR:macro_device:Error message"));
    EXPECT_THAT(loggedMessages[5], testing::HasSubstr("CRITICAL:macro_device:Critical message"));
}

// Exception tests
class DeviceExceptionTest : public ::testing::Test {};

TEST_F(DeviceExceptionTest, BasicException) {
    DeviceException ex("test_device", "Test error message");
    
    EXPECT_EQ(ex.getDeviceId(), "test_device");
    EXPECT_EQ(ex.getMessage(), "Test error message");
    EXPECT_THAT(ex.what(), testing::HasSubstr("test_device"));
    EXPECT_THAT(ex.what(), testing::HasSubstr("Test error message"));
}

TEST_F(DeviceExceptionTest, OperationException) {
    DeviceException ex("test_device", "connect", "Connection failed");
    
    EXPECT_EQ(ex.getDeviceId(), "test_device");
    EXPECT_EQ(ex.getOperation(), "connect");
    EXPECT_EQ(ex.getMessage(), "Connection failed");
    EXPECT_THAT(ex.what(), testing::HasSubstr("test_device"));
    EXPECT_THAT(ex.what(), testing::HasSubstr("connect"));
    EXPECT_THAT(ex.what(), testing::HasSubstr("Connection failed"));
}

TEST_F(DeviceExceptionTest, ConnectionException) {
    ConnectionException ex("websocket_device", "Failed to connect to server");
    
    EXPECT_EQ(ex.getDeviceId(), "websocket_device");
    EXPECT_EQ(ex.getOperation(), "Connection");
    EXPECT_EQ(ex.getMessage(), "Failed to connect to server");
}

TEST_F(DeviceExceptionTest, CommandException) {
    CommandException ex("telescope", "goto", "Invalid coordinates");
    
    EXPECT_EQ(ex.getDeviceId(), "telescope");
    EXPECT_THAT(ex.getOperation(), testing::HasSubstr("goto"));
    EXPECT_EQ(ex.getMessage(), "Invalid coordinates");
}

TEST_F(DeviceExceptionTest, ConfigurationException) {
    ConfigurationException ex("camera", "exposure_time", "Value out of range");
    
    EXPECT_EQ(ex.getDeviceId(), "camera");
    EXPECT_THAT(ex.getOperation(), testing::HasSubstr("exposure_time"));
    EXPECT_EQ(ex.getMessage(), "Value out of range");
}

TEST_F(DeviceExceptionTest, ExceptionThrowCatch) {
    try {
        throw ConnectionException("test_device", "Connection timeout");
    } catch (const DeviceException& ex) {
        EXPECT_EQ(ex.getDeviceId(), "test_device");
        EXPECT_EQ(ex.getMessage(), "Connection timeout");
    } catch (...) {
        FAIL() << "Should have caught DeviceException";
    }
}

// Performance tests
class DeviceLoggerPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        DeviceLogger::getInstance().setConsoleLogging(false);
        DeviceLogger::getInstance().setLogFile("");
        DeviceLogger::getInstance().setLogCallback(nullptr);
    }
};

TEST_F(DeviceLoggerPerformanceTest, LoggingThroughput) {
    std::atomic<int> messageCount{0};
    
    DeviceLogger::getInstance().setLogCallback(
        [&messageCount](LogLevel, const std::string&, const std::string&) {
            messageCount++;
        });
    
    DeviceLogger::getInstance().setLogLevel(LogLevel::DEBUG);
    
    const int numMessages = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numMessages; ++i) {
        DeviceLogger::getInstance().info("perf_device", "Performance test message " + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double messagesPerSecond = (numMessages * 1000000.0) / duration.count();
    
    std::cout << "Logging throughput: " << messagesPerSecond << " messages/second" << std::endl;
    
    EXPECT_EQ(messageCount.load(), numMessages);
    EXPECT_GT(messagesPerSecond, 50000.0); // Should handle at least 50k messages/second
}

TEST_F(DeviceLoggerPerformanceTest, FilteredLogging) {
    std::atomic<int> messageCount{0};
    
    DeviceLogger::getInstance().setLogCallback(
        [&messageCount](LogLevel, const std::string&, const std::string&) {
            messageCount++;
        });
    
    DeviceLogger::getInstance().setLogLevel(LogLevel::ERROR); // Only log errors and above
    
    const int numMessages = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numMessages; ++i) {
        // These should be filtered out
        DeviceLogger::getInstance().debug("perf_device", "Debug message " + std::to_string(i));
        DeviceLogger::getInstance().info("perf_device", "Info message " + std::to_string(i));
        
        // Only every 10th message should be logged
        if (i % 10 == 0) {
            DeviceLogger::getInstance().error("perf_device", "Error message " + std::to_string(i));
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double messagesPerSecond = (numMessages * 2.1 * 1000000.0) / duration.count(); // 2.1 calls per iteration on average
    
    std::cout << "Filtered logging throughput: " << messagesPerSecond << " messages/second" << std::endl;
    
    EXPECT_EQ(messageCount.load(), numMessages / 10); // Only error messages should be logged
    EXPECT_GT(messagesPerSecond, 100000.0); // Should be even faster when filtering
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
