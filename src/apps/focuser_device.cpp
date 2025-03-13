// 调焦器设备模拟器
// apps/focuser_device.cpp
#include "common/logger.h"
#include "device/focuser.h"
#include <csignal>
#include <iostream>
#include <string>

using namespace astrocomm;

std::unique_ptr<Focuser> focuser;

void signalHandler(int sig) {
  logInfo("Received signal " + std::to_string(sig) + ", shutting down...",
          "Main");
  if (focuser) {
    focuser->stop();
    focuser->disconnect();
  }
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  initLogger("focuser.log", LogLevel::INFO);

  // 解析命令行参数
  std::string host = "localhost";
  uint16_t port = 8000;
  std::string deviceId = "focuser-main";

  if (argc > 1)
    host = argv[1];
  if (argc > 2)
    port = static_cast<uint16_t>(std::stoi(argv[2]));
  if (argc > 3)
    deviceId = argv[3];

  try {
    focuser = std::make_unique<Focuser>(deviceId, "ZWO", "EAF");

    if (!focuser->connect(host, port)) {
      logCritical("Failed to connect to server", "Main");
      return 1;
    }

    if (!focuser->registerDevice()) {
      logCritical("Failed to register device", "Main");
      return 1;
    }

    if (!focuser->start()) {
      logCritical("Failed to start device", "Main");
      return 1;
    }

    std::cout << "Focuser device started and registered successfully"
              << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    focuser->run();
  } catch (const std::exception &e) {
    logCritical("Error: " + std::string(e.what()), "Main");
    return 1;
  }

  return 0;
}

// 滤镜轮设备模拟器
// apps/filter_wheel_device.cpp
#include "common/logger.h"
#include "device/filter_wheel.h"
#include <csignal>
#include <iostream>
#include <string>

using namespace astrocomm;

std::unique_ptr<FilterWheel> filterWheel;

void signalHandler(int sig) {
  logInfo("Received signal " + std::to_string(sig) + ", shutting down...",
          "Main");
  if (filterWheel) {
    filterWheel->stop();
    filterWheel->disconnect();
  }
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  initLogger("filter_wheel.log", LogLevel::INFO);

  // 解析命令行参数
  std::string host = "localhost";
  uint16_t port = 8000;
  std::string deviceId = "filter-wheel-main";

  if (argc > 1)
    host = argv[1];
  if (argc > 2)
    port = static_cast<uint16_t>(std::stoi(argv[2]));
  if (argc > 3)
    deviceId = argv[3];

  try {
    filterWheel = std::make_unique<FilterWheel>(deviceId, "QHY", "CFW3");

    if (!filterWheel->connect(host, port)) {
      logCritical("Failed to connect to server", "Main");
      return 1;
    }

    if (!filterWheel->registerDevice()) {
      logCritical("Failed to register device", "Main");
      return 1;
    }

    if (!filterWheel->start()) {
      logCritical("Failed to start device", "Main");
      return 1;
    }

    std::cout << "Filter Wheel device started and registered successfully"
              << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    filterWheel->run();
  } catch (const std::exception &e) {
    logCritical("Error: " + std::string(e.what()), "Main");
    return 1;
  }

  return 0;
}

// 导星器设备模拟器
// apps/guider_device.cpp
#include "common/logger.h"
#include "device/guider.h"
#include <csignal>
#include <iostream>
#include <string>

using namespace astrocomm;

std::unique_ptr<Guider> guider;

void signalHandler(int sig) {
  logInfo("Received signal " + std::to_string(sig) + ", shutting down...",
          "Main");
  if (guider) {
    guider->stop();
    guider->disconnect();
  }
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  initLogger("guider.log", LogLevel::INFO);

  // 解析命令行参数
  std::string host = "localhost";
  uint16_t port = 8000;
  std::string deviceId = "guider-main";

  if (argc > 1)
    host = argv[1];
  if (argc > 2)
    port = static_cast<uint16_t>(std::stoi(argv[2]));
  if (argc > 3)
    deviceId = argv[3];

  try {
    guider = std::make_unique<Guider>(deviceId, "QHY", "QHY5-II");

    if (!guider->connect(host, port)) {
      logCritical("Failed to connect to server", "Main");
      return 1;
    }

    if (!guider->registerDevice()) {
      logCritical("Failed to register device", "Main");
      return 1;
    }

    if (!guider->start()) {
      logCritical("Failed to start device", "Main");
      return 1;
    }

    std::cout << "Guider device started and registered successfully"
              << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    guider->run();
  } catch (const std::exception &e) {
    logCritical("Error: " + std::string(e.what()), "Main");
    return 1;
  }

  return 0;
}

// 解析器设备模拟器
// apps/solver_device.cpp
#include "common/logger.h"
#include "device/solver.h"
#include <csignal>
#include <iostream>
#include <string>

using namespace astrocomm;

std::unique_ptr<Solver> solver;

void signalHandler(int sig) {
  logInfo("Received signal " + std::to_string(sig) + ", shutting down...",
          "Main");
  if (solver) {
    solver->stop();
    solver->disconnect();
  }
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  initLogger("solver.log", LogLevel::INFO);

  // 解析命令行参数
  std::string host = "localhost";
  uint16_t port = 8000;
  std::string deviceId = "solver-main";

  if (argc > 1)
    host = argv[1];
  if (argc > 2)
    port = static_cast<uint16_t>(std::stoi(argv[2]));
  if (argc > 3)
    deviceId = argv[3];

  try {
    solver = std::make_unique<Solver>(deviceId, "AstroCode", "AstroSolver");

    if (!solver->connect(host, port)) {
      logCritical("Failed to connect to server", "Main");
      return 1;
    }

    if (!solver->registerDevice()) {
      logCritical("Failed to register device", "Main");
      return 1;
    }

    if (!solver->start()) {
      logCritical("Failed to start device", "Main");
      return 1;
    }

    std::cout << "Solver device started and registered successfully"
              << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    solver->run();
  } catch (const std::exception &e) {
    logCritical("Error: " + std::string(e.what()), "Main");
    return 1;
  }

  return 0;
}