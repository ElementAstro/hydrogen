#pragma once
#include "device/device_base.h"
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>

namespace astrocomm {

/**
 * Telescope device base class
 * Provides core functionality for controlling astronomical telescopes
 */
class Telescope : public DeviceBase {
public:
  Telescope(const std::string &deviceId,
            const std::string &manufacturer = "Celestron",
            const std::string &model = "NexStar 8SE");
  virtual ~Telescope();

  // Override base DeviceBase methods
  virtual bool start() override;
  virtual void stop() override;

  // Telescope specific methods - all virtual for customization
  virtual void gotoPosition(double ra, double dec);
  virtual void setTracking(bool enabled);
  virtual void setSlewRate(int rate);
  virtual void abort();
  virtual void park();
  virtual void unpark();
  virtual void sync(double ra, double dec);
  
  // Position setters and getters
  virtual void setObserverLocation(double latitude, double longitude);
  virtual std::pair<double, double> getPosition() const;
  virtual std::pair<double, double> getAltAz() const;
  virtual bool isTracking() const;
  virtual bool isParked() const;
  virtual bool isMoving() const;

protected:
  // Simulation update thread 
  virtual void updateLoop();
  virtual double getCurrentLST() const;

  // Coordinate conversion methods 
  virtual void updateAltAz();
  virtual void updateRaDec();
  
  // Event notification methods
  virtual void sendGotoCompletedEvent(const std::string &relatedMessageId);
  virtual void sendTrackingChangedEvent(bool enabled);
  virtual void sendPositionChangedEvent();

  // Command handlers - can be overridden by derived classes
  virtual void handleGotoCommand(const CommandMessage &cmd, ResponseMessage &response);
  virtual void handleTrackingCommand(const CommandMessage &cmd, ResponseMessage &response);
  virtual void handleParkCommand(const CommandMessage &cmd, ResponseMessage &response);
  virtual void handleSyncCommand(const CommandMessage &cmd, ResponseMessage &response);
  virtual void handleAbortCommand(const CommandMessage &cmd, ResponseMessage &response);
  
  // Hook methods for derived classes
  virtual void onGotoStart(double targetRa, double targetDec);
  virtual void onGotoComplete();
  virtual void onTrackingChanged(bool enabled);
  virtual void onParked();
  virtual void onUnparked();
  virtual void onSynced(double ra, double dec);
  virtual void onAborted();
  
  // Protected member variables for derived classes
  double ra;       // Right ascension (hours, 0-24)
  double dec;      // Declination (degrees, -90 to +90)
  double altitude; // Altitude (degrees, 0-90)
  double azimuth;  // Azimuth (degrees, 0-360)
  int slew_rate;   // Slew rate (1-10)
  bool tracking;   // Tracking state
  bool is_parked;  // Parking state

  // GOTO related state
  std::atomic<bool> is_moving;
  double target_ra;
  double target_dec;
  std::string current_goto_message_id;

  // Observer's location (for coordinate conversion)
  double observer_latitude;
  double observer_longitude;
  
  // Thread management
  std::thread update_thread;
  std::atomic<bool> update_running;
  
  // Mutex for thread safety
  mutable std::mutex position_mutex;
};

} // namespace astrocomm