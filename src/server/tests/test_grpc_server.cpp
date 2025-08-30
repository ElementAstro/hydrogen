#include <gtest/gtest.h>
#include "astrocomm/server/protocols/grpc/grpc_server.h"
#include <memory>

using namespace astrocomm::server::protocols::grpc;

class GrpcServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        GrpcServerConfig config;
        config.serverAddress = "localhost:50052"; // Use different port for tests
        config.maxMessageSize = 1024 * 1024; // 1MB
        config.enableReflection = false; // Disable for tests
        
        server_ = GrpcServerFactory::createServer(config);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->initialize());
    }
    
    void TearDown() override {
        if (server_) {
            server_->stop();
        }
    }
    
    std::unique_ptr<IGrpcServer> server_;
};

TEST_F(GrpcServerTest, BasicOperations) {
    EXPECT_TRUE(server_->isInitialized());
    EXPECT_FALSE(server_->isRunning());
    
    // Start server
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(server_->isRunning());
    
    // Stop server
    EXPECT_TRUE(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(GrpcServerTest, Configuration) {
    auto config = server_->getConfig();
    EXPECT_EQ(config.serverAddress, "localhost:50052");
    EXPECT_EQ(config.maxMessageSize, 1024 * 1024);
    
    // Update configuration
    config.maxMessageSize = 2 * 1024 * 1024;
    EXPECT_TRUE(server_->updateConfig(config));
    
    auto updatedConfig = server_->getConfig();
    EXPECT_EQ(updatedConfig.maxMessageSize, 2 * 1024 * 1024);
}

TEST_F(GrpcServerTest, ServiceManagement) {
    // Get registered services (should be empty initially)
    auto services = server_->getRegisteredServices();
    EXPECT_EQ(services.size(), 0);
    
    // Note: In a real test, we would register actual gRPC services
    // For now, we just test the interface
}

TEST_F(GrpcServerTest, HealthChecking) {
    EXPECT_TRUE(server_->isHealthy());
    EXPECT_EQ(server_->getHealthStatus(), "Healthy");
}

TEST_F(GrpcServerTest, Statistics) {
    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.totalRequests, 0);
    EXPECT_EQ(stats.activeConnections, 0);
    EXPECT_EQ(stats.totalErrors, 0);
    
    server_->resetStatistics();
    auto resetStats = server_->getStatistics();
    EXPECT_EQ(resetStats.totalRequests, 0);
}

TEST_F(GrpcServerTest, Security) {
    // Test TLS configuration (without actual certificates)
    EXPECT_FALSE(server_->enableTLS("nonexistent.crt", "nonexistent.key"));
    EXPECT_TRUE(server_->disableTLS());
    
    // Test authentication method
    EXPECT_TRUE(server_->setAuthenticationMethod(GrpcAuthMethod::TOKEN));
}
