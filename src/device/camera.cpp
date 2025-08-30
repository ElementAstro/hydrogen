#include "camera.h"
#include "behaviors/temperature_control_behavior.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <random>

namespace astrocomm {
namespace device {

// CameraTemperatureBehavior implementation
Camera::CameraTemperatureBehavior::CameraTemperatureBehavior(Camera* camera)
    : TemperatureControlBehavior("camera_temperature"), camera_(camera) {
}

double Camera::CameraTemperatureBehavior::readCurrentTemperature() {
    return camera_->readTemperature();
}

double Camera::CameraTemperatureBehavior::readAmbientTemperature() {
    // Simulate ambient temperature reading
    return 20.0 + (std::rand() % 10 - 5) * 0.1; // 15-25°C range
}

bool Camera::CameraTemperatureBehavior::setControlPower(double power) {
    return camera_->setTemperatureControl(power);
}

// Camera implementation
Camera::Camera(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "CAMERA", manufacturer, model)
    , temperatureBehavior_(nullptr)
    , cameraState_(CameraState::IDLE)
    , exposureDuration_(0.0)
    , exposureStartTime_(0.0)
    , exposureInProgress_(false)
    , currentGain_(0)
    , currentOffset_(0)
    , binningX_(1)
    , binningY_(1)
    , roiX_(0)
    , roiY_(0)
    , roiWidth_(1920)
    , roiHeight_(1080)
    , coolerEnabled_(false)
    , coolerPower_(0.0)
    , exposureThreadRunning_(false) {
    
    // Set default camera parameters based on manufacturer
    if (manufacturer == "ZWO") {
        cameraParams_.width = 4144;
        cameraParams_.height = 2822;
        cameraParams_.pixelSizeX = 4.63;
        cameraParams_.pixelSizeY = 4.63;
        cameraParams_.maxGain = 600;
    } else if (manufacturer == "QHY") {
        cameraParams_.width = 6280;
        cameraParams_.height = 4210;
        cameraParams_.pixelSizeX = 3.76;
        cameraParams_.pixelSizeY = 3.76;
        cameraParams_.maxGain = 400;
    } else {
        // Generic defaults
        cameraParams_.width = 1920;
        cameraParams_.height = 1080;
        cameraParams_.pixelSizeX = 5.0;
        cameraParams_.pixelSizeY = 5.0;
        cameraParams_.maxGain = 100;
    }
    
    roiWidth_ = cameraParams_.width;
    roiHeight_ = cameraParams_.height;
    
    SPDLOG_INFO("Camera {} created with manufacturer: {}, model: {}", deviceId, manufacturer, model);
}

Camera::~Camera() {
    stop();
}

bool Camera::initializeDevice() {
    initializeCameraBehaviors();
    
    // Initialize camera-specific properties
    setProperty("cameraState", static_cast<int>(CameraState::IDLE));
    setProperty("exposureInProgress", false);
    setProperty("gain", currentGain_.load());
    setProperty("offset", currentOffset_.load());
    setProperty("binningX", binningX_.load());
    setProperty("binningY", binningY_.load());
    setProperty("coolerEnabled", coolerEnabled_.load());
    setProperty("coolerPower", coolerPower_.load());
    
    return true;
}

bool Camera::startDevice() {
    // Start exposure thread
    exposureThreadRunning_ = true;
    exposureThread_ = std::thread(&Camera::exposureThreadFunction, this);
    
    return true;
}

void Camera::stopDevice() {
    // Stop exposure thread
    exposureThreadRunning_ = false;
    if (exposureThread_.joinable()) {
        exposureThread_.join();
    }
    
    // Stop any ongoing exposure
    if (exposureInProgress_) {
        stopExposure();
    }
}

void Camera::initializeCameraBehaviors() {
    // Add temperature control behavior
    temperatureBehavior_ = new CameraTemperatureBehavior(this);
    addBehavior(std::unique_ptr<behaviors::DeviceBehavior>(temperatureBehavior_));
}

// ICamera interface implementation
bool Camera::startExposure(double duration) {
    if (exposureInProgress_) {
        SPDLOG_WARN("Camera {} exposure already in progress", getDeviceId());
        return false;
    }
    
    if (duration <= 0) {
        SPDLOG_ERROR("Camera {} invalid exposure duration: {}", getDeviceId(), duration);
        return false;
    }
    
    exposureDuration_ = duration;
    exposureStartTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
    exposureInProgress_ = true;
    cameraState_ = CameraState::EXPOSING;
    
    setProperty("exposureInProgress", true);
    setProperty("cameraState", static_cast<int>(CameraState::EXPOSING));
    setProperty("exposureDuration", duration);
    
    SPDLOG_INFO("Camera {} started exposure for {:.3f} seconds", getDeviceId(), duration);
    return executeExposure(duration);
}

bool Camera::stopExposure() {
    if (!exposureInProgress_) {
        return true;
    }
    
    exposureInProgress_ = false;
    cameraState_ = CameraState::IDLE;
    
    setProperty("exposureInProgress", false);
    setProperty("cameraState", static_cast<int>(CameraState::IDLE));
    
    SPDLOG_INFO("Camera {} exposure stopped", getDeviceId());
    return executeStopExposure();
}

bool Camera::isExposing() const {
    return exposureInProgress_;
}

std::vector<uint8_t> Camera::getImageData() const {
    std::lock_guard<std::mutex> lock(imageDataMutex_);
    return imageData_;
}

bool Camera::setGain(int gain) {
    if (gain < 0 || gain > cameraParams_.maxGain) {
        SPDLOG_ERROR("Camera {} invalid gain value: {}", getDeviceId(), gain);
        return false;
    }
    
    currentGain_ = gain;
    setProperty("gain", gain);
    
    SPDLOG_DEBUG("Camera {} gain set to {}", getDeviceId(), gain);
    return true;
}

int Camera::getGain() const {
    return currentGain_;
}

bool Camera::setROI(int x, int y, int width, int height) {
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > cameraParams_.width || y + height > cameraParams_.height) {
        SPDLOG_ERROR("Camera {} invalid ROI: ({}, {}, {}, {})", getDeviceId(), x, y, width, height);
        return false;
    }
    
    roiX_ = x;
    roiY_ = y;
    roiWidth_ = width;
    roiHeight_ = height;
    
    setProperty("roiX", x);
    setProperty("roiY", y);
    setProperty("roiWidth", width);
    setProperty("roiHeight", height);
    
    SPDLOG_DEBUG("Camera {} ROI set to ({}, {}, {}, {})", getDeviceId(), x, y, width, height);
    return true;
}

// ITemperatureControlled interface implementation
bool Camera::setTargetTemperature(double temperature) {
    if (temperatureBehavior_) {
        return temperatureBehavior_->setTargetTemperature(temperature);
    }
    return false;
}

double Camera::getCurrentTemperature() const {
    if (temperatureBehavior_) {
        return temperatureBehavior_->getCurrentTemperature();
    }
    return 20.0; // Default room temperature
}

double Camera::getTargetTemperature() const {
    if (temperatureBehavior_) {
        return temperatureBehavior_->getTargetTemperature();
    }
    return 20.0;
}

bool Camera::stopTemperatureControl() {
    if (temperatureBehavior_) {
        return temperatureBehavior_->stopControl();
    }
    return true;
}

bool Camera::isTemperatureStable() const {
    if (temperatureBehavior_) {
        return temperatureBehavior_->isStable();
    }
    return true;
}

// Extended functionality
bool Camera::expose(double duration, bool synchronous) {
    if (!startExposure(duration)) {
        return false;
    }
    
    if (synchronous) {
        return waitForExposureComplete();
    }
    
    return true;
}

bool Camera::abort() {
    return stopExposure();
}

bool Camera::setCameraParameters(const CameraParameters& params) {
    cameraParams_ = params;
    
    // Update ROI to match new sensor size if needed
    if (roiWidth_ > params.width || roiHeight_ > params.height) {
        setROI(0, 0, params.width, params.height);
    }
    
    setProperty("cameraParameters", json{
        {"width", params.width},
        {"height", params.height},
        {"bitDepth", params.bitDepth},
        {"hasColorSensor", params.hasColorSensor},
        {"hasCooler", params.hasCooler}
    });
    
    return true;
}

CameraParameters Camera::getCameraParameters() const {
    return cameraParams_;
}

bool Camera::setBinning(int binX, int binY) {
    if (binX < 1 || binY < 1 || binX > cameraParams_.maxBinningX || binY > cameraParams_.maxBinningY) {
        SPDLOG_ERROR("Camera {} invalid binning: {}x{}", getDeviceId(), binX, binY);
        return false;
    }
    
    binningX_ = binX;
    binningY_ = binY;
    
    setProperty("binningX", binX);
    setProperty("binningY", binY);
    
    return true;
}

void Camera::getBinning(int& binX, int& binY) const {
    binX = binningX_;
    binY = binningY_;
}

bool Camera::setOffset(int offset) {
    if (offset < 0 || offset > cameraParams_.maxOffset) {
        SPDLOG_ERROR("Camera {} invalid offset: {}", getDeviceId(), offset);
        return false;
    }
    
    currentOffset_ = offset;
    setProperty("offset", offset);
    
    return true;
}

int Camera::getOffset() const {
    return currentOffset_;
}

CameraState Camera::getCameraState() const {
    return cameraState_;
}

double Camera::getExposureProgress() const {
    if (!exposureInProgress_) {
        return 0.0;
    }
    
    double currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
    double elapsed = currentTime - exposureStartTime_;
    
    return std::min(1.0, elapsed / exposureDuration_);
}

double Camera::getRemainingExposureTime() const {
    if (!exposureInProgress_) {
        return 0.0;
    }
    
    double progress = getExposureProgress();
    return exposureDuration_ * (1.0 - progress);
}

bool Camera::setCoolerEnabled(bool enabled) {
    coolerEnabled_ = enabled;
    setProperty("coolerEnabled", enabled);
    
    if (!enabled) {
        coolerPower_ = 0.0;
        setProperty("coolerPower", 0.0);
    }
    
    return true;
}

bool Camera::isCoolerEnabled() const {
    return coolerEnabled_;
}

double Camera::getCoolerPower() const {
    return coolerPower_;
}

bool Camera::setImageFormat(const std::string& format) {
    // Validate format
    auto supportedFormats = getSupportedImageFormats();
    if (std::find(supportedFormats.begin(), supportedFormats.end(), format) == supportedFormats.end()) {
        return false;
    }
    
    setProperty("imageFormat", format);
    return true;
}

std::vector<std::string> Camera::getSupportedImageFormats() const {
    return {"FITS", "TIFF", "PNG", "JPEG", "RAW"};
}

bool Camera::saveImage(const std::string& filename, const std::string& format) {
    std::lock_guard<std::mutex> lock(imageDataMutex_);
    
    if (imageData_.empty()) {
        SPDLOG_ERROR("Camera {} no image data to save", getDeviceId());
        return false;
    }
    
    // TODO: Implement actual image saving logic based on format
    SPDLOG_INFO("Camera {} saving image to {} in {} format", getDeviceId(), filename, format);
    return true;
}

json Camera::getImageStatistics() const {
    std::lock_guard<std::mutex> lock(imageDataMutex_);
    
    if (imageData_.empty()) {
        return json::object();
    }
    
    // Calculate basic statistics
    // TODO: Implement actual statistics calculation
    return json{
        {"mean", 1000.0},
        {"median", 950.0},
        {"stddev", 150.0},
        {"min", 0},
        {"max", 65535}
    };
}

bool Camera::waitForExposureComplete(int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (exposureInProgress_) {
        if (timeoutMs > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= timeoutMs) {
                return false;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return true;
}

// Hardware abstraction methods (to be overridden by specific camera implementations)
bool Camera::executeExposure(double duration) {
    // Simulate exposure start
    SPDLOG_DEBUG("Camera {} executing exposure for {:.3f} seconds", getDeviceId(), duration);
    return true;
}

bool Camera::executeStopExposure() {
    // Simulate exposure stop
    SPDLOG_DEBUG("Camera {} stopping exposure", getDeviceId());
    return true;
}

std::vector<uint8_t> Camera::executeImageDownload() {
    // Simulate image data generation
    int imageSize = roiWidth_ * roiHeight_ * (cameraParams_.bitDepth / 8);
    std::vector<uint8_t> data(imageSize);
    
    // Fill with simulated data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }
    
    return data;
}

double Camera::readTemperature() {
    // Simulate temperature reading
    static double simulatedTemp = 20.0;
    
    if (coolerEnabled_) {
        // Simulate cooling effect
        double targetTemp = getTargetTemperature();
        double error = targetTemp - simulatedTemp;
        simulatedTemp += error * 0.1; // Simple cooling simulation
    }
    
    return simulatedTemp;
}

bool Camera::setTemperatureControl(double power) {
    if (!cameraParams_.hasCooler) {
        return false;
    }
    
    coolerPower_ = std::clamp(power, 0.0, 100.0);
    setProperty("coolerPower", coolerPower_.load());
    
    return true;
}

void Camera::exposureThreadFunction() {
    while (exposureThreadRunning_) {
        if (exposureInProgress_) {
            double currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
            double elapsed = currentTime - exposureStartTime_;
            
            if (elapsed >= exposureDuration_) {
                // Exposure complete
                cameraState_ = CameraState::DOWNLOADING;
                setProperty("cameraState", static_cast<int>(CameraState::DOWNLOADING));
                
                // Simulate image download
                auto newImageData = executeImageDownload();
                {
                    std::lock_guard<std::mutex> lock(imageDataMutex_);
                    imageData_ = std::move(newImageData);
                }
                
                exposureInProgress_ = false;
                cameraState_ = CameraState::IDLE;
                setProperty("exposureInProgress", false);
                setProperty("cameraState", static_cast<int>(CameraState::IDLE));
                
                SPDLOG_INFO("Camera {} exposure completed", getDeviceId());
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Camera::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "START_EXPOSURE") {
        double duration = parameters.value("duration", 1.0);
        result["success"] = startExposure(duration);
        return true;
    }
    else if (command == "STOP_EXPOSURE") {
        result["success"] = stopExposure();
        return true;
    }
    else if (command == "GET_IMAGE_DATA") {
        auto data = getImageData();
        result["imageSize"] = data.size();
        result["success"] = !data.empty();
        return true;
    }
    else if (command == "SET_GAIN") {
        int gain = parameters.value("gain", 0);
        result["success"] = setGain(gain);
        return true;
    }
    else if (command == "SET_ROI") {
        int x = parameters.value("x", 0);
        int y = parameters.value("y", 0);
        int width = parameters.value("width", cameraParams_.width);
        int height = parameters.value("height", cameraParams_.height);
        result["success"] = setROI(x, y, width, height);
        return true;
    }
    
    return false;
}

void Camera::updateDevice() {
    // Update exposure progress
    if (exposureInProgress_) {
        setProperty("exposureProgress", getExposureProgress());
        setProperty("remainingTime", getRemainingExposureTime());
    }
    
    // Update temperature if cooler is available
    if (cameraParams_.hasCooler) {
        setProperty("currentTemperature", getCurrentTemperature());
    }
}

} // namespace device
} // namespace astrocomm
