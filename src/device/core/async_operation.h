#pragma once

#include <future>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace hydrogen {
namespace device {
namespace core {

using json = nlohmann::json;

/**
 * @brief Asynchronous operation states for ASCOM compliance
 */
enum class AsyncOperationState {
    IDLE = 0,
    RUNNING = 1,
    COMPLETED = 2,
    FAILED = 3,
    CANCELLED = 4
};

/**
 * @brief Base class for asynchronous operations
 * 
 * Provides ASCOM-compliant asynchronous operation support with
 * completion status tracking and cancellation capabilities.
 */
class AsyncOperation {
public:
    using CompletionCallback = std::function<void(const AsyncOperation&)>;
    using ProgressCallback = std::function<void(double progress)>;

    AsyncOperation(const std::string& operationId, const std::string& description = "");
    virtual ~AsyncOperation();

    // Operation control
    virtual void start() = 0;
    virtual void cancel();
    virtual bool isCompleted() const;
    virtual bool isRunning() const;
    virtual bool isCancelled() const;
    virtual bool hasFailed() const;

    // Status information
    std::string getOperationId() const { return operationId_; }
    std::string getDescription() const { return description_; }
    AsyncOperationState getState() const { return state_.load(); }
    double getProgress() const { return progress_.load(); }
    std::string getErrorMessage() const;
    std::chrono::system_clock::time_point getStartTime() const { return startTime_; }
    std::chrono::system_clock::time_point getEndTime() const { return endTime_; }
    std::chrono::milliseconds getDuration() const;

    // Callbacks
    void setCompletionCallback(CompletionCallback callback);
    void setProgressCallback(ProgressCallback callback);

    // Wait for completion
    bool waitForCompletion(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
    
    // Result handling
    virtual json getResult() const { return result_; }

protected:
    void setState(AsyncOperationState state);
    void setProgress(double progress);
    void setError(const std::string& error);
    void setResult(const json& result);
    void notifyCompletion();
    void notifyProgress();

    // Check if operation should be cancelled
    bool shouldCancel() const { return state_.load() == AsyncOperationState::CANCELLED; }

private:
    std::string operationId_;
    std::string description_;
    std::atomic<AsyncOperationState> state_;
    std::atomic<double> progress_;
    std::chrono::system_clock::time_point startTime_;
    std::chrono::system_clock::time_point endTime_;
    
    mutable std::mutex mutex_;
    std::condition_variable completionCV_;
    std::string errorMessage_;
    json result_;
    
    CompletionCallback completionCallback_;
    ProgressCallback progressCallback_;
};

/**
 * @brief Asynchronous operation manager
 * 
 * Manages multiple concurrent asynchronous operations for ASCOM compliance
 */
class AsyncOperationManager {
public:
    static AsyncOperationManager& getInstance();

    // Operation management
    void registerOperation(std::shared_ptr<AsyncOperation> operation);
    void unregisterOperation(const std::string& operationId);
    std::shared_ptr<AsyncOperation> getOperation(const std::string& operationId) const;
    std::vector<std::shared_ptr<AsyncOperation>> getAllOperations() const;
    std::vector<std::shared_ptr<AsyncOperation>> getRunningOperations() const;

    // Bulk operations
    void cancelAllOperations();
    void waitForAllOperations(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());
    
    // Statistics
    size_t getOperationCount() const;
    size_t getRunningOperationCount() const;

private:
    AsyncOperationManager() = default;
    ~AsyncOperationManager() = default;
    AsyncOperationManager(const AsyncOperationManager&) = delete;
    AsyncOperationManager& operator=(const AsyncOperationManager&) = delete;

    mutable std::mutex operationsMutex_;
    std::unordered_map<std::string, std::shared_ptr<AsyncOperation>> operations_;
};

/**
 * @brief Template for device-specific asynchronous operations
 */
template<typename ResultType>
class TypedAsyncOperation : public AsyncOperation {
public:
    using OperationFunction = std::function<ResultType()>;
    using TypedCompletionCallback = std::function<void(const TypedAsyncOperation<ResultType>&)>;

    TypedAsyncOperation(const std::string& operationId, 
                       OperationFunction operation,
                       const std::string& description = "")
        : AsyncOperation(operationId, description)
        , operation_(operation) {}

    void start() override {
        if (getState() != AsyncOperationState::IDLE) {
            return;
        }

        setState(AsyncOperationState::RUNNING);
        
        // Execute operation in separate thread
        operationFuture_ = std::async(std::launch::async, [this]() {
            try {
                if (shouldCancel()) {
                    setState(AsyncOperationState::CANCELLED);
                    return;
                }

                ResultType result = operation_();
                
                if (shouldCancel()) {
                    setState(AsyncOperationState::CANCELLED);
                    return;
                }

                typedResult_ = result;
                setResult(convertToJson(result));
                setState(AsyncOperationState::COMPLETED);
                
            } catch (const std::exception& e) {
                setError(e.what());
                setState(AsyncOperationState::FAILED);
            }
        });
    }

    ResultType getTypedResult() const {
        std::lock_guard<std::mutex> lock(resultMutex_);
        return typedResult_;
    }

    void setTypedCompletionCallback(TypedCompletionCallback callback) {
        typedCompletionCallback_ = callback;
        
        setCompletionCallback([this](const AsyncOperation&) {
            if (typedCompletionCallback_) {
                typedCompletionCallback_(*this);
            }
        });
    }

private:
    OperationFunction operation_;
    std::future<void> operationFuture_;
    TypedCompletionCallback typedCompletionCallback_;
    
    mutable std::mutex resultMutex_;
    ResultType typedResult_;

    json convertToJson(const ResultType& result) {
        if constexpr (std::is_same_v<ResultType, json>) {
            return result;
        } else if constexpr (std::is_arithmetic_v<ResultType>) {
            return json(result);
        } else if constexpr (std::is_same_v<ResultType, std::string>) {
            return json(result);
        } else if constexpr (std::is_same_v<ResultType, bool>) {
            return json(result);
        } else {
            // For complex types, assume they have a toJson() method
            return result.toJson();
        }
    }
};

/**
 * @brief ASCOM-specific asynchronous operation types
 */
using SlewOperation = TypedAsyncOperation<bool>;
using ExposureOperation = TypedAsyncOperation<std::vector<uint8_t>>;
using MoveOperation = TypedAsyncOperation<int>;
using TemperatureOperation = TypedAsyncOperation<double>;

/**
 * @brief Factory functions for common ASCOM async operations
 */
class AsyncOperationFactory {
public:
    static std::shared_ptr<SlewOperation> createSlewOperation(
        const std::string& deviceId,
        std::function<bool()> slewFunction,
        const std::string& description = "Telescope slew operation");

    static std::shared_ptr<ExposureOperation> createExposureOperation(
        const std::string& deviceId,
        std::function<std::vector<uint8_t>()> exposureFunction,
        const std::string& description = "Camera exposure operation");

    static std::shared_ptr<MoveOperation> createMoveOperation(
        const std::string& deviceId,
        std::function<int()> moveFunction,
        const std::string& description = "Device move operation");

    static std::shared_ptr<TemperatureOperation> createTemperatureOperation(
        const std::string& deviceId,
        std::function<double()> temperatureFunction,
        const std::string& description = "Temperature control operation");

private:
    static std::string generateOperationId(const std::string& deviceId, const std::string& operationType);
};

/**
 * @brief ASCOM async operation mixin for device classes
 * 
 * Provides common async operation functionality for ASCOM-compliant devices
 */
class ASCOMAsyncMixin {
public:
    virtual ~ASCOMAsyncMixin() = default;

protected:
    // Start an async operation and return its ID
    std::string startAsyncOperation(std::shared_ptr<AsyncOperation> operation);
    
    // Check if an async operation is complete
    bool isAsyncOperationComplete(const std::string& operationId) const;
    
    // Get async operation result
    json getAsyncOperationResult(const std::string& operationId) const;
    
    // Cancel an async operation
    void cancelAsyncOperation(const std::string& operationId);
    
    // Cancel all async operations for this device
    void cancelAllAsyncOperations();

private:
    std::vector<std::string> activeOperations_;
    mutable std::mutex operationsMutex_;
};

} // namespace core
} // namespace device
} // namespace hydrogen
