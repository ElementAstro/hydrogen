#pragma once

#include "websocket_device.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>
#include <utility>
#include <chrono>

namespace hydrogen {
namespace device {

/**
 * @class Telescope
 * @brief Telescope device implementation
 * 
 * This class provides functionality for controlling astronomical telescopes,
 * including positioning, tracking, and coordinate management.
 */
class Telescope : public WebSocketDevice {
public:
    /**
     * @brief Constructor
     * @param deviceId Unique device identifier
     * @param manufacturer Device manufacturer (default: "Generic")
     * @param model Device model (default: "Telescope")
     */
    Telescope(const std::string& deviceId,
              const std::string& manufacturer = "Generic",
              const std::string& model = "Telescope");

    virtual ~Telescope();

    // Override WebSocketDevice methods
    bool start() override;
    void stop() override;

    /**
     * @brief Go to specified coordinates
     * @param ra Right ascension in hours
     * @param dec Declination in degrees
     */
    virtual void gotoPosition(double ra, double dec);

    /**
     * @brief Set tracking state
     * @param enabled True to enable tracking, false to disable
     */
    virtual void setTracking(bool enabled);

    /**
     * @brief Set slew rate
     * @param rate Slew rate (0-9, where 9 is fastest)
     */
    virtual void setSlewRate(int rate);

    /**
     * @brief Abort current movement
     */
    virtual void abort();

    /**
     * @brief Park the telescope
     */
    virtual void park();

    /**
     * @brief Unpark the telescope
     */
    virtual void unpark();

    /**
     * @brief Sync telescope position to specified coordinates
     * @param ra Right ascension in hours
     * @param dec Declination in degrees
     */
    virtual void sync(double ra, double dec);

    /**
     * @brief Set observer location
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     */
    virtual void setObserverLocation(double latitude, double longitude);

    /**
     * @brief Get current position
     * @return Pair of (RA, Dec) in hours and degrees
     */
    virtual std::pair<double, double> getPosition() const;

    /**
     * @brief Get current altitude/azimuth
     * @return Pair of (Alt, Az) in degrees
     */
    virtual std::pair<double, double> getAltAz() const;

    /**
     * @brief Check if telescope is tracking
     * @return True if tracking is enabled
     */
    virtual bool isTracking() const;

    /**
     * @brief Check if telescope is parked
     * @return True if parked
     */
    virtual bool isParked() const;

    /**
     * @brief Check if telescope is moving
     * @return True if currently slewing
     */
    virtual bool isMoving() const;

protected:
    /**
     * @brief Initialize telescope-specific properties and commands
     */
    void initializeTelescopeProperties();

    /**
     * @brief Register telescope-specific command handlers
     */
    void registerTelescopeCommands();

    /**
     * @brief Update simulation loop (for simulated telescopes)
     */
    virtual void updateLoop();

    /**
     * @brief Get current Local Sidereal Time
     * @return LST in hours
     */
    virtual double getCurrentLST() const;

    /**
     * @brief Update altitude/azimuth from RA/Dec
     */
    virtual void updateAltAz();

    /**
     * @brief Update RA/Dec from altitude/azimuth
     */
    virtual void updateRaDec();

    /**
     * @brief Calculate angular separation between two coordinates
     * @param ra1 Right ascension 1 in hours
     * @param dec1 Declination 1 in degrees
     * @param ra2 Right ascension 2 in hours
     * @param dec2 Declination 2 in degrees
     * @return Angular separation in degrees
     */
    virtual double calculateAngularSeparation(double ra1, double dec1, double ra2, double dec2) const;

    /**
     * @brief Calculate slew time estimate
     * @param targetRA Target right ascension in hours
     * @param targetDec Target declination in degrees
     * @return Estimated slew time in seconds
     */
    virtual double calculateSlewTime(double targetRA, double targetDec) const;

    /**
     * @brief Check if coordinates are within limits
     * @param ra Right ascension in hours
     * @param dec Declination in degrees
     * @return True if coordinates are safe
     */
    virtual bool areCoordinatesWithinLimits(double ra, double dec) const;

    /**
     * @brief Start the update thread for simulation
     */
    void startUpdateThread();

    /**
     * @brief Stop the update thread
     */
    void stopUpdateThread();

    /**
     * @brief Update position smoothly towards target
     */
    void updateSlewingPosition();

    /**
     * @brief Send position update event
     */
    void sendPositionUpdate();

    // Position and state
    mutable std::mutex positionMutex_;
    double currentRA_;          // Right ascension in hours
    double currentDec_;         // Declination in degrees
    double currentAlt_;         // Altitude in degrees
    double currentAz_;          // Azimuth in degrees

    double targetRA_;           // Target RA for slewing
    double targetDec_;          // Target Dec for slewing

    // Observer location
    double observerLatitude_;   // Latitude in degrees
    double observerLongitude_;  // Longitude in degrees
    double observerElevation_;  // Elevation in meters

    // State flags
    std::atomic<bool> tracking_;
    std::atomic<bool> parked_;
    std::atomic<bool> moving_;
    std::atomic<int> slewRate_;

    // Enhanced slewing parameters
    double slewRateRA_;         // RA slew rate in degrees/second
    double slewRateDec_;        // Dec slew rate in degrees/second
    double maxSlewRate_;        // Maximum slew rate
    double minSlewRate_;        // Minimum slew rate

    // Coordinate limits
    double minAltitude_;        // Minimum altitude limit in degrees
    double maxAltitude_;        // Maximum altitude limit in degrees
    double minAzimuth_;         // Minimum azimuth limit in degrees
    double maxAzimuth_;         // Maximum azimuth limit in degrees

    // Timing and performance
    std::chrono::steady_clock::time_point lastUpdateTime_;
    std::chrono::steady_clock::time_point slewStartTime_;
    double updateIntervalMs_;   // Update interval in milliseconds

    // Update thread for simulation
    std::thread updateThread_;
    std::atomic<bool> updateThreadRunning_;

    // Position history for smoothing
    struct PositionHistory {
        double ra, dec, alt, az;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<PositionHistory> positionHistory_;
    size_t maxHistorySize_;

    // Simulation parameters
    bool simulationMode_;
    double simulationAccuracy_; // Simulation accuracy factor
};

} // namespace device
} // namespace hydrogen
