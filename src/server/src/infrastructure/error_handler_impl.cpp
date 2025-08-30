#include "astrocomm/server/infrastructure/error_handler.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>

namespace astrocomm {
namespace server {
namespace infrastructure {

/**
 * @brief Concrete implementation of the Error Handler
 */
class ErrorHandlerImpl : public IErrorHandler {
public:
    ErrorHandlerImpl() : initialized_(false), errorCount_(0) {
        spdlog::info("Error handler created");
    }

    ~ErrorHandlerImpl() {
        shutdown();
    }

    // Initialization and lifecycle
    bool initialize(const ErrorHandlerConfig& config) override {
        if (initialized_) {
            spdlog::warn("Error handler already initialized");
            return true;
        }

        try {
            config_ = config;
            
            // Initialize error tracking
            errors_.clear();
            errorCount_ = 0;
            
            // Set up default error handlers
            setupDefaultHandlers();
            
            initialized_ = true;
            spdlog::info("Error handler initialized successfully");
            spdlog::info("Error handler config: MaxErrors={}, EnableLogging={}, EnableNotifications={}", 
                        config_.maxStoredErrors, config_.enableLogging, config_.enableNotifications);
            
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize error handler: {}", e.what());
            return false;
        }
    }

    bool shutdown() override {
        if (!initialized_) {
            return true;
        }

        try {
            spdlog::info("Shutting down error handler");
            
            // Log final statistics
            auto stats = getStatistics();
            spdlog::info("Error handler shutdown - Total errors: {}, Critical: {}, Warnings: {}", 
                        stats.totalErrors, stats.criticalErrors, stats.warningErrors);
            
            // Clear handlers and errors
            {
                std::lock_guard<std::mutex> lock(handlersMutex_);
                errorHandlers_.clear();
            }
            
            {
                std::lock_guard<std::mutex> lock(errorsMutex_);
                errors_.clear();
            }
            
            initialized_ = false;
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Error during error handler shutdown: {}", e.what());
            return false;
        }
    }

    bool isInitialized() const override {
        return initialized_;
    }

    // Error handling
    void handleError(const ErrorInfo& error) override {
        if (!initialized_) {
            spdlog::error("Error handler not initialized, cannot handle error: {}", error.message);
            return;
        }

        try {
            // Store error
            storeError(error);
            
            // Log error if enabled
            if (config_.enableLogging) {
                logError(error);
            }
            
            // Send notifications if enabled
            if (config_.enableNotifications) {
                sendNotification(error);
            }
            
            // Call registered handlers
            callErrorHandlers(error);
            
            // Update statistics
            updateStatistics(error);
            
        } catch (const std::exception& e) {
            spdlog::error("Error in error handler: {}", e.what());
        }
    }

    void handleException(const std::exception& exception, const std::string& context) override {
        ErrorInfo error;
        error.id = generateErrorId();
        error.severity = ErrorSeverity::CRITICAL;
        error.category = "EXCEPTION";
        error.message = exception.what();
        error.context = context;
        error.timestamp = std::chrono::system_clock::now();
        error.threadId = std::this_thread::get_id();
        
        handleError(error);
    }

    void handleCriticalError(const std::string& message, const std::string& context) override {
        ErrorInfo error;
        error.id = generateErrorId();
        error.severity = ErrorSeverity::CRITICAL;
        error.category = "CRITICAL";
        error.message = message;
        error.context = context;
        error.timestamp = std::chrono::system_clock::now();
        error.threadId = std::this_thread::get_id();
        
        handleError(error);
    }

    void handleWarning(const std::string& message, const std::string& context) override {
        ErrorInfo error;
        error.id = generateErrorId();
        error.severity = ErrorSeverity::WARNING;
        error.category = "WARNING";
        error.message = message;
        error.context = context;
        error.timestamp = std::chrono::system_clock::now();
        error.threadId = std::this_thread::get_id();
        
        handleError(error);
    }

    // Error retrieval
    std::vector<ErrorInfo> getErrors(ErrorSeverity severity) const override {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        std::vector<ErrorInfo> result;
        for (const auto& error : errors_) {
            if (severity == ErrorSeverity::INFO || error.severity == severity) {
                result.push_back(error);
            }
        }
        
        return result;
    }

    std::vector<ErrorInfo> getRecentErrors(std::chrono::minutes timeWindow) const override {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        auto cutoffTime = std::chrono::system_clock::now() - timeWindow;
        std::vector<ErrorInfo> result;
        
        for (const auto& error : errors_) {
            if (error.timestamp >= cutoffTime) {
                result.push_back(error);
            }
        }
        
        return result;
    }

    std::optional<ErrorInfo> getError(const std::string& errorId) const override {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        auto it = std::find_if(errors_.begin(), errors_.end(),
            [&errorId](const ErrorInfo& error) {
                return error.id == errorId;
            });
        
        if (it != errors_.end()) {
            return *it;
        }
        
        return std::nullopt;
    }

    std::vector<ErrorInfo> getErrorsByCategory(const std::string& category) const override {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        std::vector<ErrorInfo> result;
        for (const auto& error : errors_) {
            if (error.category == category) {
                result.push_back(error);
            }
        }
        
        return result;
    }

    // Handler registration
    void registerErrorHandler(const std::string& name, ErrorHandlerCallback handler) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        errorHandlers_[name] = handler;
        spdlog::debug("Error handler registered: {}", name);
    }

    void unregisterErrorHandler(const std::string& name) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        auto it = errorHandlers_.find(name);
        if (it != errorHandlers_.end()) {
            errorHandlers_.erase(it);
            spdlog::debug("Error handler unregistered: {}", name);
        }
    }

    std::vector<std::string> getRegisteredHandlers() const override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        std::vector<std::string> result;
        for (const auto& pair : errorHandlers_) {
            result.push_back(pair.first);
        }
        
        return result;
    }

    // Configuration
    bool updateConfig(const ErrorHandlerConfig& config) override {
        if (!initialized_) {
            return initialize(config);
        }

        config_ = config;
        
        // Trim stored errors if necessary
        if (config_.maxStoredErrors > 0) {
            trimStoredErrors();
        }
        
        spdlog::info("Error handler configuration updated");
        return true;
    }

    ErrorHandlerConfig getConfig() const override {
        return config_;
    }

    // Statistics
    ErrorStatistics getStatistics() const override {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        ErrorStatistics stats;
        stats.totalErrors = errors_.size();
        stats.criticalErrors = 0;
        stats.warningErrors = 0;
        stats.infoErrors = 0;
        
        for (const auto& error : errors_) {
            switch (error.severity) {
                case ErrorSeverity::CRITICAL:
                    stats.criticalErrors++;
                    break;
                case ErrorSeverity::WARNING:
                    stats.warningErrors++;
                    break;
                case ErrorSeverity::INFO:
                    stats.infoErrors++;
                    break;
            }
        }
        
        stats.errorRate = calculateErrorRate();
        
        return stats;
    }

    void clearErrors() override {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        errors_.clear();
        errorCount_ = 0;
        spdlog::info("All errors cleared");
    }

    void clearErrorsByCategory(const std::string& category) override {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        auto it = std::remove_if(errors_.begin(), errors_.end(),
            [&category](const ErrorInfo& error) {
                return error.category == category;
            });
        
        size_t removed = std::distance(it, errors_.end());
        errors_.erase(it, errors_.end());
        
        spdlog::info("Cleared {} errors from category: {}", removed, category);
    }

    // Recovery and resilience
    bool attemptRecovery(const std::string& errorId) override {
        auto error = getError(errorId);
        if (!error) {
            spdlog::warn("Error not found for recovery attempt: {}", errorId);
            return false;
        }

        // Implement recovery logic based on error type
        // This is a simplified implementation
        spdlog::info("Attempting recovery for error: {} ({})", errorId, error->message);
        
        // Mark error as recovered (in a real implementation, you might have a status field)
        // For now, we'll just log the attempt
        spdlog::info("Recovery attempted for error: {}", errorId);
        
        return true;
    }

    void setRecoveryStrategy(const std::string& category, RecoveryStrategy strategy) override {
        std::lock_guard<std::mutex> lock(strategiesMutex_);
        recoveryStrategies_[category] = strategy;
        spdlog::debug("Recovery strategy set for category '{}': {}", category, static_cast<int>(strategy));
    }

    RecoveryStrategy getRecoveryStrategy(const std::string& category) const override {
        std::lock_guard<std::mutex> lock(strategiesMutex_);
        
        auto it = recoveryStrategies_.find(category);
        if (it != recoveryStrategies_.end()) {
            return it->second;
        }
        
        return RecoveryStrategy::LOG_ONLY;
    }

private:
    std::atomic<bool> initialized_;
    ErrorHandlerConfig config_;
    std::atomic<size_t> errorCount_;
    
    mutable std::mutex errorsMutex_;
    mutable std::mutex handlersMutex_;
    mutable std::mutex strategiesMutex_;
    
    std::vector<ErrorInfo> errors_;
    std::unordered_map<std::string, ErrorHandlerCallback> errorHandlers_;
    std::unordered_map<std::string, RecoveryStrategy> recoveryStrategies_;

    void setupDefaultHandlers() {
        // Register default critical error handler
        registerErrorHandler("default_critical", [](const ErrorInfo& error) {
            if (error.severity == ErrorSeverity::CRITICAL) {
                spdlog::critical("CRITICAL ERROR: {} in context: {}", error.message, error.context);
            }
        });
        
        // Register default warning handler
        registerErrorHandler("default_warning", [](const ErrorInfo& error) {
            if (error.severity == ErrorSeverity::WARNING) {
                spdlog::warn("WARNING: {} in context: {}", error.message, error.context);
            }
        });
    }

    void storeError(const ErrorInfo& error) {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        errors_.push_back(error);
        errorCount_++;
        
        // Trim if necessary
        if (config_.maxStoredErrors > 0 && errors_.size() > config_.maxStoredErrors) {
            errors_.erase(errors_.begin());
        }
    }

    void logError(const ErrorInfo& error) {
        switch (error.severity) {
            case ErrorSeverity::CRITICAL:
                spdlog::critical("[{}] {}: {} (Context: {})", error.id, error.category, error.message, error.context);
                break;
            case ErrorSeverity::WARNING:
                spdlog::warn("[{}] {}: {} (Context: {})", error.id, error.category, error.message, error.context);
                break;
            case ErrorSeverity::INFO:
                spdlog::info("[{}] {}: {} (Context: {})", error.id, error.category, error.message, error.context);
                break;
        }
    }

    void sendNotification(const ErrorInfo& error) {
        // In a real implementation, this would send notifications via email, SMS, etc.
        spdlog::debug("Notification sent for error: {} ({})", error.id, error.message);
    }

    void callErrorHandlers(const ErrorInfo& error) {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        
        for (const auto& pair : errorHandlers_) {
            try {
                pair.second(error);
            } catch (const std::exception& e) {
                spdlog::error("Error in error handler '{}': {}", pair.first, e.what());
            }
        }
    }

    void updateStatistics(const ErrorInfo& error) {
        // Update internal statistics
        // In a real implementation, you might track more detailed metrics
    }

    void trimStoredErrors() {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        
        if (config_.maxStoredErrors > 0 && errors_.size() > config_.maxStoredErrors) {
            size_t toRemove = errors_.size() - config_.maxStoredErrors;
            errors_.erase(errors_.begin(), errors_.begin() + toRemove);
        }
    }

    double calculateErrorRate() const {
        // Calculate errors per minute over the last hour
        auto oneHourAgo = std::chrono::system_clock::now() - std::chrono::hours(1);
        
        size_t recentErrors = 0;
        for (const auto& error : errors_) {
            if (error.timestamp >= oneHourAgo) {
                recentErrors++;
            }
        }
        
        return static_cast<double>(recentErrors) / 60.0; // errors per minute
    }

    std::string generateErrorId() const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::stringstream ss;
        ss << "err_";
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << dis(gen);
        }
        return ss.str();
    }
};

// Factory function implementation
std::unique_ptr<IErrorHandler> ErrorHandlerFactory::createHandler() {
    return std::make_unique<ErrorHandlerImpl>();
}

std::unique_ptr<IErrorHandler> ErrorHandlerFactory::createHandler(const ErrorHandlerConfig& config) {
    auto handler = std::make_unique<ErrorHandlerImpl>();
    if (!handler->initialize(config)) {
        return nullptr;
    }
    return handler;
}

} // namespace infrastructure
} // namespace server
} // namespace astrocomm
