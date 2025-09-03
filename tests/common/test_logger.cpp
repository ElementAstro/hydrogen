/**
 * @file test_logger.cpp
 * @brief Comprehensive tests for logger functionality
 * 
 * Tests the logging system including different log levels, file output,
 * component-based logging, and fallback behavior.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <common/logger.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>

using namespace hydrogen::common;
using namespace testing;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testLogFile = "test_log.txt";
        testComponent = "TestComponent";
        testMessage = "Test log message";
        
        // Clean up any existing test log file
        if (std::filesystem::exists(testLogFile)) {
            std::filesystem::remove(testLogFile);
        }
    }
    
    void TearDown() override {
        // Clean up test log file
        if (std::filesystem::exists(testLogFile)) {
            std::filesystem::remove(testLogFile);
        }
    }
    
    std::string testLogFile;
    std::string testComponent;
    std::string testMessage;
    
    // Helper to read log file contents
    std::string readLogFile() {
        if (!std::filesystem::exists(testLogFile)) {
            return "";
        }
        
        std::ifstream file(testLogFile);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    // Helper to capture console output
    std::string captureConsoleOutput(std::function<void()> func) {
        std::stringstream buffer;
        std::streambuf* orig = std::cout.rdbuf(buffer.rdbuf());
        
        func();
        
        std::cout.rdbuf(orig);
        return buffer.str();
    }
};

// Test logger initialization
TEST_F(LoggerTest, LoggerInitialization) {
    // Test initialization with different log levels
    EXPECT_NO_THROW(initLogger(testLogFile, LogLevel::INFO));
    EXPECT_NO_THROW(initLogger(testLogFile, LogLevel::DEBUG));
    EXPECT_NO_THROW(initLogger(testLogFile, LogLevel::WARN));
    EXPECT_NO_THROW(initLogger(testLogFile, LogLevel::ERR));
    EXPECT_NO_THROW(initLogger(testLogFile, LogLevel::CRITICAL));
}

// Test basic logging functions
TEST_F(LoggerTest, BasicLogging) {
    initLogger(testLogFile, LogLevel::DEBUG);
    
    // Test all log levels
    logInfo(testMessage);
    logDebug("Debug message");
    logWarning("Warning message");
    logError("Error message");
    logCritical("Critical message");
    
    // Verify log file was created
    EXPECT_TRUE(std::filesystem::exists(testLogFile));
    
    // Read log contents
    std::string logContents = readLogFile();
    EXPECT_FALSE(logContents.empty());
}

// Test component-based logging
TEST_F(LoggerTest, ComponentBasedLogging) {
    initLogger(testLogFile, LogLevel::DEBUG);
    
    // Test logging with component names
    logInfo(testMessage, testComponent);
    logDebug("Debug message", testComponent);
    logWarning("Warning message", testComponent);
    logError("Error message", testComponent);
    logCritical("Critical message", testComponent);
    
    std::string logContents = readLogFile();
    
    // Verify component name appears in log
    EXPECT_NE(logContents.find(testComponent), std::string::npos);
}

// Test log levels filtering
TEST_F(LoggerTest, LogLevelFiltering) {
    // Initialize with WARNING level - should filter out DEBUG and INFO
    initLogger(testLogFile, LogLevel::WARN);
    
    logDebug("This debug message should be filtered");
    logInfo("This info message should be filtered");
    logWarning("This warning message should appear");
    logError("This error message should appear");
    
    std::string logContents = readLogFile();
    
    // Debug and info messages should not appear
    EXPECT_EQ(logContents.find("debug message"), std::string::npos);
    EXPECT_EQ(logContents.find("info message"), std::string::npos);
    
    // Warning and error messages should appear
    EXPECT_NE(logContents.find("warning message"), std::string::npos);
    EXPECT_NE(logContents.find("error message"), std::string::npos);
}

// Test fallback logging when spdlog is not available
TEST_F(LoggerTest, FallbackLogging) {
    // Test console output when no file logger is initialized
    std::string output = captureConsoleOutput([this]() {
        logInfo(testMessage);
    });
    
    // Should contain the message (fallback to console)
    EXPECT_NE(output.find(testMessage), std::string::npos);
    EXPECT_NE(output.find("[INFO]"), std::string::npos);
}

// Test logging with empty messages
TEST_F(LoggerTest, EmptyMessages) {
    initLogger(testLogFile, LogLevel::DEBUG);
    
    // Test logging empty messages
    EXPECT_NO_THROW(logInfo(""));
    EXPECT_NO_THROW(logDebug("", testComponent));
    EXPECT_NO_THROW(logWarning(""));
    EXPECT_NO_THROW(logError("", testComponent));
    EXPECT_NO_THROW(logCritical(""));
}

// Test logging with special characters
TEST_F(LoggerTest, SpecialCharacters) {
    initLogger(testLogFile, LogLevel::DEBUG);
    
    std::string specialMessage = "Message with special chars: !@#$%^&*(){}[]|\\:;\"'<>,.?/~`";
    std::string unicodeMessage = "Unicode message: αβγδε 中文 🚀";
    
    EXPECT_NO_THROW(logInfo(specialMessage));
    EXPECT_NO_THROW(logInfo(unicodeMessage));
    
    std::string logContents = readLogFile();
    EXPECT_NE(logContents.find("special chars"), std::string::npos);
}

// Test concurrent logging
TEST_F(LoggerTest, ConcurrentLogging) {
    initLogger(testLogFile, LogLevel::DEBUG);
    
    const int numThreads = 4;
    const int messagesPerThread = 10;
    std::vector<std::thread> threads;
    
    // Create multiple threads that log messages
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; i++) {
                std::string message = "Thread " + std::to_string(t) + " Message " + std::to_string(i);
                logInfo(message, "Thread" + std::to_string(t));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::string logContents = readLogFile();
    EXPECT_FALSE(logContents.empty());
    
    // Verify messages from all threads appear
    for (int t = 0; t < numThreads; t++) {
        std::string threadMarker = "Thread " + std::to_string(t);
        EXPECT_NE(logContents.find(threadMarker), std::string::npos);
    }
}

// Test log file rotation/overwrite behavior
TEST_F(LoggerTest, LogFileOverwrite) {
    // Initialize logger and write some content
    initLogger(testLogFile, LogLevel::INFO);
    logInfo("First message");
    
    std::string firstContent = readLogFile();
    EXPECT_NE(firstContent.find("First message"), std::string::npos);
    
    // Re-initialize logger (should append or overwrite based on implementation)
    initLogger(testLogFile, LogLevel::INFO);
    logInfo("Second message");
    
    std::string secondContent = readLogFile();
    EXPECT_NE(secondContent.find("Second message"), std::string::npos);
}

// Test logging performance
TEST_F(LoggerTest, LoggingPerformance) {
    initLogger(testLogFile, LogLevel::INFO);
    
    const int numMessages = 1000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numMessages; i++) {
        logInfo("Performance test message " + std::to_string(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds for 1000 messages
    
    std::string logContents = readLogFile();
    EXPECT_FALSE(logContents.empty());
}

// Test log level enum values
TEST_F(LoggerTest, LogLevelEnumValues) {
    // Test that log levels have expected numeric values
    EXPECT_EQ(static_cast<int>(LogLevel::TRACE), 0);
    EXPECT_EQ(static_cast<int>(LogLevel::DEBUG), 1);
    EXPECT_EQ(static_cast<int>(LogLevel::INFO), 2);
    EXPECT_EQ(static_cast<int>(LogLevel::WARN), 3);
    EXPECT_EQ(static_cast<int>(LogLevel::ERR), 4);
    EXPECT_EQ(static_cast<int>(LogLevel::CRITICAL), 5);
}

// Test error conditions
TEST_F(LoggerTest, ErrorConditions) {
    // Test initialization with invalid file path
    std::string invalidPath = "/invalid/path/that/does/not/exist/test.log";
    
    // Should not throw, but may not create the file
    EXPECT_NO_THROW(initLogger(invalidPath, LogLevel::INFO));
    
    // Logging should still work (fallback to console)
    EXPECT_NO_THROW(logInfo("Test message after invalid init"));
}
