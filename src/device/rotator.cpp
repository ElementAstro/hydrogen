#include "device/rotator.h"
#include "common/logger.h"
#include <cmath>
#include <iomanip>
#include <stdexcept>
#include <sstream>

namespace astrocomm {

Rotator::Rotator(const std::string &deviceId, const std::string &manufacturer,
                 const std::string &model)
    : DeviceBase(deviceId, "ROTATOR", manufacturer, model), position(0.0),
      targetPosition(0.0), stepSize(0.1), maxSpeed(10.0), currentSpeed(5.0),
      isMovingFlag(false), isReversedFlag(false), update_running(false), 
      updateInterval(50) { // 50ms = 20Hz update rate

  // Initialize properties
  setProperty("position", position.load());
  setProperty("target_position", targetPosition.load());
  setProperty("step_size", stepSize);
  setProperty("max_speed", maxSpeed);
  setProperty("current_speed", currentSpeed.load());
  setProperty("is_moving", false);
  setProperty("is_reversed", false);
  setProperty("connected", false);

  // Set device capabilities
  capabilities = {"ABSOLUTE_POSITION", "RELATIVE_POSITION", "REVERSE"};

  // Register command handlers
  registerCommandHandler(
      "MOVE_TO", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleMoveToCommand(cmd, response);
      });

  registerCommandHandler(
      "MOVE_BY", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleMoveByCommand(cmd, response);
      });

  registerCommandHandler(
      "HALT", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleHaltCommand(cmd, response);
      });

  registerCommandHandler("SET_REVERSE", [this](const CommandMessage &cmd,
                                               ResponseMessage &response) {
    handleReverseCommand(cmd, response);
  });

  registerCommandHandler(
      "SYNC", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSyncCommand(cmd, response);
      });
      
  registerCommandHandler(
      "SET_SPEED", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSetSpeedCommand(cmd, response);
      });

  logInfo("Rotator device initialized", deviceId);
}

Rotator::~Rotator() { stop(); }

bool Rotator::start() {
  try {
    if (!DeviceBase::start()) {
      return false;
    }

    // Start update thread
    update_running = true;
    update_thread = std::thread(&Rotator::updateLoop, this);

    setProperty("connected", true);
    logInfo("Rotator device started", deviceId);
    return true;
  } catch (const std::exception &e) {
    logError("Failed to start rotator: " + std::string(e.what()), deviceId);
    return false;
  }
}

void Rotator::stop() {
  try {
    // Stop update thread
    update_running = false;
    if (update_thread.joinable()) {
      update_thread.join();
    }

    setProperty("connected", false);
    DeviceBase::stop();
    logInfo("Rotator device stopped", deviceId);
  } catch (const std::exception &e) {
    logError("Error stopping rotator: " + std::string(e.what()), deviceId);
  }
}

void Rotator::validateConnected() const {
  auto connectedProp = getProperty("connected");
  if (!connectedProp.is_boolean() || !connectedProp.get<bool>()) {
    throw std::runtime_error("Rotator is not connected");
  }
}

void Rotator::moveTo(double targetPos) {
  try {
    validateConnected();
    
    // Normalize target position
    double normalizedTarget = normalizeAngle(targetPos);
    
    // Call hook to allow derived classes to intercept
    if (!onBeforeMove(normalizedTarget)) {
      logWarning("Movement to " + std::to_string(normalizedTarget) + 
                " was declined by onBeforeMove", deviceId);
      return;
    }
    
    // Lock for thread safety
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      
      // Update target position
      targetPosition = normalizedTarget;
      setProperty("target_position", targetPosition.load());
      
      // Start movement
      isMovingFlag = true;
      setProperty("is_moving", true);
    }

    logInfo("Starting movement to position: " + std::to_string(targetPosition.load()),
            deviceId);
            
  } catch (const std::exception &e) {
    logError("Failed to move to position: " + std::string(e.what()), deviceId);
    throw; // Rethrow to notify caller
  }
}

void Rotator::moveBy(double offset) {
  try {
    validateConnected();
    
    // Calculate new target position
    double newTarget = normalizeAngle(position + offset);
    moveTo(newTarget);
    
  } catch (const std::exception &e) {
    logError("Failed to move by offset: " + std::string(e.what()), deviceId);
    throw; // Rethrow to notify caller
  }
}

void Rotator::halt() {
  try {
    validateConnected();
    
    bool wasMoving = false;
    
    // Lock state for thread safety
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      
      wasMoving = isMovingFlag;
      if (wasMoving) {
        isMovingFlag = false;
        setProperty("is_moving", false);
        
        // Update target position to current position
        targetPosition = position.load();
        setProperty("target_position", targetPosition.load());
        
        current_move_message_id.clear();
      }
    }
    
    // Call hook outside of lock
    if (wasMoving) {
      onHalt();
      
      logInfo("Movement halted at position: " + std::to_string(position.load()),
              deviceId);
              
      // Send halt event
      EventMessage event("MOVEMENT_HALTED");
      event.setDetails({{"position", position.load()}});
      sendEvent(event);
    }
    
  } catch (const std::exception &e) {
    logError("Failed to halt movement: " + std::string(e.what()), deviceId);
    throw; // Rethrow to notify caller
  }
}

void Rotator::setReverse(bool reversed) {
  try {
    validateConnected();
    
    bool changed = false;
    
    // Lock for thread safety
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      
      changed = (isReversedFlag != reversed);
      if (changed) {
        isReversedFlag = reversed;
        setProperty("is_reversed", reversed);
      }
    }
    
    // Call hook and send event outside of lock
    if (changed) {
      onReverseChanged(reversed);
      
      logInfo("Reverse direction set to: " + std::string(reversed ? "true" : "false"),
              deviceId);
              
      // Send reverse changed event
      EventMessage event("REVERSE_CHANGED");
      event.setDetails({{"reversed", reversed}});
      sendEvent(event);
    }
    
  } catch (const std::exception &e) {
    logError("Failed to set reverse: " + std::string(e.what()), deviceId);
    throw; // Rethrow to notify caller
  }
}

void Rotator::sync(double newPosition) {
  try {
    validateConnected();
    
    // Normalize position
    double normalizedPosition = normalizeAngle(newPosition);
    
    // Stop if moving
    if (isMovingFlag) {
      halt();
    }
    
    // Lock for thread safety
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      
      // Set positions
      position = normalizedPosition;
      targetPosition = normalizedPosition;
      setProperty("position", position.load());
      setProperty("target_position", targetPosition.load());
    }
    
    logInfo("Position synced to: " + std::to_string(position.load()), deviceId);
    
    // Call hook for derived classes
    onPositionUpdate(position);
    
    // Send sync event
    EventMessage event("POSITION_SYNCED");
    event.setDetails({{"position", position.load()}});
    sendEvent(event);
    
  } catch (const std::exception &e) {
    logError("Failed to sync position: " + std::string(e.what()), deviceId);
    throw; // Rethrow to notify caller
  }
}

void Rotator::setStepSize(double newStepSize) {
  try {
    if (newStepSize <= 0.0) {
      throw std::invalid_argument("Step size must be greater than zero");
    }
    
    // Lock for thread safety
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      
      stepSize = newStepSize;
      setProperty("step_size", stepSize);
    }
    
    logInfo("Step size set to: " + std::to_string(stepSize), deviceId);
    
  } catch (const std::exception &e) {
    logError("Failed to set step size: " + std::string(e.what()), deviceId);
    throw; // Rethrow to notify caller
  }
}

void Rotator::setSpeed(double speed) {
  try {
    if (speed <= 0.0) {
      throw std::invalid_argument("Speed must be greater than zero");
    }
    
    if (speed > maxSpeed) {
      throw std::invalid_argument("Speed exceeds maximum allowed value");
    }
    
    // Lock for thread safety
    {
      std::lock_guard<std::mutex> lock(stateMutex);
      
      currentSpeed = speed;
      setProperty("current_speed", speed);
    }
    
    logInfo("Speed set to: " + std::to_string(speed) + " deg/s", deviceId);
    
  } catch (const std::exception &e) {
    logError("Failed to set speed: " + std::string(e.what()), deviceId);
    throw; // Rethrow to notify caller
  }
}

double Rotator::getPosition() const {
  return position.load();
}

double Rotator::getTargetPosition() const {
  return targetPosition.load();
}

bool Rotator::isMoving() const {
  return isMovingFlag.load();
}

bool Rotator::isReversed() const {
  return isReversedFlag.load();
}

double Rotator::getSpeed() const {
  return currentSpeed.load();
}

double Rotator::getMaxSpeed() const {
  return maxSpeed;
}

// Hook methods for derived classes to override
bool Rotator::onBeforeMove(double targetPos) {
  // Default implementation allows all moves
  return true;
}

void Rotator::onAfterMove(double finalPos) {
  // Empty default implementation
}

void Rotator::onPositionUpdate(double newPos) {
  // Empty default implementation
}

void Rotator::onHalt() {
  // Empty default implementation
}

void Rotator::onReverseChanged(bool reversed) {
  // Empty default implementation
}

void Rotator::updateLoop() {
  logInfo("Update loop started", deviceId);

  // Cached time variables for performance
  auto lastUpdateTime = std::chrono::steady_clock::now();

  while (update_running) {
    // Record start time of this cycle
    auto startTime = std::chrono::steady_clock::now();
    auto elapsed = startTime - lastUpdateTime;
    lastUpdateTime = startTime;

    // Check if movement is in progress
    if (isMovingFlag) {
      // Get current values safely
      double currentPos = position.load();
      double targetPos = targetPosition.load();
      
      // Calculate new position using virtual method
      double newPosition = calculateNewPosition(currentPos, targetPos, elapsed);
      
      // Check if we need to update the position
      if (std::abs(newPosition - currentPos) > 0.0001) {
        // Update position
        position = newPosition;
        setProperty("position", position.load());
        
        // Call position update hook
        onPositionUpdate(newPosition);
      }
      
      // Check if we've reached target (with small tolerance)
      if (std::abs(normalizeAngle(position - targetPosition)) < 0.01) {
        // Ensure exact position at end
        position = targetPosition.load();
        setProperty("position", position.load());
        
        {
          std::lock_guard<std::mutex> lock(stateMutex);
          isMovingFlag = false;
          setProperty("is_moving", false);
        }
        
        logInfo("Movement completed at position: " + std::to_string(position.load()),
                deviceId);
        
        // Store message ID before calling hook (which might clear it)
        std::string msgId = current_move_message_id;
        
        // Call completion hook
        onAfterMove(position);
        
        // Send move completed event
        if (!msgId.empty()) {
          sendMoveCompletedEvent(msgId);
          current_move_message_id.clear();
        }
      }
    }

    // Calculate elapsed time and sleep if needed
    auto cycleDuration = std::chrono::steady_clock::now() - startTime;
    if (cycleDuration < updateInterval) {
      std::this_thread::sleep_for(updateInterval - cycleDuration);
    }
  }

  logInfo("Update loop ended", deviceId);
}

double Rotator::calculateNewPosition(double currentPos, double targetPos, 
                                   std::chrono::duration<double> elapsed) {
  // Calculate shortest path and direction
  auto [distance, clockwise] = calculateRotationPath(currentPos, targetPos);
  
  // If reversed, invert direction
  if (isReversedFlag) {
    clockwise = !clockwise;
  }
  
  // Calculate move amount based on speed and elapsed time
  double moveAmount = currentSpeed * elapsed.count();
  
  // Limit move amount to remaining distance
  if (moveAmount > distance) {
    moveAmount = distance;
  }
  
  // Apply movement
  double newPosition;
  if (clockwise) {
    newPosition = normalizeAngle(currentPos + moveAmount);
  } else {
    newPosition = normalizeAngle(currentPos - moveAmount);
  }
  
  return newPosition;
}

void Rotator::handleMoveToCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("position")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'position'"}});
      return;
    }

    double newPosition = params["position"];

    // Save message ID for completion event
    current_move_message_id = cmd.getMessageId();

    // Start movement
    moveTo(newPosition);

    // Calculate estimated completion time
    auto [distance, clockwise] = calculateRotationPath(position, targetPosition);
    double estimatedSeconds = distance / currentSpeed;

    auto now = std::chrono::system_clock::now();
    auto completeTime = now + std::chrono::milliseconds(
                                static_cast<int>(estimatedSeconds * 1000));
    auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
    std::ostringstream est_ss;
    est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

    response.setStatus("IN_PROGRESS");
    response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"progressPercentage", 0},
                       {"startPosition", position.load()},
                       {"targetPosition", targetPosition.load()},
                       {"distance", distance}});
                       
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXECUTION_ERROR"},
                       {"message", e.what()}});
  }
}

void Rotator::handleMoveByCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("offset")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'offset'"}});
      return;
    }

    double offset = params["offset"];

    // Save message ID for completion event
    current_move_message_id = cmd.getMessageId();

    // Start relative movement
    moveBy(offset);

    // Calculate estimated completion time
    auto [distance, clockwise] = calculateRotationPath(position, targetPosition);
    double estimatedSeconds = distance / currentSpeed;

    auto now = std::chrono::system_clock::now();
    auto completeTime = now + std::chrono::milliseconds(
                                static_cast<int>(estimatedSeconds * 1000));
    auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
    std::ostringstream est_ss;
    est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

    response.setStatus("IN_PROGRESS");
    response.setDetails({{"estimatedCompletionTime", est_ss.str()},
                       {"progressPercentage", 0},
                       {"startPosition", position.load()},
                       {"targetPosition", targetPosition.load()},
                       {"offset", offset},
                       {"distance", distance}});
                       
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXECUTION_ERROR"},
                       {"message", e.what()}});
  }
}

void Rotator::handleHaltCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  try {
    // Stop current movement
    halt();

    response.setStatus("SUCCESS");
    response.setDetails({{"position", position.load()}, {"message", "Movement halted"}});
    
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXECUTION_ERROR"},
                       {"message", e.what()}});
  }
}

void Rotator::handleReverseCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("reversed")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'reversed'"}});
      return;
    }

    bool reversed = params["reversed"];

    // Set reverse state
    setReverse(reversed);

    response.setStatus("SUCCESS");
    response.setDetails({{"reversed", reversed}});
    
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXECUTION_ERROR"},
                       {"message", e.what()}});
  }
}

void Rotator::handleSyncCommand(const CommandMessage &cmd,
                                ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("position")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'position'"}});
      return;
    }

    double newPosition = params["position"];

    // Sync position
    sync(newPosition);

    response.setStatus("SUCCESS");
    response.setDetails({{"position", position.load()}});
    
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXECUTION_ERROR"},
                       {"message", e.what()}});
  }
}

void Rotator::handleSetSpeedCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("speed")) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'speed'"}});
      return;
    }

    double speed = params["speed"];

    // Set speed
    setSpeed(speed);

    response.setStatus("SUCCESS");
    response.setDetails({{"speed", speed}});
    
  } catch (const std::exception &e) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "EXECUTION_ERROR"},
                       {"message", e.what()}});
  }
}

void Rotator::sendMoveCompletedEvent(const std::string &relatedMessageId) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  event.setDetails({{"command", "MOVE_TO"},
                    {"status", "SUCCESS"},
                    {"finalPosition", position.load()}});

  sendEvent(event);
}

double Rotator::normalizeAngle(double angle) const {
  // Normalize angle to 0-360 degree range
  angle = std::fmod(angle, 360.0);
  if (angle < 0.0) {
    angle += 360.0;
  }
  return angle;
}

std::pair<double, bool> Rotator::calculateRotationPath(double current,
                                                       double target) const {
  // Normalize angles
  current = normalizeAngle(current);
  target = normalizeAngle(target);

  // Calculate clockwise and counter-clockwise distances
  double clockwiseDistance =
      (target >= current) ? target - current : target + 360.0 - current;

  double counterClockwiseDistance =
      (current >= target) ? current - target : current + 360.0 - target;

  // Choose shortest path
  if (clockwiseDistance <= counterClockwiseDistance) {
    return {clockwiseDistance, true}; // clockwise, distance
  } else {
    return {counterClockwiseDistance, false}; // counter-clockwise, distance
  }
}

} // namespace astrocomm