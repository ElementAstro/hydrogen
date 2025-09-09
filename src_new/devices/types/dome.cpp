#include "dome.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using json = nlohmann::json;

namespace hydrogen {
namespace device {

Dome::Dome(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "DOME", manufacturer, model)
    , currentAzimuth_(0.0)
    , currentAltitude_(0.0)
    , targetAzimuth_(0.0)
    , targetAltitude_(0.0)
    , shutterState_(interfaces::ShutterState::CLOSED)
    , isSlewing_(false)
    , isParked_(true)
    , isAtHome_(false)
    , isSlaved_(false)
    , homePosition_(0.0)
    , parkPosition_(0.0)
    , minAzimuth_(0.0)
    , maxAzimuth_(360.0)
    , minAltitude_(0.0)
    , maxAltitude_(90.0)
    , slewRate_(5.0)  // 5 degrees per second default
    , domeRadius_(3.0)  // 3 meter radius default
    , telescopeNorthOffset_(0.0)
    , telescopeEastOffset_(0.0)
    , shutterTimeout_(30)
    , telescopeRA_(0.0)
    , telescopeDec_(0.0)
    , telescopeAlt_(0.0)
    , telescopeAz_(0.0)
    , azimuthSlewRunning_(false)
    , altitudeSlewRunning_(false)
    , shutterOperationRunning_(false)
    , slavingRunning_(false)
    , emergencyStop_(false)
    , safetyOverride_(false)
    , canSetAzimuth_(true)
    , canSetAltitude_(false)  // Most domes don't support altitude control
    , canSetShutter_(true)
    , canPark_(true)
    , canFindHome_(true)
    , canSyncAzimuth_(true)
    , canSlave_(true) {
    
    initializeManufacturerSpecific();
}

Dome::~Dome() {
    stopDevice();
}

bool Dome::initializeDevice() {
    SPDLOG_INFO("Initializing dome device {}", getDeviceId());
    
    // Initialize properties
    setProperty("azimuth", currentAzimuth_.load());
    setProperty("altitude", currentAltitude_.load());
    setProperty("shutterState", static_cast<int>(shutterState_.load()));
    setProperty("isSlewing", isSlewing_.load());
    setProperty("isParked", isParked_.load());
    setProperty("isAtHome", isAtHome_.load());
    setProperty("isSlaved", isSlaved_.load());
    setProperty("homePosition", homePosition_);
    setProperty("parkPosition", parkPosition_);
    setProperty("slewRate", slewRate_);
    setProperty("domeRadius", domeRadius_);
    setProperty("emergencyStop", emergencyStop_.load());
    
    // Set capabilities
    setProperty("canSetAzimuth", canSetAzimuth_);
    setProperty("canSetAltitude", canSetAltitude_);
    setProperty("canSetShutter", canSetShutter_);
    setProperty("canPark", canPark_);
    setProperty("canFindHome", canFindHome_);
    setProperty("canSyncAzimuth", canSyncAzimuth_);
    setProperty("canSlave", canSlave_);
    
    return true;
}

bool Dome::startDevice() {
    SPDLOG_INFO("Starting dome device {}", getDeviceId());
    
    // Start control threads
    slavingRunning_ = true;
    slavingThread_ = std::thread(&Dome::slavingThread, this);
    
    return true;
}

void Dome::stopDevice() {
    SPDLOG_INFO("Stopping dome device {}", getDeviceId());
    
    // Stop all operations
    abortSlew();
    
    // Stop threads
    slavingRunning_ = false;
    azimuthSlewRunning_ = false;
    altitudeSlewRunning_ = false;
    shutterOperationRunning_ = false;
    
    slavingCV_.notify_all();
    azimuthSlewCV_.notify_all();
    altitudeSlewCV_.notify_all();
    shutterCV_.notify_all();
    
    if (slavingThread_.joinable()) {
        slavingThread_.join();
    }
    if (azimuthSlewThread_.joinable()) {
        azimuthSlewThread_.join();
    }
    if (altitudeSlewThread_.joinable()) {
        altitudeSlewThread_.join();
    }
    if (shutterThread_.joinable()) {
        shutterThread_.join();
    }
}

// IDome interface implementation
double Dome::getAzimuth() const {
    return currentAzimuth_.load();
}

bool Dome::getCanSetAzimuth() const {
    return canSetAzimuth_;
}

void Dome::slewToAzimuth(double azimuth) {
    if (!canSetAzimuth_) {
        throw std::runtime_error("Dome does not support azimuth control");
    }
    
    if (emergencyStop_.load()) {
        throw std::runtime_error("Dome is in emergency stop state");
    }
    
    double normalizedAzimuth = normalizeAzimuth(azimuth);
    if (!isAzimuthInRange(normalizedAzimuth)) {
        throw std::invalid_argument("Azimuth out of range");
    }
    
    targetAzimuth_ = normalizedAzimuth;
    
    // Start azimuth slew thread if not already running
    if (!azimuthSlewRunning_.load()) {
        azimuthSlewRunning_ = true;
        if (azimuthSlewThread_.joinable()) {
            azimuthSlewThread_.join();
        }
        azimuthSlewThread_ = std::thread(&Dome::azimuthSlewThread, this);
    }
    
    azimuthSlewCV_.notify_one();
    
    SPDLOG_INFO("Dome {} slewing to azimuth {:.2f}°", getDeviceId(), normalizedAzimuth);
}

void Dome::syncToAzimuth(double azimuth) {
    if (!canSyncAzimuth_) {
        throw std::runtime_error("Dome does not support azimuth sync");
    }
    
    double normalizedAzimuth = normalizeAzimuth(azimuth);
    currentAzimuth_ = normalizedAzimuth;
    setProperty("azimuth", normalizedAzimuth);
    
    SPDLOG_INFO("Dome {} synced to azimuth {:.2f}°", getDeviceId(), normalizedAzimuth);
}

bool Dome::getCanSyncAzimuth() const {
    return canSyncAzimuth_;
}

void Dome::abortSlew() {
    azimuthSlewRunning_ = false;
    altitudeSlewRunning_ = false;
    isSlewing_ = false;
    setProperty("isSlewing", false);
    
    executeAbortSlew();
    
    SPDLOG_INFO("Dome {} slew aborted", getDeviceId());
}

bool Dome::getSlewing() const {
    return isSlewing_.load();
}

double Dome::getAltitude() const {
    return currentAltitude_.load();
}

bool Dome::getCanSetAltitude() const {
    return canSetAltitude_;
}

void Dome::slewToAltitude(double altitude) {
    if (!canSetAltitude_) {
        throw std::runtime_error("Dome does not support altitude control");
    }
    
    if (!isAltitudeInRange(altitude)) {
        throw std::invalid_argument("Altitude out of range");
    }
    
    targetAltitude_ = altitude;
    
    // Start altitude slew thread if not already running
    if (!altitudeSlewRunning_.load()) {
        altitudeSlewRunning_ = true;
        if (altitudeSlewThread_.joinable()) {
            altitudeSlewThread_.join();
        }
        altitudeSlewThread_ = std::thread(&Dome::altitudeSlewThread, this);
    }
    
    altitudeSlewCV_.notify_one();
    
    SPDLOG_INFO("Dome {} slewing to altitude {:.2f}°", getDeviceId(), altitude);
}

interfaces::ShutterState Dome::getShutterStatus() const {
    return shutterState_.load();
}

bool Dome::getCanSetShutter() const {
    return canSetShutter_;
}

void Dome::openShutter() {
    if (!canSetShutter_) {
        throw std::runtime_error("Dome does not support shutter control");
    }
    
    if (shutterState_.load() == interfaces::ShutterState::OPEN) {
        return;  // Already open
    }
    
    shutterState_ = interfaces::ShutterState::OPENING;
    setProperty("shutterState", static_cast<int>(shutterState_.load()));
    
    // Start shutter operation thread
    shutterOperationRunning_ = true;
    if (shutterThread_.joinable()) {
        shutterThread_.join();
    }
    shutterThread_ = std::thread(&Dome::shutterControlThread, this);
    
    SPDLOG_INFO("Dome {} opening shutter", getDeviceId());
}

void Dome::closeShutter() {
    if (!canSetShutter_) {
        throw std::runtime_error("Dome does not support shutter control");
    }
    
    if (shutterState_.load() == interfaces::ShutterState::CLOSED) {
        return;  // Already closed
    }
    
    shutterState_ = interfaces::ShutterState::CLOSING;
    setProperty("shutterState", static_cast<int>(shutterState_.load()));
    
    // Start shutter operation thread
    shutterOperationRunning_ = true;
    if (shutterThread_.joinable()) {
        shutterThread_.join();
    }
    shutterThread_ = std::thread(&Dome::shutterControlThread, this);
    
    SPDLOG_INFO("Dome {} closing shutter", getDeviceId());
}

bool Dome::getCanPark() const {
    return canPark_;
}

void Dome::park() {
    if (!canPark_) {
        throw std::runtime_error("Dome does not support parking");
    }
    
    // Close shutter first if supported
    if (canSetShutter_ && shutterState_.load() != interfaces::ShutterState::CLOSED) {
        closeShutter();
        // Wait for shutter to close
        std::this_thread::sleep_for(std::chrono::seconds(shutterTimeout_));
    }
    
    // Slew to park position
    slewToAzimuth(parkPosition_);
    
    // Wait for slew to complete
    while (isSlewing_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    isParked_ = true;
    setProperty("isParked", true);
    
    executePark();
    
    SPDLOG_INFO("Dome {} parked", getDeviceId());
}

void Dome::setPark() {
    parkPosition_ = currentAzimuth_.load();
    setProperty("parkPosition", parkPosition_);
    
    SPDLOG_INFO("Dome {} park position set to {:.2f}°", getDeviceId(), parkPosition_);
}

bool Dome::getAtPark() const {
    return isParked_.load();
}

bool Dome::getCanFindHome() const {
    return canFindHome_;
}

void Dome::findHome() {
    if (!canFindHome_) {
        throw std::runtime_error("Dome does not support find home");
    }
    
    // Slew to home position
    slewToAzimuth(homePosition_);
    
    // Wait for slew to complete
    while (isSlewing_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    isAtHome_ = true;
    isParked_ = false;
    setProperty("isAtHome", true);
    setProperty("isParked", false);
    
    executeFindHome();
    
    SPDLOG_INFO("Dome {} found home", getDeviceId());
}

bool Dome::getAtHome() const {
    return isAtHome_.load();
}

bool Dome::getCanSlave() const {
    return canSlave_;
}

bool Dome::getSlaved() const {
    return isSlaved_.load();
}

void Dome::setSlaved(bool value) {
    if (!canSlave_) {
        throw std::runtime_error("Dome does not support slaving");
    }
    
    isSlaved_ = value;
    setProperty("isSlaved", value);
    
    if (value) {
        slavingCV_.notify_one();
    }
    
    SPDLOG_INFO("Dome {} slaving {}", getDeviceId(), value ? "enabled" : "disabled");
}

// IStateful interface implementation
bool Dome::setProperty(const std::string& property, const json& value) {
    if (property == "homePosition") {
        homePosition_ = value.get<double>();
        return true;
    } else if (property == "parkPosition") {
        parkPosition_ = value.get<double>();
        return true;
    } else if (property == "slewRate") {
        slewRate_ = value.get<double>();
        return true;
    } else if (property == "domeRadius") {
        domeRadius_ = value.get<double>();
        return true;
    } else if (property == "telescopeNorthOffset") {
        telescopeNorthOffset_ = value.get<double>();
        return true;
    } else if (property == "telescopeEastOffset") {
        telescopeEastOffset_ = value.get<double>();
        return true;
    } else if (property == "shutterTimeout") {
        shutterTimeout_ = value.get<int>();
        return true;
    } else if (property == "emergencyStop") {
        emergencyStop_ = value.get<bool>();
        if (emergencyStop_.load()) {
            abortSlew();
        }
        return true;
    }

    return ModernDeviceBase::setProperty(property, value);
}

json Dome::getProperty(const std::string& property) const {
    if (property == "azimuth") {
        return currentAzimuth_.load();
    } else if (property == "altitude") {
        return currentAltitude_.load();
    } else if (property == "shutterState") {
        return static_cast<int>(shutterState_.load());
    } else if (property == "isSlewing") {
        return isSlewing_.load();
    } else if (property == "isParked") {
        return isParked_.load();
    } else if (property == "isAtHome") {
        return isAtHome_.load();
    } else if (property == "isSlaved") {
        return isSlaved_.load();
    } else if (property == "homePosition") {
        return homePosition_;
    } else if (property == "parkPosition") {
        return parkPosition_;
    } else if (property == "slewRate") {
        return slewRate_;
    } else if (property == "domeRadius") {
        return domeRadius_;
    } else if (property == "telescopeNorthOffset") {
        return telescopeNorthOffset_;
    } else if (property == "telescopeEastOffset") {
        return telescopeEastOffset_;
    } else if (property == "shutterTimeout") {
        return shutterTimeout_;
    } else if (property == "emergencyStop") {
        return emergencyStop_.load();
    }

    return ModernDeviceBase::getProperty(property);
}

json Dome::getAllProperties() const {
    json properties = ModernDeviceBase::getAllProperties();

    properties["azimuth"] = currentAzimuth_.load();
    properties["altitude"] = currentAltitude_.load();
    properties["shutterState"] = static_cast<int>(shutterState_.load());
    properties["isSlewing"] = isSlewing_.load();
    properties["isParked"] = isParked_.load();
    properties["isAtHome"] = isAtHome_.load();
    properties["isSlaved"] = isSlaved_.load();
    properties["homePosition"] = homePosition_;
    properties["parkPosition"] = parkPosition_;
    properties["slewRate"] = slewRate_;
    properties["domeRadius"] = domeRadius_;
    properties["telescopeNorthOffset"] = telescopeNorthOffset_;
    properties["telescopeEastOffset"] = telescopeEastOffset_;
    properties["shutterTimeout"] = shutterTimeout_;
    properties["emergencyStop"] = emergencyStop_.load();

    return properties;
}

std::vector<std::string> Dome::getCapabilities() const {
    std::vector<std::string> capabilities = {
        "AZIMUTH_CONTROL",
        "SHUTTER_CONTROL",
        "PARK",
        "FIND_HOME",
        "SYNC_AZIMUTH"
    };

    if (canSetAltitude_) {
        capabilities.push_back("ALTITUDE_CONTROL");
    }

    if (canSlave_) {
        capabilities.push_back("TELESCOPE_SLAVING");
    }

    return capabilities;
}

// Additional dome-specific methods
void Dome::setTelescopeCoordinates(double ra, double dec, double alt, double az) {
    telescopeRA_ = ra;
    telescopeDec_ = dec;
    telescopeAlt_ = alt;
    telescopeAz_ = az;

    if (isSlaved_.load()) {
        slavingCV_.notify_one();
    }
}

double Dome::calculateRequiredAzimuth(double telescopeAz) const {
    // Simple calculation - in reality this would be more complex
    // accounting for dome geometry and telescope offset
    double requiredAz = telescopeAz;

    // Apply telescope offset corrections
    if (telescopeNorthOffset_ != 0.0 || telescopeEastOffset_ != 0.0) {
        // Simplified offset calculation
        double offsetAngle = std::atan2(telescopeEastOffset_, telescopeNorthOffset_) * 180.0 / M_PI;
        requiredAz += offsetAngle;
    }

    return normalizeAzimuth(requiredAz);
}

void Dome::setDomeRadius(double radius) {
    if (radius > 0.0) {
        domeRadius_ = radius;
        setProperty("domeRadius", radius);
    }
}

void Dome::setTelescopeOffset(double northOffset, double eastOffset) {
    telescopeNorthOffset_ = northOffset;
    telescopeEastOffset_ = eastOffset;
    setProperty("telescopeNorthOffset", northOffset);
    setProperty("telescopeEastOffset", eastOffset);
}

void Dome::setShutterTimeout(int timeoutSeconds) {
    if (timeoutSeconds > 0) {
        shutterTimeout_ = timeoutSeconds;
        setProperty("shutterTimeout", timeoutSeconds);
    }
}

void Dome::setSlewRate(double degreesPerSecond) {
    if (degreesPerSecond > 0.0) {
        slewRate_ = degreesPerSecond;
        setProperty("slewRate", degreesPerSecond);
    }
}

// Configuration methods
void Dome::setHomePosition(double azimuth) {
    homePosition_ = normalizeAzimuth(azimuth);
    setProperty("homePosition", homePosition_);
}

void Dome::setParkPosition(double azimuth) {
    parkPosition_ = normalizeAzimuth(azimuth);
    setProperty("parkPosition", parkPosition_);
}

void Dome::setAzimuthLimits(double minAzimuth, double maxAzimuth) {
    minAzimuth_ = normalizeAzimuth(minAzimuth);
    maxAzimuth_ = normalizeAzimuth(maxAzimuth);
    setProperty("minAzimuth", minAzimuth_);
    setProperty("maxAzimuth", maxAzimuth_);
}

void Dome::setShutterLimits(double minAltitude, double maxAltitude) {
    minAltitude_ = std::clamp(minAltitude, 0.0, 90.0);
    maxAltitude_ = std::clamp(maxAltitude, 0.0, 90.0);
    setProperty("minAltitude", minAltitude_);
    setProperty("maxAltitude", maxAltitude_);
}

// Hardware abstraction methods (simulation)
bool Dome::executeAzimuthSlew(double targetAzimuth) {
    SPDLOG_DEBUG("Dome {} executing azimuth slew to {:.2f}°", getDeviceId(), targetAzimuth);
    return true;
}

bool Dome::executeAltitudeSlew(double targetAltitude) {
    SPDLOG_DEBUG("Dome {} executing altitude slew to {:.2f}°", getDeviceId(), targetAltitude);
    return true;
}

bool Dome::executeShutterOpen() {
    SPDLOG_DEBUG("Dome {} executing shutter open", getDeviceId());
    return true;
}

bool Dome::executeShutterClose() {
    SPDLOG_DEBUG("Dome {} executing shutter close", getDeviceId());
    return true;
}

bool Dome::executeAbortSlew() {
    SPDLOG_DEBUG("Dome {} executing abort slew", getDeviceId());
    return true;
}

bool Dome::executePark() {
    SPDLOG_DEBUG("Dome {} executing park", getDeviceId());
    return true;
}

bool Dome::executeUnpark() {
    SPDLOG_DEBUG("Dome {} executing unpark", getDeviceId());
    return true;
}

bool Dome::executeFindHome() {
    SPDLOG_DEBUG("Dome {} executing find home", getDeviceId());
    return true;
}

double Dome::readCurrentAzimuth() {
    return currentAzimuth_.load();
}

double Dome::readCurrentAltitude() {
    return currentAltitude_.load();
}

interfaces::ShutterState Dome::readShutterStatus() {
    return shutterState_.load();
}

// Movement control threads
void Dome::azimuthSlewThread() {
    while (azimuthSlewRunning_.load()) {
        std::unique_lock<std::mutex> lock(azimuthSlewMutex_);
        azimuthSlewCV_.wait(lock, [this] { return !azimuthSlewRunning_.load() || targetAzimuth_.load() != currentAzimuth_.load(); });

        if (!azimuthSlewRunning_.load()) {
            break;
        }

        double target = targetAzimuth_.load();
        double current = currentAzimuth_.load();

        if (std::abs(target - current) > 0.1) {
            isSlewing_ = true;
            setProperty("isSlewing", true);

            executeAzimuthSlew(target);

            // Simulate movement
            double direction = calculateShortestPath(current, target);
            double step = slewRate_ * 0.1; // 100ms update interval

            if (std::abs(direction) < step) {
                currentAzimuth_ = target;
            } else {
                currentAzimuth_ = normalizeAzimuth(current + (direction > 0 ? step : -step));
            }

            setProperty("azimuth", currentAzimuth_.load());

            if (std::abs(currentAzimuth_.load() - target) < 0.1) {
                isSlewing_ = false;
                isParked_ = (std::abs(currentAzimuth_.load() - parkPosition_) < 0.1);
                isAtHome_ = (std::abs(currentAzimuth_.load() - homePosition_) < 0.1);
                setProperty("isSlewing", false);
                setProperty("isParked", isParked_.load());
                setProperty("isAtHome", isAtHome_.load());
                SPDLOG_INFO("Dome {} azimuth slew completed", getDeviceId());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Dome::altitudeSlewThread() {
    while (altitudeSlewRunning_.load()) {
        std::unique_lock<std::mutex> lock(altitudeSlewMutex_);
        altitudeSlewCV_.wait(lock, [this] { return !altitudeSlewRunning_.load() || targetAltitude_.load() != currentAltitude_.load(); });

        if (!altitudeSlewRunning_.load()) {
            break;
        }

        double target = targetAltitude_.load();
        double current = currentAltitude_.load();

        if (std::abs(target - current) > 0.1) {
            executeAltitudeSlew(target);

            // Simulate movement
            double step = slewRate_ * 0.1; // 100ms update interval
            double diff = target - current;

            if (std::abs(diff) < step) {
                currentAltitude_ = target;
            } else {
                currentAltitude_ = current + (diff > 0 ? step : -step);
            }

            setProperty("altitude", currentAltitude_.load());

            if (std::abs(currentAltitude_.load() - target) < 0.1) {
                SPDLOG_INFO("Dome {} altitude slew completed", getDeviceId());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Dome::shutterControlThread() {
    auto currentState = shutterState_.load();

    if (currentState == interfaces::ShutterState::OPENING) {
        executeShutterOpen();

        // Simulate shutter opening time
        std::this_thread::sleep_for(std::chrono::seconds(shutterTimeout_));

        shutterState_ = interfaces::ShutterState::OPEN;
        setProperty("shutterState", static_cast<int>(shutterState_.load()));
        SPDLOG_INFO("Dome {} shutter opened", getDeviceId());

    } else if (currentState == interfaces::ShutterState::CLOSING) {
        executeShutterClose();

        // Simulate shutter closing time
        std::this_thread::sleep_for(std::chrono::seconds(shutterTimeout_));

        shutterState_ = interfaces::ShutterState::CLOSED;
        setProperty("shutterState", static_cast<int>(shutterState_.load()));
        SPDLOG_INFO("Dome {} shutter closed", getDeviceId());
    }

    shutterOperationRunning_ = false;
}

void Dome::slavingThread() {
    while (slavingRunning_.load()) {
        std::unique_lock<std::mutex> lock(slavingMutex_);
        slavingCV_.wait_for(lock, std::chrono::seconds(1), [this] { return !slavingRunning_.load() || isSlaved_.load(); });

        if (!slavingRunning_.load()) {
            break;
        }

        if (isSlaved_.load()) {
            updateSlavingPosition();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// Utility methods
double Dome::normalizeAzimuth(double azimuth) const {
    while (azimuth < 0.0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;
    return azimuth;
}

bool Dome::isAzimuthInRange(double azimuth) const {
    if (minAzimuth_ <= maxAzimuth_) {
        return azimuth >= minAzimuth_ && azimuth <= maxAzimuth_;
    } else {
        // Handle wrap-around case
        return azimuth >= minAzimuth_ || azimuth <= maxAzimuth_;
    }
}

bool Dome::isAltitudeInRange(double altitude) const {
    return altitude >= minAltitude_ && altitude <= maxAltitude_;
}

double Dome::calculateShortestPath(double current, double target) const {
    double diff = target - current;

    if (diff > 180.0) {
        diff -= 360.0;
    } else if (diff < -180.0) {
        diff += 360.0;
    }

    return diff;
}

void Dome::updateSlavingPosition() {
    if (!isSlaved_.load()) {
        return;
    }

    double telescopeAz = telescopeAz_.load();
    double requiredAz = calculateRequiredAzimuth(telescopeAz);

    // Only slew if difference is significant
    if (std::abs(calculateShortestPath(currentAzimuth_.load(), requiredAz)) > 2.0) {
        slewToAzimuth(requiredAz);
    }
}

void Dome::checkSafetyLimits() {
    // Check for emergency conditions
    if (emergencyStop_.load()) {
        abortSlew();
        return;
    }

    // Check azimuth limits
    if (!isAzimuthInRange(currentAzimuth_.load())) {
        SPDLOG_WARN("Dome {} azimuth {:.2f}° is out of range [{:.2f}°, {:.2f}°]",
                   getDeviceId(), currentAzimuth_.load(), minAzimuth_, maxAzimuth_);
    }

    // Check altitude limits
    if (!isAltitudeInRange(currentAltitude_.load())) {
        SPDLOG_WARN("Dome {} altitude {:.2f}° is out of range [{:.2f}°, {:.2f}°]",
                   getDeviceId(), currentAltitude_.load(), minAltitude_, maxAltitude_);
    }
}

// ModernDeviceBase overrides
bool Dome::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "SLEW_TO_AZIMUTH") {
        double azimuth = parameters.at("azimuth").get<double>();
        slewToAzimuth(azimuth);
        result["success"] = true;
        return true;
    } else if (command == "SYNC_TO_AZIMUTH") {
        double azimuth = parameters.at("azimuth").get<double>();
        syncToAzimuth(azimuth);
        result["success"] = true;
        return true;
    } else if (command == "OPEN_SHUTTER") {
        openShutter();
        result["success"] = true;
        return true;
    } else if (command == "CLOSE_SHUTTER") {
        closeShutter();
        result["success"] = true;
        return true;
    } else if (command == "PARK") {
        park();
        result["success"] = true;
        return true;
    } else if (command == "FIND_HOME") {
        findHome();
        result["success"] = true;
        return true;
    } else if (command == "SET_SLAVED") {
        bool slaved = parameters.at("slaved").get<bool>();
        setSlaved(slaved);
        result["success"] = true;
        return true;
    } else if (command == "SET_TELESCOPE_COORDINATES") {
        double ra = parameters.at("ra").get<double>();
        double dec = parameters.at("dec").get<double>();
        double alt = parameters.at("alt").get<double>();
        double az = parameters.at("az").get<double>();
        setTelescopeCoordinates(ra, dec, alt, az);
        result["success"] = true;
        return true;
    }

    return false;
}

void Dome::updateDevice() {
    checkSafetyLimits();

    // Update properties
    setProperty("azimuth", currentAzimuth_.load());
    setProperty("altitude", currentAltitude_.load());
    setProperty("shutterState", static_cast<int>(shutterState_.load()));
    setProperty("isSlewing", isSlewing_.load());
    setProperty("isParked", isParked_.load());
    setProperty("isAtHome", isAtHome_.load());
    setProperty("isSlaved", isSlaved_.load());
}

// Manufacturer-specific initialization
void Dome::initializeManufacturerSpecific() {
    std::string manufacturer = getProperty("manufacturer").get<std::string>();

    if (manufacturer == "Generic") {
        initializeGeneric();
    } else if (manufacturer == "Ash Manufacturing") {
        initializeAshManufacturing();
    } else if (manufacturer == "Technical Innovations") {
        initializeTechnicalInnovations();
    } else if (manufacturer == "Sirius Observatories") {
        initializeSiriusObservatories();
    } else if (manufacturer == "NexDome") {
        initializeNexDome();
    } else {
        initializeGeneric();
    }
}

void Dome::initializeGeneric() {
    // Generic dome settings
    slewRate_ = 5.0;
    shutterTimeout_ = 30;
    canSetAltitude_ = false;
}

void Dome::initializeAshManufacturing() {
    // Ash Manufacturing specific settings
    slewRate_ = 3.0;
    shutterTimeout_ = 45;
    canSetAltitude_ = true;
}

void Dome::initializeTechnicalInnovations() {
    // Technical Innovations specific settings
    slewRate_ = 8.0;
    shutterTimeout_ = 20;
    canSetAltitude_ = false;
}

void Dome::initializeSiriusObservatories() {
    // Sirius Observatories specific settings
    slewRate_ = 6.0;
    shutterTimeout_ = 25;
    canSetAltitude_ = true;
}

void Dome::initializeNexDome() {
    // NexDome specific settings
    slewRate_ = 10.0;
    shutterTimeout_ = 15;
    canSetAltitude_ = false;
}

// Factory function
std::unique_ptr<Dome> createModernDome(const std::string& deviceId,
                                       const std::string& manufacturer,
                                       const std::string& model) {
    return std::make_unique<Dome>(deviceId, manufacturer, model);
}

} // namespace device
} // namespace hydrogen
