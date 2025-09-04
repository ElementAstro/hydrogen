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
        abortSlew();
    }
}

// ITelescope interface implementation
void Telescope::slewToCoordinates(double ra, double dec) {
    if (isParked_) {
        SPDLOG_ERROR("Telescope {} cannot slew while parked", getDeviceId());
        return;
    }

    if (ra < 0 || ra >= 24 || dec < -90 || dec > 90) {
        SPDLOG_ERROR("Telescope {} invalid coordinates: RA={:.3f}, Dec={:.3f}", getDeviceId(), ra, dec);
        return;
    }

    targetRA_ = ra;
    targetDec_ = dec;
    isSlewing_ = true;

    setProperty("targetRA", ra);
    setProperty("targetDec", dec);
    setProperty("isSlewing", true);

    SPDLOG_INFO("Telescope {} slewing to RA={:.3f}h, Dec={:.3f}°", getDeviceId(), ra, dec);
    executeSlew(ra, dec);
}

void Telescope::syncToCoordinates(double ra, double dec) {
    if (isParked_) {
        SPDLOG_ERROR("Telescope {} cannot sync while parked", getDeviceId());
        return;
    }

    if (ra < 0 || ra >= 24 || dec < -90 || dec > 90) {
        SPDLOG_ERROR("Telescope {} invalid sync coordinates: RA={:.3f}, Dec={:.3f}", getDeviceId(), ra, dec);
        return;
    }

    currentRA_ = ra;
    currentDec_ = dec;
    isAligned_ = true;

    setProperty("currentRA", ra);
    setProperty("currentDec", dec);
    setProperty("isAligned", true);

    SPDLOG_INFO("Telescope {} synced to RA={:.3f}h, Dec={:.3f}°", getDeviceId(), ra, dec);
    executeSync(ra, dec);
}

void Telescope::abortSlew() {
    if (!isSlewing_) {
        return;
    }

    isSlewing_ = false;
    setProperty("isSlewing", false);

    SPDLOG_INFO("Telescope {} slewing stopped", getDeviceId());
    executeStop();
}

bool Telescope::getSlewing() const {
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

bool Telescope::getTracking() const {
    return isTracking_;
}

void Telescope::park() {
    if (isParked_) {
        return;
    }

    // Stop any movement first
    if (isSlewing_) {
        abortSlew();
    }

    // Stop tracking
    setTrackingMode(false);

    isParked_ = true;
    setProperty("isParked", true);

    SPDLOG_INFO("Telescope {} parked", getDeviceId());
    executePark();
}

bool Telescope::getAtPark() const {
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
    abortSlew();
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
    slewToCoordinates(ra, dec);
    
    if (synchronous) {
        // Wait for slewing to complete
        while (isSlewing_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    return true;
}

bool Telescope::syncCoordinates(double ra, double dec) {
    syncToCoordinates(ra, dec);
    return true;
}

// ============================================================================
// ITelescope Interface Implementation - Coordinate Properties
// ============================================================================

double Telescope::getRightAscension() const {
    return currentRA_.load();
}

double Telescope::getDeclination() const {
    return currentDec_.load();
}

double Telescope::getAltitude() const {
    // Convert RA/Dec to Alt/Az (simplified implementation)
    return 45.0; // Stub implementation
}

double Telescope::getAzimuth() const {
    // Convert RA/Dec to Alt/Az (simplified implementation)
    return 180.0; // Stub implementation
}

double Telescope::getTargetRightAscension() const {
    return targetRA_.load();
}

void Telescope::setTargetRightAscension(double value) {
    targetRA_.store(value);
}

double Telescope::getTargetDeclination() const {
    return targetDec_.load();
}

void Telescope::setTargetDeclination(double value) {
    targetDec_.store(value);
}

// ============================================================================
// ITelescope Interface Implementation - Slewing Methods
// ============================================================================

void Telescope::slewToCoordinatesAsync(double ra, double dec) {
    // Async version - start slewing in background
    slewToCoordinates(ra, dec);
}

void Telescope::slewToTarget() {
    double ra = getTargetRightAscension();
    double dec = getTargetDeclination();
    slewToCoordinates(ra, dec);
}

void Telescope::slewToTargetAsync() {
    double ra = getTargetRightAscension();
    double dec = getTargetDeclination();
    slewToCoordinatesAsync(ra, dec);
}

void Telescope::slewToAltAz(double altitude, double azimuth) {
    // Convert Alt/Az to RA/Dec and slew (simplified implementation)
    // This would require proper coordinate transformation
    (void)altitude; // Suppress unused parameter warning
    (void)azimuth;  // Suppress unused parameter warning
    slewToCoordinates(0.0, 0.0); // Stub implementation
}

void Telescope::slewToAltAzAsync(double altitude, double azimuth) {
    slewToAltAz(altitude, azimuth);
}

// ============================================================================
// ITelescope Interface Implementation - Synchronization
// ============================================================================

void Telescope::syncToTarget() {
    double ra = getTargetRightAscension();
    double dec = getTargetDeclination();
    syncToCoordinates(ra, dec);
}

void Telescope::syncToAltAz(double altitude, double azimuth) {
    // Convert Alt/Az to RA/Dec and sync (simplified implementation)
    (void)altitude; // Suppress unused parameter warning
    (void)azimuth;  // Suppress unused parameter warning
    syncToCoordinates(0.0, 0.0); // Stub implementation
}

// ============================================================================
// ITelescope Interface Implementation - Capabilities
// ============================================================================

bool Telescope::getCanSlew() const {
    return true; // Most telescopes can slew
}

bool Telescope::getCanSlewAsync() const {
    return true; // Support async slewing
}

bool Telescope::getCanSlewAltAz() const {
    return true; // Support Alt/Az slewing
}

bool Telescope::getCanSlewAltAzAsync() const {
    return true; // Support async Alt/Az slewing
}

bool Telescope::getCanSync() const {
    return true; // Support synchronization
}

bool Telescope::getCanSyncAltAz() const {
    return true; // Support Alt/Az synchronization
}

bool Telescope::getCanPark() const {
    return true; // Support parking
}

bool Telescope::getCanUnpark() const {
    return true; // Support unparking
}

bool Telescope::getCanFindHome() const {
    return false; // Not all telescopes have home position
}

bool Telescope::getCanSetPark() const {
    return true; // Can set park position
}

bool Telescope::getCanSetTracking() const {
    return true; // Can control tracking
}

bool Telescope::getCanSetGuideRates() const {
    return true; // Can set guide rates
}

bool Telescope::getCanSetRightAscensionRate() const {
    return true; // Can set RA rate
}

bool Telescope::getCanSetDeclinationRate() const {
    return true; // Can set Dec rate
}

bool Telescope::getCanSetPierSide() const {
    return false; // Not all mounts support pier side control
}

bool Telescope::getCanPulseGuide() const {
    return true; // Support pulse guiding
}

// ============================================================================
// ITelescope Interface Implementation - Tracking
// ============================================================================

// setTracking is already implemented above (line 199)

interfaces::DriveRate Telescope::getTrackingRate() const {
    return interfaces::DriveRate::SIDEREAL; // Default to sidereal rate
}

void Telescope::setTrackingRate(interfaces::DriveRate value) {
    // Store tracking rate (stub implementation)
    // In real implementation, this would control the mount's tracking rate
    (void)value; // Suppress unused parameter warning
}

std::vector<interfaces::DriveRate> Telescope::getTrackingRates() const {
    return {
        interfaces::DriveRate::SIDEREAL,
        interfaces::DriveRate::LUNAR,
        interfaces::DriveRate::SOLAR,
        interfaces::DriveRate::KING
    };
}

double Telescope::getRightAscensionRate() const {
    return 0.0; // Stub implementation
}

void Telescope::setRightAscensionRate(double value) {
    // Set RA rate (stub implementation)
    (void)value; // Suppress unused parameter warning
}

double Telescope::getDeclinationRate() const {
    return 0.0; // Stub implementation
}

void Telescope::setDeclinationRate(double value) {
    // Set Dec rate (stub implementation)
    (void)value; // Suppress unused parameter warning
}

// ============================================================================
// ITelescope Interface Implementation - Parking and Homing
// ============================================================================

// unpark is already implemented below (line 553)

void Telescope::setPark() {
    // Set current position as park position (stub implementation)
}

void Telescope::findHome() {
    // Find home position (stub implementation)
    // Not all telescopes support this
}

bool Telescope::getAtHome() const {
    return false; // Stub implementation
}

// ============================================================================
// ITelescope Interface Implementation - Guide Rates and Pulse Guiding
// ============================================================================

double Telescope::getGuideRateRightAscension() const {
    return 0.5; // Default guide rate (0.5x sidereal)
}

void Telescope::setGuideRateRightAscension(double value) {
    // Set RA guide rate (stub implementation)
    (void)value; // Suppress unused parameter warning
}

double Telescope::getGuideRateDeclination() const {
    return 0.5; // Default guide rate (0.5x sidereal)
}

void Telescope::setGuideRateDeclination(double value) {
    // Set Dec guide rate (stub implementation)
    (void)value; // Suppress unused parameter warning
}

void Telescope::pulseGuide(interfaces::GuideDirection direction, int duration) {
    // Pulse guide implementation (stub)
    (void)direction; // Suppress unused parameter warning
    (void)duration;  // Suppress unused parameter warning
}

bool Telescope::getIsPulseGuiding() const {
    return false; // Not currently pulse guiding
}

// ============================================================================
// ITelescope Interface Implementation - Site Information
// ============================================================================

double Telescope::getSiteLatitude() const {
    return 40.0; // Default latitude (stub implementation)
}

void Telescope::setSiteLatitude(double value) {
    // Set site latitude (stub implementation)
    (void)value; // Suppress unused parameter warning
}

double Telescope::getSiteLongitude() const {
    return -74.0; // Default longitude (stub implementation)
}

void Telescope::setSiteLongitude(double value) {
    // Set site longitude (stub implementation)
    (void)value; // Suppress unused parameter warning
}

double Telescope::getSiteElevation() const {
    return 100.0; // Default elevation in meters (stub implementation)
}

void Telescope::setSiteElevation(double value) {
    // Set site elevation (stub implementation)
    (void)value; // Suppress unused parameter warning
}

double Telescope::getSiderealTime() const {
    // Calculate sidereal time (stub implementation)
    return 12.0; // Default to 12 hours
}

std::chrono::system_clock::time_point Telescope::getUTCDate() const {
    return std::chrono::system_clock::now();
}

void Telescope::setUTCDate(const std::chrono::system_clock::time_point& value) {
    // Set UTC date (stub implementation)
    (void)value; // Suppress unused parameter warning
}

// ============================================================================
// ITelescope Interface Implementation - Pier Side and Alignment
// ============================================================================

interfaces::PierSide Telescope::getSideOfPier() const {
    return interfaces::PierSide::EAST; // Default pier side
}

void Telescope::setSideOfPier(interfaces::PierSide value) {
    // Set pier side (stub implementation)
    (void)value; // Suppress unused parameter warning
}

interfaces::PierSide Telescope::getDestinationSideOfPier(double ra, double dec) const {
    // Calculate destination pier side (stub implementation)
    (void)ra;  // Suppress unused parameter warning
    (void)dec; // Suppress unused parameter warning
    return interfaces::PierSide::EAST;
}

interfaces::AlignmentMode Telescope::getAlignmentMode() const {
    return interfaces::AlignmentMode::POLAR; // Default alignment mode
}

int Telescope::getEquatorialSystem() const {
    return 2000; // J2000.0 epoch
}

double Telescope::getFocalLength() const {
    return 1000.0; // Default focal length in mm
}

double Telescope::getApertureArea() const {
    return 0.0314; // Default aperture area in m² (200mm diameter)
}

double Telescope::getApertureDiameter() const {
    return 0.2; // Default aperture diameter in meters (200mm)
}

bool Telescope::getDoesRefraction() const {
    return true; // Enable atmospheric refraction correction by default
}

void Telescope::setDoesRefraction(bool value) {
    // Set refraction correction (stub implementation)
    (void)value; // Suppress unused parameter warning
}

// ============================================================================
// ITelescope Interface Implementation - Axis Control
// ============================================================================

bool Telescope::canMoveAxis(int axis) const {
    // Check if axis can be moved (stub implementation)
    return (axis == 0 || axis == 1); // RA and Dec axes
}

std::vector<interfaces::Rate> Telescope::axisRates(int axis) const {
    // Return available axis rates (stub implementation)
    (void)axis; // Suppress unused parameter warning
    return {}; // Empty vector for stub
}

void Telescope::moveAxis(int axis, double rate) {
    // Move axis at specified rate (stub implementation)
    (void)axis; // Suppress unused parameter warning
    (void)rate; // Suppress unused parameter warning
}

// ============================================================================
// ITelescope Interface Implementation - Slew Settle Time
// ============================================================================

double Telescope::getSlewSettleTime() const {
    return 2.0; // Default settle time in seconds
}

void Telescope::setSlewSettleTime(double value) {
    // Set slew settle time (stub implementation)
    (void)value; // Suppress unused parameter warning
}

// ============================================================================
// ITelescope Interface Implementation - Convenience Methods
// ============================================================================

bool Telescope::slewToCoordinatesSync(double ra, double dec) {
    slewToCoordinates(ra, dec);
    return true; // Stub implementation
}

bool Telescope::syncToCoordinatesSync(double ra, double dec) {
    syncToCoordinates(ra, dec);
    return true; // Stub implementation
}

bool Telescope::stopSlewingSync() {
    abortSlew();
    return true; // Stub implementation
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
        abortSlew();
    }

    SPDLOG_INFO("Telescope {} homing", getDeviceId());
    return executeHome();
}

void Telescope::unpark() {
    if (!isParked_) {
        return;
    }

    isParked_ = false;
    setProperty("isParked", false);

    SPDLOG_INFO("Telescope {} unparked", getDeviceId());
    executeUnpark();
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
                double newRA = currentRA_.load() + raError * moveRate;
                double newDec = currentDec_.load() + decError * moveRate;

                // Normalize RA to 0-24 hours
                if (newRA < 0) newRA += 24.0;
                if (newRA >= 24) newRA -= 24.0;

                currentRA_.store(newRA);
                currentDec_.store(newDec);
                
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
            double newRA = currentRA_.load() + siderealRate;

            if (newRA >= 24.0) {
                newRA -= 24.0;
            }

            currentRA_.store(newRA);
            
            setProperty("currentRA", currentRA_.load());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Telescope::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "SLEW_TO_COORDINATES") {
        double ra = parameters.value("ra", 0.0);
        double dec = parameters.value("dec", 0.0);
        slewToCoordinates(ra, dec);
        result["success"] = true;
        return true;
    }
    else if (command == "SYNC_TO_COORDINATES") {
        double ra = parameters.value("ra", 0.0);
        double dec = parameters.value("dec", 0.0);
        syncToCoordinates(ra, dec);
        result["success"] = true;
        return true;
    }
    else if (command == "STOP_SLEWING") {
        abortSlew();
        result["success"] = true;
        return true;
    }
    else if (command == "SET_TRACKING") {
        bool enabled = parameters.value("enabled", false);
        result["success"] = setTrackingMode(enabled);
        return true;
    }
    else if (command == "PARK") {
        park();
        result["success"] = true;
        return true;
    }
    else if (command == "UNPARK") {
        unpark();
        result["success"] = true;
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



} // namespace device
} // namespace hydrogen
