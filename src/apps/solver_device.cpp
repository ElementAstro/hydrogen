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