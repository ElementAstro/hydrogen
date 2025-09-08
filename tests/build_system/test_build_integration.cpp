#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>

/**
 * @brief Test fixture for build system integration tests
 */
class BuildSystemIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get project root directory
        projectRoot_ = std::filesystem::current_path();
        while (!std::filesystem::exists(projectRoot_ / "xmake.lua") && 
               !std::filesystem::exists(projectRoot_ / "CMakeLists.txt")) {
            auto parent = projectRoot_.parent_path();
            if (parent == projectRoot_) {
                FAIL() << "Could not find project root directory";
            }
            projectRoot_ = parent;
        }
        
        buildDir_ = projectRoot_ / "build_test";
        
        // Clean up any previous test builds
        if (std::filesystem::exists(buildDir_)) {
            std::filesystem::remove_all(buildDir_);
        }
    }

    void TearDown() override {
        // Clean up test build directory
        if (std::filesystem::exists(buildDir_)) {
            std::filesystem::remove_all(buildDir_);
        }
    }

    bool fileExists(const std::filesystem::path& path) {
        return std::filesystem::exists(path);
    }

    bool directoryExists(const std::filesystem::path& path) {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    }

    std::vector<std::string> getExpectedTestTargets() {
        return {
            "core_tests",
            "server_tests", 
            "client_tests",
            "device_tests",
            "integration_tests",
            "protocol_tests",
            "stdio_tests",
            "fifo_tests"
        };
    }

    std::filesystem::path projectRoot_;
    std::filesystem::path buildDir_;
};

/**
 * @brief Test that essential build files exist
 */
TEST_F(BuildSystemIntegrationTest, BuildFilesExist) {
    // Check for XMake files
    EXPECT_TRUE(fileExists(projectRoot_ / "xmake.lua"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "xmake"));
    EXPECT_TRUE(fileExists(projectRoot_ / "xmake" / "tests.lua"));
    EXPECT_TRUE(fileExists(projectRoot_ / "xmake" / "libraries.lua"));
    
    // Check for CMake files
    EXPECT_TRUE(fileExists(projectRoot_ / "CMakeLists.txt"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "cmake"));
    
    // Check for test directories
    EXPECT_TRUE(directoryExists(projectRoot_ / "tests"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "tests" / "core"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "tests" / "server"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "tests" / "stdio"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "tests" / "fifo_communication"));
}

/**
 * @brief Test that STDIO communication test files exist
 */
TEST_F(BuildSystemIntegrationTest, StdioTestFilesExist) {
    // Check STDIO test files
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "core" / "test_stdio_communicator.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "core" / "test_stdio_message_transformer.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "server" / "test_stdio_server.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "integration" / "test_stdio_integration.cpp"));
    
    // Check STDIO CMake configuration
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "stdio" / "CMakeLists.txt"));
}

/**
 * @brief Test that FIFO communication test files exist
 */
TEST_F(BuildSystemIntegrationTest, FifoTestFilesExist) {
    // Check FIFO test files
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "fifo_communication" / "test_fifo_communicator.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "fifo_communication" / "test_fifo_config.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "fifo_communication" / "test_fifo_integration.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "fifo_communication" / "test_fifo_performance.cpp"));
    
    // Check FIFO CMake configuration
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "fifo_communication" / "CMakeLists.txt"));
}

/**
 * @brief Test XMake configuration validation
 */
TEST_F(BuildSystemIntegrationTest, XMakeConfigurationValidation) {
    // Read and validate xmake.lua
    std::ifstream xmakeFile(projectRoot_ / "xmake.lua");
    ASSERT_TRUE(xmakeFile.is_open());
    
    std::string content((std::istreambuf_iterator<char>(xmakeFile)),
                        std::istreambuf_iterator<char>());
    
    // Check for essential configuration options
    EXPECT_NE(content.find("tests"), std::string::npos);
    EXPECT_NE(content.find("examples"), std::string::npos);
    EXPECT_NE(content.find("ssl"), std::string::npos);
    EXPECT_NE(content.find("compression"), std::string::npos);
    
    // Read and validate tests.lua
    std::ifstream testsFile(projectRoot_ / "xmake" / "tests.lua");
    ASSERT_TRUE(testsFile.is_open());
    
    std::string testsContent((std::istreambuf_iterator<char>(testsFile)),
                             std::istreambuf_iterator<char>());
    
    // Check for STDIO and FIFO test targets
    EXPECT_NE(testsContent.find("stdio_tests"), std::string::npos);
    EXPECT_NE(testsContent.find("fifo_tests"), std::string::npos);
    EXPECT_NE(testsContent.find("communication_tests"), std::string::npos);
}

/**
 * @brief Test CMake configuration validation
 */
TEST_F(BuildSystemIntegrationTest, CMakeConfigurationValidation) {
    // Read and validate main CMakeLists.txt
    std::ifstream cmakeFile(projectRoot_ / "CMakeLists.txt");
    ASSERT_TRUE(cmakeFile.is_open());
    
    std::string content((std::istreambuf_iterator<char>(cmakeFile)),
                        std::istreambuf_iterator<char>());
    
    // Check for essential configuration
    EXPECT_NE(content.find("project"), std::string::npos);
    EXPECT_NE(content.find("CXX"), std::string::npos);
    
    // Check STDIO CMake configuration
    std::ifstream stdioCmakeFile(projectRoot_ / "tests" / "stdio" / "CMakeLists.txt");
    ASSERT_TRUE(stdioCmakeFile.is_open());
    
    std::string stdioContent((std::istreambuf_iterator<char>(stdioCmakeFile)),
                             std::istreambuf_iterator<char>());
    
    // Check for STDIO test targets
    EXPECT_NE(stdioContent.find("test_stdio_communicator"), std::string::npos);
    EXPECT_NE(stdioContent.find("test_stdio_message_transformer"), std::string::npos);
    EXPECT_NE(stdioContent.find("test_stdio_server"), std::string::npos);
    EXPECT_NE(stdioContent.find("test_stdio_integration"), std::string::npos);
}

/**
 * @brief Test that source files referenced in build systems exist
 */
TEST_F(BuildSystemIntegrationTest, SourceFilesExist) {
    // Check core source files
    EXPECT_TRUE(directoryExists(projectRoot_ / "src" / "core"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "src" / "server"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "src" / "client"));
    
    // Check for essential header files
    EXPECT_TRUE(fileExists(projectRoot_ / "src" / "core" / "include" / "hydrogen" / "core" / "message.h"));
    EXPECT_TRUE(fileExists(projectRoot_ / "src" / "core" / "include" / "hydrogen" / "core" / "message_transformer.h"));
    
    // Check for STDIO implementation files
    EXPECT_TRUE(fileExists(projectRoot_ / "src" / "server" / "include" / "hydrogen" / "server" / "protocols" / "stdio" / "stdio_server.h"));
}

/**
 * @brief Test build system feature flags
 */
TEST_F(BuildSystemIntegrationTest, BuildSystemFeatureFlags) {
    // This test verifies that the build systems support the required feature flags
    // In a real scenario, you would run the build commands and check their output
    
    // For XMake, the expected flags are: --tests=y --examples=y --ssl=y --compression=y
    // For CMake, similar options should be available
    
    // Check that XMake configuration supports these flags
    std::ifstream xmakeFile(projectRoot_ / "xmake.lua");
    ASSERT_TRUE(xmakeFile.is_open());
    
    std::string content((std::istreambuf_iterator<char>(xmakeFile)),
                        std::istreambuf_iterator<char>());
    
    // Verify configuration options exist
    EXPECT_NE(content.find("option(\"tests\")"), std::string::npos);
    EXPECT_NE(content.find("option(\"examples\")"), std::string::npos);
    EXPECT_NE(content.find("option(\"ssl\")"), std::string::npos);
    EXPECT_NE(content.find("option(\"compression\")"), std::string::npos);
}

/**
 * @brief Test communication protocol examples exist
 */
TEST_F(BuildSystemIntegrationTest, CommunicationExamplesExist) {
    // Check STDIO examples
    EXPECT_TRUE(directoryExists(projectRoot_ / "examples" / "stdio_communication"));
    EXPECT_TRUE(fileExists(projectRoot_ / "examples" / "stdio_communication" / "stdio_client_example.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "examples" / "stdio_communication" / "stdio_server_example.cpp"));
    
    // Check FIFO examples
    EXPECT_TRUE(directoryExists(projectRoot_ / "examples" / "fifo_communication"));
    EXPECT_TRUE(fileExists(projectRoot_ / "examples" / "fifo_communication" / "fifo_client_example.cpp"));
    EXPECT_TRUE(fileExists(projectRoot_ / "examples" / "fifo_communication" / "fifo_server_example.cpp"));
}

/**
 * @brief Test that test framework dependencies are properly configured
 */
TEST_F(BuildSystemIntegrationTest, TestFrameworkDependencies) {
    // Check that test framework files exist
    EXPECT_TRUE(directoryExists(projectRoot_ / "tests" / "framework"));
    EXPECT_TRUE(directoryExists(projectRoot_ / "tests" / "utils"));
    
    // Check XMake test framework configuration
    std::ifstream testsFile(projectRoot_ / "xmake" / "tests.lua");
    ASSERT_TRUE(testsFile.is_open());
    
    std::string testsContent((std::istreambuf_iterator<char>(testsFile)),
                             std::istreambuf_iterator<char>());
    
    // Check for test framework dependencies
    EXPECT_NE(testsContent.find("gtest"), std::string::npos);
    EXPECT_NE(testsContent.find("gmock"), std::string::npos);
    EXPECT_NE(testsContent.find("nlohmann_json"), std::string::npos);
    EXPECT_NE(testsContent.find("hydrogen_test_framework"), std::string::npos);
}

/**
 * @brief Test comprehensive test coverage structure
 */
TEST_F(BuildSystemIntegrationTest, TestCoverageStructure) {
    // Verify comprehensive test structure exists
    auto expectedTestDirs = std::vector<std::string>{
        "core", "server", "client", "device", "integration", 
        "protocols", "comprehensive", "performance", "utils",
        "stdio", "fifo_communication"
    };
    
    for (const auto& testDir : expectedTestDirs) {
        EXPECT_TRUE(directoryExists(projectRoot_ / "tests" / testDir)) 
            << "Test directory missing: " << testDir;
    }
    
    // Check that test coverage summary exists
    EXPECT_TRUE(fileExists(projectRoot_ / "tests" / "TEST_COVERAGE_SUMMARY.md"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
