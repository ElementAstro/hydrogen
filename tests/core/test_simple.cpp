#include <gtest/gtest.h>
#include <string>
#include <memory>

// Simple tests that don't depend on complex APIs
class SimpleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Basic setup
    }

    void TearDown() override {
        // Basic cleanup
    }
};

TEST_F(SimpleTest, BasicStringOperations) {
    std::string test = "hello world";
    EXPECT_EQ(test.length(), 11);
    EXPECT_TRUE(test.find("world") != std::string::npos);
}

TEST_F(SimpleTest, BasicMemoryOperations) {
    auto ptr = std::make_unique<int>(42);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);
}

TEST_F(SimpleTest, BasicMathOperations) {
    EXPECT_EQ(2 + 2, 4);
    EXPECT_GT(5, 3);
    EXPECT_LT(1, 10);
}

// Test that Google Test framework is working
TEST(GoogleTestFramework, BasicAssertions) {
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
    EXPECT_EQ(1, 1);
    EXPECT_NE(1, 2);
}

// Test basic C++ features
TEST(CppFeatures, SmartPointers) {
    auto shared = std::make_shared<std::string>("test");
    auto weak = std::weak_ptr<std::string>(shared);
    
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(*shared, "test");
    
    shared.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(CppFeatures, Containers) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec.back(), 5);
    
    vec.push_back(6);
    EXPECT_EQ(vec.size(), 6);
    EXPECT_EQ(vec.back(), 6);
}
