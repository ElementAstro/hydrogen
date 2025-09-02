#pragma once

#ifdef HYDROGEN_ENABLE_GRPC

#include "message.h"
#include "protocol_communicators.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

// Forward declarations for generated protobuf classes
namespace astrocomm {
namespace proto {
class Message;
class CommunicationService;
}
}

namespace hydrogen {
namespace core {

/**
 * @brief gRPC streaming client implementation
 * 
 * This class provides complete gRPC streaming functionality including
 * client streaming, server streaming, and bidirectional streaming.
 */
class GrpcStreamingClient {
public:
    using StreamMessageHandler = std::function<void(const Message&)>;
    using StreamErrorHandler = std::function<void(const std::string&)>;
    using StreamStatusHandler = std::function<void(bool connected)>;

    explicit GrpcStreamingClient(const GrpcConfig& config);
    ~GrpcStreamingClient();

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;

    // Unary RPC
    CommunicationResponse sendUnaryMessage(const Message& message);

    // Client streaming
    bool startClientStreaming(const std::string& method);
    bool sendClientStreamMessage(const Message& message);
    CommunicationResponse finishClientStreaming();

    // Server streaming
    bool startServerStreaming(const std::string& method, const Message& request);
    void stopServerStreaming();

    // Bidirectional streaming
    bool startBidirectionalStreaming(const std::string& method);
    bool sendBidirectionalMessage(const Message& message);
    void stopBidirectionalStreaming();

    // Event handlers
    void setMessageHandler(StreamMessageHandler handler) { messageHandler_ = handler; }
    void setErrorHandler(StreamErrorHandler handler) { errorHandler_ = handler; }
    void setStatusHandler(StreamStatusHandler handler) { statusHandler_ = handler; }

    // Configuration
    const GrpcConfig& getConfig() const { return config_; }
    void updateConfig(const GrpcConfig& config);

    // Statistics
    struct StreamingStats {
        size_t messagesSent = 0;
        size_t messagesReceived = 0;
        size_t streamingErrors = 0;
        size_t activeStreams = 0;
        std::chrono::steady_clock::time_point lastActivity;
    };
    
    StreamingStats getStatistics() const;
    void resetStatistics();

private:
    GrpcConfig config_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<astrocomm::proto::CommunicationService::Stub> stub_;
    
    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> shutdown_{false};
    
    // Streaming state
    std::atomic<bool> clientStreaming_{false};
    std::atomic<bool> serverStreaming_{false};
    std::atomic<bool> bidirectionalStreaming_{false};
    
    // Stream objects
    std::unique_ptr<grpc::ClientWriter<astrocomm::proto::Message>> clientStream_;
    std::unique_ptr<grpc::ClientReader<astrocomm::proto::Message>> serverStream_;
    std::unique_ptr<grpc::ClientReaderWriter<astrocomm::proto::Message, astrocomm::proto::Message>> bidirectionalStream_;
    
    // Stream contexts
    std::unique_ptr<grpc::ClientContext> clientStreamContext_;
    std::unique_ptr<grpc::ClientContext> serverStreamContext_;
    std::unique_ptr<grpc::ClientContext> bidirectionalStreamContext_;
    
    // Threading
    std::thread serverStreamThread_;
    std::thread bidirectionalStreamThread_;
    
    // Message queue for client streaming
    std::queue<json> clientStreamQueue_;
    std::mutex clientStreamMutex_;
    std::condition_variable clientStreamCV_;
    
    // Event handlers
    StreamMessageHandler messageHandler_;
    StreamErrorHandler errorHandler_;
    StreamStatusHandler statusHandler_;
    
    // Statistics
    mutable std::mutex statsMutex_;
    StreamingStats stats_;
    
    // Internal methods
    bool initializeChannel();
    void shutdownStreams();
    
    // Stream processing threads
    void serverStreamingLoop();
    void bidirectionalStreamingLoop();
    void processClientStreamQueue();
    
    // Message conversion
    json convertToProtoMessage(const Message& message) const;
    std::unique_ptr<Message> convertFromProtoMessage(const json& protoMessage) const;
    
    // Error handling
    void handleStreamError(const std::string& error, const std::string& context);
    void updateConnectionStatus(bool connected);
    
    // Statistics helpers
    void incrementMessagesSent();
    void incrementMessagesReceived();
    void incrementStreamingErrors();
    void updateLastActivity();
};

/**
 * @brief gRPC streaming server implementation
 * 
 * This class provides server-side gRPC streaming functionality.
 */
class GrpcStreamingServer {
public:
    using StreamRequestHandler = std::function<void(const Message&, grpc::ServerWriter<astrocomm::proto::Message>*)>;
    using ClientStreamHandler = std::function<CommunicationResponse(grpc::ServerReader<astrocomm::proto::Message>*)>;
    using BidirectionalStreamHandler = std::function<void(grpc::ServerReaderWriter<astrocomm::proto::Message, astrocomm::proto::Message>*)>;

    explicit GrpcStreamingServer(const GrpcConfig& config);
    ~GrpcStreamingServer();

    // Server management
    bool start();
    void stop();
    bool isRunning() const;

    // Stream handlers
    void setServerStreamHandler(StreamRequestHandler handler) { serverStreamHandler_ = handler; }
    void setClientStreamHandler(ClientStreamHandler handler) { clientStreamHandler_ = handler; }
    void setBidirectionalStreamHandler(BidirectionalStreamHandler handler) { bidirectionalStreamHandler_ = handler; }

    // Configuration
    const GrpcConfig& getConfig() const { return config_; }

private:
    GrpcConfig config_;
    std::unique_ptr<grpc::Server> server_;
    std::atomic<bool> running_{false};
    
    // Stream handlers
    StreamRequestHandler serverStreamHandler_;
    ClientStreamHandler clientStreamHandler_;
    BidirectionalStreamHandler bidirectionalStreamHandler_;
    
    // Internal methods
    bool initializeServer();
    void registerServices(grpc::ServerBuilder& builder);
};

/**
 * @brief Factory for creating gRPC streaming components
 */
class GrpcStreamingFactory {
public:
    static std::unique_ptr<GrpcStreamingClient> createStreamingClient(const GrpcConfig& config);
    static std::unique_ptr<GrpcStreamingServer> createStreamingServer(const GrpcConfig& config);
    
    // Utility methods
    static bool isGrpcAvailable();
    static std::string getGrpcVersion();
};

} // namespace core
} // namespace hydrogen

#endif // HYDROGEN_ENABLE_GRPC
