#pragma once

#include "message.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace hydrogen {
namespace core {

/**
 * @brief Callback function type for sending messages.
 *
 * This callback is used by the MessageQueueManager to send messages. The
 * implementation of this callback is responsible for the actual message
 * transmission.
 *
 * @param msg The message to be sent.
 * @return True if the message was successfully sent, false otherwise.
 */
using MessageSendCallback = std::function<bool(const Message &)>;

/**
 * @brief Callback function type for message acknowledgments.
 *
 * This callback is invoked when a message is acknowledged (or not) by the
 * recipient. It allows the MessageQueueManager to track the status of sent
 * messages.
 *
 * @param messageId The ID of the message being acknowledged.
 * @param success True if the message was successfully received, false
 * otherwise.
 */
using MessageAckCallback = std::function<void(const std::string &, bool)>;

/**
 * @brief Manages a queue of messages, handling retries and acknowledgments.
 *
 * The MessageQueueManager provides a mechanism for sending messages with
 * configurable quality of service (QoS), including retries and acknowledgments.
 * It uses a priority queue to ensure that high-priority messages are sent
 * first.
 */
class MessageQueueManager {
public:
  /**
   * @brief Constructs a MessageQueueManager with the given send callback.
   *
   * @param sendCallback The callback function to use for sending messages.
   */
  explicit MessageQueueManager(MessageSendCallback sendCallback);

  /**
   * @brief Destructor. Stops the queue manager if it's running.
   */
  ~MessageQueueManager();

  /**
   * @brief Starts the message queue processing.
   *
   * This method starts a background thread that processes the message queue.
   * Messages are sent according to their priority and QoS settings.
   */
  void start();

  /**
   * @brief Stops the message queue processing.
   *
   * This method stops the background thread and clears the message queue.
   */
  void stop();

  /**
   * @brief Adds a message to the queue for sending.
   *
   * @param message The message to be queued for sending.
   */
  void enqueue(std::unique_ptr<Message> message);

  /**
   * @brief Acknowledges the receipt of a message.
   *
   * This method should be called when a message acknowledgment is received.
   * It removes the message from the retry queue if it exists.
   *
   * @param messageId The ID of the message being acknowledged.
   * @param success True if the message was successfully received, false
   * otherwise.
   */
  void acknowledge(const std::string &messageId, bool success = true);

  /**
   * @brief Sets the callback for message acknowledgments.
   *
   * @param ackCallback The callback function to invoke when messages are
   * acknowledged.
   */
  void setAckCallback(MessageAckCallback ackCallback);

  /**
   * @brief Sets the maximum number of retry attempts for failed messages.
   *
   * @param maxRetries The maximum number of retry attempts (default: 3).
   */
  void setMaxRetries(int maxRetries);

  /**
   * @brief Sets the base retry interval for failed messages.
   *
   * The actual retry interval is calculated using exponential backoff:
   * interval = baseInterval * (2 ^ attemptNumber)
   *
   * @param interval The base retry interval in milliseconds (default: 1000).
   */
  void setRetryInterval(std::chrono::milliseconds interval);

  /**
   * @brief Gets the current queue size.
   *
   * @return The number of messages currently in the queue.
   */
  size_t getQueueSize() const;

  /**
   * @brief Gets the number of messages currently pending retry.
   *
   * @return The number of messages in the retry queue.
   */
  size_t getRetryQueueSize() const;

private:
  /**
   * @brief Structure representing a queued message with metadata.
   */
  struct QueuedMessage {
    std::unique_ptr<Message> message;
    std::chrono::steady_clock::time_point nextRetryTime;
    int retryCount;

    QueuedMessage(std::unique_ptr<Message> msg)
        : message(std::move(msg)),
          nextRetryTime(std::chrono::steady_clock::now()), retryCount(0) {}
  };

  /**
   * @brief Comparator for priority queue ordering.
   */
  struct MessageComparator {
    bool operator()(const std::unique_ptr<QueuedMessage> &a,
                    const std::unique_ptr<QueuedMessage> &b) const;
  };

  /**
   * @brief The main processing loop for the message queue.
   */
  void processQueue();

  /**
   * @brief Sends a message and handles the result.
   *
   * @param queuedMsg The queued message to send.
   * @return True if the message was successfully sent, false otherwise.
   */
  bool sendMessage(const std::unique_ptr<QueuedMessage> &queuedMsg);

  /**
   * @brief Handles a failed message send by scheduling a retry if appropriate.
   *
   * @param queuedMsg The queued message that failed to send.
   */
  void handleFailedMessage(std::unique_ptr<QueuedMessage> queuedMsg);

  /**
   * @brief Calculates the next retry time for a failed message.
   *
   * @param retryCount The current retry count for the message.
   * @return The time point when the next retry should be attempted.
   */
  std::chrono::steady_clock::time_point
  calculateNextRetryTime(int retryCount) const;

  // Configuration
  MessageSendCallback sendCallback_;
  MessageAckCallback ackCallback_;
  int maxRetries_;
  std::chrono::milliseconds baseRetryInterval_;

  // Queue management
  mutable std::mutex queueMutex_;
  std::priority_queue<std::unique_ptr<QueuedMessage>,
                      std::vector<std::unique_ptr<QueuedMessage>>,
                      MessageComparator>
      messageQueue_;

  // Retry management
  mutable std::mutex retryMutex_;
  std::map<std::string, std::unique_ptr<QueuedMessage>> retryQueue_;

  // Thread management
  std::atomic<bool> running_;
  std::thread processingThread_;
  std::condition_variable queueCondition_;

  // Statistics
  std::atomic<size_t> totalMessagesSent_;
  std::atomic<size_t> totalMessagesAcknowledged_;
  std::atomic<size_t> totalMessagesFailed_;
};

} // namespace core
} // namespace hydrogen
