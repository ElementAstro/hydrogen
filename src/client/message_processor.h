#pragma once

#include "client/connection_manager.h"
#include "common/message.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace hydrogen {

using json = nlohmann::json;

/**
 * @brief Basic message processor for standard use cases
 *
 * This class provides reliable message processing functionality:
 * - Sending messages through the WebSocket connection
 * - Receiving and parsing incoming messages
 * - Running the message processing loop
 * - Handling synchronous request-response patterns
 * - Dispatching messages to registered handlers
 */
class MessageProcessor {
public:
    /**
     * @brief Message handler function type
     * @param message The received message
     */
    using MessageHandler = std::function<void(const Message& message)>;

    /**
     * @brief Constructor
     * @param connectionManager Pointer to the connection manager
     */
    explicit MessageProcessor(ConnectionManager* connectionManager);
    
    /**
     * @brief Destructor
     */
    ~MessageProcessor();

    /**
     * @brief Send a message through the WebSocket connection
     * @param msg The message to send
     * @return true if message was sent successfully, false otherwise
     */
    bool sendMessage(const Message& msg);

    /**
     * @brief Send a message and wait for a response
     * @param msg The message to send
     * @param timeoutSeconds Timeout in seconds to wait for response
     * @return JSON response from the server
     * @throws std::runtime_error if timeout occurs or connection fails
     */
    json sendAndWaitForResponse(const Message& msg, int timeoutSeconds = 10);

    /**
     * @brief Start the message processing loop in a background thread
     */
    void startMessageLoop();

    /**
     * @brief Stop the message processing loop
     */
    void stopMessageLoop();

    /**
     * @brief Register a handler for a specific message type
     * @param type The message type to handle
     * @param handler The handler function
     */
    void registerMessageHandler(MessageType type, MessageHandler handler);

    /**
     * @brief Unregister a handler for a specific message type
     * @param type The message type to unregister
     */
    void unregisterMessageHandler(MessageType type);

    /**
     * @brief Check if the message loop is currently running
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running.load(); }

    /**
     * @brief Get processing statistics
     * @return JSON object with message processing statistics
     */
    json getProcessingStats() const;

private:
    // Connection manager reference
    ConnectionManager* connectionManager;

    // Message processing thread
    std::atomic<bool> running;
    std::thread messageThread;
    mutable std::mutex threadMutex;

    // Message handlers
    std::mutex handlersMutex;
    std::map<MessageType, MessageHandler> messageHandlers;

    // Response handling for synchronous requests
    std::mutex responseMutex;
    std::condition_variable responseCV;
    std::map<std::string, json> responses;

    // Statistics
    mutable std::mutex statsMutex;
    size_t messagesSent;
    size_t messagesReceived;
    size_t messagesProcessed;
    size_t processingErrors;

    /**
     * @brief Main message processing loop
     */
    void messageLoop();

    /**
     * @brief Handle an incoming message
     * @param messageStr Raw message string from WebSocket
     */
    void handleMessage(const std::string& messageStr);

    /**
     * @brief Handle a response message for synchronous requests
     * @param originalMessageId The ID of the original request message
     * @param responseJson The response JSON
     */
    void handleResponse(const std::string& originalMessageId, const json& responseJson);

    /**
     * @brief Dispatch a message to the appropriate handler
     * @param message The message to dispatch
     */
    void dispatchMessage(const Message& message);

    /**
     * @brief Update processing statistics
     * @param sent Number of messages sent
     * @param received Number of messages received
     * @param processed Number of messages processed
     * @param errors Number of processing errors
     */
    void updateStats(size_t sent = 0, size_t received = 0, size_t processed = 0, size_t errors = 0);
};

} // namespace hydrogen
