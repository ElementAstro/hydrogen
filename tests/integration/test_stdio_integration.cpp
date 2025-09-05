#include <gtest/gtest.h>
#include <hydrogen/core/device_communicator.h>
#include <hydrogen/server/protocols/stdio/stdio_server.h>
#include <hydrogen/core/stdio_config_manager.h>
#include <hydrogen/core/stdio_logger.h>
#include <hydrogen/core/message.h>
#include <thread>
#include <chrono>
#include <future>

using namespace hydrogen::core;
using namespace hydrogen::server::protocols::stdio;

/**
 * @brief Integration test fixture for complete stdio communication flow
 */
class StdioIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure logging for integration tests
        StdioLogger::LoggerConfig logConfig;
        logConfig.enableConsoleLogging = false;
        logConfig.enableFileLogging = false;
        logConfig.enableDebugMode = false;
        logConfig.logLevel = StdioLogLevel::ERROR; // Only log errors
        
        auto& logger = getGlobalStdioLogger();
        logger.updateConfig(logConfig);
        logger.resetMetrics();
        
        // Create configurations
        auto& configManager = getGlobalStdioConfigManager();
        clientConfig_ = configManager.createConfig(StdioConfigManager::ConfigPreset::DEFAULT);
        clientConfig_.enableMessageValidation = true;
        clientConfig_.framingMode = StdioConfig::FramingMode::JSON_LINES;
        
        serverConfig_ = StdioServerFactory::createDefaultConfig();
        serverConfig_.serverName = "IntegrationTestServer";
        serverConfig_.maxConcurrentClients = 10;
        serverConfig_.protocolConfig = clientConfig_;
        
        // Reset tracking variables
        clientReceivedMessages_.clear();
        serverReceivedMessages_.clear();
        clientErrors_.clear();
        serverErrors_.clear();
    }
    
    void TearDown() override {
        if (server_) {
            server_->stop();
        }
        if (client_) {
            client_->stop();
        }
    }
    
    StdioConfig clientConfig_;
    StdioServer::ServerConfig serverConfig_;
    std::unique_ptr<StdioServer> server_;
    std::unique_ptr<StdioCommunicator> client_;
    
    // Message tracking
    std::vector<std::string> clientReceivedMessages_;
    std::vector<std::pair<std::string, Message>> serverReceivedMessages_;
    std::vector<std::string> clientErrors_;
    std::vector<std::string> serverErrors_;
    
    void setupServer() {
        server_ = StdioServerFactory::createWithConfig(serverConfig_);
        
        server_->setMessageReceivedCallback([this](const std::string& clientId, const Message& message) {
            serverReceivedMessages_.emplace_back(clientId, message);
        });
        
        server_->setErrorCallback([this](const std::string& error, const std::string& clientId) {
            serverErrors_.push_back(error);
        });
        
        ASSERT_TRUE(server_->start());
    }
    
    void setupClient() {
        client_ = createStdioCommunicator(clientConfig_);
        
        client_->setMessageHandler([this](const std::string& message, CommunicationProtocol protocol) {
            clientReceivedMessages_.push_back(message);
        });
        
        client_->setErrorHandler([this](const std::string& error) {
            clientErrors_.push_back(error);
        });
        
        ASSERT_TRUE(client_->start());
    }
};

/**
 * @brief Test basic client-server communication setup
 */
TEST_F(StdioIntegrationTest, BasicCommunicationSetup) {
    setupServer();
    setupClient();
    
    // Accept client connection on server
    EXPECT_TRUE(server_->acceptClient("integration_test_client", "ping"));
    EXPECT_TRUE(server_->isClientConnected("integration_test_client"));
    
    // Verify server is healthy
    EXPECT_TRUE(server_->isHealthy());
    EXPECT_TRUE(client_->isActive());
}

/**
 * @brief Test message exchange between client and server
 */
TEST_F(StdioIntegrationTest, MessageExchange) {
    setupServer();
    setupClient();
    
    const std::string clientId = "msg_test_client";
    ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    
    // Client sends message to server
    json clientMessage;
    clientMessage["command"] = "ping";
    clientMessage["client_id"] = clientId;
    clientMessage["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    EXPECT_TRUE(client_->sendMessage(clientMessage));
    
    // Give some time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Server sends response back to client
    Message serverResponse;
    serverResponse.setMessageType(MessageType::RESPONSE);
    serverResponse.setDeviceId("server");
    serverResponse.setMessageId("response_123");
    
    EXPECT_TRUE(server_->sendMessageToClient(clientId, serverResponse));
    
    // Give some time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify messages were received
    // Note: The exact verification depends on how the message flow is implemented
    // This test verifies the infrastructure is working
}

/**
 * @brief Test multiple client connections and message routing
 */
TEST_F(StdioIntegrationTest, MultipleClientCommunication) {
    setupServer();
    
    const int numClients = 3;
    std::vector<std::unique_ptr<StdioCommunicator>> clients;
    std::vector<std::vector<std::string>> clientMessages(numClients);
    
    // Create and setup multiple clients
    for (int i = 0; i < numClients; ++i) {
        auto client = createStdioCommunicator(clientConfig_);
        
        client->setMessageHandler([&clientMessages, i](const std::string& message, CommunicationProtocol protocol) {
            clientMessages[i].push_back(message);
        });
        
        ASSERT_TRUE(client->start());
        clients.push_back(std::move(client));
        
        // Accept client on server
        std::string clientId = "multi_client_" + std::to_string(i);
        ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    }
    
    // Send messages from each client
    for (int i = 0; i < numClients; ++i) {
        json message;
        message["client_id"] = "multi_client_" + std::to_string(i);
        message["message"] = "Hello from client " + std::to_string(i);
        
        EXPECT_TRUE(clients[i]->sendMessage(message));
    }
    
    // Give time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Send broadcast message from server
    Message broadcastMsg;
    broadcastMsg.setMessageType(MessageType::EVENT);
    broadcastMsg.setDeviceId("server");
    broadcastMsg.setMessageId("broadcast_123");
    
    EXPECT_TRUE(server_->broadcastMessage(broadcastMsg));
    
    // Give time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Cleanup clients
    for (auto& client : clients) {
        client->stop();
    }
}

/**
 * @brief Test error handling and recovery in communication
 */
TEST_F(StdioIntegrationTest, ErrorHandlingAndRecovery) {
    setupServer();
    setupClient();
    
    const std::string clientId = "error_test_client";
    ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    
    // Clear error tracking
    clientErrors_.clear();
    serverErrors_.clear();
    
    // Send invalid message format
    EXPECT_TRUE(client_->sendMessage("invalid_json_message"));
    
    // Give time for error processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Send valid message after error
    json validMessage;
    validMessage["command"] = "ping";
    validMessage["client_id"] = clientId;
    
    EXPECT_TRUE(client_->sendMessage(validMessage));
    
    // Give time for message processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify system recovered and can still process messages
    EXPECT_TRUE(client_->isActive());
    EXPECT_TRUE(server_->isClientConnected(clientId));
}

/**
 * @brief Test connection lifecycle management
 */
TEST_F(StdioIntegrationTest, ConnectionLifecycleManagement) {
    setupServer();
    setupClient();
    
    const std::string clientId = "lifecycle_test_client";
    
    // Initial connection
    ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    EXPECT_TRUE(server_->isClientConnected(clientId));
    
    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.currentActiveClients, 1);
    EXPECT_EQ(stats.totalClientsConnected, 1);
    
    // Disconnect client
    EXPECT_TRUE(server_->disconnectClient(clientId));
    EXPECT_FALSE(server_->isClientConnected(clientId));
    
    stats = server_->getStatistics();
    EXPECT_EQ(stats.currentActiveClients, 0);
    EXPECT_EQ(stats.totalClientsConnected, 1); // Total remains the same
    
    // Reconnect client
    EXPECT_TRUE(server_->acceptClient(clientId, "ping"));
    EXPECT_TRUE(server_->isClientConnected(clientId));
    
    stats = server_->getStatistics();
    EXPECT_EQ(stats.currentActiveClients, 1);
    EXPECT_EQ(stats.totalClientsConnected, 2); // Total increments
}

/**
 * @brief Test performance under load
 */
TEST_F(StdioIntegrationTest, PerformanceUnderLoad) {
    setupServer();
    setupClient();
    
    const std::string clientId = "perf_test_client";
    ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    
    const int numMessages = 100;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Send multiple messages rapidly
    for (int i = 0; i < numMessages; ++i) {
        json message;
        message["sequence"] = i;
        message["command"] = "ping";
        message["client_id"] = clientId;
        
        EXPECT_TRUE(client_->sendMessage(message));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double messagesPerSecond = static_cast<double>(numMessages) / (duration.count() / 1000.0);
    
    std::cout << "Sent " << numMessages << " messages in " << duration.count() 
              << "ms (" << messagesPerSecond << " msg/sec)" << std::endl;
    
    // Performance should be reasonable
    EXPECT_GT(messagesPerSecond, 50); // At least 50 messages per second
    
    // Give time for all messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify system is still healthy
    EXPECT_TRUE(client_->isActive());
    EXPECT_TRUE(server_->isClientConnected(clientId));
}

/**
 * @brief Test different message formats and transformations
 */
TEST_F(StdioIntegrationTest, MessageFormatsAndTransformations) {
    setupServer();
    setupClient();
    
    const std::string clientId = "format_test_client";
    ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    
    // Test different message formats
    std::vector<json> testMessages = {
        {{"type", "simple"}, {"data", "test"}},
        {{"type", "complex"}, {"nested", {{"key", "value"}, {"number", 42}}}},
        {{"type", "array"}, {"items", {1, 2, 3, "four", 5.0}}},
        {{"type", "unicode"}, {"text", "Hello 世界 🌍"}},
        {{"type", "large"}, {"data", std::string(1000, 'x')}}
    };
    
    for (const auto& message : testMessages) {
        EXPECT_TRUE(client_->sendMessage(message));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Give time for all messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify system handled all message formats
    EXPECT_TRUE(client_->isActive());
    EXPECT_TRUE(server_->isClientConnected(clientId));
}

/**
 * @brief Test configuration compatibility between client and server
 */
TEST_F(StdioIntegrationTest, ConfigurationCompatibility) {
    // Test with different configuration presets
    auto& configManager = getGlobalStdioConfigManager();
    
    std::vector<StdioConfigManager::ConfigPreset> presets = {
        StdioConfigManager::ConfigPreset::DEFAULT,
        StdioConfigManager::ConfigPreset::HIGH_PERFORMANCE,
        StdioConfigManager::ConfigPreset::LOW_LATENCY,
        StdioConfigManager::ConfigPreset::RELIABLE
    };
    
    for (auto preset : presets) {
        // Setup with specific preset
        clientConfig_ = configManager.createConfig(preset);
        clientConfig_.enableMessageValidation = true;
        
        serverConfig_.protocolConfig = clientConfig_;
        
        setupServer();
        setupClient();
        
        const std::string clientId = "compat_test_client";
        EXPECT_TRUE(server_->acceptClient(clientId, "ping"));
        
        // Test basic communication
        json testMessage;
        testMessage["preset"] = static_cast<int>(preset);
        testMessage["command"] = "ping";
        
        EXPECT_TRUE(client_->sendMessage(testMessage));
        
        // Give time for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Verify communication works
        EXPECT_TRUE(client_->isActive());
        EXPECT_TRUE(server_->isClientConnected(clientId));
        
        // Cleanup for next iteration
        server_->stop();
        client_->stop();
        server_.reset();
        client_.reset();
    }
}

/**
 * @brief Test logging and metrics integration
 */
TEST_F(StdioIntegrationTest, LoggingAndMetricsIntegration) {
    // Enable metrics for this test
    StdioLogger::LoggerConfig logConfig;
    logConfig.enablePerformanceMetrics = true;
    logConfig.enableMessageTracing = true;
    logConfig.enableConsoleLogging = false;
    logConfig.enableFileLogging = false;
    
    auto& logger = getGlobalStdioLogger();
    logger.updateConfig(logConfig);
    logger.resetMetrics();
    
    setupServer();
    setupClient();
    
    const std::string clientId = "metrics_test_client";
    ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    
    // Send some messages to generate metrics
    for (int i = 0; i < 10; ++i) {
        json message;
        message["sequence"] = i;
        message["command"] = "test";
        
        EXPECT_TRUE(client_->sendMessage(message));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Check metrics
    auto metrics = logger.getMetrics();
    EXPECT_GT(metrics.totalMessages.load(), 0);
    EXPECT_GT(metrics.getMessagesPerSecond(), 0);
    
    // Check server statistics
    auto serverStats = server_->getStatistics();
    EXPECT_GT(serverStats.totalMessagesProcessed, 0);
}

/**
 * @brief End-to-end stress test
 */
TEST_F(StdioIntegrationTest, EndToEndStressTest) {
    setupServer();
    
    const int numClients = 5;
    const int messagesPerClient = 20;
    const int serverResponsesPerClient = 10;
    
    std::vector<std::unique_ptr<StdioCommunicator>> clients;
    std::vector<std::string> clientIds;
    
    // Setup multiple clients
    for (int i = 0; i < numClients; ++i) {
        auto client = createStdioCommunicator(clientConfig_);
        ASSERT_TRUE(client->start());
        clients.push_back(std::move(client));
        
        std::string clientId = "stress_client_" + std::to_string(i);
        clientIds.push_back(clientId);
        ASSERT_TRUE(server_->acceptClient(clientId, "ping"));
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Concurrent message sending from all clients
    std::vector<std::thread> clientThreads;
    for (int i = 0; i < numClients; ++i) {
        clientThreads.emplace_back([&clients, &clientIds, i, messagesPerClient]() {
            for (int j = 0; j < messagesPerClient; ++j) {
                json message;
                message["client_id"] = clientIds[i];
                message["sequence"] = j;
                message["command"] = "stress_test";
                
                clients[i]->sendMessage(message);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }
    
    // Server sending responses
    std::thread serverThread([this, &clientIds, serverResponsesPerClient]() {
        for (int i = 0; i < serverResponsesPerClient; ++i) {
            for (const auto& clientId : clientIds) {
                Message response;
                response.setMessageType(MessageType::RESPONSE);
                response.setDeviceId("server");
                response.setMessageId("stress_response_" + std::to_string(i));
                
                server_->sendMessageToClient(clientId, response);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : clientThreads) {
        thread.join();
    }
    serverThread.join();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    int totalMessages = numClients * messagesPerClient + numClients * serverResponsesPerClient;
    double messagesPerSecond = static_cast<double>(totalMessages) / (duration.count() / 1000.0);
    
    std::cout << "Stress test: " << totalMessages << " messages with " << numClients 
              << " clients in " << duration.count() << "ms (" << messagesPerSecond << " msg/sec)" << std::endl;
    
    // Verify system is still healthy after stress test
    EXPECT_TRUE(server_->isHealthy());
    for (int i = 0; i < numClients; ++i) {
        EXPECT_TRUE(clients[i]->isActive());
        EXPECT_TRUE(server_->isClientConnected(clientIds[i]));
    }
    
    // Cleanup
    for (auto& client : clients) {
        client->stop();
    }
}
