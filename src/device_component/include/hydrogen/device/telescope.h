#pragma once

#include "websocket_device.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

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
     * @brief Start the update thread for simulation
     */
    void startUpdateThread();

    /**
     * @brief Stop the update thread
     */
    void stopUpdateThread();

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
    
    // State flags
    std::atomic<bool> tracking_;
    std::atomic<bool> parked_;
    std::atomic<bool> moving_;
    std::atomic<int> slewRate_;
    
    // Update thread for simulation
    std::thread updateThread_;
    std::atomic<bool> updateThreadRunning_;
};

} // namespace device
} // namespace hydrogen
