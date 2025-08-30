#include "async_operation.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace astrocomm {
namespace device {
namespace core {

// AsyncOperation Implementation
AsyncOperation::AsyncOperation(const std::string& operationId, const std::string& description)
    : operationId_(operationId)
    , description_(description)
    , state_(AsyncOperationState::IDLE)
    , progress_(0.0)
    , startTime_(std::chrono::system_clock::now())
    , endTime_(startTime_) {
}

AsyncOperation::~AsyncOperation() {
    if (state_.load() == AsyncOperationState::RUNNING) {
        cancel();
    }
}

void AsyncOperation::cancel() {
    AsyncOperationState expected = AsyncOperationState::RUNNING;
    if (state_.compare_exchange_strong(expected, AsyncOperationState::CANCELLED)) {
        endTime_ = std::chrono::system_clock::now();
        notifyCompletion();
        SPDLOG_INFO("Async operation '{}' cancelled", operationId_);
    }
}

bool AsyncOperation::isCompleted() const {
    auto state = state_.load();
    return state == AsyncOperationState::COMPLETED || 
           state == AsyncOperationState::FAILED || 
           state == AsyncOperationState::CANCELLED;
}

bool AsyncOperation::isRunning() const {
    return state_.load() == AsyncOperationState::RUNNING;
}

bool AsyncOperation::isCancelled() const {
    return state_.load() == AsyncOperationState::CANCELLED;
}

bool AsyncOperation::hasFailed() const {
    return state_.load() == AsyncOperationState::FAILED;
}

std::string AsyncOperation::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errorMessage_;
}

std::chrono::milliseconds AsyncOperation::getDuration() const {
    auto end = isCompleted() ? endTime_ : std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime_);
}

void AsyncOperation::setCompletionCallback(CompletionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    completionCallback_ = callback;
}

void AsyncOperation::setProgressCallback(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    progressCallback_ = callback;
}

bool AsyncOperation::waitForCompletion(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (isCompleted()) {
        return true;
    }
    
    if (timeout == std::chrono::milliseconds::zero()) {
        completionCV_.wait(lock, [this] { return isCompleted(); });
        return true;
    } else {
        return completionCV_.wait_for(lock, timeout, [this] { return isCompleted(); });
    }
}

void AsyncOperation::setState(AsyncOperationState state) {
    auto oldState = state_.exchange(state);
    
    if (state == AsyncOperationState::RUNNING && oldState == AsyncOperationState::IDLE) {
        startTime_ = std::chrono::system_clock::now();
        SPDLOG_DEBUG("Async operation '{}' started", operationId_);
    } else if (isCompleted() && oldState == AsyncOperationState::RUNNING) {
        endTime_ = std::chrono::system_clock::now();
        notifyCompletion();
        SPDLOG_DEBUG("Async operation '{}' completed with state {}", operationId_, static_cast<int>(state));
    }
}

void AsyncOperation::setProgress(double progress) {
    progress_.store(std::clamp(progress, 0.0, 100.0));
    notifyProgress();
}

void AsyncOperation::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorMessage_ = error;
    SPDLOG_ERROR("Async operation '{}' failed: {}", operationId_, error);
}

void AsyncOperation::setResult(const json& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = result;
}

void AsyncOperation::notifyCompletion() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (completionCallback_) {
        try {
            completionCallback_(*this);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Exception in completion callback for operation '{}': {}", operationId_, e.what());
        }
    }
    completionCV_.notify_all();
}

void AsyncOperation::notifyProgress() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (progressCallback_) {
        try {
            progressCallback_(progress_.load());
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Exception in progress callback for operation '{}': {}", operationId_, e.what());
        }
    }
}

// AsyncOperationManager Implementation
AsyncOperationManager& AsyncOperationManager::getInstance() {
    static AsyncOperationManager instance;
    return instance;
}

void AsyncOperationManager::registerOperation(std::shared_ptr<AsyncOperation> operation) {
    if (!operation) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(operationsMutex_);
    operations_[operation->getOperationId()] = operation;
    
    SPDLOG_DEBUG("Registered async operation '{}'", operation->getOperationId());
}

void AsyncOperationManager::unregisterOperation(const std::string& operationId) {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    auto it = operations_.find(operationId);
    if (it != operations_.end()) {
        operations_.erase(it);
        SPDLOG_DEBUG("Unregistered async operation '{}'", operationId);
    }
}

std::shared_ptr<AsyncOperation> AsyncOperationManager::getOperation(const std::string& operationId) const {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    auto it = operations_.find(operationId);
    return (it != operations_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<AsyncOperation>> AsyncOperationManager::getAllOperations() const {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    std::vector<std::shared_ptr<AsyncOperation>> result;
    result.reserve(operations_.size());
    
    for (const auto& pair : operations_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<std::shared_ptr<AsyncOperation>> AsyncOperationManager::getRunningOperations() const {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    std::vector<std::shared_ptr<AsyncOperation>> result;
    
    for (const auto& pair : operations_) {
        if (pair.second && pair.second->isRunning()) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

void AsyncOperationManager::cancelAllOperations() {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    for (const auto& pair : operations_) {
        if (pair.second && pair.second->isRunning()) {
            pair.second->cancel();
        }
    }
    SPDLOG_INFO("Cancelled all running async operations");
}

void AsyncOperationManager::waitForAllOperations(std::chrono::milliseconds timeout) {
    auto operations = getAllOperations();
    
    for (auto& operation : operations) {
        if (operation && operation->isRunning()) {
            operation->waitForCompletion(timeout);
        }
    }
}

size_t AsyncOperationManager::getOperationCount() const {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    return operations_.size();
}

size_t AsyncOperationManager::getRunningOperationCount() const {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    size_t count = 0;
    for (const auto& pair : operations_) {
        if (pair.second && pair.second->isRunning()) {
            ++count;
        }
    }
    return count;
}

// AsyncOperationFactory Implementation
std::shared_ptr<SlewOperation> AsyncOperationFactory::createSlewOperation(
    const std::string& deviceId,
    std::function<bool()> slewFunction,
    const std::string& description) {
    
    std::string operationId = generateOperationId(deviceId, "slew");
    return std::make_shared<SlewOperation>(operationId, slewFunction, description);
}

std::shared_ptr<ExposureOperation> AsyncOperationFactory::createExposureOperation(
    const std::string& deviceId,
    std::function<std::vector<uint8_t>()> exposureFunction,
    const std::string& description) {
    
    std::string operationId = generateOperationId(deviceId, "exposure");
    return std::make_shared<ExposureOperation>(operationId, exposureFunction, description);
}

std::shared_ptr<MoveOperation> AsyncOperationFactory::createMoveOperation(
    const std::string& deviceId,
    std::function<int()> moveFunction,
    const std::string& description) {
    
    std::string operationId = generateOperationId(deviceId, "move");
    return std::make_shared<MoveOperation>(operationId, moveFunction, description);
}

std::shared_ptr<TemperatureOperation> AsyncOperationFactory::createTemperatureOperation(
    const std::string& deviceId,
    std::function<double()> temperatureFunction,
    const std::string& description) {
    
    std::string operationId = generateOperationId(deviceId, "temperature");
    return std::make_shared<TemperatureOperation>(operationId, temperatureFunction, description);
}

std::string AsyncOperationFactory::generateOperationId(const std::string& deviceId, const std::string& operationType) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << deviceId << "_" << operationType << "_" 
       << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") 
       << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

// ASCOMAsyncMixin Implementation
std::string ASCOMAsyncMixin::startAsyncOperation(std::shared_ptr<AsyncOperation> operation) {
    if (!operation) {
        throw std::invalid_argument("Operation cannot be null");
    }
    
    std::lock_guard<std::mutex> lock(operationsMutex_);
    activeOperations_.push_back(operation->getOperationId());
    
    AsyncOperationManager::getInstance().registerOperation(operation);
    operation->start();
    
    return operation->getOperationId();
}

bool ASCOMAsyncMixin::isAsyncOperationComplete(const std::string& operationId) const {
    auto operation = AsyncOperationManager::getInstance().getOperation(operationId);
    return operation ? operation->isCompleted() : true;
}

json ASCOMAsyncMixin::getAsyncOperationResult(const std::string& operationId) const {
    auto operation = AsyncOperationManager::getInstance().getOperation(operationId);
    return operation ? operation->getResult() : json{};
}

void ASCOMAsyncMixin::cancelAsyncOperation(const std::string& operationId) {
    auto operation = AsyncOperationManager::getInstance().getOperation(operationId);
    if (operation) {
        operation->cancel();
    }
}

void ASCOMAsyncMixin::cancelAllAsyncOperations() {
    std::lock_guard<std::mutex> lock(operationsMutex_);
    for (const auto& operationId : activeOperations_) {
        cancelAsyncOperation(operationId);
    }
}

} // namespace core
} // namespace device
} // namespace astrocomm
