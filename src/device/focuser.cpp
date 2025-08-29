#include "device/focuser.h"
#include "common/utils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>

namespace astrocomm {

Focuser::Focuser(const std::string &deviceId, const std::string &manufacturer,
                 const std::string &model)
    : DeviceBase(deviceId, "FOCUSER", manufacturer, model), position(5000),
      targetPosition(5000), maxPosition(10000), speed(5), backlash(0),
      tempCompEnabled(false), tempCompCoefficient(0.0), temperature(20.0),
      stepMode(StepMode::FULL_STEP), isMoving(false), isAutoFocusing(false),
      movingDirection(true), ambientTemperature(20.0), temperatureDrift(0.0),
      cancelAutoFocus(false), updateRunning(false) {

  try {
    // Initialize properties
    setProperty("position", position);
    setProperty("maxPosition", maxPosition);
    setProperty("speed", speed);
    setProperty("backlash", backlash);
    setProperty("temperatureCompensation", tempCompEnabled);
    setProperty("tempCompCoefficient", tempCompCoefficient);
    setProperty("temperature", temperature);
    setProperty("isMoving", false);
    setProperty("stepMode", static_cast<int>(stepMode));
    setProperty("connected", false);
    setProperty("absolutePosition", true);
    setProperty("isAutoFocusing", false);

    // Set capabilities
    capabilities = {"ABSOLUTE_POSITION",
                    "RELATIVE_POSITION",
                    "TEMPERATURE_COMPENSATION",
                    "BACKLASH_COMPENSATION",
                    "STEP_MODE_CONTROL",
                    "AUTO_FOCUS",
                    "FOCUS_PRESETS"};

    // Register command handlers
    registerCommandHandler("MOVE_ABSOLUTE", [this](const CommandMessage &cmd,
                                                   ResponseMessage &response) {
      handleMoveAbsoluteCommand(cmd, response);
    });

    registerCommandHandler("MOVE_RELATIVE", [this](const CommandMessage &cmd,
                                                   ResponseMessage &response) {
      handleMoveRelativeCommand(cmd, response);
    });

    registerCommandHandler(
        "ABORT", [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleAbortCommand(cmd, response);
        });

    registerCommandHandler(
        "SET_MAX_POSITION",
        [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleSetMaxPositionCommand(cmd, response);
        });

    registerCommandHandler("SET_SPEED", [this](const CommandMessage &cmd,
                                               ResponseMessage &response) {
      handleSetSpeedCommand(cmd, response);
    });

    registerCommandHandler("SET_BACKLASH", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
      handleSetBacklashCommand(cmd, response);
    });

    registerCommandHandler(
        "SET_TEMPERATURE_COMPENSATION",
        [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleSetTempCompCommand(cmd, response);
        });

    registerCommandHandler("SET_STEP_MODE", [this](const CommandMessage &cmd,
                                                   ResponseMessage &response) {
      handleSetStepModeCommand(cmd, response);
    });

    registerCommandHandler(
        "SAVE_FOCUS_POINT",
        [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleSaveFocusPointCommand(cmd, response);
        });

    registerCommandHandler(
        "MOVE_TO_SAVED_POINT",
        [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleMoveToSavedPointCommand(cmd, response);
        });

    registerCommandHandler("AUTO_FOCUS", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
      handleAutoFocusCommand(cmd, response);
    });

    SPDLOG_INFO("Focuser device initialized: {}", deviceId);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to initialize focuser device {}: {}", deviceId,
                 e.what());
    throw;
  }
}

Focuser::~Focuser() {
  try {
    stop();

    // Ensure auto focus thread is stopped
    cancelAutoFocus = true;
    if (autoFocusThread.joinable()) {
      autoFocusThread.join();
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error during focuser destruction: {}", e.what());
  }
}

bool Focuser::start() {
  try {
    if (!DeviceBase::start()) {
      return false;
    }

    // Start update thread
    updateRunning = true;
    updateThread = std::thread(&Focuser::updateLoop, this);

    setProperty("connected", true);
    SPDLOG_INFO("Focuser started: {}", deviceId);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Failed to start focuser {}: {}", deviceId, e.what());
    return false;
  }
}

void Focuser::stop() {
  try {
    // Stop auto focus
    cancelAutoFocus = true;
    if (autoFocusThread.joinable()) {
      autoFocusThread.join();
    }

    // Stop update thread
    updateRunning = false;

    // Use condition variable to wake any waiting threads
    moveCompleteCv.notify_all();

    if (updateThread.joinable()) {
      updateThread.join();
    }

    setProperty("connected", false);
    DeviceBase::stop();
    SPDLOG_INFO("Focuser stopped: {}", deviceId);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error stopping focuser {}: {}", deviceId, e.what());
  }
}

bool Focuser::moveAbsolute(int newPosition, bool synchronous) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    // Validate position range
    if (newPosition < 0 || newPosition > maxPosition) {
      SPDLOG_WARN("Invalid position value: {}", newPosition);
      return false;
    }

    // If no change, don't move
    if (newPosition == position && !isMoving) {
      SPDLOG_INFO("Already at requested position: {}", position);
      return true; // Already at target position, considered success
    }

    // Set movement direction and target
    movingDirection = (newPosition > position);

    // Apply backlash compensation
    if (backlash > 0 &&
        ((targetPosition > position && newPosition < position) ||
         (targetPosition < position && newPosition > position))) {
      // Direction changed, add backlash
      if (movingDirection) {
        newPosition += backlash;
      } else {
        newPosition -= backlash;
      }
      // Ensure within valid range
      newPosition = std::max(0, std::min(maxPosition, newPosition));
    }

    targetPosition = newPosition;
    isMoving = true;
    setProperty("isMoving", true);

    SPDLOG_INFO("Starting absolute move to position: {}", targetPosition);

    // If synchronous mode, wait for move to complete
    if (synchronous) {
      statusMutex.unlock(); // Unlock to prevent deadlock
      bool success = waitForMoveComplete();
      statusMutex.lock(); // Restore lock
      return success;
    }

    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in moveAbsolute: {}", e.what());
    return false;
  }
}

bool Focuser::moveRelative(int steps, bool synchronous) {
  try {
    std::unique_lock<std::mutex> lock(statusMutex);

    // Calculate new position
    int newPosition = position + steps;

    // Ensure within valid range
    newPosition = std::max(0, std::min(maxPosition, newPosition));

    SPDLOG_INFO("Starting relative move by steps: {}", steps);

    // Release lock before calling moveAbsolute to prevent deadlock
    lock.unlock();
    return moveAbsolute(newPosition, synchronous);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in moveRelative: {}", e.what());
    return false;
  }
}

bool Focuser::abort() {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (!isMoving) {
      SPDLOG_INFO("No movement to abort");
      return true; // No movement in progress, considered success
    }

    isMoving = false;
    targetPosition = position; // Stop at current position
    currentMoveMessageId.clear();
    setProperty("isMoving", false);

    // Notify all waiting threads
    moveCompleteCv.notify_all();

    SPDLOG_INFO("Movement aborted");

    // Send abort event
    EventMessage event("ABORTED");
    event.setDetails({{"position", position}});
    sendEvent(event);

    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in abort: {}", e.what());
    return false;
  }
}

bool Focuser::setMaxPosition(int maxPos) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (maxPos <= 0) {
      SPDLOG_WARN("Invalid max position: {}", maxPos);
      return false;
    }

    maxPosition = maxPos;
    setProperty("maxPosition", maxPosition);

    SPDLOG_INFO("Max position set to {}", maxPosition);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in setMaxPosition: {}", e.what());
    return false;
  }
}

bool Focuser::setSpeed(int speedValue) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (speedValue < 1 || speedValue > 10) {
      SPDLOG_WARN("Invalid speed value: {}", speedValue);
      return false;
    }

    speed = speedValue;
    setProperty("speed", speed);

    SPDLOG_INFO("Speed set to {}", speed);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in setSpeed: {}", e.what());
    return false;
  }
}

bool Focuser::setBacklash(int backlashValue) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (backlashValue < 0 || backlashValue > 1000) {
      SPDLOG_WARN("Invalid backlash value: {}", backlashValue);
      return false;
    }

    backlash = backlashValue;
    setProperty("backlash", backlash);

    SPDLOG_INFO("Backlash set to {}", backlash);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in setBacklash: {}", e.what());
    return false;
  }
}

bool Focuser::setStepMode(StepMode mode) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    // Validate step mode
    int modeValue = static_cast<int>(mode);
    bool validMode = (modeValue == 1 || modeValue == 2 || modeValue == 4 ||
                      modeValue == 8 || modeValue == 16 || modeValue == 32);

    if (!validMode) {
      SPDLOG_WARN("Invalid step mode: {}", modeValue);
      return false;
    }

    stepMode = mode;
    setProperty("stepMode", static_cast<int>(stepMode));

    SPDLOG_INFO("Step mode set to 1/{}", modeValue);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in setStepMode: {}", e.what());
    return false;
  }
}

bool Focuser::setTemperatureCompensation(bool enabled, double coefficient) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    tempCompEnabled = enabled;

    if (coefficient != 0.0) {
      tempCompCoefficient = coefficient;
    }

    setProperty("temperatureCompensation", tempCompEnabled);
    setProperty("tempCompCoefficient", tempCompCoefficient);

    SPDLOG_INFO("Temperature compensation {}, coefficient: {}",
                enabled ? "enabled" : "disabled", tempCompCoefficient);

    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in setTemperatureCompensation: {}", e.what());
    return false;
  }
}

bool Focuser::saveFocusPoint(const std::string &name,
                             const std::string &description) {
  try {
    if (name.empty()) {
      SPDLOG_WARN("Cannot save focus point with empty name");
      return false;
    }

    std::lock_guard<std::mutex> lockStatus(statusMutex);
    std::lock_guard<std::mutex> lockPoints(focusPointsMutex);

    // Save current position
    savedFocusPoints[name] = std::make_pair(position, description);

    SPDLOG_INFO("Saved focus point '{}' at position {}", name, position);

    // Send notification event
    EventMessage event("FOCUS_POINT_SAVED");
    event.setDetails({{"name", name},
                      {"position", position},
                      {"description", description},
                      {"timestamp", getIsoTimestamp()}});
    sendEvent(event);

    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in saveFocusPoint: {}", e.what());
    return false;
  }
}

bool Focuser::moveToSavedPoint(const std::string &name, bool synchronous) {
  try {
    std::unique_lock<std::mutex> lock(focusPointsMutex);

    auto it = savedFocusPoints.find(name);
    if (it == savedFocusPoints.end()) {
      SPDLOG_WARN("Focus point '{}' not found", name);
      return false;
    }

    int pointPosition = it->second.first;

    SPDLOG_INFO("Moving to saved focus point '{}' at position {}", name,
                pointPosition);

    // Release lock before calling moveAbsolute to prevent deadlock
    lock.unlock();
    return moveAbsolute(pointPosition, synchronous);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in moveToSavedPoint: {}", e.what());
    return false;
  }
}

json Focuser::getSavedFocusPoints() const {
  try {
    std::lock_guard<std::mutex> lock(focusPointsMutex);

    json points = json::object();

    for (const auto &point : savedFocusPoints) {
      points[point.first] = {{"position", point.second.first},
                             {"description", point.second.second}};
    }

    return points;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in getSavedFocusPoints: {}", e.what());
    return json::object();
  }
}

bool Focuser::startAutoFocus(int startPos, int endPos, int steps,
                             bool useExistingCurve) {
  try {
    // Check parameter validity
    if (startPos < 0 || startPos > maxPosition || endPos < 0 ||
        endPos > maxPosition || steps <= 0 || startPos >= endPos) {
      SPDLOG_WARN("Invalid auto focus parameters");
      return false;
    }

    // Check if auto focus is already in progress
    if (isAutoFocusing) {
      SPDLOG_WARN("Auto focus already in progress");
      return false;
    }

    // If no focus metric callback set, can't perform auto focus
    if (!focusMetricCallback && !useExistingCurve) {
      SPDLOG_WARN("No focus metric callback set");
      return false;
    }

    // Clear existing curve data (if not using existing curve)
    if (!useExistingCurve) {
      std::lock_guard<std::mutex> lock(focusCurveMutex);
      focusCurve.clear();
    }

    // Set auto focus flags
    isAutoFocusing = true;
    cancelAutoFocus = false;
    setProperty("isAutoFocusing", true);

    // Start auto focus thread
    autoFocusThread =
        std::thread([this, startPos, endPos, steps, useExistingCurve]() {
          try {
            SPDLOG_INFO("Starting auto focus from {} to {} with {} steps",
                        startPos, endPos, steps);

            // Send auto focus start event
            EventMessage startEvent("AUTO_FOCUS_STARTED");
            startEvent.setDetails({{"startPosition", startPos},
                                   {"endPosition", endPos},
                                   {"steps", steps},
                                   {"useExistingCurve", useExistingCurve}});
            sendEvent(startEvent);

            // Perform auto focus
            performAutoFocus();

            // End processing
            isAutoFocusing = false;
            setProperty("isAutoFocusing", false);

            // Send auto focus complete event
            EventMessage completeEvent("AUTO_FOCUS_COMPLETED");

            {
              std::lock_guard<std::mutex> lock(statusMutex);
              json details = json::object();
              details["finalPosition"] = position;
              details["cancelled"] = cancelAutoFocus.load();
              completeEvent.setDetails(details);
            }

            sendEvent(completeEvent);

            SPDLOG_INFO("Auto focus completed");
          } catch (const std::exception &e) {
            isAutoFocusing = false;
            setProperty("isAutoFocusing", false);
            SPDLOG_ERROR("Error in auto focus thread: {}", e.what());

            // Send error event
            EventMessage errorEvent("AUTO_FOCUS_ERROR");
            errorEvent.setDetails({{"error", e.what()}});
            sendEvent(errorEvent);
          }
        });

    // Detach thread to let it run in background
    autoFocusThread.detach();
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in startAutoFocus: {}", e.what());
    return false;
  }
}

json Focuser::getFocusCurveData() const {
  try {
    std::lock_guard<std::mutex> lock(focusCurveMutex);

    json data = json::array();

    for (const auto &point : focusCurve) {
      data.push_back({{"position", point.position},
                      {"metric", point.metric},
                      {"temperature", point.temperature},
                      {"timestamp", point.timestamp}});
    }

    return data;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in getFocusCurveData: {}", e.what());
    return json::array();
  }
}

bool Focuser::saveConfiguration(const std::string &filePath) const {
  try {
    std::lock_guard<std::mutex> lockStatus(statusMutex);
    std::lock_guard<std::mutex> lockPoints(focusPointsMutex);

    json config;

    // Device basic information
    config["deviceId"] = deviceId;
    config["deviceType"] = "FOCUSER";
    config["manufacturer"] = manufacturer;
    config["model"] = model;

    // Parameter settings
    config["settings"] = {{"maxPosition", maxPosition},
                          {"speed", speed},
                          {"backlash", backlash},
                          {"temperatureCompensation", tempCompEnabled},
                          {"tempCompCoefficient", tempCompCoefficient},
                          {"stepMode", static_cast<int>(stepMode)}};

    // Saved focus points
    json focusPoints = json::object();
    for (const auto &point : savedFocusPoints) {
      focusPoints[point.first] = {{"position", point.second.first},
                                  {"description", point.second.second}};
    }
    config["focusPoints"] = focusPoints;

    // Write to file
    std::ofstream file(filePath);
    if (!file.is_open()) {
      SPDLOG_ERROR("Failed to open file for writing: {}", filePath);
      return false;
    }

    file << config.dump(2);
    file.close();

    SPDLOG_INFO("Configuration saved to {}", filePath);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error saving configuration: {}", e.what());
    return false;
  }
}

bool Focuser::loadConfiguration(const std::string &filePath) {
  try {
    // Read file
    std::ifstream file(filePath);
    if (!file.is_open()) {
      SPDLOG_ERROR("Failed to open configuration file: {}", filePath);
      return false;
    }

    json config;
    file >> config;
    file.close();

    // Validate device type
    if (!config.contains("deviceType") || config["deviceType"] != "FOCUSER") {
      SPDLOG_ERROR("Invalid configuration file: not a focuser configuration");
      return false;
    }

    std::lock_guard<std::mutex> lockStatus(statusMutex);
    std::lock_guard<std::mutex> lockPoints(focusPointsMutex);

    // Load settings
    if (config.contains("settings")) {
      const auto &settings = config["settings"];

      if (settings.contains("maxPosition")) {
        maxPosition = settings["maxPosition"];
        setProperty("maxPosition", maxPosition);
      }

      if (settings.contains("speed")) {
        speed = settings["speed"];
        setProperty("speed", speed);
      }

      if (settings.contains("backlash")) {
        backlash = settings["backlash"];
        setProperty("backlash", backlash);
      }

      if (settings.contains("temperatureCompensation")) {
        tempCompEnabled = settings["temperatureCompensation"];
        setProperty("temperatureCompensation", tempCompEnabled);
      }

      if (settings.contains("tempCompCoefficient")) {
        tempCompCoefficient = settings["tempCompCoefficient"];
        setProperty("tempCompCoefficient", tempCompCoefficient);
      }

      if (settings.contains("stepMode")) {
        stepMode = static_cast<StepMode>(settings["stepMode"].get<int>());
        setProperty("stepMode", static_cast<int>(stepMode));
      }
    }

    // Load saved focus points
    if (config.contains("focusPoints") && config["focusPoints"].is_object()) {
      savedFocusPoints.clear();

      for (auto it = config["focusPoints"].begin();
           it != config["focusPoints"].end(); ++it) {
        const std::string &name = it.key();
        int pos = it.value()["position"];
        std::string desc = it.value().contains("description")
                               ? it.value()["description"].get<std::string>()
                               : "";

        savedFocusPoints[name] = std::make_pair(pos, desc);
      }
    }

    SPDLOG_INFO("Configuration loaded from {}", filePath);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error loading configuration: {}", e.what());
    return false;
  }
}

void Focuser::setFocusMetricCallback(FocusMetricCallback callback) {
  focusMetricCallback = callback;
}

void Focuser::updateLoop() {
  SPDLOG_INFO("Update loop started");

  try {
    // Random number generator for simulating temperature changes
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> tempDist(-0.2, 0.2);

    auto lastTime = std::chrono::steady_clock::now();

    while (updateRunning) {
      // Update approximately every 50ms
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      auto currentTime = std::chrono::steady_clock::now();
      double elapsedSec =
          std::chrono::duration<double>(currentTime - lastTime).count();
      lastTime = currentTime;

      // Local scope lock to reduce lock holding time
      {
        std::lock_guard<std::mutex> lock(statusMutex);

        // Simulate temperature changes
        temperatureDrift += tempDist(gen) * elapsedSec;
        temperatureDrift = std::max(
            -3.0, std::min(3.0, temperatureDrift)); // Limit drift range

        temperature = ambientTemperature + temperatureDrift;

        // Only update temperature property when change exceeds 0.1 degree
        static double lastReportedTemp = temperature;
        if (std::abs(temperature - lastReportedTemp) >= 0.1) {
          setProperty("temperature", temperature);
          lastReportedTemp = temperature;
        }

        // If moving
        if (isMoving) {
          // Calculate step size, considering step mode
          double stepMultiplier =
              1.0 / static_cast<double>(static_cast<int>(stepMode));
          int step = std::max(
              1, static_cast<int>(speed * 20 * elapsedSec * stepMultiplier));

          // Move toward target position
          if (std::abs(targetPosition - position) <= step) {
            // Reached target position
            position = targetPosition;
            isMoving = false;
            setProperty("isMoving", false);

            // Notify waiting threads
            moveCompleteCv.notify_all();

            // Send move complete event
            if (!currentMoveMessageId.empty()) {
              sendMoveCompletedEvent(currentMoveMessageId);
              currentMoveMessageId.clear();
            }

            SPDLOG_INFO("Move completed at position: {}", position);
          } else {
            // Continue moving
            if (position < targetPosition) {
              position += step;
            } else {
              position -= step;
            }
          }

          // Update position property
          setProperty("position", position);
        } else if (tempCompEnabled) {
          // Apply temperature compensation
          int compensatedPosition = applyTemperatureCompensation(position);

          if (compensatedPosition != position) {
            // Temperature compensation requires movement
            int originalPosition = position;
            position = compensatedPosition;
            setProperty("position", position);

            SPDLOG_INFO(
                "Temperature compensation adjusted position from {} to {}",
                originalPosition, position);
          }
        }
      }
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in updateLoop: {}", e.what());
    updateRunning = false;
  }

  SPDLOG_INFO("Update loop ended");
}

bool Focuser::waitForMoveComplete(int timeoutMs) {
  try {
    std::unique_lock<std::mutex> lock(statusMutex);

    if (!isMoving) {
      return true; // Already stopped moving
    }

    auto pred = [this]() { return !isMoving; };

    if (timeoutMs <= 0) {
      // Wait indefinitely
      moveCompleteCv.wait(lock, pred);
      return true;
    } else {
      // Wait with timeout
      return moveCompleteCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                     pred);
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in waitForMoveComplete: {}", e.what());
    return false;
  }
}

int Focuser::applyTemperatureCompensation(int currentPosition) {
  try {
    // Calculate difference between last temperature and current temperature
    static double lastTemp = temperature;
    double tempDiff = temperature - lastTemp;
    lastTemp = temperature;

    // If temperature difference is too small, don't compensate
    if (std::abs(tempDiff) < 0.1) {
      return currentPosition;
    }

    // Calculate steps to move: temperature difference × compensation
    // coefficient
    int steps = static_cast<int>(tempDiff * tempCompCoefficient);

    // Return compensated position, ensuring within valid range
    return std::max(0, std::min(maxPosition, currentPosition + steps));
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in applyTemperatureCompensation: {}", e.what());
    return currentPosition; // Return original position on error
  }
}

double Focuser::calculateFocusMetric(int position) {
  try {
    // Default implementation uses callback function (if set)
    if (focusMetricCallback) {
      return focusMetricCallback(position);
    }

    // Otherwise return a simulated focus quality
    // Generate a Gaussian curve near the ideal position
    const int idealFocusPos = maxPosition / 2;
    const double sigma = maxPosition / 10.0;

    double distanceFromIdeal = position - idealFocusPos;
    double metric = 100.0 * std::exp(-(distanceFromIdeal * distanceFromIdeal) /
                                     (2 * sigma * sigma));

    // Add some random noise
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> noiseDist(-5.0, 5.0);

    metric += noiseDist(gen);
    metric = std::max(0.0, std::min(100.0, metric)); // Limit range

    return metric;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in calculateFocusMetric: {}", e.what());
    return 0.0; // Return minimum quality on error
  }
}

void Focuser::performAutoFocus() {
  try {
    std::lock_guard<std::mutex> lockFocusCurve(focusCurveMutex);

    // Check if using existing curve
    if (focusCurve.empty()) {
      // Get parameters
      int startPos, endPos, steps;

      {
        std::lock_guard<std::mutex> lockStatus(statusMutex);
        startPos = targetPosition;
        endPos = maxPosition;
        steps = 10;
      }

      // Calculate step size
      int stepSize = (endPos - startPos) / (steps - 1);
      if (stepSize < 1)
        stepSize = 1;

      // Move to start position
      moveAbsolute(startPos, true);

      if (cancelAutoFocus)
        return;

      // Measure focus quality at each position
      for (int i = 0; i < steps; i++) {
        if (cancelAutoFocus)
          return;

        int currentPos = startPos + i * stepSize;
        if (currentPos > endPos)
          currentPos = endPos;

        // Move to current position
        moveAbsolute(currentPos, true);

        if (cancelAutoFocus)
          return;

        // Wait for stability
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (cancelAutoFocus)
          return;

        // Calculate focus quality
        double metric = calculateFocusMetric(currentPos);

        // Record data point
        FocusPoint point;
        point.position = currentPos;
        point.metric = metric;
        point.temperature = temperature;
        point.timestamp = getIsoTimestamp();
        focusCurve.push_back(point);

        // Send progress event
        EventMessage progressEvent("AUTO_FOCUS_PROGRESS");
        progressEvent.setDetails(
            {{"step", i + 1},
             {"totalSteps", steps},
             {"position", currentPos},
             {"metric", metric},
             {"progress", static_cast<double>(i + 1) / steps * 100.0}});
        sendEvent(progressEvent);

        SPDLOG_INFO("Auto focus step {}/{} - Position: {}, Metric: {}", i + 1,
                    steps, currentPos, metric);
      }
    }

    // Find best position
    int bestPosition = 0;
    double bestMetric = -1.0;

    for (const auto &point : focusCurve) {
      if (point.metric > bestMetric) {
        bestMetric = point.metric;
        bestPosition = point.position;
      }
    }

    SPDLOG_INFO("Best focus position found: {} with metric {}", bestPosition,
                bestMetric);

    // Move to best position
    moveAbsolute(bestPosition, true);

    // Send best position event
    EventMessage bestPosEvent("AUTO_FOCUS_BEST_POSITION");
    bestPosEvent.setDetails(
        {{"position", bestPosition}, {"metric", bestMetric}});
    sendEvent(bestPosEvent);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error during auto focus: {}", e.what());

    // Send error event
    EventMessage errorEvent("AUTO_FOCUS_ERROR");
    errorEvent.setDetails({{"error", e.what()}});
    sendEvent(errorEvent);

    // Re-throw to be caught by the thread handler
    throw;
  }
}

void Focuser::sendMoveCompletedEvent(const std::string &relatedMessageId) {
  try {
    EventMessage event("COMMAND_COMPLETED");
    event.setRelatedMessageId(relatedMessageId);

    event.setDetails({{"command", "MOVE"},
                      {"status", "SUCCESS"},
                      {"finalPosition", position}});

    sendEvent(event);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error sending move completed event: {}", e.what());
  }
}

// Command handler implementations
void Focuser::handleMoveAbsoluteCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("position")) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_PARAMETERS"},
           {"message", "Missing required parameter 'position'"}});
      return;
    }

    int newPosition = params["position"];
    bool synchronous = params.contains("synchronous")
                           ? params["synchronous"].get<bool>()
                           : false;

    // Store message ID for completion event
    currentMoveMessageId = cmd.getMessageId();

    // Start movement
    bool success = moveAbsolute(newPosition, synchronous);

    if (!success) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "MOVE_FAILED"}, {"message", "Failed to start movement"}});
      return;
    }

    if (synchronous) {
      // Synchronous mode completed
      response.setStatus("SUCCESS");
      response.setDetails({{"finalPosition", position}});
    } else {
      // Asynchronous mode - calculate estimated completion time
      int moveDistance = std::abs(targetPosition - position);
      int estimatedStepsPerSecond = speed * 20 / static_cast<int>(stepMode);
      if (estimatedStepsPerSecond < 1)
        estimatedStepsPerSecond = 1;

      int estimatedSeconds = moveDistance / estimatedStepsPerSecond + 1;

      auto now = std::chrono::system_clock::now();
      auto completeTime = now + std::chrono::seconds(estimatedSeconds);
      auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
      std::ostringstream est_ss;
      est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

      double progressPercentage = 0.0;
      if (moveDistance > 0) {
        progressPercentage = 0.0; // Just started moving
      } else {
        progressPercentage = 100.0; // Already at target position
      }

      response.setStatus("IN_PROGRESS");
      response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                           {"progressPercentage", progressPercentage},
                           {"targetPosition", targetPosition},
                           {"currentPosition", position}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleMoveAbsoluteCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleMoveRelativeCommand(const CommandMessage &cmd,
                                        ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("steps")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing required parameter 'steps'"}});
      return;
    }

    int steps = params["steps"];
    bool synchronous = params.contains("synchronous")
                           ? params["synchronous"].get<bool>()
                           : false;

    // Store message ID for completion event
    currentMoveMessageId = cmd.getMessageId();

    // Start relative movement
    bool success = moveRelative(steps, synchronous);

    if (!success) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "MOVE_FAILED"}, {"message", "Failed to start movement"}});
      return;
    }

    if (synchronous) {
      // Synchronous mode completed
      response.setStatus("SUCCESS");
      response.setDetails({{"finalPosition", position}});
    } else {
      // Asynchronous mode - calculate estimated completion time
      int moveDistance = std::abs(targetPosition - position);
      int estimatedStepsPerSecond = speed * 20 / static_cast<int>(stepMode);
      if (estimatedStepsPerSecond < 1)
        estimatedStepsPerSecond = 1;

      int estimatedSeconds = moveDistance / estimatedStepsPerSecond + 1;

      auto now = std::chrono::system_clock::now();
      auto completeTime = now + std::chrono::seconds(estimatedSeconds);
      auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
      std::ostringstream est_ss;
      est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

      response.setStatus("IN_PROGRESS");
      response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                           {"progressPercentage", 0.0},
                           {"steps", steps},
                           {"targetPosition", targetPosition},
                           {"currentPosition", position}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleMoveRelativeCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleAbortCommand(const CommandMessage &cmd,
                                 ResponseMessage &response) {
  try {
    bool success = abort();

    response.setStatus(success ? "SUCCESS" : "ERROR");
    response.setDetails(
        {{"message", success ? "Movement aborted" : "Failed to abort movement"},
         {"position", position}});
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleAbortCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleSetMaxPositionCommand(const CommandMessage &cmd,
                                          ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("maxPosition")) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_PARAMETERS"},
           {"message", "Missing required parameter 'maxPosition'"}});
      return;
    }

    int maxPos = params["maxPosition"];
    bool success = setMaxPosition(maxPos);

    response.setStatus(success ? "SUCCESS" : "ERROR");
    if (success) {
      response.setDetails({{"maxPosition", maxPosition}});
    } else {
      response.setDetails({{"error", "INVALID_VALUE"},
                           {"message", "Invalid maximum position value"}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleSetMaxPositionCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleSetSpeedCommand(const CommandMessage &cmd,
                                    ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("speed")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing required parameter 'speed'"}});
      return;
    }

    int speedValue = params["speed"];
    bool success = setSpeed(speedValue);

    response.setStatus(success ? "SUCCESS" : "ERROR");
    if (success) {
      response.setDetails({{"speed", speed}});
    } else {
      response.setDetails({{"error", "INVALID_VALUE"},
                           {"message", "Speed must be between 1 and 10"}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleSetSpeedCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleSetBacklashCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("backlash")) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_PARAMETERS"},
           {"message", "Missing required parameter 'backlash'"}});
      return;
    }

    int backlashValue = params["backlash"];
    bool success = setBacklash(backlashValue);

    response.setStatus(success ? "SUCCESS" : "ERROR");
    if (success) {
      response.setDetails({{"backlash", backlash}});
    } else {
      response.setDetails({{"error", "INVALID_VALUE"},
                           {"message", "Backlash must be between 0 and 1000"}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleSetBacklashCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleSetTempCompCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("enabled")) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_PARAMETERS"},
           {"message", "Missing required parameter 'enabled'"}});
      return;
    }

    bool enabled = params["enabled"];
    double coefficient = tempCompCoefficient;

    if (params.contains("coefficient")) {
      coefficient = params["coefficient"];
    }

    bool success = setTemperatureCompensation(enabled, coefficient);

    response.setStatus(success ? "SUCCESS" : "ERROR");
    if (success) {
      response.setDetails({{"temperatureCompensation", tempCompEnabled},
                           {"coefficient", tempCompCoefficient}});
    } else {
      response.setDetails(
          {{"error", "OPERATION_FAILED"},
           {"message", "Failed to set temperature compensation"}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleSetTempCompCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleSetStepModeCommand(const CommandMessage &cmd,
                                       ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("mode")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing required parameter 'mode'"}});
      return;
    }

    int modeValue = params["mode"];
    bool success = setStepMode(static_cast<StepMode>(modeValue));

    response.setStatus(success ? "SUCCESS" : "ERROR");
    if (success) {
      response.setDetails({{"stepMode", static_cast<int>(stepMode)}});
    } else {
      response.setDetails(
          {{"error", "INVALID_VALUE"},
           {"message", "Invalid step mode. Valid values: 1, 2, 4, 8, 16, 32"}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleSetStepModeCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleSaveFocusPointCommand(const CommandMessage &cmd,
                                          ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("name")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing required parameter 'name'"}});
      return;
    }

    std::string name = params["name"];
    std::string description = params.contains("description")
                                  ? params["description"].get<std::string>()
                                  : "";

    bool success = saveFocusPoint(name, description);

    response.setStatus(success ? "SUCCESS" : "ERROR");
    if (success) {
      response.setDetails({{"name", name},
                           {"position", position},
                           {"description", description}});
    } else {
      response.setDetails({{"error", "OPERATION_FAILED"},
                           {"message", "Failed to save focus point"}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleSaveFocusPointCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleMoveToSavedPointCommand(const CommandMessage &cmd,
                                            ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("name")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing required parameter 'name'"}});
      return;
    }

    std::string name = params["name"];
    bool synchronous = params.contains("synchronous")
                           ? params["synchronous"].get<bool>()
                           : false;

    // Store message ID for completion event
    currentMoveMessageId = cmd.getMessageId();

    bool success = moveToSavedPoint(name, synchronous);

    if (!success) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "POINT_NOT_FOUND"},
                           {"message", "Focus point not found: " + name}});
      return;
    }

    if (synchronous) {
      // Synchronous mode completed
      response.setStatus("SUCCESS");
      response.setDetails({{"name", name}, {"finalPosition", position}});
    } else {
      // Asynchronous mode
      response.setStatus("IN_PROGRESS");
      response.setDetails(
          {{"name", name}, {"message", "Moving to saved point: " + name}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleMoveToSavedPointCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

void Focuser::handleAutoFocusCommand(const CommandMessage &cmd,
                                     ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    int startPos = params.contains("startPosition")
                       ? params["startPosition"].get<int>()
                       : position;

    int endPos = params.contains("endPosition")
                     ? params["endPosition"].get<int>()
                     : maxPosition;

    int steps = params.contains("steps") ? params["steps"].get<int>() : 10;

    bool useExistingCurve = params.contains("useExistingCurve")
                                ? params["useExistingCurve"].get<bool>()
                                : false;

    bool success = startAutoFocus(startPos, endPos, steps, useExistingCurve);

    if (success) {
      response.setStatus("IN_PROGRESS");
      response.setDetails({{"startPosition", startPos},
                           {"endPosition", endPos},
                           {"steps", steps},
                           {"useExistingCurve", useExistingCurve}});
    } else {
      response.setStatus("ERROR");
      response.setDetails({{"error", "AUTO_FOCUS_FAILED"},
                           {"message", "Failed to start auto focus"}});
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error in handleAutoFocusCommand: {}", e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", e.what()}});
  }
}

} // namespace astrocomm