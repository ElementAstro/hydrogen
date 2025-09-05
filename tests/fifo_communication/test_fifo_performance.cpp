#include <gtest/gtest.h>
#include <hydrogen/core/fifo_communicator.h>
#include <hydrogen/core/fifo_config_manager.h>
#include <hydrogen/core/fifo_logger.h>
#include <hydrogen/server/protocols/fifo/fifo_server.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <random>
#include <algorithm>

using namespace hydrogen::core;
using namespace hydrogen::server::protocols::fifo;
using json = nlohmann::json;

class FifoPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable logging for performance tests
        FifoLoggerConfig logConfig;
        logConfig.enableConsoleLogging = false;
        logConfig.enableFileLogging = false;
        logConfig.logLevel = FifoLogLevel::OFF;
        getGlobalFifoLogger().updateConfig(logConfig);
        
        // Create unique test identifier
        static int testCounter = 0;
        testId_ = std::to_string(++testCounter);
    }
    
    void TearDown() override {
        // Clean up any test artifacts
    }
    
    std::string testId_;
    
    // Helper function to create performance-optimized config
    FifoConfig createPerformanceConfig() {
        auto& configManager = getGlobalFifoConfigManager();
        FifoConfig config = configManager.createConfig(FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE);
        
#ifdef _WIN32
        config.windowsPipePath = "\\\\.\\pipe\\perf_test_" + testId_;
        config.pipeType = FifoPipeType::WINDOWS_NAMED_PIPE;
#else
        config.unixPipePath = "/tmp/perf_test_" + testId_;
        config.pipeType = FifoPipeType::UNIX_FIFO;
#endif
        
        config.pipeName = "perf_test_" + testId_;
        config.enablePerformanceMetrics = true;
        config.enableMessageLogging = false;
        config.enableMessageTracing = false;
        config.enableDebugLogging = false;
        
        return config;
    }
    
    // Helper function to generate test messages
    std::vector<std::string> generateTestMessages(int count, size_t messageSize) {
        std::vector<std::string> messages;
        messages.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis('A', 'Z');
        
        for (int i = 0; i < count; ++i) {
            std::string message;
            message.reserve(messageSize);
            
            for (size_t j = 0; j < messageSize; ++j) {
                message += static_cast<char>(dis(gen));
            }
            
            messages.push_back(message);
        }
        
        return messages;
    }
    
    // Helper function to measure execution time
    template<typename Func>
    std::chrono::milliseconds measureTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }
};

// Test communicator creation performance
TEST_F(FifoPerformanceTest, CommunicatorCreationPerformance) {
    const int iterations = 100;
    std::vector<std::unique_ptr<FifoCommunicator>> communicators;
    communicators.reserve(iterations);
    
    auto duration = measureTime([&]() {
        for (int i = 0; i < iterations; ++i) {
            FifoConfig config = createPerformanceConfig();
            config.pipeName = "perf_test_" + testId_ + "_" + std::to_string(i);
            
            auto communicator = FifoCommunicatorFactory::create(config);
            communicators.push_back(std::move(communicator));
        }
    });
    
    std::cout << "Created " << iterations << " communicators in " << duration.count() << " ms" << std::endl;
    std::cout << "Average creation time: " << (duration.count() / static_cast<double>(iterations)) << " ms" << std::endl;
    
    // Performance expectation: should create communicators quickly
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds for 100 communicators
    EXPECT_EQ(communicators.size(), iterations);
}

// Test message sending performance
TEST_F(FifoPerformanceTest, MessageSendingPerformance) {
    FifoConfig config = createPerformanceConfig();
    auto communicator = FifoCommunicatorFactory::create(config);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    const int messageCount = 1000;
    const size_t messageSize = 1024; // 1KB messages
    auto testMessages = generateTestMessages(messageCount, messageSize);
    
    std::atomic<int> messagesSent{0};
    
    auto duration = measureTime([&]() {
        for (const auto& message : testMessages) {
            if (communicator->sendMessage(message)) {
                messagesSent.fetch_add(1);
            }
        }
    });
    
    auto stats = communicator->getStatistics();
    
    std::cout << "Sent " << messagesSent.load() << "/" << messageCount << " messages in " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (messagesSent.load() * 1000.0 / duration.count()) << " messages/second" << std::endl;
    std::cout << "Data rate: " << (messagesSent.load() * messageSize * 1000.0 / duration.count() / 1024 / 1024) << " MB/second" << std::endl;
    
    // Performance expectations
    EXPECT_GT(messagesSent.load(), messageCount * 0.8); // At least 80% success rate
    EXPECT_LT(duration.count(), 10000); // Less than 10 seconds
    
    communicator->stop();
}

// Test different message sizes performance
TEST_F(FifoPerformanceTest, MessageSizePerformance) {
    FifoConfig config = createPerformanceConfig();
    auto communicator = FifoCommunicatorFactory::create(config);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    std::vector<size_t> messageSizes = {64, 256, 1024, 4096, 16384, 65536}; // 64B to 64KB
    const int messagesPerSize = 100;
    
    for (size_t messageSize : messageSizes) {
        auto testMessages = generateTestMessages(messagesPerSize, messageSize);
        std::atomic<int> messagesSent{0};
        
        auto duration = measureTime([&]() {
            for (const auto& message : testMessages) {
                if (communicator->sendMessage(message)) {
                    messagesSent.fetch_add(1);
                }
            }
        });
        
        double throughput = messagesSent.load() * 1000.0 / duration.count();
        double dataRate = messagesSent.load() * messageSize * 1000.0 / duration.count() / 1024 / 1024;
        
        std::cout << "Message size " << messageSize << " bytes: " 
                  << throughput << " msg/s, " 
                  << dataRate << " MB/s" << std::endl;
        
        // Basic performance check
        EXPECT_GT(messagesSent.load(), messagesPerSize * 0.5); // At least 50% success rate
    }
    
    communicator->stop();
}

// Test concurrent communicators performance
TEST_F(FifoPerformanceTest, ConcurrentCommunicatorsPerformance) {
    const int communicatorCount = 10;
    const int messagesPerCommunicator = 100;
    
    std::vector<std::unique_ptr<FifoCommunicator>> communicators;
    std::vector<std::thread> threads;
    std::atomic<int> totalMessagesSent{0};
    
    // Create communicators
    for (int i = 0; i < communicatorCount; ++i) {
        FifoConfig config = createPerformanceConfig();
        config.pipeName = "perf_test_" + testId_ + "_" + std::to_string(i);
        
        auto communicator = FifoCommunicatorFactory::create(config);
        ASSERT_NE(communicator, nullptr);
        EXPECT_TRUE(communicator->start());
        
        communicators.push_back(std::move(communicator));
    }
    
    auto duration = measureTime([&]() {
        // Start threads for concurrent message sending
        for (int i = 0; i < communicatorCount; ++i) {
            threads.emplace_back([&, i]() {
                auto testMessages = generateTestMessages(messagesPerCommunicator, 512);
                int messagesSent = 0;
                
                for (const auto& message : testMessages) {
                    if (communicators[i]->sendMessage(message)) {
                        messagesSent++;
                    }
                }
                
                totalMessagesSent.fetch_add(messagesSent);
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
    });
    
    std::cout << "Concurrent test: " << communicatorCount << " communicators, " 
              << messagesPerCommunicator << " messages each" << std::endl;
    std::cout << "Total messages sent: " << totalMessagesSent.load() 
              << "/" << (communicatorCount * messagesPerCommunicator) << std::endl;
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;
    std::cout << "Overall throughput: " << (totalMessagesSent.load() * 1000.0 / duration.count()) << " messages/second" << std::endl;
    
    // Performance expectations
    EXPECT_GT(totalMessagesSent.load(), communicatorCount * messagesPerCommunicator * 0.5);
    EXPECT_LT(duration.count(), 30000); // Less than 30 seconds
    
    // Stop all communicators
    for (auto& communicator : communicators) {
        communicator->stop();
    }
}

// Test server performance with multiple clients
TEST_F(FifoPerformanceTest, ServerMultiClientPerformance) {
    FifoServerConfig serverConfig = FifoServerFactory::createHighPerformanceConfig();
    serverConfig.serverName = "PerfTestServer_" + testId_;
    serverConfig.serverId = "perf_server_" + testId_;
    serverConfig.maxConcurrentClients = 50;
    serverConfig.enablePerformanceMetrics = true;
    
#ifdef _WIN32
    serverConfig.protocolConfig.windowsBasePipePath = "\\\\.\\pipe\\perf_server_" + testId_;
#else
    serverConfig.protocolConfig.basePipePath = "/tmp/perf_server_" + testId_;
#endif
    
    auto server = FifoServerFactory::createWithConfig(serverConfig);
    ASSERT_NE(server, nullptr);
    
    std::atomic<int> clientsConnected{0};
    std::atomic<int> messagesReceived{0};
    
    server->setClientConnectedCallback([&](const std::string& clientId) {
        clientsConnected.fetch_add(1);
    });
    
    server->setMessageReceivedCallback([&](const std::string& clientId, const Message& message) {
        messagesReceived.fetch_add(1);
    });
    
    EXPECT_TRUE(server->start());
    
    const int clientCount = 20;
    
    auto duration = measureTime([&]() {
        // Accept multiple clients quickly
        for (int i = 0; i < clientCount; ++i) {
            std::string clientId = "client" + std::to_string(i);
            server->acceptClient(clientId, "performance_test");
        }
        
        // Give time for connections to be established
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Send messages to all clients
        Message testMessage;
        testMessage.senderId = "server";
        testMessage.topic = "performance_test";
        testMessage.payload = "Performance test message";
        testMessage.sourceProtocol = CommunicationProtocol::FIFO;
        testMessage.targetProtocol = CommunicationProtocol::FIFO;
        
        for (int i = 0; i < clientCount; ++i) {
            std::string clientId = "client" + std::to_string(i);
            testMessage.recipientId = clientId;
            server->sendMessageToClient(clientId, testMessage);
        }
        
        // Test broadcast performance
        for (int i = 0; i < 10; ++i) {
            server->broadcastMessage(testMessage);
        }
    });
    
    auto stats = server->getStatistics();
    
    std::cout << "Server performance test completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Clients connected: " << clientsConnected.load() << "/" << clientCount << std::endl;
    std::cout << "Server statistics:" << std::endl;
    std::cout << "  Total clients connected: " << stats.totalClientsConnected.load() << std::endl;
    std::cout << "  Current active clients: " << stats.currentActiveClients.load() << std::endl;
    std::cout << "  Messages processed: " << stats.totalMessagesProcessed.load() << std::endl;
    std::cout << "  Bytes transferred: " << stats.totalBytesTransferred.load() << std::endl;
    std::cout << "  Uptime: " << stats.getUptime().count() << " ms" << std::endl;
    
    // Performance expectations
    EXPECT_GE(clientsConnected.load(), clientCount * 0.8); // At least 80% connection success
    EXPECT_LT(duration.count(), 10000); // Less than 10 seconds
    
    server->stop();
}

// Test memory usage during high load
TEST_F(FifoPerformanceTest, MemoryUsageTest) {
    FifoConfig config = createPerformanceConfig();
    config.maxQueueSize = 10000; // Large queue for testing
    
    auto communicator = FifoCommunicatorFactory::create(config);
    ASSERT_NE(communicator, nullptr);
    
    EXPECT_TRUE(communicator->start());
    
    const int messageCount = 5000;
    const size_t messageSize = 2048; // 2KB messages
    auto testMessages = generateTestMessages(messageCount, messageSize);
    
    // Send many messages quickly to test memory usage
    std::atomic<int> messagesSent{0};
    
    auto duration = measureTime([&]() {
        for (const auto& message : testMessages) {
            if (communicator->sendMessage(message)) {
                messagesSent.fetch_add(1);
            }
        }
        
        // Give time for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    });
    
    auto stats = communicator->getStatistics();
    
    std::cout << "Memory usage test: " << messageCount << " messages of " << messageSize << " bytes each" << std::endl;
    std::cout << "Messages sent: " << messagesSent.load() << std::endl;
    std::cout << "Total bytes: " << stats.bytesTransferred.load() << std::endl;
    std::cout << "Test duration: " << duration.count() << " ms" << std::endl;
    
    // Basic checks - mainly ensuring no crashes or excessive memory usage
    EXPECT_GT(messagesSent.load(), 0);
    EXPECT_LT(duration.count(), 60000); // Less than 1 minute
    
    communicator->stop();
}

// Test configuration impact on performance
TEST_F(FifoPerformanceTest, ConfigurationImpactTest) {
    const int messageCount = 500;
    const size_t messageSize = 1024;
    auto testMessages = generateTestMessages(messageCount, messageSize);
    
    struct ConfigTest {
        std::string name;
        FifoConfigManager::ConfigPreset preset;
    };
    
    std::vector<ConfigTest> configTests = {
        {"Default", FifoConfigManager::ConfigPreset::DEFAULT},
        {"High Performance", FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE},
        {"Low Latency", FifoConfigManager::ConfigPreset::LOW_LATENCY},
        {"Reliable", FifoConfigManager::ConfigPreset::RELIABLE}
    };
    
    for (const auto& configTest : configTests) {
        auto& configManager = getGlobalFifoConfigManager();
        FifoConfig config = configManager.createConfig(configTest.preset);
        
        config.pipeName = "perf_test_" + testId_ + "_" + configTest.name;
#ifdef _WIN32
        config.windowsPipePath = "\\\\.\\pipe\\" + config.pipeName;
#else
        config.unixPipePath = "/tmp/" + config.pipeName;
#endif
        
        auto communicator = FifoCommunicatorFactory::create(config);
        ASSERT_NE(communicator, nullptr);
        
        EXPECT_TRUE(communicator->start());
        
        std::atomic<int> messagesSent{0};
        
        auto duration = measureTime([&]() {
            for (const auto& message : testMessages) {
                if (communicator->sendMessage(message)) {
                    messagesSent.fetch_add(1);
                }
            }
        });
        
        double throughput = messagesSent.load() * 1000.0 / duration.count();
        
        std::cout << configTest.name << " config: " 
                  << messagesSent.load() << " messages in " << duration.count() << " ms, "
                  << throughput << " msg/s" << std::endl;
        
        communicator->stop();
        
        // Basic performance check
        EXPECT_GT(messagesSent.load(), 0);
        EXPECT_LT(duration.count(), 30000); // Less than 30 seconds
    }
}

// Stress test with rapid start/stop cycles
TEST_F(FifoPerformanceTest, StartStopStressTest) {
    const int cycles = 50;
    
    auto duration = measureTime([&]() {
        for (int i = 0; i < cycles; ++i) {
            FifoConfig config = createPerformanceConfig();
            config.pipeName = "stress_test_" + testId_ + "_" + std::to_string(i);
            
            auto communicator = FifoCommunicatorFactory::create(config);
            ASSERT_NE(communicator, nullptr);
            
            EXPECT_TRUE(communicator->start());
            
            // Send a few messages
            for (int j = 0; j < 5; ++j) {
                communicator->sendMessage("Test message " + std::to_string(j));
            }
            
            communicator->stop();
        }
    });
    
    std::cout << "Start/stop stress test: " << cycles << " cycles in " << duration.count() << " ms" << std::endl;
    std::cout << "Average cycle time: " << (duration.count() / static_cast<double>(cycles)) << " ms" << std::endl;
    
    // Performance expectation
    EXPECT_LT(duration.count(), 30000); // Less than 30 seconds for 50 cycles
}
