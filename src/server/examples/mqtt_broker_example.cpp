#include <astrocomm/server/mqtt_broker.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace astrocomm::server;
using namespace astrocomm::core;

int main() {
    std::cout << "MQTT Broker Example" << std::endl;
    
    MqttConfig config;
    config.brokerHost = "localhost";
    config.brokerPort = 1883;
    
    auto broker = MqttBrokerFactory::createBroker(config);
    
    broker->setMessageHandler([](const MqttMessage& message) {
        std::cout << "Received: " << message.payload << " on topic: " << message.topic << std::endl;
    });
    
    if (broker->start()) {
        std::cout << "MQTT Broker started on port 1883" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        broker->stop();
    }
    
    return 0;
}
