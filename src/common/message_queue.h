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

namespace astrocomm {

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
   * @brief Constructs a new MessageQueueManager.
   */
  MessageQueueManager();

  /**
   * @brief Destroys the MessageQueueManager, releasing any resources.
   */
  ~MessageQueueManager();

  /**
   * @brief Starts the message processing thread.
   *
   * This method starts the internal thread responsible for processing the
   * message queue and sending messages.
   */
  void start();

  /**
   * @brief Stops the message processing thread.
   *
   * This method stops the internal thread, preventing further message
   * processing.
   */
  void stop();

  /**
   * @brief Sets the callback function used to send messages.
   *
   * @param sender The callback function to be used for sending messages.
   */
  void setMessageSender(MessageSendCallback sender);

  /**
   * @brief Sends a message, supporting QoS, retries, and acknowledgments.
   *
   * This method adds a message to the queue and configures its retry and
   * acknowledgment behavior based on the message's QoS level:
   * - AT_MOST_ONCE: Messages are sent once without tracking or retries
   * - AT_LEAST_ONCE: Messages will be retried until acknowledged or max retries reached
   * - EXACTLY_ONCE: Messages ensure delivery with deduplication
   *
   * @param msg The message to be sent.
   * @param ackCallback The callback function to be invoked when the message
   *                    is acknowledged (or not). Defaults to nullptr if no
   *                    acknowledgment is required.
   */
  void sendMessage(const Message &msg,
                   MessageAckCallback ackCallback = nullptr);

  /**
   * @brief Acknowledges that a message has been received.
   *
   * This method is called by the recipient of a message to indicate that
   * the message has been successfully (or unsuccessfully) received.
   *
   * @param messageId The ID of the message being acknowledged.
   * @param success True if the message was successfully received, false
   * otherwise.
   */
  void acknowledgeMessage(const std::string &messageId, bool success = true);

  /**
   * @brief Sets the default retry parameters for messages.
   *
   * @param maxRetries The maximum number of times to retry sending a message.
   * @param retryIntervalMs The interval (in milliseconds) between retries.
   */
  void setRetryParams(int maxRetries, int retryIntervalMs);

  /**
   * @brief Checks for message timeouts and expirations, and performs retries.
   *
   * This method is called periodically by the message processing thread to
   * check for messages that have timed out or expired, and to retry sending
   * them if necessary.
   */
  void processMessageQueue();

private:
  /**
   * @brief Process high priority messages in the queue.
   * 
   * This method processes messages with HIGH and CRITICAL priority first,
   * ensuring timely delivery of important messages.
   */
  void processHighPriorityMessages();

  /**
   * @brief Structure to store the status of a message in the queue.
   */
  struct MessageStatus {
    std::shared_ptr<Message> message; ///< The message itself.
    std::chrono::system_clock::time_point
        lastSentTime; ///< The time the message was last sent.
    std::chrono::system_clock::time_point
        expiryTime;    ///< The time the message expires.
    int retryCount{0}; ///< The number of times the message has been retried.
    int maxRetries{3}; ///< The maximum number of times to retry the message.
    int retryIntervalMs{
        1000}; ///< The interval between retries (in milliseconds).
    MessageAckCallback callback{
        nullptr}; ///< The acknowledgment callback for the message.
  };

  std::thread processingThread; ///< The message processing thread.

  MessageSendCallback
      messageSender; ///< The callback function used to send messages.

  std::mutex
      messagesMutex; ///< Mutex to protect access to the pendingMessages map.
  std::map<std::string, MessageStatus>
      pendingMessages; ///< Map of pending messages.

  std::priority_queue<std::pair<Message::Priority, std::string>>
      messageQueue; ///< Priority queue of messages to be sent.

  std::atomic<bool> running{false}; ///< Flag indicating whether the message
                                    ///< processing thread is running.
  std::condition_variable queueCV;  ///< Condition variable used to signal the
                                    ///< message processing thread.
  std::mutex queueMutex; ///< Mutex to protect access to the message queue.

  int defaultMaxRetries{3}; ///< Default maximum number of retries.
  int defaultRetryIntervalMs{
      1000}; ///< Default retry interval (in milliseconds).
};

} // namespace astrocomm