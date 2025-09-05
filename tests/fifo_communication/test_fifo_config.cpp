#include <gtest/gtest.h>
#include <hydrogen/core/fifo_config_manager.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using namespace hydrogen::core;
using json = nlohmann::json;

class FifoConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        configManager_ = &getGlobalFifoConfigManager();
        tempConfigFile_ = "test_fifo_config.json";
    }
    
    void TearDown() override {
        // Clean up temporary files
        if (std::filesystem::exists(tempConfigFile_)) {
            std::filesystem::remove(tempConfigFile_);
        }
    }
    
    FifoConfigManager* configManager_;
    std::string tempConfigFile_;
};

// Test basic configuration creation
TEST_F(FifoConfigTest, CreateDefaultConfig) {
    FifoConfig config = configManager_->createConfig();
    
    EXPECT_FALSE(config.pipeName.empty());
    EXPECT_GT(config.bufferSize, 0);
    EXPECT_GT(config.maxMessageSize, 0);
    EXPECT_GT(config.connectTimeout.count(), 0);
    EXPECT_TRUE(config.validate());
}

// Test configuration presets
TEST_F(FifoConfigTest, CreatePresetConfigs) {
    // Test all presets
    std::vector<FifoConfigManager::ConfigPreset> presets = {
        FifoConfigManager::ConfigPreset::DEFAULT,
        FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE,
        FifoConfigManager::ConfigPreset::LOW_LATENCY,
        FifoConfigManager::ConfigPreset::RELIABLE,
        FifoConfigManager::ConfigPreset::SECURE,
        FifoConfigManager::ConfigPreset::DEBUG,
        FifoConfigManager::ConfigPreset::EMBEDDED,
        FifoConfigManager::ConfigPreset::BIDIRECTIONAL,
        FifoConfigManager::ConfigPreset::BROADCAST,
        FifoConfigManager::ConfigPreset::STREAMING
    };
    
    for (auto preset : presets) {
        FifoConfig config = configManager_->createConfig(preset);
        EXPECT_TRUE(config.validate()) << "Preset " << static_cast<int>(preset) << " failed validation";
        EXPECT_FALSE(config.pipeName.empty()) << "Preset " << static_cast<int>(preset) << " has empty pipe name";
    }
}

// Test high-performance preset specific settings
TEST_F(FifoConfigTest, HighPerformancePreset) {
    FifoConfig config = configManager_->createConfig(FifoConfigManager::ConfigPreset::HIGH_PERFORMANCE);
    
    EXPECT_GT(config.bufferSize, 32768); // Should have large buffers
    EXPECT_TRUE(config.enableNonBlocking);
    EXPECT_TRUE(config.enableFlowControl);
    EXPECT_TRUE(config.enablePlatformOptimizations);
    EXPECT_FALSE(config.enableMessageValidation); // Disabled for performance
}

// Test secure preset specific settings
TEST_F(FifoConfigTest, SecurePreset) {
    FifoConfig config = configManager_->createConfig(FifoConfigManager::ConfigPreset::SECURE);
    
    EXPECT_EQ(config.authMethod, FifoAuthMethod::TOKEN_BASED);
    EXPECT_TRUE(config.enableEncryption);
    EXPECT_TRUE(config.enableMessageValidation);
    EXPECT_TRUE(config.enableMessageLogging);
    EXPECT_EQ(config.pipePermissions, 0600); // Owner read/write only
}

// Test configuration serialization
TEST_F(FifoConfigTest, ConfigSerialization) {
    FifoConfig originalConfig = configManager_->createConfig();
    originalConfig.pipeName = "test_pipe";
    originalConfig.bufferSize = 16384;
    originalConfig.enableDebugLogging = true;
    
    // Serialize to JSON
    json configJson = originalConfig.toJson();
    EXPECT_FALSE(configJson.empty());
    EXPECT_EQ(configJson["pipeName"], "test_pipe");
    EXPECT_EQ(configJson["bufferSize"], 16384);
    EXPECT_TRUE(configJson["enableDebugLogging"]);
    
    // Deserialize from JSON
    FifoConfig deserializedConfig;
    deserializedConfig.fromJson(configJson);
    
    EXPECT_EQ(deserializedConfig.pipeName, originalConfig.pipeName);
    EXPECT_EQ(deserializedConfig.bufferSize, originalConfig.bufferSize);
    EXPECT_EQ(deserializedConfig.enableDebugLogging, originalConfig.enableDebugLogging);
}

// Test configuration validation
TEST_F(FifoConfigTest, ConfigValidation) {
    FifoConfig validConfig = configManager_->createConfig();
    EXPECT_TRUE(validConfig.validate());
    
    // Test invalid configurations
    FifoConfig invalidConfig1 = validConfig;
    invalidConfig1.pipeName = ""; // Empty pipe name
    EXPECT_FALSE(invalidConfig1.validate());
    
    FifoConfig invalidConfig2 = validConfig;
    invalidConfig2.bufferSize = 0; // Zero buffer size
    EXPECT_FALSE(invalidConfig2.validate());
    
    FifoConfig invalidConfig3 = validConfig;
    invalidConfig3.maxMessageSize = 0; // Zero max message size
    EXPECT_FALSE(invalidConfig3.validate());
    
    FifoConfig invalidConfig4 = validConfig;
    invalidConfig4.connectTimeout = std::chrono::milliseconds(0); // Zero timeout
    EXPECT_FALSE(invalidConfig4.validate());
}

// Test configuration validation with detailed results
TEST_F(FifoConfigTest, DetailedConfigValidation) {
    FifoConfig config = configManager_->createConfig();
    
    auto result = configManager_->validateConfig(config);
    EXPECT_TRUE(result.isValid);
    EXPECT_TRUE(result.errors.empty());
    
    // Test with invalid config
    config.pipeName = "";
    config.bufferSize = 0;
    
    result = configManager_->validateConfig(config);
    EXPECT_FALSE(result.isValid);
    EXPECT_FALSE(result.errors.empty());
    EXPECT_GT(result.errors.size(), 1); // Should have multiple errors
}

// Test configuration merging
TEST_F(FifoConfigTest, ConfigMerging) {
    FifoConfig baseConfig = configManager_->createConfig();
    FifoConfig overrideConfig = configManager_->createConfig();
    
    overrideConfig.pipeName = "merged_pipe";
    overrideConfig.bufferSize = 32768;
    overrideConfig.enableDebugLogging = true;
    
    FifoConfig mergedConfig = configManager_->mergeConfigs(baseConfig, overrideConfig);
    
    EXPECT_EQ(mergedConfig.pipeName, "merged_pipe");
    EXPECT_EQ(mergedConfig.bufferSize, 32768);
    EXPECT_TRUE(mergedConfig.enableDebugLogging);
    
    // Other fields should remain from base config
    EXPECT_EQ(mergedConfig.maxMessageSize, baseConfig.maxMessageSize);
    EXPECT_EQ(mergedConfig.connectTimeout, baseConfig.connectTimeout);
}

// Test custom configuration creation
TEST_F(FifoConfigTest, CustomConfigCreation) {
    json customSettings;
    customSettings["pipeName"] = "custom_pipe";
    customSettings["bufferSize"] = 65536;
    customSettings["enableNonBlocking"] = true;
    customSettings["compressionType"] = static_cast<int>(FifoCompressionType::LZ4);
    
    FifoConfig customConfig = configManager_->createCustomConfig(customSettings);
    
    EXPECT_EQ(customConfig.pipeName, "custom_pipe");
    EXPECT_EQ(customConfig.bufferSize, 65536);
    EXPECT_TRUE(customConfig.enableNonBlocking);
    EXPECT_EQ(customConfig.compressionType, FifoCompressionType::LZ4);
    EXPECT_TRUE(customConfig.validate());
}

// Test platform-specific configuration
TEST_F(FifoConfigTest, PlatformOptimizedConfig) {
    FifoConfig config = configManager_->createPlatformOptimizedConfig();
    
    EXPECT_TRUE(config.validate());
    EXPECT_FALSE(config.pipeName.empty());
    
    // Check platform-specific paths
#ifdef _WIN32
    EXPECT_EQ(config.pipeType, FifoPipeType::WINDOWS_NAMED_PIPE);
    EXPECT_FALSE(config.windowsPipePath.empty());
    EXPECT_TRUE(config.windowsPipePath.find("\\\\.\\pipe\\") == 0);
#else
    EXPECT_EQ(config.pipeType, FifoPipeType::UNIX_FIFO);
    EXPECT_FALSE(config.unixPipePath.empty());
    EXPECT_TRUE(config.unixPipePath[0] == '/');
#endif
}

// Test configuration file I/O
TEST_F(FifoConfigTest, ConfigFileIO) {
    FifoConfig originalConfig = configManager_->createConfig();
    originalConfig.pipeName = "file_test_pipe";
    originalConfig.bufferSize = 16384;
    
    // Save configuration
    EXPECT_TRUE(configManager_->saveConfig(originalConfig, tempConfigFile_));
    EXPECT_TRUE(std::filesystem::exists(tempConfigFile_));
    
    // Load configuration
    FifoConfig loadedConfig = configManager_->loadConfig(tempConfigFile_);
    
    EXPECT_EQ(loadedConfig.pipeName, originalConfig.pipeName);
    EXPECT_EQ(loadedConfig.bufferSize, originalConfig.bufferSize);
    EXPECT_TRUE(loadedConfig.validate());
}

// Test configuration comparison
TEST_F(FifoConfigTest, ConfigComparison) {
    FifoConfig config1 = configManager_->createConfig();
    FifoConfig config2 = configManager_->createConfig();
    
    // Modify config2
    config2.pipeName = "different_pipe";
    config2.bufferSize = config1.bufferSize * 2;
    
    json comparison = configManager_->compareConfigs(config1, config2);
    EXPECT_FALSE(comparison.empty());
    
    auto differences = configManager_->getConfigDifferences(config1, config2);
    EXPECT_FALSE(differences.empty());
    EXPECT_GT(differences.size(), 1); // Should have multiple differences
}

// Test configuration optimization
TEST_F(FifoConfigTest, ConfigOptimization) {
    FifoConfig config = configManager_->createConfig();
    
    // Set some suboptimal values
    config.bufferSize = 1; // Very small buffer
    config.readTimeout = std::chrono::milliseconds(1); // Very short timeout
    
    FifoConfig optimizedConfig = configManager_->optimizeConfig(config);
    
    EXPECT_GT(optimizedConfig.bufferSize, config.bufferSize);
    EXPECT_GT(optimizedConfig.readTimeout.count(), config.readTimeout.count());
    EXPECT_TRUE(optimizedConfig.validate());
}

// Test configuration schema
TEST_F(FifoConfigTest, ConfigSchema) {
    json schema = configManager_->getConfigSchema();
    
    EXPECT_FALSE(schema.empty());
    EXPECT_TRUE(schema.contains("properties"));
    EXPECT_TRUE(schema["properties"].contains("pipeName"));
    EXPECT_TRUE(schema["properties"].contains("bufferSize"));
    EXPECT_TRUE(schema["properties"].contains("maxMessageSize"));
}

// Test runtime configuration updates
TEST_F(FifoConfigTest, RuntimeConfigUpdates) {
    FifoConfig config = configManager_->createConfig();
    
    json updates;
    updates["bufferSize"] = 32768;
    updates["enableDebugLogging"] = true;
    
    EXPECT_TRUE(configManager_->updateConfig(config, updates));
    EXPECT_EQ(config.bufferSize, 32768);
    EXPECT_TRUE(config.enableDebugLogging);
    
    // Test invalid update
    json invalidUpdates;
    invalidUpdates["bufferSize"] = -1; // Invalid value
    
    EXPECT_FALSE(configManager_->updateConfig(config, invalidUpdates));
}

// Test preset descriptions
TEST_F(FifoConfigTest, PresetDescriptions) {
    auto presets = configManager_->getAvailablePresets();
    EXPECT_FALSE(presets.empty());
    
    for (auto preset : presets) {
        std::string description = configManager_->getPresetDescription(preset);
        EXPECT_FALSE(description.empty());
        
        FifoConfig presetConfig = configManager_->getPresetConfig(preset);
        EXPECT_TRUE(presetConfig.validate());
    }
}
