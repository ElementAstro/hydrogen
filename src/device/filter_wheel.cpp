#include "device/filter_wheel.h"

#include <spdlog/spdlog.h> // Use spdlog for logging

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace astrocomm {

FilterWheel::FilterWheel(const std::string &deviceId,
                         const std::string &manufacturer,
                         const std::string &model)
    : DeviceBase(deviceId, "FILTER_WHEEL", manufacturer, model), position(0),
      targetPosition(0), filterCount(5), isMoving(false), moveDirection(1),
      updateRunning(false) {

  try {
    // Initialize filter names and offsets
    filterNames = {"Red", "Green", "Blue", "Luminance", "H-Alpha"};
    filterOffsets = {0, 0, 0, 0, 0}; // Default: no offsets

    // Initialize properties
    setProperty("position", position);
    setProperty("filterCount", filterCount);
    setProperty("filterNames", filterNames);
    setProperty("filterOffsets", filterOffsets);
    setProperty("isMoving", false);
    setProperty("connected", false);
    setProperty("currentFilter", getCurrentFilterName());

    // Set capabilities
    capabilities = {"NAMED_FILTERS", "FILTER_OFFSETS"};

    // Register command handlers
    registerCommandHandler("SET_POSITION", [this](const CommandMessage &cmd,
                                                  ResponseMessage &response) {
      handleSetPositionCommand(cmd, response);
    });

    registerCommandHandler(
        "SET_FILTER_NAMES",
        [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleSetFilterNamesCommand(cmd, response);
        });

    registerCommandHandler(
        "SET_FILTER_OFFSETS",
        [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleSetFilterOffsetsCommand(cmd, response);
        });

    registerCommandHandler(
        "ABORT", [this](const CommandMessage &cmd, ResponseMessage &response) {
          handleAbortCommand(cmd, response);
        });

    spdlog::info("FilterWheel device {} initialized with {} filters", deviceId,
                 filterCount);
  } catch (const std::exception &e) {
    spdlog::error("Error initializing FilterWheel {}: {}", deviceId, e.what());
    throw FilterWheelException("Failed to initialize filter wheel: " +
                               std::string(e.what()));
  }
}

FilterWheel::~FilterWheel() {
  try {
    stop();
  } catch (const std::exception &e) {
    // Log error but don't rethrow in destructor
    spdlog::error("Error in FilterWheel {} destructor: {}", deviceId, e.what());
  }
}

bool FilterWheel::start() {
  try {
    if (!DeviceBase::start()) {
      return false;
    }

    // Start update thread
    updateRunning = true;
    updateThread = std::thread(&FilterWheel::updateLoop, this);

    setProperty("connected", true);
    spdlog::info("FilterWheel {} started", deviceId);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Failed to start FilterWheel {}: {}", deviceId, e.what());
    setProperty("connected", false);
    updateRunning = false;
    return false;
  }
}

void FilterWheel::stop() {
  try {
    // Stop update thread
    updateRunning = false;
    if (updateThread.joinable()) {
      updateThread.join();
    }

    setProperty("connected", false);
    DeviceBase::stop();
    spdlog::info("FilterWheel {} stopped", deviceId);
  } catch (const std::exception &e) {
    spdlog::error("Error stopping FilterWheel {}: {}", deviceId, e.what());
    throw FilterWheelException("Failed to stop filter wheel: " +
                               std::string(e.what()));
  }
}

void FilterWheel::setPosition(int newPosition) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    // Validate position
    validatePosition(newPosition);

    // If already at the target position and not moving, do nothing
    if (newPosition == position && !isMoving) {
      spdlog::info("FilterWheel {} already at position {} ({})", deviceId,
                   position, getCurrentFilterName());
      return;
    }

    // Determine shortest path direction
    moveDirection = determineShortestPath(position, newPosition);

    targetPosition = newPosition;
    isMoving = true;
    setProperty("isMoving", true);

    spdlog::info("FilterWheel {} starting move to position {} ({})", deviceId,
                 targetPosition, filterNames[targetPosition]);
  } catch (const FilterWheelException &e) {
    spdlog::warn("FilterWheel {}: {}", deviceId, e.what());
    throw;
  } catch (const std::exception &e) {
    std::string errorMsg = "Error setting position: " + std::string(e.what());
    spdlog::error("FilterWheel {}: {}", deviceId, errorMsg);
    throw FilterWheelException(errorMsg);
  }
}

void FilterWheel::validatePosition(int posToValidate) const {
  if (posToValidate < 0 || posToValidate >= filterCount) {
    throw FilterWheelException(
        "Invalid position: " + std::to_string(posToValidate) +
        ", must be between 0 and " + std::to_string(filterCount - 1));
  }
}

void FilterWheel::setFilterNames(const std::vector<std::string> &names) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    // Ensure name count matches filter count
    if (names.size() != static_cast<size_t>(filterCount)) {
      throw FilterWheelException(
          "Filter names count (" + std::to_string(names.size()) +
          ") doesn't match filter count (" + std::to_string(filterCount) + ")");
    }

    filterNames = names;
    setProperty("filterNames", filterNames);
    setProperty("currentFilter", getCurrentFilterName());

    spdlog::info("FilterWheel {} filter names updated", deviceId);
  } catch (const FilterWheelException &e) {
    spdlog::warn("FilterWheel {}: {}", deviceId, e.what());
    throw;
  } catch (const std::exception &e) {
    std::string errorMsg =
        "Error setting filter names: " + std::string(e.what());
    spdlog::error("FilterWheel {}: {}", deviceId, errorMsg);
    throw FilterWheelException(errorMsg);
  }
}

void FilterWheel::setFilterOffsets(const std::vector<int> &offsets) {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    // Ensure offset count matches filter count
    if (offsets.size() != static_cast<size_t>(filterCount)) {
      throw FilterWheelException(
          "Filter offsets count (" + std::to_string(offsets.size()) +
          ") doesn't match filter count (" + std::to_string(filterCount) + ")");
    }

    filterOffsets = offsets;
    setProperty("filterOffsets", filterOffsets);

    spdlog::info("FilterWheel {} filter offsets updated", deviceId);
  } catch (const FilterWheelException &e) {
    spdlog::warn("FilterWheel {}: {}", deviceId, e.what());
    throw;
  } catch (const std::exception &e) {
    std::string errorMsg =
        "Error setting filter offsets: " + std::string(e.what());
    spdlog::error("FilterWheel {}: {}", deviceId, errorMsg);
    throw FilterWheelException(errorMsg);
  }
}

void FilterWheel::abort() {
  try {
    std::lock_guard<std::mutex> lock(statusMutex);

    if (!isMoving) {
      spdlog::info("FilterWheel {}: No movement to abort", deviceId);
      return;
    }

    isMoving = false;
    targetPosition = position; // Stay at current position
    currentMoveMessageId.clear();
    setProperty("isMoving", false);

    spdlog::info("FilterWheel {} movement aborted at position {}", deviceId,
                 position);

    // Send abort event
    EventMessage event("ABORTED");
    event.setDetails(
        {{"position", position}, {"filter", getCurrentFilterName()}});
    sendEvent(event);
  } catch (const std::exception &e) {
    std::string errorMsg = "Error aborting movement: " + std::string(e.what());
    spdlog::error("FilterWheel {}: {}", deviceId, errorMsg);
    throw FilterWheelException(errorMsg);
  }
}

bool FilterWheel::isMovementComplete() const {
  std::lock_guard<std::mutex> lock(statusMutex);
  return !isMoving;
}

int FilterWheel::getMaxFilterCount() const {
  // Default max filter count - can be overridden by subclasses
  return 10;
}

void FilterWheel::setFilterCount(int count) {
  try {
    if (count <= 0 || count > getMaxFilterCount()) {
      throw FilterWheelException(
          "Invalid filter count: " + std::to_string(count) +
          ", must be between 1 and " + std::to_string(getMaxFilterCount()));
    }

    std::lock_guard<std::mutex> lock(statusMutex);

    // Only allow changing filter count when not moving
    if (isMoving) {
      throw FilterWheelException("Cannot change filter count while moving");
    }

    filterCount = count;

    // Resize and initialize filter names and offsets
    filterNames.resize(count);
    filterOffsets.resize(count);

    // Fill in default names for new entries
    for (int i = 0; i < count; i++) {
      if (filterNames[i].empty()) {
        filterNames[i] = "Filter " + std::to_string(i + 1);
      }
      filterOffsets[i] = 0;
    }

    setProperty("filterCount", filterCount);
    setProperty("filterNames", filterNames);
    setProperty("filterOffsets", filterOffsets);

    // Reset position if current position would be out of range
    if (position >= count) {
      position = 0;
      targetPosition = 0;
      setProperty("position", position);
    }

    spdlog::info("FilterWheel {} filter count updated to {}", deviceId, count);
  } catch (const FilterWheelException &e) {
    spdlog::warn("FilterWheel {}: {}", deviceId, e.what());
    throw;
  } catch (const std::exception &e) {
    std::string errorMsg =
        "Error setting filter count: " + std::string(e.what());
    spdlog::error("FilterWheel {}: {}", deviceId, errorMsg);
    throw FilterWheelException(errorMsg);
  }
}

std::string FilterWheel::getCurrentFilterName() const {
  try {
    // No lock needed here as position and filterNames are read atomically
    // or filterNames size is checked first.
    if (position >= 0 && static_cast<size_t>(position) < filterNames.size()) {
      return filterNames[position];
    }
    return "Unknown";
  } catch (const std::exception &e) {
    // This catch might be overly broad, consider if specific exceptions are
    // expected
    spdlog::error("FilterWheel {}: Error getting filter name: {}", deviceId,
                  e.what());
    return "Error";
  }
}

int FilterWheel::getCurrentFilterOffset() const {
  try {
    // No lock needed here for similar reasons as getCurrentFilterName
    if (position >= 0 && static_cast<size_t>(position) < filterOffsets.size()) {
      return filterOffsets[position];
    }
    return 0; // Default offset
  } catch (const std::exception &e) {
    spdlog::error("FilterWheel {}: Error getting filter offset: {}", deviceId,
                  e.what());
    return 0; // Return default offset on error
  }
}

int FilterWheel::determineShortestPath(int from, int to) const {
  try {
    // No lock needed, filterCount is read atomically
    // Calculate clockwise and counter-clockwise distances
    int clockwise = (to >= from) ? (to - from) : (filterCount - from + to);
    int counterClockwise =
        (from >= to) ? (from - to) : (from + filterCount - to);

    // Return the direction of the shortest path
    return (clockwise <= counterClockwise) ? 1 : -1;
  } catch (const std::exception &e) {
    // This catch might be overly broad
    spdlog::error("FilterWheel {}: Error determining path: {}", deviceId,
                  e.what());
    return 1; // Default to clockwise if error
  }
}

void FilterWheel::updateLoop() {
  spdlog::info("FilterWheel {} update loop started", deviceId);

  try {
    auto lastTime = std::chrono::steady_clock::now();

    while (updateRunning) {
      // Update approximately every 100ms
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      auto currentTime = std::chrono::steady_clock::now();
      double elapsedSec =
          std::chrono::duration<double>(currentTime - lastTime).count();
      lastTime = currentTime;

      simulateMovement(elapsedSec);
    }
  } catch (const std::exception &e) {
    spdlog::error("FilterWheel {}: Error in update loop: {}", deviceId,
                  e.what());
    // Consider how to handle this error, maybe set an error state
    updateRunning = false; // Stop the loop on error
  }

  spdlog::info("FilterWheel {} update loop ended", deviceId);
}

void FilterWheel::simulateMovement(double elapsedSec) {
  // Thread-safe state update
  std::lock_guard<std::mutex> lock(statusMutex);

  // If currently moving
  if (isMoving) {
    // Filter wheel rotation speed - 1 position per second (can be adjusted)
    // Using a static variable here might cause issues if multiple FilterWheel
    // instances exist. Consider making it a member variable if needed.
    static double progressFraction = 0.0;
    progressFraction += elapsedSec;

    // Define movement speed (positions per second)
    const double positionsPerSecond = 1.0;
    const double timePerPosition = 1.0 / positionsPerSecond;

    if (progressFraction >= timePerPosition) {
      // Enough time has passed to move to next position
      progressFraction -=
          timePerPosition; // Subtract time used, don't reset to 0

      // Update position
      updatePositionInternal(); // Renamed to avoid confusion with public
                                // setPosition
    }
  }
}

// Internal helper, assumes lock is held
void FilterWheel::updatePositionInternal() {
  // Update position in the movement direction
  position = (position + moveDirection + filterCount) % filterCount;
  setProperty("position", position);
  setProperty("currentFilter",
              getCurrentFilterName()); // Update filter name property

  // Check if target position reached
  if (position == targetPosition) {
    isMoving = false;
    setProperty("isMoving", false);

    // Send movement complete event
    if (!currentMoveMessageId.empty()) {
      sendPositionChangeCompletedEvent(currentMoveMessageId);
      currentMoveMessageId.clear(); // Clear message ID after sending event
    }

    spdlog::info("FilterWheel {} move completed at position {} ({})", deviceId,
                 position, getCurrentFilterName());
  }
}

void FilterWheel::sendPositionChangeCompletedEvent(
    const std::string &relatedMessageId) {
  try {
    EventMessage event("COMMAND_COMPLETED");
    event.setRelatedMessageId(relatedMessageId);

    event.setDetails({{"command", "SET_POSITION"},
                      {"status", "SUCCESS"},
                      {"position", position},
                      {"filter", getCurrentFilterName()},
                      {"offset", getCurrentFilterOffset()}});

    sendEvent(event);
  } catch (const std::exception &e) {
    spdlog::error("FilterWheel {}: Error sending event: {}", deviceId,
                  e.what());
  }
}

// Command handler implementations
void FilterWheel::handleSetPositionCommand(const CommandMessage &cmd,
                                           ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("position") && !params.contains("filter")) {
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_PARAMETERS"},
           {"message", "Missing required parameter 'position' or 'filter'"}});
      return;
    }

    int newPosition = -1;
    std::string targetFilterName = ""; // Store target name for response

    // Position can be set by position number or filter name
    if (params.contains("position")) {
      newPosition = params["position"];
      // Validate position early
      try {
        validatePosition(newPosition);
        targetFilterName =
            filterNames[newPosition]; // Get name after validation
      } catch (const FilterWheelException &e) {
        response.setStatus("ERROR");
        response.setDetails(
            {{"error", "INVALID_POSITION"}, {"message", e.what()}});
        return;
      }
    } else if (params.contains("filter")) {
      std::string filterNameParam = params["filter"];
      targetFilterName = filterNameParam; // Use provided name for response
      // Find position for the filter name (requires lock)
      std::lock_guard<std::mutex> lock(
          statusMutex); // Lock needed to access filterNames safely
      auto it =
          std::find(filterNames.begin(), filterNames.end(), filterNameParam);
      if (it != filterNames.end()) {
        newPosition = static_cast<int>(std::distance(filterNames.begin(), it));
      } else {
        response.setStatus("ERROR");
        response.setDetails(
            {{"error", "INVALID_FILTER"},
             {"message", "Filter name not found: " + filterNameParam}});
        return;
      }
    }

    // Store message ID for completion event *before* calling setPosition
    // which might complete immediately if already at position.
    currentMoveMessageId = cmd.getMessageId();

    // Start moving (or log if already there)
    setPosition(newPosition); // This handles locking internally

    // Calculate estimated completion time (requires lock for current state)
    int estimatedSeconds = 0;
    double progressPercentage = 0.0;
    {
      std::lock_guard<std::mutex> lock(
          statusMutex); // Lock to read current state

      if (isMoving) { // Check if setPosition actually started a move
        int currentPos = position;      // Use locked position
        int targetPos = targetPosition; // Use locked target position
        int moveSteps = 0;

        // Calculate shortest path steps
        int clockwise = (targetPos >= currentPos)
                            ? (targetPos - currentPos)
                            : (filterCount - currentPos + targetPos);

        int counterClockwise = (currentPos >= targetPos)
                                   ? (currentPos - targetPos)
                                   : (currentPos + filterCount - targetPos);

        moveSteps = std::min(clockwise, counterClockwise);

        // Estimate time based on 1 second per step (adjust if speed changes)
        estimatedSeconds = moveSteps;
        if (estimatedSeconds == 0 && newPosition != currentPos) {
          // Should not happen if isMoving is true, but as a fallback
          estimatedSeconds = 1;
        }
        progressPercentage = 0.0; // Just started moving
      } else {
        // Already at target position or move completed instantly
        estimatedSeconds = 0; // No time needed
        progressPercentage = 100.0;
      }
    } // Unlock statusMutex

    // Format estimated completion time
    std::string est_completion_str = "N/A";
    if (estimatedSeconds > 0) {
      auto now = std::chrono::system_clock::now();
      auto completeTime = now + std::chrono::seconds(estimatedSeconds);
      auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
      // Using gmtime might not be thread-safe on all platforms, consider
      // alternatives if needed
      std::tm gmt_buf;
#ifdef _WIN32
      gmtime_s(&gmt_buf, &complete_itt);
#else
      gmtime_r(&complete_itt, &gmt_buf); // POSIX version
#endif
      std::ostringstream est_ss;
      est_ss << std::put_time(&gmt_buf, "%FT%T") << "Z";
      est_completion_str = est_ss.str();
    }

    response.setStatus(
        "IN_PROGRESS"); // Even if complete, indicates command accepted
    response.setDetails(
        {{"estimatedCompletionTime", est_completion_str},
         {"progressPercentage", progressPercentage},
         {"targetPosition", newPosition},    // Use the validated target
         {"targetFilter", targetFilterName}, // Use the target name
         {"currentPosition",
          position}, // Use current (possibly updated) position
         {"currentFilter", getCurrentFilterName()}, // Use current filter name
         {"offset", getCurrentFilterOffset()}});    // Use current offset
  } catch (const std::exception &e) {
    spdlog::error("FilterWheel {}: Error handling set position command: {}",
                  deviceId, e.what());
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "COMMAND_FAILED"},
         {"message", "Failed to set position: " + std::string(e.what())}});
    currentMoveMessageId.clear(); // Clear message ID on error
  }
}

void FilterWheel::handleSetFilterNamesCommand(const CommandMessage &cmd,
                                              ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("names") || !params["names"].is_array()) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing or invalid 'names' parameter "
                                       "(must be an array of strings)"}});
      return;
    }

    std::vector<std::string> names =
        params["names"].get<std::vector<std::string>>();

    // Call setFilterNames which handles locking and validation
    setFilterNames(names);

    response.setStatus("SUCCESS");
    response.setDetails(
        {{"filterNames", filterNames}}); // Return the updated names
  } catch (const FilterWheelException &e) {
    // Catch specific exception from setFilterNames for better error reporting
    spdlog::warn("FilterWheel {}: Failed to set filter names: {}", deviceId,
                 e.what());
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_DATA"}, // Or more specific error code if available
         {"message", e.what()}});
  } catch (const std::exception &e) {
    spdlog::error("FilterWheel {}: Error handling set filter names command: {}",
                  deviceId, e.what());
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "COMMAND_FAILED"},
         {"message", "Failed to set filter names: " + std::string(e.what())}});
  }
}

void FilterWheel::handleSetFilterOffsetsCommand(const CommandMessage &cmd,
                                                ResponseMessage &response) {
  try {
    json params = cmd.getParameters();

    if (!params.contains("offsets") || !params["offsets"].is_array()) {
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                           {"message", "Missing or invalid 'offsets' parameter "
                                       "(must be an array of integers)"}});
      return;
    }

    std::vector<int> offsets = params["offsets"].get<std::vector<int>>();

    // Call setFilterOffsets which handles locking and validation
    setFilterOffsets(offsets);

    response.setStatus("SUCCESS");
    response.setDetails(
        {{"filterOffsets", filterOffsets}}); // Return updated offsets
  } catch (const FilterWheelException &e) {
    // Catch specific exception from setFilterOffsets
    spdlog::warn("FilterWheel {}: Failed to set filter offsets: {}", deviceId,
                 e.what());
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_DATA"}, // Or more specific error code
         {"message", e.what()}});
  } catch (const std::exception &e) {
    spdlog::error(
        "FilterWheel {}: Error handling set filter offsets command: {}",
        deviceId, e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "COMMAND_FAILED"},
                         {"message", "Failed to set filter offsets: " +
                                         std::string(e.what())}});
  }
}

void FilterWheel::handleAbortCommand(const CommandMessage &cmd,
                                     ResponseMessage &response) {
  try {
    // Call abort which handles locking
    abort();

    response.setStatus("SUCCESS");
    response.setDetails(
        {{"message", "Movement aborted"},
         {"position", position}, // Return current position after abort
         {"filter", getCurrentFilterName()}});
  } catch (const FilterWheelException &e) {
    // Catch specific exception from abort
    spdlog::error("FilterWheel {}: Failed to abort movement: {}", deviceId,
                  e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "ABORT_FAILED"}, {"message", e.what()}});
  } catch (const std::exception &e) {
    spdlog::error("FilterWheel {}: Error handling abort command: {}", deviceId,
                  e.what());
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "COMMAND_FAILED"},
         {"message", "Failed to abort: " + std::string(e.what())}});
  }
}

} // namespace astrocomm