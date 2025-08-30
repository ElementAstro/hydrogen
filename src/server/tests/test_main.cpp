#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>

/**
 * @brief Main test entry point for AstroComm Server tests
 */
int main(int argc, char** argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Configure logging for tests (use null sink to avoid spam)
    auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("test_logger", null_sink);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::off);
    
    // Print test banner
    std::cout << "========================================" << std::endl;
    std::cout << "  AstroComm Server Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Run all tests
    int result = RUN_ALL_TESTS();
    
    // Print results summary
    std::cout << "========================================" << std::endl;
    if (result == 0) {
        std::cout << "  All tests PASSED!" << std::endl;
    } else {
        std::cout << "  Some tests FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;
    
    return result;
}
