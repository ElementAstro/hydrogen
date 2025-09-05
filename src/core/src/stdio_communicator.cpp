#include <chrono>
#include <condition_variable>
#include <hydrogen/core/protocol_communicators.h>
#include <hydrogen/core/stdio_logger.h>
#include <iostream>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

namespace hydrogen {
namespace core {

/**
 * @brief Connection state enumeration
 */
enum class ConnectionState {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  RECONNECTING,
  ERROR,
  SHUTDOWN
};

/**
 * @brief Concrete implementation of stdio communicator with enhanced error
 * handling
 */
class StdioCommunicatorImpl : public StdioCommunicator {
public:
  StdioCommunicatorImpl(const StdioConfig &config)
      : StdioCommunicator(config), linesSent_(0), linesReceived_(0),
        connectionState_(ConnectionState::DISCONNECTED), reconnectAttempts_(0),
        maxReconnectAttempts_(5), reconnectDelay_(1000),
        lastErrorTime_(std::chrono::steady_clock::now()) {

    // Configure iostream settings based on config
    if (config_.enableLineBuffering) {
      std::cout.setf(std::ios::unitbuf);
    }

    if (config_.enableBinaryMode) {
      std::cin.setf(std::ios::binary);
      std::cout.setf(std::ios::binary);
    }

    // Initialize error recovery settings
    errorRecoveryEnabled_ = true;
    maxConsecutiveErrors_ = 10;
    errorResetInterval_ = std::chrono::minutes(5);
  }

  ~StdioCommunicatorImpl() override { stop(); }

  bool start() override {
    if (running_.load()) {
      return true;
    }

    try {
      connectionState_ = ConnectionState::CONNECTING;
      running_.store(true);

      // Reset error counters
      consecutiveErrors_ = 0;
      reconnectAttempts_ = 0;

      // Start input reading thread
      inputThread_ = std::thread(&StdioCommunicatorImpl::inputLoop, this);

      // Start monitoring thread for connection health
      monitoringThread_ =
          std::thread(&StdioCommunicatorImpl::monitoringLoop, this);

      connectionState_ = ConnectionState::CONNECTED;
      active_.store(true);

      STDIO_LOG_INFO("Stdio communicator started successfully", "");
      return true;
    } catch (const std::exception &e) {
      connectionState_ = ConnectionState::ERROR;
      running_.store(false);
      active_.store(false);

      STDIO_LOG_ERROR(
          "Failed to start stdio communicator: " + std::string(e.what()), "");
      handleError("Startup failed: " + std::string(e.what()));
      return false;
    }
  }

  void stop() override {
    if (!running_.load()) {
      return;
    }

    connectionState_ = ConnectionState::SHUTDOWN;
    running_.store(false);
    active_.store(false);

    // Wake up all threads
    inputCondition_.notify_all();
    monitoringCondition_.notify_all();

    // Join threads safely
    if (inputThread_.joinable()) {
      inputThread_.join();
    }

    if (monitoringThread_.joinable()) {
      monitoringThread_.join();
    }

    // Clear queues
    {
      std::lock_guard<std::mutex> lock(inputMutex_);
      std::queue<std::string> empty;
      inputQueue_.swap(empty);
    }

    connectionState_ = ConnectionState::DISCONNECTED;
    SPDLOG_INFO("Stdio communicator stopped gracefully");
  }

  bool isActive() const override { return active_.load(); }

  bool sendMessage(const std::string &message) override {
    if (!active_.load() || connectionState_ != ConnectionState::CONNECTED) {
      SPDLOG_WARN(
          "Cannot send message: communicator not active or not connected");
      return false;
    }

    // Check message size limits
    if (message.size() >
        config_.bufferSize * 10) { // Allow up to 10x buffer size
      SPDLOG_ERROR("Message too large: {} bytes", message.size());
      handleError("Message size exceeds limit");
      return false;
    }

    try {
      std::lock_guard<std::mutex> lock(outputMutex_);

      // Check if stdout is still valid
      if (std::cout.bad()) {
        handleError("Output stream is in bad state");
        return false;
      }

      if (config_.enableBinaryMode) {
        std::cout.write(message.c_str(), message.length());
      } else {
        std::cout << message;
        if (!message.empty() && message.back() != '\n' &&
            !config_.lineTerminator.empty()) {
          std::cout << config_.lineTerminator;
        }
      }

      if (config_.enableFlush) {
        std::cout.flush();
      }

      // Check for write errors
      if (std::cout.fail()) {
        handleError("Failed to write to stdout");
        return false;
      }

      linesSent_.fetch_add(1);
      resetErrorState(); // Reset error state on successful operation
      return true;
    } catch (const std::exception &e) {
      std::string error =
          "Failed to send stdio message: " + std::string(e.what());
      SPDLOG_ERROR(error);
      handleError(error);
      return false;
    }
  }

  bool sendMessage(const json &message) override {
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

  uint64_t getLinesSent() const override { return linesSent_.load(); }

  uint64_t getLinesReceived() const override { return linesReceived_.load(); }

private:
  std::thread inputThread_;
  std::thread monitoringThread_;

  mutable std::mutex inputMutex_;
  mutable std::mutex outputMutex_;
  mutable std::mutex stateMutex_;
  std::condition_variable inputCondition_;
  std::condition_variable monitoringCondition_;
  std::queue<std::string> inputQueue_;

  std::atomic<uint64_t> linesSent_;
  std::atomic<uint64_t> linesReceived_;

  // Connection state management
  ConnectionState connectionState_;
  std::atomic<int> reconnectAttempts_;
  int maxReconnectAttempts_;
  std::chrono::milliseconds reconnectDelay_;

  // Error handling
  std::atomic<int> consecutiveErrors_;
  int maxConsecutiveErrors_;
  bool errorRecoveryEnabled_;
  std::chrono::steady_clock::time_point lastErrorTime_;
  std::chrono::milliseconds errorResetInterval_;

  // Helper methods
  void handleError(const std::string &error) {
    consecutiveErrors_.fetch_add(1);
    lastErrorTime_ = std::chrono::steady_clock::now();

    STDIO_LOG_ERROR("Stdio communicator error: " + error, "");
    getGlobalStdioLogger().recordError("stdio_communicator");

    if (errorHandler_) {
      errorHandler_(error);
    }

    // Check if we should attempt recovery
    if (errorRecoveryEnabled_ &&
        consecutiveErrors_.load() >= maxConsecutiveErrors_) {
      STDIO_LOG_WARN("Too many consecutive errors (" +
                         std::to_string(consecutiveErrors_.load()) +
                         "), attempting recovery",
                     "");
      attemptRecovery();
    }
  }

  void resetErrorState() { consecutiveErrors_.store(0); }

  void attemptRecovery() {
    if (connectionState_ == ConnectionState::SHUTDOWN) {
      return;
    }

    connectionState_ = ConnectionState::RECONNECTING;

    // Clear error state
    std::cout.clear();
    std::cin.clear();

    // Reset error counters after successful recovery attempt
    if (std::chrono::steady_clock::now() - lastErrorTime_ >
        errorResetInterval_) {
      resetErrorState();
    }

    connectionState_ = ConnectionState::CONNECTED;
    SPDLOG_INFO("Stdio communicator recovery attempt completed");
  }

  void monitoringLoop() {
    while (running_.load()) {
      try {
        std::unique_lock<std::mutex> lock(stateMutex_);
        monitoringCondition_.wait_for(lock, std::chrono::seconds(5));

        if (!running_.load()) {
          break;
        }

        // Check connection health
        if (connectionState_ == ConnectionState::CONNECTED) {
          // Perform health checks
          if (std::cout.bad() || std::cin.bad()) {
            handleError("Stream in bad state detected by monitor");
          }
        }

        // Reset error state if enough time has passed
        if (consecutiveErrors_.load() > 0 &&
            std::chrono::steady_clock::now() - lastErrorTime_ >
                errorResetInterval_) {
          resetErrorState();
          SPDLOG_INFO("Error state reset after timeout");
        }

      } catch (const std::exception &e) {
        SPDLOG_ERROR("Error in monitoring loop: {}", e.what());
      }
    }
  }

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

        // Check for read errors
        if (std::cin.fail() && !std::cin.eof()) {
          handleError("Input stream error detected");
          std::cin.clear(); // Clear error flags
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

      } catch (const std::exception &e) {
        std::string error =
            "Error in stdio input loop: " + std::string(e.what());
        SPDLOG_ERROR(error);
        handleError(error);

        // Brief pause before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    SPDLOG_DEBUG("Stdio input loop terminated");
  }

  void processInputLine(const std::string &line) {
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
      } catch (const std::exception &e) {
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
StdioCommunicator::StdioCommunicator(const StdioConfig &config)
    : config_(config) {}
StdioCommunicator::~StdioCommunicator() = default;

// External factory function
std::unique_ptr<StdioCommunicator>
createStdioCommunicatorImpl(const StdioConfig &config) {
  return std::make_unique<StdioCommunicatorImpl>(config);
}

} // namespace core
} // namespace hydrogen
