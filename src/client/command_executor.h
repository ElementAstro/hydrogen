#pragma once

#include "client/message_processor.h"
#include "client/message_queue_manager.h"
#include "common/message.h"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace hydrogen {

using json = nlohmann::json;

/**
 * @brief Handles command execution (sync/async) and batch operations
 * 
 * This class is responsible for:
 * - Synchronous command execution with response waiting
 * - Asynchronous command execution with callbacks
 * - Batch command execution (sequential/parallel)
 * - Command retry and QoS handling
 * - Callback management for async operations
 */
class CommandExecutor {
public:
    /**
     * @brief Callback function type for async command responses
     * @param response The JSON response from the command
     */
    using AsyncCallback = std::function<void(const json& response)>;

    /**
     * @brief Constructor
     * @param messageProcessor Pointer to the message processor for communication
     */
    explicit CommandExecutor(MessageProcessor* messageProcessor);

    /**
     * @brief Destructor
     */
    ~CommandExecutor();

    /**
     * @brief Execute a command synchronously and wait for response
     * @param deviceId The ID of the target device
     * @param command The command to execute
     * @param parameters Optional command parameters
     * @param qosLevel Quality of Service level for the message
     * @return JSON response from the device
     * @throws std::runtime_error if execution fails or times out
     */
    json executeCommand(const std::string& deviceId, const std::string& command,
                       const json& parameters = json::object(),
                       Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);

    /**
     * @brief Execute a command asynchronously with callback
     * @param deviceId The ID of the target device
     * @param command The command to execute
     * @param parameters Optional command parameters
     * @param qosLevel Quality of Service level for the message
     * @param callback Optional callback function for the response
     */
    void executeCommandAsync(const std::string& deviceId, const std::string& command,
                            const json& parameters = json::object(),
                            Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE,
                            AsyncCallback callback = nullptr);

    /**
     * @brief Execute multiple commands in batch
     * @param deviceId The ID of the target device
     * @param commands Vector of command-parameter pairs
     * @param sequential Whether to execute commands sequentially (true) or in parallel (false)
     * @param qosLevel Quality of Service level for the batch message
     * @return JSON response containing results of all commands
     * @throws std::runtime_error if execution fails
     */
    json executeBatchCommands(const std::string& deviceId,
                             const std::vector<std::pair<std::string, json>>& commands,
                             bool sequential = true,
                             Message::QoSLevel qosLevel = Message::QoSLevel::AT_MOST_ONCE);

    /**
     * @brief Set message retry parameters for high QoS messages
     * @param maxRetries Maximum number of retry attempts
     * @param retryIntervalMs Interval between retries in milliseconds
     */
    void setMessageRetryParams(int maxRetries, int retryIntervalMs);

    /**
     * @brief Cancel a pending async command
     * @param messageId The ID of the message to cancel
     * @return true if command was found and cancelled, false otherwise
     */
    bool cancelAsyncCommand(const std::string& messageId);

    /**
     * @brief Get the number of pending async commands
     * @return Number of commands waiting for responses
     */
    size_t getPendingAsyncCount() const;

    /**
     * @brief Get command execution statistics
     * @return JSON object with execution statistics
     */
    json getExecutionStats() const;

    /**
     * @brief Clear all pending async commands and call their callbacks with error
     */
    void clearPendingCommands();

private:
    // Message processor for communication
    MessageProcessor* messageProcessor;

    // Message queue manager for high QoS messages
    std::unique_ptr<MessageQueueManager> messageQueueManager;

    // Async callback handling
    mutable std::mutex callbacksMutex;
    std::map<std::string, AsyncCallback> asyncCallbacks;

    // Statistics
    mutable std::mutex statsMutex;
    mutable size_t commandsExecuted;
    mutable size_t asyncCommandsExecuted;
    mutable size_t batchCommandsExecuted;
    mutable size_t commandErrors;
    mutable size_t timeouts;

    /**
     * @brief Handle async command response
     * @param messageId The ID of the original message
     * @param response The response JSON
     */
    void handleAsyncResponse(const std::string& messageId, const json& response);

    /**
     * @brief Register an async callback for a message
     * @param messageId The message ID
     * @param callback The callback function
     */
    void registerAsyncCallback(const std::string& messageId, AsyncCallback callback);

    /**
     * @brief Unregister an async callback
     * @param messageId The message ID
     */
    void unregisterAsyncCallback(const std::string& messageId);

    /**
     * @brief Validate device ID format
     * @param deviceId The device ID to validate
     * @return true if valid, false otherwise
     */
    bool isValidDeviceId(const std::string& deviceId) const;

    /**
     * @brief Validate command name format
     * @param command The command name to validate
     * @return true if valid, false otherwise
     */
    bool isValidCommand(const std::string& command) const;

    /**
     * @brief Update execution statistics
     * @param executed Number of commands executed
     * @param asyncExecuted Number of async commands executed
     * @param batchExecuted Number of batch commands executed
     * @param errors Number of command errors
     * @param timeoutCount Number of timeouts
     */
    void updateStats(size_t executed = 0, size_t asyncExecuted = 0, 
                    size_t batchExecuted = 0, size_t errors = 0, size_t timeoutCount = 0);

    /**
     * @brief Execute command with high QoS using message queue
     * @param msg The command message
     * @param timeoutSeconds Timeout for waiting response
     * @return JSON response
     */
    json executeWithQoS(const CommandMessage& msg, int timeoutSeconds = 30);
};

} // namespace hydrogen
