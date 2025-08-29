#include "simple_helpers.h"
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace astrocomm {
namespace test {

std::string SimpleHelpers::generateTestId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    return "test_" + std::to_string(dis(gen));
}

std::string SimpleHelpers::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return ss.str();
}

bool SimpleHelpers::waitForCondition(std::function<bool()> condition, 
                                    int timeoutMs, 
                                    int intervalMs) {
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);
    
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
    
    return false;
}

std::string SimpleHelpers::createTempDirectory() {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return "temp_test_" + std::to_string(timestamp);
}

void SimpleHelpers::cleanupTempDirectory(const std::string& path) {
    // Simple cleanup - in a real implementation you'd use filesystem operations
    // For now, just a placeholder
}

// Base test class implementation
void SimpleTestBase::SetUp() {
    testId = SimpleHelpers::generateTestId();
    startTime = std::chrono::steady_clock::now();
}

void SimpleTestBase::TearDown() {
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    
    // Log test duration if needed
    // std::cout << "Test " << testId << " took " << duration.count() << "ms" << std::endl;
}

} // namespace test
} // namespace astrocomm
