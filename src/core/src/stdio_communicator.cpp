#include <hydrogen/core/protocol_communicators.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace hydrogen {
namespace core {

/**
 * @brief Concrete implementation of stdio communicator
 */
class StdioCommunicatorImpl : public StdioCommunicator {
public:
    StdioCommunicatorImpl(const StdioConfig& config)
        : StdioCommunicator(config), linesSent_(0), linesReceived_(0) {
        
        // Configure iostream settings based on config
        if (config_.enableLineBuffering) {
            std::cout.setf(std::ios::unitbuf);
        }
        
        if (config_.enableBinaryMode) {
            std::cin.setf(std::ios::binary);
            std::cout.setf(std::ios::binary);
        }
    }

    ~StdioCommunicatorImpl() override {
        stop();
    }

    bool start() override {
        if (running_.load()) {
            return true;
        }

        try {
            running_.store(true);
            active_.store(true);
            
            // Start input reading thread
            inputThread_ = std::thread(&StdioCommunicatorImpl::inputLoop, this);
            
            SPDLOG_INFO("Stdio communicator started");
            return true;
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Failed to start stdio communicator: {}", e.what());
            if (errorHandler_) {
                errorHandler_(e.what());
            }
            return false;
        }
    }

    void stop() override {
        if (!running_.load()) {
            return;
        }

        running_.store(false);
        active_.store(false);
        
        // Wake up input thread
        inputCondition_.notify_all();
        
        if (inputThread_.joinable()) {
            inputThread_.join();
        }
        
        SPDLOG_INFO("Stdio communicator stopped");
    }

    bool isActive() const override {
        return active_.load();
    }

    bool sendMessage(const std::string& message) override {
        if (!active_.load()) {
            return false;
        }

        try {
            std::lock_guard<std::mutex> lock(outputMutex_);
            
            if (config_.enableBinaryMode) {
                std::cout.write(message.c_str(), message.length());
            } else {
                std::cout << message;
                if (!message.empty() && message.back() != '\n' && !config_.lineTerminator.empty()) {
                    std::cout << config_.lineTerminator;
                }
            }
            
            if (config_.enableFlush) {
                std::cout.flush();
            }
            
            linesSent_.fetch_add(1);
            return true;
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Failed to send stdio message: {}", e.what());
            if (errorHandler_) {
                errorHandler_(e.what());
            }
            return false;
        }
    }

    bool sendMessage(const json& message) override {
        return sendMessage(message.dump());
    }

    std::string readLine() override {
        if (!active_.load()) {
            return "";
        }

        std::unique_lock<std::mutex> lock(inputMutex_);
        
        // Wait for input with timeout
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
        return linesSent_.load();
    }

    uint64_t getLinesReceived() const override {
        return linesReceived_.load();
    }

private:
    std::thread inputThread_;
    
    mutable std::mutex inputMutex_;
    mutable std::mutex outputMutex_;
    std::condition_variable inputCondition_;
    std::queue<std::string> inputQueue_;
    
    std::atomic<uint64_t> linesSent_;
    std::atomic<uint64_t> linesReceived_;

    void inputLoop() {
        std::string line;
        
        while (running_.load()) {
            try {
                if (config_.enableBinaryMode) {
                    // Binary mode - read fixed chunks
                    std::vector<char> buffer(config_.bufferSize);
                    std::cin.read(buffer.data(), buffer.size());
                    std::streamsize bytesRead = std::cin.gcount();
                    
                    if (bytesRead > 0) {
                        line.assign(buffer.data(), bytesRead);
                        processInputLine(line);
                    }
                } else {
                    // Line mode - read until line terminator
                    if (config_.lineTerminator == "\n") {
                        if (std::getline(std::cin, line)) {
                            processInputLine(line);
                        } else if (std::cin.eof()) {
                            break; // EOF reached
                        }
                    } else {
                        // Custom line terminator
                        char ch;
                        line.clear();
                        
                        while (std::cin.get(ch)) {
                            if (config_.lineTerminator.find(ch) != std::string::npos) {
                                if (!line.empty()) {
                                    processInputLine(line);
                                    line.clear();
                                }
                                break;
                            } else {
                                line += ch;
                                
                                // Prevent buffer overflow
                                if (line.length() >= config_.bufferSize) {
                                    processInputLine(line);
                                    line.clear();
                                }
                            }
                        }
                    }
                }
                
                // Small delay to prevent busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in stdio input loop: {}", e.what());
                if (errorHandler_) {
                    errorHandler_(e.what());
                }
                
                // Brief pause before retrying
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        SPDLOG_DEBUG("Stdio input loop terminated");
    }

    void processInputLine(const std::string& line) {
        if (line.empty() && !config_.enableBinaryMode) {
            return; // Skip empty lines in text mode
        }
        
        {
            std::lock_guard<std::mutex> lock(inputMutex_);
            inputQueue_.push(line);
            linesReceived_.fetch_add(1);
        }
        
        inputCondition_.notify_one();
        
        // Call message handler if set
        if (messageHandler_) {
            try {
                messageHandler_(line);
            } catch (const std::exception& e) {
                SPDLOG_ERROR("Error in stdio message handler: {}", e.what());
            }
        }
        
        // Echo if enabled
        if (config_.enableEcho && !config_.enableBinaryMode) {
            std::lock_guard<std::mutex> lock(outputMutex_);
            std::cout << "Echo: " << line << std::endl;
            if (config_.enableFlush) {
                std::cout.flush();
            }
        }
    }
};

// Factory method implementation
StdioCommunicator::StdioCommunicator(const StdioConfig& config) : config_(config) {}
StdioCommunicator::~StdioCommunicator() = default;

// External factory function
std::unique_ptr<StdioCommunicator> createStdioCommunicatorImpl(const StdioConfig& config) {
    return std::make_unique<StdioCommunicatorImpl>(config);
}

} // namespace core
} // namespace hydrogen
