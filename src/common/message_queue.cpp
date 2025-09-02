#include "message_queue.h"

#include <chrono>
#include <functional>
#ifdef HYDROGEN_HAS_SPDLOG
#include <spdlog/spdlog.h>
#endif


namespace hydrogen {

// Priority comparator for the priority queue
struct PriorityCompare {
  bool operator()(const std::pair<Message::Priority, std::string> &a,
                  const std::pair<Message::Priority, std::string> &b) {
    // Lower value means higher priority
    return a.first > b.first;
  }
};

MessageQueueManager::MessageQueueManager()
    : running(false), defaultMaxRetries(3), defaultRetryIntervalMs(1000) {}

MessageQueueManager::~MessageQueueManager() { stop(); }

void MessageQueueManager::start() {
  if (running.load()) {
    return; // Already running
  }

  running.store(true);
  processingThread =
      std::thread(&MessageQueueManager::processMessageQueue, this);

#ifdef HYDROGEN_HAS_SPDLOG
  SPDLOG_INFO("MessageQueueManager: Message queue processor started");
#endif
}

void MessageQueueManager::stop() {
  if (!running.load()) {
    return; // Already stopped
  }

  running.store(false);
  queueCV.notify_all(); // Wake up processing thread

  if (processingThread.joinable()) {
    processingThread.join();
  }

#ifdef HYDROGEN_HAS_SPDLOG
  SPDLOG_INFO("MessageQueueManager: Message queue processor stopped");
#endif
}

void MessageQueueManager::setMessageSender(MessageSendCallback sender) {
  std::lock_guard<std::mutex> lock(messagesMutex);
  messageSender = sender;
}

void MessageQueueManager::sendMessage(const Message &msg,
                                      MessageAckCallback ackCallback) {
  if (!messageSender) {
#ifdef HYDROGEN_HAS_SPDLOG
    SPDLOG_ERROR("MessageQueueManager: No message sender set");
#endif
    if (ackCallback) {
      ackCallback(msg.getMessageId(), false);
    }
    return;
  }

  auto qosLevel = msg.getQoSLevel();
  auto priority = msg.getPriority();
  auto messageId = msg.getMessageId();

  // QoS level AT_MOST_ONCE messages are sent directly without acknowledgment
  if (qosLevel == Message::QoSLevel::AT_MOST_ONCE) {
    bool sent = messageSender(msg);
    if (ackCallback) {
      ackCallback(messageId, sent);
    }
    return;
  }

  // For messages requiring acknowledgment, add to queue
  {
    std::lock_guard<std::mutex> lock(messagesMutex);

    // Create message status record
    MessageStatus status;
    status.message = std::make_shared<Message>(msg);
    status.lastSentTime = std::chrono::system_clock::now();
    status.callback = ackCallback;
    status.maxRetries = defaultMaxRetries;
    status.retryIntervalMs = defaultRetryIntervalMs;

    // If message has expiration time
    if (msg.getExpireAfter() > 0) {
      status.expiryTime =
          status.lastSentTime + std::chrono::seconds(msg.getExpireAfter());
    } else {
      // Default 24-hour expiry
      status.expiryTime = status.lastSentTime + std::chrono::hours(24);
    }

    pendingMessages[messageId] = status;
  }

  // Add to priority queue
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    messageQueue.push(std::make_pair(priority, messageId));
    queueCV.notify_one();
  }

  // Send immediately once
  bool sent = messageSender(msg);
#ifdef HYDROGEN_HAS_SPDLOG
  SPDLOG_DEBUG("MessageQueueManager: Message sent {}, QoS={}, Priority={}",
               messageId, static_cast<int>(qosLevel),
               static_cast<int>(priority));
#endif
}

void MessageQueueManager::acknowledgeMessage(const std::string &messageId,
                                             bool success) {
  {
    std::lock_guard<std::mutex> lock(messagesMutex);
    auto it = pendingMessages.find(messageId);
    if (it != pendingMessages.end()) {
      // Call callback function
      if (it->second.callback) {
        it->second.callback(messageId, success);
      }

      // Remove from queue
      pendingMessages.erase(it);
#ifdef HYDROGEN_HAS_SPDLOG
      SPDLOG_DEBUG("MessageQueueManager: Message acknowledged {}, success={}",
                   messageId, success);
#endif
    }
  }
}

void MessageQueueManager::setRetryParams(int maxRetries, int retryIntervalMs) {
  defaultMaxRetries = maxRetries;
  defaultRetryIntervalMs = retryIntervalMs;
#ifdef HYDROGEN_HAS_SPDLOG
  SPDLOG_INFO("MessageQueueManager: Retry parameters set maxRetries={}, "
              "retryIntervalMs={}",
              maxRetries, retryIntervalMs);
#endif
}

void MessageQueueManager::processMessageQueue() {
  while (running.load()) {
    // Use condition variable to wait, avoiding CPU spinning
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCV.wait_for(lock, std::chrono::milliseconds(100), [this]() {
      return !running.load() || !messageQueue.empty();
    });

    if (!running.load()) {
      break;
    }

    auto now = std::chrono::system_clock::now();
    std::vector<std::string> messagesToRetry;
    std::vector<std::string> messagesToRemove;

    // Check for messages that need retry
    {
      std::lock_guard<std::mutex> lock(messagesMutex);
      for (auto &[id, status] : pendingMessages) {
        // Check if message has expired
        if (now > status.expiryTime) {
#ifdef HYDROGEN_HAS_SPDLOG
          SPDLOG_WARN("MessageQueueManager: Message {} has expired", id);
#endif
          messagesToRemove.push_back(id);
          if (status.callback) {
            status.callback(id, false);
          }
          continue;
        }

        // Check if retry is needed
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - status.lastSentTime)
                           .count();

        if (elapsed >= status.retryIntervalMs) {
          // Check if maximum retry count has been reached
          if (status.retryCount >= status.maxRetries) {
#ifdef HYDROGEN_HAS_SPDLOG
            SPDLOG_WARN(
                "MessageQueueManager: Message {} retry count reached limit {}",
                id, status.maxRetries);
#endif
            messagesToRemove.push_back(id);
            if (status.callback) {
              status.callback(id, false);
            }
            continue;
          }

          // Needs retry
          messagesToRetry.push_back(id);
          status.retryCount++;
          status.lastSentTime = now;
#ifdef HYDROGEN_HAS_SPDLOG
          SPDLOG_DEBUG("MessageQueueManager: Retrying message {}, attempt {}",
                       id, status.retryCount);
#endif
        }
      }

      // Remove expired or failed retry messages from queue
      for (const auto &id : messagesToRemove) {
        pendingMessages.erase(id);
      }
    }

    // Perform retries
    for (const auto &id : messagesToRetry) {
      std::shared_ptr<Message> msgPtr = nullptr;

      {
        std::lock_guard<std::mutex> lock(messagesMutex);
        auto it = pendingMessages.find(id);
        if (it != pendingMessages.end()) {
          msgPtr = it->second.message;
        }
      }

      if (msgPtr && messageSender) {
        messageSender(*msgPtr);
      }
    }

    // Process any high-priority messages in the queue
    processHighPriorityMessages();
  }
}

void MessageQueueManager::processHighPriorityMessages() {
  // Process up to a batch of messages at once to avoid hogging the thread
  const int MAX_BATCH_SIZE = 10;
  int processedCount = 0;

  std::lock_guard<std::mutex> queueLock(queueMutex);

  while (!messageQueue.empty() && processedCount < MAX_BATCH_SIZE) {
    auto topMsg = messageQueue.top();
    messageQueue.pop();

    // Only process high priority and critical messages in this fast path
    if (topMsg.first > Message::Priority::NORMAL) {
      // We've already popped it, so re-add it to process later
      messageQueue.push(topMsg);
      break;
    }

    // Get message and process it
    std::string messageId = topMsg.second;
    std::shared_ptr<Message> msgPtr = nullptr;

    {
      std::lock_guard<std::mutex> messagesLock(messagesMutex);
      auto it = pendingMessages.find(messageId);
      if (it != pendingMessages.end()) {
        msgPtr = it->second.message;
      }
    }

    if (msgPtr && messageSender) {
      messageSender(*msgPtr);
      processedCount++;
    }
  }
}

} // namespace hydrogen