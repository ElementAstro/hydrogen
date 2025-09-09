#include "solver.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>

namespace hydrogen {
namespace device {

/**
 * @brief Concrete implementation of the Plate Solver
 */
class SolverImpl : public ISolver {
public:
    explicit SolverImpl(const std::string& deviceId)
        : deviceId_(deviceId), connected_(false), solving_(false) {
        spdlog::info("Plate solver created: {}", deviceId_);
        
        // Initialize solver configuration with defaults
        config_.timeout = 30; // 30 seconds
        config_.searchRadius = 15.0; // 15 degrees
        config_.minStars = 10;
        config_.maxStars = 500;
        config_.pixelScale = 1.0; // arcsec/pixel
        config_.focalLength = 1000.0; // mm
        config_.useIndex = true;
        config_.downsample = 2;
    }

    ~SolverImpl() {
        disconnect();
    }

    // Device interface implementation
    std::string getDeviceId() const override {
        return deviceId_;
    }

    std::string getDeviceType() const override {
        return "Solver";
    }

    bool connect() override {
        if (connected_) {
            spdlog::warn("Solver already connected: {}", deviceId_);
            return true;
        }

        try {
            // Simulate solver connection
            connected_ = true;
            spdlog::info("Solver connected: {}", deviceId_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to connect solver {}: {}", deviceId_, e.what());
            return false;
        }
    }

    bool disconnect() override {
        if (!connected_) {
            return true;
        }

        try {
            // Stop any ongoing solve
            if (solving_) {
                stopSolve();
            }

            connected_ = false;
            spdlog::info("Solver disconnected: {}", deviceId_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to disconnect solver {}: {}", deviceId_, e.what());
            return false;
        }
    }

    bool isConnected() const override {
        return connected_;
    }

    DeviceStatus getStatus() const override {
        if (!connected_) {
            return DeviceStatus::DISCONNECTED;
        }
        if (solving_) {
            return DeviceStatus::BUSY;
        }
        return DeviceStatus::IDLE;
    }

    std::string getStatusString() const override {
        switch (getStatus()) {
            case DeviceStatus::DISCONNECTED: return "Disconnected";
            case DeviceStatus::IDLE: return "Idle";
            case DeviceStatus::BUSY: return "Solving";
            case DeviceStatus::DEVICE_ERROR: return "Error";
            default: return "Unknown";
        }
    }

    // Solver interface implementation
    bool startSolve(const SolveRequest& request) override {
        if (!connected_) {
            spdlog::error("Solver not connected: {}", deviceId_);
            return false;
        }

        if (solving_) {
            spdlog::warn("Solver already solving: {}", deviceId_);
            return false;
        }

        try {
            currentRequest_ = request;
            solving_ = true;
            
            // Start solve in background thread
            solveThread_ = std::thread(&SolverImpl::performSolve, this);
            
            spdlog::info("Plate solve started: {}", deviceId_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to start solve {}: {}", deviceId_, e.what());
            solving_ = false;
            return false;
        }
    }

    bool stopSolve() override {
        if (!solving_) {
            return true;
        }

        try {
            solving_ = false;
            
            if (solveThread_.joinable()) {
                solveThread_.join();
            }
            
            spdlog::info("Plate solve stopped: {}", deviceId_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to stop solve {}: {}", deviceId_, e.what());
            return false;
        }
    }

    bool isSolving() const override {
        return solving_;
    }

    // Additional device interface methods
    bool connect(const std::string& host, uint16_t port) override {
        // For solver, we can ignore host/port and just connect
        return connect();
    }

    bool start() override {
        return connect();
    }

    void stop() override {
        disconnect();
    }

    void run() override {
        // Main device loop - for solver this can be a simple wait loop
        while (connected_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool registerDevice() override {
        // For solver, registration is automatic on connect
        return connected_;
    }

    SolveResult getLastResult() const override {
        std::lock_guard<std::mutex> lock(resultMutex_);
        return lastResult_;
    }

    bool hasResult() const override {
        std::lock_guard<std::mutex> lock(resultMutex_);
        return lastResult_.success;
    }

    void clearResult() override {
        std::lock_guard<std::mutex> lock(resultMutex_);
        lastResult_ = SolveResult{};
        spdlog::debug("Solve result cleared: {}", deviceId_);
    }

    // Configuration
    SolverConfig getConfig() const override {
        return config_;
    }

    bool setConfig(const SolverConfig& config) override {
        if (solving_) {
            spdlog::error("Cannot change config while solving: {}", deviceId_);
            return false;
        }

        config_ = config;
        spdlog::info("Solver config updated: {}", deviceId_);
        return true;
    }

    bool setTimeout(int timeoutSeconds) override {
        config_.timeout = timeoutSeconds;
        spdlog::debug("Solver timeout set to {} seconds: {}", timeoutSeconds, deviceId_);
        return true;
    }

    int getTimeout() const override {
        return config_.timeout;
    }

    bool setSearchRadius(double radiusDegrees) override {
        config_.searchRadius = radiusDegrees;
        spdlog::debug("Solver search radius set to {} degrees: {}", radiusDegrees, deviceId_);
        return true;
    }

    double getSearchRadius() const override {
        return config_.searchRadius;
    }

    // Star detection
    bool setStarLimits(int minStars, int maxStars) override {
        config_.minStars = minStars;
        config_.maxStars = maxStars;
        spdlog::debug("Solver star limits set to {}-{}: {}", minStars, maxStars, deviceId_);
        return true;
    }

    bool setStarDetectionLimits(int minStars, int maxStars) override {
        return setStarLimits(minStars, maxStars);
    }

    std::pair<int, int> getStarDetectionLimits() const override {
        return {config_.minStars, config_.maxStars};
    }

    int getMinStars() const override {
        return config_.minStars;
    }

    int getMaxStars() const override {
        return config_.maxStars;
    }

    bool setPixelScale(double arcsecPerPixel) override {
        config_.pixelScale = arcsecPerPixel;
        spdlog::debug("Solver pixel scale set to {} arcsec/pixel: {}", arcsecPerPixel, deviceId_);
        return true;
    }

    double getPixelScale() const override {
        return config_.pixelScale;
    }

    // Optical parameters
    bool setFocalLength(double focalLengthMm) override {
        config_.focalLength = focalLengthMm;
        spdlog::debug("Solver focal length set to {} mm: {}", focalLengthMm, deviceId_);
        return true;
    }

    double getFocalLength() const override {
        return config_.focalLength;
    }

    bool setDownsample(int factor) override {
        config_.downsample = factor;
        spdlog::debug("Solver downsample set to {}: {}", factor, deviceId_);
        return true;
    }

    int getDownsample() const override {
        return config_.downsample;
    }

    // Index management
    bool useIndex(bool enabled) override {
        config_.useIndex = enabled;
        spdlog::debug("Solver index usage {}: {}", enabled ? "enabled" : "disabled", deviceId_);
        return true;
    }

    bool isUsingIndex() const override {
        return config_.useIndex;
    }

    std::vector<std::string> getAvailableIndexes() const override {
        // Simulate available index files
        return {
            "index-4200-00.fits",
            "index-4200-01.fits",
            "index-4200-02.fits",
            "index-4201-00.fits",
            "index-4201-01.fits"
        };
    }

    bool loadIndex(const std::string& indexPath) override {
        if (solving_) {
            spdlog::error("Cannot load index while solving: {}", deviceId_);
            return false;
        }

        // Simulate index loading
        loadedIndexes_.push_back(indexPath);
        spdlog::info("Index loaded: {} for solver {}", indexPath, deviceId_);
        return true;
    }

    bool unloadIndex(const std::string& indexPath) override {
        if (solving_) {
            spdlog::error("Cannot unload index while solving: {}", deviceId_);
            return false;
        }

        auto it = std::find(loadedIndexes_.begin(), loadedIndexes_.end(), indexPath);
        if (it != loadedIndexes_.end()) {
            loadedIndexes_.erase(it);
            spdlog::info("Index unloaded: {} for solver {}", indexPath, deviceId_);
            return true;
        }

        spdlog::warn("Index not found for unloading: {} for solver {}", indexPath, deviceId_);
        return false;
    }

    void clearIndexes() override {
        if (solving_) {
            spdlog::error("Cannot clear indexes while solving: {}", deviceId_);
            return;
        }

        loadedIndexes_.clear();
        spdlog::info("All indexes cleared: {}", deviceId_);
    }

    void unloadAllIndexes() override {
        if (solving_) {
            spdlog::error("Cannot unload indexes while solving: {}", deviceId_);
            return;
        }

        loadedIndexes_.clear();
        spdlog::info("All indexes unloaded: {}", deviceId_);
    }

    std::vector<std::string> getLoadedIndexes() const override {
        return loadedIndexes_;
    }

    // Statistics
    SolverStatistics getStatistics() const override {
        return statistics_;
    }

    void resetStatistics() override {
        statistics_ = SolverStatistics{};
        spdlog::debug("Solver statistics reset: {}", deviceId_);
    }

    // Calibration and blind solving
    bool performBlindSolve(const std::string& imagePath) override {
        SolveRequest request;
        request.imagePath = imagePath;
        request.useHint = false;
        request.blindSolve = true;
        
        return startSolve(request);
    }

    bool performHintedSolve(const std::string& imagePath, double raHours, double decDegrees) override {
        SolveRequest request;
        request.imagePath = imagePath;
        request.useHint = true;
        request.hintRA = raHours;
        request.hintDec = decDegrees;
        request.blindSolve = false;
        
        return startSolve(request);
    }

    bool calibrateFromImage(const std::string& imagePath, double knownRA, double knownDec) override {
        // Simulate calibration process
        spdlog::info("Calibrating solver from image: {} at RA={}, Dec={}", imagePath, knownRA, knownDec);
        
        // Update pixel scale based on calibration
        config_.pixelScale = 1.2; // Simulated calibrated value
        
        statistics_.calibrationCount++;
        return true;
    }

private:
    std::string deviceId_;
    std::atomic<bool> connected_;
    std::atomic<bool> solving_;
    
    SolverConfig config_;
    SolveRequest currentRequest_;
    SolveResult lastResult_;
    SolverStatistics statistics_;
    std::vector<std::string> loadedIndexes_;
    
    std::thread solveThread_;
    mutable std::mutex resultMutex_;

    void performSolve() {
        auto startTime = std::chrono::steady_clock::now();
        
        try {
            spdlog::info("Starting plate solve: {}", deviceId_);
            
            // Simulate solve process
            SolveResult result;
            result.success = simulateSolveProcess();
            result.solveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            
            if (result.success) {
                // Generate simulated solution
                generateSimulatedSolution(result);
                statistics_.successfulSolves++;
                spdlog::info("Plate solve successful: {} ({}ms)", deviceId_, result.solveTime);
            } else {
                statistics_.failedSolves++;
                result.errorMessage = "Simulated solve failure";
                spdlog::warn("Plate solve failed: {}", deviceId_);
            }
            
            // Store result
            {
                std::lock_guard<std::mutex> lock(resultMutex_);
                lastResult_ = result;
            }
            
            statistics_.totalSolves++;
            statistics_.averageSolveTime = (statistics_.averageSolveTime * (statistics_.totalSolves - 1) + 
                                          result.solveTime) / statistics_.totalSolves;
            
        } catch (const std::exception& e) {
            spdlog::error("Error during solve: {}", e.what());
            
            SolveResult result;
            result.success = false;
            result.errorMessage = e.what();
            result.solveTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            
            {
                std::lock_guard<std::mutex> lock(resultMutex_);
                lastResult_ = result;
            }
            
            statistics_.failedSolves++;
            statistics_.totalSolves++;
        }
        
        solving_ = false;
    }

    bool simulateSolveProcess() {
        // Simulate solve process with timeout
        auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(config_.timeout);
        
        while (solving_ && std::chrono::steady_clock::now() < endTime) {
            // Simulate processing steps
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!solving_) {
            return false; // Cancelled
        }
        
        // Simulate success rate based on configuration
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        
        double successRate = 0.8; // 80% base success rate
        if (currentRequest_.useHint) {
            successRate = 0.95; // Higher success rate with hints
        }
        if (config_.useIndex) {
            successRate += 0.1; // Index improves success rate
        }
        
        return dis(gen) < successRate;
    }

    void generateSimulatedSolution(SolveResult& result) {
        // Generate simulated plate solution
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> raDis(0.0, 24.0);
        std::uniform_real_distribution<> decDis(-90.0, 90.0);
        std::uniform_real_distribution<> rotDis(0.0, 360.0);
        std::uniform_real_distribution<> scaleDis(0.8, 1.2);
        
        if (currentRequest_.useHint) {
            // Use hint as base with small variation
            std::normal_distribution<> hintVar(0.0, 0.1);
            result.centerRA = currentRequest_.hintRA + hintVar(gen);
            result.centerDec = currentRequest_.hintDec + hintVar(gen);
        } else {
            // Random solution for blind solve
            result.centerRA = raDis(gen);
            result.centerDec = decDis(gen);
        }
        
        result.rotation = rotDis(gen);
        result.pixelScale = config_.pixelScale * scaleDis(gen);
        result.fieldWidth = 1920 * result.pixelScale / 3600.0; // degrees
        result.fieldHeight = 1080 * result.pixelScale / 3600.0; // degrees
        result.starsDetected = 50 + (gen() % 100); // 50-150 stars
        result.starsMatched = result.starsDetected * 0.7; // 70% matched
    }
};

// Factory function implementation
std::unique_ptr<ISolver> SolverFactory::createSolver(const std::string& deviceId) {
    return std::make_unique<SolverImpl>(deviceId);
}

std::unique_ptr<ISolver> SolverFactory::createSolver(const std::string& deviceId, const SolverConfig& config) {
    auto solver = std::make_unique<SolverImpl>(deviceId);
    solver->setConfig(config);
    return solver;
}

} // namespace device
} // namespace hydrogen
