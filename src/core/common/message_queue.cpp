#include "hydrogen/core/message_queue.h"

#include <chrono>
#include <functional>

namespace hydrogen {
namespace core {

// Helper function to create a message from JSON
// This is a simplified implementation for build compatibility
std::unique_ptr<Message> createMessageFromJson(const std::string &json) {
  // For now, return a simple message implementation
  // In a real implementation, this would parse the JSON and create the
  // appropriate message type
  (void)json;     // Suppress unused parameter warning
  return nullptr; // Simplified implementation
}

MessageQueueManager::MessageQueueManager(MessageSendCallback sendCallback)
    : sendCallback_(sendCallback), maxRetries_(3), baseRetryInterval_(1000),
      running_(false), totalMessagesSent_(0), totalMessagesAcknowledged_(0),
      totalMessagesFailed_(0) {}

MessageQueueManager::~MessageQueueManager() { stop(); }

void MessageQueueManager::start() {
  if (running_.load()) {
    return; // Already running
  }

  running_.store(true);
  processingThread_ = std::thread(&MessageQueueManager::processQueue, this);
}

void MessageQueueManager::stop() {
  if (!running_.load()) {
    return; // Already stopped
  }

  running_.store(false);
  queueCondition_.notify_all(); // Wake up processing thread

  if (processingThread_.joinable()) {
    processingThread_.join();
  }

  // Clear queues
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!messageQueue_.empty()) {
      messageQueue_.pop();
    }
  }

  {
    std::lock_guard<std::mutex> lock(retryMutex_);
    retryQueue_.clear();
  }
}

void MessageQueueManager::enqueue(std::unique_ptr<Message> message) {
  if (!running_.load()) {
    return;
  }

  auto queuedMsg = std::make_unique<QueuedMessage>(std::move(message));

  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    messageQueue_.push(std::move(queuedMsg));
  }

  queueCondition_.notify_one();
}

void MessageQueueManager::acknowledge(const std::string &messageId,
                                      bool success) {
  {
    std::lock_guard<std::mutex> lock(retryMutex_);
    auto it = retryQueue_.find(messageId);
    if (it != retryQueue_.end()) {
      retryQueue_.erase(it);
    }
  }

  if (ackCallback_) {
    ackCallback_(messageId, success);
  }

  if (success) {
    totalMessagesAcknowledged_.fetch_add(1);
  } else {
    totalMessagesFailed_.fetch_add(1);
  }
}

void MessageQueueManager::setAckCallback(MessageAckCallback ackCallback) {
  ackCallback_ = ackCallback;
}

void MessageQueueManager::setMaxRetries(int maxRetries) {
  maxRetries_ = maxRetries;
}

void MessageQueueManager::setRetryInterval(std::chrono::milliseconds interval) {
  baseRetryInterval_ = interval;
}

size_t MessageQueueManager::getQueueSize() const {
  std::lock_guard<std::mutex> lock(queueMutex_);
  return messageQueue_.size();
}

size_t MessageQueueManager::getRetryQueueSize() const {
  std::lock_guard<std::mutex> lock(retryMutex_);
  return retryQueue_.size();
}

bool MessageQueueManager::MessageComparator::operator()(
    const std::unique_ptr<QueuedMessage> &a,
    const std::unique_ptr<QueuedMessage> &b) const {
  // Higher priority messages should be processed first
  // Since priority queue is max-heap, we reverse the comparison
  auto priorityA = a->message->getPriority();
  auto priorityB = b->message->getPriority();

  if (priorityA != priorityB) {
    return priorityA < priorityB; // Higher priority (larger enum value) first
  }

  // If priorities are equal, process older messages first
  return a->nextRetryTime > b->nextRetryTime;
}

void MessageQueueManager::processQueue() {
  while (running_.load()) {
    std::unique_lock<std::mutex> lock(queueMutex_);

    // Wait for messages or stop signal
    queueCondition_.wait(
        lock, [this] { return !messageQueue_.empty() || !running_.load(); });

    if (!running_.load()) {
      break;
    }

    // Process messages
    while (!messageQueue_.empty() && running_.load()) {
      auto queuedMsg = std::move(
          const_cast<std::unique_ptr<QueuedMessage> &>(messageQueue_.top()));
      messageQueue_.pop();
      lock.unlock();

      // Check if message has expired
      if (queuedMsg->message->isExpired()) {
        if (ackCallback_) {
          ackCallback_(queuedMsg->message->getMessageId(), false);
        }
        totalMessagesFailed_.fetch_add(1);
        lock.lock();
        continue;
      }

      // Check if it's time to send/retry this message
      auto now = std::chrono::steady_clock::now();
      if (now >= queuedMsg->nextRetryTime) {
        bool sent = sendMessage(queuedMsg);
        if (!sent) {
          handleFailedMessage(std::move(queuedMsg));
        }
      } else {
        // Put message back in queue for later
        lock.lock();
        messageQueue_.push(std::move(queuedMsg));
        lock.unlock();

        // Sleep until next message is ready
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      lock.lock();
    }
  }
}

bool MessageQueueManager::sendMessage(
    const std::unique_ptr<QueuedMessage> &queuedMsg) {
  if (!sendCallback_) {
    return false;
  }

  bool sent = sendCallback_(*queuedMsg->message);

  if (sent) {
    totalMessagesSent_.fetch_add(1);

    // For messages requiring acknowledgment, add to retry queue
    auto qosLevel = queuedMsg->message->getQoSLevel();
    if (qosLevel == Message::QoSLevel::AT_LEAST_ONCE ||
        qosLevel == Message::QoSLevel::EXACTLY_ONCE) {
      std::lock_guard<std::mutex> lock(retryMutex_);
      auto messageId = queuedMsg->message->getMessageId();
      // Create a copy of the message for the retry queue
      auto messageCopy = createMessageFromJson(queuedMsg->message->toJson());
      auto retryMsg = std::make_unique<QueuedMessage>(std::move(messageCopy));
      retryMsg->nextRetryTime = queuedMsg->nextRetryTime;
      retryMsg->retryCount = queuedMsg->retryCount;
      retryQueue_[messageId] = std::move(retryMsg);
    } else {
      // AT_MOST_ONCE - fire and forget
      if (ackCallback_) {
        ackCallback_(queuedMsg->message->getMessageId(), true);
      }
    }
  }

  return sent;
}

void MessageQueueManager::handleFailedMessage(
    std::unique_ptr<QueuedMessage> queuedMsg) {
  queuedMsg->retryCount++;

  if (queuedMsg->retryCount >= maxRetries_) {
    // Max retries exceeded
    if (ackCallback_) {
      ackCallback_(queuedMsg->message->getMessageId(), false);
    }
    totalMessagesFailed_.fetch_add(1);
    return;
  }

  // Schedule retry
  queuedMsg->nextRetryTime = calculateNextRetryTime(queuedMsg->retryCount);

  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    messageQueue_.push(std::move(queuedMsg));
  }

  queueCondition_.notify_one();
}

std::chrono::steady_clock::time_point
MessageQueueManager::calculateNextRetryTime(int retryCount) const {
  // Exponential backoff: baseInterval * (2 ^ retryCount)
  auto backoffMs = baseRetryInterval_ * (1 << retryCount);
  return std::chrono::steady_clock::now() + backoffMs;
}

} // namespace core
} // namespace hydrogen
