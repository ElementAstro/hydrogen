#pragma once
#include "device/device_base.h"
#include <atomic>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace astrocomm {

/**
 * Solver state enumeration
 * Represents the various states a solver can be in during its operation
 */
enum class SolverState { IDLE, SOLVING, COMPLETE, FAILED };

/**
 * Exception class for solver-related errors
 * Used to report specific solver exceptions with detailed error messages
 */
class SolverException : public std::runtime_error {
public:
    explicit SolverException(const std::string& message) : std::runtime_error(message) {}
};

// Forward declarations
std::string formatRAToHMS(double ra);
std::string formatDecToDMS(double dec);
std::string getIsoTimestamp();
std::vector<uint8_t> base64_decode(const std::string& encoded_string);

/**
 * Astrometric plate solver device class
 * Base class for implementing astronomical image solvers.
 * Provides core functionality for astrometric plate solving including
 * image analysis, star pattern matching, and coordinate determination.
 */
class Solver : public DeviceBase {
public:
    /**
     * Constructor for Solver device
     * @param deviceId Unique identifier for this device
     * @param manufacturer Device manufacturer name
     * @param model Device model name
     */
    Solver(const std::string &deviceId,
           const std::string &manufacturer = "AstroCode",
           const std::string &model = "AstroSolver");
    
    /**
     * Virtual destructor to ensure proper cleanup in derived classes
     */
    virtual ~Solver();

    /**
     * Start the solver device
     * @return True if started successfully, false otherwise
     */
    virtual bool start() override;
    
    /**
     * Stop the solver device and clean up resources
     */
    virtual void stop() override;

    /**
     * Solve an image from raw data
     * @param imageData Raw image data buffer
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @throws SolverException if the solve cannot be started
     */
    virtual void solve(const std::vector<uint8_t> &imageData, int width, int height);
    
    /**
     * Solve an image from a file
     * @param filePath Path to the image file
     * @throws SolverException if the file doesn't exist or solve cannot be started
     */
    virtual void solveFromFile(const std::string &filePath);
    
    /**
     * Abort a running solve operation
     * Safely stops any solving process and cleans up resources
     */
    virtual void abort();
    
    /**
     * Update solver parameters
     * @param params JSON object containing parameters to update
     * @throws SolverException if parameters are invalid
     */
    virtual void setParameters(const json &params);
    
    /**
     * Set external solver path
     * @param path Path to external solver executable
     */
    virtual void setSolverPath(const std::string &path);
    
    /**
     * Set options for the external solver
     * @param options Map of option name to option value
     */
    virtual void setSolverOptions(const std::map<std::string, std::string> &options);
    
    /**
     * Get the last successful solution
     * @return JSON object containing solution data or empty if no solution
     */
    virtual json getLastSolution() const;
    
    /**
     * Get current solver state
     * @return Current state of the solver
     */
    virtual SolverState getState() const;
    
    /**
     * Get current solving progress (0-100)
     * @return Progress percentage
     */
    virtual int getProgress() const;

protected:
    // Main solve thread methods - can be overridden by derived classes
    
    /**
     * Main solving thread for raw image data
     * Orchestrates the solving process in a separate thread
     * 
     * @param imageData Raw image data to solve
     * @param width Image width
     * @param height Image height
     */
    virtual void solveThread(std::vector<uint8_t> imageData, int width, int height);

    /**
     * Main solving thread for image files
     * Orchestrates the solving process from file in a separate thread
     * 
     * @param filePath Path to image file
     */
    virtual void solveFileThread(std::string filePath);

    /**
     * Core implementation of plate solving algorithm
     * Override this method in derived classes to implement custom solving logic
     * 
     * @param imageData Image data buffer
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @return true if solve successful, false otherwise
     */
    virtual bool performSolve(const std::vector<uint8_t> &imageData, int width, int height);

    /**
     * Extract stars from an image
     * Default implementation provides basic star detection
     * Override in derived classes for advanced star extraction algorithms
     * 
     * @param imageData Image data buffer
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @return JSON array of detected stars with x,y coordinates and magnitudes
     */
    virtual json extractStars(const std::vector<uint8_t> &imageData, int width, int height);

    /**
     * Match star pattern against catalog
     * Default implementation provides basic pattern matching
     * Override in derived classes for advanced catalog matching algorithms
     * 
     * @param stars JSON array of detected stars
     * @return true if matching successful, false otherwise
     */
    virtual bool matchStarPattern(const json &stars);

    /**
     * Calculate image distortion parameters
     * Default implementation provides basic distortion calculation
     * Override in derived classes for advanced distortion analysis
     * 
     * @param stars JSON array of detected stars
     * @param matchedStars JSON array of catalog-matched stars
     * @return JSON object with distortion parameters
     */
    virtual json calculateDistortion(const json &stars, const json &matchedStars);

    /**
     * Generate solution data from solve results
     * Override this to customize solution format
     * 
     * @param success Whether the solve was successful
     * @return JSON object with solution data
     */
    virtual json generateSolution(bool success);

    /**
     * Send a solve completion event to clients
     * @param relatedMessageId Message ID of the original solve request
     */
    virtual void sendSolveCompletedEvent(const std::string &relatedMessageId);

    /**
     * Convert solver state enum to string representation
     * @param state The state to convert
     * @return String representation of the state
     */
    virtual std::string solverStateToString(SolverState state) const;

    /**
     * Check if the solver is in a particular state
     * Thread-safe method to check solver state
     * 
     * @param expectedState State to check against
     * @return true if current state matches expected state
     */
    virtual bool isInState(SolverState expectedState) const;

    /**
     * Update solver state with proper synchronization
     * 
     * @param newState New state to set
     * @param updateProgress Optional progress value to update simultaneously
     */
    virtual void updateState(SolverState newState, int updateProgress = -1);

    /**
     * Check if file format is supported for solving
     * 
     * @param filePath Path to the image file
     * @return true if format is supported, false otherwise
     */
    virtual bool isSupportedImageFormat(const std::string &filePath) const;

    // Command handlers - can be overridden by derived classes
    virtual void handleSolveCommand(const CommandMessage &cmd, ResponseMessage &response);
    virtual void handleSolveFileCommand(const CommandMessage &cmd, ResponseMessage &response);
    virtual void handleAbortCommand(const CommandMessage &cmd, ResponseMessage &response);
    virtual void handleSetParametersCommand(const CommandMessage &cmd, ResponseMessage &response);
    virtual void handleGetSolutionCommand(const CommandMessage &cmd, ResponseMessage &response);

    // Protected member variables - accessible to derived classes
    SolverState state;                 // Current solver state
    std::atomic<int> progress;         // Solve progress percentage (0-100)

    // Solver parameters
    double fovMin;                     // Minimum field of view (arcmin)
    double fovMax;                     // Maximum field of view (arcmin)
    double scaleMin;                   // Minimum pixel scale (arcsec/pixel)
    double scaleMax;                   // Maximum pixel scale (arcsec/pixel)
    bool useDistortion;                // Whether to use distortion correction
    int downsample;                    // Downsampling factor
    double raHint;                     // RA hint (hours)
    double decHint;                    // DEC hint (degrees)
    double radiusHint;                 // Search radius hint (degrees)

    // External solver settings
    std::string solverPath;            // Path to external solver executable
    std::map<std::string, std::string> solverOptions;  // Options for external solver

    // Solution results
    json lastSolution;                 // Last solution data in JSON format
    bool hasValidSolution;             // Whether we have a valid solution

    // Current solving task ID
    std::string currentSolveMessageId; // Message ID of current solve request

    // Thread safety
    mutable std::mutex statusMutex;    // Protect status updates
    std::mutex solveMutex;             // Protect solve operations
    
    // Random number generator for simulation
    std::mt19937 rng;                  // Mersenne Twister random number generator

    // Flag to signal thread termination
    std::atomic<bool> terminateThread;

private:
    // Solving thread object
    std::thread solveThreadObj;

    // Logger instance
    std::shared_ptr<spdlog::logger> logger_;

    // Register command handlers
    void registerCommandHandlers();

    // Helper methods for input validation
    bool validateSolveParameters(const json &params, ResponseMessage &response);
    bool validateFilePathParameter(const json &params, ResponseMessage &response);
};

} // namespace astrocomm