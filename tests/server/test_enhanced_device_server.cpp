#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <astrocomm/server/enhanced_device_server.h>
#include <astrocomm/core/protocol_communicators.h>
#include <thread>
#include <chrono>

using namespace astrocomm::server;
using namespace astrocomm::core;
using namespace testing;

class EnhancedDeviceServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        json config = {
            {"server", {
                {"name", "Test Server"},
                {"version", "1.0.0"},
                {"metrics_enabled", true}
            }}
        };
        
        server_ = std::make_unique<EnhancedDeviceServer>(config);
    }

    void TearDown() override {
        if (server_ && server_->isRunning()) {
            server_->stop();
        }
    }

    std::unique_ptr<EnhancedDeviceServer> server_;
};

TEST_F(EnhancedDeviceServerTest, ServerInitialization) {
    EXPECT_FALSE(server_->isRunning());
    EXPECT_TRUE(server_->getEnabledProtocols().empty());
}

TEST_F(EnhancedDeviceServerTest, ProtocolEnableDisable) {
    // Test enabling MQTT protocol
    json mqttConfig = {
        {"brokerHost", "localhost"},
        {"brokerPort", 1883},
        {"useTls", false}
    };
    
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::MQTT, mqttConfig));
    EXPECT_TRUE(server_->isProtocolEnabled(CommunicationProtocol::MQTT));
    
    auto enabledProtocols = server_->getEnabledProtocols();
    EXPECT_EQ(enabledProtocols.size(), 1);
    EXPECT_EQ(enabledProtocols[0], CommunicationProtocol::MQTT);
    
    // Test disabling protocol
    EXPECT_TRUE(server_->disableProtocol(CommunicationProtocol::MQTT));
    EXPECT_FALSE(server_->isProtocolEnabled(CommunicationProtocol::MQTT));
    EXPECT_TRUE(server_->getEnabledProtocols().empty());
}

TEST_F(EnhancedDeviceServerTest, MultipleProtocolsEnabled) {
    // Enable multiple protocols
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::MQTT));
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::GRPC));
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::ZEROMQ));
    
    auto enabledProtocols = server_->getEnabledProtocols();
    EXPECT_EQ(enabledProtocols.size(), 3);
    
    EXPECT_TRUE(server_->isProtocolEnabled(CommunicationProtocol::MQTT));
    EXPECT_TRUE(server_->isProtocolEnabled(CommunicationProtocol::GRPC));
    EXPECT_TRUE(server_->isProtocolEnabled(CommunicationProtocol::ZEROMQ));
}

TEST_F(EnhancedDeviceServerTest, ServerStartStop) {
    // Enable a protocol first
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::MQTT));
    
    // Start server
    EXPECT_TRUE(server_->start(false)); // Don't load previous config
    EXPECT_TRUE(server_->isRunning());
    
    // Stop server
    server_->stop();
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(EnhancedDeviceServerTest, MqttBrokerSetup) {
    MqttConfig config;
    config.brokerHost = "localhost";
    config.brokerPort = 1883;
    config.useTls = false;
    
    EXPECT_TRUE(server_->setupMqttBroker(config));
    EXPECT_NE(server_->getMqttBroker(), nullptr);
}

TEST_F(EnhancedDeviceServerTest, GrpcServerSetup) {
    GrpcConfig config;
    config.serverAddress = "localhost:50051";
    config.useTls = false;
    config.enableReflection = true;
    
    EXPECT_TRUE(server_->setupGrpcServer(config));
    EXPECT_NE(server_->getGrpcServer(), nullptr);
}

TEST_F(EnhancedDeviceServerTest, ZmqServerSetup) {
    ZmqConfig config;
    config.bindAddress = "tcp://*:5555";
    config.socketType = static_cast<int>(ZmqCommunicator::SocketType::REP);
    
    EXPECT_TRUE(server_->setupZmqServer(config));
    EXPECT_NE(server_->getZmqServer(), nullptr);
}

TEST_F(EnhancedDeviceServerTest, ProtocolConfiguration) {
    json mqttConfig = {
        {"brokerHost", "test.mosquitto.org"},
        {"brokerPort", 1883},
        {"clientId", "test_client"}
    };
    
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::MQTT, mqttConfig));
    
    json retrievedConfig = server_->getProtocolConfig(CommunicationProtocol::MQTT);
    EXPECT_EQ(retrievedConfig["brokerHost"], "test.mosquitto.org");
    EXPECT_EQ(retrievedConfig["brokerPort"], 1883);
    EXPECT_EQ(retrievedConfig["clientId"], "test_client");
}

TEST_F(EnhancedDeviceServerTest, MetricsCollection) {
    EXPECT_TRUE(server_->enableMetricsCollection(true));
    EXPECT_TRUE(server_->isMetricsCollectionEnabled());
    
    json metrics = server_->getServerMetrics();
    EXPECT_TRUE(metrics.contains("uptime_seconds"));
    EXPECT_TRUE(metrics.contains("running"));
    EXPECT_TRUE(metrics.contains("total_connections"));
    EXPECT_TRUE(metrics.contains("enabled_protocols"));
}

TEST_F(EnhancedDeviceServerTest, ConfigurationSaveLoad) {
    // Enable some protocols
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::MQTT));
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::GRPC));
    
    // Save configuration
    std::string testConfigFile = "test_server_config.json";
    EXPECT_TRUE(server_->saveConfiguration(testConfigFile));
    
    // Create new server and load configuration
    auto newServer = std::make_unique<EnhancedDeviceServer>();
    EXPECT_TRUE(newServer->loadConfiguration(testConfigFile));
    
    // Verify protocols are enabled
    EXPECT_TRUE(newServer->isProtocolEnabled(CommunicationProtocol::MQTT));
    EXPECT_TRUE(newServer->isProtocolEnabled(CommunicationProtocol::GRPC));
    
    // Cleanup
    std::remove(testConfigFile.c_str());
}

TEST_F(EnhancedDeviceServerTest, ProtocolBridging) {
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::MQTT));
    EXPECT_TRUE(server_->enableProtocol(CommunicationProtocol::WEBSOCKET));
    
    // Enable bridging from MQTT to WebSocket
    EXPECT_TRUE(server_->enableProtocolBridging(CommunicationProtocol::MQTT, CommunicationProtocol::WEBSOCKET));
    
    // This would be tested with actual message flow in integration tests
}

class MockConnectionHandler {
public:
    MOCK_METHOD(void, onConnection, (const std::string& clientId, CommunicationProtocol protocol, bool connected));
};

class MockMessageHandler {
public:
    MOCK_METHOD(void, onMessage, (const std::string& clientId, CommunicationProtocol protocol, const std::string& message));
};

TEST_F(EnhancedDeviceServerTest, EventHandlers) {
    MockConnectionHandler connectionHandler;
    MockMessageHandler messageHandler;
    
    server_->setConnectionHandler([&connectionHandler](const std::string& clientId, CommunicationProtocol protocol, bool connected) {
        connectionHandler.onConnection(clientId, protocol, connected);
    });
    
    server_->setMessageHandler([&messageHandler](const std::string& clientId, CommunicationProtocol protocol, const std::string& message) {
        messageHandler.onMessage(clientId, protocol, message);
    });
    
    // Event handlers are set (actual testing would require triggering events)
    SUCCEED();
}

// Factory tests
class EnhancedDeviceServerFactoryTest : public ::testing::Test {};

TEST_F(EnhancedDeviceServerFactoryTest, CreateServer) {
    json config = {{"test", "value"}};
    auto server = EnhancedDeviceServerFactory::createServer(config);
    
    EXPECT_NE(server, nullptr);
    EXPECT_FALSE(server->isRunning());
}

TEST_F(EnhancedDeviceServerFactoryTest, CreateMultiProtocolServer) {
    std::vector<CommunicationProtocol> protocols = {
        CommunicationProtocol::MQTT,
        CommunicationProtocol::GRPC,
        CommunicationProtocol::WEBSOCKET
    };
    
    auto server = EnhancedDeviceServerFactory::createMultiProtocolServer(protocols);
    
    EXPECT_NE(server, nullptr);
    EXPECT_EQ(server->getEnabledProtocols().size(), 3);
    EXPECT_TRUE(server->isProtocolEnabled(CommunicationProtocol::MQTT));
    EXPECT_TRUE(server->isProtocolEnabled(CommunicationProtocol::GRPC));
    EXPECT_TRUE(server->isProtocolEnabled(CommunicationProtocol::WEBSOCKET));
}

TEST_F(EnhancedDeviceServerFactoryTest, CreateServerWithDefaults) {
    auto server = EnhancedDeviceServerFactory::createServerWithDefaults();
    
    EXPECT_NE(server, nullptr);
    EXPECT_FALSE(server->getEnabledProtocols().empty());
    EXPECT_TRUE(server->isMetricsCollectionEnabled());
    EXPECT_TRUE(server->isRealTimeMonitoringEnabled());
}

// Protocol utilities tests
class ProtocolServerUtilsTest : public ::testing::Test {};

TEST_F(ProtocolServerUtilsTest, ProtocolStringConversion) {
    EXPECT_EQ(ProtocolServerUtils::protocolToString(CommunicationProtocol::MQTT), "mqtt");
    EXPECT_EQ(ProtocolServerUtils::protocolToString(CommunicationProtocol::GRPC), "grpc");
    EXPECT_EQ(ProtocolServerUtils::protocolToString(CommunicationProtocol::ZEROMQ), "zeromq");
    EXPECT_EQ(ProtocolServerUtils::protocolToString(CommunicationProtocol::WEBSOCKET), "websocket");
    
    EXPECT_EQ(ProtocolServerUtils::stringToProtocol("mqtt"), CommunicationProtocol::MQTT);
    EXPECT_EQ(ProtocolServerUtils::stringToProtocol("grpc"), CommunicationProtocol::GRPC);
    EXPECT_EQ(ProtocolServerUtils::stringToProtocol("zeromq"), CommunicationProtocol::ZEROMQ);
    EXPECT_EQ(ProtocolServerUtils::stringToProtocol("websocket"), CommunicationProtocol::WEBSOCKET);
}

TEST_F(ProtocolServerUtilsTest, SupportedProtocols) {
    auto supportedProtocols = ProtocolServerUtils::getAllSupportedProtocols();
    EXPECT_FALSE(supportedProtocols.empty());
    
    EXPECT_TRUE(ProtocolServerUtils::isProtocolSupported(CommunicationProtocol::MQTT));
    EXPECT_TRUE(ProtocolServerUtils::isProtocolSupported(CommunicationProtocol::GRPC));
    EXPECT_TRUE(ProtocolServerUtils::isProtocolSupported(CommunicationProtocol::ZEROMQ));
    EXPECT_TRUE(ProtocolServerUtils::isProtocolSupported(CommunicationProtocol::WEBSOCKET));
}

TEST_F(ProtocolServerUtilsTest, DefaultConfigurations) {
    json mqttConfig = ProtocolServerUtils::getDefaultProtocolConfig(CommunicationProtocol::MQTT);
    EXPECT_TRUE(mqttConfig.contains("brokerHost"));
    EXPECT_TRUE(mqttConfig.contains("brokerPort"));
    
    json grpcConfig = ProtocolServerUtils::getDefaultProtocolConfig(CommunicationProtocol::GRPC);
    EXPECT_TRUE(grpcConfig.contains("serverAddress"));
    
    json zmqConfig = ProtocolServerUtils::getDefaultProtocolConfig(CommunicationProtocol::ZEROMQ);
    EXPECT_TRUE(zmqConfig.contains("bindAddress"));
}

TEST_F(ProtocolServerUtilsTest, ConfigurationValidation) {
    json validMqttConfig = {
        {"brokerHost", "localhost"},
        {"brokerPort", 1883}
    };
    EXPECT_TRUE(ProtocolServerUtils::validateProtocolConfig(CommunicationProtocol::MQTT, validMqttConfig));
    
    json invalidMqttConfig = {
        {"invalidField", "value"}
    };
    EXPECT_FALSE(ProtocolServerUtils::validateProtocolConfig(CommunicationProtocol::MQTT, invalidMqttConfig));
    
    json validGrpcConfig = {
        {"serverAddress", "localhost:50051"}
    };
    EXPECT_TRUE(ProtocolServerUtils::validateProtocolConfig(CommunicationProtocol::GRPC, validGrpcConfig));
}

// Performance and stress tests
class EnhancedDeviceServerPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_ = EnhancedDeviceServerFactory::createServerWithDefaults();
    }

    void TearDown() override {
        if (server_ && server_->isRunning()) {
            server_->stop();
        }
    }

    std::unique_ptr<EnhancedDeviceServer> server_;
};

TEST_F(EnhancedDeviceServerPerformanceTest, DISABLED_MultipleProtocolStartup) {
    // This test is disabled by default as it may take time
    std::vector<CommunicationProtocol> protocols = {
        CommunicationProtocol::MQTT,
        CommunicationProtocol::GRPC,
        CommunicationProtocol::ZEROMQ,
        CommunicationProtocol::WEBSOCKET,
        CommunicationProtocol::HTTP
    };
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (const auto& protocol : protocols) {
        EXPECT_TRUE(server_->enableProtocol(protocol));
    }
    
    EXPECT_TRUE(server_->start(false));
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_LT(duration.count(), 5000); // Should start within 5 seconds
    EXPECT_TRUE(server_->isRunning());
}

TEST_F(EnhancedDeviceServerPerformanceTest, DISABLED_MetricsCollectionOverhead) {
    // Test metrics collection performance impact
    server_->enableMetricsCollection(true);
    EXPECT_TRUE(server_->start(false));
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Collect metrics multiple times
    for (int i = 0; i < 1000; ++i) {
        auto metrics = server_->getAllMetrics();
        EXPECT_FALSE(metrics.empty());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
}
