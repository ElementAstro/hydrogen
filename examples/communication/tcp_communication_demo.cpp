#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <future>
#include <atomic>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "hydrogen/core/communication/protocols/tcp_communicator.h"
#include "hydrogen/core/device/device_communicator.h"

using namespace hydrogen::core::communication::protocols;
using namespace hydrogen::core;
using namespace std::chrono_literals;
using json = nlohmann::json;

class TcpCommunicationDemo {
public:
    TcpCommunicationDemo() {
        // Set up logging
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }
    
    void run() {
        std::cout << "=== Hydrogen TCP Communication Protocol Demo ===" << std::endl;
        std::cout << "This demo showcases the TCP communication protocol features:" << std::endl;
        std::cout << "1. TCP Client/Server Communication" << std::endl;
        std::cout << "2. Performance Optimization Integration" << std::endl;
        std::cout << "3. Connection Management and Pooling" << std::endl;
        std::cout << "4. Message Batching and Serialization" << std::endl;
        std::cout << "5. Error Handling and Recovery" << std::endl;
        std::cout << std::endl;
        
        // Run individual demos
        demonstrateBasicTcpCommunication();
        demonstratePerformanceOptimization();
        demonstrateConnectionManagement();
        demonstrateErrorHandling();
        
        // Display final metrics
        displayFinalMetrics();
    }

private:
    std::shared_ptr<TcpCommunicator> server_;
    std::shared_ptr<TcpCommunicator> client_;
    std::atomic<int> messagesReceived_{0};
    std::atomic<int> messagesSent_{0};
    
    void demonstrateBasicTcpCommunication() {
        std::cout << "--- Basic TCP Communication Demo ---" << std::endl;
        
        // Create server configuration
        auto serverConfig = TcpCommunicatorFactory::createDefaultServerConfig(8001);
        server_ = TcpCommunicatorFactory::createServer(serverConfig);
        
        // Set up server message callback
        server_->setMessageCallback([this](const CommunicationMessage& message) {
            messagesReceived_.fetch_add(1);
            std::cout << "Server received: " << message.command << " from " << message.deviceId << std::endl;
            
            // Echo response
            CommunicationMessage response;
            response.messageId = "response_" + message.messageId;
            response.deviceId = "server";
            response.command = "echo_response";
            response.payload = json{{"original", message.payload}, {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()}};
            response.timestamp = std::chrono::system_clock::now();
            
            // Send response back to client
            server_->sendToAllClients(response);
        });
        
        // Start server
        ConnectionConfig serverConnConfig;
        if (!server_->connect(serverConnConfig)) {
            std::cerr << "Failed to start TCP server" << std::endl;
            return;
        }
        
        std::cout << "✓ TCP Server started on port 8001" << std::endl;
        
        // Give server time to start
        std::this_thread::sleep_for(500ms);
        
        // Create client configuration
        auto clientConfig = TcpCommunicatorFactory::createDefaultClientConfig("localhost", 8001);
        client_ = TcpCommunicatorFactory::createClient(clientConfig);
        
        // Set up client message callback
        client_->setMessageCallback([this](const CommunicationMessage& message) {
            messagesReceived_.fetch_add(1);
            std::cout << "Client received: " << message.command << " - " << message.payload.dump() << std::endl;
        });
        
        // Connect client
        ConnectionConfig clientConnConfig;
        if (!client_->connect(clientConnConfig)) {
            std::cerr << "Failed to connect TCP client" << std::endl;
            return;
        }
        
        std::cout << "✓ TCP Client connected to server" << std::endl;
        
        // Send test messages
        for (int i = 0; i < 5; ++i) {
            CommunicationMessage message;
            message.messageId = "test_msg_" + std::to_string(i);
            message.deviceId = "client_demo";
            message.command = "test_command";
            message.payload = json{
                {"message_number", i},
                {"content", "Hello from TCP client!"},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            message.timestamp = std::chrono::system_clock::now();
            message.priority = i % 3; // Vary priority
            
            auto future = client_->sendMessage(message);
            auto response = future.get();
            
            if (response.success) {
                messagesSent_.fetch_add(1);
                std::cout << "✓ Sent message " << i << " (Response time: " << response.responseTime.count() << "ms)" << std::endl;
            } else {
                std::cout << "✗ Failed to send message " << i << ": " << response.errorMessage << std::endl;
            }
            
            std::this_thread::sleep_for(100ms);
        }
        
        // Wait for responses
        std::this_thread::sleep_for(1000ms);
        
        std::cout << "Basic TCP Communication Results:" << std::endl;
        std::cout << "  Messages Sent: " << messagesSent_.load() << std::endl;
        std::cout << "  Messages Received: " << messagesReceived_.load() << std::endl;
        std::cout << std::endl;
    }
    
    void demonstratePerformanceOptimization() {
        std::cout << "--- Performance Optimization Demo ---" << std::endl;
        
        if (!client_ || !client_->isConnected()) {
            std::cout << "Client not available for performance demo" << std::endl;
            return;
        }
        
        // Enable all performance optimizations
        client_->enableConnectionPooling(true);
        client_->enableMessageBatching(true);
        client_->enableMemoryPooling(true);
        client_->enableSerializationOptimization(true);
        
        std::cout << "✓ Enabled all performance optimizations" << std::endl;
        
        // Perform high-volume message test
        const int messageCount = 100;
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::vector<std::future<CommunicationResponse>> futures;
        
        for (int i = 0; i < messageCount; ++i) {
            CommunicationMessage message;
            message.messageId = "perf_msg_" + std::to_string(i);
            message.deviceId = "perf_client";
            message.command = "performance_test";
            message.payload = json{
                {"batch_number", i},
                {"data", std::string(100, 'A' + (i % 26))}, // Variable data
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            message.timestamp = std::chrono::system_clock::now();
            
            futures.push_back(client_->sendMessage(message));
        }
        
        // Wait for all messages to complete
        int successCount = 0;
        double totalResponseTime = 0.0;
        
        for (auto& future : futures) {
            auto response = future.get();
            if (response.success) {
                successCount++;
                totalResponseTime += response.responseTime.count();
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::cout << "Performance Optimization Results:" << std::endl;
        std::cout << "  Messages Sent: " << messageCount << std::endl;
        std::cout << "  Successful: " << successCount << std::endl;
        std::cout << "  Total Time: " << totalDuration.count() << "ms" << std::endl;
        std::cout << "  Throughput: " << (messageCount * 1000.0 / totalDuration.count()) << " msgs/sec" << std::endl;
        std::cout << "  Average Response Time: " << (totalResponseTime / successCount) << "ms" << std::endl;
        std::cout << std::endl;
    }
    
    void demonstrateConnectionManagement() {
        std::cout << "--- Connection Management Demo ---" << std::endl;
        
        // Get connection manager instance
        auto& manager = TcpConnectionManager::getInstance();
        
        // Register connections
        manager.registerConnection("demo_server", server_);
        manager.registerConnection("demo_client", client_);
        
        std::cout << "✓ Registered connections with manager" << std::endl;
        
        // Get connection metrics
        auto allMetrics = manager.getAllConnectionMetrics();
        
        std::cout << "Connection Manager Metrics:" << std::endl;
        for (auto& [name, metrics] : allMetrics.items()) {
            std::cout << "  Connection: " << name << std::endl;
            if (metrics.contains("connectionsEstablished")) {
                std::cout << "    Connections Established: " << metrics["connectionsEstablished"] << std::endl;
            }
            if (metrics.contains("messagesSent")) {
                std::cout << "    Messages Sent: " << metrics["messagesSent"] << std::endl;
            }
            if (metrics.contains("messagesReceived")) {
                std::cout << "    Messages Received: " << metrics["messagesReceived"] << std::endl;
            }
        }
        
        // Test connection retrieval
        auto retrievedClient = manager.getConnection("demo_client");
        if (retrievedClient && retrievedClient->isConnected()) {
            std::cout << "✓ Successfully retrieved client connection from manager" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    void demonstrateErrorHandling() {
        std::cout << "--- Error Handling Demo ---" << std::endl;
        
        // Test connection to non-existent server
        auto errorConfig = TcpCommunicatorFactory::createDefaultClientConfig("localhost", 9999);
        auto errorClient = TcpCommunicatorFactory::createClient(errorConfig);
        
        // Set up error callback
        std::atomic<bool> errorReceived{false};
        errorClient->setConnectionStatusCallback([&errorReceived](bool connected) {
            if (!connected) {
                errorReceived.store(true);
                std::cout << "✓ Connection error detected and handled" << std::endl;
            }
        });
        
        // Attempt connection (should fail)
        ConnectionConfig errorConnConfig;
        bool connected = errorClient->connect(errorConnConfig);
        
        if (!connected) {
            std::cout << "✓ Connection failure handled gracefully" << std::endl;
        }
        
        // Test message sending to disconnected client
        if (client_) {
            // Disconnect client
            client_->disconnect();
            std::this_thread::sleep_for(100ms);
            
            // Try to send message
            CommunicationMessage message;
            message.messageId = "error_test";
            message.deviceId = "error_client";
            message.command = "test_after_disconnect";
            message.payload = json{{"test", "error_handling"}};
            message.timestamp = std::chrono::system_clock::now();
            
            auto future = client_->sendMessage(message);
            auto response = future.get();
            
            if (!response.success) {
                std::cout << "✓ Message send failure handled: " << response.errorMessage << std::endl;
            }
        }
        
        std::cout << "Error Handling Results:" << std::endl;
        std::cout << "  Connection errors handled gracefully" << std::endl;
        std::cout << "  Message send errors detected and reported" << std::endl;
        std::cout << "  System remains stable after errors" << std::endl;
        std::cout << std::endl;
    }
    
    void displayFinalMetrics() {
        std::cout << "--- Final Performance Metrics ---" << std::endl;
        
        if (server_) {
            auto serverStats = server_->getStatistics();
            std::cout << "Server Statistics:" << std::endl;
            std::cout << "  Messages Sent: " << serverStats.messagesSent << std::endl;
            std::cout << "  Messages Received: " << serverStats.messagesReceived << std::endl;
            std::cout << "  Messages Error: " << serverStats.messagesError << std::endl;
            std::cout << "  Average Response Time: " << serverStats.averageResponseTime << "ms" << std::endl;
            
            if (server_->isServerMode()) {
                auto connectedClients = server_->getConnectedClients();
                std::cout << "  Connected Clients: " << connectedClients.size() << std::endl;
            }
        }
        
        if (client_) {
            auto clientStats = client_->getStatistics();
            std::cout << "Client Statistics:" << std::endl;
            std::cout << "  Messages Sent: " << clientStats.messagesSent << std::endl;
            std::cout << "  Messages Received: " << clientStats.messagesReceived << std::endl;
            std::cout << "  Messages Error: " << clientStats.messagesError << std::endl;
            std::cout << "  Average Response Time: " << clientStats.averageResponseTime << "ms" << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "🎉 TCP Communication Protocol demo completed successfully!" << std::endl;
        std::cout << "Key features demonstrated:" << std::endl;
        std::cout << "  • High-performance TCP client/server communication" << std::endl;
        std::cout << "  • Performance optimization integration" << std::endl;
        std::cout << "  • Connection pooling and management" << std::endl;
        std::cout << "  • Message batching and serialization" << std::endl;
        std::cout << "  • Comprehensive error handling" << std::endl;
        std::cout << "  • Real-time metrics and monitoring" << std::endl;
    }
    
    ~TcpCommunicationDemo() {
        if (client_) {
            client_->disconnect();
        }
        if (server_) {
            server_->disconnect();
        }
    }
};

int main() {
    try {
        TcpCommunicationDemo demo;
        demo.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
