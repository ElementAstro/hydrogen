#include "message_queue_manager.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace hydrogen {

void MessageQueueManager::sendMessage(const Message& message, DeliveryCallback callback) {
    totalSent++;
    
    // Simulate message sending - in a real implementation this would
    // integrate with WebSocket connection or message broker
    std::string messageId = message.getMessageId();
    
    spdlog::debug("MessageQueueManager: Sending message {}", messageId);
    
    // Simulate async delivery with a small delay
    std::thread([this, messageId, callback]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // For testing, assume 95% success rate
        bool success = (rand() % 100) < 95;
        
        if (success) {
            totalDelivered++;
            spdlog::debug("MessageQueueManager: Message {} delivered successfully", messageId);
        } else {
            totalFailed++;
            spdlog::warn("MessageQueueManager: Message {} delivery failed", messageId);
        }
        
        if (callback) {
            callback(messageId, success);
        }
    }).detach();
}

bool MessageQueueManager::sendMessageSync(const Message& message) {
    totalSent++;
    
    std::string messageId = message.getMessageId();
    spdlog::debug("MessageQueueManager: Sending message {} (sync)", messageId);
    
    // Simulate synchronous delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // For testing, assume 95% success rate
    bool success = (rand() % 100) < 95;
    
    if (success) {
        totalDelivered++;
        spdlog::debug("MessageQueueManager: Message {} delivered successfully (sync)", messageId);
    } else {
        totalFailed++;
        spdlog::warn("MessageQueueManager: Message {} delivery failed (sync)", messageId);
    }
    
    return success;
}

nlohmann::json MessageQueueManager::getDeliveryStats() const {
    nlohmann::json stats;
    stats["totalSent"] = totalSent.load();
    stats["totalDelivered"] = totalDelivered.load();
    stats["totalFailed"] = totalFailed.load();
    stats["successRate"] = totalSent > 0 ? (double)totalDelivered / totalSent : 0.0;
    return stats;
}

void MessageQueueManager::setMessageSender(std::function<void(const Message&)> sender) {
    // Store the sender function for future use
    // For now, this is just a stub for compatibility
    spdlog::debug("MessageQueueManager: Message sender configured");
    (void)sender; // Avoid unused parameter warning
}

void MessageQueueManager::start() {
    spdlog::info("MessageQueueManager: Started");
}

void MessageQueueManager::stop() {
    spdlog::info("MessageQueueManager: Stopped");
}

void MessageQueueManager::setRetryParams(int maxRetries, int retryIntervalMs) {
    spdlog::debug("MessageQueueManager: Retry parameters set - maxRetries: {}, intervalMs: {}",
                  maxRetries, retryIntervalMs);
    // Store these parameters for future use
    (void)maxRetries; // Avoid unused parameter warnings
    (void)retryIntervalMs;
}

} // namespace hydrogen
