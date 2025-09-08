#pragma once

#include <hydrogen/core/communication/infrastructure/protocol_communicators.h>
#include <gmock/gmock.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace hydrogen {
namespace testing {

/**
 * @brief Mock STDIO communicator for testing without actual I/O operations
 * 
 * This mock communicator simulates STDIO operations without blocking on
 * actual stdin/stdout, making it suitable for unit tests.
 */
class MockStdioCommunicator : public hydrogen::core::StdioCommunicator {
public:
    explicit MockStdioCommunicator(const hydrogen::core::StdioConfig& config)
        : StdioCommunicator(config), running_(false), active_(false) {
        setupDefaultBehavior();
    }

    virtual ~MockStdioCommunicator() {
        stop();
    }

    // Override virtual methods from StdioCommunicator
    bool start() override {
        if (running_.load()) {
            return true;
        }
        
        running_.store(true);
        active_.store(true);
        
        // Simulate successful start without actually reading from stdin
        return true;
    }

    void stop() override {
        running_.store(false);
        active_.store(false);
        
        // Notify any waiting threads
        inputCondition_.notify_all();
    }

    bool isActive() const override {
        return active_.load();
    }

    bool sendMessage(const std::string& message) override {
        if (!active_.load()) {
            return false;
        }
        
        // Simulate sending message (store in sent messages for verification)
        std::lock_guard<std::mutex> lock(sentMessagesMutex_);
        sentMessages_.push_back(message);
        
        return true;
    }

    bool sendMessage(const nlohmann::json& message) override {
        return sendMessage(message.dump());
    }

    std::string readLine() override {
        if (!active_.load()) {
            return "";
        }

        std::unique_lock<std::mutex> lock(inputMutex_);
        
        // Wait for input with timeout (simulated)
        if (inputCondition_.wait_for(lock, config_.readTimeout, [this] {
            return !inputQueue_.empty() || !running_.load();
        })) {
            if (!inputQueue_.empty()) {
                std::string line = inputQueue_.front();
                inputQueue_.pop();
                return line;
            }
        }
        
        return ""; // Timeout or stopped
    }

    bool hasInput() const override {
        std::lock_guard<std::mutex> lock(inputMutex_);
        return !inputQueue_.empty();
    }

    uint64_t getLinesSent() const override {
        std::lock_guard<std::mutex> lock(sentMessagesMutex_);
        return sentMessages_.size();
    }

    uint64_t getLinesReceived() const override {
        std::lock_guard<std::mutex> lock(inputMutex_);
        return linesReceived_;
    }

    // Test helper methods
    void simulateInput(const std::string& input) {
        std::lock_guard<std::mutex> lock(inputMutex_);
        inputQueue_.push(input);
        linesReceived_++;
        inputCondition_.notify_one();
        
        // Call message handler if set
        if (messageHandler_) {
            messageHandler_(input);
        }
    }

    void simulateMultipleInputs(const std::vector<std::string>& inputs) {
        for (const auto& input : inputs) {
            simulateInput(input);
        }
    }

    std::vector<std::string> getSentMessages() const {
        std::lock_guard<std::mutex> lock(sentMessagesMutex_);
        return sentMessages_;
    }

    void clearSentMessages() {
        std::lock_guard<std::mutex> lock(sentMessagesMutex_);
        sentMessages_.clear();
    }

    void clearInputQueue() {
        std::lock_guard<std::mutex> lock(inputMutex_);
        while (!inputQueue_.empty()) {
            inputQueue_.pop();
        }
        linesReceived_ = 0;
    }

    // Simulate error conditions for testing
    void simulateError(const std::string& error) {
        if (errorHandler_) {
            errorHandler_(error);
        }
    }

private:
    void setupDefaultBehavior() {
        // Set up default mock behavior
    }

    // Mock state
    std::atomic<bool> running_;
    std::atomic<bool> active_;
    
    // Input simulation
    mutable std::mutex inputMutex_;
    std::queue<std::string> inputQueue_;
    std::condition_variable inputCondition_;
    uint64_t linesReceived_ = 0;
    
    // Output tracking
    mutable std::mutex sentMessagesMutex_;
    std::vector<std::string> sentMessages_;
};

/**
 * @brief Factory for creating mock STDIO communicators
 */
class MockStdioCommunicatorFactory {
public:
    static std::unique_ptr<MockStdioCommunicator> create(
        const hydrogen::core::StdioConfig& config = hydrogen::core::StdioConfig{}) {
        return std::make_unique<MockStdioCommunicator>(config);
    }
    
    static std::unique_ptr<MockStdioCommunicator> createWithDefaults() {
        hydrogen::core::StdioConfig config;
        config.enableLineBuffering = true;
        config.enableBinaryMode = false;
        config.lineTerminator = "\n";
        config.enableFlush = true;
        config.bufferSize = 4096;
        config.readTimeout = std::chrono::milliseconds(100); // Short timeout for tests
        config.writeTimeout = std::chrono::milliseconds(100);
        
        return create(config);
    }
};

} // namespace testing
} // namespace hydrogen
