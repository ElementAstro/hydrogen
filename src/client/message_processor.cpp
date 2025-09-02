#include "client/message_processor.h"
#include <boost/asio/buffer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <future>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace beast = boost::beast;

#ifdef _WIN32
#undef ERROR
#endif

namespace hydrogen {

MessageProcessor::MessageProcessor(ConnectionManager* connectionManager)
    : connectionManager(connectionManager), running(false),
      messagesSent(0), messagesReceived(0), messagesProcessed(0), processingErrors(0) {
  if (!connectionManager) {
    throw std::invalid_argument("ConnectionManager cannot be null");
  }
  spdlog::debug("MessageProcessor initialized");
}

MessageProcessor::~MessageProcessor() {
  stopMessageLoop();
  spdlog::debug("MessageProcessor destroyed");
}

bool MessageProcessor::sendMessage(const Message& msg) {
  if (!connectionManager->isConnected()) {
    spdlog::warn("Cannot send message ID {}: Not connected", msg.getMessageId());
    return false;
  }

  auto* ws = connectionManager->getWebSocket();
  if (!ws) {
    spdlog::error("Cannot send message ID {}: WebSocket is null", msg.getMessageId());
    return false;
  }

  try {
    std::string msgJson = msg.toJson().dump();
    boost::system::error_code ec;
    ws->write(boost::asio::buffer(msgJson), ec);

    if (ec) {
      spdlog::error("Error sending message ID {}: {}", msg.getMessageId(), ec.message());
      updateStats(0, 0, 0, 1);
      return false;
    }

    spdlog::trace("Successfully sent message ID: {}", msg.getMessageId());
    updateStats(1, 0, 0, 0);
    return true;

  } catch (const beast::system_error& e) {
    spdlog::error("WebSocket error sending message ID {}: {}", msg.getMessageId(), e.what());
    updateStats(0, 0, 0, 1);
    return false;
  } catch (const std::exception& e) {
    spdlog::error("Error sending message ID {}: {}", msg.getMessageId(), e.what());
    updateStats(0, 0, 0, 1);
    return false;
  }
}

json MessageProcessor::sendAndWaitForResponse(const Message& msg, int timeoutSeconds) {
  if (!connectionManager->isConnected()) {
    throw std::runtime_error("Not connected to server");
  }

  std::string messageId = msg.getMessageId();
  if (messageId.empty()) {
    spdlog::error("Attempting to send message without an ID: {}", msg.toJson().dump());
    throw std::runtime_error("Internal error: Message ID missing");
  }

  try {
    // Clear any existing response for this message ID
    {
      std::unique_lock<std::mutex> lock(responseMutex);
      responses.erase(messageId);
    }

    // Send the message
    if (!sendMessage(msg)) {
      throw std::runtime_error("Failed to send message");
    }

    spdlog::debug("Sent message ID: {}, waiting for response...", messageId);

    // Wait for response
    std::unique_lock<std::mutex> lock(responseMutex);
    if (!responseCV.wait_for(
            lock, std::chrono::seconds(timeoutSeconds),
            [this, &messageId]() { return responses.count(messageId) > 0; })) {
      spdlog::error("Timeout waiting for response for message ID: {}", messageId);
      responses.erase(messageId);
      throw std::runtime_error("Timeout waiting for response for message ID: " + messageId);
    }

    spdlog::debug("Response received for message ID: {}", messageId);
    json response = std::move(responses[messageId]);
    responses.erase(messageId);

    if (response.contains("messageType") && response["messageType"] == "ERROR") {
      spdlog::warn("Received error response for message ID {}: {}", messageId, response.dump());
    }

    return response;

  } catch (const std::exception& e) {
    spdlog::error("Error during sendAndWaitForResponse for message ID {}: {}", messageId, e.what());
    throw;
  }
}

void MessageProcessor::startMessageLoop() {
  std::lock_guard<std::mutex> lock(threadMutex);
  
  if (running.load()) {
    spdlog::debug("Message processing loop already running.");
    return;
  }

  if (messageThread.joinable()) {
    spdlog::warn("Message processing thread was joinable but not marked as running. Joining previous thread.");
    messageThread.join();
  }

  if (!connectionManager->isConnected()) {
    spdlog::warn("Cannot start message processing: Not connected.");
    return;
  }

  running.store(true);
  messageThread = std::thread(&MessageProcessor::messageLoop, this);

  spdlog::info("Message processing loop started");
}

void MessageProcessor::stopMessageLoop() {
  std::lock_guard<std::mutex> lock(threadMutex);
  
  if (!running.load()) {
    spdlog::debug("Message processing loop already stopped.");
    if (messageThread.joinable()) {
      messageThread.join();
    }
    return;
  }

  running.store(false);

  // Notify any waiting threads
  responseCV.notify_all();

  if (messageThread.joinable()) {
    auto future = std::async(std::launch::async, &std::thread::join, &messageThread);
    if (future.wait_for(std::chrono::seconds(2)) == std::future_status::timeout) {
      spdlog::error("Message processing thread join timed out. Detaching.");
      messageThread.detach();
    } else {
      spdlog::info("Message processing loop stopped");
    }
  } else {
    spdlog::warn("stopMessageLoop called but thread was not joinable.");
  }
}

void MessageProcessor::registerMessageHandler(MessageType type, MessageHandler handler) {
  std::lock_guard<std::mutex> lock(handlersMutex);
  messageHandlers[type] = std::move(handler);
  spdlog::debug("Registered message handler for type: {}", messageTypeToString(type));
}

void MessageProcessor::unregisterMessageHandler(MessageType type) {
  std::lock_guard<std::mutex> lock(handlersMutex);
  auto it = messageHandlers.find(type);
  if (it != messageHandlers.end()) {
    messageHandlers.erase(it);
    spdlog::debug("Unregistered message handler for type: {}", messageTypeToString(type));
  }
}

json MessageProcessor::getProcessingStats() const {
  std::lock_guard<std::mutex> lock(statsMutex);
  
  json stats;
  stats["messagesSent"] = messagesSent;
  stats["messagesReceived"] = messagesReceived;
  stats["messagesProcessed"] = messagesProcessed;
  stats["processingErrors"] = processingErrors;
  stats["running"] = running.load();
  
  return stats;
}

void MessageProcessor::messageLoop() {
  spdlog::info("Message processing loop started");

  try {
    while (running.load() && connectionManager->isConnected()) {
      auto* ws = connectionManager->getWebSocket();
      if (!ws) {
        spdlog::error("WebSocket stream is null in message loop.");
        break;
      }

      beast::flat_buffer buffer;
      boost::system::error_code ec;
      ws->read(buffer, ec);

      if (ec) {
        if (ec == beast::websocket::error::closed ||
            ec == boost::asio::error::operation_aborted || !running.load()) {
          spdlog::info("WebSocket connection closed or operation aborted. Exiting message loop.");
        } else {
          spdlog::error("WebSocket read error: {}", ec.message());
        }
        break;
      }

      std::string message = beast::buffers_to_string(buffer.data());
      spdlog::trace("Received raw message: {}", message);
      
      updateStats(0, 1, 0, 0);
      handleMessage(message);
    }
  } catch (const std::exception& e) {
    spdlog::error("Exception in message loop: {}", e.what());
    updateStats(0, 0, 0, 1);
  } catch (...) {
    spdlog::error("Unknown exception in message loop.");
    updateStats(0, 0, 0, 1);
  }

  running.store(false);
  spdlog::info("Message processing loop ended");
}

void MessageProcessor::handleMessage(const std::string& messageStr) {
  try {
    json j = json::parse(messageStr);

    // Basic validation
    if (!j.is_object() || !j.contains("messageType")) {
      spdlog::warn("Received invalid JSON message structure: {}", messageStr);
      updateStats(0, 0, 0, 1);
      return;
    }

    std::unique_ptr<Message> msg = createMessageFromJson(j);
    if (!msg) {
      spdlog::error("Failed to create message object from JSON: {}", messageStr);
      updateStats(0, 0, 0, 1);
      return;
    }

    spdlog::debug("Handling message type: {}, ID: {}",
                  messageTypeToString(msg->getMessageType()),
                  msg->getMessageId());

    // Handle responses for synchronous requests
    MessageType msgType = msg->getMessageType();
    if (msgType == MessageType::RESPONSE || msgType == MessageType::DISCOVERY_RESPONSE || 
        msgType == MessageType::ERR) {
      if (!msg->getOriginalMessageId().empty()) {
        handleResponse(msg->getOriginalMessageId(), msg->toJson());
      }
    }

    // Dispatch to registered handlers
    dispatchMessage(*msg);
    updateStats(0, 0, 1, 0);

  } catch (const json::parse_error& e) {
    spdlog::error("Error parsing JSON message: {}. Content: {}", e.what(), messageStr);
    updateStats(0, 0, 0, 1);
  } catch (const std::exception& e) {
    spdlog::error("Error handling message: {}", e.what());
    updateStats(0, 0, 0, 1);
  }
}

void MessageProcessor::handleResponse(const std::string& originalMessageId, const json& responseJson) {
  if (originalMessageId.empty()) {
    spdlog::warn("Received response/error message with no original message ID: {}", responseJson.dump());
    return;
  }

  spdlog::debug("Processing response/error for original message ID: {}", originalMessageId);

  {
    std::lock_guard<std::mutex> lock(responseMutex);
    responses[originalMessageId] = responseJson;
  }
  responseCV.notify_all();
}

void MessageProcessor::dispatchMessage(const Message& message) {
  std::lock_guard<std::mutex> lock(handlersMutex);
  
  auto it = messageHandlers.find(message.getMessageType());
  if (it != messageHandlers.end()) {
    try {
      it->second(message);
    } catch (const std::exception& e) {
      spdlog::error("Error in message handler for type {}: {}", 
                    messageTypeToString(message.getMessageType()), e.what());
      updateStats(0, 0, 0, 1);
    }
  } else {
    spdlog::trace("No handler registered for message type: {}", 
                  messageTypeToString(message.getMessageType()));
  }
}

void MessageProcessor::updateStats(size_t sent, size_t received, size_t processed, size_t errors) {
  std::lock_guard<std::mutex> lock(statsMutex);
  messagesSent += sent;
  messagesReceived += received;
  messagesProcessed += processed;
  processingErrors += errors;
}

} // namespace hydrogen
