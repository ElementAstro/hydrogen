#include <hydrogen/device/telescope.h>
#include <hydrogen/core/utils.h>
#include <chrono>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace hydrogen {
namespace device {

Telescope::Telescope(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : WebSocketDevice(deviceId, "telescope", manufacturer, model),
      currentRA_(0.0), currentDec_(0.0), currentAlt_(0.0), currentAz_(0.0),
      targetRA_(0.0), targetDec_(0.0),
      observerLatitude_(0.0), observerLongitude_(0.0), observerElevation_(0.0),
      tracking_(false), parked_(true), moving_(false), slewRate_(5),
      slewRateRA_(1.0), slewRateDec_(1.0), maxSlewRate_(5.0), minSlewRate_(0.1),
      minAltitude_(-90.0), maxAltitude_(90.0), minAzimuth_(0.0), maxAzimuth_(360.0),
      updateIntervalMs_(100.0), updateThreadRunning_(false),
      maxHistorySize_(10), simulationMode_(true), simulationAccuracy_(1.0) {

    lastUpdateTime_ = std::chrono::steady_clock::now();
    slewStartTime_ = lastUpdateTime_;

    initializeTelescopeProperties();
    registerTelescopeCommands();
}

Telescope::~Telescope() {
    stop();
}

bool Telescope::start() {
    if (!WebSocketDevice::start()) {
        return false;
    }
    
    startUpdateThread();
    return true;
}

void Telescope::stop() {
    stopUpdateThread();
    WebSocketDevice::stop();
}

void Telescope::gotoPosition(double ra, double dec) {
    if (parked_) {
        throw std::runtime_error("Telescope is parked");
    }

    // Validate coordinates
    if (!areCoordinatesWithinLimits(ra, dec)) {
        throw std::invalid_argument("Target coordinates are outside safe limits");
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        targetRA_ = ra;
        targetDec_ = dec;
        slewStartTime_ = std::chrono::steady_clock::now();
    }

    moving_ = true;
    setProperty("moving", true);
    setProperty("target_ra", ra);
    setProperty("target_dec", dec);

    // Calculate estimated slew time
    double estimatedTime = calculateSlewTime(ra, dec);
    setProperty("estimated_slew_time", estimatedTime);

    // Send event
    hydrogen::core::EventMessage event("goto_started");
    event.setDeviceId(getDeviceId());
    event.setProperties({
        {"target_ra", ra},
        {"target_dec", dec},
        {"estimated_slew_time", estimatedTime}
    });
    sendEvent(event);
}

void Telescope::setTracking(bool enabled) {
    tracking_ = enabled;
    setProperty("tracking", enabled);
    
    hydrogen::core::EventMessage event("tracking_changed");
    event.setDeviceId(getDeviceId());
    event.setProperties({{"tracking", enabled}});
    sendEvent(event);
}

void Telescope::setSlewRate(int rate) {
    if (rate < 0 || rate > 9) {
        throw std::invalid_argument("Slew rate must be between 0 and 9");
    }
    
    slewRate_ = rate;
    setProperty("slew_rate", rate);
}

void Telescope::abort() {
    moving_ = false;
    setProperty("moving", false);
    
    hydrogen::core::EventMessage event("movement_aborted");
    event.setDeviceId(getDeviceId());
    sendEvent(event);
}

void Telescope::park() {
    // Move to park position (typically NCP for northern hemisphere)
    gotoPosition(0.0, 90.0);
    
    parked_ = true;
    tracking_ = false;
    setProperty("parked", true);
    setProperty("tracking", false);
    
    hydrogen::core::EventMessage event("parked");
    event.setDeviceId(getDeviceId());
    sendEvent(event);
}

void Telescope::unpark() {
    parked_ = false;
    setProperty("parked", false);
    
    hydrogen::core::EventMessage event("unparked");
    event.setDeviceId(getDeviceId());
    sendEvent(event);
}

void Telescope::sync(double ra, double dec) {
    if (parked_) {
        throw std::runtime_error("Cannot sync while parked");
    }
    
    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        currentRA_ = ra;
        currentDec_ = dec;
    }
    
    setProperty("ra", ra);
    setProperty("dec", dec);
    updateAltAz();
    
    hydrogen::core::EventMessage event("synced");
    event.setDeviceId(getDeviceId());
    event.setProperties({
        {"ra", ra},
        {"dec", dec}
    });
    sendEvent(event);
}

void Telescope::setObserverLocation(double latitude, double longitude) {
    observerLatitude_ = latitude;
    observerLongitude_ = longitude;
    setProperty("observer_latitude", latitude);
    setProperty("observer_longitude", longitude);
    
    updateAltAz(); // Recalculate alt/az with new location
}

std::pair<double, double> Telescope::getPosition() const {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return {currentRA_, currentDec_};
}

std::pair<double, double> Telescope::getAltAz() const {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return {currentAlt_, currentAz_};
}

bool Telescope::isTracking() const {
    return tracking_;
}

bool Telescope::isParked() const {
    return parked_;
}

bool Telescope::isMoving() const {
    return moving_;
}

void Telescope::initializeTelescopeProperties() {
    // Add telescope-specific capabilities
    addCapability("goto");
    addCapability("tracking");
    addCapability("parking");
    addCapability("syncing");
    addCapability("abort");
    
    // Initialize properties
    setProperty("ra", currentRA_);
    setProperty("dec", currentDec_);
    setProperty("alt", currentAlt_);
    setProperty("az", currentAz_);
    setProperty("target_ra", targetRA_);
    setProperty("target_dec", targetDec_);
    setProperty("tracking", tracking_.load());
    setProperty("parked", parked_.load());
    setProperty("moving", moving_.load());
    setProperty("slew_rate", slewRate_.load());
    setProperty("observer_latitude", observerLatitude_);
    setProperty("observer_longitude", observerLongitude_);
}

void Telescope::registerTelescopeCommands() {
    registerCommandHandler("goto",
        [this](const hydrogen::core::CommandMessage& cmd, hydrogen::core::ResponseMessage& response) {
            auto params = cmd.getParameters();
            if (!params.contains("ra") || !params.contains("dec")) {
                throw std::invalid_argument("Missing ra or dec parameter");
            }
            
            double ra = params["ra"];
            double dec = params["dec"];
            gotoPosition(ra, dec);
            
            response.setDetails({
                {"message", "Goto command initiated"},
                {"target_ra", ra},
                {"target_dec", dec}
            });
        });
    
    registerCommandHandler("set_tracking",
        [this](const hydrogen::core::CommandMessage& cmd, hydrogen::core::ResponseMessage& response) {
            auto params = cmd.getParameters();
            if (!params.contains("enabled")) {
                throw std::invalid_argument("Missing enabled parameter");
            }
            
            bool enabled = params["enabled"];
            setTracking(enabled);
            
            response.setDetails({
                {"message", "Tracking state changed"},
                {"tracking", enabled}
            });
        });
    
    registerCommandHandler("abort",
        [this](const hydrogen::core::CommandMessage& cmd, hydrogen::core::ResponseMessage& response) {
            abort();
            response.setDetails({{"message", "Movement aborted"}});
        });
    
    registerCommandHandler("park",
        [this](const hydrogen::core::CommandMessage& cmd, hydrogen::core::ResponseMessage& response) {
            park();
            response.setDetails({{"message", "Telescope parked"}});
        });
    
    registerCommandHandler("unpark",
        [this](const hydrogen::core::CommandMessage& cmd, hydrogen::core::ResponseMessage& response) {
            unpark();
            response.setDetails({{"message", "Telescope unparked"}});
        });
    
    registerCommandHandler("sync",
        [this](const hydrogen::core::CommandMessage& cmd, hydrogen::core::ResponseMessage& response) {
            auto params = cmd.getParameters();
            if (!params.contains("ra") || !params.contains("dec")) {
                throw std::invalid_argument("Missing ra or dec parameter");
            }
            
            double ra = params["ra"];
            double dec = params["dec"];
            sync(ra, dec);
            
            response.setDetails({
                {"message", "Telescope synced"},
                {"ra", ra},
                {"dec", dec}
            });
        });
    
    registerCommandHandler("set_observer_location",
        [this](const hydrogen::core::CommandMessage& cmd, hydrogen::core::ResponseMessage& response) {
            auto params = cmd.getParameters();
            if (!params.contains("latitude") || !params.contains("longitude")) {
                throw std::invalid_argument("Missing latitude or longitude parameter");
            }
            
            double lat = params["latitude"];
            double lon = params["longitude"];
            setObserverLocation(lat, lon);
            
            response.setDetails({
                {"message", "Observer location set"},
                {"latitude", lat},
                {"longitude", lon}
            });
        });
}

void Telescope::updateLoop() {
    while (updateThreadRunning_ && running_) {
        auto currentTime = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastUpdateTime_).count();

        if (deltaTime >= updateIntervalMs_) {
            {
                std::lock_guard<std::mutex> lock(positionMutex_);

                // Update slewing position
                if (moving_ && !parked_) {
                    updateSlewingPosition();
                }

                // Simulate tracking (RA changes with time)
                if (tracking_ && !parked_ && !moving_) {
                    // Sidereal rate: approximately 15.041 arcseconds per second
                    double siderealRate = 15.041 / 3600.0 / 15.0; // Convert to hours per second
                    double timeStep = deltaTime / 1000.0; // Convert to seconds
                    currentRA_ += siderealRate * timeStep;

                    if (currentRA_ >= 24.0) {
                        currentRA_ -= 24.0;
                    } else if (currentRA_ < 0.0) {
                        currentRA_ += 24.0;
                    }

                    updateAltAz();
                }

                // Update position history
                PositionHistory historyEntry = {
                    currentRA_, currentDec_, currentAlt_, currentAz_, currentTime
                };
                positionHistory_.push_back(historyEntry);

                if (positionHistory_.size() > maxHistorySize_) {
                    positionHistory_.erase(positionHistory_.begin());
                }
            }

            // Send position update if significant change
            sendPositionUpdate();
            lastUpdateTime_ = currentTime;
        }

        // Sleep for a shorter interval to maintain responsiveness
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

double Telescope::getCurrentLST() const {
    // Simplified LST calculation
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::gmtime(&time_t);
    
    double ut = tm.tm_hour + tm.tm_min / 60.0 + tm.tm_sec / 3600.0;
    double lst = ut + observerLongitude_ / 15.0; // Simplified, ignores equation of time
    
    while (lst < 0) lst += 24.0;
    while (lst >= 24.0) lst -= 24.0;
    
    return lst;
}

void Telescope::updateAltAz() {
    // Convert RA/Dec to Alt/Az using observer location
    // This is a simplified calculation
    double ha = getCurrentLST() - currentRA_; // Hour angle
    double lat_rad = observerLatitude_ * M_PI / 180.0;
    double dec_rad = currentDec_ * M_PI / 180.0;
    double ha_rad = ha * M_PI / 12.0;
    
    double alt_rad = std::asin(std::sin(lat_rad) * std::sin(dec_rad) + 
                              std::cos(lat_rad) * std::cos(dec_rad) * std::cos(ha_rad));
    
    double az_rad = std::atan2(-std::sin(ha_rad),
                              std::tan(dec_rad) * std::cos(lat_rad) - std::sin(lat_rad) * std::cos(ha_rad));
    
    currentAlt_ = alt_rad * 180.0 / M_PI;
    currentAz_ = az_rad * 180.0 / M_PI;
    
    if (currentAz_ < 0) currentAz_ += 360.0;
    
    setProperty("alt", currentAlt_);
    setProperty("az", currentAz_);
}

void Telescope::updateRaDec() {
    // Convert Alt/Az to RA/Dec (inverse of updateAltAz)
    // Implementation would be similar but inverse calculation
}

void Telescope::startUpdateThread() {
    if (updateThreadRunning_) {
        return;
    }
    
    updateThreadRunning_ = true;
    updateThread_ = std::thread(&Telescope::updateLoop, this);
}

void Telescope::stopUpdateThread() {
    if (!updateThreadRunning_) {
        return;
    }

    updateThreadRunning_ = false;

    if (updateThread_.joinable()) {
        updateThread_.join();
    }
}

double Telescope::calculateAngularSeparation(double ra1, double dec1, double ra2, double dec2) const {
    // Convert to radians
    double ra1_rad = ra1 * M_PI / 12.0;
    double dec1_rad = dec1 * M_PI / 180.0;
    double ra2_rad = ra2 * M_PI / 12.0;
    double dec2_rad = dec2 * M_PI / 180.0;

    // Use spherical law of cosines
    double cos_sep = std::sin(dec1_rad) * std::sin(dec2_rad) +
                     std::cos(dec1_rad) * std::cos(dec2_rad) * std::cos(ra1_rad - ra2_rad);

    // Clamp to valid range to avoid numerical errors
    cos_sep = std::max(-1.0, std::min(1.0, cos_sep));

    return std::acos(cos_sep) * 180.0 / M_PI;
}

double Telescope::calculateSlewTime(double targetRA, double targetDec) const {
    double separation = calculateAngularSeparation(currentRA_, currentDec_, targetRA, targetDec);

    // Estimate slew time based on current slew rate
    double effectiveSlewRate = minSlewRate_ + (maxSlewRate_ - minSlewRate_) * (slewRate_ / 9.0);

    return separation / effectiveSlewRate; // Time in seconds
}

bool Telescope::areCoordinatesWithinLimits(double ra, double dec) const {
    // Check declination limits
    if (dec < minAltitude_ || dec > maxAltitude_) {
        return false;
    }

    // Convert to Alt/Az to check altitude limits
    double lst = getCurrentLST();
    double ha = lst - ra;
    double lat_rad = observerLatitude_ * M_PI / 180.0;
    double dec_rad = dec * M_PI / 180.0;
    double ha_rad = ha * M_PI / 12.0;

    double alt_rad = std::asin(std::sin(lat_rad) * std::sin(dec_rad) +
                              std::cos(lat_rad) * std::cos(dec_rad) * std::cos(ha_rad));
    double alt = alt_rad * 180.0 / M_PI;

    return alt >= minAltitude_;
}

void Telescope::updateSlewingPosition() {
    double raError = targetRA_ - currentRA_;
    double decError = targetDec_ - currentDec_;

    // Handle RA wrap-around
    if (raError > 12.0) raError -= 24.0;
    if (raError < -12.0) raError += 24.0;

    double separation = std::sqrt(raError * raError * 225.0 + decError * decError); // Approximate

    if (separation > 0.001) { // Still moving
        // Calculate time-based movement
        auto currentTime = std::chrono::steady_clock::now();
        auto slewTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - slewStartTime_).count() / 1000.0;

        // Adaptive slew rate based on distance
        double adaptiveRate = minSlewRate_;
        if (separation > 1.0) {
            adaptiveRate = maxSlewRate_ * (slewRate_ / 9.0);
        } else {
            // Slow down as we approach target
            adaptiveRate = minSlewRate_ + (maxSlewRate_ - minSlewRate_) * separation;
        }

        double moveStep = adaptiveRate * (updateIntervalMs_ / 1000.0) / 15.0; // Convert to hours

        // Move towards target
        if (std::abs(raError) > moveStep) {
            currentRA_ += (raError > 0 ? moveStep : -moveStep);
        } else {
            currentRA_ = targetRA_;
        }

        if (std::abs(decError) > moveStep * 15.0) { // Dec in degrees
            currentDec_ += (decError > 0 ? moveStep * 15.0 : -moveStep * 15.0);
        } else {
            currentDec_ = targetDec_;
        }

        updateAltAz();

    } else {
        // Reached target
        currentRA_ = targetRA_;
        currentDec_ = targetDec_;
        moving_ = false;
        setProperty("moving", false);

        hydrogen::core::EventMessage event("goto_complete");
        event.setDeviceId(getDeviceId());
        event.setProperties({
            {"ra", currentRA_},
            {"dec", currentDec_}
        });
        sendEvent(event);
    }
}

void Telescope::sendPositionUpdate() {
    static double lastSentRA = -999.0;
    static double lastSentDec = -999.0;

    // Only send update if position changed significantly
    double raDiff = std::abs(currentRA_ - lastSentRA);
    double decDiff = std::abs(currentDec_ - lastSentDec);

    if (raDiff > 0.0001 || decDiff > 0.001) { // Threshold for significant change
        setProperty("ra", currentRA_);
        setProperty("dec", currentDec_);
        setProperty("alt", currentAlt_);
        setProperty("az", currentAz_);

        lastSentRA = currentRA_;
        lastSentDec = currentDec_;

        // Send position update event
        hydrogen::core::EventMessage event("position_update");
        event.setDeviceId(getDeviceId());
        event.setProperties({
            {"ra", currentRA_},
            {"dec", currentDec_},
            {"alt", currentAlt_},
            {"az", currentAz_}
        });
        sendEvent(event);
    }
}

} // namespace device
} // namespace hydrogen
