#include <gtest/gtest.h>
#include "hydrogen/server/repositories/config_repository.h"
#include <memory>
#include <filesystem>

using namespace hydrogen::server::repositories;

class ConfigRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDataPath_ = "./test_data/config_test.json";
        std::filesystem::create_directories("./test_data");
        repository_ = ConfigRepositoryFactory::createRepository(testDataPath_);
        ASSERT_NE(repository_, nullptr);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(testDataPath_)) {
            std::filesystem::remove(testDataPath_);
        }
    }
    
    std::unique_ptr<IConfigRepository> repository_;
    std::string testDataPath_;
};

TEST_F(ConfigRepositoryTest, BasicOperations) {
    // Set and get string value
    EXPECT_TRUE(repository_->setValue("test.key", "test_value"));
    auto value = repository_->getValue("test.key");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "test_value");
    
    // Get with default
    EXPECT_EQ(repository_->getValue("nonexistent", "default"), "default");
    
    // Check key existence
    EXPECT_TRUE(repository_->hasKey("test.key"));
    EXPECT_FALSE(repository_->hasKey("nonexistent"));
    
    // Remove key
    EXPECT_TRUE(repository_->removeKey("test.key"));
    EXPECT_FALSE(repository_->hasKey("test.key"));
}

TEST_F(ConfigRepositoryTest, TypedOperations) {
    // Integer values
    EXPECT_TRUE(repository_->setIntValue("int.key", 42));
    auto intValue = repository_->getIntValue("int.key");
    ASSERT_TRUE(intValue.has_value());
    EXPECT_EQ(*intValue, 42);
    EXPECT_EQ(repository_->getIntValue("nonexistent", 100), 100);
    
    // Double values
    EXPECT_TRUE(repository_->setDoubleValue("double.key", 3.14));
    auto doubleValue = repository_->getDoubleValue("double.key");
    ASSERT_TRUE(doubleValue.has_value());
    EXPECT_DOUBLE_EQ(*doubleValue, 3.14);
    
    // Boolean values
    EXPECT_TRUE(repository_->setBoolValue("bool.key", true));
    auto boolValue = repository_->getBoolValue("bool.key");
    ASSERT_TRUE(boolValue.has_value());
    EXPECT_TRUE(*boolValue);
    EXPECT_FALSE(repository_->getBoolValue("nonexistent", false));
}

TEST_F(ConfigRepositoryTest, SectionOperations) {
    // Set section
    std::unordered_map<std::string, std::string> sectionData = {
        {"host", "localhost"},
        {"port", "8080"},
        {"timeout", "30"}
    };
    EXPECT_TRUE(repository_->setSection("server", sectionData));
    
    // Get section
    auto retrievedSection = repository_->getSection("server");
    EXPECT_EQ(retrievedSection.size(), 3);
    EXPECT_EQ(retrievedSection["host"], "localhost");
    EXPECT_EQ(retrievedSection["port"], "8080");
    EXPECT_EQ(retrievedSection["timeout"], "30");
    
    // Get section names
    auto sectionNames = repository_->getSectionNames();
    EXPECT_EQ(sectionNames.size(), 1);
    EXPECT_EQ(sectionNames[0], "server");
    
    // Remove section
    EXPECT_TRUE(repository_->removeSection("server"));
    auto emptySectionNames = repository_->getSectionNames();
    EXPECT_EQ(emptySectionNames.size(), 0);
}

TEST_F(ConfigRepositoryTest, BulkOperations) {
    std::unordered_map<std::string, std::string> configs = {
        {"app.name", "TestApp"},
        {"app.version", "1.0.0"},
        {"db.host", "localhost"},
        {"db.port", "5432"}
    };
    
    // Set all configurations
    EXPECT_TRUE(repository_->setAll(configs));
    EXPECT_EQ(repository_->count(), 4);
    
    // Get all configurations
    auto allConfigs = repository_->getAll();
    EXPECT_EQ(allConfigs.size(), 4);
    EXPECT_EQ(allConfigs["app.name"], "TestApp");
    
    // Merge additional configurations
    std::unordered_map<std::string, std::string> additionalConfigs = {
        {"app.debug", "true"},
        {"cache.enabled", "false"}
    };
    EXPECT_TRUE(repository_->merge(additionalConfigs));
    EXPECT_EQ(repository_->count(), 6);
    
    // Clear all
    EXPECT_TRUE(repository_->clear());
    EXPECT_EQ(repository_->count(), 0);
}

TEST_F(ConfigRepositoryTest, SearchOperations) {
    repository_->setValue("server.host", "localhost");
    repository_->setValue("server.port", "8080");
    repository_->setValue("database.host", "dbhost");
    repository_->setValue("cache.enabled", "true");
    
    // Find keys by pattern
    auto serverKeys = repository_->findKeys("server");
    EXPECT_EQ(serverKeys.size(), 2);
    
    // Find by key pattern
    auto hostConfigs = repository_->findByKeyPattern("host");
    EXPECT_EQ(hostConfigs.size(), 2);
    
    // Find by value pattern
    auto trueConfigs = repository_->findByValuePattern("true");
    EXPECT_EQ(trueConfigs.size(), 1);
}

TEST_F(ConfigRepositoryTest, PersistenceOperations) {
    repository_->setValue("persist.test", "value");
    
    // Save to file
    EXPECT_TRUE(repository_->save());
    EXPECT_TRUE(std::filesystem::exists(testDataPath_));
    
    // Create new repository and load
    auto newRepository = ConfigRepositoryFactory::createRepository(testDataPath_);
    EXPECT_TRUE(newRepository->load());
    EXPECT_EQ(newRepository->count(), 1);
    
    auto loaded = newRepository->getValue("persist.test");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, "value");
}
