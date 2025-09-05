#include <hydrogen/device/websocket_device.h>
#include <hydrogen/device/telescope.h>
#include <hydrogen/device/performance_monitor.h>
#include <hydrogen/device/device_logger.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <iomanip>

using namespace hydrogen::device;

class PerformanceBenchmark {
public:
    PerformanceBenchmark() {
        // Configure logging for benchmarks
        DeviceLogger::getInstance().setLogLevel(LogLevel::WARN);
        DeviceLogger::getInstance().setConsoleLogging(false);
    }

    void runAllBenchmarks() {
        std::cout << "=== Hydrogen Device Component Performance Benchmarks ===" << std::endl;
        std::cout << std::endl;

        benchmarkWebSocketDevice();
        benchmarkTelescope();
        benchmarkPerformanceMonitor();
        benchmarkDeviceLogger();

        std::cout << "=== Benchmark Summary ===" << std::endl;
        printSummary();
    }

private:
    struct BenchmarkResult {
        std::string name;
        double operationsPerSecond;
        double averageLatency;
        std::string unit;
    };

    std::vector<BenchmarkResult> results_;

    void addResult(const std::string& name, double opsPerSec, double avgLatency, const std::string& unit = "ops/sec") {
        results_.push_back({name, opsPerSec, avgLatency, unit});
    }

    void benchmarkWebSocketDevice() {
        std::cout << "--- WebSocket Device Benchmarks ---" << std::endl;

        auto device = std::make_unique<WebSocketDevice>("benchmark_ws", "benchmark", "Test", "WebSocket");
        device->start();

        // Benchmark message queuing
        const int messageCount = 10000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < messageCount; ++i) {
            device->sendMessage("benchmark message " + std::to_string(i));
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double opsPerSec = (messageCount * 1000000.0) / duration.count();
        double avgLatency = duration.count() / (double)messageCount;

        std::cout << "Message Queuing: " << std::fixed << std::setprecision(0) 
                  << opsPerSec << " messages/sec, " 
                  << std::setprecision(2) << avgLatency << " μs/message" << std::endl;

        addResult("WebSocket Message Queuing", opsPerSec, avgLatency, "messages/sec");

        // Benchmark connection statistics
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            auto stats = device->getConnectionStats();
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        opsPerSec = (1000 * 1000000.0) / duration.count();
        avgLatency = duration.count() / 1000.0;

        std::cout << "Statistics Retrieval: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " ops/sec, "
                  << std::setprecision(2) << avgLatency << " μs/op" << std::endl;

        addResult("WebSocket Stats Retrieval", opsPerSec, avgLatency);

        device->stop();
        std::cout << std::endl;
    }

    void benchmarkTelescope() {
        std::cout << "--- Telescope Benchmarks ---" << std::endl;

        auto telescope = std::make_unique<Telescope>("benchmark_telescope", "Test", "Benchmark");
        telescope->start();
        telescope->unpark();

        // Benchmark position queries
        const int positionQueries = 10000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < positionQueries; ++i) {
            auto pos = telescope->getPosition();
            auto altaz = telescope->getAltAz();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        double opsPerSec = (positionQueries * 2 * 1000000.0) / duration.count();
        double avgLatency = duration.count() / (positionQueries * 2.0);

        std::cout << "Position Queries: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " queries/sec, "
                  << std::setprecision(2) << avgLatency << " μs/query" << std::endl;

        addResult("Telescope Position Queries", opsPerSec, avgLatency, "queries/sec");

        // Benchmark coordinate calculations
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            double sep = telescope->calculateAngularSeparation(0.0, 0.0, 12.0, 45.0);
            double slewTime = telescope->calculateSlewTime(6.0, 30.0);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        opsPerSec = (1000 * 2 * 1000000.0) / duration.count();
        avgLatency = duration.count() / 2000.0;

        std::cout << "Coordinate Calculations: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " calculations/sec, "
                  << std::setprecision(2) << avgLatency << " μs/calculation" << std::endl;

        addResult("Telescope Coordinate Calculations", opsPerSec, avgLatency, "calculations/sec");

        // Benchmark state changes
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            telescope->setTracking(i % 2 == 0);
            telescope->setSlewRate((i % 9) + 1);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        opsPerSec = (1000 * 2 * 1000000.0) / duration.count();
        avgLatency = duration.count() / 2000.0;

        std::cout << "State Changes: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " changes/sec, "
                  << std::setprecision(2) << avgLatency << " μs/change" << std::endl;

        addResult("Telescope State Changes", opsPerSec, avgLatency, "changes/sec");

        telescope->stop();
        std::cout << std::endl;
    }

    void benchmarkPerformanceMonitor() {
        std::cout << "--- Performance Monitor Benchmarks ---" << std::endl;

        auto monitor = std::make_unique<PerformanceMonitor>("benchmark_monitor");

        // Benchmark metric recording
        const int metricCount = 100000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < metricCount; ++i) {
            monitor->recordMetric("test_metric", i);
            monitor->incrementCounter("test_counter");
            monitor->recordMemoryUsage(1024 + i);
            monitor->recordMessage(100, i % 2 == 0);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        double opsPerSec = (metricCount * 4 * 1000000.0) / duration.count();
        double avgLatency = duration.count() / (metricCount * 4.0);

        std::cout << "Metric Recording: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " metrics/sec, "
                  << std::setprecision(2) << avgLatency << " μs/metric" << std::endl;

        addResult("Performance Monitor Metrics", opsPerSec, avgLatency, "metrics/sec");

        // Benchmark timing operations
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            monitor->startTiming("benchmark_op");
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            monitor->endTiming("benchmark_op");
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        opsPerSec = (1000 * 1000000.0) / duration.count();
        avgLatency = duration.count() / 1000.0;

        std::cout << "Timing Operations: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " timings/sec, "
                  << std::setprecision(2) << avgLatency << " μs/timing" << std::endl;

        addResult("Performance Monitor Timing", opsPerSec, avgLatency, "timings/sec");

        // Benchmark disabled monitoring
        monitor->setEnabled(false);
        start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < metricCount; ++i) {
            monitor->recordMetric("disabled_metric", i);
            monitor->incrementCounter("disabled_counter");
        }

        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        opsPerSec = (metricCount * 2 * 1000000.0) / duration.count();
        avgLatency = duration.count() / (metricCount * 2.0);

        std::cout << "Disabled Monitoring: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " ops/sec, "
                  << std::setprecision(2) << avgLatency << " μs/op" << std::endl;

        addResult("Performance Monitor Disabled", opsPerSec, avgLatency);

        std::cout << std::endl;
    }

    void benchmarkDeviceLogger() {
        std::cout << "--- Device Logger Benchmarks ---" << std::endl;

        auto& logger = DeviceLogger::getInstance();
        logger.setConsoleLogging(false);
        logger.setLogFile("");
        logger.setLogLevel(LogLevel::INFO);

        std::atomic<int> messageCount{0};
        logger.setLogCallback([&messageCount](LogLevel, const std::string&, const std::string&) {
            messageCount++;
        });

        // Benchmark logging throughput
        const int logCount = 50000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < logCount; ++i) {
            logger.info("benchmark_device", "Benchmark log message " + std::to_string(i));
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        double opsPerSec = (logCount * 1000000.0) / duration.count();
        double avgLatency = duration.count() / (double)logCount;

        std::cout << "Logging Throughput: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " logs/sec, "
                  << std::setprecision(2) << avgLatency << " μs/log" << std::endl;

        addResult("Device Logger Throughput", opsPerSec, avgLatency, "logs/sec");

        // Benchmark filtered logging
        logger.setLogLevel(LogLevel::ERROR);
        messageCount = 0;

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < logCount; ++i) {
            logger.debug("benchmark_device", "Filtered debug message " + std::to_string(i));
            logger.info("benchmark_device", "Filtered info message " + std::to_string(i));
            if (i % 10 == 0) {
                logger.error("benchmark_device", "Error message " + std::to_string(i));
            }
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        opsPerSec = (logCount * 2.1 * 1000000.0) / duration.count();
        avgLatency = duration.count() / (logCount * 2.1);

        std::cout << "Filtered Logging: " << std::fixed << std::setprecision(0)
                  << opsPerSec << " logs/sec, "
                  << std::setprecision(2) << avgLatency << " μs/log" << std::endl;
        std::cout << "  (Filtered out " << (logCount * 2 - messageCount.load()) << " messages)" << std::endl;

        addResult("Device Logger Filtered", opsPerSec, avgLatency, "logs/sec");

        logger.setLogCallback(nullptr);
        std::cout << std::endl;
    }

    void printSummary() {
        std::cout << std::left << std::setw(35) << "Benchmark" 
                  << std::right << std::setw(15) << "Performance" 
                  << std::setw(15) << "Avg Latency" << std::endl;
        std::cout << std::string(65, '-') << std::endl;

        for (const auto& result : results_) {
            std::cout << std::left << std::setw(35) << result.name
                      << std::right << std::setw(12) << std::fixed << std::setprecision(0) << result.operationsPerSecond
                      << " " << std::setw(2) << (result.unit.find("sec") != std::string::npos ? result.unit.substr(0, result.unit.find("/")) : "ops")
                      << std::setw(12) << std::setprecision(2) << result.averageLatency << " μs"
                      << std::endl;
        }

        std::cout << std::endl;
        std::cout << "All benchmarks completed successfully!" << std::endl;
    }
};

int main() {
    try {
        PerformanceBenchmark benchmark;
        benchmark.runAllBenchmarks();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}
