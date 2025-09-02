#include <gtest/gtest.h>
#include "hydrogen/server/infrastructure/error_handler.h"
#include <memory>

using namespace hydrogen::server::infrastructure;

class ErrorHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ErrorHandlerConfig config;
        config.maxStoredErrors = 100;
        config.enableLogging = false; // Disable for tests
        config.enableNotifications = false;
        
        handler_ = ErrorHandlerFactory::createHandler(config);
        ASSERT_NE(handler_, nullptr);
        ASSERT_TRUE(handler_->isInitialized());
    }
    
    void TearDown() override {
        if (handler_) {
            handler_->shutdown();
        }
    }
    
    std::unique_ptr<IErrorHandler> handler_;
};

TEST_F(ErrorHandlerTest, BasicOperations) {
    EXPECT_TRUE(handler_->isInitialized());
    
    // Handle a critical error
    handler_->handleCriticalError("Test critical error", "test context");
    
    // Handle a warning
    handler_->handleWarning("Test warning", "test context");
    
    // Get errors
    auto criticalErrors = handler_->getErrors(ErrorSeverity::CRITICAL);
    EXPECT_EQ(criticalErrors.size(), 1);
    
    auto warnings = handler_->getErrors(ErrorSeverity::WARNING);
    EXPECT_EQ(warnings.size(), 1);
}

TEST_F(ErrorHandlerTest, ErrorRetrieval) {
    handler_->handleCriticalError("Test error 1", "context 1");
    handler_->handleWarning("Test error 2", "context 2");
    
    // Get recent errors
    auto recentErrors = handler_->getRecentErrors(std::chrono::minutes(1));
    EXPECT_EQ(recentErrors.size(), 2);
    
    // Get errors by category
    auto criticalErrors = handler_->getErrorsByCategory("CRITICAL");
    EXPECT_EQ(criticalErrors.size(), 1);
}

TEST_F(ErrorHandlerTest, HandlerRegistration) {
    bool handlerCalled = false;
    
    handler_->registerErrorHandler("test_handler", 
        [&](const ErrorInfo& error) {
            handlerCalled = true;
        });
    
    handler_->handleCriticalError("Test error", "test context");
    EXPECT_TRUE(handlerCalled);
    
    auto handlers = handler_->getRegisteredHandlers();
    EXPECT_GE(handlers.size(), 1);
    
    handler_->unregisterErrorHandler("test_handler");
}

TEST_F(ErrorHandlerTest, Statistics) {
    handler_->handleCriticalError("Error 1", "context");
    handler_->handleWarning("Warning 1", "context");
    
    auto stats = handler_->getStatistics();
    EXPECT_EQ(stats.totalErrors, 2);
    EXPECT_EQ(stats.criticalErrors, 1);
    EXPECT_EQ(stats.warningErrors, 1);
}

TEST_F(ErrorHandlerTest, RecoveryOperations) {
    handler_->handleCriticalError("Recoverable error", "test context");
    
    auto errors = handler_->getErrors(ErrorSeverity::CRITICAL);
    ASSERT_EQ(errors.size(), 1);
    
    std::string errorId = errors[0].id;
    EXPECT_TRUE(handler_->attemptRecovery(errorId));
    
    // Set recovery strategy
    handler_->setRecoveryStrategy("CRITICAL", RecoveryStrategy::RESTART);
    auto strategy = handler_->getRecoveryStrategy("CRITICAL");
    EXPECT_EQ(strategy, RecoveryStrategy::RESTART);
}
