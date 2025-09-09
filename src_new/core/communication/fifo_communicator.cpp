#include "hydrogen/core/communication/protocols/fifo_communicator.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <winsock2.h>  // For htonl, ntohl on Windows
#else
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>  // For htonl, ntohl on Unix
#endif

// Undefine Windows ERROR macro to avoid conflicts
#ifdef ERROR
#undef ERROR
#endif

namespace hydrogen {
namespace core {

// FifoStatistics implementation
double FifoStatistics::getMessagesPerSecond() const {
  auto now = std::chrono::system_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
  if (duration.count() == 0)
    return 0.0;
  return static_cast<double>(messagesSent.load() + messagesReceived.load()) /
         duration.count();
}

double FifoStatistics::getBytesPerSecond() const {
  auto now = std::chrono::system_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
  if (duration.count() == 0)
    return 0.0;
  return static_cast<double>(bytesTransferred.load()) / duration.count();
}

std::chrono::milliseconds FifoStatistics::getUptime() const {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
}

json FifoStatistics::toJson() const {
  json j;
  j["messagesSent"] = messagesSent.load();
  j["messagesReceived"] = messagesReceived.load();
  j["bytesTransferred"] = bytesTransferred.load();
  j["connectionAttempts"] = connectionAttempts.load();
  j["reconnectionAttempts"] = reconnectionAttempts.load();
  j["errors"] = errors.load();
  j["messagesPerSecond"] = getMessagesPerSecond();
  j["bytesPerSecond"] = getBytesPerSecond();
  j["uptimeMs"] = getUptime().count();
  return j;
}

// FifoCommunicator base class implementation
FifoCommunicator::FifoCommunicator(const FifoConfig &config) : config_(config) {
  spdlog::debug("FifoCommunicator created with pipe: {}",
                config_.pipeType == FifoPipeType::WINDOWS_NAMED_PIPE
                    ? config_.windowsPipePath
                    : config_.unixPipePath);
}

FifoCommunicator::~FifoCommunicator() { stop(); }

void FifoCommunicator::handleError(const std::string &error) {
  spdlog::error("FIFO error: {}", error);
  if (errorHandler_) {
    errorHandler_(error);
  }
}

void FifoCommunicator::handleConnection(bool connected) {
  spdlog::info("FIFO connection state changed: {}",
               connected ? "connected" : "disconnected");
  if (connectionHandler_) {
    connectionHandler_(connected);
  }
}

void FifoCommunicator::processMessage(const std::string &message) {
  if (messageHandler_) {
    messageHandler_(message);
  }
}

std::string FifoCommunicator::formatMessage(const std::string &message) const {
  switch (config_.framingMode) {
  case FifoFramingMode::NEWLINE_DELIMITED:
    return message + config_.lineTerminator;
  case FifoFramingMode::LENGTH_PREFIXED: {
    uint32_t length = static_cast<uint32_t>(message.length());
    std::string result(reinterpret_cast<const char *>(&length), sizeof(length));
    result += message;
    return result;
  }
  case FifoFramingMode::JSON_LINES:
    return message + "\n";
  case FifoFramingMode::CUSTOM_DELIMITER:
    return message + config_.customDelimiter;
  case FifoFramingMode::NULL_TERMINATED:
    return message + '\0';
  case FifoFramingMode::BINARY_LENGTH_PREFIXED: {
    uint32_t length = htonl(static_cast<uint32_t>(message.length()));
    std::string result(reinterpret_cast<const char *>(&length), sizeof(length));
    result += message;
    return result;
  }
  default:
    return message + "\n";
  }
}

std::string
FifoCommunicator::parseMessage(const std::string &rawMessage) const {
  // Remove framing based on mode
  switch (config_.framingMode) {
  case FifoFramingMode::NEWLINE_DELIMITED:
  case FifoFramingMode::JSON_LINES: {
    std::string result = rawMessage;
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
    return result;
  }
  case FifoFramingMode::LENGTH_PREFIXED:
  case FifoFramingMode::BINARY_LENGTH_PREFIXED: {
    if (rawMessage.length() < sizeof(uint32_t)) {
      return rawMessage;
    }
    return rawMessage.substr(sizeof(uint32_t));
  }
  case FifoFramingMode::CUSTOM_DELIMITER: {
    std::string result = rawMessage;
    size_t pos = result.find(config_.customDelimiter);
    if (pos != std::string::npos) {
      result = result.substr(0, pos);
    }
    return result;
  }
  case FifoFramingMode::NULL_TERMINATED: {
    std::string result = rawMessage;
    size_t pos = result.find('\0');
    if (pos != std::string::npos) {
      result = result.substr(0, pos);
    }
    return result;
  }
  default:
    return rawMessage;
  }
}

// FifoCommunicatorImpl implementation
FifoCommunicatorImpl::FifoCommunicatorImpl(const FifoConfig &config)
    : FifoCommunicator(config),
      lastReconnectAttempt_(std::chrono::system_clock::now()) {

#ifdef _WIN32
  // Initialize Windows overlapped structures
  memset(&readOverlapped_, 0, sizeof(readOverlapped_));
  memset(&writeOverlapped_, 0, sizeof(writeOverlapped_));
  readOverlapped_.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  writeOverlapped_.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
}

FifoCommunicatorImpl::~FifoCommunicatorImpl() {
  stop();
  cleanup();

#ifdef _WIN32
  if (readOverlapped_.hEvent != NULL) {
    CloseHandle(readOverlapped_.hEvent);
  }
  if (writeOverlapped_.hEvent != NULL) {
    CloseHandle(writeOverlapped_.hEvent);
  }
#endif
}

bool FifoCommunicatorImpl::start() {
  if (active_.load()) {
    return true;
  }

  spdlog::info("Starting FIFO communicator");

  if (!createPipe()) {
    handleError("Failed to create FIFO pipe");
    return false;
  }

  if (!connect()) {
    handleError("Failed to connect to FIFO pipe");
    return false;
  }

  active_.store(true);
  running_.store(true);

  // Start worker threads
  readerThread_ = std::make_unique<std::thread>(
      &FifoCommunicatorImpl::readerThreadFunction, this);
  writerThread_ = std::make_unique<std::thread>(
      &FifoCommunicatorImpl::writerThreadFunction, this);

  if (config_.enableAutoReconnect) {
    reconnectThread_ = std::make_unique<std::thread>(
        &FifoCommunicatorImpl::reconnectThreadFunction, this);
  }

  statistics_.startTime = std::chrono::system_clock::now();
  handleConnection(true);

  spdlog::info("FIFO communicator started successfully");
  return true;
}

void FifoCommunicatorImpl::stop() {
  if (!active_.load()) {
    return;
  }

  spdlog::info("Stopping FIFO communicator");

  active_.store(false);
  running_.store(false);

  // Stop threads
  stopThreads();

  // Close pipe
  disconnect();
  closePipe();

  // Clear queues
  clearQueues();

  handleConnection(false);
  spdlog::info("FIFO communicator stopped");
}

bool FifoCommunicatorImpl::isActive() const { return active_.load(); }

bool FifoCommunicatorImpl::isConnected() const {
  return connectionState_.load() == FifoConnectionState::CONNECTED;
}

bool FifoCommunicatorImpl::sendMessage(const std::string &message) {
  if (!isActive() || !isConnected()) {
    return false;
  }

  if (message.length() > config_.maxMessageSize) {
    handleError("Message too large: " + std::to_string(message.length()) +
                " bytes");
    return false;
  }

  std::string framedMessage = frameMessage(message);
  queueOutgoingMessage(framedMessage);

  return true;
}

bool FifoCommunicatorImpl::sendMessage(const json &message) {
  return sendMessage(message.dump());
}

std::string FifoCommunicatorImpl::readMessage() {
  return dequeueIncomingMessage();
}

bool FifoCommunicatorImpl::hasMessage() const {
  std::lock_guard<std::mutex> lock(incomingMutex_);
  return !incomingMessages_.empty();
}

bool FifoCommunicatorImpl::connect() {
  connectionState_.store(FifoConnectionState::CONNECTING);
  statistics_.connectionAttempts.fetch_add(1);

  if (attemptConnection()) {
    connectionState_.store(FifoConnectionState::CONNECTED);
    resetCircuitBreaker();
    return true;
  } else {
    connectionState_.store(FifoConnectionState::ERROR);
    incrementErrorCount();
    return false;
  }
}

void FifoCommunicatorImpl::disconnect() {
  connectionState_.store(FifoConnectionState::DISCONNECTED);
  closePipe();
}

bool FifoCommunicatorImpl::reconnect() {
  if (!config_.enableAutoReconnect) {
    return false;
  }

  if (reconnectAttempts_.load() >= config_.maxReconnectAttempts) {
    spdlog::warn("Maximum reconnect attempts reached");
    return false;
  }

  connectionState_.store(FifoConnectionState::RECONNECTING);
  statistics_.reconnectionAttempts.fetch_add(1);
  reconnectAttempts_.fetch_add(1);

  // Wait for reconnect delay
  std::this_thread::sleep_for(config_.reconnectDelay);

  if (connect()) {
    reconnectAttempts_.store(0);
    return true;
  }

  return false;
}

FifoConnectionState FifoCommunicatorImpl::getConnectionState() const {
  return connectionState_.load();
}

FifoStatistics FifoCommunicatorImpl::getStatistics() const {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  return statistics_;
}

bool FifoCommunicatorImpl::isHealthy() const {
  return isConnected() && !isCircuitBreakerOpen() && performHealthCheck();
}

std::string FifoCommunicatorImpl::getHealthStatus() const {
  if (!isActive()) {
    return "INACTIVE";
  }
  if (!isConnected()) {
    return "DISCONNECTED";
  }
  if (isCircuitBreakerOpen()) {
    return "CIRCUIT_BREAKER_OPEN";
  }
  if (!performHealthCheck()) {
    return "UNHEALTHY";
  }
  return "HEALTHY";
}

bool FifoCommunicatorImpl::enableBidirectional() {
  // Implementation depends on creating paired pipes
  return config_.accessMode == FifoAccessMode::DUPLEX ||
         config_.accessMode == FifoAccessMode::READ_WRITE;
}

bool FifoCommunicatorImpl::enableMultiplexing() {
  return config_.enableMultiplexing && config_.maxConcurrentConnections > 1;
}

std::vector<std::string> FifoCommunicatorImpl::getConnectedClients() const {
  // For basic FIFO, there's typically one connection
  // Multiplexing would require additional implementation
  if (isConnected()) {
    return {"default_client"};
  }
  return {};
}

// Platform-specific pipe creation
bool FifoCommunicatorImpl::createPipe() {
#ifdef _WIN32
  return createWindowsNamedPipe();
#else
  return createUnixFifo();
#endif
}

bool FifoCommunicatorImpl::openPipe() {
#ifdef _WIN32
  return openWindowsNamedPipe();
#else
  return openUnixFifo();
#endif
}

void FifoCommunicatorImpl::closePipe() {
#ifdef _WIN32
  closeWindowsNamedPipe();
#else
  closeUnixFifo();
#endif
}

bool FifoCommunicatorImpl::isPipeValid() const {
#ifdef _WIN32
  return readHandle_ != INVALID_HANDLE_VALUE ||
         writeHandle_ != INVALID_HANDLE_VALUE;
#else
  return readFd_ != -1 || writeFd_ != -1;
#endif
}

#ifdef _WIN32
// Windows Named Pipe implementation
bool FifoCommunicatorImpl::createWindowsNamedPipe() {
  std::string pipePath = config_.windowsPipePath;

  DWORD openMode = PIPE_ACCESS_DUPLEX;
  if (config_.enableNonBlocking) {
    openMode |= FILE_FLAG_OVERLAPPED;
  }

  DWORD pipeMode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;

  readHandle_ = CreateNamedPipeA(
      pipePath.c_str(), openMode, pipeMode, config_.windowsPipeInstances,
      config_.windowsOutBufferSize, config_.windowsInBufferSize,
      config_.windowsDefaultTimeout, NULL);

  if (readHandle_ == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    handleError("Failed to create Windows named pipe: " +
                std::to_string(error));
    return false;
  }

  writeHandle_ = readHandle_; // Same handle for duplex
  return true;
}

bool FifoCommunicatorImpl::openWindowsNamedPipe() {
  if (readHandle_ == INVALID_HANDLE_VALUE) {
    std::string pipePath = config_.windowsPipePath;

    DWORD access = GENERIC_READ | GENERIC_WRITE;
    DWORD flags = 0;
    if (config_.enableNonBlocking) {
      flags |= FILE_FLAG_OVERLAPPED;
    }

    readHandle_ = CreateFileA(pipePath.c_str(), access, 0, NULL, OPEN_EXISTING,
                              flags, NULL);

    if (readHandle_ == INVALID_HANDLE_VALUE) {
      DWORD error = GetLastError();
      handleError("Failed to open Windows named pipe: " +
                  std::to_string(error));
      return false;
    }

    writeHandle_ = readHandle_;
  }

  return true;
}

void FifoCommunicatorImpl::closeWindowsNamedPipe() {
  if (readHandle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(readHandle_);
    readHandle_ = INVALID_HANDLE_VALUE;
  }
  if (writeHandle_ != INVALID_HANDLE_VALUE && writeHandle_ != readHandle_) {
    CloseHandle(writeHandle_);
    writeHandle_ = INVALID_HANDLE_VALUE;
  }
}

bool FifoCommunicatorImpl::readFromWindowsPipe(std::string &message) {
  if (readHandle_ == INVALID_HANDLE_VALUE) {
    return false;
  }

  std::vector<char> buffer(config_.readBufferSize);
  DWORD bytesRead = 0;

  if (config_.enableNonBlocking) {
    BOOL result = ReadFile(readHandle_, buffer.data(), buffer.size(),
                           &bytesRead, &readOverlapped_);
    if (!result) {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING) {
        // Wait for completion
        if (WaitForSingleObject(readOverlapped_.hEvent,
                                config_.readTimeout.count()) == WAIT_TIMEOUT) {
          CancelIo(readHandle_);
          return false;
        }
        GetOverlappedResult(readHandle_, &readOverlapped_, &bytesRead, FALSE);
      } else {
        handleReadError("Windows pipe read error: " + std::to_string(error));
        return false;
      }
    }
  } else {
    BOOL result =
        ReadFile(readHandle_, buffer.data(), buffer.size(), &bytesRead, NULL);
    if (!result) {
      DWORD error = GetLastError();
      handleReadError("Windows pipe read error: " + std::to_string(error));
      return false;
    }
  }

  if (bytesRead > 0) {
    message.assign(buffer.data(), bytesRead);
    updateStatistics(false, bytesRead);
    return true;
  }

  return false;
}

bool FifoCommunicatorImpl::writeToWindowsPipe(const std::string &message) {
  if (writeHandle_ == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD bytesWritten = 0;

  if (config_.enableNonBlocking) {
    BOOL result = WriteFile(writeHandle_, message.data(), message.size(),
                            &bytesWritten, &writeOverlapped_);
    if (!result) {
      DWORD error = GetLastError();
      if (error == ERROR_IO_PENDING) {
        // Wait for completion
        if (WaitForSingleObject(writeOverlapped_.hEvent,
                                config_.writeTimeout.count()) == WAIT_TIMEOUT) {
          CancelIo(writeHandle_);
          return false;
        }
        GetOverlappedResult(writeHandle_, &writeOverlapped_, &bytesWritten,
                            FALSE);
      } else {
        handleWriteError("Windows pipe write error: " + std::to_string(error));
        return false;
      }
    }
  } else {
    BOOL result = WriteFile(writeHandle_, message.data(), message.size(),
                            &bytesWritten, NULL);
    if (!result) {
      DWORD error = GetLastError();
      handleWriteError("Windows pipe write error: " + std::to_string(error));
      return false;
    }
  }

  if (bytesWritten == message.size()) {
    updateStatistics(true, bytesWritten);
    return true;
  }

  return false;
}

#else
// Unix FIFO implementation
bool FifoCommunicatorImpl::createUnixFifo() {
  std::string pipePath = config_.unixPipePath;

  // Remove existing FIFO if it exists
  unlink(pipePath.c_str());

  // Create FIFO with specified permissions
  if (mkfifo(pipePath.c_str(), config_.pipePermissions) == -1) {
    if (errno != EEXIST) {
      handleError("Failed to create Unix FIFO: " +
                  std::string(strerror(errno)));
      return false;
    }
  }

  // Set ownership if specified
  if (!config_.pipeOwner.empty() || !config_.pipeGroup.empty()) {
    // Implementation would require user/group lookup
    // This is a simplified version
    spdlog::debug("FIFO ownership setting not fully implemented");
  }

  return true;
}

bool FifoCommunicatorImpl::openUnixFifo() {
  std::string pipePath = config_.unixPipePath;

  int flags = O_RDWR;
  if (config_.enableNonBlocking) {
    flags |= O_NONBLOCK;
  }

  switch (config_.accessMode) {
  case FifoAccessMode::READ_ONLY:
    flags = O_RDONLY;
    break;
  case FifoAccessMode::WRITE_ONLY:
    flags = O_WRONLY;
    break;
  case FifoAccessMode::READ_WRITE:
  case FifoAccessMode::DUPLEX:
    flags = O_RDWR;
    break;
  }

  if (config_.enableNonBlocking) {
    flags |= O_NONBLOCK;
  }

  readFd_ = open(pipePath.c_str(), flags);
  if (readFd_ == -1) {
    handleError("Failed to open Unix FIFO: " + std::string(strerror(errno)));
    return false;
  }

  writeFd_ = readFd_; // Same descriptor for duplex
  return true;
}

void FifoCommunicatorImpl::closeUnixFifo() {
  if (readFd_ != -1) {
    close(readFd_);
    readFd_ = -1;
  }
  if (writeFd_ != -1 && writeFd_ != readFd_) {
    close(writeFd_);
    writeFd_ = -1;
  }
}

bool FifoCommunicatorImpl::readFromUnixFifo(std::string &message) {
  if (readFd_ == -1) {
    return false;
  }

  std::vector<char> buffer(config_.readBufferSize);

  if (config_.enableNonBlocking) {
    // Use poll for timeout
    struct pollfd pfd;
    pfd.fd = readFd_;
    pfd.events = POLLIN;

    int pollResult = poll(&pfd, 1, config_.readTimeout.count());
    if (pollResult <= 0) {
      return false; // Timeout or error
    }
  }

  ssize_t bytesRead = read(readFd_, buffer.data(), buffer.size());
  if (bytesRead == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false; // No data available
    }
    handleReadError("Unix FIFO read error: " + std::string(strerror(errno)));
    return false;
  }

  if (bytesRead > 0) {
    message.assign(buffer.data(), bytesRead);
    updateStatistics(false, bytesRead);
    return true;
  }

  return false;
}

bool FifoCommunicatorImpl::writeToUnixFifo(const std::string &message) {
  if (writeFd_ == -1) {
    return false;
  }

  if (config_.enableNonBlocking) {
    // Use poll for timeout
    struct pollfd pfd;
    pfd.fd = writeFd_;
    pfd.events = POLLOUT;

    int pollResult = poll(&pfd, 1, config_.writeTimeout.count());
    if (pollResult <= 0) {
      return false; // Timeout or error
    }
  }

  ssize_t bytesWritten = write(writeFd_, message.data(), message.size());
  if (bytesWritten == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false; // Would block
    }
    handleWriteError("Unix FIFO write error: " + std::string(strerror(errno)));
    return false;
  }

  if (bytesWritten == static_cast<ssize_t>(message.size())) {
    updateStatistics(true, bytesWritten);
    return true;
  }

  return false;
}
#endif

// Threading methods
void FifoCommunicatorImpl::readerThreadFunction() {
  spdlog::debug("FIFO reader thread started");

  while (running_.load()) {
    std::string message;

#ifdef _WIN32
    if (readFromWindowsPipe(message)) {
#else
    if (readFromUnixFifo(message)) {
#endif
      processIncomingMessage(message);
      updateLastActivity();
    } else {
      // No data available, sleep briefly
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  spdlog::debug("FIFO reader thread stopped");
}

void FifoCommunicatorImpl::writerThreadFunction() {
  spdlog::debug("FIFO writer thread started");

  while (running_.load()) {
    std::string message = dequeueOutgoingMessage();
    if (!message.empty()) {
#ifdef _WIN32
      if (!writeToWindowsPipe(message)) {
#else
      if (!writeToUnixFifo(message)) {
#endif
        handleWriteError("Failed to write message to FIFO");
      } else {
        updateLastActivity();
      }
    }
  }

  spdlog::debug("FIFO writer thread stopped");
}

void FifoCommunicatorImpl::reconnectThreadFunction() {
  spdlog::debug("FIFO reconnect thread started");

  while (running_.load()) {
    if (!isConnected() && shouldReconnect()) {
      spdlog::info("Attempting FIFO reconnection");
      if (reconnect()) {
        spdlog::info("FIFO reconnection successful");
      } else {
        spdlog::warn("FIFO reconnection failed");
      }
    }

    std::this_thread::sleep_for(config_.reconnectDelay);
  }

  spdlog::debug("FIFO reconnect thread stopped");
}

// Message processing methods
void FifoCommunicatorImpl::processIncomingMessage(const std::string &message) {
  std::vector<std::string> messages = parseFramedMessages(message);

  for (const auto &msg : messages) {
    std::string parsedMessage = parseMessage(msg);

    {
      std::lock_guard<std::mutex> lock(incomingMutex_);
      incomingMessages_.push(parsedMessage);
    }
    incomingCondition_.notify_one();

    // Call message handler if set
    processMessage(parsedMessage);
  }
}

void FifoCommunicatorImpl::queueOutgoingMessage(const std::string &message) {
  {
    std::lock_guard<std::mutex> lock(outgoingMutex_);
    if (outgoingMessages_.size() >= config_.maxQueueSize) {
      if (config_.enableBackpressure) {
        // Drop oldest message
        outgoingMessages_.pop();
      } else {
        // Drop current message
        return;
      }
    }
    outgoingMessages_.push(message);
  }
  outgoingCondition_.notify_one();
}

std::string FifoCommunicatorImpl::dequeueIncomingMessage() {
  std::unique_lock<std::mutex> lock(incomingMutex_);

  if (config_.readTimeout.count() > 0) {
    incomingCondition_.wait_for(lock, config_.readTimeout, [this] {
      return !incomingMessages_.empty() || !running_.load();
    });
  } else {
    incomingCondition_.wait(lock, [this] {
      return !incomingMessages_.empty() || !running_.load();
    });
  }

  if (!incomingMessages_.empty()) {
    std::string message = incomingMessages_.front();
    incomingMessages_.pop();
    return message;
  }

  return "";
}

std::string FifoCommunicatorImpl::dequeueOutgoingMessage() {
  std::unique_lock<std::mutex> lock(outgoingMutex_);

  outgoingCondition_.wait_for(lock, std::chrono::milliseconds(100), [this] {
    return !outgoingMessages_.empty() || !running_.load();
  });

  if (!outgoingMessages_.empty()) {
    std::string message = outgoingMessages_.front();
    outgoingMessages_.pop();
    return message;
  }

  return "";
}

// Connection management methods
bool FifoCommunicatorImpl::attemptConnection() {
  if (isCircuitBreakerOpen()) {
    return false;
  }

  return openPipe();
}

bool FifoCommunicatorImpl::shouldReconnect() const {
  if (!config_.enableAutoReconnect) {
    return false;
  }

  if (reconnectAttempts_.load() >= config_.maxReconnectAttempts) {
    return false;
  }

  auto now = std::chrono::system_clock::now();
  auto timeSinceLastAttempt =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now - lastReconnectAttempt_);

  return timeSinceLastAttempt >= config_.reconnectDelay;
}

void FifoCommunicatorImpl::resetCircuitBreaker() {
  circuitBreakerOpen_.store(false);
  consecutiveErrors_.store(0);
}

void FifoCommunicatorImpl::openCircuitBreaker() {
  circuitBreakerOpen_.store(true);
  circuitBreakerOpenTime_ = std::chrono::system_clock::now();
}

bool FifoCommunicatorImpl::isCircuitBreakerOpen() const {
  if (!config_.enableCircuitBreaker) {
    return false;
  }

  if (!circuitBreakerOpen_.load()) {
    return false;
  }

  // Check if circuit breaker timeout has passed
  auto now = std::chrono::system_clock::now();
  auto timeSinceOpen = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - circuitBreakerOpenTime_);

  return timeSinceOpen < config_.circuitBreakerTimeout;
}

// Message framing methods
std::string
FifoCommunicatorImpl::frameMessage(const std::string &message) const {
  return formatMessage(message);
}

std::vector<std::string>
FifoCommunicatorImpl::parseFramedMessages(const std::string &data) const {
  std::vector<std::string> messages;

  switch (config_.framingMode) {
  case FifoFramingMode::NEWLINE_DELIMITED:
  case FifoFramingMode::JSON_LINES: {
    std::istringstream stream(data);
    std::string line;
    while (std::getline(stream, line)) {
      if (!line.empty()) {
        messages.push_back(line);
      }
    }
    break;
  }
  case FifoFramingMode::CUSTOM_DELIMITER: {
    std::string delimiter = config_.customDelimiter;
    size_t start = 0;
    size_t end = data.find(delimiter);

    while (end != std::string::npos) {
      std::string message = data.substr(start, end - start);
      if (!message.empty()) {
        messages.push_back(message);
      }
      start = end + delimiter.length();
      end = data.find(delimiter, start);
    }

    // Handle remaining data
    if (start < data.length()) {
      std::string message = data.substr(start);
      if (!message.empty()) {
        messages.push_back(message);
      }
    }
    break;
  }
  case FifoFramingMode::LENGTH_PREFIXED:
  case FifoFramingMode::BINARY_LENGTH_PREFIXED: {
    size_t offset = 0;
    while (offset + sizeof(uint32_t) <= data.length()) {
      uint32_t length;
      memcpy(&length, data.data() + offset, sizeof(length));

      if (config_.framingMode == FifoFramingMode::BINARY_LENGTH_PREFIXED) {
        length = ntohl(length);
      }

      offset += sizeof(uint32_t);

      if (offset + length <= data.length()) {
        std::string message = data.substr(offset, length);
        messages.push_back(message);
        offset += length;
      } else {
        break; // Incomplete message
      }
    }
    break;
  }
  case FifoFramingMode::NULL_TERMINATED: {
    size_t start = 0;
    size_t end = data.find('\0');

    while (end != std::string::npos) {
      std::string message = data.substr(start, end - start);
      if (!message.empty()) {
        messages.push_back(message);
      }
      start = end + 1;
      end = data.find('\0', start);
    }
    break;
  }
  default:
    messages.push_back(data);
    break;
  }

  return messages;
}

// Error handling methods
void FifoCommunicatorImpl::handleConnectionError(const std::string &error) {
  incrementErrorCount();
  handleError("Connection error: " + error);

  if (consecutiveErrors_.load() >= config_.circuitBreakerThreshold) {
    openCircuitBreaker();
  }
}

void FifoCommunicatorImpl::handleReadError(const std::string &error) {
  incrementErrorCount();
  handleError("Read error: " + error);
}

void FifoCommunicatorImpl::handleWriteError(const std::string &error) {
  incrementErrorCount();
  handleError("Write error: " + error);
}

// Statistics methods
void FifoCommunicatorImpl::updateStatistics(bool sent, size_t bytes) {
  std::lock_guard<std::mutex> lock(statisticsMutex_);

  if (sent) {
    statistics_.messagesSent.fetch_add(1);
  } else {
    statistics_.messagesReceived.fetch_add(1);
  }

  statistics_.bytesTransferred.fetch_add(bytes);
  updateLastActivity();
}

void FifoCommunicatorImpl::incrementErrorCount() {
  statistics_.errors.fetch_add(1);
  consecutiveErrors_.fetch_add(1);
}

void FifoCommunicatorImpl::updateLastActivity() {
  std::lock_guard<std::mutex> lock(statisticsMutex_);
  statistics_.lastActivity = std::chrono::system_clock::now();
}

// Health checking methods
bool FifoCommunicatorImpl::performHealthCheck() const {
  if (!isPipeValid()) {
    return false;
  }

  // Check if we've had recent activity
  auto now = std::chrono::system_clock::now();
  auto timeSinceActivity =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now - statistics_.lastActivity);

  if (config_.enableKeepAlive &&
      timeSinceActivity > config_.keepAliveInterval * 2) {
    return false;
  }

  return true;
}

std::string FifoCommunicatorImpl::generateHealthReport() const {
  json report;
  report["status"] = getHealthStatus();
  report["connectionState"] = static_cast<int>(connectionState_.load());
  report["circuitBreakerOpen"] = isCircuitBreakerOpen();
  report["consecutiveErrors"] = consecutiveErrors_.load();
  report["reconnectAttempts"] = reconnectAttempts_.load();
  report["statistics"] = getStatistics().toJson();

  return report.dump(2);
}

// Cleanup methods
void FifoCommunicatorImpl::cleanup() {
  stopThreads();
  clearQueues();
  closePipe();
}

void FifoCommunicatorImpl::stopThreads() {
  if (readerThread_ && readerThread_->joinable()) {
    readerThread_->join();
    readerThread_.reset();
  }

  if (writerThread_ && writerThread_->joinable()) {
    writerThread_->join();
    writerThread_.reset();
  }

  if (reconnectThread_ && reconnectThread_->joinable()) {
    reconnectThread_->join();
    reconnectThread_.reset();
  }
}

void FifoCommunicatorImpl::clearQueues() {
  {
    std::lock_guard<std::mutex> lock(incomingMutex_);
    while (!incomingMessages_.empty()) {
      incomingMessages_.pop();
    }
  }

  {
    std::lock_guard<std::mutex> lock(outgoingMutex_);
    while (!outgoingMessages_.empty()) {
      outgoingMessages_.pop();
    }
  }
}

// Factory implementation
std::unique_ptr<FifoCommunicator>
FifoCommunicatorFactory::create(const FifoConfig &config) {
  return std::make_unique<FifoCommunicatorImpl>(config);
}

std::unique_ptr<FifoCommunicator> FifoCommunicatorFactory::createDefault() {
  auto &configManager = getGlobalFifoConfigManager();
  FifoConfig config = configManager.createConfig();
  return create(config);
}

std::unique_ptr<FifoCommunicator> FifoCommunicatorFactory::createWithPreset(
    FifoConfigManager::ConfigPreset preset) {
  auto &configManager = getGlobalFifoConfigManager();
  FifoConfig config = configManager.createConfig(preset);
  return create(config);
}

std::unique_ptr<FifoCommunicator>
FifoCommunicatorFactory::createForWindows(const FifoConfig &config) {
  FifoConfig windowsConfig = config;
  windowsConfig.pipeType = FifoPipeType::WINDOWS_NAMED_PIPE;
  return create(windowsConfig);
}

std::unique_ptr<FifoCommunicator>
FifoCommunicatorFactory::createForUnix(const FifoConfig &config) {
  FifoConfig unixConfig = config;
  unixConfig.pipeType = FifoPipeType::UNIX_FIFO;
  return create(unixConfig);
}

std::unique_ptr<FifoCommunicator>
FifoCommunicatorFactory::createBidirectional(const FifoConfig &config) {
  FifoConfig bidirectionalConfig = config;
  bidirectionalConfig.accessMode = FifoAccessMode::DUPLEX;
  bidirectionalConfig.enableBidirectional = true;
  return create(bidirectionalConfig);
}

std::unique_ptr<FifoCommunicator>
FifoCommunicatorFactory::createHighPerformance(const FifoConfig &config) {
  auto &configManager = getGlobalFifoConfigManager();
  FifoConfig perfConfig = configManager.mergeConfigs(
      config, configManager.createConfig(
                  FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE));
  return create(perfConfig);
}

std::unique_ptr<FifoCommunicator>
FifoCommunicatorFactory::createReliable(const FifoConfig &config) {
  auto &configManager = getGlobalFifoConfigManager();
  FifoConfig reliableConfig = configManager.mergeConfigs(
      config,
      configManager.createConfig(FifoConfigManager::ConfigPreset::RELIABLE));
  return create(reliableConfig);
}

} // namespace core
} // namespace hydrogen
