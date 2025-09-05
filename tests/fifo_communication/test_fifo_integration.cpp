#include <gtest/gtest.h>
#include <hydrogen/server/protocols/fifo/fifo_server.h>
#include <hydrogen/core/fifo_communicator.h>
#include <hydrogen/core/fifo_config_manager.h>
#include <hydrogen/core/fifo_logger.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <filesystem>

using namespace hydrogen::server::protocols::fifo;
using namespace hydrogen::core;
using json = nlohmann::json;

class FifoIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create unique test identifiers
        static int testCounter = 0;
        testId_ = std::to_string(++testCounter);
        
        // Configure server
        serverConfig_ = FifoServerFactory::createDefaultConfig();
        serverConfig_.serverName = "TestFifoServer_" + testId_;
        serverConfig_.serverId = "test_server_" + testId_;
        serverConfig_.maxConcurrentClients = 5;
        serverConfig_.enableServerLogging = false; // Reduce noise in tests
        
        // Configure FIFO protocol
#ifdef _WIN32
        serverConfig_.protocolConfig.windowsBasePipePath = "\\\\.\\pipe\\test_fifo_server_" + testId_;
#else
        serverConfig_.protocolConfig.basePipePath = "/tmp/test_fifo_server_" + testId_;
#endif
        
        // Configure client
        auto& configManager = getGlobalFifoConfigManager();
        clientConfig_ = configManager.createConfig();
        
#ifdef _WIN32
        clientConfig_.windowsPipePath = serverConfig_.protocolConfig.windowsBasePipePath + "_client1";
        clientConfig_.pipeType = FifoPipeType::WINDOWS_NAMED_PIPE;
#else
        clientConfig_.unixPipePath = serverConfig_.protocolConfig.basePipePath + "/client1";
        clientConfig_.pipeType = FifoPipeType::UNIX_FIFO;
#endif
        
        clientConfig_.connectTimeout = std::chrono::milliseconds(2000);
        clientConfig_.readTimeout = std::chrono::milliseconds(1000);
        clientConfig_.writeTimeout = std::chrono::milliseconds(1000);
        
        // Initialize logger with minimal settings for tests
        FifoLoggerConfig logConfig;
        logConfig.enableConsoleLogging = false;
        logConfig.enableFileLogging = false;
        logConfig.logLevel = FifoLogLevel::ERROR; // Only errors
        getGlobalFifoLogger().updateConfig(logConfig);
    }
    
    void TearDown() override {
        // Clean up test pipes
#ifndef _WIN32
        std::string basePath = serverConfig_.protocolConfig.basePipePath;
        if (std::filesystem::exists(basePath)) {
            std::filesystem::remove_all(basePath);
        }
#endif
    }
    
    std::string testId_;
    FifoServerConfig serverConfig_;
    FifoConfig clientConfig_;
};

// Test basic server-client communication
TEST_F(FifoIntegrationTest, BasicServerClientCommunication) {
    // Create and start server
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    std::atomic<bool> clientConnected{false};
    std::atomic<bool> messageReceived{false};
    std::string receivedMessage;
    
    // Set up server callbacks
    server->setClientConnectedCallback([&](const std::string& clientId) {
        clientConnected.store(true);
    });
    
    server->setMessageReceivedCallback([&](const std::string& clientId, const Message& message) {
        receivedMessage = message.payload;
        messageReceived.store(true);
    });
    
    EXPECT_TRUE(server->start());
    EXPECT_TRUE(server->isRunning());
    
    // Give server time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Accept a client
    EXPECT_TRUE(server->acceptClient("client1", "test"));
    
    // Give time for client acceptance
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify client is connected
    EXPECT_TRUE(server->isClientConnected("client1"));
    auto connectedClients = server->getConnectedClients();
    EXPECT_EQ(connectedClients.size(), 1);
    EXPECT_EQ(connectedClients[0], "client1");
    
    // Send message to client
    Message testMessage;
    testMessage.senderId = "server";
    testMessage.recipientId = "client1";
    testMessage.topic = "test";
    testMessage.payload = "Hello Client!";
    testMessage.sourceProtocol = CommunicationProtocol::FIFO;
    testMessage.targetProtocol = CommunicationProtocol::FIFO;
    
    EXPECT_TRUE(server->sendMessageToClient("client1", testMessage));
    
    // Clean up
    server->stop();
    EXPECT_FALSE(server->isRunning());
}

// Test multiple clients
TEST_F(FifoIntegrationTest, MultipleClients) {
    serverConfig_.maxConcurrentClients = 3;
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    std::atomic<int> clientsConnected{0};
    
    server->setClientConnectedCallback([&](const std::string& clientId) {
        clientsConnected.fetch_add(1);
    });
    
    EXPECT_TRUE(server->start());
    
    // Accept multiple clients
    EXPECT_TRUE(server->acceptClient("client1", "test"));
    EXPECT_TRUE(server->acceptClient("client2", "test"));
    EXPECT_TRUE(server->acceptClient("client3", "test"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify all clients are connected
    auto connectedClients = server->getConnectedClients();
    EXPECT_EQ(connectedClients.size(), 3);
    
    // Test broadcast message
    Message broadcastMessage;
    broadcastMessage.senderId = "server";
    broadcastMessage.topic = "broadcast";
    broadcastMessage.payload = "Hello Everyone!";
    broadcastMessage.sourceProtocol = CommunicationProtocol::FIFO;
    broadcastMessage.targetProtocol = CommunicationProtocol::FIFO;
    
    EXPECT_TRUE(server->broadcastMessage(broadcastMessage));
    
    // Disconnect clients
    EXPECT_TRUE(server->disconnectClient("client1"));
    EXPECT_TRUE(server->disconnectClient("client2"));
    EXPECT_TRUE(server->disconnectClient("client3"));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify clients are disconnected
    EXPECT_EQ(server->getConnectedClients().size(), 0);
    
    server->stop();
}

// Test client limit enforcement
TEST_F(FifoIntegrationTest, ClientLimitEnforcement) {
    serverConfig_.maxConcurrentClients = 2;
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    EXPECT_TRUE(server->start());
    
    // Accept clients up to limit
    EXPECT_TRUE(server->acceptClient("client1", "test"));
    EXPECT_TRUE(server->acceptClient("client2", "test"));
    
    // Try to accept one more (should fail)
    EXPECT_FALSE(server->acceptClient("client3", "test"));
    
    // Verify only 2 clients are connected
    EXPECT_EQ(server->getConnectedClients().size(), 2);
    
    server->stop();
}

// Test command filtering
TEST_F(FifoIntegrationTest, CommandFiltering) {
    serverConfig_.enableCommandFiltering = true;
    serverConfig_.allowedCommands = {"ping", "echo"};
    
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    EXPECT_TRUE(server->start());
    
    // Accept client with allowed command
    EXPECT_TRUE(server->acceptClient("client1", "ping"));
    
    // Try to accept client with disallowed command
    EXPECT_FALSE(server->acceptClient("client2", "forbidden"));
    
    // Verify only allowed client is connected
    EXPECT_EQ(server->getConnectedClients().size(), 1);
    
    server->stop();
}

// Test server statistics
TEST_F(FifoIntegrationTest, ServerStatistics) {
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    EXPECT_TRUE(server->start());
    
    // Get initial statistics
    auto initialStats = server->getStatistics();
    EXPECT_EQ(initialStats.totalClientsConnected.load(), 0);
    EXPECT_EQ(initialStats.currentActiveClients.load(), 0);
    
    // Accept a client
    EXPECT_TRUE(server->acceptClient("client1", "test"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check updated statistics
    auto updatedStats = server->getStatistics();
    EXPECT_EQ(updatedStats.totalClientsConnected.load(), 1);
    EXPECT_EQ(updatedStats.currentActiveClients.load(), 1);
    
    // Test statistics JSON serialization
    json statsJson = updatedStats.toJson();
    EXPECT_FALSE(statsJson.empty());
    EXPECT_TRUE(statsJson.contains("totalClientsConnected"));
    EXPECT_TRUE(statsJson.contains("currentActiveClients"));
    EXPECT_EQ(statsJson["totalClientsConnected"], 1);
    EXPECT_EQ(statsJson["currentActiveClients"], 1);
    
    server->stop();
}

// Test server health monitoring
TEST_F(FifoIntegrationTest, ServerHealthMonitoring) {
    serverConfig_.enableHealthChecking = true;
    serverConfig_.healthCheckInterval = std::chrono::milliseconds(100);
    
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    // Initially not healthy (not started)
    EXPECT_FALSE(server->isHealthy());
    
    EXPECT_TRUE(server->start());
    
    // Should be healthy after starting
    EXPECT_TRUE(server->isHealthy());
    
    std::string healthStatus = server->getHealthStatus();
    EXPECT_FALSE(healthStatus.empty());
    EXPECT_NE(healthStatus.find("HEALTHY"), std::string::npos);
    
    server->stop();
    
    // Should not be healthy after stopping
    EXPECT_FALSE(server->isHealthy());
}

// Test server configuration updates
TEST_F(FifoIntegrationTest, ServerConfigurationUpdates) {
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    EXPECT_TRUE(server->start());
    
    // Update configuration
    json configUpdates;
    configUpdates["maxConcurrentClients"] = 10;
    configUpdates["enableDebugMode"] = true;
    
    EXPECT_TRUE(server->updateConfig(configUpdates));
    
    // Verify configuration was updated
    auto updatedConfig = server->getServerConfig();
    EXPECT_EQ(updatedConfig.maxConcurrentClients, 10);
    EXPECT_TRUE(updatedConfig.enableDebugMode);
    
    server->stop();
}

// Test server info retrieval
TEST_F(FifoIntegrationTest, ServerInfoRetrieval) {
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    EXPECT_TRUE(server->start());
    
    json serverInfo = server->getServerInfo();
    EXPECT_FALSE(serverInfo.empty());
    
    EXPECT_TRUE(serverInfo.contains("serverName"));
    EXPECT_TRUE(serverInfo.contains("serverId"));
    EXPECT_TRUE(serverInfo.contains("status"));
    EXPECT_TRUE(serverInfo.contains("isRunning"));
    EXPECT_TRUE(serverInfo.contains("isHealthy"));
    EXPECT_TRUE(serverInfo.contains("connectedClients"));
    EXPECT_TRUE(serverInfo.contains("statistics"));
    EXPECT_TRUE(serverInfo.contains("config"));
    
    EXPECT_EQ(serverInfo["serverName"], serverConfig_.serverName);
    EXPECT_EQ(serverInfo["serverId"], serverConfig_.serverId);
    EXPECT_TRUE(serverInfo["isRunning"]);
    
    server->stop();
}

// Test error handling
TEST_F(FifoIntegrationTest, ErrorHandling) {
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    std::atomic<bool> errorOccurred{false};
    std::string errorMessage;
    
    server->setErrorCallback([&](const std::string& error, const std::string& clientId) {
        errorMessage = error;
        errorOccurred.store(true);
    });
    
    EXPECT_TRUE(server->start());
    
    // Try to send message to non-existent client (should trigger error)
    Message testMessage;
    testMessage.senderId = "server";
    testMessage.recipientId = "nonexistent";
    testMessage.payload = "test";
    
    EXPECT_FALSE(server->sendMessageToClient("nonexistent", testMessage));
    
    server->stop();
}

// Test server restart functionality
TEST_F(FifoIntegrationTest, ServerRestart) {
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    // Start server
    EXPECT_TRUE(server->start());
    EXPECT_TRUE(server->isRunning());
    EXPECT_EQ(server->getStatus(), ServerStatus::RUNNING);
    
    // Accept a client
    EXPECT_TRUE(server->acceptClient("client1", "test"));
    EXPECT_EQ(server->getConnectedClients().size(), 1);
    
    // Restart server
    EXPECT_TRUE(server->restart());
    EXPECT_TRUE(server->isRunning());
    EXPECT_EQ(server->getStatus(), ServerStatus::RUNNING);
    
    // Clients should be disconnected after restart
    EXPECT_EQ(server->getConnectedClients().size(), 0);
    
    server->stop();
}

// Test concurrent operations
TEST_F(FifoIntegrationTest, ConcurrentOperations) {
    serverConfig_.maxConcurrentClients = 10;
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    EXPECT_TRUE(server->start());
    
    std::atomic<int> successfulConnections{0};
    std::vector<std::thread> threads;
    
    // Create multiple threads trying to connect clients
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            std::string clientId = "client" + std::to_string(i);
            if (server->acceptClient(clientId, "test")) {
                successfulConnections.fetch_add(1);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Give time for connections to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify connections
    EXPECT_EQ(successfulConnections.load(), 5);
    EXPECT_EQ(server->getConnectedClients().size(), 5);
    
    server->stop();
}

// Performance test
TEST_F(FifoIntegrationTest, PerformanceTest) {
    serverConfig_.enablePerformanceMetrics = true;
    auto server = FifoServerFactory::createWithConfig(serverConfig_);
    ASSERT_NE(server, nullptr);
    
    EXPECT_TRUE(server->start());
    
    // Accept multiple clients quickly
    const int clientCount = 10;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < clientCount; ++i) {
        std::string clientId = "client" + std::to_string(i);
        server->acceptClient(clientId, "test");
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 2000); // Less than 2 seconds
    
    // Check final statistics
    auto stats = server->getStatistics();
    EXPECT_EQ(stats.totalClientsConnected.load(), clientCount);
    
    server->stop();
}
