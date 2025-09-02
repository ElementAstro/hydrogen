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
      observerLatitude_(0.0), observerLongitude_(0.0),
      tracking_(false), parked_(true), moving_(false), slewRate_(5),
      updateThreadRunning_(false) {
    
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
    
    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        targetRA_ = ra;
        targetDec_ = dec;
    }
    
    moving_ = true;
    setProperty("moving", true);
    setProperty("target_ra", ra);
    setProperty("target_dec", dec);
    
    // Send event
    hydrogen::core::EventMessage event("goto_started");
    event.setDeviceId(getDeviceId());
    event.setProperties({
        {"target_ra", ra},
        {"target_dec", dec}
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
        {
            std::lock_guard<std::mutex> lock(positionMutex_);
            
            // Simulate movement towards target
            if (moving_ && !parked_) {
                double raError = targetRA_ - currentRA_;
                double decError = targetDec_ - currentDec_;
                
                // Simple proportional movement
                double moveRate = 0.1 * (slewRate_ + 1); // Faster for higher slew rates
                
                if (std::abs(raError) > 0.001 || std::abs(decError) > 0.001) {
                    currentRA_ += raError * moveRate;
                    currentDec_ += decError * moveRate;
                    
                    setProperty("ra", currentRA_);
                    setProperty("dec", currentDec_);
                    updateAltAz();
                } else {
                    // Reached target
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
            
            // Simulate tracking (RA changes with time)
            if (tracking_ && !parked_ && !moving_) {
                // Sidereal rate: approximately 15 arcseconds per second
                double siderealRate = 15.0 / 3600.0 / 15.0; // Convert to hours per second
                currentRA_ += siderealRate;
                
                if (currentRA_ >= 24.0) {
                    currentRA_ -= 24.0;
                }
                
                setProperty("ra", currentRA_);
                updateAltAz();
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

} // namespace device
} // namespace hydrogen
