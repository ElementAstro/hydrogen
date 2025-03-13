#include "common/logger.h"
#include "device/telescope.h"
#include <csignal>
#include <iostream>
#include <string>

using namespace astrocomm;

// 全局设备实例
std::unique_ptr<Telescope> telescope;

// 信号处理
void signalHandler(int sig) {
  logInfo("Received signal " + std::to_string(sig) + ", shutting down...",
          "Main");
  if (telescope) {
    telescope->stop();
    telescope->disconnect();
  }
}

void printBanner() {
  std::cout << "\n";
  std::cout << "  ╔══════════════════════════════════════════════════════╗\n";
  std::cout << "  ║                                                      ║\n";
  std::cout << "  ║           Telescope Device Simulator                 ║\n";
  std::cout << "  ║                                                      ║\n";
  std::cout << "  ╚══════════════════════════════════════════════════════╝\n\n";
}

int main(int argc, char *argv[]) {
  // 设置信号处理
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // 显示欢迎信息
  printBanner();

  // 初始化日志
  initLogger("telescope.log", LogLevel::INFO);

  // 解析命令行参数
  std::string host = "localhost";
  uint16_t port = 8000;
  std::string deviceId = "telescope-main";

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
    // 创建望远镜设备
    telescope =
        std::make_unique<Telescope>(deviceId, "Celestron", "NexStar 8SE");

    // 连接到服务器
    if (!telescope->connect(host, port)) {
      logCritical("Failed to connect to server", "Main");
      std::cerr << "Failed to connect to server" << std::endl;
      return 1;
    }

    // 注册设备
    if (!telescope->registerDevice()) {
      logCritical("Failed to register device", "Main");
      std::cerr << "Failed to register device" << std::endl;
      return 1;
    }

    // 启动设备
    if (!telescope->start()) {
      logCritical("Failed to start device", "Main");
      std::cerr << "Failed to start device" << std::endl;
      return 1;
    }

    std::cout << "Telescope device started and registered successfully"
              << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    // 运行消息循环
    telescope->run();
  } catch (const std::exception &e) {
    logCritical("Error: " + std::string(e.what()), "Main");
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}