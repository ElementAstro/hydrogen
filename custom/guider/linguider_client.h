#pragma once

#include "device/guider.h"

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

namespace astrocomm {

using boost::asio::ip::tcp;

class LinGuiderInterface : public GuiderInterface {
public:
  LinGuiderInterface();
  ~LinGuiderInterface() override;

  // GuiderInterface implementation
  bool connect(const std::string &host, int port) override;
  void disconnect() override;
  bool isConnected() const override;

  bool startGuiding() override;
  bool stopGuiding() override;
  bool pauseGuiding() override;
  bool resumeGuiding() override;

  bool startCalibration() override;
  bool cancelCalibration() override;
  bool dither(double amount, double settleTime, double settlePixels) override;

  GuiderState getGuiderState() const override;
  CalibrationState getCalibrationState() const override;
  GuiderStats getStats() const override;
  StarInfo getGuideStar() const override;
  CalibrationData getCalibrationData() const override;
  GuidingCorrection getCurrentCorrection() const override;

  void setPixelScale(double scaleArcsecPerPixel) override;
  void setGuideRate(double raRateMultiplier, double decRateMultiplier) override;

  GuiderInterfaceType getInterfaceType() const override;
  std::string getInterfaceName() const override;

  void update() override;

private:
  // TCP client wrapper class
  class TcpClient;

  // Internal helper methods
  void sendCommand(const std::string &command);
  void receiveLoop();
  void processMessage(const std::string &message);
  std::vector<std::string> splitString(const std::string &str, char delimiter);

  // Connection state
  bool connected;
  GuiderState state;
  CalibrationState calState;
  std::string host;
  int port;

  // TCP client and thread
  std::shared_ptr<TcpClient> tcpClient;
  std::thread receiveThread;
  bool isRunning;

  // Guiding data
  StarInfo guideStar;
  GuidingCorrection lastCorrection;
  CalibrationData calibration;
  GuiderStats stats;

  // Thread safety
  mutable std::mutex stateMutex;
};

} // namespace astrocomm