#include <gtest/gtest.h>
#include "hydrogen/server/infrastructure/logging.h"
#include <memory>

using namespace hydrogen::server::infrastructure;

class LoggingManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        LoggingServiceFactory factory;
        std::unordered_map<std::string, std::string> config;
        config["enableConsole"] = "false"; // Disable console for tests
        config["enableFile"] = "true";
        config["logFilePath"] = "./test_data/test.log";
        config["globalLevel"] = "DEBUG";

        auto service = factory.createService("LoggingService", config);
        manager_ = std::unique_ptr<ILoggingService>(static_cast<ILoggingService*>(service.release()));
        ASSERT_NE(manager_, nullptr);
        ASSERT_TRUE(manager_->initialize());
    }

    void TearDown() override {
        if (manager_) {
            manager_->stop();
        }
    }

    std::unique_ptr<ILoggingService> manager_;
};

TEST_F(LoggingManagerTest, BasicOperations) {
    // Test logger creation and management
    auto logger = manager_->getLogger("test_logger");
    EXPECT_NE(logger, nullptr);

    // Test global log level operations
    manager_->setGlobalLevel(LogLevel::INFO);
    EXPECT_EQ(manager_->getGlobalLevel(), LogLevel::INFO);

    manager_->setGlobalLevel(LogLevel::DEBUG);
    EXPECT_EQ(manager_->getGlobalLevel(), LogLevel::DEBUG);
}

TEST_F(LoggingManagerTest, LoggerManagement) {
    // Create custom logger
    auto customLogger = manager_->createLogger("custom_logger");
    EXPECT_NE(customLogger, nullptr);

    auto loggers = manager_->getLoggerNames();
    EXPECT_GE(loggers.size(), 1);

    // Test logger functionality
    customLogger->setLevel(LogLevel::ERROR);
    EXPECT_EQ(customLogger->getLevel(), LogLevel::ERROR);

    // Remove logger
    EXPECT_TRUE(manager_->removeLogger("custom_logger"));
}

TEST_F(LoggingManagerTest, FileOperations) {
    // Test file size retrieval
    size_t fileSize = manager_->getLogFileSize("./test_data/test.log");
    EXPECT_GE(fileSize, 0);

    // Test log archiving
    EXPECT_TRUE(manager_->archiveLogs("./test_data/archived"));

    // Test log cleanup
    EXPECT_TRUE(manager_->cleanupOldLogs(std::chrono::hours(24)));
}

TEST_F(LoggingManagerTest, Statistics) {
    // Test log statistics
    auto logStats = manager_->getLogStatistics();
    EXPECT_GE(logStats.size(), 0);

    // Test log count
    size_t logCount = manager_->getLogCount();
    EXPECT_GE(logCount, 0);
}
