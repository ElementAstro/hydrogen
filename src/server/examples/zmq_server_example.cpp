#include <hydrogen/server/zmq_server.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace hydrogen::server;
using namespace hydrogen::core;

int main() {
    std::cout << "ZeroMQ Server Example" << std::endl;
    
    ZmqConfig config;
    config.bindAddress = "tcp://*:5555";
    config.socketType = static_cast<int>(ZmqCommunicator::SocketType::REP);
    
    auto server = ZmqServerFactory::createServer(config);
    
    if (server->start()) {
        std::cout << "ZeroMQ Server started on port 5555" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        server->stop();
    }
    
    return 0;
}
