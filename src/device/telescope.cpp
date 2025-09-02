#include "telescope.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace hydrogen {
namespace device {

Telescope::Telescope(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "TELESCOPE", manufacturer, model)
    , mountType_(MountType::EQUATORIAL)
    , trackingMode_(TrackingMode::OFF)
    , slewSpeed_(SlewSpeed::CENTERING)
    , currentRA_(0.0)
    , currentDec_(0.0)
    , targetRA_(0.0)
    , targetDec_(0.0)
    , siteLatitude_(40.0)  // Default to 40°N
    , siteLongitude_(-74.0) // Default to 74°W
    , siteElevation_(100.0)
    , isSlewing_(false)
    , isTracking_(false)
    , isParked_(true)
    , isAligned_(false)
    , customRARate_(15.041067) // Sidereal rate in arcsec/sec
    , customDecRate_(0.0)
    , slewThreadRunning_(false) {
    
    SPDLOG_INFO("Telescope {} created with manufacturer: {}, model: {}", deviceId, manufacturer, model);
}

Telescope::~Telescope() {
    stop();
}

bool Telescope::initializeDevice() {
    // Initialize telescope-specific properties
    setProperty("mountType", static_cast<int>(mountType_));
    setProperty("trackingMode", static_cast<int>(trackingMode_));
    setProperty("slewSpeed", static_cast<int>(slewSpeed_));
    setProperty("currentRA", currentRA_.load());
    setProperty("currentDec", currentDec_.load());
    setProperty("targetRA", targetRA_.load());
    setProperty("targetDec", targetDec_.load());
    setProperty("siteLatitude", siteLatitude_.load());
    setProperty("siteLongitude", siteLongitude_.load());
    setProperty("siteElevation", siteElevation_.load());
    setProperty("isSlewing", isSlewing_.load());
    setProperty("isTracking", isTracking_.load());
    setProperty("isParked", isParked_.load());
    setProperty("isAligned", isAligned_.load());
    
    return true;
}

bool Telescope::startDevice() {
    // Start slewing thread
    slewThreadRunning_ = true;
    slewThread_ = std::thread(&Telescope::slewThreadFunction, this);
    
    return true;
}

void Telescope::stopDevice() {
    // Stop slewing thread
    slewThreadRunning_ = false;
    if (slewThread_.joinable()) {
        slewThread_.join();
    }
    
    // Stop any ongoing movement
    if (isSlewing_) {
        stopSlewing();
    }
}

// ITelescope interface implementation
bool Telescope::slewToCoordinates(double ra, double dec) {
    if (isParked_) {
        SPDLOG_ERROR("Telescope {} cannot slew while parked", getDeviceId());
        return false;
    }
    
    if (ra < 0 || ra >= 24 || dec < -90 || dec > 90) {
        SPDLOG_ERROR("Telescope {} invalid coordinates: RA={:.3f}, Dec={:.3f}", getDeviceId(), ra, dec);
        return false;
    }
    
    targetRA_ = ra;
    targetDec_ = dec;
    isSlewing_ = true;
    
    setProperty("targetRA", ra);
    setProperty("targetDec", dec);
    setProperty("isSlewing", true);
    
    SPDLOG_INFO("Telescope {} slewing to RA={:.3f}h, Dec={:.3f}°", getDeviceId(), ra, dec);
    return executeSlew(ra, dec);
}

bool Telescope::syncToCoordinates(double ra, double dec) {
    if (isParked_) {
        SPDLOG_ERROR("Telescope {} cannot sync while parked", getDeviceId());
        return false;
    }
    
    if (ra < 0 || ra >= 24 || dec < -90 || dec > 90) {
        SPDLOG_ERROR("Telescope {} invalid sync coordinates: RA={:.3f}, Dec={:.3f}", getDeviceId(), ra, dec);
        return false;
    }
    
    currentRA_ = ra;
    currentDec_ = dec;
    isAligned_ = true;
    
    setProperty("currentRA", ra);
    setProperty("currentDec", dec);
    setProperty("isAligned", true);
    
    SPDLOG_INFO("Telescope {} synced to RA={:.3f}h, Dec={:.3f}°", getDeviceId(), ra, dec);
    return executeSync(ra, dec);
}

bool Telescope::stopSlewing() {
    if (!isSlewing_) {
        return true;
    }
    
    isSlewing_ = false;
    setProperty("isSlewing", false);
    
    SPDLOG_INFO("Telescope {} slewing stopped", getDeviceId());
    return executeStop();
}

bool Telescope::isSlewing() const {
    return isSlewing_;
}

void Telescope::getCurrentCoordinates(double& ra, double& dec) const {
    ra = currentRA_;
    dec = currentDec_;
}

bool Telescope::setTrackingMode(bool enabled) {
    if (isParked_) {
        SPDLOG_ERROR("Telescope {} cannot change tracking while parked", getDeviceId());
        return false;
    }
    
    isTracking_ = enabled;
    trackingMode_ = enabled ? TrackingMode::SIDEREAL : TrackingMode::OFF;
    
    setProperty("isTracking", enabled);
    setProperty("trackingMode", static_cast<int>(trackingMode_));
    
    SPDLOG_INFO("Telescope {} tracking {}", getDeviceId(), enabled ? "enabled" : "disabled");
    return setTrackingEnabled(enabled);
}

bool Telescope::isTracking() const {
    return isTracking_;
}

bool Telescope::park() {
    if (isParked_) {
        return true;
    }
    
    // Stop any movement first
    if (isSlewing_) {
        stopSlewing();
    }
    
    // Stop tracking
    setTrackingMode(false);
    
    isParked_ = true;
    setProperty("isParked", true);
    
    SPDLOG_INFO("Telescope {} parked", getDeviceId());
    return executePark();
}

bool Telescope::isParked() const {
    return isParked_;
}

// Extended functionality (backward compatibility)
void Telescope::gotoPosition(double ra, double dec) {
    slewToCoordinates(ra, dec);
}

void Telescope::setTracking(bool enabled) {
    setTrackingMode(enabled);
}

void Telescope::setSlewRate(int rate) {
    if (rate < 1 || rate > 4) {
        SPDLOG_ERROR("Telescope {} invalid slew rate: {}", getDeviceId(), rate);
        return;
    }
    
    slewSpeed_ = static_cast<SlewSpeed>(rate);
    setProperty("slewSpeed", rate);
    
    SPDLOG_DEBUG("Telescope {} slew rate set to {}", getDeviceId(), rate);
}

void Telescope::abort() {
    stopSlewing();
}

bool Telescope::setSlewSpeed(SlewSpeed speed) {
    slewSpeed_ = speed;
    setProperty("slewSpeed", static_cast<int>(speed));
    return true;
}

SlewSpeed Telescope::getSlewSpeed() const {
    return slewSpeed_;
}

bool Telescope::gotoCoordinates(double ra, double dec, bool synchronous) {
    if (!slewToCoordinates(ra, dec)) {
        return false;
    }
    
    if (synchronous) {
        // Wait for slewing to complete
        while (isSlewing_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    return true;
}

bool Telescope::syncCoordinates(double ra, double dec) {
    return syncToCoordinates(ra, dec);
}

TelescopeCoordinates Telescope::getCurrentCoordinates() const {
    TelescopeCoordinates coords;
    coords.ra = currentRA_;
    coords.dec = currentDec_;
    
    // Calculate alt/az
    equatorialToAltAz(currentRA_, currentDec_, coords.alt, coords.az);
    coords.lst = calculateLST();
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    coords.timestamp = std::to_string(time_t);
    
    return coords;
}

bool Telescope::setSiteLocation(double latitude, double longitude, double elevation) {
    if (latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) {
        SPDLOG_ERROR("Telescope {} invalid site location: lat={:.3f}, lon={:.3f}", 
                     getDeviceId(), latitude, longitude);
        return false;
    }
    
    siteLatitude_ = latitude;
    siteLongitude_ = longitude;
    siteElevation_ = elevation;
    
    setProperty("siteLatitude", latitude);
    setProperty("siteLongitude", longitude);
    setProperty("siteElevation", elevation);
    
    SPDLOG_INFO("Telescope {} site location set: lat={:.3f}°, lon={:.3f}°, elev={:.1f}m", 
                getDeviceId(), latitude, longitude, elevation);
    return true;
}

void Telescope::getSiteLocation(double& latitude, double& longitude, double& elevation) const {
    latitude = siteLatitude_;
    longitude = siteLongitude_;
    elevation = siteElevation_;
}

bool Telescope::home() {
    if (isSlewing_) {
        stopSlewing();
    }
    
    SPDLOG_INFO("Telescope {} homing", getDeviceId());
    return executeHome();
}

bool Telescope::unpark() {
    if (!isParked_) {
        return true;
    }
    
    isParked_ = false;
    setProperty("isParked", false);
    
    SPDLOG_INFO("Telescope {} unparked", getDeviceId());
    return executeUnpark();
}

// Hardware abstraction methods (simulation)
bool Telescope::executeSlew(double ra, double dec) {
    // Simulate slew command
    SPDLOG_DEBUG("Telescope {} executing slew to RA={:.3f}h, Dec={:.3f}°", getDeviceId(), ra, dec);
    return true;
}

bool Telescope::executeSync(double ra, double dec) {
    // Simulate sync command
    SPDLOG_DEBUG("Telescope {} executing sync to RA={:.3f}h, Dec={:.3f}°", getDeviceId(), ra, dec);
    return true;
}

bool Telescope::executeStop() {
    // Simulate stop command
    SPDLOG_DEBUG("Telescope {} executing stop", getDeviceId());
    return true;
}

bool Telescope::executePark() {
    // Simulate park command
    SPDLOG_DEBUG("Telescope {} executing park", getDeviceId());
    return true;
}

bool Telescope::executeUnpark() {
    // Simulate unpark command
    SPDLOG_DEBUG("Telescope {} executing unpark", getDeviceId());
    return true;
}

bool Telescope::executeHome() {
    // Simulate home command
    SPDLOG_DEBUG("Telescope {} executing home", getDeviceId());
    return true;
}

TelescopeCoordinates Telescope::readCurrentCoordinates() {
    return getCurrentCoordinates();
}

bool Telescope::setTrackingEnabled(bool enabled) {
    // Simulate tracking enable/disable
    SPDLOG_DEBUG("Telescope {} setting tracking: {}", getDeviceId(), enabled);
    return true;
}

void Telescope::equatorialToAltAz(double ra, double dec, double& alt, double& az) const {
    double lst = calculateLST();
    double ha = lst - ra; // Hour angle
    
    // Convert to radians
    double haRad = ha * M_PI / 12.0;
    double decRad = dec * M_PI / 180.0;
    double latRad = siteLatitude_ * M_PI / 180.0;
    
    // Calculate altitude
    double sinAlt = sin(decRad) * sin(latRad) + cos(decRad) * cos(latRad) * cos(haRad);
    alt = asin(sinAlt) * 180.0 / M_PI;
    
    // Calculate azimuth
    double cosAz = (sin(decRad) - sin(latRad) * sinAlt) / (cos(latRad) * cos(asin(sinAlt)));
    az = acos(cosAz) * 180.0 / M_PI;
    
    if (sin(haRad) > 0) {
        az = 360.0 - az;
    }
}

double Telescope::calculateLST() const {
    // Simplified LST calculation
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::gmtime(&time_t);
    
    double ut = tm.tm_hour + tm.tm_min / 60.0 + tm.tm_sec / 3600.0;
    double jd = 2451545.0 + (time_t / 86400.0); // Simplified Julian Date
    
    double gmst = 18.697374558 + 24.06570982441908 * (jd - 2451545.0);
    double lst = gmst + siteLongitude_ / 15.0;
    
    // Normalize to 0-24 hours
    while (lst < 0) lst += 24.0;
    while (lst >= 24) lst -= 24.0;
    
    return lst;
}

void Telescope::slewThreadFunction() {
    while (slewThreadRunning_) {
        if (isSlewing_ && !isParked_) {
            double raError = targetRA_ - currentRA_;
            double decError = targetDec_ - currentDec_;
            
            // Calculate movement rate based on slew speed
            double moveRate = 0.01 * static_cast<int>(slewSpeed_);
            
            if (std::abs(raError) > 0.001 || std::abs(decError) > 0.001) {
                // Move towards target
                currentRA_ += raError * moveRate;
                currentDec_ += decError * moveRate;
                
                // Normalize RA to 0-24 hours
                if (currentRA_ < 0) currentRA_ += 24.0;
                if (currentRA_ >= 24) currentRA_ -= 24.0;
                
                // Clamp Dec to -90 to +90 degrees
                currentDec_ = std::clamp(currentDec_.load(), -90.0, 90.0);
                
                setProperty("currentRA", currentRA_.load());
                setProperty("currentDec", currentDec_.load());
            } else {
                // Reached target
                isSlewing_ = false;
                setProperty("isSlewing", false);
                
                SPDLOG_INFO("Telescope {} slew completed to RA={:.3f}h, Dec={:.3f}°", 
                           getDeviceId(), currentRA_.load(), currentDec_.load());
                
                // Notify completion
                slewCompleteCV_.notify_all();
            }
        }
        
        // Handle tracking
        if (isTracking_ && !isParked_ && !isSlewing_) {
            // Apply sidereal tracking rate
            double siderealRate = 15.041067 / 3600.0 / 15.0; // arcsec/sec to hours/sec
            currentRA_ += siderealRate;
            
            if (currentRA_ >= 24.0) {
                currentRA_ -= 24.0;
            }
            
            setProperty("currentRA", currentRA_.load());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Telescope::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "SLEW_TO_COORDINATES") {
        double ra = parameters.value("ra", 0.0);
        double dec = parameters.value("dec", 0.0);
        result["success"] = slewToCoordinates(ra, dec);
        return true;
    }
    else if (command == "SYNC_TO_COORDINATES") {
        double ra = parameters.value("ra", 0.0);
        double dec = parameters.value("dec", 0.0);
        result["success"] = syncToCoordinates(ra, dec);
        return true;
    }
    else if (command == "STOP_SLEWING") {
        result["success"] = stopSlewing();
        return true;
    }
    else if (command == "SET_TRACKING") {
        bool enabled = parameters.value("enabled", false);
        result["success"] = setTrackingMode(enabled);
        return true;
    }
    else if (command == "PARK") {
        result["success"] = park();
        return true;
    }
    else if (command == "UNPARK") {
        result["success"] = unpark();
        return true;
    }
    else if (command == "GET_COORDINATES") {
        double ra, dec;
        getCurrentCoordinates(ra, dec);
        result["ra"] = ra;
        result["dec"] = dec;
        result["success"] = true;
        return true;
    }
    else if (command == "SET_SITE_LOCATION") {
        double lat = parameters.value("latitude", 0.0);
        double lon = parameters.value("longitude", 0.0);
        double elev = parameters.value("elevation", 0.0);
        result["success"] = setSiteLocation(lat, lon, elev);
        return true;
    }
    
    return false;
}

void Telescope::updateDevice() {
    // Update coordinate properties
    setProperty("currentRA", currentRA_.load());
    setProperty("currentDec", currentDec_.load());
    
    // Calculate and update alt/az coordinates
    double alt, az;
    equatorialToAltAz(currentRA_, currentDec_, alt, az);
    setProperty("currentAlt", alt);
    setProperty("currentAz", az);
    
    // Update LST
    setProperty("lst", calculateLST());
}

std::vector<std::string> Telescope::getCapabilities() const {
    return {
        "SLEW_TO_COORDINATES",
        "SYNC_TO_COORDINATES", 
        "STOP_SLEWING",
        "SET_TRACKING",
        "PARK",
        "UNPARK",
        "GET_COORDINATES",
        "SET_SITE_LOCATION"
    };
}

} // namespace device
} // namespace hydrogen
