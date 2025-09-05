#include "hydrogen/core/stdio_logger.h"
#include <spdlog/async.h>
#include <spdlog/sinks/json_sink.h>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <thread>
#include <random>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace hydrogen {
namespace core {

// PerformanceMetrics implementation
double PerformanceMetrics::getAverageProcessingTime() const {
    uint64_t total = totalMessages.load();
    if (total == 0) return 0.0;
    return static_cast<double>(totalProcessingTime.load()) / total;
}

double PerformanceMetrics::getMessagesPerSecond() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(totalMessages.load()) / duration.count();
}

double PerformanceMetrics::getBytesPerSecond() const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(totalBytes.load()) / duration.count();
}

double PerformanceMetrics::getSuccessRate() const {
    uint64_t total = totalMessages.load();
    if (total == 0) return 0.0;
    return static_cast<double>(successfulMessages.load()) / total * 100.0;
}

json PerformanceMetrics::toJson() const {
    json j;
    j["totalMessages"] = totalMessages.load();
    j["successfulMessages"] = successfulMessages.load();
    j["failedMessages"] = failedMessages.load();
    j["totalBytes"] = totalBytes.load();
    j["averageProcessingTime"] = getAverageProcessingTime();
    j["messagesPerSecond"] = getMessagesPerSecond();
    j["bytesPerSecond"] = getBytesPerSecond();
    j["successRate"] = getSuccessRate();
    j["minProcessingTime"] = minProcessingTime.load();
    j["maxProcessingTime"] = maxProcessingTime.load();
    j["totalConnections"] = totalConnections.load();
    j["activeConnections"] = activeConnections.load();
    j["connectionErrors"] = connectionErrors.load();
    j["protocolErrors"] = protocolErrors.load();
    j["timeoutErrors"] = timeoutErrors.load();
    j["validationErrors"] = validationErrors.load();
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    j["uptimeSeconds"] = uptime.count();
    
    return j;
}

// StdioLogger implementation
StdioLogger::StdioLogger(const LoggerConfig& config) : config_(config) {
    metrics_.startTime = std::chrono::system_clock::now();
    initializeLoggers();
}

StdioLogger::~StdioLogger() {
    if (logger_) {
        logger_->flush();
    }
    if (traceLogger_) {
        traceLogger_->flush();
    }
}

void StdioLogger::updateConfig(const LoggerConfig& config) {
    config_ = config;
    initializeLoggers();
}

StdioLogger::LoggerConfig StdioLogger::getConfig() const {
    return config_;
}

void StdioLogger::trace(const std::string& message, const std::string& clientId) {
    if (logger_ && logger_->level() <= spdlog::level::trace) {
        logger_->trace(formatLogMessage(message, clientId));
    }
}

void StdioLogger::debug(const std::string& message, const std::string& clientId) {
    if (logger_ && logger_->level() <= spdlog::level::debug) {
        logger_->debug(formatLogMessage(message, clientId));
    }
}

void StdioLogger::info(const std::string& message, const std::string& clientId) {
    if (logger_ && logger_->level() <= spdlog::level::info) {
        logger_->info(formatLogMessage(message, clientId));
    }
}

void StdioLogger::warn(const std::string& message, const std::string& clientId) {
    if (logger_ && logger_->level() <= spdlog::level::warn) {
        logger_->warn(formatLogMessage(message, clientId));
    }
}

void StdioLogger::error(const std::string& message, const std::string& clientId) {
    if (logger_ && logger_->level() <= spdlog::level::err) {
        logger_->err(formatLogMessage(message, clientId));
    }
    updateErrorStats(message);
}

void StdioLogger::critical(const std::string& message, const std::string& clientId) {
    if (logger_ && logger_->level() <= spdlog::level::critical) {
        logger_->critical(formatLogMessage(message, clientId));
    }
    updateErrorStats(message);
}

void StdioLogger::traceMessage(const MessageTrace& trace) {
    if (!config_.enableMessageTracing || !shouldTrace(trace.clientId, trace.messageType)) {
        return;
    }
    
    if (traceLogger_) {
        if (config_.enableJsonLogging) {
            traceLogger_->info(messageTraceToJson(trace).dump());
        } else {
            std::ostringstream oss;
            oss << "[" << trace.direction << "] "
                << "Client: " << trace.clientId << ", "
                << "Type: " << trace.messageType << ", "
                << "Size: " << formatBytes(trace.messageSize) << ", "
                << "Time: " << formatDuration(trace.processingTime) << ", "
                << "Success: " << (trace.success ? "YES" : "NO");
            
            if (!trace.success) {
                oss << ", Error: " << trace.errorMessage;
            }
            
            traceLogger_->info(oss.str());
        }
    }
    
    addToHistory(trace);
}

void StdioLogger::traceIncomingMessage(const std::string& messageId, const std::string& clientId,
                                     const std::string& messageType, const json& content, size_t size) {
    MessageTrace trace;
    trace.messageId = messageId;
    trace.clientId = clientId;
    trace.direction = "INCOMING";
    trace.messageType = messageType;
    trace.messageSize = size;
    trace.timestamp = std::chrono::system_clock::now();
    trace.messageContent = content;
    trace.success = true;
    
    traceMessage(trace);
    updateClientActivity(clientId);
}

void StdioLogger::traceOutgoingMessage(const std::string& messageId, const std::string& clientId,
                                     const std::string& messageType, const json& content, size_t size) {
    MessageTrace trace;
    trace.messageId = messageId;
    trace.clientId = clientId;
    trace.direction = "OUTGOING";
    trace.messageType = messageType;
    trace.messageSize = size;
    trace.timestamp = std::chrono::system_clock::now();
    trace.messageContent = content;
    trace.success = true;
    
    traceMessage(trace);
    updateClientActivity(clientId);
}

void StdioLogger::traceMessageProcessing(const std::string& messageId,
                                       std::chrono::microseconds processingTime, bool success,
                                       const std::string& errorMessage) {
    if (config_.enablePerformanceMetrics) {
        recordMessage(success, 0, processingTime);
    }
    
    if (config_.enableMessageTracing) {
        MessageTrace trace;
        trace.messageId = messageId;
        trace.direction = "INTERNAL";
        trace.messageType = "PROCESSING";
        trace.processingTime = processingTime;
        trace.timestamp = std::chrono::system_clock::now();
        trace.success = success;
        trace.errorMessage = errorMessage;
        
        traceMessage(trace);
    }
}

void StdioLogger::logClientConnect(const std::string& clientId, 
                                 const std::unordered_map<std::string, std::string>& metadata) {
    info("Client connected: " + clientId, clientId);
    
    if (config_.enablePerformanceMetrics) {
        recordConnection(true);
    }
    
    if (config_.enableMessageTracing) {
        MessageTrace trace;
        trace.messageId = "CONNECTION";
        trace.clientId = clientId;
        trace.direction = "INTERNAL";
        trace.messageType = "CONNECT";
        trace.timestamp = std::chrono::system_clock::now();
        trace.metadata = metadata;
        trace.success = true;
        
        addToHistory(trace);
    }
}

void StdioLogger::logClientDisconnect(const std::string& clientId, const std::string& reason) {
    std::string message = "Client disconnected: " + clientId;
    if (!reason.empty()) {
        message += " (Reason: " + reason + ")";
    }
    
    info(message, clientId);
    
    if (config_.enableMessageTracing) {
        MessageTrace trace;
        trace.messageId = "DISCONNECTION";
        trace.clientId = clientId;
        trace.direction = "INTERNAL";
        trace.messageType = "DISCONNECT";
        trace.timestamp = std::chrono::system_clock::now();
        trace.success = true;
        if (!reason.empty()) {
            trace.metadata["reason"] = reason;
        }
        
        addToHistory(trace);
    }
}

void StdioLogger::logConnectionError(const std::string& clientId, const std::string& error) {
    this->error("Connection error: " + error, clientId);
    
    if (config_.enablePerformanceMetrics) {
        recordConnection(false);
        recordError("connection");
    }
}

void StdioLogger::logProtocolEvent(const std::string& event, const std::string& details, const std::string& clientId) {
    debug("Protocol event: " + event + " - " + details, clientId);
}

void StdioLogger::logProtocolError(const std::string& error, const std::string& clientId) {
    this->error("Protocol error: " + error, clientId);
    
    if (config_.enablePerformanceMetrics) {
        recordError("protocol");
    }
}

void StdioLogger::logValidationError(const std::string& messageId, const std::string& error, const std::string& clientId) {
    this->error("Validation error for message " + messageId + ": " + error, clientId);
    
    if (config_.enablePerformanceMetrics) {
        recordError("validation");
    }
}

void StdioLogger::recordMessage(bool success, size_t bytes, std::chrono::microseconds processingTime) {
    metrics_.totalMessages.fetch_add(1);
    metrics_.totalBytes.fetch_add(bytes);

    if (success) {
        metrics_.successfulMessages.fetch_add(1);
    } else {
        metrics_.failedMessages.fetch_add(1);
    }

    uint64_t timeUs = processingTime.count();
    metrics_.totalProcessingTime.fetch_add(timeUs);

    // Update min/max processing times
    uint64_t currentMin = metrics_.minProcessingTime.load();
    while (timeUs < currentMin && !metrics_.minProcessingTime.compare_exchange_weak(currentMin, timeUs)) {
        // Retry if another thread updated the value
    }

    uint64_t currentMax = metrics_.maxProcessingTime.load();
    while (timeUs > currentMax && !metrics_.maxProcessingTime.compare_exchange_weak(currentMax, timeUs)) {
        // Retry if another thread updated the value
    }
}

void StdioLogger::recordConnection(bool success) {
    metrics_.totalConnections.fetch_add(1);

    if (success) {
        metrics_.activeConnections.fetch_add(1);
    } else {
        metrics_.connectionErrors.fetch_add(1);
    }
}

void StdioLogger::recordError(const std::string& errorType) {
    if (errorType == "protocol") {
        metrics_.protocolErrors.fetch_add(1);
    } else if (errorType == "timeout") {
        metrics_.timeoutErrors.fetch_add(1);
    } else if (errorType == "validation") {
        metrics_.validationErrors.fetch_add(1);
    }
}

PerformanceMetrics StdioLogger::getMetrics() const {
    return metrics_;
}

void StdioLogger::resetMetrics() {
    metrics_ = PerformanceMetrics{};
    metrics_.startTime = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(errorsMutex_);
    errorCounts_.clear();

    std::lock_guard<std::mutex> clientLock(clientsMutex_);
    clientActivity_.clear();
}

void StdioLogger::enableDebugMode(bool enable) {
    config_.enableDebugMode = enable;
    if (enable) {
        config_.logLevel = StdioLogLevel::DEBUG;
        config_.enableMessageTracing = true;
        config_.enablePerformanceMetrics = true;
    }
    initializeLoggers();
}

void StdioLogger::setDebugFilter(const std::vector<std::string>& clientIds, const std::vector<std::string>& messageTypes) {
    config_.trackedClients = clientIds;
    config_.trackedMessageTypes = messageTypes;
}

void StdioLogger::dumpDebugInfo(const std::string& filename) const {
    json debugInfo;
    debugInfo["config"] = configToJson();
    debugInfo["metrics"] = metrics_.toJson();
    debugInfo["messageHistory"] = json::array();

    {
        std::lock_guard<std::mutex> lock(historyMutex_);
        for (const auto& trace : messageHistory_) {
            debugInfo["messageHistory"].push_back(messageTraceToJson(trace));
        }
    }

    debugInfo["errorCounts"] = json::object();
    {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        for (const auto& [error, count] : errorCounts_) {
            debugInfo["errorCounts"][error] = count;
        }
    }

    debugInfo["clientActivity"] = json::object();
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& [client, activity] : clientActivity_) {
            debugInfo["clientActivity"][client] = activity;
        }
    }

    if (filename.empty()) {
        info("Debug info: " + debugInfo.dump(2));
    } else {
        std::ofstream file(filename);
        file << debugInfo.dump(2);
        file.close();
        info("Debug info saved to: " + filename);
    }
}

void StdioLogger::enableMessageHistory(size_t maxMessages) {
    std::lock_guard<std::mutex> lock(historyMutex_);
    historyEnabled_ = true;
    maxHistorySize_ = maxMessages;
    messageHistory_.reserve(maxMessages);
}

void StdioLogger::disableMessageHistory() {
    std::lock_guard<std::mutex> lock(historyMutex_);
    historyEnabled_ = false;
    messageHistory_.clear();
}

std::vector<MessageTrace> StdioLogger::getMessageHistory(const std::string& clientId) const {
    std::lock_guard<std::mutex> lock(historyMutex_);

    if (clientId.empty()) {
        return messageHistory_;
    }

    std::vector<MessageTrace> filtered;
    for (const auto& trace : messageHistory_) {
        if (trace.clientId == clientId) {
            filtered.push_back(trace);
        }
    }

    return filtered;
}

void StdioLogger::clearMessageHistory() {
    std::lock_guard<std::mutex> lock(historyMutex_);
    messageHistory_.clear();
}

json StdioLogger::generateReport(std::chrono::system_clock::time_point startTime,
                               std::chrono::system_clock::time_point endTime) const {
    json report;

    if (startTime == std::chrono::system_clock::time_point{}) {
        startTime = metrics_.startTime;
    }

    if (endTime == std::chrono::system_clock::time_point{}) {
        endTime = std::chrono::system_clock::now();
    }

    report["reportPeriod"] = {
        {"startTime", std::chrono::duration_cast<std::chrono::seconds>(startTime.time_since_epoch()).count()},
        {"endTime", std::chrono::duration_cast<std::chrono::seconds>(endTime.time_since_epoch()).count()}
    };

    report["metrics"] = metrics_.toJson();
    report["topErrors"] = getTopErrors(10);
    report["mostActiveClients"] = getMostActiveClients(10);

    return report;
}

std::vector<std::string> StdioLogger::getTopErrors(size_t count) const {
    std::lock_guard<std::mutex> lock(errorsMutex_);

    std::vector<std::pair<std::string, uint64_t>> errorPairs(errorCounts_.begin(), errorCounts_.end());
    std::sort(errorPairs.begin(), errorPairs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> topErrors;
    for (size_t i = 0; i < std::min(count, errorPairs.size()); ++i) {
        topErrors.push_back(errorPairs[i].first + " (" + std::to_string(errorPairs[i].second) + ")");
    }

    return topErrors;
}

std::vector<std::string> StdioLogger::getMostActiveClients(size_t count) const {
    std::lock_guard<std::mutex> lock(clientsMutex_);

    std::vector<std::pair<std::string, uint64_t>> clientPairs(clientActivity_.begin(), clientActivity_.end());
    std::sort(clientPairs.begin(), clientPairs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> activeClients;
    for (size_t i = 0; i < std::min(count, clientPairs.size()); ++i) {
        activeClients.push_back(clientPairs[i].first + " (" + std::to_string(clientPairs[i].second) + ")");
    }

    return activeClients;
}

// Static utility methods
std::string StdioLogger::formatMessage(const json& message, bool pretty) {
    if (pretty) {
        return message.dump(2);
    } else {
        return message.dump();
    }
}

std::string StdioLogger::formatDuration(std::chrono::microseconds duration) {
    auto us = duration.count();
    if (us < 1000) {
        return std::to_string(us) + "μs";
    } else if (us < 1000000) {
        return std::to_string(us / 1000) + "ms";
    } else {
        return std::to_string(us / 1000000) + "s";
    }
}

std::string StdioLogger::formatBytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << units[unit];
    return oss.str();
}

// Private helper methods
void StdioLogger::initializeLoggers() {
    try {
        std::vector<spdlog::sink_ptr> sinks;

        // Console sink
        if (config_.enableConsoleLogging) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(config_.logPattern);
            sinks.push_back(console_sink);
        }

        // File sink
        if (config_.enableFileLogging) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config_.logFileName, config_.maxFileSize, config_.maxFiles);
            file_sink->set_pattern(config_.logPattern);
            sinks.push_back(file_sink);
        }

        // Create main logger
        if (config_.enableAsyncLogging) {
            spdlog::init_thread_pool(config_.asyncQueueSize, 1);
            logger_ = std::make_shared<spdlog::async_logger>("stdio_logger", sinks.begin(), sinks.end(),
                                                           spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        } else {
            logger_ = std::make_shared<spdlog::logger>("stdio_logger", sinks.begin(), sinks.end());
        }

        // Set log level
        spdlog::level::level_enum level;
        switch (config_.logLevel) {
            case StdioLogLevel::TRACE: level = spdlog::level::trace; break;
            case StdioLogLevel::DEBUG: level = spdlog::level::debug; break;
            case StdioLogLevel::INFO: level = spdlog::level::info; break;
            case StdioLogLevel::WARN: level = spdlog::level::warn; break;
            case StdioLogLevel::ERROR: level = spdlog::level::err; break;
            case StdioLogLevel::CRITICAL: level = spdlog::level::critical; break;
            case StdioLogLevel::OFF: level = spdlog::level::off; break;
        }
        logger_->set_level(level);

        // Create trace logger if message tracing is enabled
        if (config_.enableMessageTracing) {
            std::string traceFileName = config_.logFileName + ".trace";
            auto trace_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                traceFileName, config_.maxFileSize, config_.maxFiles);

            if (config_.enableJsonLogging) {
                trace_sink->set_pattern("%v");
            } else {
                trace_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [TRACE] %v");
            }

            traceLogger_ = std::make_shared<spdlog::logger>("stdio_trace", trace_sink);
            traceLogger_->set_level(spdlog::level::info);
        }

    } catch (const std::exception& e) {
        // Fallback to console logging
        logger_ = spdlog::stdout_color_mt("stdio_logger_fallback");
        logger_->error("Failed to initialize stdio logger: {}", e.what());
    }
}

bool StdioLogger::shouldTrace(const std::string& clientId, const std::string& messageType) const {
    // Check client filter
    if (!config_.trackedClients.empty()) {
        if (std::find(config_.trackedClients.begin(), config_.trackedClients.end(), clientId)
            == config_.trackedClients.end()) {
            return false;
        }
    }

    // Check message type filter
    if (!config_.trackedMessageTypes.empty()) {
        if (std::find(config_.trackedMessageTypes.begin(), config_.trackedMessageTypes.end(), messageType)
            == config_.trackedMessageTypes.end()) {
            return false;
        }
    }

    return true;
}

void StdioLogger::addToHistory(const MessageTrace& trace) {
    if (!historyEnabled_) {
        return;
    }

    std::lock_guard<std::mutex> lock(historyMutex_);

    if (messageHistory_.size() >= maxHistorySize_) {
        messageHistory_.erase(messageHistory_.begin());
    }

    messageHistory_.push_back(trace);
}

std::string StdioLogger::formatLogMessage(const std::string& message, const std::string& clientId) const {
    if (clientId.empty()) {
        return message;
    } else {
        return "[" + clientId + "] " + message;
    }
}

void StdioLogger::updateErrorStats(const std::string& error) {
    std::lock_guard<std::mutex> lock(errorsMutex_);
    errorCounts_[error]++;
}

void StdioLogger::updateClientActivity(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clientActivity_[clientId]++;
}

json StdioLogger::messageTraceToJson(const MessageTrace& trace) const {
    json j;
    j["messageId"] = trace.messageId;
    j["clientId"] = trace.clientId;
    j["direction"] = trace.direction;
    j["messageType"] = trace.messageType;
    j["messageSize"] = trace.messageSize;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(trace.timestamp.time_since_epoch()).count();
    j["processingTime"] = trace.processingTime.count();
    j["success"] = trace.success;
    j["errorMessage"] = trace.errorMessage;
    j["metadata"] = trace.metadata;

    if (!trace.messageContent.is_null()) {
        j["messageContent"] = trace.messageContent;
    }

    return j;
}

json StdioLogger::configToJson() const {
    json j;
    j["logLevel"] = static_cast<int>(config_.logLevel);
    j["enableConsoleLogging"] = config_.enableConsoleLogging;
    j["enableFileLogging"] = config_.enableFileLogging;
    j["logFileName"] = config_.logFileName;
    j["maxFileSize"] = config_.maxFileSize;
    j["maxFiles"] = config_.maxFiles;
    j["enableMessageTracing"] = config_.enableMessageTracing;
    j["enablePerformanceMetrics"] = config_.enablePerformanceMetrics;
    j["enableDebugMode"] = config_.enableDebugMode;
    j["logPattern"] = config_.logPattern;
    j["enableAsyncLogging"] = config_.enableAsyncLogging;
    j["asyncQueueSize"] = config_.asyncQueueSize;
    j["enableJsonLogging"] = config_.enableJsonLogging;
    j["trackedClients"] = config_.trackedClients;
    j["trackedMessageTypes"] = config_.trackedMessageTypes;
    return j;
}

// Global instance
StdioLogger& getGlobalStdioLogger() {
    static StdioLogger instance;
    return instance;
}

// MessageTracer implementation
MessageTracer::MessageTracer(StdioLogger& logger, const std::string& messageId,
                           const std::string& clientId, const std::string& operation)
    : logger_(logger), messageId_(messageId), clientId_(clientId), operation_(operation),
      startTime_(std::chrono::system_clock::now()) {
}

MessageTracer::~MessageTracer() {
    auto endTime = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_);

    logger_.traceMessageProcessing(messageId_, duration, success_, error_);
}

void MessageTracer::setSuccess(bool success) {
    success_ = success;
}

void MessageTracer::setError(const std::string& error) {
    success_ = false;
    error_ = error;
}

void MessageTracer::addMetadata(const std::string& key, const std::string& value) {
    metadata_[key] = value;
}

} // namespace core
} // namespace hydrogen
