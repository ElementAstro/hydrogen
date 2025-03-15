#include "linguider_client.h"

#include <array>
#include <boost/asio.hpp>
#include <sstream>
#include <thread>

namespace astrocomm {

using boost::asio::ip::tcp;

// TCP客户端封装类
class LinGuiderInterface::TcpClient {
public:
  TcpClient(std::shared_ptr<boost::asio::io_context> io_context,
            std::shared_ptr<tcp::socket> socket)
      : io_context(io_context), socket(socket) {}

  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<tcp::socket> socket;
};

// Lin-guider接口实现
LinGuiderInterface::LinGuiderInterface()
    : connected(false), state(GuiderState::DISCONNECTED),
      calState(CalibrationState::IDLE), host("localhost"), port(5656),
      tcpClient(nullptr), isRunning(false) {
  // 初始化数据结构
  guideStar = StarInfo();
  lastCorrection = GuidingCorrection();
  calibration = CalibrationData();
  stats = GuiderStats();
}

LinGuiderInterface::~LinGuiderInterface() { disconnect(); }

bool LinGuiderInterface::connect(const std::string &host, int port) {
  std::lock_guard<std::mutex> lock(stateMutex);

  if (connected) {
    SPDLOG_INFO("Already connected to Lin-guider");
    return true;
  }

  this->host = host;
  this->port = port;

  try {
    // 创建io_context
    auto io_context = std::make_shared<boost::asio::io_context>();

    // 创建TCP客户端
    auto socket = std::make_shared<tcp::socket>(*io_context);
    tcpClient = std::make_shared<TcpClient>(io_context, socket);

    // 连接到Lin-guider
    tcp::resolver resolver(*io_context);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    boost::asio::connect(*socket, endpoints);

    connected = true;
    state = GuiderState::CONNECTED;

    // 启动消息接收线程
    isRunning = true;
    receiveThread = std::thread([this]() {
      try {
        receiveLoop();
      } catch (const std::exception &e) {
        SPDLOG_ERROR("LinGuider receive error: {}", e.what());
        std::lock_guard<std::mutex> lock(stateMutex);
        connected = false;
        state = GuiderState::DISCONNECTED;
      }
    });

    SPDLOG_INFO("Connected to Lin-guider at {}:{}", host, port);

    // 发送状态查询命令
    sendCommand("get_status");

    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Lin-guider connection error: {}", e.what());
    connected = false;
    state = GuiderState::DISCONNECTED;
    tcpClient.reset();
    return false;
  }
}

void LinGuiderInterface::disconnect() {
  std::lock_guard<std::mutex> lock(stateMutex);

  if (!connected) {
    return;
  }

  isRunning = false;

  // 关闭TCP连接
  try {
    if (tcpClient) {
      sendCommand("stop"); // 停止导星
      // TCP客户端封装类

      tcpClient.reset();
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error disconnecting from Lin-guider: {}", e.what());
  }

  // 等待接收线程结束
  if (receiveThread.joinable()) {
    receiveThread.join();
  }

  connected = false;
  state = GuiderState::DISCONNECTED;

  SPDLOG_INFO("Disconnected from Lin-guider");
}

bool LinGuiderInterface::isConnected() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return connected;
}

bool LinGuiderInterface::startGuiding() {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot start guiding: not connected to Lin-guider");
    return false;
  }

  try {
    // Lin-guider使用"guide"命令开始导星
    sendCommand("guide");
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error starting guiding: {}", e.what());
    return false;
  }
}

bool LinGuiderInterface::stopGuiding() {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot stop guiding: not connected to Lin-guider");
    return false;
  }

  try {
    // Lin-guider使用"stop"命令停止导星
    sendCommand("stop");
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error stopping guiding: {}", e.what());
    return false;
  }
}

bool LinGuiderInterface::pauseGuiding() {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot pause guiding: not connected to Lin-guider");
    return false;
  }

  try {
    // Lin-guider使用"pause"命令暂停导星
    sendCommand("pause");
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error pausing guiding: {}", e.what());
    return false;
  }
}

bool LinGuiderInterface::resumeGuiding() {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot resume guiding: not connected to Lin-guider");
    return false;
  }

  try {
    // Lin-guider使用"resume"命令恢复导星
    sendCommand("resume");
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error resuming guiding: {}", e.what());
    return false;
  }
}

bool LinGuiderInterface::startCalibration() {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot start calibration: not connected to Lin-guider");
    return false;
  }

  try {
    // Lin-guider使用"calibrate"命令开始校准
    sendCommand("calibrate");
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error starting calibration: {}", e.what());
    return false;
  }
}

bool LinGuiderInterface::cancelCalibration() {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot cancel calibration: not connected to Lin-guider");
    return false;
  }

  try {
    // Lin-guider使用"stop"命令停止校准
    sendCommand("stop");
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error canceling calibration: {}", e.what());
    return false;
  }
}

bool LinGuiderInterface::dither(double amount, double settleTime,
                                double settlePixels) {
  if (!isConnected() || state != GuiderState::GUIDING) {
    SPDLOG_WARN("Cannot dither: not guiding");
    return false;
  }

  try {
    // Lin-guider的dither命令: dither <amount> <settle_pixels>
    std::stringstream ss;
    ss << "dither " << amount << " " << settlePixels;
    sendCommand(ss.str());
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error during dither: {}", e.what());
    return false;
  }
}

GuiderState LinGuiderInterface::getGuiderState() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return state;
}

CalibrationState LinGuiderInterface::getCalibrationState() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return calState;
}

GuiderStats LinGuiderInterface::getStats() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return stats;
}

StarInfo LinGuiderInterface::getGuideStar() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return guideStar;
}

CalibrationData LinGuiderInterface::getCalibrationData() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return calibration;
}

void LinGuiderInterface::setPixelScale(double scaleArcsecPerPixel) {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot set pixel scale: not connected");
    return;
  }

  try {
    // Lin-guider使用set_pixel_scale命令
    std::stringstream ss;
    ss << "set_pixel_scale " << scaleArcsecPerPixel;
    sendCommand(ss.str());

    SPDLOG_INFO("Pixel scale set to {:.2f} arcsec/pixel", scaleArcsecPerPixel);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error setting pixel scale: {}", e.what());
  }
}

void LinGuiderInterface::setGuideRate(double raRateMultiplier,
                                      double decRateMultiplier) {
  if (!isConnected()) {
    SPDLOG_WARN("Cannot set guide rate: not connected");
    return;
  }

  try {
    // Lin-guider使用set_guide_rate命令
    std::stringstream ss;
    ss << "set_guide_rate " << raRateMultiplier << " " << decRateMultiplier;
    sendCommand(ss.str());

    SPDLOG_INFO("Guide rates set to RA: {:.2f}, Dec: {:.2f}", raRateMultiplier,
                decRateMultiplier);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error setting guide rate: {}", e.what());
  }
}

GuidingCorrection LinGuiderInterface::getCurrentCorrection() const {
  std::lock_guard<std::mutex> lock(stateMutex);
  return lastCorrection;
}

GuiderInterfaceType LinGuiderInterface::getInterfaceType() const {
  return GuiderInterfaceType::LINGUIDER;
}

std::string LinGuiderInterface::getInterfaceName() const {
  return "Lin-guider";
}

void LinGuiderInterface::update() {
  if (isConnected()) {
    try {
      // 周期性发送状态查询
      static int counter = 0;
      if (++counter % 10 == 0) {
        sendCommand("get_status");
      }
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Error in update: {}", e.what());
    }
  }
}

// 私有方法

void LinGuiderInterface::sendCommand(const std::string &command) {
  if (!tcpClient || !tcpClient->socket->is_open()) {
    throw std::runtime_error("TCP client not initialized or socket not open");
  }

  // 添加命令结束符
  std::string fullCommand = command + "\n";

  boost::asio::write(*tcpClient->socket, boost::asio::buffer(fullCommand));
  SPDLOG_DEBUG("Sent to Lin-guider: {}", command);
}

void LinGuiderInterface::receiveLoop() {
  if (!tcpClient || !tcpClient->socket) {
    SPDLOG_ERROR("TCP client not initialized");
    return;
  }

  std::array<char, 4096> buffer;
  std::string incomplete_message;

  while (isRunning && tcpClient->socket->is_open()) {
    try {
      // 读取数据
      size_t length = tcpClient->socket->read_some(boost::asio::buffer(buffer));

      if (length > 0) {
        // 将收到的数据添加到不完整消息
        incomplete_message.append(buffer.data(), length);

        // 处理所有完整消息
        size_t pos;
        while ((pos = incomplete_message.find('\n')) != std::string::npos) {
          // 提取一条完整消息
          std::string message = incomplete_message.substr(0, pos);
          incomplete_message.erase(0, pos + 1);

          // 处理消息
          processMessage(message);
        }
      }

      // 小延迟，避免CPU占用过高
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } catch (const boost::system::system_error &e) {
      if (e.code() == boost::asio::error::eof ||
          e.code() == boost::asio::error::connection_reset) {
        // 连接关闭
        SPDLOG_WARN("Lin-guider connection closed");
        break;
      } else {
        SPDLOG_ERROR("Error reading from Lin-guider: {}", e.what());
        break;
      }
    } catch (const std::exception &e) {
      SPDLOG_ERROR("Error in receive loop: {}", e.what());
      break;
    }
  }

  // 更新状态
  std::lock_guard<std::mutex> lock(stateMutex);
  connected = false;
  state = GuiderState::DISCONNECTED;
}

void LinGuiderInterface::processMessage(const std::string &message) {
  std::lock_guard<std::mutex> lock(stateMutex);
  SPDLOG_DEBUG("Received from Lin-guider: {}", message);

  try {
    // Lin-guider消息格式：命令:值1,值2,...
    size_t colonPos = message.find(':');
    if (colonPos == std::string::npos) {
      return; // 无效消息格式
    }

    std::string command = message.substr(0, colonPos);
    std::string data = message.substr(colonPos + 1);

    if (command == "status") {
      // 状态消息: status:state,calibrated,rms,peak
      std::vector<std::string> parts = splitString(data, ',');
      if (parts.size() >= 4) {
        // 解析状态
        std::string stateStr = parts[0];
        if (stateStr == "idle") {
          state = GuiderState::CONNECTED;
        } else if (stateStr == "calibrating") {
          state = GuiderState::CALIBRATING;
        } else if (stateStr == "guiding") {
          state = GuiderState::GUIDING;
        } else if (stateStr == "paused") {
          state = GuiderState::PAUSED;
        } else if (stateStr == "settling") {
          state = GuiderState::SETTLING;
        }

        // 解析校准状态
        bool calibrated = (parts[1] == "1");
        calibration.calibrated = calibrated;

        if (calibrated) {
          calState = CalibrationState::COMPLETED;
        } else if (state == GuiderState::CALIBRATING) {
          calState =
              CalibrationState::NORTH_MOVING; // 简化，实际应该根据详细信息设置
        } else {
          calState = CalibrationState::IDLE;
        }

        // 解析RMS和Peak
        try {
          stats.rms = std::stod(parts[2]);
          stats.peak = std::stod(parts[3]);
        } catch (...) {
          // 解析错误，忽略
        }
      }
    } else if (command == "correction") {
      // 修正消息: correction:ra_raw,dec_raw,ra_correction,dec_correction
      std::vector<std::string> parts = splitString(data, ',');
      if (parts.size() >= 4) {
        try {
          lastCorrection.raRaw = std::stod(parts[0]);
          lastCorrection.decRaw = std::stod(parts[1]);
          lastCorrection.raCorrection = std::stod(parts[2]);
          lastCorrection.decCorrection = std::stod(parts[3]);

          // 更新统计数据
          double errorX = lastCorrection.raRaw;
          double errorY = lastCorrection.decRaw;
          double totalError = std::sqrt(errorX * errorX + errorY * errorY);

          // 更新RMS (指数移动平均)
          stats.totalFrames++;
          if (stats.totalFrames == 1) {
            stats.rmsRa = std::abs(errorX);
            stats.rmsDec = std::abs(errorY);
          } else {
            stats.rmsRa = stats.rmsRa * 0.9 + std::abs(errorX) * 0.1;
            stats.rmsDec = stats.rmsDec * 0.9 + std::abs(errorY) * 0.1;
          }

        } catch (...) {
          // 解析错误，忽略
        }
      }
    } else if (command == "star") {
      // 星体位置消息: star:x,y,snr
      std::vector<std::string> parts = splitString(data, ',');
      if (parts.size() >= 3) {
        try {
          guideStar.x = std::stod(parts[0]);
          guideStar.y = std::stod(parts[1]);
          guideStar.snr = std::stod(parts[2]);
          guideStar.locked = true;
        } catch (...) {
          // 解析错误，忽略
        }
      }
    } else if (command == "calibration") {
      // 校准数据消息: calibration:ra_angle,dec_angle,ra_rate,dec_rate,flipped
      std::vector<std::string> parts = splitString(data, ',');
      if (parts.size() >= 5) {
        try {
          calibration.raAngle = std::stod(parts[0]);
          calibration.decAngle = std::stod(parts[1]);
          calibration.raRate = std::stod(parts[2]);
          calibration.decRate = std::stod(parts[3]);
          calibration.flipped = (parts[4] == "1");
          calibration.calibrated = true;
        } catch (...) {
          // 解析错误，忽略
        }
      }
    } else if (command == "calibration_state") {
      // 校准状态消息: calibration_state:state
      if (data == "idle") {
        calState = CalibrationState::IDLE;
      } else if (data == "north_moving") {
        calState = CalibrationState::NORTH_MOVING;
      } else if (data == "north_complete") {
        calState = CalibrationState::NORTH_COMPLETE;
      } else if (data == "south_moving") {
        calState = CalibrationState::SOUTH_MOVING;
      } else if (data == "south_complete") {
        calState = CalibrationState::SOUTH_COMPLETE;
      } else if (data == "east_moving") {
        calState = CalibrationState::EAST_MOVING;
      } else if (data == "east_complete") {
        calState = CalibrationState::EAST_COMPLETE;
      } else if (data == "west_moving") {
        calState = CalibrationState::WEST_MOVING;
      } else if (data == "west_complete") {
        calState = CalibrationState::WEST_COMPLETE;
      } else if (data == "completed") {
        calState = CalibrationState::COMPLETED;
      } else if (data == "failed") {
        calState = CalibrationState::FAILED;
      }
    } else if (command == "calibration_completed") {
      // 校准完成事件
      calState = CalibrationState::COMPLETED;
      calibration.calibrated = true;
    } else if (command == "calibration_failed") {
      // 校准失败事件
      calState = CalibrationState::FAILED;
      calibration.calibrated = false;
    } else if (command == "star_lost") {
      // 星体丢失事件
      guideStar.locked = false;
      SPDLOG_WARN("Lin-guider reported star lost");
    } else if (command == "settle_begin") {
      // 抖动稳定开始
      state = GuiderState::SETTLING;
    } else if (command == "settle_done") {
      // 抖动稳定完成
      state = GuiderState::GUIDING;
    }
  } catch (const std::exception &e) {
    SPDLOG_ERROR("Error processing Lin-guider message: {}", e.what());
  }
}

// 字符串分割辅助函数
std::vector<std::string> LinGuiderInterface::splitString(const std::string &str,
                                                         char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream ss(str);
  std::string item;

  while (std::getline(ss, item, delimiter)) {
    tokens.push_back(item);
  }

  return tokens;
}

} // namespace astrocomm