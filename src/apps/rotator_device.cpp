#include "common/logger.h"
#include "device/rotator.h"
#include <csignal>
#include <iostream>
#include <string>

using namespace hydrogen;
using namespace hydrogen::device;

// 全局设备实例
std::unique_ptr<Rotator> rotatorDevice;

// 信号处理
void signalHandler(int sig) {
  logInfo("Received signal " + std::to_string(sig) + ", shutting down...",
          "Main");
  if (rotatorDevice) {
    rotatorDevice->stop();
    rotatorDevice->disconnect();
  }
}

void printBanner() {
  std::cout << "\n";
  std::cout << "  ╔══════════════════════════════════════════════════════╗\n";
  std::cout << "  �?                                                     ║\n";
  std::cout << "  �?          Rotator Device Simulator                   ║\n";
  std::cout << "  �?                                                     ║\n";
  std::cout << "  ╚══════════════════════════════════════════════════════╝\n\n";
}

int main(int argc, char *argv[]) {
  // 设置信号处理
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // 显示欢迎信息
  printBanner();

  // 初始化日�?
  initLogger("rotator.log", LogLevel::INFO);

  // 解析命令行参�?
  std::string host = "localhost";
  uint16_t port = 8000;
  std::string deviceId = "rotator-main";

  if (argc > 1)
    host = argv[1];
  if (argc > 2) {
    try {
      port = static_cast<uint16_t>(std::stoi(argv[2]));
    } catch (const std::exception &e) {
      std::cerr << "Invalid port number: " << argv[2] << std::endl;
      return 1;
    }
  }
  if (argc > 3)
    deviceId = argv[3];

  std::cout << "Connecting to server at " << host << ":" << port << std::endl;
  std::cout << "Device ID: " << deviceId << std::endl;

  try {
    // 创建旋转器设�?
    rotatorDevice =
        std::make_unique<Rotator>(deviceId, "Optec", "Pyxis Field Rotator");

    // 连接到服务器
    if (!rotatorDevice->connect(host, port)) {
      logCritical("Failed to connect to server", "Main");
      std::cerr << "Failed to connect to server" << std::endl;
      return 1;
    }

    // 注册设备
    if (!rotatorDevice->registerDevice()) {
      logCritical("Failed to register device", "Main");
      std::cerr << "Failed to register device" << std::endl;
      return 1;
    }

    // 启动设备
    if (!rotatorDevice->start()) {
      logCritical("Failed to start device", "Main");
      std::cerr << "Failed to start device" << std::endl;
      return 1;
    }

    std::cout << "Rotator device started and registered successfully"
              << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    // 运行消息循环
    rotatorDevice->run();
  } catch (const std::exception &e) {
    logCritical("Error: " + std::string(e.what()), "Main");
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}