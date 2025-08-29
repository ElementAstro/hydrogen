#include <astrocomm/server.h>

namespace astrocomm {
namespace server {

void initialize() {
    // Initialize the core component first
    astrocomm::core::initialize();
    
    // Initialize any server-specific global state
    // This could include setting up logging, initializing web server components, etc.
}

void cleanup() {
    // Clean up server-specific resources
    
    // Clean up the core component
    astrocomm::core::cleanup();
}

std::string getVersion() {
    return "1.0.0";
}

std::unique_ptr<DeviceServer> createDeviceServer(int port, const std::string& persistenceDir) {
    auto server = std::make_unique<DeviceServer>(port, persistenceDir);
    
    // Set up default message handlers
    server->setMessageHandler(MessageType::COMMAND, 
        [](std::shared_ptr<Message> msg, crow::websocket::connection& conn) {
            // Default command handler
            auto response = std::make_shared<ResponseMessage>();
            response->setOriginalMessageId(msg->getMessageId());
            response->setStatus("OK");
            conn.send_text(response->toJson().dump());
        });
    
    server->setMessageHandler(MessageType::REGISTRATION,
        [&server](std::shared_ptr<Message> msg, crow::websocket::connection& conn) {
            // Default registration handler
            auto regMsg = std::dynamic_pointer_cast<RegistrationMessage>(msg);
            if (regMsg) {
                auto deviceInfo = regMsg->getDeviceInfo();
                std::string deviceId = deviceInfo.value("id", "");
                
                if (!deviceId.empty()) {
                    server->getDeviceManager().registerDevice(deviceId, deviceInfo);
                    server->getDeviceManager().setDeviceConnectionStatus(deviceId, true);
                }
            }
            
            auto response = std::make_shared<ResponseMessage>();
            response->setOriginalMessageId(msg->getMessageId());
            response->setStatus("REGISTERED");
            conn.send_text(response->toJson().dump());
        });
    
    return server;
}

} // namespace server
} // namespace astrocomm
