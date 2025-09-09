#include "device/focuser.h"
#include <csignal>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>


using namespace hydrogen;
using namespace hydrogen::device;

std::unique_ptr<Focuser> focuser;

void signalHandler(int sig) {
  spdlog::info("Received signal {}, shutting down...", sig);
  if (focuser) {
    focuser->stop();
    focuser->disconnect();
  }
}

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // 配置spdlog
  try {
    auto file_sink =
        std::make_shared<spdlog::sinks::basic_file_sink_mt>("focuser.log");
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger =
        std::make_shared<spdlog::logger>("focuser", sinks.begin(), sinks.end());

    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);
  } catch (const spdlog::spdlog_ex &ex) {
    std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    return 1;
  }

  // 解析命令行参�?
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
      spdlog::critical("Failed to connect to server");
      return 1;
    }

    if (!focuser->registerDevice()) {
      spdlog::critical("Failed to register device");
      return 1;
    }

    if (!focuser->start()) {
      spdlog::critical("Failed to start device");
      return 1;
    }

    std::cout << "Focuser device started and registered successfully"
              << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;

    focuser->run();
  } catch (const std::exception &e) {
    spdlog::critical("Error: {}", e.what());
    return 1;
  }

  return 0;
}