#include "enhanced_telescope_device.h"
#include <astrocomm/core/utils.h>
#include <spdlog/spdlog.h>
#include <cmath>

using json = nlohmann::json;

namespace astrocomm {
namespace device {
namespace telescope {

EnhancedTelescopeDevice::EnhancedTelescopeDevice(const DeviceConfiguration& config, 
                                               const TelescopeConfiguration& telescopeConfig)
    : EnhancedDeviceBase(config), telescopeConfig_(telescopeConfig),
      telescopeState_(TelescopeState::IDLE), tracking_(false), parked_(true), 
      slewProgress_(0.0), currentTrackingMode_(TrackingMode::SIDEREAL),
      monitoringActive_(false) {
    
    // Initialize telescope-specific properties
    setProperty("telescopeState", static_cast<int>(TelescopeState::IDLE));
    setProperty("isTracking", false);
    setProperty("isParked", true);
    setProperty("slewProgress", 0.0);
    setProperty("trackingMode", static_cast<int>(TrackingMode::SIDEREAL));
    
    // Initialize position
    currentPosition_.timestamp = std::chrono::system_clock::now();
    targetPosition_.timestamp = std::chrono::system_clock::now();
    
    // Initialize motor status
    motorStatus_["raMotor"] = json{{"enabled", false}, {"current", 0.0}, {"temperature", 25.0}};
    motorStatus_["decMotor"] = json{{"enabled", false}, {"current", 0.0}, {"temperature", 25.0}};
    
    SPDLOG_INFO("Enhanced telescope device created: {}", getDeviceId());
}

EnhancedTelescopeDevice::~EnhancedTelescopeDevice() {
    stop();
    SPDLOG_INFO("Enhanced telescope device destroyed: {}", getDeviceId());
}

bool EnhancedTelescopeDevice::initializeDevice() {
    try {
        // Initialize telescope-specific commands
        initializeTelescopeCommands();
        
        // Set initial state
        telescopeState_.store(TelescopeState::IDLE);
        parked_.store(true);
        
        SPDLOG_INFO("Telescope device {} initialized", getDeviceId());
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to initialize telescope device {}: {}", getDeviceId(), e.what());
        return false;
    }
}

bool EnhancedTelescopeDevice::startDevice() {
    try {
        // Start monitoring threads
        if (telescopeConfig_.enablePositionMonitoring) {
            startPositionMonitoring();
        }
        
        if (telescopeConfig_.enableTrackingMonitoring) {
            startTrackingMonitoring();
        }
        
        if (telescopeConfig_.enableEnvironmentalMonitoring) {
            startEnvironmentalMonitoring();
        }
        
        SPDLOG_INFO("Telescope device {} started", getDeviceId());
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to start telescope device {}: {}", getDeviceId(), e.what());
        return false;
    }
}

void EnhancedTelescopeDevice::stopDevice() {
    // Stop all monitoring
    stopPositionMonitoring();
    stopTrackingMonitoring();
    stopEnvironmentalMonitoring();
    
    // Stop any ongoing operations
    abortSlew();
    stopTracking();
    
    SPDLOG_INFO("Telescope device {} stopped", getDeviceId());
}

bool EnhancedTelescopeDevice::connectDevice() {
    // Simulate connection to telescope hardware
    motorsEnabled_.store(true);
    updateHealthStatus(DeviceHealthStatus::EXCELLENT);
    
    SPDLOG_INFO("Telescope device {} connected to hardware", getDeviceId());
    return true;
}

void EnhancedTelescopeDevice::disconnectDevice() {
    motorsEnabled_.store(false);
    abortSlew();
    stopTracking();
    
    SPDLOG_INFO("Telescope device {} disconnected from hardware", getDeviceId());
}

json EnhancedTelescopeDevice::getDeviceSpecificInfo() const {
    json info;
    info["telescopeState"] = static_cast<int>(telescopeState_.load());
    info["isTracking"] = tracking_.load();
    info["isParked"] = parked_.load();
    info["slewProgress"] = slewProgress_.load();
    info["trackingMode"] = static_cast<int>(currentTrackingMode_);
    info["currentPosition"] = getCurrentPosition().toJson();
    info["environmentalData"] = getEnvironmentalData();
    info["motorStatus"] = getMotorStatus();
    info["powerConsumption"] = getPowerConsumption();
    
    return info;
}

std::vector<EnhancedDeviceBase::DeviceCapability> EnhancedTelescopeDevice::getDeviceSpecificCapabilities() const {
    return {
        DeviceCapability::STREAM_DATA  // Position streaming
    };
}

bool EnhancedTelescopeDevice::performDeviceSpecificDiagnostics() {
    bool result = true;
    
    // Check motor health
    if (!checkMotorHealth()) {
        SPDLOG_WARN("Motor health check failed for telescope {}", getDeviceId());
        result = false;
    }
    
    // Check environmental conditions
    if (!checkEnvironmentalHealth()) {
        SPDLOG_WARN("Environmental health check failed for telescope {}", getDeviceId());
        result = false;
    }
    
    // Check position accuracy
    if (!checkPositionAccuracy()) {
        SPDLOG_WARN("Position accuracy check failed for telescope {}", getDeviceId());
        result = false;
    }
    
    // Check tracking accuracy if tracking
    if (tracking_.load() && !checkTrackingAccuracy()) {
        SPDLOG_WARN("Tracking accuracy check failed for telescope {}", getDeviceId());
        result = false;
    }
    
    return result;
}

bool EnhancedTelescopeDevice::performDeviceSpecificHealthCheck() {
    // Update environmental metrics
    updateEnvironmentalMetrics();
    
    // Check health thresholds
    checkHealthThresholds();
    
    // Record health check metric
    recordMetric(MetricType::CUSTOM, "health_check_performed", 1.0);
    
    return true;
}

bool EnhancedTelescopeDevice::slewToCoordinates(double ra, double dec) {
    if (!validateCoordinates(ra, dec)) {
        SPDLOG_ERROR("Invalid coordinates for telescope {}: RA={}, Dec={}", getDeviceId(), ra, dec);
        return false;
    }
    
    if (telescopeState_.load() == TelescopeState::SLEWING) {
        SPDLOG_WARN("Telescope {} already slewing", getDeviceId());
        return false;
    }
    
    // Convert to Alt/Az for limit checking
    auto [altitude, azimuth] = raDecToAltAz(ra, dec);
    if (!isWithinLimits(altitude, azimuth)) {
        SPDLOG_ERROR("Target coordinates outside limits for telescope {}", getDeviceId());
        return false;
    }
    
    // Start slew
    telescopeState_.store(TelescopeState::SLEWING);
    slewProgress_.store(0.0);
    parked_.store(false);
    
    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        targetPosition_.rightAscension = ra;
        targetPosition_.declination = dec;
        targetPosition_.altitude = altitude;
        targetPosition_.azimuth = azimuth;
        targetPosition_.timestamp = std::chrono::system_clock::now();
    }
    
    // Start slew simulation in separate thread
    std::thread slewThread(&EnhancedTelescopeDevice::simulateSlew, this, ra, dec);
    slewThread.detach();
    
    setProperty("telescopeState", static_cast<int>(TelescopeState::SLEWING));
    setProperty("isParked", false);
    
    SPDLOG_INFO("Telescope {} starting slew to RA={:.3f}h, Dec={:.3f}°", getDeviceId(), ra, dec);
    return true;
}

bool EnhancedTelescopeDevice::slewToAltAz(double altitude, double azimuth) {
    if (!validateAltAz(altitude, azimuth)) {
        return false;
    }
    
    auto [ra, dec] = altAzToRaDec(altitude, azimuth);
    return slewToCoordinates(ra, dec);
}

bool EnhancedTelescopeDevice::startTracking(TrackingMode mode) {
    if (parked_.load()) {
        SPDLOG_ERROR("Cannot start tracking while telescope {} is parked", getDeviceId());
        return false;
    }
    
    if (telescopeState_.load() == TelescopeState::SLEWING) {
        SPDLOG_ERROR("Cannot start tracking while telescope {} is slewing", getDeviceId());
        return false;
    }
    
    tracking_.store(true);
    currentTrackingMode_ = mode;
    telescopeState_.store(TelescopeState::TRACKING);
    
    setProperty("isTracking", true);
    setProperty("trackingMode", static_cast<int>(mode));
    setProperty("telescopeState", static_cast<int>(TelescopeState::TRACKING));
    
    SPDLOG_INFO("Telescope {} started tracking in mode {}", getDeviceId(), static_cast<int>(mode));
    return true;
}

bool EnhancedTelescopeDevice::stopTracking() {
    if (!tracking_.load()) {
        return true;
    }
    
    tracking_.store(false);
    telescopeState_.store(TelescopeState::IDLE);
    
    setProperty("isTracking", false);
    setProperty("telescopeState", static_cast<int>(TelescopeState::IDLE));
    
    SPDLOG_INFO("Telescope {} stopped tracking", getDeviceId());
    return true;
}

bool EnhancedTelescopeDevice::parkTelescope() {
    // Stop any ongoing operations
    abortSlew();
    stopTracking();
    
    // Slew to park position (typically zenith or home position)
    if (slewToAltAz(90.0, 0.0)) {  // Zenith
        parked_.store(true);
        telescopeState_.store(TelescopeState::PARKED);
        
        setProperty("isParked", true);
        setProperty("telescopeState", static_cast<int>(TelescopeState::PARKED));
        
        SPDLOG_INFO("Telescope {} parked", getDeviceId());
        return true;
    }
    
    return false;
}

bool EnhancedTelescopeDevice::unparkTelescope() {
    if (!parked_.load()) {
        return true;
    }
    
    parked_.store(false);
    telescopeState_.store(TelescopeState::IDLE);
    
    setProperty("isParked", false);
    setProperty("telescopeState", static_cast<int>(TelescopeState::IDLE));
    
    SPDLOG_INFO("Telescope {} unparked", getDeviceId());
    return true;
}

bool EnhancedTelescopeDevice::abortSlew() {
    if (telescopeState_.load() != TelescopeState::SLEWING) {
        return true;
    }
    
    telescopeState_.store(TelescopeState::IDLE);
    slewProgress_.store(0.0);
    
    setProperty("telescopeState", static_cast<int>(TelescopeState::IDLE));
    setProperty("slewProgress", 0.0);
    
    SPDLOG_INFO("Telescope {} slew aborted", getDeviceId());
    return true;
}

EnhancedTelescopeDevice::MountPosition EnhancedTelescopeDevice::getCurrentPosition() const {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return currentPosition_;
}

EnhancedTelescopeDevice::TelescopeState EnhancedTelescopeDevice::getTelescopeState() const {
    return telescopeState_.load();
}

bool EnhancedTelescopeDevice::isSlewing() const {
    return telescopeState_.load() == TelescopeState::SLEWING;
}

bool EnhancedTelescopeDevice::isTracking() const {
    return tracking_.load();
}

bool EnhancedTelescopeDevice::isParked() const {
    return parked_.load();
}

double EnhancedTelescopeDevice::getSlewProgress() const {
    return slewProgress_.load();
}

double EnhancedTelescopeDevice::getTemperature() const {
    std::lock_guard<std::mutex> lock(environmentalMutex_);
    return temperature_;
}

double EnhancedTelescopeDevice::getHumidity() const {
    std::lock_guard<std::mutex> lock(environmentalMutex_);
    return humidity_;
}

double EnhancedTelescopeDevice::getPressure() const {
    std::lock_guard<std::mutex> lock(environmentalMutex_);
    return pressure_;
}

double EnhancedTelescopeDevice::getWindSpeed() const {
    std::lock_guard<std::mutex> lock(environmentalMutex_);
    return windSpeed_;
}

json EnhancedTelescopeDevice::getEnvironmentalData() const {
    json data;
    {
        std::lock_guard<std::mutex> lock(environmentalMutex_);
        data["temperature"] = temperature_;
        data["humidity"] = humidity_;
        data["pressure"] = pressure_;
        data["windSpeed"] = windSpeed_;
    }
    data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return data;
}

double EnhancedTelescopeDevice::getPowerConsumption() const {
    std::lock_guard<std::mutex> lock(motorMutex_);
    return powerConsumption_;
}

json EnhancedTelescopeDevice::getMotorStatus() const {
    std::lock_guard<std::mutex> lock(motorMutex_);
    return motorStatus_;
}

void EnhancedTelescopeDevice::initializeTelescopeCommands() {
    // Register telescope-specific commands
    registerCommand("slew_to_coordinates", [this](const json& params) {
        if (!params.contains("ra") || !params.contains("dec")) {
            return json{{"error", "Missing ra or dec parameter"}};
        }

        double ra = params["ra"];
        double dec = params["dec"];
        bool success = slewToCoordinates(ra, dec);

        return json{{"success", success}, {"ra", ra}, {"dec", dec}};
    });

    registerCommand("slew_to_altaz", [this](const json& params) {
        if (!params.contains("altitude") || !params.contains("azimuth")) {
            return json{{"error", "Missing altitude or azimuth parameter"}};
        }

        double altitude = params["altitude"];
        double azimuth = params["azimuth"];
        bool success = slewToAltAz(altitude, azimuth);

        return json{{"success", success}, {"altitude", altitude}, {"azimuth", azimuth}};
    });

    registerCommand("start_tracking", [this](const json& params) {
        TrackingMode mode = TrackingMode::SIDEREAL;
        if (params.contains("mode")) {
            mode = static_cast<TrackingMode>(params["mode"].get<int>());
        }

        bool success = startTracking(mode);
        return json{{"success", success}, {"trackingMode", static_cast<int>(mode)}};
    });

    registerCommand("stop_tracking", [this]([[maybe_unused]] const json& params) {
        bool success = stopTracking();
        return json{{"success", success}};
    });

    registerCommand("park_telescope", [this]([[maybe_unused]] const json& params) {
        bool success = parkTelescope();
        return json{{"success", success}};
    });

    registerCommand("unpark_telescope", [this]([[maybe_unused]] const json& params) {
        bool success = unparkTelescope();
        return json{{"success", success}};
    });

    registerCommand("abort_slew", [this]([[maybe_unused]] const json& params) {
        bool success = abortSlew();
        return json{{"success", success}};
    });

    registerCommand("get_position", [this]([[maybe_unused]] const json& params) {
        return getCurrentPosition().toJson();
    });

    registerCommand("get_environmental_data", [this]([[maybe_unused]] const json& params) {
        return getEnvironmentalData();
    });
}

void EnhancedTelescopeDevice::startPositionMonitoring() {
    if (monitoringActive_.load()) {
        return;
    }

    monitoringActive_.store(true);
    positionMonitorThread_ = std::thread(&EnhancedTelescopeDevice::positionMonitorLoop, this);
}

void EnhancedTelescopeDevice::stopPositionMonitoring() {
    monitoringActive_.store(false);
    if (positionMonitorThread_.joinable()) {
        positionMonitorThread_.join();
    }
}

void EnhancedTelescopeDevice::startTrackingMonitoring() {
    trackingMonitorThread_ = std::thread(&EnhancedTelescopeDevice::trackingMonitorLoop, this);
}

void EnhancedTelescopeDevice::stopTrackingMonitoring() {
    if (trackingMonitorThread_.joinable()) {
        trackingMonitorThread_.join();
    }
}

void EnhancedTelescopeDevice::startEnvironmentalMonitoring() {
    environmentalMonitorThread_ = std::thread(&EnhancedTelescopeDevice::environmentalMonitorLoop, this);
}

void EnhancedTelescopeDevice::stopEnvironmentalMonitoring() {
    if (environmentalMonitorThread_.joinable()) {
        environmentalMonitorThread_.join();
    }
}

void EnhancedTelescopeDevice::positionMonitorLoop() {
    while (monitoringActive_.load() && isRunning()) {
        try {
            updatePosition();
            recordPositionMetrics();
            std::this_thread::sleep_for(telescopeConfig_.positionUpdateInterval);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in position monitor loop for telescope {}: {}", getDeviceId(), e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void EnhancedTelescopeDevice::trackingMonitorLoop() {
    while (monitoringActive_.load() && isRunning()) {
        try {
            if (tracking_.load()) {
                updateTrackingMetrics();
            }
            std::this_thread::sleep_for(telescopeConfig_.trackingUpdateInterval);
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in tracking monitor loop for telescope {}: {}", getDeviceId(), e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void EnhancedTelescopeDevice::environmentalMonitorLoop() {
    while (monitoringActive_.load() && isRunning()) {
        try {
            simulateEnvironmentalChanges();
            updateEnvironmentalMetrics();
            checkHealthThresholds();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error in environmental monitor loop for telescope {}: {}", getDeviceId(), e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void EnhancedTelescopeDevice::updatePosition() {
    std::lock_guard<std::mutex> lock(positionMutex_);
    currentPosition_.timestamp = std::chrono::system_clock::now();

    // Update properties
    setProperty("currentRA", currentPosition_.rightAscension);
    setProperty("currentDec", currentPosition_.declination);
    setProperty("currentAlt", currentPosition_.altitude);
    setProperty("currentAz", currentPosition_.azimuth);
}

void EnhancedTelescopeDevice::simulateSlew(double targetRa, double targetDec) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Calculate distance to target
    double distance = calculateAngularDistance(
        currentPosition_.rightAscension, currentPosition_.declination,
        targetRa, targetDec);

    // Simulate slew duration based on distance and max slew rate
    double slewTime = distance / telescopeConfig_.maxSlewRate;  // seconds
    auto slewDuration = std::chrono::milliseconds(static_cast<int>(slewTime * 1000));

    // Simulate gradual movement
    auto startPos = currentPosition_;
    int steps = 100;
    for (int i = 0; i <= steps && telescopeState_.load() == TelescopeState::SLEWING; ++i) {
        double progress = static_cast<double>(i) / steps;
        slewProgress_.store(progress);
        setProperty("slewProgress", progress);

        // Interpolate position
        {
            std::lock_guard<std::mutex> lock(positionMutex_);
            currentPosition_.rightAscension = startPos.rightAscension +
                (targetRa - startPos.rightAscension) * progress;
            currentPosition_.declination = startPos.declination +
                (targetDec - startPos.declination) * progress;

            // Update Alt/Az
            auto [alt, az] = raDecToAltAz(currentPosition_.rightAscension, currentPosition_.declination);
            currentPosition_.altitude = alt;
            currentPosition_.azimuth = az;
        }

        std::this_thread::sleep_for(slewDuration / steps);
    }

    // Complete slew
    if (telescopeState_.load() == TelescopeState::SLEWING) {
        {
            std::lock_guard<std::mutex> lock(positionMutex_);
            currentPosition_.rightAscension = targetRa;
            currentPosition_.declination = targetDec;
            auto [alt, az] = raDecToAltAz(targetRa, targetDec);
            currentPosition_.altitude = alt;
            currentPosition_.azimuth = az;
        }

        telescopeState_.store(TelescopeState::IDLE);
        slewProgress_.store(1.0);

        setProperty("telescopeState", static_cast<int>(TelescopeState::IDLE));
        setProperty("slewProgress", 1.0);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto actualDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordSlewMetrics(distance, actualDuration);

        SPDLOG_INFO("Telescope {} slew completed to RA={:.3f}h, Dec={:.3f}°",
                   getDeviceId(), targetRa, targetDec);
    }
}

// Utility methods
bool EnhancedTelescopeDevice::validateCoordinates(double ra, double dec) const {
    return ra >= 0.0 && ra < 24.0 && dec >= -90.0 && dec <= 90.0;
}

bool EnhancedTelescopeDevice::validateAltAz(double altitude, double azimuth) const {
    return altitude >= 0.0 && altitude <= 90.0 && azimuth >= 0.0 && azimuth < 360.0;
}

bool EnhancedTelescopeDevice::isWithinLimits(double altitude, double azimuth) const {
    return altitude >= telescopeConfig_.minAltitude &&
           altitude <= telescopeConfig_.maxAltitude &&
           azimuth >= telescopeConfig_.minAzimuth &&
           azimuth <= telescopeConfig_.maxAzimuth;
}

double EnhancedTelescopeDevice::calculateAngularDistance(double ra1, double dec1, double ra2, double dec2) const {
    // Convert to radians
    double ra1_rad = ra1 * M_PI / 12.0;  // hours to radians
    double dec1_rad = dec1 * M_PI / 180.0;
    double ra2_rad = ra2 * M_PI / 12.0;
    double dec2_rad = dec2 * M_PI / 180.0;

    // Haversine formula
    double dra = ra2_rad - ra1_rad;
    double ddec = dec2_rad - dec1_rad;

    double a = sin(ddec/2) * sin(ddec/2) +
               cos(dec1_rad) * cos(dec2_rad) * sin(dra/2) * sin(dra/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));

    return c * 180.0 / M_PI;  // Convert back to degrees
}

std::pair<double, double> EnhancedTelescopeDevice::raDecToAltAz(double ra, double dec) const {
    // Simplified conversion - in reality would need observer location and time
    // For simulation purposes, use simple approximation
    double altitude = 45.0 + dec * 0.5;  // Simplified
    double azimuth = ra * 15.0;  // Convert hours to degrees

    // Ensure within bounds
    altitude = std::max(0.0, std::min(90.0, altitude));
    azimuth = fmod(azimuth, 360.0);
    if (azimuth < 0) azimuth += 360.0;

    return {altitude, azimuth};
}

std::pair<double, double> EnhancedTelescopeDevice::altAzToRaDec(double altitude, double azimuth) const {
    // Simplified conversion - reverse of above
    double dec = (altitude - 45.0) * 2.0;
    double ra = azimuth / 15.0;  // Convert degrees to hours

    // Ensure within bounds
    dec = std::max(-90.0, std::min(90.0, dec));
    ra = fmod(ra, 24.0);
    if (ra < 0) ra += 24.0;

    return {ra, dec};
}

} // namespace telescope
} // namespace device
} // namespace astrocomm
