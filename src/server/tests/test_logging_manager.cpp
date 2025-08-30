#include <gtest/gtest.h>
#include "astrocomm/server/infrastructure/logging.h"
#include <memory>

using namespace astrocomm::server::infrastructure;

class LoggingManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        LoggingConfig config;
        config.enableConsole = false; // Disable console for tests
        config.enableFile = true;
        config.logFilePath = "./test_data/test.log";
        config.globalLevel = LogLevel::DEBUG;
        
        manager_ = LoggingManagerFactory::createManager(config);
        ASSERT_NE(manager_, nullptr);
        ASSERT_TRUE(manager_->isInitialized());
    }
    
    void TearDown() override {
        if (manager_) {
            manager_->shutdown();
        }
    }
    
    std::unique_ptr<ILoggingManager> manager_;
};

TEST_F(LoggingManagerTest, BasicOperations) {
    EXPECT_TRUE(manager_->isInitialized());
    
    // Test log level operations
    EXPECT_TRUE(manager_->setLogLevel(LogLevel::INFO));
    EXPECT_EQ(manager_->getLogLevel(), LogLevel::INFO);
    
    EXPECT_TRUE(manager_->setLogLevel(LogLevel::DEBUG));
    EXPECT_EQ(manager_->getLogLevel(), LogLevel::DEBUG);
}

TEST_F(LoggingManagerTest, LoggerManagement) {
    LoggingConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "./test_data/custom.log";
    
    // Create custom logger
    EXPECT_TRUE(manager_->createLogger("custom_logger", config));
    
    auto loggers = manager_->getLoggerNames();
    EXPECT_GE(loggers.size(), 1);
    
    // Set log level for specific logger
    EXPECT_TRUE(manager_->setLogLevel("custom_logger", LogLevel::ERROR));
    
    // Remove logger
    EXPECT_TRUE(manager_->removeLogger("custom_logger"));
}

TEST_F(LoggingManagerTest, FileOperations) {
    // Test file size retrieval
    size_t fileSize = manager_->getLogFileSize();
    EXPECT_GE(fileSize, 0);
    
    // Test log rotation
    EXPECT_TRUE(manager_->rotateLogFile());
    
    // Test archiving
    EXPECT_TRUE(manager_->archiveLogFile("./test_data/archived.log"));
}

TEST_F(LoggingManagerTest, Statistics) {
    auto stats = manager_->getStatistics();
    EXPECT_GE(stats.activeLoggers, 1);
    EXPECT_GE(stats.logFileSize, 0);
}
