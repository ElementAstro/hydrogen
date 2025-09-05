#include "hydrogen/core/grpc_streaming_client.h"

#ifdef HYDROGEN_ENABLE_GRPC
#include "hydrogen/core/message_transformer.h"
#include <chrono>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>

// Include generated protobuf headers
// Note: These would be generated from the proto files
// #include "communication.grpc.pb.h"

namespace hydrogen {
namespace core {

// GrpcStreamingClient implementation
GrpcStreamingClient::GrpcStreamingClient(const GrpcConfig &config)
    : config_(config) {
  stats_.lastActivity = std::chrono::steady_clock::now();
}

GrpcStreamingClient::~GrpcStreamingClient() { disconnect(); }

bool GrpcStreamingClient::connect() {
  if (connected_.load()) {
    spdlog::warn("GrpcStreamingClient: Already connected");
    return true;
  }

  try {
    if (!initializeChannel()) {
      spdlog::error("GrpcStreamingClient: Failed to initialize channel");
      return false;
    }

    // Create stub
    // stub_ = astrocomm::proto::CommunicationService::NewStub(channel_);

    // Test connection with a simple call
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(5));

    // For now, simulate successful connection
    // In real implementation, this would make a health check call
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    connected_.store(true);
    updateConnectionStatus(true);

    spdlog::info("GrpcStreamingClient: Connected to {}", config_.serverAddress);
    return true;

  } catch (const std::exception &e) {
    spdlog::error("GrpcStreamingClient: Connection failed: {}", e.what());
    handleStreamError(e.what(), "connection");
    return false;
  }
}

void GrpcStreamingClient::disconnect() {
  if (!connected_.load()) {
    return;
  }

  spdlog::info("GrpcStreamingClient: Disconnecting from {}",
               config_.serverAddress);

  shutdown_.store(true);
  shutdownStreams();

  connected_.store(false);
  updateConnectionStatus(false);

  spdlog::info("GrpcStreamingClient: Disconnected");
}

bool GrpcStreamingClient::isConnected() const { return connected_.load(); }

CommunicationResponse
GrpcStreamingClient::sendUnaryMessage(const Message &message) {
  CommunicationResponse response;

  if (!connected_.load()) {
    response.success = false;
    response.errorMessage = "Not connected to gRPC server";
    return response;
  }

  try {
    auto protoMessage = convertToProtoMessage(message);

    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(30)); // Default timeout

    // astrocomm::proto::Message protoResponse;
    // grpc::Status status = stub_->SendMessage(&context, protoMessage,
    // &protoResponse);

    // For now, simulate successful response
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    grpc::Status status = grpc::Status::OK;

    if (status.ok()) {
      response.success = true;
      response.messageId = message.getMessageId();
      // response.payload = convertFromProtoMessage(protoResponse)->toJson();
      response.payload = json{{"status", "success"}};
      incrementMessagesSent();
      updateLastActivity();
    } else {
      response.success = false;
      response.errorMessage = status.error_message();
      incrementStreamingErrors();
    }

  } catch (const std::exception &e) {
    response.success = false;
    response.errorMessage = "gRPC unary call failed: " + std::string(e.what());
    handleStreamError(e.what(), "unary_call");
  }

  return response;
}

bool GrpcStreamingClient::startClientStreaming(const std::string &method) {
  if (!connected_.load()) {
    spdlog::error(
        "GrpcStreamingClient: Cannot start client streaming - not connected");
    return false;
  }

  if (clientStreaming_.load()) {
    spdlog::warn("GrpcStreamingClient: Client streaming already active");
    return true;
  }

  try {
    clientStreamContext_ = std::make_unique<grpc::ClientContext>();
    clientStreamContext_->set_deadline(std::chrono::system_clock::now() +
                                       std::chrono::hours(1));

    // astrocomm::proto::Message response;
    // clientStream_ = stub_->ClientStreamingMethod(clientStreamContext_.get(),
    // &response);

    if (clientStream_) {
      clientStreaming_.store(true);
      stats_.activeStreams++;

      // Start processing thread for client stream queue
      std::thread(&GrpcStreamingClient::processClientStreamQueue, this)
          .detach();

      spdlog::info(
          "GrpcStreamingClient: Started client streaming for method: {}",
          method);
      return true;
    } else {
      spdlog::error("GrpcStreamingClient: Failed to create client stream");
      return false;
    }

  } catch (const std::exception &e) {
    spdlog::error("GrpcStreamingClient: Failed to start client streaming: {}",
                  e.what());
    handleStreamError(e.what(), "client_streaming_start");
    return false;
  }
}

bool GrpcStreamingClient::sendClientStreamMessage(const Message &message) {
  if (!clientStreaming_.load()) {
    spdlog::error("GrpcStreamingClient: No active client stream");
    return false;
  }

  try {
    auto protoMessage = convertToProtoMessage(message);

    {
      std::lock_guard<std::mutex> lock(clientStreamMutex_);
      clientStreamQueue_.push(protoMessage);
    }

    clientStreamCV_.notify_one();
    incrementMessagesSent();
    updateLastActivity();

    return true;

  } catch (const std::exception &e) {
    spdlog::error(
        "GrpcStreamingClient: Failed to queue client stream message: {}",
        e.what());
    handleStreamError(e.what(), "client_stream_send");
    return false;
  }
}

CommunicationResponse GrpcStreamingClient::finishClientStreaming() {
  CommunicationResponse response;

  if (!clientStreaming_.load()) {
    response.success = false;
    response.errorMessage = "No active client stream";
    return response;
  }

  try {
    // Signal end of client streaming
    clientStreaming_.store(false);
    clientStreamCV_.notify_all();

    if (clientStream_) {
      // clientStream_->WritesDone();
      // grpc::Status status = clientStream_->Finish();
      grpc::Status status = grpc::Status::OK; // Simulate success

      if (status.ok()) {
        response.success = true;
        response.messageId = "client_stream_finished";
        response.payload = json{{"status", "completed"}};
      } else {
        response.success = false;
        response.errorMessage = status.error_message();
        incrementStreamingErrors();
      }
    }

    stats_.activeStreams--;
    clientStream_.reset();
    clientStreamContext_.reset();

    spdlog::info("GrpcStreamingClient: Finished client streaming");

  } catch (const std::exception &e) {
    response.success = false;
    response.errorMessage =
        "Failed to finish client streaming: " + std::string(e.what());
    handleStreamError(e.what(), "client_streaming_finish");
  }

  return response;
}

bool GrpcStreamingClient::startServerStreaming(const std::string &method,
                                               const Message &request) {
  if (!connected_.load()) {
    spdlog::error(
        "GrpcStreamingClient: Cannot start server streaming - not connected");
    return false;
  }

  if (serverStreaming_.load()) {
    spdlog::warn("GrpcStreamingClient: Server streaming already active");
    return true;
  }

  try {
    serverStreamContext_ = std::make_unique<grpc::ClientContext>();
    serverStreamContext_->set_deadline(std::chrono::system_clock::now() +
                                       std::chrono::hours(1));

    auto protoRequest = convertToProtoMessage(request);
    // serverStream_ = stub_->ServerStreamingMethod(serverStreamContext_.get(),
    // protoRequest);

    if (serverStream_) {
      serverStreaming_.store(true);
      stats_.activeStreams++;

      // Start server streaming thread
      serverStreamThread_ =
          std::thread(&GrpcStreamingClient::serverStreamingLoop, this);

      spdlog::info(
          "GrpcStreamingClient: Started server streaming for method: {}",
          method);
      return true;
    } else {
      spdlog::error("GrpcStreamingClient: Failed to create server stream");
      return false;
    }

  } catch (const std::exception &e) {
    spdlog::error("GrpcStreamingClient: Failed to start server streaming: {}",
                  e.what());
    handleStreamError(e.what(), "server_streaming_start");
    return false;
  }
}

void GrpcStreamingClient::stopServerStreaming() {
  if (!serverStreaming_.load()) {
    return;
  }

  spdlog::info("GrpcStreamingClient: Stopping server streaming");

  serverStreaming_.store(false);

  if (serverStreamThread_.joinable()) {
    serverStreamThread_.join();
  }

  stats_.activeStreams--;
  serverStream_.reset();
  serverStreamContext_.reset();

  spdlog::info("GrpcStreamingClient: Server streaming stopped");
}

bool GrpcStreamingClient::startBidirectionalStreaming(
    const std::string &method) {
  if (!connected_.load()) {
    spdlog::error("GrpcStreamingClient: Cannot start bidirectional streaming - "
                  "not connected");
    return false;
  }

  if (bidirectionalStreaming_.load()) {
    spdlog::warn("GrpcStreamingClient: Bidirectional streaming already active");
    return true;
  }

  try {
    bidirectionalStreamContext_ = std::make_unique<grpc::ClientContext>();
    bidirectionalStreamContext_->set_deadline(std::chrono::system_clock::now() +
                                              std::chrono::hours(1));

    // bidirectionalStream_ =
    // stub_->BidirectionalStreamingMethod(bidirectionalStreamContext_.get());

    if (bidirectionalStream_) {
      bidirectionalStreaming_.store(true);
      stats_.activeStreams++;

      // Start bidirectional streaming thread
      bidirectionalStreamThread_ =
          std::thread(&GrpcStreamingClient::bidirectionalStreamingLoop, this);

      spdlog::info(
          "GrpcStreamingClient: Started bidirectional streaming for method: {}",
          method);
      return true;
    } else {
      spdlog::error(
          "GrpcStreamingClient: Failed to create bidirectional stream");
      return false;
    }

  } catch (const std::exception &e) {
    spdlog::error(
        "GrpcStreamingClient: Failed to start bidirectional streaming: {}",
        e.what());
    handleStreamError(e.what(), "bidirectional_streaming_start");
    return false;
  }
}

bool GrpcStreamingClient::sendBidirectionalMessage(const Message &message) {
  if (!bidirectionalStreaming_.load()) {
    spdlog::error("GrpcStreamingClient: No active bidirectional stream");
    return false;
  }

  try {
    auto protoMessage = convertToProtoMessage(message);

    // bool success = bidirectionalStream_->Write(protoMessage);
    bool success = true; // Simulate success

    if (success) {
      incrementMessagesSent();
      updateLastActivity();
      return true;
    } else {
      spdlog::error(
          "GrpcStreamingClient: Failed to write to bidirectional stream");
      incrementStreamingErrors();
      return false;
    }

  } catch (const std::exception &e) {
    spdlog::error(
        "GrpcStreamingClient: Failed to send bidirectional message: {}",
        e.what());
    handleStreamError(e.what(), "bidirectional_send");
    return false;
  }
}

void GrpcStreamingClient::stopBidirectionalStreaming() {
  if (!bidirectionalStreaming_.load()) {
    return;
  }

  spdlog::info("GrpcStreamingClient: Stopping bidirectional streaming");

  bidirectionalStreaming_.store(false);

  if (bidirectionalStreamThread_.joinable()) {
    bidirectionalStreamThread_.join();
  }

  stats_.activeStreams--;
  bidirectionalStream_.reset();
  bidirectionalStreamContext_.reset();

  spdlog::info("GrpcStreamingClient: Bidirectional streaming stopped");
}

void GrpcStreamingClient::updateConfig(const GrpcConfig &config) {
  config_ = config;
  if (connected_.load()) {
    spdlog::info("GrpcStreamingClient: Configuration updated, reconnection may "
                 "be required");
  }
}

GrpcStreamingClient::StreamingStats GrpcStreamingClient::getStatistics() const {
  std::lock_guard<std::mutex> lock(statsMutex_);
  return stats_;
}

void GrpcStreamingClient::resetStatistics() {
  std::lock_guard<std::mutex> lock(statsMutex_);
  stats_ = StreamingStats{};
  stats_.lastActivity = std::chrono::steady_clock::now();
}

bool GrpcStreamingClient::initializeChannel() {
  try {
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(config_.maxReceiveMessageSize);
    args.SetMaxSendMessageSize(config_.maxSendMessageSize);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 30000);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);
    args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);

    if (config_.useTls) {
      auto creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
      channel_ = grpc::CreateCustomChannel(config_.serverAddress, creds, args);
    } else {
      channel_ = grpc::CreateCustomChannel(
          config_.serverAddress, grpc::InsecureChannelCredentials(), args);
    }

    if (!channel_) {
      spdlog::error("GrpcStreamingClient: Failed to create gRPC channel");
      return false;
    }

    // Wait for channel to be ready
    auto deadline = std::chrono::system_clock::now() +
                    std::chrono::seconds(30); // Default timeout
    if (!channel_->WaitForConnected(deadline)) {
      spdlog::error("GrpcStreamingClient: Channel connection timeout");
      return false;
    }

    return true;

  } catch (const std::exception &e) {
    spdlog::error("GrpcStreamingClient: Failed to initialize channel: {}",
                  e.what());
    return false;
  }
}

void GrpcStreamingClient::shutdownStreams() {
  // Stop all active streams
  stopServerStreaming();
  stopBidirectionalStreaming();

  if (clientStreaming_.load()) {
    finishClientStreaming();
  }

  // Clear any remaining queued messages
  {
    std::lock_guard<std::mutex> lock(clientStreamMutex_);
    while (!clientStreamQueue_.empty()) {
      clientStreamQueue_.pop();
    }
  }
}

void GrpcStreamingClient::serverStreamingLoop() {
  spdlog::debug("GrpcStreamingClient: Server streaming loop started");

  while (serverStreaming_.load() && !shutdown_.load()) {
    try {
      // astrocomm::proto::Message protoMessage;
      // if (serverStream_->Read(&protoMessage)) {
      //     auto message = convertFromProtoMessage(protoMessage);
      //     if (message && messageHandler_) {
      //         messageHandler_(*message);
      //     }
      //     incrementMessagesReceived();
      //     updateLastActivity();
      // } else {
      //     // Stream ended
      //     break;
      // }

      // For now, simulate periodic messages
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Simulate receiving a message
      if (messageHandler_) {
        auto simulatedMessage =
            std::make_unique<EventMessage>("server_stream_data");
        simulatedMessage->setDetails(
            json{{"timestamp",
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count()}});
        messageHandler_(*simulatedMessage);
        incrementMessagesReceived();
        updateLastActivity();
      }

    } catch (const std::exception &e) {
      spdlog::error("GrpcStreamingClient: Error in server streaming loop: {}",
                    e.what());
      handleStreamError(e.what(), "server_streaming_loop");
      break;
    }
  }

  spdlog::debug("GrpcStreamingClient: Server streaming loop stopped");
}

void GrpcStreamingClient::bidirectionalStreamingLoop() {
  spdlog::debug("GrpcStreamingClient: Bidirectional streaming loop started");

  while (bidirectionalStreaming_.load() && !shutdown_.load()) {
    try {
      // astrocomm::proto::Message protoMessage;
      // if (bidirectionalStream_->Read(&protoMessage)) {
      //     auto message = convertFromProtoMessage(protoMessage);
      //     if (message && messageHandler_) {
      //         messageHandler_(*message);
      //     }
      //     incrementMessagesReceived();
      //     updateLastActivity();
      // } else {
      //     // Stream ended
      //     break;
      // }

      // For now, simulate periodic messages
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Simulate receiving a message
      if (messageHandler_) {
        auto simulatedMessage =
            std::make_unique<EventMessage>("bidirectional_stream_data");
        simulatedMessage->setDetails(
            json{{"timestamp",
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count()}});
        messageHandler_(*simulatedMessage);
        incrementMessagesReceived();
        updateLastActivity();
      }

    } catch (const std::exception &e) {
      spdlog::error(
          "GrpcStreamingClient: Error in bidirectional streaming loop: {}",
          e.what());
      handleStreamError(e.what(), "bidirectional_streaming_loop");
      break;
    }
  }

  spdlog::debug("GrpcStreamingClient: Bidirectional streaming loop stopped");
}

void GrpcStreamingClient::processClientStreamQueue() {
  spdlog::debug("GrpcStreamingClient: Client stream queue processor started");

  while (clientStreaming_.load() && !shutdown_.load()) {
    try {
      std::unique_lock<std::mutex> lock(clientStreamMutex_);

      // Wait for messages or stream end
      clientStreamCV_.wait(lock, [this] {
        return !clientStreamQueue_.empty() || !clientStreaming_.load() ||
               shutdown_.load();
      });

      // Process all queued messages
      while (!clientStreamQueue_.empty() && clientStreaming_.load()) {
        auto message = clientStreamQueue_.front();
        clientStreamQueue_.pop();
        lock.unlock();

        // bool success = clientStream_->Write(message);
        bool success = true; // Simulate success

        if (!success) {
          spdlog::error(
              "GrpcStreamingClient: Failed to write to client stream");
          incrementStreamingErrors();
          break;
        }

        lock.lock();
      }

    } catch (const std::exception &e) {
      spdlog::error(
          "GrpcStreamingClient: Error in client stream queue processor: {}",
          e.what());
      handleStreamError(e.what(), "client_stream_queue");
      break;
    }
  }

  spdlog::debug("GrpcStreamingClient: Client stream queue processor stopped");
}

json GrpcStreamingClient::convertToProtoMessage(const Message &message) const {
  // Use the message transformer to convert to protobuf format
  auto &transformer = getGlobalMessageTransformer();
  auto result = transformer.transform(message, MessageFormat::PROTOBUF);

  if (result.success) {
    return result.transformedData;
  }

  // Fallback to basic conversion
  json protoMessage;
  protoMessage["message_id"] = message.getMessageId();
  protoMessage["device_id"] = message.getDeviceId();
  protoMessage["timestamp"] = message.getTimestamp();
  protoMessage["message_type"] = static_cast<int>(message.getMessageType());
  protoMessage["priority"] = static_cast<int>(message.getPriority());
  protoMessage["qos_level"] = static_cast<int>(message.getQoSLevel());

  return protoMessage;
}

std::unique_ptr<Message>
GrpcStreamingClient::convertFromProtoMessage(const json &protoMessage) const {
  // Use the message transformer to convert from protobuf format
  auto &transformer = getGlobalMessageTransformer();
  return transformer.transformToInternal(protoMessage, MessageFormat::PROTOBUF);
}

void GrpcStreamingClient::handleStreamError(const std::string &error,
                                            const std::string &context) {
  spdlog::error("GrpcStreamingClient: Stream error in {}: {}", context, error);

  incrementStreamingErrors();

  if (errorHandler_) {
    errorHandler_(error);
  }

  // Consider reconnection logic here if appropriate
}

void GrpcStreamingClient::updateConnectionStatus(bool connected) {
  if (statusHandler_) {
    statusHandler_(connected);
  }
}

void GrpcStreamingClient::incrementMessagesSent() {
  std::lock_guard<std::mutex> lock(statsMutex_);
  stats_.messagesSent++;
}

void GrpcStreamingClient::incrementMessagesReceived() {
  std::lock_guard<std::mutex> lock(statsMutex_);
  stats_.messagesReceived++;
}

void GrpcStreamingClient::incrementStreamingErrors() {
  std::lock_guard<std::mutex> lock(statsMutex_);
  stats_.streamingErrors++;
}

void GrpcStreamingClient::updateLastActivity() {
  std::lock_guard<std::mutex> lock(statsMutex_);
  stats_.lastActivity = std::chrono::steady_clock::now();
}

// GrpcStreamingFactory implementation
std::unique_ptr<GrpcStreamingClient>
GrpcStreamingFactory::createStreamingClient(const GrpcConfig &config) {
  return std::make_unique<GrpcStreamingClient>(config);
}

std::unique_ptr<GrpcStreamingServer>
GrpcStreamingFactory::createStreamingServer(const GrpcConfig &config) {
  return std::make_unique<GrpcStreamingServer>(config);
}

bool GrpcStreamingFactory::isGrpcAvailable() {
  return true; // Would check if gRPC libraries are available
}

std::string GrpcStreamingFactory::getGrpcVersion() {
  return "1.60.0"; // Would return actual gRPC version
}

} // namespace core
} // namespace hydrogen

#endif // HYDROGEN_ENABLE_GRPC
