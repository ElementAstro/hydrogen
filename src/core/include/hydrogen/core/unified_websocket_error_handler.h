#pragma once

#ifdef HYDROGEN_HAS_WEBSOCKETS

#include "websocket_error_handler.h"
#include "protocol_error_mapper.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>

namespace hydrogen {
namespace core {

using json = nlohmann::json;

/**
 * @brief WebSocket connection context for error handling
 */
struct WebSocketConnectionContext {
    std::string connectionId;
    std::string componentName;
    std::string endpoint;
    bool isClient = true;
    std::chrono::system_clock::time_point connectionStartTime;
    std::chrono::system_clock::time_point lastActivityTime;
    size_t reconnectAttempts = 0;
    json metadata;
};

/**
 * @brief Enhanced WebSocket error with connection context
 */
struct EnhancedWebSocketError : public WebSocketError {
    WebSocketConnectionContext connectionContext;
    std::string correlationId;
    std::vector<std::string> errorChain; // For tracking cascading errors
    bool isRetryable = true;
    std::chrono::milliseconds suggestedRetryDelay{1000};
    
    // Enhanced utility methods
    json toDetailedJson() const;
    bool shouldTriggerCircuitBreaker() const;
    std::string getErrorFingerprint() const;
};

/**
 * @brief WebSocket error event for notifications
 */
struct WebSocketErrorEvent {
    EnhancedWebSocketError error;
    WebSocketRecoveryAction actionTaken;
    bool recoverySuccessful = false;
    std::chrono::system_clock::time_point eventTime;
    std::string eventId;
};

/**
 * @brief Circuit breaker for WebSocket connections
 */
class WebSocketCircuitBreaker {
public:
    enum class State {
        CLOSED,    // Normal operation
        OPEN,      // Failing, blocking requests
        HALF_OPEN  // Testing if service recovered
    };
    
    explicit WebSocketCircuitBreaker(const std::string& connectionId);
    
    // Circuit breaker operations
    bool canAttemptConnection() const;
    void recordSuccess();
    void recordFailure();
    void reset();
    
    // Configuration
    void setFailureThreshold(size_t threshold) { failureThreshold_ = threshold; }
    void setRecoveryTimeout(std::chrono::milliseconds timeout) { recoveryTimeout_ = timeout; }
    void setSuccessThreshold(size_t threshold) { successThreshold_ = threshold; }
    
    // Status
    State getState() const { return state_; }
    size_t getFailureCount() const { return failureCount_; }
    std::chrono::system_clock::time_point getLastFailureTime() const { return lastFailureTime_; }

private:
    std::string connectionId_;
    std::atomic<State> state_{State::CLOSED};
    std::atomic<size_t> failureCount_{0};
    std::atomic<size_t> successCount_{0};
    std::chrono::system_clock::time_point lastFailureTime_;
    
    size_t failureThreshold_ = 5;
    size_t successThreshold_ = 3;
    std::chrono::milliseconds recoveryTimeout_{30000};
    
    mutable std::mutex stateMutex_;
};

/**
 * @brief Unified WebSocket error handler that standardizes error handling across all components
 */
class UnifiedWebSocketErrorHandler : public WebSocketErrorHandler {
public:
    using ErrorEventCallback = std::function<void(const WebSocketErrorEvent&)>;
    using RecoveryStrategyCallback = std::function<WebSocketRecoveryAction(const EnhancedWebSocketError&)>;
    using ConnectionRecoveryCallback = std::function<bool(const std::string& connectionId, WebSocketRecoveryAction action)>;
    
    UnifiedWebSocketErrorHandler();
    ~UnifiedWebSocketErrorHandler() override = default;
    
    // Connection management
    void registerConnection(const WebSocketConnectionContext& context);
    void unregisterConnection(const std::string& connectionId);
    void updateConnectionActivity(const std::string& connectionId);
    
    // Enhanced error handling
    void handleError(const WebSocketError& error) override;
    void handleEnhancedError(const EnhancedWebSocketError& error);
    WebSocketRecoveryAction determineRecoveryAction(const WebSocketError& error) override;
    bool shouldRetry(const WebSocketError& error, int attemptCount) override;
    std::chrono::milliseconds getRetryDelay(const WebSocketError& error, int attemptCount) override;
    
    // Error correlation and analysis
    void correlateError(const std::string& correlationId, const EnhancedWebSocketError& error);
    std::vector<EnhancedWebSocketError> getCorrelatedErrors(const std::string& correlationId) const;
    
    // Circuit breaker management
    std::shared_ptr<WebSocketCircuitBreaker> getCircuitBreaker(const std::string& connectionId);
    void resetCircuitBreaker(const std::string& connectionId);
    
    // Callbacks and configuration
    void setErrorEventCallback(ErrorEventCallback callback) { errorEventCallback_ = callback; }
    void setRecoveryStrategyCallback(RecoveryStrategyCallback callback) { recoveryStrategyCallback_ = callback; }
    void setConnectionRecoveryCallback(ConnectionRecoveryCallback callback) { connectionRecoveryCallback_ = callback; }
    
    // Configuration
    void setGlobalRetryPolicy(int maxAttempts, std::chrono::milliseconds baseDelay, bool exponentialBackoff = true);
    void setConnectionSpecificRetryPolicy(const std::string& connectionId, int maxAttempts, std::chrono::milliseconds baseDelay);
    void enableCircuitBreaker(bool enable) { circuitBreakerEnabled_ = enable; }
    void setErrorCorrelationWindow(std::chrono::milliseconds window) { correlationWindow_ = window; }
    
    // Statistics and monitoring
    struct UnifiedErrorStatistics {
        size_t totalErrors = 0;
        size_t connectionErrors = 0;
        size_t protocolErrors = 0;
        size_t timeoutErrors = 0;
        size_t messageErrors = 0;
        size_t authenticationErrors = 0;
        size_t networkErrors = 0;
        size_t unknownErrors = 0;
        
        size_t retriesAttempted = 0;
        size_t successfulRecoveries = 0;
        size_t failedRecoveries = 0;
        size_t circuitBreakerTrips = 0;
        
        std::chrono::system_clock::time_point lastErrorTime;
        std::chrono::system_clock::time_point lastRecoveryTime;
        
        double averageRecoveryTime = 0.0;
        std::unordered_map<std::string, size_t> errorsByConnection;
        std::unordered_map<std::string, size_t> errorsByComponent;
    };
    
    UnifiedErrorStatistics getStatistics() const;
    void resetStatistics();
    
    // Error reporting and analysis
    json generateErrorReport(const std::string& connectionId = "") const;
    std::vector<std::string> getTopErrorPatterns(size_t limit = 10) const;
    bool isConnectionHealthy(const std::string& connectionId) const;

private:
    // Connection tracking
    mutable std::mutex connectionsMutex_;
    std::unordered_map<std::string, WebSocketConnectionContext> connections_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketCircuitBreaker>> circuitBreakers_;
    
    // Error correlation
    mutable std::mutex correlationMutex_;
    std::unordered_map<std::string, std::vector<EnhancedWebSocketError>> correlatedErrors_;
    std::chrono::milliseconds correlationWindow_{5000};
    
    // Statistics
    mutable std::mutex statisticsMutex_;
    UnifiedErrorStatistics statistics_;
    
    // Configuration
    struct RetryPolicy {
        int maxAttempts = 3;
        std::chrono::milliseconds baseDelay{1000};
        std::chrono::milliseconds maxDelay{30000};
        bool exponentialBackoff = true;
    };
    
    RetryPolicy globalRetryPolicy_;
    std::unordered_map<std::string, RetryPolicy> connectionRetryPolicies_;
    bool circuitBreakerEnabled_ = true;
    
    // Callbacks
    ErrorEventCallback errorEventCallback_;
    RecoveryStrategyCallback recoveryStrategyCallback_;
    ConnectionRecoveryCallback connectionRecoveryCallback_;
    
    // Protocol error mapper for cross-protocol error handling
    std::shared_ptr<ProtocolErrorMapper> protocolErrorMapper_;
    
    // Internal methods
    EnhancedWebSocketError enhanceError(const WebSocketError& error);
    void updateStatistics(const EnhancedWebSocketError& error, WebSocketRecoveryAction action, bool recoverySuccessful);
    void cleanupOldCorrelations();
    std::string generateCorrelationId() const;
    std::string generateEventId() const;
    RetryPolicy getRetryPolicy(const std::string& connectionId) const;
    bool executeRecoveryAction(const std::string& connectionId, WebSocketRecoveryAction action);
    void notifyErrorEvent(const WebSocketErrorEvent& event);
};

/**
 * @brief Factory for creating unified WebSocket error handlers
 */
class UnifiedWebSocketErrorHandlerFactory {
public:
    static std::shared_ptr<UnifiedWebSocketErrorHandler> createHandler();
    static std::shared_ptr<UnifiedWebSocketErrorHandler> createHandlerWithDefaults();
    
    // Preconfigured handlers
    static std::shared_ptr<UnifiedWebSocketErrorHandler> createClientHandler();
    static std::shared_ptr<UnifiedWebSocketErrorHandler> createServerHandler();
    static std::shared_ptr<UnifiedWebSocketErrorHandler> createHighAvailabilityHandler();
    static std::shared_ptr<UnifiedWebSocketErrorHandler> createDevelopmentHandler();
};

/**
 * @brief Global unified WebSocket error handler registry
 */
class UnifiedWebSocketErrorRegistry {
public:
    static UnifiedWebSocketErrorRegistry& getInstance();
    
    // Handler management
    void setGlobalHandler(std::shared_ptr<UnifiedWebSocketErrorHandler> handler);
    std::shared_ptr<UnifiedWebSocketErrorHandler> getGlobalHandler();
    
    void registerComponentHandler(const std::string& component, std::shared_ptr<UnifiedWebSocketErrorHandler> handler);
    void unregisterComponentHandler(const std::string& component);
    std::shared_ptr<UnifiedWebSocketErrorHandler> getComponentHandler(const std::string& component);
    
    // Convenience methods for error handling
    void handleError(const WebSocketError& error, const std::string& component = "");
    void handleEnhancedError(const EnhancedWebSocketError& error, const std::string& component = "");
    
    // Global statistics
    UnifiedWebSocketErrorHandler::UnifiedErrorStatistics getGlobalStatistics() const;
    json generateGlobalErrorReport() const;

private:
    UnifiedWebSocketErrorRegistry() = default;
    
    mutable std::mutex handlersMutex_;
    std::shared_ptr<UnifiedWebSocketErrorHandler> globalHandler_;
    std::unordered_map<std::string, std::shared_ptr<UnifiedWebSocketErrorHandler>> componentHandlers_;
};

/**
 * @brief Convenience macros for unified WebSocket error handling
 */
#define HANDLE_WEBSOCKET_ERROR(error, component) \
    UnifiedWebSocketErrorRegistry::getInstance().handleError(error, component)

#define HANDLE_ENHANCED_WEBSOCKET_ERROR(error, component) \
    UnifiedWebSocketErrorRegistry::getInstance().handleEnhancedError(error, component);

#define CREATE_WEBSOCKET_CONNECTION_CONTEXT(id, component, endpoint, isClient) \
    WebSocketConnectionContext{id, component, endpoint, isClient, \
                              std::chrono::system_clock::now(), \
                              std::chrono::system_clock::now(), 0, json::object()}

} // namespace core
} // namespace hydrogen

#endif // HYDROGEN_HAS_WEBSOCKETS
