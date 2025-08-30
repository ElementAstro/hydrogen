#include "guider.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <random>
#include <cmath>

namespace astrocomm {
namespace device {

Guider::Guider(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "GUIDER", manufacturer, model)
    , guiderState_(GuiderState::DISCONNECTED)
    , calibrationState_(CalibrationState::IDLE)
    , isGuiding_(false)
    , isCalibrated_(false)
    , isDithering_(false)
    , rmsError_(0.0)
    , peakError_(0.0)
    , raError_(0.0)
    , decError_(0.0)
    , totalFrames_(0)
    , droppedFrames_(0)
    , guidingDuration_(0)
    , exposureTime_(1.0)
    , settleTime_(2.0)
    , ditherAmount_(3.0)
    , aggressiveness_(100)
    , minMovePixels_(0.15)
    , maxMovePixels_(15.0)
    , guidingThreadRunning_(false) {
    
    SPDLOG_INFO("Guider {} created with manufacturer: {}, model: {}", deviceId, manufacturer, model);
}

Guider::~Guider() {
    stop();
}

bool Guider::initializeDevice() {
    // Initialize guider-specific properties
    setProperty("guiderState", static_cast<int>(guiderState_));
    setProperty("calibrationState", static_cast<int>(calibrationState_));
    setProperty("isGuiding", isGuiding_.load());
    setProperty("isCalibrated", isCalibrated_.load());
    setProperty("isDithering", isDithering_.load());
    setProperty("rmsError", rmsError_.load());
    setProperty("peakError", peakError_.load());
    setProperty("exposureTime", exposureTime_.load());
    setProperty("settleTime", settleTime_.load());
    setProperty("ditherAmount", ditherAmount_.load());
    setProperty("aggressiveness", aggressiveness_.load());
    setProperty("minMovePixels", minMovePixels_.load());
    setProperty("maxMovePixels", maxMovePixels_.load());
    
    return true;
}

bool Guider::startDevice() {
    // Start guiding thread
    guidingThreadRunning_ = true;
    guidingThread_ = std::thread(&Guider::guidingThreadFunction, this);
    
    return true;
}

void Guider::stopDevice() {
    // Stop guiding thread
    guidingThreadRunning_ = false;
    if (guidingThread_.joinable()) {
        guidingThread_.join();
    }
    
    // Stop any ongoing guiding
    if (isGuiding_) {
        stopGuiding();
    }
}

// IGuider interface implementation
bool Guider::startGuiding() {
    if (!isCalibrated_) {
        SPDLOG_ERROR("Guider {} cannot start guiding without calibration", getDeviceId());
        return false;
    }
    
    if (isGuiding_) {
        SPDLOG_WARN("Guider {} already guiding", getDeviceId());
        return true;
    }
    
    isGuiding_ = true;
    guiderState_ = GuiderState::GUIDING;
    guidingStartTime_ = std::chrono::steady_clock::now();
    
    setProperty("isGuiding", true);
    setProperty("guiderState", static_cast<int>(GuiderState::GUIDING));
    
    SPDLOG_INFO("Guider {} started guiding", getDeviceId());
    return executeStartGuiding();
}

bool Guider::stopGuiding() {
    if (!isGuiding_) {
        return true;
    }
    
    isGuiding_ = false;
    guiderState_ = GuiderState::CONNECTED;
    
    setProperty("isGuiding", false);
    setProperty("guiderState", static_cast<int>(GuiderState::CONNECTED));
    
    SPDLOG_INFO("Guider {} stopped guiding", getDeviceId());
    return executeStopGuiding();
}

bool Guider::isGuiding() const {
    return isGuiding_;
}

bool Guider::startCalibration() {
    if (isGuiding_) {
        SPDLOG_ERROR("Guider {} cannot calibrate while guiding", getDeviceId());
        return false;
    }
    
    calibrationState_ = CalibrationState::CALIBRATING;
    isCalibrated_ = false;
    
    setProperty("calibrationState", static_cast<int>(CalibrationState::CALIBRATING));
    setProperty("isCalibrated", false);
    
    SPDLOG_INFO("Guider {} started calibration", getDeviceId());
    return executeCalibration();
}

bool Guider::isCalibrated() const {
    return isCalibrated_;
}

bool Guider::dither(double amount) {
    if (!isGuiding_) {
        SPDLOG_ERROR("Guider {} cannot dither while not guiding", getDeviceId());
        return false;
    }
    
    if (isDithering_) {
        SPDLOG_WARN("Guider {} already dithering", getDeviceId());
        return false;
    }
    
    isDithering_ = true;
    ditherAmount_ = amount;
    
    setProperty("isDithering", true);
    setProperty("ditherAmount", amount);
    
    SPDLOG_INFO("Guider {} started dithering with amount {:.2f}", getDeviceId(), amount);
    return executeDither(amount);
}

bool Guider::isDithering() const {
    return isDithering_;
}

GuiderStats Guider::getGuidingStats() const {
    GuiderStats stats;
    stats.rmsError = rmsError_;
    stats.peakError = peakError_;
    stats.raError = raError_;
    stats.decError = decError_;
    stats.totalFrames = totalFrames_;
    stats.droppedFrames = droppedFrames_;
    stats.guidingDuration = guidingDuration_;
    
    return stats;
}

// Hardware abstraction methods (simulation)
bool Guider::executeStartGuiding() {
    SPDLOG_DEBUG("Guider {} executing start guiding", getDeviceId());
    return true;
}

bool Guider::executeStopGuiding() {
    SPDLOG_DEBUG("Guider {} executing stop guiding", getDeviceId());
    return true;
}

bool Guider::executeCalibration() {
    SPDLOG_DEBUG("Guider {} executing calibration", getDeviceId());
    
    // Simulate calibration process
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(10)); // Simulate calibration time
        
        isCalibrated_ = true;
        calibrationState_ = CalibrationState::COMPLETE;
        
        setProperty("isCalibrated", true);
        setProperty("calibrationState", static_cast<int>(CalibrationState::COMPLETE));
        
        SPDLOG_INFO("Guider {} calibration completed", getDeviceId());
    }).detach();
    
    return true;
}

bool Guider::executeDither(double amount) {
    SPDLOG_DEBUG("Guider {} executing dither with amount {:.2f}", getDeviceId(), amount);
    
    // Simulate dithering process
    std::thread([this, amount]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(settleTime_ * 1000)));
        
        isDithering_ = false;
        setProperty("isDithering", false);
        
        SPDLOG_INFO("Guider {} dithering completed", getDeviceId());
    }).detach();
    
    return true;
}

void Guider::guidingThreadFunction() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> errorDist(0.0, 0.5); // Simulate guiding errors
    
    while (guidingThreadRunning_) {
        if (isGuiding_ && guiderState_ == GuiderState::GUIDING) {
            // Simulate guiding corrections
            double raErrorPixels = errorDist(gen);
            double decErrorPixels = errorDist(gen);
            
            raError_ = raErrorPixels;
            decError_ = decErrorPixels;
            
            // Calculate RMS and peak errors
            double currentError = std::sqrt(raErrorPixels * raErrorPixels + decErrorPixels * decErrorPixels);
            rmsError_ = (rmsError_ * 0.9) + (currentError * 0.1); // Running average
            peakError_ = std::max(peakError_.load(), currentError);
            
            totalFrames_++;
            
            setProperty("rmsError", rmsError_.load());
            setProperty("peakError", peakError_.load());
            setProperty("raError", raError_.load());
            setProperty("decError", decError_.load());
            setProperty("totalFrames", totalFrames_.load());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(exposureTime_ * 1000)));
    }
}

bool Guider::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "START_GUIDING") {
        result["success"] = startGuiding();
        return true;
    }
    else if (command == "STOP_GUIDING") {
        result["success"] = stopGuiding();
        return true;
    }
    else if (command == "START_CALIBRATION") {
        result["success"] = startCalibration();
        return true;
    }
    else if (command == "DITHER") {
        double amount = parameters.value("amount", ditherAmount_.load());
        result["success"] = dither(amount);
        return true;
    }
    
    return false;
}

void Guider::updateDevice() {
    // Update guiding state and statistics
    setProperty("guiderState", static_cast<int>(guiderState_));
    setProperty("calibrationState", static_cast<int>(calibrationState_));
    setProperty("isGuiding", isGuiding_.load());
    setProperty("isCalibrated", isCalibrated_.load());
    setProperty("isDithering", isDithering_.load());
    
    // Update error statistics
    setProperty("rmsError", rmsError_.load());
    setProperty("peakError", peakError_.load());
    setProperty("raError", raError_.load());
    setProperty("decError", decError_.load());
}

std::vector<std::string> Guider::getCapabilities() const {
    return {
        "START_GUIDING",
        "STOP_GUIDING",
        "START_CALIBRATION",
        "DITHER"
    };
}

} // namespace device
} // namespace astrocomm
