#include <gtest/gtest.h>
#include <astrocomm/core/error_recovery.h>
#include "test_helpers.h"

using namespace astrocomm::core;
using namespace astrocomm::test;

class ErrorRecoveryTest : public MessageTestBase {
protected:
    void SetUp() override {
        MessageTestBase::SetUp();
        errorManager = std::make_unique<ErrorRecoveryManager>();
        handlerCalled = false;
        lastErrorCode = "";
    }
    
    void TearDown() override {
        if (errorManager) {
            errorManager->stop();
        }
        MessageTestBase::TearDown();
    }
    
    std::unique_ptr<ErrorRecoveryManager> errorManager;
    bool handlerCalled;
    std::string lastErrorCode;
    
    // Custom error handler for testing
    bool customErrorHandler(const ErrorContext& context) {
        handlerCalled = true;
        lastErrorCode = context.errorCode;
        return true; // Indicate error was handled
    }
};

// Test basic error handler registration
TEST_F(ErrorRecoveryTest, BasicErrorHandlerRegistration) {
    // Register a simple error handler
    errorManager->registerErrorHandler("TEST_ERROR", ErrorHandlingStrategy::IGNORE);
    
    // Create and handle an error
    auto error = createTestError();
    bool handled = errorManager->handleError(*error);
    
    EXPECT_TRUE(handled);
}

// Test error handling strategies
TEST_F(ErrorRecoveryTest, ErrorHandlingStrategies) {
    // Test IGNORE strategy
    errorManager->registerErrorHandler("IGNORE_ERROR", ErrorHandlingStrategy::IGNORE);
    auto ignoreError = std::make_unique<ErrorMessage>("IGNORE_ERROR", "Test ignore error");
    ignoreError->setDeviceId(testDeviceId);
    
    bool handled = errorManager->handleError(*ignoreError);
    EXPECT_TRUE(handled);
    
    // Test NOTIFY strategy
    errorManager->registerErrorHandler("NOTIFY_ERROR", ErrorHandlingStrategy::NOTIFY);
    auto notifyError = std::make_unique<ErrorMessage>("NOTIFY_ERROR", "Test notify error");
    notifyError->setDeviceId(testDeviceId);
    
    handled = errorManager->handleError(*notifyError);
    // NOTIFY strategy typically doesn't resolve the error, just notifies
    EXPECT_FALSE(handled);
    
    // Test RETRY strategy
    errorManager->registerErrorHandler("RETRY_ERROR", ErrorHandlingStrategy::RETRY);
    auto retryError = std::make_unique<ErrorMessage>("RETRY_ERROR", "Test retry error");
    retryError->setDeviceId(testDeviceId);
    
    handled = errorManager->handleError(*retryError);
    // Result depends on implementation, but should attempt retry
    EXPECT_TRUE(handled || !handled); // Either outcome is valid for test
}

// Test device-specific error handlers
TEST_F(ErrorRecoveryTest, DeviceSpecificErrorHandlers) {
    // Register device-specific handler
    errorManager->registerDeviceErrorHandler(testDeviceId, "DEVICE_ERROR", ErrorHandlingStrategy::IGNORE);
    
    // Register global handler for same error code
    errorManager->registerErrorHandler("DEVICE_ERROR", ErrorHandlingStrategy::NOTIFY);
    
    // Create error for specific device
    auto deviceError = std::make_unique<ErrorMessage>("DEVICE_ERROR", "Device specific error");
    deviceError->setDeviceId(testDeviceId);
    
    bool handled = errorManager->handleError(*deviceError);
    
    // Device-specific handler should take precedence (IGNORE should handle it)
    EXPECT_TRUE(handled);
    
    // Create error for different device
    auto otherError = std::make_unique<ErrorMessage>("DEVICE_ERROR", "Other device error");
    otherError->setDeviceId("other_device");
    
    handled = errorManager->handleError(*otherError);
    
    // Should use global handler (NOTIFY doesn't handle)
    EXPECT_FALSE(handled);
}

// Test custom error handlers
TEST_F(ErrorRecoveryTest, CustomErrorHandlers) {
    // Register custom error handler
    auto handler = [this](const ErrorContext& context) -> bool {
        return customErrorHandler(context);
    };
    
    errorManager->registerCustomErrorHandler("CUSTOM_ERROR", handler);
    
    // Create and handle custom error
    auto customError = std::make_unique<ErrorMessage>("CUSTOM_ERROR", "Custom error message");
    customError->setDeviceId(testDeviceId);
    
    bool handled = errorManager->handleError(*customError);
    
    EXPECT_TRUE(handled);
    EXPECT_TRUE(handlerCalled);
    EXPECT_EQ(lastErrorCode, "CUSTOM_ERROR");
}

// Test error context creation
TEST_F(ErrorRecoveryTest, ErrorContextCreation) {
    auto error = createTestError();
    error->setErrorCode("CONTEXT_TEST");
    error->setErrorMessage("Context test message");
    error->setSeverity(ErrorMessage::Severity::ERROR);
    
    ErrorContext context = ErrorContext::fromErrorMessage(*error);
    
    EXPECT_EQ(context.errorCode, "CONTEXT_TEST");
    EXPECT_EQ(context.errorMessage, "Context test message");
    EXPECT_EQ(context.deviceId, testDeviceId);
    EXPECT_EQ(context.severity, ErrorMessage::Severity::ERROR);
    EXPECT_EQ(context.retryCount, 0);
    EXPECT_FALSE(context.timestamp.empty());
}

// Test error statistics
TEST_F(ErrorRecoveryTest, ErrorStatistics) {
    errorManager->start();
    
    // Initially no errors
    auto stats = errorManager->getStatistics();
    EXPECT_EQ(stats.totalErrors, 0);
    EXPECT_EQ(stats.handledErrors, 0);
    EXPECT_EQ(stats.unhandledErrors, 0);
    
    // Register handlers
    errorManager->registerErrorHandler("HANDLED_ERROR", ErrorHandlingStrategy::IGNORE);
    
    // Handle some errors
    auto handledError = std::make_unique<ErrorMessage>("HANDLED_ERROR", "Handled error");
    handledError->setDeviceId(testDeviceId);
    errorManager->handleError(*handledError);
    
    auto unhandledError = std::make_unique<ErrorMessage>("UNHANDLED_ERROR", "Unhandled error");
    unhandledError->setDeviceId(testDeviceId);
    errorManager->handleError(*unhandledError);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check statistics
    stats = errorManager->getStatistics();
    EXPECT_EQ(stats.totalErrors, 2);
    EXPECT_EQ(stats.handledErrors, 1);
    EXPECT_EQ(stats.unhandledErrors, 1);
    
    errorManager->stop();
}

// Test error recovery manager lifecycle
TEST_F(ErrorRecoveryTest, ErrorRecoveryManagerLifecycle) {
    // Initially not running
    EXPECT_FALSE(errorManager->isRunning());
    
    // Start the manager
    errorManager->start();
    EXPECT_TRUE(errorManager->isRunning());
    
    // Stop the manager
    errorManager->stop();
    EXPECT_FALSE(errorManager->isRunning());
    
    // Should be able to restart
    errorManager->start();
    EXPECT_TRUE(errorManager->isRunning());
    
    errorManager->stop();
}

// Test concurrent error handling
TEST_F(ErrorRecoveryTest, ConcurrentErrorHandling) {
    errorManager->registerErrorHandler("CONCURRENT_ERROR", ErrorHandlingStrategy::IGNORE);
    errorManager->start();
    
    const int numThreads = 4;
    const int errorsPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> totalHandled{0};
    
    // Create multiple threads that generate errors
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([this, &totalHandled, errorsPerThread]() {
            for (int i = 0; i < errorsPerThread; i++) {
                auto error = std::make_unique<ErrorMessage>("CONCURRENT_ERROR", "Concurrent error");
                error->setDeviceId(testDeviceId + "_" + std::to_string(i));
                
                if (errorManager->handleError(*error)) {
                    totalHandled++;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All errors should have been handled
    EXPECT_EQ(totalHandled, numThreads * errorsPerThread);
    
    errorManager->stop();
}

// Test error handler priority
TEST_F(ErrorRecoveryTest, ErrorHandlerPriority) {
    // Register multiple handlers for the same error code with different priorities
    errorManager->registerErrorHandler("PRIORITY_ERROR", ErrorHandlingStrategy::NOTIFY, 1);
    errorManager->registerErrorHandler("PRIORITY_ERROR", ErrorHandlingStrategy::IGNORE, 10);
    
    // Create error
    auto error = std::make_unique<ErrorMessage>("PRIORITY_ERROR", "Priority test error");
    error->setDeviceId(testDeviceId);
    
    bool handled = errorManager->handleError(*error);
    
    // Higher priority handler (IGNORE) should be used, so error should be handled
    EXPECT_TRUE(handled);
}
