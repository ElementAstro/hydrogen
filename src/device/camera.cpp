#include "device/camera.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace astrocomm {

Camera::Camera(const std::string &deviceId, const std::string &manufacturer,
               const std::string &model, const CameraParameters &params)
    : DeviceBase(deviceId, "CAMERA", manufacturer, model),
      cameraState(CameraState::IDLE), cameraParams(params),
      exposureDuration(0.0), exposureProgress(0.0),
      currentImageType(ImageType::LIGHT), autoSave(false), gain(0), offset(0),
      sensorTemperature(20.0), targetTemperature(0.0), coolerEnabled(false),
      coolerPower(0), currentFilterPosition(0), exposureDelay(0.0),
      updateRunning(false), baseImplementation(true) {

  // Initialize random number generator with a high-quality seed
  std::random_device rd;
  rng.seed(rd());

  // Initialize ROI to full frame
  roi.x = 0;
  roi.y = 0;
  roi.width = params.width;
  roi.height = params.height;
  roi.binX = 1;
  roi.binY = 1;

  // Initialize default filter names
  if (params.hasFilterWheel && params.numFilters > 0) {
    for (int i = 0; i < params.numFilters; ++i) {
      filterNames[i] = "Filter " + std::to_string(i + 1);
    }
  }

  // Initialize properties
  setProperty("sensorWidth", params.width);
  setProperty("sensorHeight", params.height);
  setProperty("pixelSizeX", params.pixelSizeX);
  setProperty("pixelSizeY", params.pixelSizeY);
  setProperty("bitDepth", params.bitDepth);
  setProperty("coolerEnabled", coolerEnabled.load());
  setProperty("targetTemperature", targetTemperature.load());
  setProperty("temperature", sensorTemperature.load());
  setProperty("coolerPower", coolerPower.load());
  setProperty("gain", gain.load());
  setProperty("offset", offset.load());
  setProperty("state", cameraStateToString(cameraState));
  setProperty("exposureProgress", 0.0);
  setProperty("exposureDuration", 0.0);
  setProperty("binningX", roi.binX);
  setProperty("binningY", roi.binY);
  setProperty("hasColorSensor", params.hasColorSensor);
  setProperty("hasCooler", params.hasCooler);
  setProperty("hasFilterWheel", params.hasFilterWheel);
  setProperty("connected", false);
  setProperty("imageReady", false);

  if (params.hasFilterWheel) {
    setProperty("filterPosition", currentFilterPosition.load());

    json filtersJson = json::array();
    for (int i = 0; i < params.numFilters; ++i) {
      filtersJson.push_back(filterNames[i]);
    }
    setProperty("filterNames", filtersJson);
  }

  // Set capabilities
  capabilities = {"EXPOSURE", "GAIN_CONTROL", "ROI", "BINNING"};

  if (params.hasCooler) {
    capabilities.push_back("COOLING");
  }

  if (params.hasFilterWheel) {
    capabilities.push_back("FILTER_WHEEL");
  }

  if (params.hasColorSensor) {
    capabilities.push_back("COLOR");
  } else {
    capabilities.push_back("MONOCHROME");
  }

  // Register command handlers
  registerCommandHandler("START_EXPOSURE", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleStartExposureCommand(cmd, response);
  });

  registerCommandHandler("ABORT_EXPOSURE", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
    handleAbortExposureCommand(cmd, response);
  });

  registerCommandHandler("GET_IMAGE", [this](const CommandMessage &cmd,
                                             ResponseMessage &response) {
    handleGetImageCommand(cmd, response);
  });

  registerCommandHandler("SET_COOLER", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSetCoolerCommand(cmd, response);
  });

  registerCommandHandler(
      "SET_ROI", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetROICommand(cmd, response);
      });

  registerCommandHandler("SET_GAIN_OFFSET", [this](const CommandMessage &cmd,
                                                   ResponseMessage &response) {
    handleSetGainOffsetCommand(cmd, response);
  });

  registerCommandHandler("SET_FILTER", [this](const CommandMessage &cmd,
                                              ResponseMessage &response) {
    handleSetFilterCommand(cmd, response);
  });

  SPDLOG_INFO("Camera device initialized: {}", deviceId);
}

Camera::~Camera() {
  // Make sure to stop the device properly
  stop();
}

bool Camera::start() {
  try {
    if (!DeviceBase::start()) {
      return false;
    }

    // Start update thread
    updateRunning = true;
    updateThread = std::thread(&Camera::updateLoop, this);

    setProperty("connected", true);
    SPDLOG_INFO("Camera started: {}", deviceId);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to start camera: {}: {}", e.what(), deviceId);
    return false;
  }
}

void Camera::stop() {
  try {
    // Stop update thread
    updateRunning = false;

    // Notify any waiting condition variables
    exposureCV.notify_all();

    if (updateThread.joinable()) {
      updateThread.join();
    }

    setProperty("connected", false);
    DeviceBase::stop();
    SPDLOG_INFO("Camera stopped: {}", deviceId);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error during camera stop: {}: {}", e.what(), deviceId);
  }
}

bool Camera::startExposure(double duration, ImageType type, bool saveImage) {
  std::lock_guard lock(stateMutex);

  try {
    if (cameraState != CameraState::IDLE) {
      SPDLOG_WARN("Cannot start exposure: camera is busy: {}", deviceId);
      return false;
    }

    if (duration < cameraParams.minExposureTime ||
        duration > cameraParams.maxExposureTime) {
      SPDLOG_WARN("Invalid exposure time: {}", duration);
      return false;
    }

    exposureDuration = duration;
    currentImageType = type;
    autoSave = saveImage;
    exposureProgress = 0.0;

    // Set camera state
    cameraState = CameraState::EXPOSING;
    setProperty("state", cameraStateToString(cameraState));
    setProperty("exposureProgress", 0.0);
    setProperty("exposureDuration", duration);
    setProperty("imageType", imageTypeToString(type));
    setProperty("imageReady", false);

    SPDLOG_INFO("Starting exposure: {}s, type: {}", duration,
                imageTypeToString(type));

    // Trigger condition variable to start exposure
    exposureCV.notify_one();
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error starting exposure: {}: {}", e.what(), deviceId);
    return false;
  }
}

bool Camera::abortExposure() {
  std::lock_guard lock(stateMutex);

  try {
    if (cameraState != CameraState::EXPOSING &&
        cameraState != CameraState::WAITING_TRIGGER) {
      SPDLOG_WARN("No exposure to abort: {}", deviceId);
      return false;
    }

    // Reset state
    cameraState = CameraState::IDLE;
    setProperty("state", cameraStateToString(cameraState));
    setProperty("exposureProgress", 0.0);
    currentExposureMessageId.clear();

    SPDLOG_INFO("Exposure aborted: {}", deviceId);

    // Send abort event
    EventMessage event("EXPOSURE_ABORTED");
    sendEvent(event);

    // Call callback function (if set)
    if (exposureCallback) {
      std::lock_guard cbLock(callbackMutex);
      exposureCallback(false, "Exposure aborted");
    }

    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error aborting exposure: {}: {}", e.what(), deviceId);
    return false;
  }
}

std::vector<uint8_t> Camera::getImageData() const {
  std::lock_guard lock(imageDataMutex);
  return imageData;
}

bool Camera::saveImage(const std::string &filename, const std::string &format) {
  std::lock_guard lock(imageDataMutex);

  if (imageData.empty()) {
    SPDLOG_WARN("No image data to save: {}", deviceId);
    return false;
  }

  try {
    // Generate filename (if not provided)
    std::string actualFilename = filename;
    if (actualFilename.empty()) {
      // Create filename using date and time
      auto now = std::chrono::system_clock::now();
      auto now_time_t = std::chrono::system_clock::to_time_t(now);
      std::stringstream ss;
      ss << "image_"
         << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");

      // Add image type
      ss << "_" << imageTypeToString(currentImageType);

      // Add extension
      if (format == "FITS") {
        ss << ".fits";
      } else if (format == "JPEG" || format == "JPG") {
        ss << ".jpg";
      } else if (format == "PNG") {
        ss << ".png";
      } else if (format == "RAW") {
        ss << ".raw";
      } else {
        ss << ".dat"; // Default binary format
      }

      actualFilename = ss.str();
    }

    // Open file
    std::ofstream outFile(actualFilename, std::ios::binary);
    if (!outFile) {
      SPDLOG_ERROR("Failed to open file for writing: {}: {}", actualFilename,
                   deviceId);
      return false;
    }

    // For a simple demonstration, we just write the raw data
    // Format-specific file header processing can be added here
    outFile.write(reinterpret_cast<const char *>(imageData.data()),
                  imageData.size());

    SPDLOG_INFO("Image saved to {}: {}", actualFilename, deviceId);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error saving image: {}: {}", e.what(), deviceId);
    return false;
  }
}

bool Camera::setGain(int gainValue) {
  if (gainValue < 0 || gainValue > cameraParams.maxGain) {
    SPDLOG_WARN("Invalid gain value: {}", gainValue);
    return false;
  }

  gain = gainValue;
  setProperty("gain", gain.load());
  SPDLOG_INFO("Gain set to {}", gain.load());
  return true;
}

bool Camera::setOffset(int offsetValue) {
  if (offsetValue < 0 || offsetValue > cameraParams.maxOffset) {
    SPDLOG_WARN("Invalid offset value: {}", offsetValue);
    return false;
  }

  offset = offsetValue;
  setProperty("offset", offset.load());
  SPDLOG_INFO("Offset set to {}", offset.load());
  return true;
}

bool Camera::setROI(int x, int y, int width, int height) {
  // Validate parameters
  if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > cameraParams.width || y + height > cameraParams.height) {
    SPDLOG_WARN("Invalid ROI parameters: {}", deviceId);
    return false;
  }

  // Update ROI
  std::lock_guard lock(stateMutex);
  roi.x = x;
  roi.y = y;
  roi.width = width;
  roi.height = height;

  // Update properties
  json roiJson = {{"x", x}, {"y", y}, {"width", width}, {"height", height}};
  setProperty("roi", roiJson);

  SPDLOG_INFO("ROI set to x={}, y={}, width={}, height={}", x, y, width,
              height);
  return true;
}

bool Camera::setBinning(int binX, int binY) {
  // Validate parameters
  if (binX < 1 || binX > cameraParams.maxBinningX || binY < 1 ||
      binY > cameraParams.maxBinningY) {
    SPDLOG_WARN("Invalid binning parameters: {}", deviceId);
    return false;
  }

  // Update binning settings
  std::lock_guard lock(stateMutex);
  roi.binX = binX;
  roi.binY = binY;

  // Update properties
  setProperty("binningX", binX);
  setProperty("binningY", binY);

  SPDLOG_INFO("Binning set to {}x{}", binX, binY);
  return true;
}

bool Camera::setCoolerTemperature(double temperature) {
  if (!cameraParams.hasCooler) {
    SPDLOG_WARN("Camera does not have a cooler: {}", deviceId);
    return false;
  }

  if (temperature < cameraParams.minCoolerTemp) {
    SPDLOG_WARN("Temperature too low: {}", temperature);
    return false;
  }

  targetTemperature = temperature;
  setProperty("targetTemperature", targetTemperature.load());
  SPDLOG_INFO("Target temperature set to {}", targetTemperature.load());
  return true;
}

bool Camera::setCoolerEnabled(bool enabled) {
  if (!cameraParams.hasCooler) {
    SPDLOG_WARN("Camera does not have a cooler: {}", deviceId);
    return false;
  }

  coolerEnabled = enabled;
  setProperty("coolerEnabled", coolerEnabled.load());

  if (!enabled) {
    // If cooler is disabled, set power to 0
    coolerPower = 0;
    setProperty("coolerPower", coolerPower.load());
  }

  SPDLOG_INFO("Cooler {}", enabled ? "enabled" : "disabled");
  return true;
}

double Camera::getCurrentTemperature() const { return sensorTemperature; }

int Camera::getCoolerPower() const { return coolerPower; }

bool Camera::setFilterPosition(int position) {
  if (!cameraParams.hasFilterWheel) {
    SPDLOG_WARN("Camera does not have a filter wheel: {}", deviceId);
    return false;
  }

  if (position < 0 || position >= cameraParams.numFilters) {
    SPDLOG_WARN("Invalid filter position: {}", position);
    return false;
  }

  // In a real camera, this might be a time-consuming operation
  // In this simulation, we change position immediately
  currentFilterPosition = position;
  setProperty("filterPosition", position);

  // Get and set current filter name
  std::string filterName;
  {
    std::lock_guard lock(filterMutex);
    filterName = filterNames[position];
  }
  setProperty("currentFilter", filterName);

  SPDLOG_INFO("Filter set to position {} ({})", position, filterName);
  return true;
}

int Camera::getFilterPosition() const { return currentFilterPosition; }

bool Camera::setFilterName(int position, const std::string &name) {
  if (!cameraParams.hasFilterWheel) {
    SPDLOG_WARN("Camera does not have a filter wheel: {}", deviceId);
    return false;
  }

  if (position < 0 || position >= cameraParams.numFilters) {
    SPDLOG_WARN("Invalid filter position: {}", position);
    return false;
  }

  // Update filter name
  {
    std::lock_guard lock(filterMutex);
    filterNames[position] = name;
  }

  // Update all filter names property
  json filtersJson = json::array();
  {
    std::lock_guard lock(filterMutex);
    for (int i = 0; i < cameraParams.numFilters; ++i) {
      filtersJson.push_back(filterNames[i]);
    }
  }
  setProperty("filterNames", filtersJson);

  // If current position is the updated position, update current filter name
  if (currentFilterPosition == position) {
    setProperty("currentFilter", name);
  }

  SPDLOG_INFO("Filter {} name set to \"{}\"", position, name);
  return true;
}

std::string Camera::getFilterName(int position) const {
  if (!cameraParams.hasFilterWheel || position < 0 ||
      position >= cameraParams.numFilters) {
    return "";
  }

  std::lock_guard lock(filterMutex);
  auto it = filterNames.find(position);
  if (it != filterNames.end()) {
    return it->second;
  }
  return "";
}

CameraState Camera::getState() const { return cameraState; }

double Camera::getExposureProgress() const { return exposureProgress; }

const CameraParameters &Camera::getCameraParameters() const {
  return cameraParams;
}

bool Camera::isExposing() const {
  return (cameraState == CameraState::EXPOSING ||
          cameraState == CameraState::READING_OUT ||
          cameraState == CameraState::DOWNLOADING);
}

bool Camera::setAutoExposure(int targetBrightness, int tolerance) {
  if (targetBrightness < 0 || targetBrightness > 255 || tolerance < 0) {
    SPDLOG_WARN("Invalid auto exposure parameters: {}", deviceId);
    return false;
  }

  autoExposure.enabled = true;
  autoExposure.targetBrightness = targetBrightness;
  autoExposure.tolerance = tolerance;

  setProperty("autoExposure", autoExposure.enabled);
  setProperty("autoExposureTarget", autoExposure.targetBrightness);
  setProperty("autoExposureTolerance", autoExposure.tolerance);

  SPDLOG_INFO("Auto exposure enabled with target brightness {} ± {}",
              targetBrightness, tolerance);
  return true;
}

bool Camera::setExposureDelay(double delaySeconds) {
  if (delaySeconds < 0) {
    SPDLOG_WARN("Invalid exposure delay: {}", delaySeconds);
    return false;
  }

  exposureDelay = delaySeconds;
  setProperty("exposureDelay", delaySeconds);
  SPDLOG_INFO("Exposure delay set to {} seconds", delaySeconds);
  return true;
}

void Camera::setExposureCallback(
    std::function<void(bool, const std::string &)> callback) {
  std::lock_guard lock(callbackMutex);
  exposureCallback = callback;
}

bool Camera::isBaseImplementation() const { return baseImplementation; }

void Camera::updateLoop() {
  SPDLOG_INFO("Update loop started: {}", deviceId);

  // Random distributions for temperature and noise simulation
  std::uniform_real_distribution<> tempDist(-0.1, 0.1);

  try {
    while (updateRunning) {
      // Update temperature simulation
      if (cameraParams.hasCooler) {
        if (coolerEnabled) {
          // Calculate required cooling power
          double tempDiff = sensorTemperature - targetTemperature;
          if (std::abs(tempDiff) > 0.1) {
            // Adjust cooling power based on temperature difference
            if (tempDiff > 0) {
              // Need cooling
              coolerPower = std::min(100, static_cast<int>(tempDiff * 10));
              // Simulate cooling effect
              sensorTemperature.store(sensorTemperature.load() -
                                      (0.1 * coolerPower / 100.0));
            } else {
              // Need warming (stop cooling)
              coolerPower = 0;
              // Natural warming
              sensorTemperature.store(sensorTemperature.load() + 0.05);
            }
          } else {
            // Temperature has reached target, adjust to maintenance power
            coolerPower = 30; // Assume 30% power maintains temperature
            sensorTemperature.store(sensorTemperature.load() +
                                    tempDist(rng) * 0.05); // Small fluctuation
          }
        } else {
          // Cooler off, temperature naturally rises to ambient
          if (sensorTemperature < 20.0) {
            sensorTemperature.store(sensorTemperature.load() + 0.1);
          } else {
            sensorTemperature =
                20.0 + (tempDist(rng) * 0.2); // Room temp fluctuation
          }
          coolerPower = 0;
        }

        // Update temperature and cooling power properties
        setProperty("temperature", sensorTemperature.load());
        setProperty("coolerPower", coolerPower.load());
      }

      // Exposure state machine handling
      std::unique_lock lock(stateMutex);

      switch (cameraState) {
      case CameraState::IDLE:
        // Wait for new exposure command
        exposureCV.wait_for(lock, std::chrono::milliseconds(100), [this] {
          return cameraState != CameraState::IDLE || !updateRunning;
        });
        break;

      case CameraState::EXPOSING: {
        // Handle exposure delay
        if (exposureDelay > 0) {
          cameraState = CameraState::WAITING_TRIGGER;
          setProperty("state", cameraStateToString(cameraState));

          // Wait for delay time
          exposureCV.wait_for(
              lock,
              std::chrono::milliseconds(static_cast<int>(exposureDelay * 1000)),
              [this] {
                return cameraState != CameraState::WAITING_TRIGGER ||
                       !updateRunning;
              });

          if (cameraState == CameraState::WAITING_TRIGGER && updateRunning) {
            // Delay ended, restart exposure
            cameraState = CameraState::EXPOSING;
            setProperty("state", cameraStateToString(cameraState));
            exposureProgress = 0.0;
            break;
          } else {
            // May have been aborted
            break;
          }
        }

        // Update exposure progress
        double increment =
            0.05 / exposureDuration; // Progress increment each 100ms
        double newProgress = exposureProgress + increment;

        if (newProgress >= 1.0) {
          // Exposure complete
          exposureProgress = 1.0;
          cameraState = CameraState::READING_OUT;
          setProperty("state", cameraStateToString(cameraState));
          setProperty("exposureProgress", 1.0);

          SPDLOG_INFO("Exposure completed, reading out: {}", deviceId);
        } else {
          // Exposure in progress
          exposureProgress = newProgress;
          setProperty("exposureProgress", exposureProgress.load());

          // Send periodic progress events
          if (static_cast<int>(exposureProgress * 100) % 10 == 0) {
            sendExposureProgressEvent(exposureProgress);
          }
        }
        break;
      }

      case CameraState::READING_OUT: {
        // Simulate readout time (about 1 second)
        exposureCV.wait_for(lock, std::chrono::milliseconds(1000), [this] {
          return cameraState != CameraState::READING_OUT || !updateRunning;
        });

        // If state is still READING_OUT, readout is complete
        if (cameraState == CameraState::READING_OUT && updateRunning) {
          // Generate image data
          cameraState = CameraState::PROCESSING;
          setProperty("state", cameraStateToString(cameraState));

          SPDLOG_INFO("Reading out completed, processing: {}", deviceId);

          // Process image (do outside lock to avoid blocking)
          lock.unlock();
          generateImageData();
          lock.lock();

          // Processing complete
          cameraState = CameraState::IDLE;
          setProperty("state", cameraStateToString(cameraState));
          setProperty("imageReady", true);

          SPDLOG_INFO("Image ready: {}", deviceId);

          // If configured to auto-save
          if (autoSave) {
            lock.unlock();
            saveImage();
            lock.lock();
          }

          // Send completion event
          if (!currentExposureMessageId.empty()) {
            sendExposureCompleteEvent(currentExposureMessageId);
            currentExposureMessageId.clear();
          }

          // Call callback function (if set)
          if (exposureCallback) {
            std::lock_guard cbLock(callbackMutex);
            exposureCallback(true, "Exposure completed successfully");
          }
        }
        break;
      }

      case CameraState::WAITING_TRIGGER:
      case CameraState::DOWNLOADING:
      case CameraState::PROCESSING:
      case CameraState::ERROR:
      case CameraState::COOLING:
      case CameraState::WARMING_UP:
        // Handle other states...
        exposureCV.wait_for(lock, std::chrono::milliseconds(100));
        break;
      }
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Exception in update loop: {}: {}", e.what(), deviceId);
  }

  SPDLOG_INFO("Update loop ended: {}", deviceId);
}

void Camera::generateImageData() {
  try {
    // Calculate image dimensions
    int effectiveWidth = roi.width / roi.binX;
    int effectiveHeight = roi.height / roi.binY;
    int bytesPerPixel = cameraParams.bitDepth / 8;
    if (bytesPerPixel < 1)
      bytesPerPixel = 1;

    // Number of color channels
    int channels = cameraParams.hasColorSensor ? 3 : 1;

    // Calculate total data size
    size_t dataSize =
        effectiveWidth * effectiveHeight * bytesPerPixel * channels;

    {
      std::lock_guard lock(imageDataMutex);
      imageData.resize(dataSize);

      // Base brightness depends on exposure time and gain
      double baseIntensity =
          std::min(1.0, exposureDuration / 10.0) * (1.0 + gain / 100.0);

      // Adjust based on image type
      if (currentImageType == ImageType::DARK) {
        baseIntensity = 0.05; // Dark frames have minimal signal
      } else if (currentImageType == ImageType::BIAS) {
        baseIntensity = 0.02; // Bias frames only have bias level
      } else if (currentImageType == ImageType::FLAT) {
        baseIntensity =
            0.7; // Flat frames are relatively uniform with medium brightness
      }

      // Generate image data
      std::uniform_real_distribution<> noiseDist(-0.1, 0.1);
      std::normal_distribution<> starDist(0.5, 0.2);

      for (int y = 0; y < effectiveHeight; ++y) {
        for (int x = 0; x < effectiveWidth; ++x) {
          // Base value (with noise)
          double value = baseIntensity * (1.0 + noiseDist(rng) * 0.1);

          // For light and flat frames, add some structure
          if (currentImageType == ImageType::LIGHT) {
            // Add some random stars
            if (starDist(rng) > 0.95) {
              value = std::min(1.0, value * (5.0 + starDist(rng) * 10.0));
            }

            // Add some gradient
            value *= 1.0 + 0.1 * std::sin(x * 0.01) * std::cos(y * 0.01);
          } else if (currentImageType == ImageType::FLAT) {
            // Flat frames have slight vignetting
            value *= 1.0 - 0.1 *
                               ((x - effectiveWidth / 2.0) *
                                    (x - effectiveWidth / 2.0) +
                                (y - effectiveHeight / 2.0) *
                                    (y - effectiveHeight / 2.0)) /
                               (effectiveWidth * effectiveHeight / 4.0);
          }

          // Convert value to pixel value
          int pixelValue =
              static_cast<int>(value * ((1 << cameraParams.bitDepth) - 1));
          pixelValue = std::max(
              0, std::min(pixelValue, (1 << cameraParams.bitDepth) - 1));

          // Fill pixel data
          for (int c = 0; c < channels; ++c) {
            size_t index = (y * effectiveWidth + x) * bytesPerPixel * channels +
                           c * bytesPerPixel;

            // For color sensors, add different intensities for different
            // channels
            double channelFactor = 1.0;
            if (channels > 1) {
              // Simulate different RGB channel responses on color sensors
              if (c == 0)
                channelFactor = 1.0; // Red channel
              else if (c == 1)
                channelFactor = 0.8; // Green channel
              else
                channelFactor = 0.6; // Blue channel
            }

            int channelValue = static_cast<int>(pixelValue * channelFactor);

            // Fill bytes (in big-endian order)
            for (int b = 0; b < bytesPerPixel; ++b) {
              imageData[index + b] =
                  (channelValue >> (8 * (bytesPerPixel - b - 1))) & 0xFF;
            }
          }
        }
      }

      // Apply additional image effects
      applyImageEffects(imageData);
    }

    // Set image parameter properties
    setProperty("imageWidth", effectiveWidth);
    setProperty("imageHeight", effectiveHeight);
    setProperty("imageChannels", channels);
    setProperty("imageDepth", cameraParams.bitDepth);
    setProperty("imageSize", dataSize);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error generating image data: {}: {}", e.what(), deviceId);
    throw; // Re-throw to be caught by updateLoop
  }
}

void Camera::applyImageEffects(std::vector<uint8_t> &imageData) {
  // This method can apply various image effects like hot pixels, bad columns,
  // vignetting etc.

  try {
    // Add some hot pixels (only for longer exposures)
    if (exposureDuration > 1.0) {
      int effectiveWidth = roi.width / roi.binX;
      int effectiveHeight = roi.height / roi.binY;
      int bytesPerPixel = cameraParams.bitDepth / 8;
      if (bytesPerPixel < 1)
        bytesPerPixel = 1;
      int channels = cameraParams.hasColorSensor ? 3 : 1;

      // Number of hot pixels depends on exposure time and sensor temperature
      int hotPixelCount = static_cast<int>(
          5.0 * exposureDuration * std::exp((sensorTemperature + 20) / 30.0));

      std::uniform_int_distribution<> xDist(0, effectiveWidth - 1);
      std::uniform_int_distribution<> yDist(0, effectiveHeight - 1);

      for (int i = 0; i < hotPixelCount; ++i) {
        int x = xDist(rng);
        int y = yDist(rng);

        // Set to high value
        for (int c = 0; c < channels; ++c) {
          size_t index = (y * effectiveWidth + x) * bytesPerPixel * channels +
                         c * bytesPerPixel;

          // Set all bytes to max value (hot pixel)
          for (int b = 0; b < bytesPerPixel; ++b) {
            imageData[index + b] = 0xFF;
          }
        }
      }
    }

    // More effects can be added here...
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error applying image effects: {}: {}", e.what(), deviceId);
  }
}

void Camera::sendExposureProgressEvent(double progress) {
  EventMessage event("EXPOSURE_PROGRESS");

  json details = {{"progress", progress},
                  {"duration", exposureDuration.load()},
                  {"remainingTime", exposureDuration * (1.0 - progress)},
                  {"imageType", imageTypeToString(currentImageType)}};
  event.setDetails(details);

  sendEvent(event);
}

void Camera::sendExposureCompleteEvent(const std::string &relatedMessageId,
                                       bool success,
                                       const std::string &errorMessage) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  json details = {{"command", "START_EXPOSURE"},
                  {"status", success ? "SUCCESS" : "ERROR"},
                  {"duration", exposureDuration.load()},
                  {"imageType", imageTypeToString(currentImageType)},
                  {"width", roi.width / roi.binX},
                  {"height", roi.height / roi.binY},
                  {"imageReady", success}};

  if (!success && !errorMessage.empty()) {
    details["errorMessage"] = errorMessage;
  }

  event.setDetails(details);
  sendEvent(event);
}

ImageType Camera::stringToImageType(const std::string &typeStr) {
  if (typeStr == "DARK")
    return ImageType::DARK;
  if (typeStr == "BIAS")
    return ImageType::BIAS;
  if (typeStr == "FLAT")
    return ImageType::FLAT;
  if (typeStr == "TEST")
    return ImageType::TEST;
  return ImageType::LIGHT; // Default
}

std::string Camera::imageTypeToString(ImageType type) {
  switch (type) {
  case ImageType::DARK:
    return "DARK";
  case ImageType::BIAS:
    return "BIAS";
  case ImageType::FLAT:
    return "FLAT";
  case ImageType::TEST:
    return "TEST";
  case ImageType::LIGHT:
    return "LIGHT";
  default:
    return "UNKNOWN";
  }
}

CameraState Camera::stringToCameraState(const std::string &stateStr) {
  if (stateStr == "EXPOSING")
    return CameraState::EXPOSING;
  if (stateStr == "READING_OUT")
    return CameraState::READING_OUT;
  if (stateStr == "DOWNLOADING")
    return CameraState::DOWNLOADING;
  if (stateStr == "PROCESSING")
    return CameraState::PROCESSING;
  if (stateStr == "ERROR")
    return CameraState::ERROR;
  if (stateStr == "WAITING_TRIGGER")
    return CameraState::WAITING_TRIGGER;
  if (stateStr == "COOLING")
    return CameraState::COOLING;
  if (stateStr == "WARMING_UP")
    return CameraState::WARMING_UP;
  return CameraState::IDLE; // Default
}

std::string Camera::cameraStateToString(CameraState state) {
  switch (state) {
  case CameraState::EXPOSING:
    return "EXPOSING";
  case CameraState::READING_OUT:
    return "READING_OUT";
  case CameraState::DOWNLOADING:
    return "DOWNLOADING";
  case CameraState::PROCESSING:
    return "PROCESSING";
  case CameraState::ERROR:
    return "ERROR";
  case CameraState::WAITING_TRIGGER:
    return "WAITING_TRIGGER";
  case CameraState::COOLING:
    return "COOLING";
  case CameraState::WARMING_UP:
    return "WARMING_UP";
  case CameraState::IDLE:
    return "IDLE";
  default:
    return "UNKNOWN";
  }
}

// Command handler implementations
void Camera::handleStartExposureCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();

  // Check required parameters
  if (!params.contains("duration")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'duration'"}});
    return;
  }

  double duration = params["duration"];

  // Image type (optional)
  ImageType type = ImageType::LIGHT;
  if (params.contains("type") && params["type"].is_string()) {
    type = stringToImageType(params["type"]);
  }

  // Auto-save (optional)
  bool saveImage = false;
  if (params.contains("autoSave") && params["autoSave"].is_boolean()) {
    saveImage = params["autoSave"];
  }

  // Store message ID for completion event
  currentExposureMessageId = cmd.getMessageId();

  // Start exposure
  bool success = startExposure(duration, type, saveImage);

  if (success) {
    // Calculate estimated completion time
    auto now = std::chrono::system_clock::now();
    auto completeTime =
        now + std::chrono::milliseconds(static_cast<int>(
                  (duration + 1.0) * 1000)); // Add 1 second for readout
    auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
    std::ostringstream est_ss;
    est_ss << std::put_time(std::localtime(&complete_itt), "%FT%T") << "Z";

    response.setStatus("IN_PROGRESS");
    response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                         {"progressPercentage", 0.0},
                         {"duration", duration},
                         {"imageType", imageTypeToString(type)}});
  } else {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXPOSURE_FAILED"},
                         {"message", "Failed to start exposure"}});
  }
}

void Camera::handleAbortExposureCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  bool success = abortExposure();

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Exposure aborted"}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "ABORT_FAILED"},
         {"message", "No exposure in progress or abort failed"}});
  }
}

void Camera::handleGetImageCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  std::lock_guard lock(imageDataMutex);

  if (imageData.empty()) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "NO_IMAGE"}, {"message", "No image data available"}});
    return;
  }

  // Here we don't send the actual image data (which might be large)
  // but provide a download URL or data summary information
  response.setStatus("SUCCESS");
  response.setDetails({{"imageReady", true},
                       {"imageSize", imageData.size()},
                       {"imageWidth", roi.width / roi.binX},
                       {"imageHeight", roi.height / roi.binY},
                       {"imageDepth", cameraParams.bitDepth},
                       {"imageChannels", cameraParams.hasColorSensor ? 3 : 1}});

  // In a real implementation, a data transfer session ID or URL could be set
  // here The client could then use this to download the complete image
  SPDLOG_INFO("Image data info sent to client: {}", deviceId);
}

void Camera::handleSetCoolerCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // Set cooler enable state
  if (params.contains("enabled")) {
    if (!params["enabled"].is_boolean()) {
      success = false;
      errorMessage = "Invalid 'enabled' parameter, must be boolean";
    } else if (!setCoolerEnabled(params["enabled"])) {
      success = false;
      errorMessage = "Failed to set cooler state";
    }
  }

  // Set target temperature
  if (success && params.contains("temperature")) {
    if (!params["temperature"].is_number()) {
      success = false;
      errorMessage = "Invalid 'temperature' parameter, must be number";
    } else if (!setCoolerTemperature(params["temperature"])) {
      success = false;
      errorMessage = "Failed to set cooler temperature";
    }
  }

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"coolerEnabled", coolerEnabled.load()},
                         {"targetTemperature", targetTemperature.load()},
                         {"currentTemperature", sensorTemperature.load()},
                         {"coolerPower", coolerPower.load()}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "COOLER_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

void Camera::handleSetROICommand(const CommandMessage &cmd,
                                 ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // Set ROI
  if (params.contains("x") && params.contains("y") &&
      params.contains("width") && params.contains("height")) {

    if (!params["x"].is_number() || !params["y"].is_number() ||
        !params["width"].is_number() || !params["height"].is_number()) {
      success = false;
      errorMessage = "ROI parameters must be numbers";
    } else {
      int x = params["x"];
      int y = params["y"];
      int width = params["width"];
      int height = params["height"];

      if (!setROI(x, y, width, height)) {
        success = false;
        errorMessage = "Failed to set ROI";
      }
    }
  }

  // Set binning
  if (success && params.contains("binX") && params.contains("binY")) {
    if (!params["binX"].is_number() || !params["binY"].is_number()) {
      success = false;
      errorMessage = "Binning parameters must be numbers";
    } else {
      int binX = params["binX"];
      int binY = params["binY"];

      if (!setBinning(binX, binY)) {
        success = false;
        errorMessage = "Failed to set binning";
      }
    }
  }

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"x", roi.x},
                         {"y", roi.y},
                         {"width", roi.width},
                         {"height", roi.height},
                         {"binX", roi.binX},
                         {"binY", roi.binY},
                         {"effectiveWidth", roi.width / roi.binX},
                         {"effectiveHeight", roi.height / roi.binY}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "ROI_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

void Camera::handleSetGainOffsetCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // Set gain
  if (params.contains("gain")) {
    if (!params["gain"].is_number()) {
      success = false;
      errorMessage = "Invalid 'gain' parameter, must be number";
    } else if (!setGain(params["gain"])) {
      success = false;
      errorMessage = "Failed to set gain";
    }
  }

  // Set offset
  if (success && params.contains("offset")) {
    if (!params["offset"].is_number()) {
      success = false;
      errorMessage = "Invalid 'offset' parameter, must be number";
    } else if (!setOffset(params["offset"])) {
      success = false;
      errorMessage = "Failed to set offset";
    }
  }

  if (success) {
    response.setStatus("SUCCESS");
    response.setDetails({{"gain", gain.load()}, {"offset", offset.load()}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "GAIN_OFFSET_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

void Camera::handleSetFilterCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  if (!cameraParams.hasFilterWheel) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "NOT_SUPPORTED"},
                         {"message", "Camera does not have a filter wheel"}});
    return;
  }

  json params = cmd.getParameters();
  bool success = true;
  std::string errorMessage;

  // Set filter position
  if (params.contains("position")) {
    if (!params["position"].is_number()) {
      success = false;
      errorMessage = "Invalid 'position' parameter, must be number";
    } else if (!setFilterPosition(params["position"])) {
      success = false;
      errorMessage = "Failed to set filter position";
    }
  }

  // Set filter name
  if (success && params.contains("name") && params.contains("namePosition")) {
    int pos = params["namePosition"];
    std::string name = params["name"];

    if (!setFilterName(pos, name)) {
      success = false;
      errorMessage = "Failed to set filter name";
    }
  }

  if (success) {
    response.setStatus("SUCCESS");

    // Get filter name list
    json filterNamesJson = json::array();
    {
      std::lock_guard lock(filterMutex);
      for (int i = 0; i < cameraParams.numFilters; ++i) {
        filterNamesJson.push_back(filterNames[i]);
      }
    }

    response.setDetails(
        {{"filterPosition", currentFilterPosition.load()},
         {"currentFilter", getFilterName(currentFilterPosition)},
         {"filterNames", filterNamesJson}});
  } else {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "FILTER_COMMAND_FAILED"}, {"message", errorMessage}});
  }
}

} // namespace astrocomm