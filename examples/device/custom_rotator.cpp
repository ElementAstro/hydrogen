#include "custom_rotator.h"
#include "common/logger.h"
#include <cmath>
#include <sstream>

namespace astrocomm {

CustomRotator::CustomRotator(const std::string &deviceId,
                             const std::string &manufacturer,
                             const std::string &model,
                             double limitMin, double limitMax)
    : Rotator(deviceId, manufacturer, model), minLimit(limitMin),
      maxLimit(limitMax), moveCount(0), totalDistance(0.0) {
  
  // Add additional properties for statistics and limits
  setProperty("min_limit", minLimit);
  setProperty("max_limit", maxLimit);
  setProperty("move_count", moveCount);
  setProperty("total_distance", totalDistance);
  
  // Add custom capabilities
  capabilities.push_back("POSITION_LIMITS");
  
  logInfo("CustomRotator initialized with limits: " + std::to_string(minLimit) +
          " to " + std::to_string(maxLimit), deviceId);
}

CustomRotator::~CustomRotator() {
  // Cleanup any custom resources if needed
  logInfo("CustomRotator is being destroyed", deviceId);
}

void CustomRotator::setLimits(double minPos, double maxPos) {
  if (minPos >= maxPos) {
    throw std::invalid_argument("Minimum limit must be less than maximum limit");
  }
  
  // Normalize limits to 0-360 range
  minLimit = normalizeAngle(minPos);
  maxLimit = normalizeAngle(maxPos);
  
  // Update properties
  setProperty("min_limit", minLimit);
  setProperty("max_limit", maxLimit);
  
  logInfo("Position limits updated: " + std::to_string(minLimit) +
          " to " + std::to_string(maxLimit), deviceId);
          
  // If current position is outside new limits, log a warning
  double currentPos = getPosition();
  if (!isPositionWithinLimits(currentPos)) {
    logWarning("Current position " + std::to_string(currentPos) + 
              " is outside the new limits", deviceId);
  }
  
  // Send an event about the limit change
  EventMessage event("LIMITS_CHANGED");
  event.setDetails({{"min_limit", minLimit}, {"max_limit", maxLimit}});
  sendEvent(event);
}

double CustomRotator::getMinLimit() const {
  return minLimit;
}

double CustomRotator::getMaxLimit() const {
  return maxLimit;
}

bool CustomRotator::onBeforeMove(double targetPos) {
  // Check if target position is within limits
  if (!isPositionWithinLimits(targetPos)) {
    logWarning("Movement to " + std::to_string(targetPos) + 
              " rejected: outside position limits", deviceId);
    
    // Send event about the rejected move
    EventMessage event("MOVEMENT_REJECTED");
    event.setDetails({
      {"target_position", targetPos},
      {"reason", "OUTSIDE_LIMITS"},
      {"min_limit", minLimit},
      {"max_limit", maxLimit}
    });
    sendEvent(event);
    
    return false;
  }
  
  // Calculate the distance for this movement
  auto startPos = getPosition();
  auto [distance, clockwise] = calculateRotationPath(startPos, targetPos);
  
  // Log information about the upcoming movement
  std::stringstream ss;
  ss << "Preparing movement from " << startPos << " to " << targetPos
     << " (distance: " << distance << "°, direction: " 
     << (clockwise ? "clockwise" : "counter-clockwise") << ")";
  logInfo(ss.str(), deviceId);
  
  return true; // Allow movement
}

void CustomRotator::onAfterMove(double finalPos) {
  // Update statistics
  moveCount++;
  
  // Calculate the distance moved in this operation
  double startPos = getTargetPosition();  // We saved this when movement started
  auto [distance, clockwise] = calculateRotationPath(startPos, finalPos);
  totalDistance += distance;
  
  // Update properties
  setProperty("move_count", moveCount);
  setProperty("total_distance", totalDistance);
  
  // Log completion information
  std::stringstream ss;
  ss << "Movement completed to position " << finalPos 
     << " (total movements: " << moveCount 
     << ", total distance: " << totalDistance << "°)";
  logInfo(ss.str(), deviceId);
  
  // Send extended event with statistics
  EventMessage event("MOVEMENT_COMPLETED_EXTENDED");
  event.setDetails({
    {"final_position", finalPos},
    {"move_count", moveCount},
    {"total_distance", totalDistance},
    {"last_distance", distance}
  });
  sendEvent(event);
}

void CustomRotator::onPositionUpdate(double newPos) {
  // Check if position is approaching limits
  double buffer = 10.0;  // Warning buffer in degrees
  
  // Only warn if we're moving and approaching a limit
  if (isMoving()) {
    bool approachingMin = false;
    bool approachingMax = false;
    
    // Check if approaching min limit
    if (minLimit + buffer > 0 && newPos <= minLimit + buffer && newPos > minLimit) {
      approachingMin = true;
    }
    
    // Check if approaching max limit
    if (maxLimit - buffer < 360 && newPos >= maxLimit - buffer && newPos < maxLimit) {
      approachingMax = true;
    }
    
    // Send warning if approaching limits
    if (approachingMin || approachingMax) {
      std::string limitType = approachingMin ? "minimum" : "maximum";
      double limit = approachingMin ? minLimit : maxLimit;
      double distance = std::abs(newPos - limit);
      
      logWarning("Approaching " + limitType + " position limit, " +
                std::to_string(distance) + "° remaining", deviceId);
                
      // Send approaching limit event
      EventMessage event("APPROACHING_LIMIT");
      event.setDetails({
        {"current_position", newPos},
        {"limit_type", limitType},
        {"limit_value", limit},
        {"remaining_distance", distance}
      });
      sendEvent(event);
    }
  }
}

void CustomRotator::onHalt() {
  // Log halt with additional information
  std::stringstream ss;
  ss << "Movement halted at position " << getPosition()
     << " (target was: " << getTargetPosition() << ")";
  logWarning(ss.str(), deviceId);
  
  // Reset any internal states if needed
  // ...
  
  // Send extended halt event
  EventMessage event("MOVEMENT_HALTED_EXTENDED");
  event.setDetails({
    {"halt_position", getPosition()},
    {"original_target", getTargetPosition()},
    {"remaining_distance", std::abs(getPosition() - getTargetPosition())}
  });
  sendEvent(event);
}

void CustomRotator::onReverseChanged(bool reversed) {
  // Log the change
  logInfo("Rotator direction changed to: " + 
          std::string(reversed ? "reversed" : "normal"), deviceId);
  
  // Perform any custom actions needed when direction changes
  // ...
}

// Private helper methods

bool CustomRotator::isPositionWithinLimits(double position) const {
  // Normalize the position
  double pos = normalizeAngle(position);
  
  // Special case: if min > max, it means we can rotate through 0°
  if (minLimit > maxLimit) {
    return (pos >= minLimit && pos <= 360.0) || (pos >= 0.0 && pos <= maxLimit);
  }
  
  // Normal case: position must be between min and max
  return pos >= minLimit && pos <= maxLimit;
}

} // namespace astrocomm