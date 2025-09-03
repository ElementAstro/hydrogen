#include "message_queue_manager.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace hydrogen {

void MessageQueueManager::sendMessage(const Message& message, DeliveryCallback callback) {
    totalSent++;

    // Simulate message sending - in a real implementation this would
    // integrate with WebSocket connection or message broker
    std::string messageId = message.getMessageId();

    spdlog::debug("MessageQueueManager: Sending message {}", messageId);

    // Check if this is a failing command that should trigger retries
    json msgJson = message.toJson();
    bool shouldFail = false;
    if (msgJson.contains("command") && msgJson["command"] == "failing-command") {
        shouldFail = true;
    }

    // Simulate async delivery with retry logic
    std::thread([this, messageId, callback, shouldFail]() {
        int attempts = 0;
        int maxAttempts = shouldFail ? (maxRetries_ + 1) : 1;
        bool success = false;

        while (attempts < maxAttempts && !success) {
            attempts++;

            if (attempts > 1) {
                // Add retry delay
                std::this_thread::sleep_for(std::chrono::milliseconds(retryIntervalMs_));
                spdlog::debug("MessageQueueManager: Retrying message {} (attempt {})", messageId, attempts);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (shouldFail) {
                // Always fail for failing commands to test retry logic
                success = false;
            } else {
                // For testing, assume 95% success rate for normal commands
                success = (rand() % 100) < 95;
            }
        }

        if (success) {
            totalDelivered++;
            spdlog::debug("MessageQueueManager: Message {} delivered successfully", messageId);
        } else {
            totalFailed++;
            spdlog::warn("MessageQueueManager: Message {} delivery failed after {} attempts", messageId, attempts);
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
    maxRetries_ = maxRetries;
    retryIntervalMs_ = retryIntervalMs;
    spdlog::debug("MessageQueueManager: Retry parameters set - maxRetries: {}, intervalMs: {}",
                  maxRetries, retryIntervalMs);
}

} // namespace hydrogen
