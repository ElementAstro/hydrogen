#include <astrocomm/client.h>

namespace astrocomm {
namespace client {

void initialize() {
    // Initialize the core component first
    astrocomm::core::initialize();
    
    // Initialize any client-specific global state
    // This could include setting up networking, initializing SSL contexts, etc.
}

void cleanup() {
    // Clean up client-specific resources
    
    // Clean up the core component
    astrocomm::core::cleanup();
}

std::string getVersion() {
    return "1.0.0";
}

std::unique_ptr<DeviceClient> createDeviceClient() {
    auto client = std::make_unique<DeviceClient>();
    
    // Set up default connection callback
    client->setConnectionCallback([](bool connected) {
        if (connected) {
            // Connection established
        } else {
            // Connection lost
        }
    });
    
    // Enable auto-reconnect by default
    client->setAutoReconnect(true, 5000);
    
    return client;
}

std::unique_ptr<DeviceClient> createAndConnect(const std::string& host, uint16_t port) {
    auto client = createDeviceClient();
    
    if (client->connect(host, port)) {
        return client;
    }
    
    return nullptr;
}

} // namespace client
} // namespace astrocomm
