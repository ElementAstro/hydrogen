#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <vector>
#include <future>
#include <atomic>

#include "hydrogen/core/communication/protocols/tcp_communicator.h"

using namespace hydrogen::core::communication::protocols;
using namespace hydrogen::core;
using namespace std::chrono_literals;

class TcpCommunicatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create server configuration
        serverConfig_ = TcpCommunicatorFactory::createDefaultServerConfig(testPort_);
        server_ = TcpCommunicatorFactory::createServer(serverConfig_);

        // Create client configuration
        clientConfig_ = TcpCommunicatorFactory::createDefaultClientConfig("localhost", testPort_);
        client_ = TcpCommunicatorFactory::createClient(clientConfig_);

        // Reset counters
        messagesReceived_ = 0;
        messagesSent_ = 0;
        errorsReceived_ = 0;
    }

    void TearDown() override {
        if (client_ && client_->isConnected()) {
            client_->disconnect();
        }
        if (server_ && server_->isConnected()) {
            server_->disconnect();
        }

        // Give time for cleanup
        std::this_thread::sleep_for(100ms);
    }

    bool startServer() {
        ConnectionConfig config;
        bool success = server_->connect(config);
        if (success) {
            std::this_thread::sleep_for(100ms); // Give server time to start
        }
        return success;
    }

    bool connectClient() {
        ConnectionConfig config;
        bool success = client_->connect(config);
        if (success) {
            std::this_thread::sleep_for(100ms); // Give client time to connect
        }
        return success;
    }

    static constexpr uint16_t testPort_ = 8002;
    TcpConnectionConfig serverConfig_;
    TcpConnectionConfig clientConfig_;
    std::shared_ptr<hydrogen::core::communication::protocols::TcpCommunicator> server_;
    std::shared_ptr<hydrogen::core::communication::protocols::TcpCommunicator> client_;

    std::atomic<int> messagesReceived_{0};
    std::atomic<int> messagesSent_{0};
    std::atomic<int> errorsReceived_{0};
};

// Test Basic Server/Client Connection
TEST_F(TcpCommunicatorTest, BasicServerClientConnection) {
    // Start server
    ASSERT_TRUE(startServer());
    EXPECT_TRUE(server_->isConnected());
    EXPECT_TRUE(server_->isServerMode());
    
    // Connect client
    ASSERT_TRUE(connectClient());
    EXPECT_TRUE(client_->isConnected());
    EXPECT_FALSE(client_->isServerMode());
    
    // Verify connection
    std::this_thread::sleep_for(200ms);
    
    // Check server has client connections
    auto connectedClients = server_->getConnectedClients();
    EXPECT_GT(connectedClients.size(), 0);
}

// Test Message Sending and Receiving
TEST_F(TcpCommunicatorTest, MessageSendingAndReceiving) {
    // Set up message callbacks
    server_->setMessageCallback([this](const CommunicationMessage& message) {
        messagesReceived_.fetch_add(1);
        EXPECT_EQ(message.command, "test_command");
        EXPECT_EQ(message.deviceId, "test_client");
    });
    
    client_->setMessageCallback([this](const CommunicationMessage& message) {
        messagesReceived_.fetch_add(1);
        EXPECT_EQ(message.command, "response");
    });
    
    // Start server and connect client
    ASSERT_TRUE(startServer());
    ASSERT_TRUE(connectClient());
    
    // Send message from client to server
    CommunicationMessage message;
    message.messageId = "test_msg_001";
    message.deviceId = "test_client";
    message.command = "test_command";
    message.payload = json{{"data", "test_payload"}};
    message.timestamp = std::chrono::system_clock::now();
    
    auto future = client_->sendMessage(message);
    auto response = future.get();
    
    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.messageId, message.messageId);
    
    // Wait for message to be received
    std::this_thread::sleep_for(200ms);
    
    EXPECT_GE(messagesReceived_.load(), 1);
}

// Test Multiple Concurrent Messages
TEST_F(TcpCommunicatorTest, ConcurrentMessageSending) {
    // Set up server message callback
    server_->setMessageCallback([this](const CommunicationMessage& message) {
        messagesReceived_.fetch_add(1);
    });
    
    // Start server and connect client
    ASSERT_TRUE(startServer());
    ASSERT_TRUE(connectClient());
    
    const int messageCount = 50;
    std::vector<std::future<CommunicationResponse>> futures;
    
    // Send multiple messages concurrently
    for (int i = 0; i < messageCount; ++i) {
        CommunicationMessage message;
        message.messageId = "concurrent_msg_" + std::to_string(i);
        message.deviceId = "concurrent_client";
        message.command = "concurrent_test";
        message.payload = json{{"index", i}, {"data", "concurrent_data"}};
        message.timestamp = std::chrono::system_clock::now();
        
        futures.push_back(client_->sendMessage(message));
    }
    
    // Wait for all messages to complete
    int successCount = 0;
    for (auto& future : futures) {
        auto response = future.get();
        if (response.success) {
            successCount++;
        }
    }
    
    EXPECT_EQ(successCount, messageCount);
    
    // Wait for server to receive all messages
    std::this_thread::sleep_for(500ms);
    
    EXPECT_GE(messagesReceived_.load(), messageCount * 0.8); // Allow for some message loss
}

// Test Connection Error Handling
TEST_F(TcpCommunicatorTest, ConnectionErrorHandling) {
    // Try to connect client without server
    EXPECT_FALSE(connectClient());
    EXPECT_FALSE(client_->isConnected());
    
    // Try to send message on disconnected client
    CommunicationMessage message;
    message.messageId = "error_test";
    message.deviceId = "error_client";
    message.command = "error_command";
    message.payload = json{{"test", "error"}};
    message.timestamp = std::chrono::system_clock::now();
    
    auto future = client_->sendMessage(message);
    auto response = future.get();
    
    EXPECT_FALSE(response.success);
    EXPECT_FALSE(response.errorMessage.empty());
}

// Test Server Multiple Clients
TEST_F(TcpCommunicatorTest, ServerMultipleClients) {
    // Start server
    ASSERT_TRUE(startServer());
    
    // Create multiple clients
    const int clientCount = 3;
    std::vector<std::shared_ptr<hydrogen::core::communication::protocols::TcpCommunicator>> clients;
    
    for (int i = 0; i < clientCount; ++i) {
        auto clientConfig = TcpCommunicatorFactory::createDefaultClientConfig("localhost", testPort_);
        auto client = TcpCommunicatorFactory::createClient(clientConfig);
        
        ConnectionConfig config;
        if (client->connect(config)) {
            clients.push_back(client);
        }
    }
    
    // Give time for connections
    std::this_thread::sleep_for(300ms);
    
    // Check server has all clients
    auto connectedClients = server_->getConnectedClients();
    EXPECT_EQ(connectedClients.size(), clients.size());
    
    // Send broadcast message
    CommunicationMessage broadcastMessage;
    broadcastMessage.messageId = "broadcast_test";
    broadcastMessage.deviceId = "server";
    broadcastMessage.command = "broadcast";
    broadcastMessage.payload = json{{"message", "Hello all clients!"}};
    broadcastMessage.timestamp = std::chrono::system_clock::now();
    
    bool broadcastSuccess = server_->sendToAllClients(broadcastMessage);
    EXPECT_TRUE(broadcastSuccess);
    
    // Cleanup clients
    for (auto& client : clients) {
        client->disconnect();
    }
}

// Test Performance Optimization Integration
TEST_F(TcpCommunicatorTest, PerformanceOptimizationIntegration) {
    // Create client with performance optimizations
    auto optimizedClient = TcpCommunicatorFactory::createWithPerformanceOptimization(
        clientConfig_, true, true, true, true);
    
    // Start server
    ASSERT_TRUE(startServer());
    
    // Connect optimized client
    ConnectionConfig config;
    ASSERT_TRUE(optimizedClient->connect(config));
    EXPECT_TRUE(optimizedClient->isConnected());
    
    // Test high-volume message sending
    const int messageCount = 100;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<CommunicationResponse>> futures;
    for (int i = 0; i < messageCount; ++i) {
        CommunicationMessage message;
        message.messageId = "perf_msg_" + std::to_string(i);
        message.deviceId = "perf_client";
        message.command = "performance_test";
        message.payload = json{{"index", i}, {"data", std::string(100, 'A')}};
        message.timestamp = std::chrono::system_clock::now();
        
        futures.push_back(optimizedClient->sendMessage(message));
    }
    
    // Wait for completion
    int successCount = 0;
    for (auto& future : futures) {
        auto response = future.get();
        if (response.success) {
            successCount++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_GT(successCount, messageCount * 0.9); // At least 90% success rate
    EXPECT_LT(duration.count(), 5000); // Should complete within 5 seconds
    
    // Check performance metrics
    auto stats = optimizedClient->getStatistics();
    EXPECT_GT(stats.messagesSent, 0);
    EXPECT_GT(stats.averageResponseTime, 0.0);
    
    optimizedClient->disconnect();
}

// Test Configuration Management
TEST_F(TcpCommunicatorTest, ConfigurationManagement) {
    // Test different configuration types
    auto defaultConfig = TcpCommunicatorFactory::createDefaultClientConfig("localhost", testPort_);
    EXPECT_EQ(defaultConfig.serverAddress, "localhost");
    EXPECT_EQ(defaultConfig.serverPort, testPort_);
    EXPECT_FALSE(defaultConfig.isServer);
    
    auto serverConfig = TcpCommunicatorFactory::createDefaultServerConfig(testPort_);
    EXPECT_EQ(serverConfig.serverPort, testPort_);
    EXPECT_TRUE(serverConfig.isServer);
    
    auto highPerfConfig = TcpCommunicatorFactory::createHighPerformanceConfig();
    EXPECT_GT(highPerfConfig.bufferSize, defaultConfig.bufferSize);
    EXPECT_FALSE(highPerfConfig.enableNagle);
    EXPECT_TRUE(highPerfConfig.enableMessageBatching);
    
    // Test configuration serialization
    json configJson = defaultConfig.toJson();
    EXPECT_TRUE(configJson.contains("serverAddress"));
    EXPECT_TRUE(configJson.contains("serverPort"));
    EXPECT_TRUE(configJson.contains("isServer"));
    
    // Test configuration deserialization
    auto deserializedConfig = TcpConnectionConfig::fromJson(configJson);
    EXPECT_EQ(deserializedConfig.serverAddress, defaultConfig.serverAddress);
    EXPECT_EQ(deserializedConfig.serverPort, defaultConfig.serverPort);
    EXPECT_EQ(deserializedConfig.isServer, defaultConfig.isServer);
}

// Test Connection Manager
TEST_F(TcpCommunicatorTest, ConnectionManager) {
    auto& manager = TcpConnectionManager::getInstance();
    
    // Register connections
    manager.registerConnection("test_server", server_);
    manager.registerConnection("test_client", client_);
    
    // Retrieve connections
    auto retrievedServer = manager.getConnection("test_server");
    auto retrievedClient = manager.getConnection("test_client");
    
    EXPECT_EQ(retrievedServer, server_);
    EXPECT_EQ(retrievedClient, client_);
    
    // Test non-existent connection
    auto nonExistent = manager.getConnection("non_existent");
    EXPECT_EQ(nonExistent, nullptr);
    
    // Start connections and get metrics
    ASSERT_TRUE(startServer());
    ASSERT_TRUE(connectClient());
    
    auto allMetrics = manager.getAllConnectionMetrics();
    EXPECT_TRUE(allMetrics.contains("test_server"));
    EXPECT_TRUE(allMetrics.contains("test_client"));
    
    // Unregister connections
    manager.unregisterConnection("test_server");
    manager.unregisterConnection("test_client");
    
    auto retrievedAfterUnregister = manager.getConnection("test_server");
    EXPECT_EQ(retrievedAfterUnregister, nullptr);
}

// Test Statistics and Metrics
TEST_F(TcpCommunicatorTest, StatisticsAndMetrics) {
    // Start server and connect client
    ASSERT_TRUE(startServer());
    ASSERT_TRUE(connectClient());
    
    // Send some messages
    for (int i = 0; i < 5; ++i) {
        CommunicationMessage message;
        message.messageId = "stats_msg_" + std::to_string(i);
        message.deviceId = "stats_client";
        message.command = "stats_test";
        message.payload = json{{"index", i}};
        message.timestamp = std::chrono::system_clock::now();
        
        auto future = client_->sendMessage(message);
        auto response = future.get();
        EXPECT_TRUE(response.success);
    }
    
    // Check client statistics
    auto clientStats = client_->getStatistics();
    EXPECT_GE(clientStats.messagesSent, 5);
    EXPECT_GT(clientStats.averageResponseTime, 0.0);
    
    // Check TCP-specific metrics
    auto tcpMetrics = client_->getTcpMetrics();
    EXPECT_GE(tcpMetrics.messagesSent.load(), 5);
    EXPECT_GT(tcpMetrics.averageLatency.load(), 0.0);
    
    // Test metrics serialization
    json metricsJson = tcpMetrics.toJson();
    EXPECT_TRUE(metricsJson.contains("messagesSent"));
    EXPECT_TRUE(metricsJson.contains("averageLatency"));
    
    // Reset statistics
    client_->resetStatistics();
    auto resetStats = client_->getStatistics();
    EXPECT_EQ(resetStats.messagesSent, 0);
}

// Test QoS and Optimization Settings
TEST_F(TcpCommunicatorTest, QoSAndOptimizationSettings) {
    // Test QoS parameters
    json qosParams = {
        {"priority", "high"},
        {"timeout", 5000},
        {"retries", 3}
    };
    
    client_->setQoSParameters(qosParams);
    
    // Test compression settings
    client_->setCompressionEnabled(true);
    
    // Test encryption settings
    client_->setEncryptionEnabled(true, "test_key");
    
    // Test performance optimization toggles
    client_->enableConnectionPooling(true);
    client_->enableMessageBatching(true);
    client_->enableMemoryPooling(true);
    client_->enableSerializationOptimization(true);
    
    // Verify settings don't break basic functionality
    ASSERT_TRUE(startServer());
    ASSERT_TRUE(connectClient());
    
    CommunicationMessage message;
    message.messageId = "qos_test";
    message.deviceId = "qos_client";
    message.command = "qos_command";
    message.payload = json{{"test", "qos_data"}};
    message.timestamp = std::chrono::system_clock::now();
    
    auto future = client_->sendMessage(message);
    auto response = future.get();
    
    EXPECT_TRUE(response.success);
}

// Test Supported Protocols
TEST_F(TcpCommunicatorTest, SupportedProtocols) {
    auto protocols = client_->getSupportedProtocols();
    
    EXPECT_FALSE(protocols.empty());
    EXPECT_TRUE(std::find(protocols.begin(), protocols.end(), CommunicationProtocol::TCP) != protocols.end());
}

// Test Error Recovery
TEST_F(TcpCommunicatorTest, ErrorRecovery) {
    // Start server and connect client
    ASSERT_TRUE(startServer());
    ASSERT_TRUE(connectClient());
    
    // Disconnect server to simulate network failure
    server_->disconnect();
    std::this_thread::sleep_for(200ms);
    
    // Try to send message (should fail)
    CommunicationMessage message;
    message.messageId = "recovery_test";
    message.deviceId = "recovery_client";
    message.command = "recovery_command";
    message.payload = json{{"test", "recovery"}};
    message.timestamp = std::chrono::system_clock::now();
    
    auto future = client_->sendMessage(message);
    auto response = future.get();
    
    EXPECT_FALSE(response.success);
    EXPECT_FALSE(response.errorMessage.empty());
    
    // Restart server
    ASSERT_TRUE(startServer());
    
    // Reconnect client
    client_->disconnect();
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(connectClient());
    
    // Try sending message again (should succeed)
    message.messageId = "recovery_test_2";
    auto future2 = client_->sendMessage(message);
    auto response2 = future2.get();
    
    EXPECT_TRUE(response2.success);
}
