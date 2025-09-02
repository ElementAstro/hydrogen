#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

namespace hydrogen {
namespace device {

/**
 * @brief Device status enumeration
 */
enum class DeviceStatus {
    DISCONNECTED,
    IDLE,
    BUSY,
    DEVICE_ERROR
};

/**
 * @brief Solver configuration structure
 */
struct SolverConfig {
    int timeout = 30;                    // Timeout in seconds
    double searchRadius = 15.0;          // Search radius in degrees
    int minStars = 10;                   // Minimum stars required
    int maxStars = 500;                  // Maximum stars to use
    double pixelScale = 1.0;             // Pixel scale in arcsec/pixel
    double focalLength = 1000.0;         // Focal length in mm
    bool useIndex = true;                // Use star index
    int downsample = 2;                  // Downsample factor
};

/**
 * @brief Solve request structure
 */
struct SolveRequest {
    std::string imagePath;               // Path to image file
    bool useHint = false;                // Use position hint
    double hintRA = 0.0;                 // RA hint in hours
    double hintDec = 0.0;                // Dec hint in degrees
    bool blindSolve = false;             // Perform blind solve
};

/**
 * @brief Solve result structure
 */
struct SolveResult {
    bool success = false;                // Solve successful
    std::string errorMessage;            // Error message if failed
    double centerRA = 0.0;               // Center RA in hours
    double centerDec = 0.0;              // Center Dec in degrees
    double rotation = 0.0;               // Rotation in degrees
    double pixelScale = 0.0;             // Pixel scale in arcsec/pixel
    double fieldWidth = 0.0;             // Field width in degrees
    double fieldHeight = 0.0;            // Field height in degrees
    int starsDetected = 0;               // Number of stars detected
    int starsMatched = 0;                // Number of stars matched
    long solveTime = 0;                  // Solve time in milliseconds
};

/**
 * @brief Solver statistics structure
 */
struct SolverStatistics {
    int totalSolves = 0;                 // Total solve attempts
    int successfulSolves = 0;            // Successful solves
    int failedSolves = 0;                // Failed solves
    double averageSolveTime = 0.0;       // Average solve time in ms
    int calibrationCount = 0;            // Number of calibrations performed
};

/**
 * @brief Plate solver interface
 */
class ISolver {
public:
    virtual ~ISolver() = default;

    // Device interface
    virtual std::string getDeviceId() const = 0;
    virtual std::string getDeviceType() const = 0;
    virtual bool connect() = 0;
    virtual bool connect(const std::string& host, uint16_t port) = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual DeviceStatus getStatus() const = 0;
    virtual std::string getStatusString() const = 0;

    // Device lifecycle
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void run() = 0;
    virtual bool registerDevice() = 0;

    // Solver interface
    virtual bool startSolve(const SolveRequest& request) = 0;
    virtual bool stopSolve() = 0;
    virtual bool isSolving() const = 0;
    virtual SolveResult getLastResult() const = 0;
    virtual bool hasResult() const = 0;
    virtual void clearResult() = 0;

    // Configuration
    virtual SolverConfig getConfig() const = 0;
    virtual bool setConfig(const SolverConfig& config) = 0;
    virtual bool setTimeout(int timeoutSeconds) = 0;
    virtual int getTimeout() const = 0;
    virtual bool setSearchRadius(double radiusDegrees) = 0;
    virtual double getSearchRadius() const = 0;
    virtual bool setStarLimits(int minStars, int maxStars) = 0;
    virtual bool setStarDetectionLimits(int minStars, int maxStars) = 0;
    virtual std::pair<int, int> getStarDetectionLimits() const = 0;
    virtual int getMinStars() const = 0;
    virtual int getMaxStars() const = 0;
    virtual bool setPixelScale(double arcsecPerPixel) = 0;
    virtual double getPixelScale() const = 0;
    virtual bool setFocalLength(double focalLengthMm) = 0;
    virtual double getFocalLength() const = 0;
    virtual bool setDownsample(int factor) = 0;
    virtual int getDownsample() const = 0;

    // Index management
    virtual bool useIndex(bool enabled) = 0;
    virtual bool isUsingIndex() const = 0;
    virtual bool loadIndex(const std::string& indexPath) = 0;
    virtual bool unloadIndex(const std::string& indexPath) = 0;
    virtual std::vector<std::string> getLoadedIndexes() const = 0;
    virtual std::vector<std::string> getAvailableIndexes() const = 0;
    virtual void clearIndexes() = 0;
    virtual void unloadAllIndexes() = 0;

    // Statistics
    virtual SolverStatistics getStatistics() const = 0;
    virtual void resetStatistics() = 0;

    // Calibration and blind solving
    virtual bool performBlindSolve(const std::string& imagePath) = 0;
    virtual bool performHintedSolve(const std::string& imagePath, double raHours, double decDegrees) = 0;
    virtual bool calibrateFromImage(const std::string& imagePath, double knownRA, double knownDec) = 0;
};

/**
 * @brief Solver factory class
 */
class SolverFactory {
public:
    static std::unique_ptr<ISolver> createSolver(const std::string& deviceId);
    static std::unique_ptr<ISolver> createSolver(const std::string& deviceId, const SolverConfig& config);
};

} // namespace device
} // namespace hydrogen
