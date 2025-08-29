#include "device/telescope.h"
#include <spdlog/spdlog.h> // Use spdlog for logging
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cmath> // For M_PI, sin, cos, asin, acos, abs, sqrt, pow

// Define M_PI if not already defined (might be needed on some platforms)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace astrocomm {

Telescope::Telescope(const std::string &deviceId,
                     const std::string &manufacturer, const std::string &model)
    : DeviceBase(deviceId, "TELESCOPE", manufacturer, model), ra(0.0), dec(0.0),
      altitude(45.0), azimuth(180.0), slew_rate(3), tracking(false),
      is_parked(false), is_moving(false), target_ra(0.0), target_dec(0.0),
      update_running(false), observer_latitude(40.0),
      observer_longitude(-74.0) {

  try {
    // Initialize properties
    setProperty("ra", ra);
    setProperty("dec", dec);
    setProperty("altitude", altitude);
    setProperty("azimuth", azimuth);
    setProperty("slew_rate", slew_rate);
    setProperty("tracking", tracking);
    setProperty("parked", is_parked);
    setProperty("connected", false);
    setProperty("latitude", observer_latitude);
    setProperty("longitude", observer_longitude);

    // Set capabilities
    capabilities = {"GOTO", "TRACKING", "ALIGNMENT", "PARKING", "SYNC", "ABORT"};

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

    spdlog::info("Telescope device initialized: {}", deviceId);
  } catch (const std::exception &e) {
    spdlog::error("Failed to initialize telescope {}: {}", deviceId, e.what());
    throw; // Re-throw the exception after logging
  }
}

Telescope::~Telescope() {
  try {
    stop();
  } catch (const std::exception &e) {
    // Log error but don't throw from destructor
    spdlog::error("Error during telescope {} destruction: {}", deviceId, e.what());
  } catch (...) {
    spdlog::error("Unknown error during telescope {} destruction", deviceId);
  }
}

bool Telescope::start() {
  try {
    if (!DeviceBase::start()) {
      spdlog::warn("DeviceBase::start() failed for telescope {}", deviceId);
      return false;
    }

    // Start the update thread
    if (!update_running) {
        update_running = true;
        update_thread = std::thread(&Telescope::updateLoop, this);
        spdlog::info("Update thread started for telescope {}", deviceId);
    } else {
        spdlog::warn("Update thread already running for telescope {}", deviceId);
    }


    setProperty("connected", true);
    spdlog::info("Telescope started: {}", deviceId);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Failed to start telescope {}: {}", deviceId, e.what());
    update_running = false; // Ensure flag is reset on error
    setProperty("connected", false);
    // Attempt to join thread if it was somehow started before exception
    if (update_thread.joinable()) {
        try {
            update_thread.join();
        } catch (const std::system_error& se) {
            spdlog::error("System error joining update thread during start failure for {}: {}", deviceId, se.what());
        }
    }
    return false;
  }
}

void Telescope::stop() {
  try {
    // Stop the update thread first
    if (update_running) {
        update_running = false;
        if (update_thread.joinable()) {
            update_thread.join();
            spdlog::info("Update thread joined for telescope {}", deviceId);
        } else {
             spdlog::warn("Update thread was not joinable for telescope {}", deviceId);
        }
    } else {
        spdlog::info("Update thread already stopped for telescope {}", deviceId);
    }


    setProperty("connected", false);
    DeviceBase::stop(); // Call base class stop
    spdlog::info("Telescope stopped: {}", deviceId);
  } catch (const std::exception &e) {
    spdlog::error("Error stopping telescope {}: {}", deviceId, e.what());
    // Ensure connected status reflects reality even on error
    setProperty("connected", false);
  } catch (...) {
     spdlog::error("Unknown error stopping telescope {}", deviceId);
     setProperty("connected", false);
  }
}

void Telescope::gotoPosition(double ra_target, double dec_target) {
  try {
    std::lock_guard<std::mutex> lock(position_mutex);

    if (is_parked) {
      spdlog::warn("Cannot GOTO: telescope {} is parked", deviceId);
      throw std::runtime_error("Telescope is parked"); // Throw exception for clarity
    }

    // Validate input
    if (ra_target < 0.0 || ra_target >= 24.0) {
      spdlog::warn("Invalid RA value for GOTO: {} (Device: {})", ra_target, deviceId);
      throw std::invalid_argument("RA value must be between 0 and 24");
    }

    if (dec_target < -90.0 || dec_target > 90.0) {
      spdlog::warn("Invalid DEC value for GOTO: {} (Device: {})", dec_target, deviceId);
      throw std::invalid_argument("DEC value must be between -90 and +90");
    }

    this->target_ra = ra_target;
    this->target_dec = dec_target;
    is_moving = true; // Set atomic flag

    spdlog::info("Starting GOTO for {}: RA={}, DEC={}", deviceId, ra_target, dec_target);

    // Call the hook for derived classes
    onGotoStart(ra_target, dec_target);

  } catch (const std::invalid_argument& iae) {
      spdlog::error("Invalid argument in gotoPosition for {}: {}", deviceId, iae.what());
      throw; // Re-throw validation errors
  } catch (const std::runtime_error& rte) {
      spdlog::error("Runtime error in gotoPosition for {}: {}", deviceId, rte.what());
      throw; // Re-throw runtime errors (like parked state)
  } catch (const std::exception &e) {
    is_moving = false; // Ensure moving flag is reset on unexpected error
    spdlog::error("Error in gotoPosition for {}: {}", deviceId, e.what());
    throw; // Re-throw other exceptions
  }
}

void Telescope::setTracking(bool enabled) {
  try {
    std::lock_guard<std::mutex> lock(position_mutex);

    if (is_parked && enabled) {
      spdlog::warn("Cannot enable tracking: telescope {} is parked", deviceId);
      // Optionally throw an exception or just return
      // throw std::runtime_error("Cannot enable tracking while parked");
      return;
    }

    if (tracking != enabled) {
      tracking = enabled;
      setProperty("tracking", tracking);

      spdlog::info("Tracking {} for {}", (enabled ? "enabled" : "disabled"), deviceId);

      // Call hooks and send events
      onTrackingChanged(enabled);
      sendTrackingChangedEvent(enabled);
    } else {
       spdlog::debug("Tracking already {} for {}", (enabled ? "enabled" : "disabled"), deviceId);
    }
  } catch (const std::exception &e) {
    spdlog::error("Error in setTracking for {}: {}", deviceId, e.what());
    throw; // Re-throw exception
  }
}

void Telescope::setSlewRate(int rate) {
  try {
    // No mutex needed if slew_rate is atomic or only read in update loop
    // std::lock_guard<std::mutex> lock(position_mutex); // Add if needed

    if (rate < 1 || rate > 10) { // Assuming 1-10 is a valid range
      spdlog::warn("Invalid slew rate: {} (Device: {})", rate, deviceId);
      throw std::invalid_argument("Slew rate must be between 1 and 10");
    }

    slew_rate = rate;
    setProperty("slew_rate", slew_rate);

    spdlog::info("Slew rate set to {} for {}", rate, deviceId);
  } catch (const std::invalid_argument& iae) {
      spdlog::error("Invalid argument in setSlewRate for {}: {}", deviceId, iae.what());
      throw; // Re-throw validation errors
  } catch (const std::exception &e) {
    spdlog::error("Error in setSlewRate for {}: {}", deviceId, e.what());
    throw; // Re-throw other exceptions
  }
}

void Telescope::abort() {
  try {
    // Use a lock if is_moving interacts with position updates requiring mutex
    // std::lock_guard<std::mutex> lock(position_mutex);

    if (!is_moving.load()) { // Check atomic flag
      spdlog::info("Abort called for {} but telescope is not moving.", deviceId);
      return; // Nothing to abort
    }

    is_moving = false; // Set atomic flag to stop movement simulation/hardware command
    current_goto_message_id.clear(); // Clear associated command ID

    spdlog::info("Movement aborted for {}", deviceId);

    // Send an event
    EventMessage event("ABORTED");
    event.setDeviceId(deviceId); // Ensure device ID is set in event
    sendEvent(event);

    // Call the hook for derived classes
    onAborted();
  } catch (const std::exception &e) {
    spdlog::error("Error in abort for {}: {}", deviceId, e.what());
    throw; // Re-throw exception
  }
}

void Telescope::park() {
    spdlog::info("Park command received for {}", deviceId);
    try {
        double park_target_ra = 0.0; // Default park position (North Celestial Pole for Northern Hemisphere)
        double park_target_dec = 90.0;

        { // Scope for initial lock
            std::lock_guard<std::mutex> lock(position_mutex);
            if (is_parked) {
                spdlog::info("Telescope {} already parked", deviceId);
                return; // Already parked
            }

            // Determine park position based on latitude
            if (observer_latitude < 0) {
                park_target_dec = -90.0; // South Celestial Pole for Southern Hemisphere
            }

            // Save current position before parking (optional, but good practice)
            setProperty("park_ra", ra);
            setProperty("park_dec", dec);
            spdlog::info("Saving pre-park position for {}: RA={}, DEC={}", deviceId, ra, dec);

            // Disable tracking immediately
            if (tracking) {
                setTracking(false); // This will log and send events
            }
        } // Release lock before potentially long GOTO operation

        spdlog::info("Initiating GOTO to park position for {}: RA={}, DEC={}", deviceId, park_target_ra, park_target_dec);
        gotoPosition(park_target_ra, park_target_dec); // This might throw

        // Wait for the GOTO to complete. This is blocking.
        // Consider making park asynchronous if needed.
        while (is_moving.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        spdlog::info("GOTO to park position completed for {}", deviceId);


        // Re-acquire lock to finalize parked state
        std::lock_guard<std::mutex> lock(position_mutex);

        is_parked = true;
        setProperty("parked", true);

        spdlog::info("Telescope {} successfully parked", deviceId);

        // Send parked event
        EventMessage event("PARKED");
        event.setDeviceId(deviceId);
        sendEvent(event);

        // Call the hook for derived classes
        onParked();

    } catch (const std::exception &e) {
        spdlog::error("Error during park operation for {}: {}", deviceId, e.what());
        // Attempt to revert state if possible, though might be inconsistent
        {
             std::lock_guard<std::mutex> lock(position_mutex);
             is_parked = false; // Assume park failed
             setProperty("parked", false);
        }
        throw; // Re-throw the exception
    }
}


void Telescope::unpark() {
    spdlog::info("Unpark command received for {}", deviceId);
    try {
        double restore_ra = -1.0; // Use invalid values initially
        double restore_dec = -100.0;
        bool should_restore = false;

        { // Scope for lock
            std::lock_guard<std::mutex> lock(position_mutex);

            if (!is_parked) {
                spdlog::info("Telescope {} not parked, unpark command ignored", deviceId);
                return; // Not parked
            }

            is_parked = false; // Mark as unparked immediately
            setProperty("parked", false);

            // Check if there's a saved position to restore
            json park_ra_json = getProperty("park_ra");
            json park_dec_json = getProperty("park_dec");

            if (!park_ra_json.is_null() && park_ra_json.is_number() &&
                !park_dec_json.is_null() && park_dec_json.is_number())
            {
                restore_ra = park_ra_json.get<double>();
                restore_dec = park_dec_json.get<double>();
                // Basic validation of restored coordinates
                if (restore_ra >= 0.0 && restore_ra < 24.0 && restore_dec >= -90.0 && restore_dec <= 90.0) {
                    should_restore = true;
                    spdlog::info("Found saved position for {}: RA={}, DEC={}. Attempting restore.", deviceId, restore_ra, restore_dec);
                } else {
                    spdlog::warn("Invalid saved park position for {}: RA={}, DEC={}. Skipping restore.", deviceId, restore_ra, restore_dec);
                    should_restore = false; // Don't restore invalid coords
                }
            } else {
                 spdlog::info("No valid saved park position found for {}. Telescope will remain at park coordinates.", deviceId);
            }
        } // Release lock before potential GOTO

        // If a valid position was found, initiate GOTO
        if (should_restore) {
            try {
                gotoPosition(restore_ra, restore_dec);
                // Optionally wait for GOTO completion here if unpark should be synchronous
                // while (is_moving.load()) {
                //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // }
                // spdlog::info("Restore GOTO completed for {}", deviceId);
            } catch (const std::exception& goto_e) {
                spdlog::error("Error during restore GOTO after unpark for {}: {}", deviceId, goto_e.what());
                // Decide how to handle GOTO failure during unpark. Maybe log and continue?
            }
        }

        spdlog::info("Telescope {} unparked", deviceId);

        // Send unparked event
        EventMessage event("UNPARKED");
        event.setDeviceId(deviceId);
        sendEvent(event);

        // Call the hook for derived classes
        onUnparked();

    } catch (const std::exception &e) {
        spdlog::error("Error during unpark operation for {}: {}", deviceId, e.what());
        // Attempt to revert state if possible
         {
             std::lock_guard<std::mutex> lock(position_mutex);
             is_parked = true; // Assume unpark failed
             setProperty("parked", true);
         }
        throw; // Re-throw the exception
    }
}


void Telescope::sync(double ra_target, double dec_target) {
  try {
    std::lock_guard<std::mutex> lock(position_mutex);

    // Validate input
    if (ra_target < 0.0 || ra_target >= 24.0) {
      spdlog::warn("Invalid RA value for SYNC: {} (Device: {})", ra_target, deviceId);
      throw std::invalid_argument("RA value must be between 0 and 24");
    }

    if (dec_target < -90.0 || dec_target > 90.0) {
      spdlog::warn("Invalid DEC value for SYNC: {} (Device: {})", dec_target, deviceId);
      throw std::invalid_argument("DEC value must be between -90 and +90");
    }

    // Set the current position to the target (sync)
    ra = ra_target;
    dec = dec_target;
    updateAltAz(); // Recalculate Alt/Az based on new RA/Dec

    setProperty("ra", ra);
    setProperty("dec", dec);
    setProperty("altitude", altitude);
    setProperty("azimuth", azimuth);

    spdlog::info("Synced {} to RA={}, DEC={}", deviceId, ra, dec);

    // Send sync event
    EventMessage event("SYNCED");
    event.setDeviceId(deviceId);
    event.setDetails({{"ra", ra}, {"dec", dec}});
    sendEvent(event);

    // Call the hook for derived classes
    onSynced(ra, dec);
  } catch (const std::invalid_argument& iae) {
      spdlog::error("Invalid argument in sync for {}: {}", deviceId, iae.what());
      throw; // Re-throw validation errors
  } catch (const std::exception &e) {
    spdlog::error("Error in sync for {}: {}", deviceId, e.what());
    throw; // Re-throw other exceptions
  }
}

void Telescope::setObserverLocation(double latitude, double longitude) {
  try {
    // Validate input
    if (latitude < -90.0 || latitude > 90.0) {
      throw std::invalid_argument("Latitude must be between -90 and +90 degrees");
    }
    if (longitude < -180.0 || longitude > 180.0) { // Corrected range check
      throw std::invalid_argument("Longitude must be between -180 and +180 degrees");
    }

    std::lock_guard<std::mutex> lock(position_mutex);
    observer_latitude = latitude;
    observer_longitude = longitude;

    // Update alt-az coordinates based on new location and current RA/Dec
    updateAltAz();

    setProperty("latitude", latitude);
    setProperty("longitude", longitude);
    // Also update altitude/azimuth properties as they depend on location
    setProperty("altitude", altitude);
    setProperty("azimuth", azimuth);


    spdlog::info("Observer location set for {}: Lat={}, Lon={}", deviceId, latitude, longitude);
  } catch (const std::invalid_argument& iae) {
      spdlog::error("Invalid argument in setObserverLocation for {}: {}", deviceId, iae.what());
      throw; // Re-throw validation errors
  } catch (const std::exception &e) {
    spdlog::error("Error in setObserverLocation for {}: {}", deviceId, e.what());
    throw; // Re-throw other exceptions
  }
}

std::pair<double, double> Telescope::getPosition() const {
  std::lock_guard<std::mutex> lock(position_mutex);
  return std::make_pair(ra, dec);
}

std::pair<double, double> Telescope::getAltAz() const {
  std::lock_guard<std::mutex> lock(position_mutex);
  return std::make_pair(altitude, azimuth);
}

bool Telescope::isTracking() const {
  std::lock_guard<std::mutex> lock(position_mutex);
  return tracking;
}

bool Telescope::isParked() const {
  std::lock_guard<std::mutex> lock(position_mutex);
  return is_parked;
}

bool Telescope::isMoving() const {
  return is_moving.load(); // Read atomic flag
}

void Telescope::updateLoop() {
  spdlog::info("Update loop started for {}", deviceId);

  const auto update_interval = std::chrono::milliseconds(500); // 2 Hz update rate
  const double sidereal_rate_hz = 15.0 / 3600.0; // degrees per second
  const double sidereal_rate_ra_hours = sidereal_rate_hz / 15.0; // RA hours per second
  const double update_step_ra = sidereal_rate_ra_hours * (update_interval.count() / 1000.0);

  while (update_running) {
    try {
      auto loop_start_time = std::chrono::steady_clock::now();
      bool position_changed = false;
      bool moving_this_cycle = is_moving.load(); // Cache atomic value for this cycle

      { // Scope for lock
        std::lock_guard<std::mutex> lock(position_mutex);

        // 1. Apply Sidereal Tracking (if enabled and not parked/moving)
        if (tracking && !is_parked && !moving_this_cycle) {
          ra += update_step_ra;
          // Handle RA wrap-around
          if (ra >= 24.0) {
            ra -= 24.0;
          } else if (ra < 0.0) { // Should not happen with positive rate, but good practice
            ra += 24.0;
          }
          position_changed = true;
        }

        // 2. Simulate GOTO Movement (if active)
        if (moving_this_cycle) {
          // Simple linear interpolation towards target
          // More realistic simulation would involve acceleration/deceleration
          // Slew rate unit needs definition (e.g., degrees/sec). Assume 1 unit = 1 deg/sec for now.
          double slew_speed_deg_per_sec = static_cast<double>(slew_rate); // Example: slew_rate 3 -> 3 deg/sec
          double max_step_deg = slew_speed_deg_per_sec * (update_interval.count() / 1000.0);

          // Calculate angular distance (approximation on sphere, good for small steps)
          // Convert RA difference to degrees (1 hour = 15 degrees)
          double delta_ra_deg = (target_ra - ra) * 15.0;
          // Handle RA wrap-around for distance calculation
          if (delta_ra_deg > 180.0) delta_ra_deg -= 360.0;
          if (delta_ra_deg < -180.0) delta_ra_deg += 360.0;

          double delta_dec_deg = target_dec - dec;
          double distance_deg = std::sqrt(delta_ra_deg * delta_ra_deg + delta_dec_deg * delta_dec_deg);

          if (distance_deg <= max_step_deg) {
            // Reached target (or very close)
            ra = target_ra;
            dec = target_dec;
            is_moving = false; // Stop moving
            moving_this_cycle = false; // Update local flag for this cycle
            position_changed = true;

            spdlog::info("GOTO completed for {}: RA={}, DEC={}", deviceId, ra, dec);

            // Send GOTO completed event
            if (!current_goto_message_id.empty()) {
              sendGotoCompletedEvent(current_goto_message_id);
              current_goto_message_id.clear();
            }
            // Call the hook for derived classes
            onGotoComplete();

          } else {
            // Move towards target
            double move_fraction = max_step_deg / distance_deg;
            ra += (delta_ra_deg / 15.0) * move_fraction; // Move RA (in hours)
            dec += delta_dec_deg * move_fraction;       // Move Dec (in degrees)
            position_changed = true;

            // Handle RA wrap-around after move
            if (ra >= 24.0) ra -= 24.0;
            if (ra < 0.0) ra += 24.0;

            // Clamp DEC (shouldn't exceed limits if target is valid, but safety check)
            dec = std::max(-90.0, std::min(90.0, dec));
          }
        }

        // 3. Update Altitude/Azimuth if RA/Dec changed
        if (position_changed) {
          updateAltAz(); // Update internal alt/az values
          // Update properties for external access
          setProperty("ra", ra);
          setProperty("dec", dec);
          setProperty("altitude", altitude);
          setProperty("azimuth", azimuth);
          // Send position changed event (consider rate-limiting this if needed)
          sendPositionChangedEvent();
        }
      } // Release lock

      // Calculate time spent and sleep accordingly
      auto loop_end_time = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(loop_end_time - loop_start_time);
      auto wait_time = update_interval - elapsed;

      if (wait_time > std::chrono::milliseconds::zero()) {
        std::this_thread::sleep_for(wait_time);
      } else if (elapsed > update_interval) {
          spdlog::warn("Update loop for {} took longer than interval: {}ms", deviceId, elapsed.count());
      }

    } catch (const std::exception &e) {
      spdlog::error("Error in update loop for {}: {}", deviceId, e.what());
      // Avoid tight error loop by sleeping
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (...) {
       spdlog::error("Unknown error in update loop for {}", deviceId);
       std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  spdlog::info("Update loop ended for {}", deviceId);
}


// Helper function to get current Local Sidereal Time (LST) - Simplified
double Telescope::getCurrentLST() const {
    // VERY simplified LST calculation. Replace with a proper astronomical library (e.g., SOFA, NOVAS)
    // This approximation is only vaguely correct and doesn't account for date, UT1-UTC, etc.
    auto now = std::chrono::system_clock::now();
    time_t now_tt = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *gmtime(&now_tt); // Use gmtime for UTC

    double hours_utc = now_tm.tm_hour + now_tm.tm_min / 60.0 + now_tm.tm_sec / 3600.0;

    // Rough approximation of Greenwich Mean Sidereal Time (GMST)
    // This needs day of year, Julian date etc. for accuracy.
    // Let's pretend GMST = UTC for this simplification (highly inaccurate!)
    double gmst = hours_utc;

    // Local Sidereal Time = GMST + Longitude
    double lst = gmst + observer_longitude / 15.0; // Longitude in hours (15 deg = 1 hour)

    // Normalize LST to 0-24 hours
    lst = fmod(lst, 24.0);
    if (lst < 0) {
        lst += 24.0;
    }
    return lst;
}


void Telescope::updateAltAz() {
  // Requires position_mutex to be locked by caller if accessing shared members like ra, dec, observer_latitude etc.
  // Use proper astronomical formulas for coordinate conversion.
  // This implementation uses standard spherical trigonometry.

  double lst = getCurrentLST();
  double hour_angle = lst - ra; // Hour Angle in hours

  // Normalize Hour Angle to -12h to +12h for trigonometric functions
  // Or keep it 0-24h and adjust formulas accordingly. Let's use 0-24h.
   if (hour_angle < 0) hour_angle += 24.0;
   // if (hour_angle > 12.0) hour_angle -= 24.0; // Use this if HA needs to be -12 to +12

  // Convert angles to radians
  double ha_rad = hour_angle * (M_PI / 12.0); // HA from hours to radians
  double dec_rad = dec * (M_PI / 180.0);
  double lat_rad = observer_latitude * (M_PI / 180.0);

  // Calculate Altitude
  double sin_alt = sin(dec_rad) * sin(lat_rad) + cos(dec_rad) * cos(lat_rad) * cos(ha_rad);
  // Clamp sin_alt to [-1, 1] to avoid domain errors in asin
  sin_alt = std::max(-1.0, std::min(1.0, sin_alt));
  double alt_rad = asin(sin_alt);
  altitude = alt_rad * (180.0 / M_PI); // Altitude in degrees

  // Calculate Azimuth
  double cos_az_numerator = sin(dec_rad) - sin(lat_rad) * sin_alt;
  double cos_az_denominator = cos(lat_rad) * cos(alt_rad); // cos(alt_rad) is cos(asin(sin_alt))

  double azimuth_rad; // Azimuth in radians

  // Avoid division by zero if altitude is +/- 90 degrees (at zenith/nadir)
  if (std::abs(cos_az_denominator) < 1e-9) {
      // At zenith or nadir, azimuth is undefined or conventionally set.
      // Let's set it to 0 (North) arbitrarily.
      azimuth_rad = 0.0;
  } else {
      double cos_az = cos_az_numerator / cos_az_denominator;
      // Clamp cos_az to [-1, 1] to avoid domain errors in acos
      cos_az = std::max(-1.0, std::min(1.0, cos_az));
      azimuth_rad = acos(cos_az);

      // acos gives angle in [0, PI]. Adjust based on hour angle to get full 0-2PI range.
      // If sin(ha) is negative, the object is East of the meridian.
      if (sin(ha_rad) > 0.0) { // Check if HA > 0 and < 12h (West) vs > 12h (East)
          azimuth_rad = 2.0 * M_PI - azimuth_rad; // Azimuth measured Eastward from North
      }
  }

  azimuth = azimuth_rad * (180.0 / M_PI); // Azimuth in degrees (0-360)
}


void Telescope::updateRaDec() {
    // Requires position_mutex to be locked by caller.
    // Converts current Altitude/Azimuth back to RA/Dec. Useful for Alt/Az mounts or syncing from Alt/Az.
    // Use proper astronomical formulas.

    double lst = getCurrentLST();

    // Convert angles to radians
    double alt_rad = altitude * (M_PI / 180.0);
    double az_rad = azimuth * (M_PI / 180.0);
    double lat_rad = observer_latitude * (M_PI / 180.0);

    // Calculate Declination
    double sin_dec = sin(alt_rad) * sin(lat_rad) + cos(alt_rad) * cos(lat_rad) * cos(az_rad);
    // Clamp sin_dec to [-1, 1]
    sin_dec = std::max(-1.0, std::min(1.0, sin_dec));
    double dec_rad = asin(sin_dec);
    dec = dec_rad * (180.0 / M_PI); // Declination in degrees

    // Calculate Hour Angle
    double cos_ha_numerator = sin(alt_rad) - sin(lat_rad) * sin_dec;
    double cos_ha_denominator = cos(lat_rad) * cos(dec_rad); // cos(dec_rad) is cos(asin(sin_dec))

    double hour_angle; // Hour Angle in hours

    // Avoid division by zero if latitude is +/- 90 or declination is +/- 90
    if (std::abs(cos_ha_denominator) < 1e-9) {
        // At celestial poles, HA is undefined or conventionally set.
        // If Dec is +/- 90, HA doesn't matter much. Let's set to 0.
        hour_angle = 0.0;
    } else {
        double cos_ha = cos_ha_numerator / cos_ha_denominator;
        // Clamp cos_ha to [-1, 1]
        cos_ha = std::max(-1.0, std::min(1.0, cos_ha));
        double ha_rad = acos(cos_ha); // HA in radians [0, PI]

        // Adjust based on Azimuth to get full 0-2PI range for HA (measured Westward)
        // If sin(az) is positive (East), HA should be > PI (or negative if using -PI to PI)
        if (sin(az_rad) > 0.0) { // Azimuth > 0 and < 180 (East)
            ha_rad = 2.0 * M_PI - ha_rad;
        }
        hour_angle = ha_rad * (12.0 / M_PI); // Hour Angle in hours [0, 24)
    }

    // Calculate Right Ascension
    ra = lst - hour_angle;

    // Normalize RA to 0-24 hours
    ra = fmod(ra, 24.0);
    if (ra < 0) {
        ra += 24.0;
    }
}


void Telescope::sendGotoCompletedEvent(const std::string &relatedMessageId) {
  EventMessage event("COMMAND_COMPLETED");
  event.setDeviceId(deviceId);
  event.setRelatedMessageId(relatedMessageId);

  // Lock needed to safely read current position
  std::lock_guard<std::mutex> lock(position_mutex);
  event.setDetails({{"command", "GOTO"},
                    {"status", "SUCCESS"},
                    {"finalRa", ra},
                    {"finalDec", dec},
                    //{"alignmentAccuracy", "HIGH"} // Accuracy might be device specific
                   });

  sendEvent(event);
  spdlog::debug("Sent GOTO_COMPLETED event for {} (related to {})", deviceId, relatedMessageId);
}

void Telescope::sendTrackingChangedEvent(bool enabled) {
  EventMessage event("TRACKING_CHANGED");
  event.setDeviceId(deviceId);
  event.setDetails({{"enabled", enabled}});
  sendEvent(event);
  spdlog::debug("Sent TRACKING_CHANGED event for {}: {}", deviceId, enabled);
}

void Telescope::sendPositionChangedEvent() {
  EventMessage event("POSITION_CHANGED");
  event.setDeviceId(deviceId);
  // No lock needed here if called from updateLoop where lock is already held
  // If called from elsewhere, acquire lock: std::lock_guard<std::mutex> lock(position_mutex);
  event.setDetails({{"ra", ra},
                    {"dec", dec},
                    {"altitude", altitude},
                    {"azimuth", azimuth}});
  sendEvent(event);
   // Avoid logging this every time, can be very verbose. Use debug level if needed.
   // spdlog::trace("Sent POSITION_CHANGED event for {}", deviceId);
}

// Command handlers
void Telescope::handleGotoCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  spdlog::debug("Handling GOTO command for {} (MsgID: {})", deviceId, cmd.getMessageId());
  try {
    json params = cmd.getParameters();

    // Check parked status (no lock needed for atomic read, but GOTO needs lock later)
    if (is_parked) { // Check internal state first
      spdlog::warn("GOTO command rejected for {}: Telescope is parked", deviceId);
      response.setStatus("ERROR");
      response.setDetails({{"error", "TELESCOPE_PARKED"},
                          {"message", "Cannot perform GOTO while parked"}});
      return;
    }

    if (!params.contains("ra") || !params["ra"].is_number() ||
        !params.contains("dec") || !params["dec"].is_number()) {
      spdlog::warn("GOTO command rejected for {}: Invalid or missing parameters (RA/DEC)", deviceId);
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_PARAMETERS"},
          {"message", "Missing or invalid required parameters: ra (number), dec (number)"}});
      return;
    }

    double ra_target = params["ra"];
    double dec_target = params["dec"];

    // Store the message ID for the completion event *before* calling gotoPosition
    // in case gotoPosition throws immediately (e.g., invalid coords)
    current_goto_message_id = cmd.getMessageId();

    // Start the GOTO (this might throw validation errors)
    gotoPosition(ra_target, dec_target); // This acquires the lock

    // If gotoPosition succeeded, respond with IN_PROGRESS
    // Calculate estimated completion time (rough estimate)
    double distance_deg = 0;
    int estimatedSeconds = 5; // Default estimate
    { // Need lock to read current position and slew rate safely
        std::lock_guard<std::mutex> lock(position_mutex);
        double delta_ra_deg = (target_ra - ra) * 15.0;
        if (delta_ra_deg > 180.0) delta_ra_deg -= 360.0;
        if (delta_ra_deg < -180.0) delta_ra_deg += 360.0;
        double delta_dec_deg = target_dec - dec;
        distance_deg = std::sqrt(delta_ra_deg * delta_ra_deg + delta_dec_deg * delta_dec_deg);

        double slew_speed_deg_per_sec = static_cast<double>(slew_rate); // Assuming 1 unit = 1 deg/sec
        if (slew_speed_deg_per_sec > 1e-6) { // Avoid division by zero
             estimatedSeconds = static_cast<int>(distance_deg / slew_speed_deg_per_sec) + 1; // Add 1 sec buffer
        }
    }


    auto now = std::chrono::system_clock::now();
    auto completeTime = now + std::chrono::seconds(estimatedSeconds);
    auto complete_itt = std::chrono::system_clock::to_time_t(completeTime);
    std::ostringstream est_ss;
    // Use std::put_time with gmtime for UTC ISO 8601 format
    est_ss << std::put_time(std::gmtime(&complete_itt), "%Y-%m-%dT%H:%M:%SZ");

    response.setStatus("IN_PROGRESS");
    response.setDetails(
        {{"estimatedCompletionTime", est_ss.str()},
         //{"progressPercentage", 0} // Progress reporting is complex, maybe omit
         });
    spdlog::info("GOTO command accepted for {}, target RA={}, DEC={}, Est. Time: {}s", deviceId, ra_target, dec_target, estimatedSeconds);

  } catch (const std::invalid_argument& iae) {
      spdlog::error("Invalid argument processing GOTO command for {}: {}", deviceId, iae.what());
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"}, {"message", iae.what()}});
      current_goto_message_id.clear(); // Clear ID if GOTO didn't even start
  } catch (const std::runtime_error& rte) { // Catch specific errors like "Telescope is parked"
       spdlog::error("Runtime error processing GOTO command for {}: {}", deviceId, rte.what());
       response.setStatus("ERROR");
       // Map specific runtime errors to protocol errors if possible
       if (std::string(rte.what()) == "Telescope is parked") {
            response.setDetails({{"error", "TELESCOPE_PARKED"}, {"message", rte.what()}});
       } else {
           response.setDetails({{"error", "COMMAND_FAILED"}, {"message", rte.what()}});
       }
       current_goto_message_id.clear(); // Clear ID if GOTO didn't start
  } catch (const std::exception &e) {
    spdlog::error("Unexpected error processing GOTO command for {}: {}", deviceId, e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"},
                         {"message", std::string("Error processing GOTO command: ") + e.what()}});
    current_goto_message_id.clear(); // Clear ID on unexpected failure
  } catch (...) {
     spdlog::error("Unknown error processing GOTO command for {}", deviceId);
     response.setStatus("ERROR");
     response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", "An unknown error occurred"}});
     current_goto_message_id.clear();
  }
}

void Telescope::handleTrackingCommand(const CommandMessage &cmd,
                                      ResponseMessage &response) {
  spdlog::debug("Handling SET_TRACKING command for {} (MsgID: {})", deviceId, cmd.getMessageId());
  try {
    json params = cmd.getParameters();

    if (!params.contains("enabled") || !params["enabled"].is_boolean()) {
      spdlog::warn("SET_TRACKING command rejected for {}: Invalid or missing 'enabled' parameter", deviceId);
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"},
                          {"message", "Missing or invalid required parameter: enabled (boolean)"}});
      return;
    }

    bool enabled = params["enabled"];

    // Check parked status before attempting to enable tracking
    if (enabled && is_parked) {
        spdlog::warn("SET_TRACKING command rejected for {}: Cannot enable tracking while parked", deviceId);
        response.setStatus("ERROR");
        response.setDetails({{"error", "TELESCOPE_PARKED"},
                             {"message", "Cannot enable tracking while parked"}});
        return;
    }

    setTracking(enabled); // This acquires lock and handles logic

    response.setStatus("SUCCESS");
    response.setDetails({{"tracking", enabled}}); // Reflect the final state
    spdlog::info("SET_TRACKING command successful for {}: Tracking {}", deviceId, (enabled ? "enabled" : "disabled"));

  } catch (const std::runtime_error& rte) { // Catch specific errors like parked state if setTracking throws it
       spdlog::error("Runtime error processing SET_TRACKING command for {}: {}", deviceId, rte.what());
       response.setStatus("ERROR");
       response.setDetails({{"error", "COMMAND_FAILED"}, {"message", rte.what()}});
  } catch (const std::exception &e) {
    spdlog::error("Unexpected error processing SET_TRACKING command for {}: {}", deviceId, e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"},
                         {"message", std::string("Error processing tracking command: ") + e.what()}});
  } catch (...) {
     spdlog::error("Unknown error processing SET_TRACKING command for {}", deviceId);
     response.setStatus("ERROR");
     response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", "An unknown error occurred"}});
  }
}

void Telescope::handleParkCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
    spdlog::debug("Handling PARK command for {} (MsgID: {})", deviceId, cmd.getMessageId());
    try {
        json params = cmd.getParameters();
        bool parkAction = true; // Default to park

        if (params.contains("action") && params["action"].is_string()) {
            std::string action = params["action"];
            if (action == "unpark") {
                parkAction = false;
            } else if (action != "park") {
                 spdlog::warn("PARK command rejected for {}: Invalid 'action' parameter: {}", deviceId, action);
                 response.setStatus("ERROR");
                 response.setDetails({{"error", "INVALID_PARAMETERS"},
                                      {"message", "Invalid 'action' parameter. Use 'park' or 'unpark'."}});
                 return;
            }
        }

        if (parkAction) {
            spdlog::info("Processing PARK action for {}", deviceId);
            // Check if already parked (read atomic flag)
            if (is_parked) {
                spdlog::info("PARK command for {}: Already parked.", deviceId);
                response.setStatus("SUCCESS"); // Or maybe "NO_ACTION"? SUCCESS is fine.
                response.setDetails({{"message", "Telescope already parked"}});
                return;
            }

            // Respond immediately that park is starting.
            // The actual park operation happens asynchronously.
            response.setStatus("IN_PROGRESS");
            response.setDetails({{"message", "Park sequence initiated"}});

            // Start parking in a separate thread to avoid blocking the command handler
            std::thread([this, cmd_id = cmd.getMessageId()]() { // Capture message ID if needed for completion event
                try {
                    park(); // This contains the blocking GOTO and state updates
                    // Optionally send a COMMAND_COMPLETED event for PARK here if needed
                    // EventMessage park_complete("COMMAND_COMPLETED");
                    // park_complete.setDeviceId(deviceId);
                    // park_complete.setRelatedMessageId(cmd_id);
                    // park_complete.setDetails({{"command", "PARK"}, {"status", "SUCCESS"}});
                    // sendEvent(park_complete);
                } catch (const std::exception &e) {
                    spdlog::error("Error during asynchronous park operation for {}: {}", deviceId, e.what());
                    // Send a failure event or log appropriately
                    // EventMessage park_failed("COMMAND_COMPLETED");
                    // park_failed.setDeviceId(deviceId);
                    // park_failed.setRelatedMessageId(cmd_id);
                    // park_failed.setDetails({{"command", "PARK"}, {"status", "ERROR"}, {"message", e.what()}});
                    // sendEvent(park_failed);
                } catch (...) {
                     spdlog::error("Unknown error during asynchronous park operation for {}", deviceId);
                }
            }).detach(); // Detach the thread to let it run independently

        } else { // Unpark action
            spdlog::info("Processing UNPARK action for {}", deviceId);
            // Check if already unparked (read atomic flag)
             if (!is_parked) {
                spdlog::info("UNPARK command for {}: Already unparked.", deviceId);
                response.setStatus("SUCCESS");
                response.setDetails({{"message", "Telescope already unparked"}});
                return;
            }

            // Unpark is currently synchronous in this implementation
            unpark(); // This might throw if errors occur
            response.setStatus("SUCCESS");
            response.setDetails({{"message", "Unpark sequence initiated or completed"}}); // Message depends if unpark involves GOTO
        }
    } catch (const std::exception &e) {
        // Catch errors from synchronous parts (like unpark or initial checks)
        spdlog::error("Error processing PARK/UNPARK command for {}: {}", deviceId, e.what());
        response.setStatus("ERROR");
        response.setDetails({{"error", "INTERNAL_ERROR"},
                             {"message", std::string("Error processing park/unpark command: ") + e.what()}});
    } catch (...) {
        spdlog::error("Unknown error processing PARK/UNPARK command for {}", deviceId);
        response.setStatus("ERROR");
        response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", "An unknown error occurred"}});
    }
}


void Telescope::handleSyncCommand(const CommandMessage &cmd,
                                  ResponseMessage &response) {
  spdlog::debug("Handling SYNC command for {} (MsgID: {})", deviceId, cmd.getMessageId());
  try {
    json params = cmd.getParameters();

    if (!params.contains("ra") || !params["ra"].is_number() ||
        !params.contains("dec") || !params["dec"].is_number()) {
      spdlog::warn("SYNC command rejected for {}: Invalid or missing parameters (RA/DEC)", deviceId);
      response.setStatus("ERROR");
      response.setDetails(
          {{"error", "INVALID_PARAMETERS"},
          {"message", "Missing or invalid required parameters: ra (number), dec (number)"}});
      return;
    }

    double ra_target = params["ra"];
    double dec_target = params["dec"];

    sync(ra_target, dec_target); // This acquires lock and performs the sync

    // Read back the synced position (should match target, but read actual state)
    double current_ra, current_dec;
    {
        std::lock_guard<std::mutex> lock(position_mutex);
        current_ra = ra;
        current_dec = dec;
    }

    response.setStatus("SUCCESS");
    response.setDetails({{"ra", current_ra}, {"dec", current_dec}});
    spdlog::info("SYNC command successful for {}: Synced to RA={}, DEC={}", deviceId, current_ra, current_dec);

  } catch (const std::invalid_argument& iae) {
      spdlog::error("Invalid argument processing SYNC command for {}: {}", deviceId, iae.what());
      response.setStatus("ERROR");
      response.setDetails({{"error", "INVALID_PARAMETERS"}, {"message", iae.what()}});
  } catch (const std::exception &e) {
    spdlog::error("Unexpected error processing SYNC command for {}: {}", deviceId, e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"},
                         {"message", std::string("Error processing sync command: ") + e.what()}});
  } catch (...) {
     spdlog::error("Unknown error processing SYNC command for {}", deviceId);
     response.setStatus("ERROR");
     response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", "An unknown error occurred"}});
  }
}

void Telescope::handleAbortCommand(const CommandMessage &cmd,
                                   ResponseMessage &response) {
  spdlog::debug("Handling ABORT command for {} (MsgID: {})", deviceId, cmd.getMessageId());
  try {
    // Check if moving (read atomic flag)
    if (!is_moving.load()) {
      spdlog::info("ABORT command for {}: No movement to abort.", deviceId);
      response.setStatus("SUCCESS"); // Aborting nothing is success
      response.setDetails({{"message", "No movement to abort"}});
      return;
    }

    abort(); // This sets the atomic flag and sends event

    response.setStatus("SUCCESS");
    response.setDetails({{"message", "Movement aborted"}});
    spdlog::info("ABORT command successful for {}", deviceId);

  } catch (const std::exception &e) {
    spdlog::error("Unexpected error processing ABORT command for {}: {}", deviceId, e.what());
    response.setStatus("ERROR");
    response.setDetails({{"error", "INTERNAL_ERROR"},
                         {"message", std::string("Error processing abort command: ") + e.what()}});
  } catch (...) {
     spdlog::error("Unknown error processing ABORT command for {}", deviceId);
     response.setStatus("ERROR");
     response.setDetails({{"error", "INTERNAL_ERROR"}, {"message", "An unknown error occurred"}});
  }
}

// Hook methods implementation (empty by default, intended for override)
// Add logging here if useful for debugging base class behavior
void Telescope::onGotoStart(double targetRa, double targetDec) {
    spdlog::debug("Hook: onGotoStart called for {} (RA={}, DEC={})", deviceId, targetRa, targetDec);
}
void Telescope::onGotoComplete() {
     spdlog::debug("Hook: onGotoComplete called for {}", deviceId);
}
void Telescope::onTrackingChanged(bool enabled) {
     spdlog::debug("Hook: onTrackingChanged called for {}: {}", deviceId, enabled);
}
void Telescope::onParked() {
     spdlog::debug("Hook: onParked called for {}", deviceId);
}
void Telescope::onUnparked() {
     spdlog::debug("Hook: onUnparked called for {}", deviceId);
}
void Telescope::onSynced(double ra, double dec) {
     spdlog::debug("Hook: onSynced called for {} (RA={}, DEC={})", deviceId, ra, dec);
}
void Telescope::onAborted() {
     spdlog::debug("Hook: onAborted called for {}", deviceId);
}

} // namespace astrocomm