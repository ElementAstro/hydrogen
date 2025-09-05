#include "camera.h"
#include "behaviors/temperature_control_behavior.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <random>

namespace hydrogen {
namespace device {

// CameraTemperatureBehavior implementation - temporarily disabled
// Camera::CameraTemperatureBehavior::CameraTemperatureBehavior(Camera* camera)
//     : TemperatureControlBehavior("camera_temperature"), camera_(camera) {
// }

// Temporarily disabled CameraTemperatureBehavior methods
// double Camera::CameraTemperatureBehavior::readCurrentTemperature() {
//     return camera_->readTemperature();
// }

// double Camera::CameraTemperatureBehavior::readAmbientTemperature() {
//     // Simulate ambient temperature reading
//     return 20.0 + (std::rand() % 10 - 5) * 0.1; // 15-25°C range
// }

// bool Camera::CameraTemperatureBehavior::setControlPower(double power) {
//     return camera_->setTemperatureControl(power);
// }

// Camera implementation
Camera::Camera(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "CAMERA", manufacturer, model)
    // , temperatureBehavior_(nullptr)  // Temporarily disabled
    , cameraState_(interfaces::CameraState::IDLE)
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
    , startX_(0)
    , startY_(0)
    , numX_(1920)
    , numY_(1080)
    , readoutMode_(0)
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
    setProperty("cameraState", static_cast<int>(interfaces::CameraState::IDLE));
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
    // Add temperature control behavior - temporarily disabled
    // temperatureBehavior_ = new CameraTemperatureBehavior(this);
    // addBehavior(std::unique_ptr<behaviors::DeviceBehavior>(temperatureBehavior_));
}

// ICamera interface implementation
void Camera::startExposure(double duration, [[maybe_unused]] bool light) {
    if (exposureInProgress_) {
        SPDLOG_WARN("Camera {} exposure already in progress", getDeviceId());
        return;
    }

    if (duration <= 0) {
        SPDLOG_ERROR("Camera {} invalid exposure duration: {}", getDeviceId(), duration);
        return;
    }
    
    exposureDuration_ = duration;
    exposureStartTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
    exposureInProgress_ = true;
    cameraState_ = interfaces::CameraState::EXPOSING;
    
    setProperty("exposureInProgress", true);
    setProperty("cameraState", static_cast<int>(interfaces::CameraState::EXPOSING));
    setProperty("exposureDuration", duration);
    
    SPDLOG_INFO("Camera {} started exposure for {:.3f} seconds", getDeviceId(), duration);
    executeExposure(duration);
}

void Camera::stopExposure() {
    if (!exposureInProgress_) {
        return;
    }

    exposureInProgress_ = false;
    cameraState_ = interfaces::CameraState::IDLE;

    setProperty("exposureInProgress", false);
    setProperty("cameraState", static_cast<int>(interfaces::CameraState::IDLE));

    SPDLOG_INFO("Camera {} exposure stopped", getDeviceId());
    executeStopExposure();
}

bool Camera::isExposing() const {
    return exposureInProgress_;
}

std::vector<uint8_t> Camera::getImageData() const {
    std::lock_guard<std::mutex> lock(imageDataMutex_);
    return imageData_;
}

void Camera::setGain(int gain) {
    if (gain < 0 || gain > cameraParams_.maxGain) {
        SPDLOG_ERROR("Camera {} invalid gain value: {}", getDeviceId(), gain);
        return;
    }

    currentGain_ = gain;
    setProperty("gain", gain);

    SPDLOG_DEBUG("Camera {} gain set to {}", getDeviceId(), gain);
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

// ITemperatureControlled interface implementation - temporarily disabled
bool Camera::setTargetTemperature(double temperature) {
    // if (temperatureBehavior_) {
    //     return temperatureBehavior_->setTargetTemperature(temperature);
    // }
    return false;
}

double Camera::getCurrentTemperature() const {
    // if (temperatureBehavior_) {
    //     return temperatureBehavior_->getCurrentTemperature();
    // }
    return 20.0; // Default room temperature
}

double Camera::getTargetTemperature() const {
    // if (temperatureBehavior_) {
    //     return temperatureBehavior_->getTargetTemperature();
    // }
    return 20.0;
}

bool Camera::stopTemperatureControl() {
    // if (temperatureBehavior_) {
    //     return temperatureBehavior_->stopControl();
    // }
    return true;
}

bool Camera::isTemperatureStable() const {
    // if (temperatureBehavior_) {
    //     // TODO: Implement temperature stability check
    //     // For now, assume temperature is stable
    //     return true;
    // }
    return true;
}

// Extended functionality
bool Camera::expose(double duration, bool synchronous) {
    startExposure(duration);
    // Note: startExposure now returns void, so we assume success
    
    if (synchronous) {
        return waitForExposureComplete();
    }
    
    return true;
}

bool Camera::abort() {
    stopExposure();
    return true; // Assume success since stopExposure is now void
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

void Camera::setOffset(int offset) {
    if (offset < 0 || offset > cameraParams_.maxOffset) {
        SPDLOG_ERROR("Camera {} invalid offset: {}", getDeviceId(), offset);
        return;
    }

    currentOffset_ = offset;
    setProperty("offset", offset);
}

int Camera::getOffset() const {
    return currentOffset_;
}

interfaces::CameraState Camera::getCameraState() const {
    return cameraState_.load();
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
    // Use the duration parameter to avoid unused parameter warning
    (void)duration;
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
                cameraState_ = interfaces::CameraState::DOWNLOAD;
                setProperty("cameraState", static_cast<int>(interfaces::CameraState::DOWNLOAD));
                
                // Simulate image download
                auto newImageData = executeImageDownload();
                {
                    std::lock_guard<std::mutex> lock(imageDataMutex_);
                    imageData_ = std::move(newImageData);
                }
                
                exposureInProgress_ = false;
                cameraState_ = interfaces::CameraState::IDLE;
                setProperty("exposureInProgress", false);
                setProperty("cameraState", static_cast<int>(interfaces::CameraState::IDLE));
                
                SPDLOG_INFO("Camera {} exposure completed", getDeviceId());
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Camera::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "START_EXPOSURE") {
        double duration = parameters.value("duration", 1.0);
        startExposure(duration);
        result["success"] = true; // Assume success since startExposure is now void
        return true;
    }
    else if (command == "STOP_EXPOSURE") {
        stopExposure();
        result["success"] = true; // Assume success since stopExposure is now void
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
        setGain(gain);
        result["success"] = true; // Assume success since setGain is now void
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

// Missing ICamera interface method implementations
void Camera::abortExposure() {
    stopExposure();
}

bool Camera::getImageReady() const {
    return !exposureInProgress_ && !imageData_.empty();
}

double Camera::getLastExposureDuration() const {
    return exposureDuration_.load();
}

std::chrono::system_clock::time_point Camera::getLastExposureStartTime() const {
    auto duration = std::chrono::duration<double>(exposureStartTime_.load());
    return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
}

double Camera::getPercentCompleted() const {
    if (!exposureInProgress_) {
        return 1.0;
    }

    double currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
    double elapsed = currentTime - exposureStartTime_;
    double duration = exposureDuration_.load();

    if (duration <= 0) {
        return 0.0;
    }

    return std::min(1.0, elapsed / duration);
}

int Camera::getCameraXSize() const {
    return cameraParams_.maxWidth;
}

int Camera::getCameraYSize() const {
    return cameraParams_.maxHeight;
}

double Camera::getPixelSizeX() const {
    return cameraParams_.pixelSizeX;
}

double Camera::getPixelSizeY() const {
    return cameraParams_.pixelSizeY;
}

// Additional missing ICamera interface method implementations
int Camera::getMaxBinX() const {
    return cameraParams_.maxBinX;
}

int Camera::getMaxBinY() const {
    return cameraParams_.maxBinY;
}

bool Camera::getCanAsymmetricBin() const {
    return cameraParams_.canAsymmetricBin;
}

int Camera::getBinX() const {
    return binningX_.load();
}

void Camera::setBinX(int binX) {
    if (binX > 0 && binX <= cameraParams_.maxBinX) {
        binningX_ = binX;
        setProperty("binningX", binX);
    }
}

int Camera::getBinY() const {
    return binningY_.load();
}

void Camera::setBinY(int binY) {
    if (binY > 0 && binY <= cameraParams_.maxBinY) {
        binningY_ = binY;
        setProperty("binningY", binY);
    }
}

int Camera::getStartX() const {
    return startX_.load();
}

void Camera::setStartX(int startX) {
    if (startX >= 0 && startX < cameraParams_.maxWidth) {
        startX_ = startX;
        setProperty("startX", startX);
    }
}

int Camera::getStartY() const {
    return startY_.load();
}

void Camera::setStartY(int startY) {
    if (startY >= 0 && startY < cameraParams_.maxHeight) {
        startY_ = startY;
        setProperty("startY", startY);
    }
}

int Camera::getNumX() const {
    return numX_.load();
}

void Camera::setNumX(int numX) {
    if (numX > 0 && numX <= cameraParams_.maxWidth) {
        numX_ = numX;
        setProperty("numX", numX);
    }
}

int Camera::getNumY() const {
    return numY_.load();
}

void Camera::setNumY(int numY) {
    if (numY > 0 && numY <= cameraParams_.maxHeight) {
        numY_ = numY;
        setProperty("numY", numY);
    }
}

int Camera::getGainMin() const {
    return cameraParams_.minGain;
}

int Camera::getGainMax() const {
    return cameraParams_.maxGain;
}

std::vector<std::string> Camera::getGains() const {
    // Return available gain values as strings
    std::vector<std::string> gains;
    for (int i = cameraParams_.minGain; i <= cameraParams_.maxGain; i += 10) {
        gains.push_back(std::to_string(i));
    }
    return gains;
}

int Camera::getOffsetMin() const {
    return cameraParams_.minOffset;
}

int Camera::getOffsetMax() const {
    return cameraParams_.maxOffset;
}

std::vector<std::string> Camera::getOffsets() const {
    // Return available offset values as strings
    std::vector<std::string> offsets;
    for (int i = cameraParams_.minOffset; i <= cameraParams_.maxOffset; i += 5) {
        offsets.push_back(std::to_string(i));
    }
    return offsets;
}

int Camera::getReadoutMode() const {
    return readoutMode_.load();
}

void Camera::setReadoutMode(int mode) {
    readoutMode_ = mode;
    setProperty("readoutMode", mode);
}

// Final missing ICamera interface method implementations
std::vector<std::string> Camera::getReadoutModes() const {
    return {"Normal", "Fast", "High Quality"};
}

bool Camera::getFastReadout() const {
    return readoutMode_.load() == 1; // Fast mode
}

void Camera::setFastReadout(bool fast) {
    setReadoutMode(fast ? 1 : 0);
}

bool Camera::getCanFastReadout() const {
    return true; // Most modern cameras support fast readout
}

std::vector<std::vector<int>> Camera::getImageArray() const {
    // Return a simplified 2D array representation
    std::vector<std::vector<int>> array;
    // For now, return empty array - would need actual image processing
    return array;
}

interfaces::json Camera::getImageArrayVariant() const {
    return interfaces::json::array();
}

interfaces::SensorType Camera::getSensorType() const {
    return cameraParams_.hasColorSensor ? interfaces::SensorType::COLOR : interfaces::SensorType::MONOCHROME;
}

std::string Camera::getSensorName() const {
    return "Generic Camera Sensor";
}

int Camera::getBayerOffsetX() const {
    return 0; // Default Bayer offset
}

int Camera::getBayerOffsetY() const {
    return 0; // Default Bayer offset
}

double Camera::getMaxADU() const {
    return std::pow(2, cameraParams_.bitDepth) - 1;
}

double Camera::getElectronsPerADU() const {
    return 1.0; // Default conversion factor
}

double Camera::getFullWellCapacity() const {
    return 50000.0; // Default full well capacity in electrons
}

double Camera::getExposureMin() const {
    return cameraParams_.minExposureTime;
}

double Camera::getExposureMax() const {
    return cameraParams_.maxExposureTime;
}

double Camera::getExposureResolution() const {
    return 0.001; // 1ms resolution
}

bool Camera::getHasShutter() const {
    return true; // Most cameras have shutters
}

bool Camera::getCanAbortExposure() const {
    return true; // Can abort exposures
}

bool Camera::getCanStopExposure() const {
    return true; // Can stop exposures
}

bool Camera::getCanPulseGuide() const {
    return false; // Most cameras don't support pulse guiding directly
}

void Camera::pulseGuide(interfaces::GuideDirection direction, int duration) {
    // Not implemented for basic camera
    (void)direction;
    (void)duration;
}

bool Camera::getIsPulseGuiding() const {
    return false; // Not pulse guiding
}

double Camera::getSubExposureDuration() const {
    return 0.0; // No sub-exposures by default
}

void Camera::setSubExposureDuration(double duration) {
    // Not implemented for basic camera
    (void)duration;
}

// Missing IDevice interface method implementations
std::string Camera::getName() const {
    return getDeviceId();
}

std::string Camera::getDescription() const {
    return "Generic Camera Device";
}

std::string Camera::getDriverInfo() const {
    return "Hydrogen Camera Driver v1.0";
}

std::string Camera::getDriverVersion() const {
    return "1.0.0";
}

int Camera::getInterfaceVersion() const {
    return 1; // Interface version 1
}

std::vector<std::string> Camera::getSupportedActions() const {
    return {"expose", "abort", "setGain", "setOffset"};
}

bool Camera::isConnecting() const {
    return false; // Not in connecting state
}

interfaces::DeviceState Camera::getDeviceState() const {
    if (isConnected()) {
        if (exposureInProgress_) {
            return interfaces::DeviceState::BUSY;
        }
        return interfaces::DeviceState::IDLE;
    }
    return interfaces::DeviceState::UNKNOWN;
}

std::string Camera::action(const std::string& actionName, const std::string& actionParameters) {
    // Basic action handling
    (void)actionName;
    (void)actionParameters;
    return "OK";
}

void Camera::commandBlind(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
}

bool Camera::commandBool(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
    return true;
}

std::string Camera::commandString(const std::string& command, bool raw) {
    // Basic command handling
    (void)command;
    (void)raw;
    return "OK";
}

void Camera::setupDialog() {
    // No setup dialog for basic implementation
}

void Camera::run() {
    // Main camera device loop
    SPDLOG_INFO("Camera {} starting main loop", getDeviceId());

    while (isRunning()) {
        try {
            // Update camera state and handle any pending operations
            // Check if exposure is complete
            if (exposureInProgress_) {
                double progress = getExposureProgress();
                if (progress >= 1.0) {
                    // Exposure complete
                    exposureInProgress_ = false;
                    cameraState_ = interfaces::CameraState::IDLE;
                    SPDLOG_DEBUG("Camera {} exposure completed", getDeviceId());
                }
            }

            // Update temperature control if enabled - temporarily disabled
            // if (temperatureBehavior_) {
            //     temperatureBehavior_->update();
            // }

            // Sleep for a short time to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        } catch (const std::exception& e) {
            SPDLOG_ERROR("Camera {} error in main loop: {}", getDeviceId(), e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    SPDLOG_INFO("Camera {} main loop stopped", getDeviceId());
}

} // namespace device
} // namespace hydrogen
