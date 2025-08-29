#include <gtest/gtest.h>
#include <astrocomm/core/utils.h>
#include "test_helpers.h"
#include <regex>

using namespace astrocomm::core;
using namespace astrocomm::test;

class UtilsTest : public AstroCommTestBase {
protected:
    void SetUp() override {
        AstroCommTestBase::SetUp();
    }
};

// Test UUID generation
TEST_F(UtilsTest, UuidGeneration) {
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
    
    // UUIDs should have correct format (basic check)
    EXPECT_EQ(uuid1.length(), 36); // Standard UUID length with hyphens
    EXPECT_EQ(uuid1[8], '-');
    EXPECT_EQ(uuid1[13], '-');
    EXPECT_EQ(uuid1[18], '-');
    EXPECT_EQ(uuid1[23], '-');
}

// Test timestamp generation and parsing
TEST_F(UtilsTest, TimestampHandling) {
    // Generate current timestamp
    std::string timestamp = getCurrentTimestamp();
    
    EXPECT_FALSE(timestamp.empty());
    
    // Timestamp should be in ISO 8601 format
    // Basic format check: YYYY-MM-DDTHH:MM:SS.sssZ
    std::regex iso8601_regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z)");
    EXPECT_TRUE(std::regex_match(timestamp, iso8601_regex));
    
    // Parse timestamp
    auto parsed = parseTimestamp(timestamp);
    EXPECT_TRUE(parsed.has_value());
    
    // Test invalid timestamp
    auto invalid = parseTimestamp("invalid-timestamp");
    EXPECT_FALSE(invalid.has_value());
}

// Test string trimming
TEST_F(UtilsTest, StringTrimming) {
    // Test left trim
    EXPECT_EQ(ltrim("   hello"), "hello");
    EXPECT_EQ(ltrim("\t\n  hello"), "hello");
    EXPECT_EQ(ltrim("hello"), "hello");
    EXPECT_EQ(ltrim(""), "");
    
    // Test right trim
    EXPECT_EQ(rtrim("hello   "), "hello");
    EXPECT_EQ(rtrim("hello  \t\n"), "hello");
    EXPECT_EQ(rtrim("hello"), "hello");
    EXPECT_EQ(rtrim(""), "");
    
    // Test full trim
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\t\n hello \t\n"), "hello");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   "), "");
}

// Test string case conversion
TEST_F(UtilsTest, StringCaseConversion) {
    // Test to lowercase
    EXPECT_EQ(toLowerCase("HELLO"), "hello");
    EXPECT_EQ(toLowerCase("Hello World"), "hello world");
    EXPECT_EQ(toLowerCase("MiXeD cAsE"), "mixed case");
    EXPECT_EQ(toLowerCase(""), "");
    EXPECT_EQ(toLowerCase("123"), "123");
    
    // Test to uppercase
    EXPECT_EQ(toUpperCase("hello"), "HELLO");
    EXPECT_EQ(toUpperCase("Hello World"), "HELLO WORLD");
    EXPECT_EQ(toUpperCase("MiXeD cAsE"), "MIXED CASE");
    EXPECT_EQ(toUpperCase(""), "");
    EXPECT_EQ(toUpperCase("123"), "123");
}

// Test string splitting
TEST_F(UtilsTest, StringSplitting) {
    // Test basic splitting
    auto result = split("hello,world,test", ",");
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
    EXPECT_EQ(result[2], "test");
    
    // Test splitting with multiple delimiters
    result = split("hello,,world", ",");
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "");
    EXPECT_EQ(result[2], "world");
    
    // Test splitting with no delimiter
    result = split("hello", ",");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "hello");
    
    // Test empty string
    result = split("", ",");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "");
    
    // Test different delimiter
    result = split("hello|world|test", "|");
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
    EXPECT_EQ(result[2], "test");
}

// Test boolean parsing
TEST_F(UtilsTest, BooleanParsing) {
    // Test true values
    EXPECT_TRUE(parseBool("true"));
    EXPECT_TRUE(parseBool("TRUE"));
    EXPECT_TRUE(parseBool("True"));
    EXPECT_TRUE(parseBool("1"));
    EXPECT_TRUE(parseBool("yes"));
    EXPECT_TRUE(parseBool("YES"));
    EXPECT_TRUE(parseBool("on"));
    EXPECT_TRUE(parseBool("ON"));
    
    // Test false values
    EXPECT_FALSE(parseBool("false"));
    EXPECT_FALSE(parseBool("FALSE"));
    EXPECT_FALSE(parseBool("False"));
    EXPECT_FALSE(parseBool("0"));
    EXPECT_FALSE(parseBool("no"));
    EXPECT_FALSE(parseBool("NO"));
    EXPECT_FALSE(parseBool("off"));
    EXPECT_FALSE(parseBool("OFF"));
    
    // Test invalid values (should default to false)
    EXPECT_FALSE(parseBool("invalid"));
    EXPECT_FALSE(parseBool(""));
    EXPECT_FALSE(parseBool("maybe"));
    EXPECT_FALSE(parseBool("2"));
}

// Test string replacement
TEST_F(UtilsTest, StringReplacement) {
    // Test basic replacement
    EXPECT_EQ(replaceAll("hello world", "world", "universe"), "hello universe");
    EXPECT_EQ(replaceAll("test test test", "test", "exam"), "exam exam exam");
    
    // Test no replacement needed
    EXPECT_EQ(replaceAll("hello world", "foo", "bar"), "hello world");
    
    // Test empty strings
    EXPECT_EQ(replaceAll("", "foo", "bar"), "");
    EXPECT_EQ(replaceAll("hello", "", "bar"), "hello");
    
    // Test replacement with empty string
    EXPECT_EQ(replaceAll("hello world", "world", ""), "hello ");
}

// Test string joining
TEST_F(UtilsTest, StringJoining) {
    // Test basic joining
    std::vector<std::string> parts = {"hello", "world", "test"};
    EXPECT_EQ(join(parts, ","), "hello,world,test");
    EXPECT_EQ(join(parts, " "), "hello world test");
    EXPECT_EQ(join(parts, ""), "helloworldtest");
    
    // Test empty vector
    std::vector<std::string> empty;
    EXPECT_EQ(join(empty, ","), "");
    
    // Test single element
    std::vector<std::string> single = {"hello"};
    EXPECT_EQ(join(single, ","), "hello");
}

// Test string validation
TEST_F(UtilsTest, StringValidation) {
    // Test if string is numeric
    EXPECT_TRUE(isNumeric("123"));
    EXPECT_TRUE(isNumeric("123.456"));
    EXPECT_TRUE(isNumeric("-123"));
    EXPECT_TRUE(isNumeric("-123.456"));
    EXPECT_TRUE(isNumeric("0"));
    
    EXPECT_FALSE(isNumeric("abc"));
    EXPECT_FALSE(isNumeric("123abc"));
    EXPECT_FALSE(isNumeric(""));
    EXPECT_FALSE(isNumeric("12.34.56"));
    
    // Test if string is alphanumeric
    EXPECT_TRUE(isAlphanumeric("abc123"));
    EXPECT_TRUE(isAlphanumeric("ABC"));
    EXPECT_TRUE(isAlphanumeric("123"));
    EXPECT_TRUE(isAlphanumeric("Test123"));
    
    EXPECT_FALSE(isAlphanumeric("abc-123"));
    EXPECT_FALSE(isAlphanumeric("abc 123"));
    EXPECT_FALSE(isAlphanumeric(""));
    EXPECT_FALSE(isAlphanumeric("test@123"));
}

// Test URL encoding/decoding
TEST_F(UtilsTest, UrlEncoding) {
    // Test basic URL encoding
    EXPECT_EQ(urlEncode("hello world"), "hello%20world");
    EXPECT_EQ(urlEncode("test@example.com"), "test%40example.com");
    EXPECT_EQ(urlEncode("a+b=c"), "a%2Bb%3Dc");
    
    // Test URL decoding
    EXPECT_EQ(urlDecode("hello%20world"), "hello world");
    EXPECT_EQ(urlDecode("test%40example.com"), "test@example.com");
    EXPECT_EQ(urlDecode("a%2Bb%3Dc"), "a+b=c");
    
    // Test round trip
    std::string original = "Hello World! @#$%^&*()";
    std::string encoded = urlEncode(original);
    std::string decoded = urlDecode(encoded);
    EXPECT_EQ(original, decoded);
}
