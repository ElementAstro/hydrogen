#include <gtest/gtest.h>
#include "astrocomm/server/infrastructure/config_manager.h"
#include "astrocomm/server/repositories/config_repository.h"
#include <memory>

using namespace astrocomm::server::infrastructure;
using namespace astrocomm::server::repositories;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto repository = ConfigRepositoryFactory::createRepository("./test_data/config_manager_test.json");
        manager_ = ConfigManagerFactory::createManager(std::move(repository));
        ASSERT_NE(manager_, nullptr);
        ASSERT_TRUE(manager_->initialize());
    }
    
    void TearDown() override {
        if (manager_) {
            manager_->shutdown();
        }
    }
    
    std::unique_ptr<IConfigManager> manager_;
};

TEST_F(ConfigManagerTest, BasicOperations) {
    EXPECT_TRUE(manager_->isInitialized());
    
    // String operations
    EXPECT_TRUE(manager_->setString("test.string", "hello"));
    EXPECT_EQ(manager_->getString("test.string", "default"), "hello");
    
    // Integer operations
    EXPECT_TRUE(manager_->setInt("test.int", 42));
    EXPECT_EQ(manager_->getInt("test.int", 0), 42);
    
    // Double operations
    EXPECT_TRUE(manager_->setDouble("test.double", 3.14));
    EXPECT_DOUBLE_EQ(manager_->getDouble("test.double", 0.0), 3.14);
    
    // Boolean operations
    EXPECT_TRUE(manager_->setBool("test.bool", true));
    EXPECT_TRUE(manager_->getBool("test.bool", false));
}

TEST_F(ConfigManagerTest, SectionOperations) {
    std::unordered_map<std::string, std::string> section = {
        {"host", "localhost"},
        {"port", "8080"}
    };
    
    EXPECT_TRUE(manager_->setSection("server", section));
    auto retrieved = manager_->getSection("server");
    EXPECT_EQ(retrieved.size(), 2);
    EXPECT_EQ(retrieved["host"], "localhost");
    
    auto sectionNames = manager_->getSectionNames();
    EXPECT_GE(sectionNames.size(), 1);
}

TEST_F(ConfigManagerTest, DefaultValues) {
    // Test default configurations are loaded
    EXPECT_EQ(manager_->getString("server.host", ""), "localhost");
    EXPECT_EQ(manager_->getInt("server.port", 0), 8080);
    EXPECT_EQ(manager_->getString("logging.level", ""), "INFO");
}

TEST_F(ConfigManagerTest, ChangeNotification) {
    bool notified = false;
    std::string notifiedKey;
    std::string notifiedValue;
    
    manager_->registerChangeListener("test.notify", 
        [&](const std::string& key, const std::string& value) {
            notified = true;
            notifiedKey = key;
            notifiedValue = value;
        });
    
    manager_->setString("test.notify", "changed");
    
    EXPECT_TRUE(notified);
    EXPECT_EQ(notifiedKey, "test.notify");
    EXPECT_EQ(notifiedValue, "changed");
}

TEST_F(ConfigManagerTest, Validation) {
    EXPECT_TRUE(manager_->validateConfiguration());
    
    auto errors = manager_->getValidationErrors();
    EXPECT_TRUE(errors.empty()); // Should have no errors with default config
}
