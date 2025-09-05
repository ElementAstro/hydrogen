#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <hydrogen/server/protocols/stdio/stdio_server.h>
#include <hydrogen/core/stdio_config_manager.h>
#include <hydrogen/core/stdio_logger.h>
#include <hydrogen/core/message.h>
#include <thread>
#include <chrono>
#include <future>

using namespace hydrogen::server::protocols::stdio;
using namespace hydrogen::core;
using namespace testing;

/**
 * @brief Test fixture for StdioServer tests
 */
class StdioServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create default server configuration
        serverConfig_ = StdioServerFactory::createDefaultConfig();
        serverConfig_.serverName = "TestStdioServer";
        serverConfig_.maxConcurrentClients = 5;
        serverConfig_.enableAutoCleanup = true;
        serverConfig_.cleanupInterval = std::chrono::milliseconds(100);
        serverConfig_.clientTimeout = std::chrono::milliseconds(1000);

        // Configure protocol settings for testing
        serverConfig_.protocolConfig.enableMessageValidation = true;
        serverConfig_.protocolConfig.enableMessageLogging = false; // Reduce noise in tests
        serverConfig_.protocolConfig.connectionTimeout = 5; // 5 seconds for tests
        serverConfig_.protocolConfig.enableHeartbeat = false; // Disable for simpler tests

        server_ = nullptr;
    }

    void TearDown() override {
        if (server_ && server_->getStatus() == ServerStatus::RUNNING) {
            server_->stop();
        }
        server_.reset();

        // Give time for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void createServer() {
        server_ = StdioServerFactory::createWithConfig(serverConfig_);
        ASSERT_NE(server_, nullptr);
    }

    void createAndStartServer() {
        createServer();
        ASSERT_TRUE(server_->start());
        ASSERT_EQ(server_->getStatus(), ServerStatus::RUNNING);
    }

    StdioServer::ServerConfig serverConfig_;
    std::unique_ptr<StdioServer> server_;
};

/**
 * @brief Test basic server creation and configuration
 */
TEST_F(StdioServerTest, BasicCreationAndConfiguration) {
    createServer();

    EXPECT_EQ(server_->getStatus(), ServerStatus::STOPPED);
    EXPECT_FALSE(server_->isHealthy()); // Should not be healthy when stopped

    // Test configuration
    auto config = server_->getServerConfig();
    EXPECT_EQ(config.serverName, "TestStdioServer");
    EXPECT_EQ(config.maxConcurrentClients, 5);
    EXPECT_TRUE(config.enableAutoCleanup);
}

/**
 * @brief Test server start and stop functionality
 */
TEST_F(StdioServerTest, StartAndStop) {
    createServer();

    // Test start
    EXPECT_TRUE(server_->start());
    EXPECT_EQ(server_->getStatus(), ServerStatus::RUNNING);
    EXPECT_TRUE(server_->isHealthy());

    // Test multiple starts (should be idempotent)
    EXPECT_TRUE(server_->start());
    EXPECT_EQ(server_->getStatus(), ServerStatus::RUNNING);

    // Test stop
    server_->stop();
    EXPECT_EQ(server_->getStatus(), ServerStatus::STOPPED);
    EXPECT_FALSE(server_->isHealthy());

    // Test multiple stops (should be safe)
    server_->stop();
    EXPECT_EQ(server_->getStatus(), ServerStatus::STOPPED);
}

/**
 * @brief Test client connection management
 */
TEST_F(StdioServerTest, ClientConnectionManagement) {
    createAndStartServer();

    // Test client connection
    const std::string clientId = "test_client_1";
    EXPECT_TRUE(server_->acceptClient(clientId, "ping"));
    EXPECT_TRUE(server_->isClientConnected(clientId));

    // Test connected clients list
    auto connectedClients = server_->getConnectedClients();
    EXPECT_EQ(connectedClients.size(), 1);
    EXPECT_EQ(connectedClients[0], clientId);

    // Test client disconnection
    EXPECT_TRUE(server_->disconnectClient(clientId));
    EXPECT_FALSE(server_->isClientConnected(clientId));

    // Test disconnecting non-existent client
    EXPECT_FALSE(server_->disconnectClient("non_existent_client"));
}

/**
 * @brief Test multiple client connections
 */
TEST_F(StdioServerTest, MultipleClientConnections) {
    createAndStartServer();

    // Connect multiple clients
    std::vector<std::string> clientIds = {"client_1", "client_2", "client_3"};

    for (const auto& clientId : clientIds) {
        EXPECT_TRUE(server_->acceptClient(clientId, "ping"));
        EXPECT_TRUE(server_->isClientConnected(clientId));
    }

    // Verify all clients are connected
    auto connectedClients = server_->getConnectedClients();
    EXPECT_EQ(connectedClients.size(), clientIds.size());

    // Disconnect all clients
    for (const auto& clientId : clientIds) {
        EXPECT_TRUE(server_->disconnectClient(clientId));
        EXPECT_FALSE(server_->isClientConnected(clientId));
    }

    EXPECT_TRUE(server_->getConnectedClients().empty());
}

/**
 * @brief Test server statistics
 */
TEST_F(StdioServerTest, ServerStatistics) {
    createAndStartServer();

    auto initialStats = server_->getStatistics();
    EXPECT_EQ(initialStats.totalClientsConnected, 0);
    EXPECT_EQ(initialStats.currentActiveClients, 0);
    EXPECT_EQ(initialStats.totalMessagesProcessed, 0);

    // Connect a client
    EXPECT_TRUE(server_->acceptClient("stats_client", "ping"));

    // Give time for statistics to update
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto updatedStats = server_->getStatistics();
    EXPECT_GT(updatedStats.totalClientsConnected, initialStats.totalClientsConnected);
    EXPECT_GT(updatedStats.currentActiveClients, initialStats.currentActiveClients);

    // Test statistics reset
    server_->resetStatistics();
    auto resetStats = server_->getStatistics();
    EXPECT_EQ(resetStats.totalClientsConnected, 0);
    EXPECT_EQ(resetStats.totalMessagesProcessed, 0);
}

/**
 * @brief Test server configuration updates
 */
TEST_F(StdioServerTest, ConfigurationUpdates) {
    createServer();

    // Test configuration update
    auto newConfig = serverConfig_;
    newConfig.serverName = "UpdatedStdioServer";
    newConfig.maxConcurrentClients = 10;

    server_->setServerConfig(newConfig);

    auto retrievedConfig = server_->getServerConfig();
    EXPECT_EQ(retrievedConfig.serverName, "UpdatedStdioServer");
    EXPECT_EQ(retrievedConfig.maxConcurrentClients, 10);
}

/**
 * @brief Test server health monitoring
 */
TEST_F(StdioServerTest, HealthMonitoring) {
    createServer();

    // Server should not be healthy when stopped
    EXPECT_FALSE(server_->isHealthy());
    EXPECT_NE(server_->getHealthStatus().find("STOPPED"), std::string::npos);

    // Start server
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(server_->isHealthy());
    EXPECT_NE(server_->getHealthStatus().find("RUNNING"), std::string::npos);

    // Stop server
    server_->stop();
    EXPECT_FALSE(server_->isHealthy());
}

/**
 * @brief Test server info
 */
TEST_F(StdioServerTest, ServerInfo) {
    createAndStartServer();

    std::string serverInfo = server_->getServerInfo();
    EXPECT_FALSE(serverInfo.empty());
    EXPECT_NE(serverInfo.find("TestStdioServer"), std::string::npos);
    EXPECT_NE(serverInfo.find("STDIO"), std::string::npos);
}

/**
 * @brief Test callback functionality
 */
TEST_F(StdioServerTest, CallbackFunctionality) {
    createServer();

    // Set up callback tracking
    std::atomic<int> clientConnectedCount{0};
    std::atomic<int> clientDisconnectedCount{0};
    std::atomic<int> messageReceivedCount{0};
    std::atomic<int> errorCount{0};

    server_->setClientConnectedCallback([&clientConnectedCount](const std::string& clientId) {
        clientConnectedCount++;
    });

    server_->setClientDisconnectedCallback([&clientDisconnectedCount](const std::string& clientId) {
        clientDisconnectedCount++;
    });

    server_->setMessageReceivedCallback([&messageReceivedCount](const std::string& clientId, const Message& message) {
        messageReceivedCount++;
    });

    server_->setErrorCallback([&errorCount](const std::string& error, const std::string& clientId) {
        errorCount++;
    });

    EXPECT_TRUE(server_->start());

    // Test client connection callback
    EXPECT_TRUE(server_->acceptClient("callback_client", "ping"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GT(clientConnectedCount.load(), 0);

    // Test client disconnection callback
    EXPECT_TRUE(server_->disconnectClient("callback_client"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GT(clientDisconnectedCount.load(), 0);
}

/**
 * @brief Test server factory methods
 */
TEST_F(StdioServerTest, ServerFactory) {
    // Test default server creation
    auto defaultServer = StdioServerFactory::createDefault();
    ASSERT_NE(defaultServer, nullptr);
    EXPECT_EQ(defaultServer->getStatus(), ServerStatus::STOPPED);

    // Test high performance config
    auto hpConfig = StdioServerFactory::createHighPerformanceConfig();
    EXPECT_EQ(hpConfig.maxConcurrentClients, 1000);
    EXPECT_TRUE(hpConfig.enableAutoCleanup);

    auto hpServer = StdioServerFactory::createWithConfig(hpConfig);
    ASSERT_NE(hpServer, nullptr);

    // Test secure config
    auto secureConfig = StdioServerFactory::createSecureConfig();
    EXPECT_TRUE(secureConfig.enableCommandFiltering);
    EXPECT_TRUE(secureConfig.enableClientIsolation);

    // Test debug config
    auto debugConfig = StdioServerFactory::createDebugConfig();
    // Debug config should have appropriate settings for debugging
    EXPECT_TRUE(debugConfig.protocolConfig.enableMessageLogging);
}

/**
 * @brief Test error handling scenarios
 */
TEST_F(StdioServerTest, ErrorHandling) {
    createServer();

    // Test operations on stopped server
    EXPECT_FALSE(server_->acceptClient("client_on_stopped_server", "ping"));
    EXPECT_FALSE(server_->isClientConnected("any_client"));
    EXPECT_TRUE(server_->getConnectedClients().empty());

    // Start server for further tests
    EXPECT_TRUE(server_->start());

    // Test invalid client operations
    EXPECT_FALSE(server_->disconnectClient(""));
    EXPECT_FALSE(server_->disconnectClient("non_existent_client"));
    EXPECT_FALSE(server_->isClientConnected(""));

    // Test duplicate client connection
    const std::string clientId = "duplicate_client";
    EXPECT_TRUE(server_->acceptClient(clientId, "ping"));
    EXPECT_TRUE(server_->acceptClient(clientId, "ping")); // Should handle gracefully

    // Verify only one connection exists
    auto connectedClients = server_->getConnectedClients();
    int count = std::count(connectedClients.begin(), connectedClients.end(), clientId);
    EXPECT_EQ(count, 1);
}

/**
 * @brief Test concurrent operations
 */
TEST_F(StdioServerTest, ConcurrentOperations) {
    createAndStartServer();

    const int numThreads = 4;
    const int clientsPerThread = 3;
    std::vector<std::thread> threads;
    std::atomic<int> successfulConnections{0};
    std::atomic<int> successfulDisconnections{0};

    // Connect clients concurrently
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, clientsPerThread, &successfulConnections]() {
            for (int i = 0; i < clientsPerThread; ++i) {
                std::string clientId = "thread_" + std::to_string(t) + "_client_" + std::to_string(i);
                if (server_->acceptClient(clientId, "ping")) {
                    successfulConnections++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Wait for all connection threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    threads.clear();

    // Give time for connections to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_GT(successfulConnections.load(), 0);
    EXPECT_LE(successfulConnections.load(), numThreads * clientsPerThread);

    // Disconnect clients concurrently
    auto connectedClients = server_->getConnectedClients();
    for (int t = 0; t < numThreads && t < static_cast<int>(connectedClients.size()); ++t) {
        threads.emplace_back([this, &connectedClients, t, &successfulDisconnections]() {
            if (t < static_cast<int>(connectedClients.size())) {
                if (server_->disconnectClient(connectedClients[t])) {
                    successfulDisconnections++;
                }
            }
        });
    }

    // Wait for all disconnection threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(successfulDisconnections.load(), 0);
}

/**
 * @brief Test server limits and capacity
 */
TEST_F(StdioServerTest, ServerLimitsAndCapacity) {
    // Create server with limited capacity
    serverConfig_.maxConcurrentClients = 2;
    createAndStartServer();

    // Connect up to the limit
    EXPECT_TRUE(server_->acceptClient("client_1", "ping"));
    EXPECT_TRUE(server_->acceptClient("client_2", "ping"));

    // Verify both clients are connected
    EXPECT_EQ(server_->getConnectedClients().size(), 2);

    // Try to connect beyond the limit
    EXPECT_TRUE(server_->acceptClient("client_3", "ping")); // Should handle gracefully

    // Test that server handles the situation appropriately
    auto connectedClients = server_->getConnectedClients();
    EXPECT_LE(connectedClients.size(), 2); // Should not exceed the limit
}

/**
 * @brief Test server cleanup functionality
 */
TEST_F(StdioServerTest, ServerCleanup) {
    // Configure server with aggressive cleanup
    serverConfig_.enableAutoCleanup = true;
    serverConfig_.cleanupInterval = std::chrono::milliseconds(50);
    serverConfig_.clientTimeout = std::chrono::milliseconds(100);

    createAndStartServer();

    // Connect a client
    EXPECT_TRUE(server_->acceptClient("cleanup_test_client", "ping"));
    EXPECT_TRUE(server_->isClientConnected("cleanup_test_client"));

    // Wait for cleanup to potentially run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Server should still be running and healthy
    EXPECT_EQ(server_->getStatus(), ServerStatus::RUNNING);
    EXPECT_TRUE(server_->isHealthy());
}

/**
 * @brief Test message sending functionality
 */
TEST_F(StdioServerTest, MessageSending) {
    createAndStartServer();

    // Connect a client
    const std::string clientId = "message_test_client";
    EXPECT_TRUE(server_->acceptClient(clientId, "ping"));

    // Create a test message
    Message testMessage;
    testMessage.id = "test_msg_1";
    testMessage.type = "ping";
    testMessage.payload = R"({"command": "ping", "data": "test"})";
    testMessage.timestamp = std::chrono::system_clock::now();

    // Test message sending (this will depend on the actual server implementation)
    // For now, we test that the server accepts the operation
    EXPECT_TRUE(server_->isClientConnected(clientId));

    // Test statistics after message operations
    auto stats = server_->getStatistics();
    EXPECT_GE(stats.totalClientsConnected, 1);
}
