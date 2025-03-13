#include "device/telescope.h"
#include "common/logger.h"

namespace astrocomm {

Telescope::Telescope(const std::string &deviceId,
                     const std::string &manufacturer, const std::string &model)
    : DeviceBase(deviceId, "TELESCOPE", manufacturer, model), ra(0.0), dec(0.0),
      altitude(45.0), azimuth(180.0), slew_rate(3), tracking(false),
      is_parked(false), is_moving(false), target_ra(0.0), target_dec(0.0),
      update_running(false), observer_latitude(40.0),
      observer_longitude(-74.0) {

  // Initialize properties
  setProperty("ra", ra);
  setProperty("dec", dec);
  setProperty("altitude", altitude);
  setProperty("azimuth", azimuth);
  setProperty("slew_rate", slew_rate);
  setProperty("tracking", tracking);
  setProperty("parked", is_parked);
  setProperty("connected", false);

  // Set capabilities
  capabilities = {"GOTO", "TRACKING", "ALIGNMENT", "PARKING"};

  // Register command handlers
  registerCommandHandler(
      "GOTO", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleGotoCommand(cmd, response);
      });

  registerCommandHandler("SET_TRACKING", [this](const CommandMessage &cmd,
                                                ResponseMessage &response) {
    handleTrackingCommand(cmd, response);
  });

  registerCommandHandler(
      "PARK", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleParkCommand(cmd, response);
      });

  registerCommandHandler(
      "SYNC", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleSyncCommand(cmd, response);
      });

  registerCommandHandler(
      "ABORT", [this](const CommandMessage &cmd, ResponseMessage &response) {
        handleAbortCommand(cmd, response);
      });

  logInfo("Telescope device initialized", deviceId);
}

Telescope::~Telescope() { stop(); }

bool Telescope::start() {
  if (!DeviceBase::start()) {
    return false;
  }

  // Start the update thread
  update_running = true;
  update_thread = std::thread(&Telescope::updateLoop, this);

  setProperty("connected", true);
  logInfo("Telescope started", deviceId);
  return true;
}

void Telescope::stop() {
  // Stop the update thread
  update_running = false;
  if (update_thread.joinable()) {
    update_thread.join();
  }

  setProperty("connected", false);
  DeviceBase::stop();
  logInfo("Telescope stopped", deviceId);
}

void Telescope::gotoPosition(double ra_target, double dec_target) {
  if (is_parked) {
    logWarning("Cannot GOTO: telescope is parked", deviceId);
    return;
  }

  // Validate input
  if (ra_target < 0 || ra_target >= 24) {
    logWarning("Invalid RA value: " + std::to_string(ra_target), deviceId);
    return;
  }

  if (dec_target < -90 || dec_target > 90) {
    logWarning("Invalid DEC value: " + std::to_string(dec_target), deviceId);
    return;
  }

  target_ra = ra_target;
  target_dec = dec_target;
  is_moving = true;

  logInfo("Starting GOTO to RA: " + std::to_string(ra_target) +
              ", DEC: " + std::to_string(dec_target),
          deviceId);
}

void Telescope::setTracking(bool enabled) {
  if (is_parked && enabled) {
    logWarning("Cannot enable tracking: telescope is parked", deviceId);
    return;
  }

  tracking = enabled;
  setProperty("tracking", tracking);

  logInfo("Tracking " + std::string(enabled ? "enabled" : "disabled"),
          deviceId);
}

void Telescope::setSlewRate(int rate) {
  if (rate < 1 || rate > 10) {
    logWarning("Invalid slew rate: " + std::to_string(rate), deviceId);
    return;
  }

  slew_rate = rate;
  setProperty("slew_rate", slew_rate);

  logInfo("Slew rate set to " + std::to_string(rate), deviceId);
}

void Telescope::abort() {
  is_moving = false;
  current_goto_message_id.clear();

  logInfo("Movement aborted", deviceId);

  // Send an event
  EventMessage event("ABORTED");
  sendEvent(event);
}

void Telescope::park() {
  if (is_parked) {
    logInfo("Telescope already parked", deviceId);
    return;
  }

  // Save current position
  setProperty("park_ra", ra);
  setProperty("park_dec", dec);

  // Go to park position (pointing to celestial pole)
  gotoPosition(0.0, observer_latitude > 0 ? 90.0 : -90.0);

  // Wait for the GOTO to complete
  while (is_moving) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Mark as parked and disable tracking
  is_parked = true;
  setProperty("parked", true);
  setTracking(false);

  logInfo("Telescope parked", deviceId);

  // Send parked event
  EventMessage event("PARKED");
  sendEvent(event);
}

void Telescope::unpark() {
  if (!is_parked) {
    logInfo("Telescope not parked", deviceId);
    return;
  }

  is_parked = false;
  setProperty("parked", false);

  // Restore previous position if available
  json park_ra = getProperty("park_ra");
  json park_dec = getProperty("park_dec");

  if (!park_ra.is_null() && !park_dec.is_null()) {
    gotoPosition(park_ra.get<double>(), park_dec.get<double>());
  }

  logInfo("Telescope unparked", deviceId);

  // Send unparked event
  EventMessage event("UNPARKED");
  sendEvent(event);
}

void Telescope::sync(double ra_target, double dec_target) {
  // Validate input
  if (ra_target < 0 || ra_target >= 24) {
    logWarning("Invalid RA value: " + std::to_string(ra_target), deviceId);
    return;
  }

  if (dec_target < -90 || dec_target > 90) {
    logWarning("Invalid DEC value: " + std::to_string(dec_target), deviceId);
    return;
  }

  // Set the current position to the target (sync)
  ra = ra_target;
  dec = dec_target;
  updateAltAz();

  setProperty("ra", ra);
  setProperty("dec", dec);
  setProperty("altitude", altitude);
  setProperty("azimuth", azimuth);

  logInfo("Synced to RA: " + std::to_string(ra) +
              ", DEC: " + std::to_string(dec),
          deviceId);

  // Send sync event
  EventMessage event("SYNCED");
  event.setDetails({{"ra", ra}, {"dec", dec}});
  sendEvent(event);
}

void Telescope::updateLoop() {
  logInfo("Update loop started", deviceId);

  while (update_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (tracking && !is_parked && !is_moving) {
      // Simulate sidereal tracking (RA increases)
      ra += 0.002; // ~0.004 hours per second
      if (ra >= 24.0)
        ra -= 24.0;

      updateAltAz();

      setProperty("ra", ra);
      setProperty("dec", dec);
      setProperty("altitude", altitude);
      setProperty("azimuth", azimuth);
    }

    if (is_moving) {
      // Simulate telescope movement
      double step = 0.1 * slew_rate;

      // Move towards target position
      if (std::abs(target_ra - ra) < step) {
        ra = target_ra;
      } else {
        ra += (target_ra > ra) ? step : -step;
      }

      if (std::abs(target_dec - dec) < step) {
        dec = target_dec;
      } else {
        dec += (target_dec > dec) ? step : -step;
      }

      // Keep RA within range
      if (ra >= 24.0)
        ra -= 24.0;
      if (ra < 0.0)
        ra += 24.0;

      // Keep DEC within range
      if (dec > 90.0)
        dec = 90.0;
      if (dec < -90.0)
        dec = -90.0;

      updateAltAz();

      setProperty("ra", ra);
      setProperty("dec", dec);
      setProperty("altitude", altitude);
      setProperty("azimuth", azimuth);

      // Check if we've reached the target
      if (std::abs(target_ra - ra) < 0.01 &&
          std::abs(target_dec - dec) < 0.01) {
        is_moving = false;

        // Send GOTO completed event
        if (!current_goto_message_id.empty()) {
          sendGotoCompletedEvent(current_goto_message_id);
          current_goto_message_id.clear();
        }

        logInfo("GOTO completed at RA: " + std::to_string(ra) +
                    ", DEC: " + std::to_string(dec),
                deviceId);
      }
    }
  }

  logInfo("Update loop ended", deviceId);
}

void Telescope::updateAltAz() {
  // This is a simplified calculation for demo purposes
  // In a real implementation, you'd use proper astronomical formulas

  // Convert RA to hour angle
  double local_sidereal_time =
      (std::chrono::system_clock::now().time_since_epoch().count() / 8.64e13) *
      24.0; // Approximate LST
  double hour_angle = local_sidereal_time - ra;
  if (hour_angle < 0)
    hour_angle += 24.0;

  // Convert to radians
  double ha_rad = hour_angle * M_PI / 12.0;
  double dec_rad = dec * M_PI / 180.0;
  double lat_rad = observer_latitude * M_PI / 180.0;

  // Calculate altitude
  double sin_alt =
      sin(dec_rad) * sin(lat_rad) + cos(dec_rad) * cos(lat_rad) * cos(ha_rad);
  altitude = asin(sin_alt) * 180.0 / M_PI;

  // Calculate azimuth
  double cos_az = (sin(dec_rad) - sin(lat_rad) * sin_alt) /
                  (cos(lat_rad) * cos(asin(sin_alt)));
  azimuth = acos(cos_az) * 180.0 / M_PI;

  // Adjust azimuth based on hour angle
  if (sin(ha_rad) > 0) {
    azimuth = 360.0 - azimuth;
  }
}

void Telescope::updateRaDec() {
  // This is a simplified calculation for demo purposes
  // In a real implementation, you'd use proper astronomical formulas

  // Convert to radians
  double alt_rad = altitude * M_PI / 180.0;
  double az_rad = azimuth * M_PI / 180.0;
  double lat_rad = observer_latitude * M_PI / 180.0;

  // Calculate declination
  double sin_dec =
      sin(alt_rad) * sin(lat_rad) + cos(alt_rad) * cos(lat_rad) * cos(az_rad);
  dec = asin(sin_dec) * 180.0 / M_PI;

  // Calculate hour angle
  double cos_ha = (sin(alt_rad) - sin(lat_rad) * sin_dec) /
                  (cos(lat_rad) * cos(asin(sin_dec)));
  double hour_angle = acos(cos_ha) * 12.0 / M_PI;

  // Adjust hour angle based on azimuth
  if (sin(az_rad) > 0) {
    hour_angle = 24.0 - hour_angle;
  }

  // Convert hour angle to RA
  double local_sidereal_time =
      (std::chrono::system_clock::now().time_since_epoch().count() / 8.64e13) *
      24.0; // Approximate LST
  ra = local_sidereal_time - hour_angle;
  if (ra < 0)
    ra += 24.0;
  if (ra >= 24.0)
    ra -= 24.0;
}

void Telescope::sendGotoCompletedEvent(const std::string &relatedMessageId) {
  EventMessage event("COMMAND_COMPLETED");
  event.setRelatedMessageId(relatedMessageId);

  event.setDetails({{"command", "GOTO"},
                    {"status", "SUCCESS"},
                    {"finalRa", ra},
                    {"finalDec", dec},
                    {"alignmentAccuracy", "HIGH"}});

  sendEvent(event);
}

// Command handlers
void Telescope::handleGotoCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  json params = cmd.getParameters();

  if (is_parked) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "TELESCOPE_PARKED"},
                         {"message", "Cannot perform GOTO while parked"}});
    return;
  }

  if (!params.contains("ra") || !params.contains("dec")) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameters RA and DEC"}});
    return;
  }

  double ra_target = params["ra"];
  double dec_target = params["dec"];

  // Store the message ID for the completion event
  current_goto_message_id = cmd.getMessageId();

  // Start the GOTO
  gotoPosition(ra_target, dec_target);

  // Calculate estimated completion time based on distance and slew rate
  double distance =
      std::sqrt(std::pow(target_ra - ra, 2) + std::pow(target_dec - dec, 2));
  int estimatedSeconds = static_cast<int>(distance / (0.1 * slew_rate)) + 1;

  auto now = std::chrono::system_clock::now();
  auto completeTime = now + std::chrono::seconds(estimatedSeconds);
  auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
  std::ostringstream est_ss;
  est_ss << std::put_time(gmtime(&complete_itt), "%FT%T") << "Z";

  response.setStatus("IN_PROGRESS");
  response.setDetails(
      {{"estimatedCompletionTime", est_ss.str()}, {"progressPercentage", 0}});
}

void Telescope::handleTrackingCommand(const CommandMessage &cmd,
                                      ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("enabled")) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "INVALID_PARAMETERS"},
                         {"message", "Missing required parameter 'enabled'"}});
    return;
  }

  bool enabled = params["enabled"];

  if (is_parked && enabled) {
    response.setStatus("ERROR");
    response.setDetails({{"error", "TELESCOPE_PARKED"},
                         {"message", "Cannot enable tracking while parked"}});
    return;
  }

  setTracking(enabled);

  response.setStatus("SUCCESS");
  response.setDetails({{"tracking", enabled}});
}

void Telescope::handleParkCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  json params = cmd.getParameters();
  bool parkAction = true; // Default to park

  if (params.contains("action")) {
    std::string action = params["action"];
    if (action == "unpark") {
      parkAction = false;
    }
  }

  if (parkAction) {
    if (is_parked) {
      response.setStatus("SUCCESS");
      response.setDetails({{"message", "Telescope already parked"}});
      return;
    }

    // For the response, we'll say it started successfully
    // The actual completion will be signaled by an event
    response.setStatus("IN_PROGRESS");

    // Start parking in a separate thread to not block
    std::thread([this]() { park(); }).detach();
  } else {
    if (!is_parked) {
      response.setStatus("SUCCESS");
      response.setDetails({{"message", "Telescope already unparked"}});
      return;
    }

    unpark();
    response.setStatus("SUCCESS");
  }
}

void Telescope::handleSyncCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  json params = cmd.getParameters();

  if (!params.contains("ra") || !params.contains("dec")) {
    response.setStatus("ERROR");
    response.setDetails(
        {{"error", "INVALID_PARAMETERS"},
         {"message", "Missing required parameters RA and DEC"}});
    return;
  }

  double ra_target = params["ra"];
  double dec_target = params["dec"];

  sync(ra_target, dec_target);

  response.setStatus("SUCCESS");
  response.setDetails({{"ra", ra}, {"dec", dec}});
}

void Telescope::handleAbortCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  if (!is_moving) {
    response.setStatus("SUCCESS");
    response.setDetails({{"message", "No movement to abort"}});
    return;
  }

  abort();

  response.setStatus("SUCCESS");
  response.setDetails({{"message", "Movement aborted"}});
}

} // namespace astrocomm