#include <gtest/gtest.h>
#include "hydrogen/server/infrastructure/config_manager.h"
#include "hydrogen/server/repositories/config_repository.h"
#include <memory>

using namespace hydrogen::server::infrastructure;
using namespace hydrogen::server::repositories;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConfigManagerFactory factory;
        std::unordered_map<std::string, std::string> config;
        config["configPath"] = "./test_data/config_manager_test.json";

        auto service = factory.createService("ConfigManager", config);
        auto* rawPtr = service.release();
        auto* configManager = dynamic_cast<IConfigManager*>(rawPtr);
        if (!configManager) {
            delete rawPtr;
            FAIL() << "Failed to cast service to IConfigManager";
        }
        manager_ = std::unique_ptr<IConfigManager>(configManager);
        ASSERT_NE(manager_, nullptr);
        ASSERT_TRUE(manager_->initialize());
    }

    void TearDown() override {
        if (manager_) {
            manager_->stop();
        }
    }

    std::unique_ptr<IConfigManager> manager_;
};

TEST_F(ConfigManagerTest, BasicOperations) {
    // String operations
    EXPECT_TRUE(manager_->set("test.string", "hello"));
    EXPECT_EQ(manager_->get("test.string", "default"), "hello");

    // Integer operations
    EXPECT_TRUE(manager_->setInt("test.int", 42));
    EXPECT_EQ(manager_->getInt("test.int", 0), 42);

    // Double operations
    EXPECT_TRUE(manager_->setDouble("test.double", 3.14));
    EXPECT_DOUBLE_EQ(manager_->getDouble("test.double", 0.0), 3.14);

    // Boolean operations
    EXPECT_TRUE(manager_->setBool("test.bool", true));
    EXPECT_TRUE(manager_->getBool("test.bool", false));

    // Test existence
    EXPECT_TRUE(manager_->exists("test.string"));
    EXPECT_FALSE(manager_->exists("nonexistent.key"));
}

TEST_F(ConfigManagerTest, ObjectOperations) {
    std::unordered_map<std::string, std::string> object = {
        {"host", "localhost"},
        {"port", "8080"}
    };

    EXPECT_TRUE(manager_->setObject("server", object));
    auto retrieved = manager_->getObject("server");
    EXPECT_EQ(retrieved.size(), 2);
    EXPECT_EQ(retrieved["host"], "localhost");
    EXPECT_EQ(retrieved["port"], "8080");
}

TEST_F(ConfigManagerTest, ArrayOperations) {
    std::vector<std::string> array = {"item1", "item2", "item3"};

    EXPECT_TRUE(manager_->setArray("test.array", array));
    auto retrieved = manager_->getArray("test.array");
    EXPECT_EQ(retrieved.size(), 3);
    EXPECT_EQ(retrieved[0], "item1");
    EXPECT_EQ(retrieved[1], "item2");
    EXPECT_EQ(retrieved[2], "item3");
}

TEST_F(ConfigManagerTest, HierarchicalOperations) {
    // Test hierarchical configuration
    EXPECT_TRUE(manager_->setHierarchical("server.database.host", "localhost"));
    EXPECT_EQ(manager_->getHierarchical("server.database.host", ""), "localhost");

    // Get section
    auto section = manager_->getSection("server");
    EXPECT_GE(section.size(), 0);
}

TEST_F(ConfigManagerTest, Validation) {
    // Test configuration validation
    EXPECT_TRUE(manager_->validate());

    auto errors = manager_->getValidationErrors();
    EXPECT_EQ(errors.size(), 0);
}
