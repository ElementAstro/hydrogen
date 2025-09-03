#include <gtest/gtest.h>
#include "hydrogen/server/infrastructure/error_handler.h"
#include <memory>

using namespace hydrogen::server::infrastructure;

class ErrorHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ErrorHandlerFactory factory;
        std::unordered_map<std::string, std::string> config;
        config["maxStoredErrors"] = "100";
        config["enableLogging"] = "false"; // Disable for tests
        config["enableNotifications"] = "false";

        auto service = factory.createService("ErrorHandler", config);
        handler_ = std::unique_ptr<IErrorHandler>(static_cast<IErrorHandler*>(service.release()));
        ASSERT_NE(handler_, nullptr);
        ASSERT_TRUE(handler_->initialize());
    }

    void TearDown() override {
        if (handler_) {
            handler_->stop();
        }
    }

    std::unique_ptr<IErrorHandler> handler_;
};

TEST_F(ErrorHandlerTest, BasicOperations) {
    // Report a critical error
    std::string errorId1 = handler_->reportError("CRITICAL_ERROR", "Test critical error",
                                                ErrorSeverity::CRITICAL, ErrorCategory::SYSTEM, "test_component");
    EXPECT_FALSE(errorId1.empty());

    // Report a medium severity error
    std::string errorId2 = handler_->reportError("MEDIUM_ERROR", "Test medium error",
                                                ErrorSeverity::MEDIUM, ErrorCategory::SYSTEM, "test_component");
    EXPECT_FALSE(errorId2.empty());

    // Get errors
    auto criticalErrors = handler_->getErrors(ErrorSeverity::CRITICAL);
    EXPECT_GE(criticalErrors.size(), 1);

    auto allErrors = handler_->getErrors(ErrorSeverity::LOW);
    EXPECT_GE(allErrors.size(), 2);
}

TEST_F(ErrorHandlerTest, ErrorRetrieval) {
    // Report some errors
    handler_->reportError("ERROR_1", "Test error 1", ErrorSeverity::CRITICAL, ErrorCategory::SYSTEM, "component1");
    handler_->reportError("ERROR_2", "Test error 2", ErrorSeverity::MEDIUM, ErrorCategory::NETWORK, "component2");

    // Get recent errors
    auto recentErrors = handler_->getRecentErrors(std::chrono::minutes(1));
    EXPECT_GE(recentErrors.size(), 2);

    // Get errors by severity
    auto criticalErrors = handler_->getErrors(ErrorSeverity::CRITICAL);
    EXPECT_GE(criticalErrors.size(), 1);
}

TEST_F(ErrorHandlerTest, ErrorPatternManagement) {
    // Create an error pattern
    ErrorPattern pattern;
    pattern.patternId = "test_pattern";
    pattern.name = "Test Pattern";
    pattern.errorCodePattern = "TEST_.*";
    pattern.messagePattern = ".*test.*";
    pattern.category = ErrorCategory::SYSTEM;
    pattern.minSeverity = ErrorSeverity::MEDIUM;
    pattern.componentPattern = "test_component";
    pattern.enabled = true;
    pattern.priority = 1;

    // Add the pattern
    EXPECT_TRUE(handler_->addErrorPattern(pattern));

    // Get the pattern back
    auto retrievedPattern = handler_->getErrorPattern("test_pattern");
    EXPECT_EQ(retrievedPattern.patternId, "test_pattern");

    // Remove the pattern
    EXPECT_TRUE(handler_->removeErrorPattern("test_pattern"));
}

TEST_F(ErrorHandlerTest, ErrorStatistics) {
    // Report some errors
    handler_->reportError("ERROR_1", "Error 1", ErrorSeverity::CRITICAL, ErrorCategory::SYSTEM, "component1");
    handler_->reportError("ERROR_2", "Error 2", ErrorSeverity::MEDIUM, ErrorCategory::NETWORK, "component2");

    // Get error counts by severity
    auto severityStats = handler_->getErrorCountBySeverity(std::chrono::hours(1));
    EXPECT_GE(severityStats[ErrorSeverity::CRITICAL], 1);
    EXPECT_GE(severityStats[ErrorSeverity::MEDIUM], 1);

    // Get error counts by component
    auto componentStats = handler_->getErrorCountByComponent(std::chrono::hours(1));
    EXPECT_GE(componentStats["component1"], 1);
    EXPECT_GE(componentStats["component2"], 1);
}

TEST_F(ErrorHandlerTest, ErrorSuppression) {
    // Suppress an error code
    EXPECT_TRUE(handler_->suppressError("TEST_ERROR", std::chrono::minutes(5)));
    EXPECT_TRUE(handler_->isErrorSuppressed("TEST_ERROR"));

    // Get suppressed errors
    auto suppressedErrors = handler_->getSuppressedErrors();
    EXPECT_GE(suppressedErrors.size(), 1);

    // Unsuppress the error
    EXPECT_TRUE(handler_->unsuppressError("TEST_ERROR"));
    EXPECT_FALSE(handler_->isErrorSuppressed("TEST_ERROR"));
}
