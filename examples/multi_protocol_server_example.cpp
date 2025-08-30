#include <astrocomm/server/enhanced_device_server.h>
#include <astrocomm/core/enhanced_device_manager.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

using namespace astrocomm::server;
using namespace astrocomm::core;

// Global server instance for signal handling
std::unique_ptr<EnhancedDeviceServer> g_server;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main() {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=== AstroComm Multi-Protocol Communication Server ===" << std::endl;
    std::cout << "Starting enhanced device server with multiple protocols..." << std::endl;

    try {
        // Create server configuration
        json serverConfig = {
            {"server", {
                {"name", "AstroComm Multi-Protocol Server"},
                {"version", "1.0.0"},
                {"description", "Enhanced device communication server"},
                {"metrics_enabled", true},
                {"monitoring_enabled", true}
            }},
            {"logging", {
                {"level", "info"},
                {"file", "astrocomm_server.log"}
            }}
        };

        // Create enhanced device server
        g_server = std::make_unique<EnhancedDeviceServer>(serverConfig);

        // Configure MQTT protocol
        json mqttConfig = {
            {"brokerHost", "localhost"},
            {"brokerPort", 1883},
            {"useTls", false},
            {"keepAliveInterval", 60},
            {"qosLevel", 1},
            {"topicPrefix", "astrocomm"}
        };
        
        if (g_server->enableProtocol(CommunicationProtocol::MQTT, mqttConfig)) {
            std::cout << "✓ MQTT protocol enabled on port 1883" << std::endl;
        }

        // Configure gRPC protocol
        json grpcConfig = {
            {"serverAddress", "localhost:50051"},
            {"useTls", false},
            {"maxReceiveMessageSize", 4194304},
            {"maxSendMessageSize", 4194304},
            {"enableReflection", true}
        };
        
        if (g_server->enableProtocol(CommunicationProtocol::GRPC, grpcConfig)) {
            std::cout << "✓ gRPC protocol enabled on port 50051" << std::endl;
        }

        // Configure ZeroMQ protocol
        json zmqConfig = {
            {"bindAddress", "tcp://*:5555"},
            {"socketType", 1}, // REP socket
            {"highWaterMark", 1000},
            {"lingerTime", 1000}
        };
        
        if (g_server->enableProtocol(CommunicationProtocol::ZEROMQ, zmqConfig)) {
            std::cout << "✓ ZeroMQ protocol enabled on port 5555" << std::endl;
        }

        // Configure WebSocket protocol
        json wsConfig = {
            {"port", 8080},
            {"path", "/ws"},
            {"enableCompression", true}
        };
        
        if (g_server->enableProtocol(CommunicationProtocol::WEBSOCKET, wsConfig)) {
            std::cout << "✓ WebSocket protocol enabled on port 8080" << std::endl;
        }

        // Set up event handlers
        g_server->setConnectionHandler([](const std::string& clientId, CommunicationProtocol protocol, bool connected) {
            std::cout << "[CONNECTION] Client '" << clientId << "' " 
                      << (connected ? "connected" : "disconnected") 
                      << " via " << ProtocolServerUtils::protocolToString(protocol) << std::endl;
        });

        g_server->setMessageHandler([](const std::string& clientId, CommunicationProtocol protocol, const std::string& message) {
            std::cout << "[MESSAGE] From '" << clientId << "' via " 
                      << ProtocolServerUtils::protocolToString(protocol) 
                      << ": " << message.substr(0, 100) << (message.length() > 100 ? "..." : "") << std::endl;
        });

        g_server->setProtocolStatusHandler([](CommunicationProtocol protocol, bool running) {
            std::cout << "[PROTOCOL] " << ProtocolServerUtils::protocolToString(protocol) 
                      << " is now " << (running ? "running" : "stopped") << std::endl;
        });

        // Enable protocol bridging (MQTT <-> WebSocket)
        g_server->enableProtocolBridging(CommunicationProtocol::MQTT, CommunicationProtocol::WEBSOCKET);
        g_server->enableProtocolBridging(CommunicationProtocol::WEBSOCKET, CommunicationProtocol::MQTT);
        std::cout << "✓ Protocol bridging enabled (MQTT ↔ WebSocket)" << std::endl;

        // Enable metrics collection and monitoring
        g_server->enableMetricsCollection(true);
        g_server->enableRealTimeMonitoring(std::chrono::milliseconds(5000));
        std::cout << "✓ Metrics collection and monitoring enabled" << std::endl;

        // Start the server
        if (!g_server->start()) {
            std::cerr << "Failed to start server!" << std::endl;
            return 1;
        }

        std::cout << "\n🚀 Server started successfully!" << std::endl;
        std::cout << "Enabled protocols: ";
        for (const auto& protocol : g_server->getEnabledProtocols()) {
            std::cout << ProtocolServerUtils::protocolToString(protocol) << " ";
        }
        std::cout << std::endl;

        // Set up enhanced device manager with health monitoring
        auto deviceManager = g_server->getEnhancedDeviceManager();
        if (deviceManager) {
            deviceManager->startHealthMonitoring();
            deviceManager->startDeviceDiscovery();
            std::cout << "✓ Device health monitoring and discovery started" << std::endl;

            // Create some example device groups and templates
            std::string sensorGroupId = deviceManager->createDeviceGroup("Sensors", "All sensor devices");
            std::string actuatorGroupId = deviceManager->createDeviceGroup("Actuators", "All actuator devices");

            json sensorTemplate = {
                {"sampling_rate", 1000},
                {"precision", "high"},
                {"enabled", true},
                {"data_format", "json"}
            };
            std::string sensorTemplateId = deviceManager->createConfigTemplate("Standard Sensor", "sensor", sensorTemplate);

            std::cout << "✓ Created device groups and configuration templates" << std::endl;
        }

        // Print server information
        std::cout << "\n📊 Server Information:" << std::endl;
        json serverMetrics = g_server->getServerMetrics();
        std::cout << "  - Running: " << (serverMetrics["running"].get<bool>() ? "Yes" : "No") << std::endl;
        std::cout << "  - Uptime: " << serverMetrics["uptime_seconds"] << " seconds" << std::endl;
        std::cout << "  - Connected clients: " << serverMetrics["connected_clients"] << std::endl;

        std::cout << "\n🔗 Connection Endpoints:" << std::endl;
        std::cout << "  - MQTT: mqtt://localhost:1883" << std::endl;
        std::cout << "  - gRPC: localhost:50051" << std::endl;
        std::cout << "  - ZeroMQ: tcp://localhost:5555" << std::endl;
        std::cout << "  - WebSocket: ws://localhost:8080/ws" << std::endl;
        std::cout << "  - HTTP: http://localhost:8080" << std::endl;

        std::cout << "\n📝 Example Usage:" << std::endl;
        std::cout << "  MQTT Publish: mosquitto_pub -h localhost -t 'astrocomm/test' -m 'Hello World'" << std::endl;
        std::cout << "  MQTT Subscribe: mosquitto_sub -h localhost -t 'astrocomm/+'" << std::endl;
        std::cout << "  WebSocket: Connect to ws://localhost:8080/ws" << std::endl;

        std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;

        // Main server loop with periodic status updates
        auto lastStatusUpdate = std::chrono::steady_clock::now();
        while (g_server->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Print status update every 30 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusUpdate).count() >= 30) {
                json currentMetrics = g_server->getServerMetrics();
                std::cout << "\n📈 Status Update:" << std::endl;
                std::cout << "  - Uptime: " << currentMetrics["uptime_seconds"] << "s" << std::endl;
                std::cout << "  - Total connections: " << currentMetrics["total_connections"] << std::endl;
                std::cout << "  - Messages received: " << currentMetrics["total_messages_received"] << std::endl;
                std::cout << "  - Messages sent: " << currentMetrics["total_messages_sent"] << std::endl;
                std::cout << "  - Connected clients: " << currentMetrics["connected_clients"] << std::endl;

                // Show protocol-specific metrics
                for (const auto& protocol : g_server->getEnabledProtocols()) {
                    json protocolMetrics = g_server->getProtocolMetrics(protocol);
                    std::cout << "  - " << ProtocolServerUtils::protocolToString(protocol) 
                              << " connections: " << protocolMetrics["connections"] << std::endl;
                }

                lastStatusUpdate = now;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Server error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n✅ Server stopped gracefully." << std::endl;
    return 0;
}
