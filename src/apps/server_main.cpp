#include "common/logger.h"
#include "server/device_server.h"
#include <csignal>
#include <iostream>
#include <string>

using namespace astrocomm;

// 全局服务器实例
std::unique_ptr<DeviceServer> server;

// 信号处理
void signalHandler(int sig) {
  logInfo("Received signal " + std::to_string(sig) + ", shutting down...",
          "Main");
  if (server) {
    server->stop();
  }
}

void printBanner() {
  std::cout << "\n";
  std::cout << "  ╔══════════════════════════════════════════════════════╗\n";
  std::cout << "  ║                                                      ║\n";
  std::cout << "  ║    Astro Device Communication Protocol Server        ║\n";
  std::cout << "  ║                                                      ║\n";
  std::cout << "  ║    Modern JSON-based astronomy device control        ║\n";
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
  initLogger("server.log", LogLevel::INFO);

  // 解析命令行参数
  uint16_t port = 8000;
  if (argc > 1) {
    try {
      port = static_cast<uint16_t>(std::stoi(argv[1]));
    } catch (const std::exception &e) {
      std::cerr << "Invalid port number: " << argv[1] << std::endl;
      return 1;
    }
  }

  logInfo("Starting server on port " + std::to_string(port), "Main");
  std::cout << "Server starting on port " << port << "..." << std::endl;

  try {
    // 创建并启动服务器
    server = std::make_unique<DeviceServer>(port);
    server->start();
  } catch (const std::exception &e) {
    logCritical("Server error: " + std::string(e.what()), "Main");
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}