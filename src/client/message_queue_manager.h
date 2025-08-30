#pragma once

#include "common/message.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace astrocomm {

/**
 * @brief Manages message queuing and delivery with QoS support
 * 
 * This is a simplified implementation for testing the refactored architecture.
 * In a production system, this would integrate with a proper message broker.
 */
class MessageQueueManager {
public:
    using DeliveryCallback = std::function<void(const std::string& messageId, bool success)>;

    MessageQueueManager() = default;
    ~MessageQueueManager() = default;

    /**
     * @brief Send a message with delivery confirmation callback
     * @param message The message to send
     * @param callback Callback function called when delivery is confirmed/failed
     */
    void sendMessage(const Message& message, DeliveryCallback callback);

    /**
     * @brief Send a message and wait for delivery confirmation
     * @param message The message to send
     * @return true if message was delivered successfully
     */
    bool sendMessageSync(const Message& message);

    /**
     * @brief Get delivery statistics
     * @return JSON object with delivery statistics
     */
    nlohmann::json getDeliveryStats() const;

    /**
     * @brief Set message sender callback (stub for compatibility)
     * @param sender Function to send messages
     */
    void setMessageSender(std::function<void(const Message&)> sender);

    /**
     * @brief Start the message queue manager
     */
    void start();

    /**
     * @brief Stop the message queue manager
     */
    void stop();

    /**
     * @brief Set retry parameters
     * @param maxRetries Maximum number of retries
     * @param retryIntervalMs Retry interval in milliseconds
     */
    void setRetryParams(int maxRetries, int retryIntervalMs);

private:
    // Simple counters for testing
    mutable std::atomic<int> totalSent{0};
    mutable std::atomic<int> totalDelivered{0};
    mutable std::atomic<int> totalFailed{0};
};

} // namespace astrocomm
