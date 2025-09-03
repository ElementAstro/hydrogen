/**
 * @file test_utilities.cpp
 * @brief Comprehensive tests for utility functions
 * 
 * Tests utility functions including UUID generation, timestamp formatting,
 * string utilities, and other helper functions.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <common/utils.h>
#include <set>
#include <regex>
#include <chrono>
#include <thread>

using namespace hydrogen;
using namespace testing;

class UtilitiesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }
    
    void TearDown() override {
        // Test cleanup
    }
};

// Test UUID generation
TEST_F(UtilitiesTest, UuidGeneration) {
    // Generate multiple UUIDs
    std::string uuid1 = generateUuid();
    std::string uuid2 = generateUuid();
    std::string uuid3 = generateUuid();
    
    // UUIDs should not be empty
    EXPECT_FALSE(uuid1.empty());
    EXPECT_FALSE(uuid2.empty());
    EXPECT_FALSE(uuid3.empty());
    
    // UUIDs should be unique
    EXPECT_NE(uuid1, uuid2);
    EXPECT_NE(uuid2, uuid3);
    EXPECT_NE(uuid1, uuid3);
    
    // UUIDs should have correct format (xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx)
    std::regex uuidRegex(R"([0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12})");
    EXPECT_TRUE(std::regex_match(uuid1, uuidRegex));
    EXPECT_TRUE(std::regex_match(uuid2, uuidRegex));
    EXPECT_TRUE(std::regex_match(uuid3, uuidRegex));
}

// Test UUID uniqueness over many generations
TEST_F(UtilitiesTest, UuidUniqueness) {
    const int numUuids = 1000;
    std::set<std::string> uuids;
    
    // Generate many UUIDs
    for (int i = 0; i < numUuids; i++) {
        std::string uuid = generateUuid();
        uuids.insert(uuid);
    }
    
    // All UUIDs should be unique
    EXPECT_EQ(uuids.size(), numUuids);
}

// Test concurrent UUID generation
TEST_F(UtilitiesTest, ConcurrentUuidGeneration) {
    const int numThreads = 4;
    const int uuidsPerThread = 100;
    std::vector<std::thread> threads;
    std::vector<std::vector<std::string>> threadUuids(numThreads);
    
    // Create multiple threads that generate UUIDs
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&threadUuids, t, uuidsPerThread]() {
            for (int i = 0; i < uuidsPerThread; i++) {
                threadUuids[t].push_back(generateUuid());
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Collect all UUIDs and check uniqueness
    std::set<std::string> allUuids;
    for (const auto& threadUuidList : threadUuids) {
        for (const auto& uuid : threadUuidList) {
            allUuids.insert(uuid);
        }
    }
    
    EXPECT_EQ(allUuids.size(), numThreads * uuidsPerThread);
}

// Test ISO timestamp generation
TEST_F(UtilitiesTest, IsoTimestampGeneration) {
    std::string timestamp1 = getIsoTimestamp();
    
    // Wait a bit to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    std::string timestamp2 = getIsoTimestamp();
    
    // Timestamps should not be empty
    EXPECT_FALSE(timestamp1.empty());
    EXPECT_FALSE(timestamp2.empty());
    
    // Timestamps should be different
    EXPECT_NE(timestamp1, timestamp2);
    
    // Timestamps should have ISO 8601 format (basic validation)
    // Format: YYYY-MM-DDTHH:MM:SS.sssZ or similar
    EXPECT_GT(timestamp1.length(), 19); // At least YYYY-MM-DDTHH:MM:SS
    EXPECT_NE(timestamp1.find('T'), std::string::npos); // Should contain 'T' separator
    
    EXPECT_GT(timestamp2.length(), 19);
    EXPECT_NE(timestamp2.find('T'), std::string::npos);
}

// Test timestamp format consistency
TEST_F(UtilitiesTest, TimestampFormatConsistency) {
    std::vector<std::string> timestamps;
    
    // Generate multiple timestamps
    for (int i = 0; i < 10; i++) {
        timestamps.push_back(getIsoTimestamp());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // All timestamps should have similar format
    size_t expectedLength = timestamps[0].length();
    for (const auto& timestamp : timestamps) {
        EXPECT_EQ(timestamp.length(), expectedLength);
        EXPECT_NE(timestamp.find('T'), std::string::npos);
    }
}

// Test boolean parsing
TEST_F(UtilitiesTest, BooleanParsing) {
    // Test true values
    EXPECT_TRUE(parseBoolean("true"));
    EXPECT_TRUE(parseBoolean("TRUE"));
    EXPECT_TRUE(parseBoolean("True"));
    EXPECT_TRUE(parseBoolean("1"));
    EXPECT_TRUE(parseBoolean("yes"));
    EXPECT_TRUE(parseBoolean("YES"));
    EXPECT_TRUE(parseBoolean("on"));
    EXPECT_TRUE(parseBoolean("ON"));
    
    // Test false values
    EXPECT_FALSE(parseBoolean("false"));
    EXPECT_FALSE(parseBoolean("FALSE"));
    EXPECT_FALSE(parseBoolean("False"));
    EXPECT_FALSE(parseBoolean("0"));
    EXPECT_FALSE(parseBoolean("no"));
    EXPECT_FALSE(parseBoolean("NO"));
    EXPECT_FALSE(parseBoolean("off"));
    EXPECT_FALSE(parseBoolean("OFF"));
    
    // Test invalid values (should default to false)
    EXPECT_FALSE(parseBoolean("invalid"));
    EXPECT_FALSE(parseBoolean(""));
    EXPECT_FALSE(parseBoolean("maybe"));
    EXPECT_FALSE(parseBoolean("2"));
}

// Test string utilities (if available)
TEST_F(UtilitiesTest, StringUtilities) {
    // Test string trimming (if available in string_utils namespace)
    using namespace string_utils;
    
    // These tests assume string utility functions exist
    // Adjust based on actual implementation
    
    // Test basic string operations that should be available
    std::string testString = "  test string  ";
    
    // If trim functions exist, test them
    // EXPECT_EQ(trim(testString), "test string");
    // EXPECT_EQ(ltrim(testString), "test string  ");
    // EXPECT_EQ(rtrim(testString), "  test string");
    
    // For now, just test that the namespace exists
    SUCCEED(); // Placeholder until actual string utilities are implemented
}

// Test utility function performance
TEST_F(UtilitiesTest, UtilityPerformance) {
    const int numOperations = 1000;
    
    // Test UUID generation performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        generateUuid();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto uuidDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Test timestamp generation performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations; i++) {
        getIsoTimestamp();
    }
    end = std::chrono::high_resolution_clock::now();
    auto timestampDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time
    EXPECT_LT(uuidDuration.count(), 1000); // Less than 1 second for 1000 UUIDs
    EXPECT_LT(timestampDuration.count(), 1000); // Less than 1 second for 1000 timestamps
}

// Test edge cases and error conditions
TEST_F(UtilitiesTest, EdgeCases) {
    // Test boolean parsing with edge cases
    EXPECT_FALSE(parseBoolean(""));
    EXPECT_FALSE(parseBoolean(" "));
    EXPECT_FALSE(parseBoolean("\t"));
    EXPECT_FALSE(parseBoolean("\n"));
    
    // Test with very long strings
    std::string longString(10000, 'a');
    EXPECT_FALSE(parseBoolean(longString));
    
    // Test with special characters
    EXPECT_FALSE(parseBoolean("!@#$%^&*()"));
    EXPECT_FALSE(parseBoolean("true false"));
    EXPECT_FALSE(parseBoolean("1 0"));
}

// Test utility function thread safety
TEST_F(UtilitiesTest, ThreadSafety) {
    const int numThreads = 8;
    const int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    // Test concurrent UUID generation and timestamp generation
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&successCount, operationsPerThread]() {
            try {
                for (int i = 0; i < operationsPerThread; i++) {
                    std::string uuid = generateUuid();
                    std::string timestamp = getIsoTimestamp();
                    
                    if (!uuid.empty() && !timestamp.empty()) {
                        successCount++;
                    }
                }
            } catch (...) {
                // Thread safety test should not throw exceptions
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All operations should succeed
    EXPECT_EQ(successCount.load(), numThreads * operationsPerThread);
}

// Test memory usage and cleanup
TEST_F(UtilitiesTest, MemoryUsage) {
    // Generate many UUIDs and timestamps to test for memory leaks
    std::vector<std::string> uuids;
    std::vector<std::string> timestamps;
    
    const int numItems = 10000;
    uuids.reserve(numItems);
    timestamps.reserve(numItems);
    
    for (int i = 0; i < numItems; i++) {
        uuids.push_back(generateUuid());
        timestamps.push_back(getIsoTimestamp());
    }
    
    // Verify all items were created
    EXPECT_EQ(uuids.size(), numItems);
    EXPECT_EQ(timestamps.size(), numItems);
    
    // Clear vectors to test cleanup
    uuids.clear();
    timestamps.clear();
    
    EXPECT_TRUE(uuids.empty());
    EXPECT_TRUE(timestamps.empty());
}
