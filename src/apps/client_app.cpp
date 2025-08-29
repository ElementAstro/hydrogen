#include <astrocomm/client.h>
#include "common/logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

using namespace astrocomm::client;

// 命令行界面颜色
namespace Color {
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";
const std::string WHITE = "\033[37m";
const std::string BOLD = "\033[1m";
} // namespace Color

// 全局客户端实例
std::unique_ptr<DeviceClient> client;

void printBanner() {
  std::cout << Color::BOLD << Color::CYAN;
  std::cout << "\n";
  std::cout << "  ╔══════════════════════════════════════════════════════╗\n";
  std::cout << "  ║                                                      ║\n";
  std::cout << "  ║          Astronomy Device Control Client             ║\n";
  std::cout << "  ║                                                      ║\n";
  std::cout << "  ╚══════════════════════════════════════════════════════╝\n\n";
  std::cout << Color::RESET;
}

void printMenu() {
  std::cout << Color::BOLD << Color::BLUE
            << "\n=== MAIN MENU ===" << Color::RESET << "\n";
  std::cout << " 1. List connected devices\n";
  std::cout << " 2. Telescope control\n";
  std::cout << " 3. Camera control\n";
  std::cout << " 4. Focuser control\n";
  std::cout << " 5. Refresh devices\n";
  std::cout << " 0. Exit\n";
  std::cout << Color::YELLOW << "\nEnter your choice: " << Color::RESET;
}

void printTelescopeMenu() {
  std::cout << Color::BOLD << Color::BLUE
            << "\n=== TELESCOPE CONTROL ===" << Color::RESET << "\n";
  std::cout << " 1. Get position (RA/DEC)\n";
  std::cout << " 2. GOTO position\n";
  std::cout << " 3. Toggle tracking\n";
  std::cout << " 4. Set slew rate\n";
  std::cout << " 5. Park/Unpark\n";
  std::cout << " 6. Abort movement\n";
  std::cout << " 0. Back to main menu\n";
  std::cout << Color::YELLOW << "\nEnter your choice: " << Color::RESET;
}

void handleTelescopeControl() {
  // Get available telescopes
  json devices = client->getDevices();
  std::vector<std::string> telescopeIds;

  for (const auto &device : devices) {
    if (device["deviceType"] == "TELESCOPE") {
      telescopeIds.push_back(device["deviceId"]);
    }
  }

  if (telescopeIds.empty()) {
    std::cout << Color::RED << "No telescopes available!" << Color::RESET
              << std::endl;
    return;
  }

  // Select telescope if multiple are available
  std::string selectedId = telescopeIds[0];
  if (telescopeIds.size() > 1) {
    std::cout << Color::BLUE << "Available telescopes:" << Color::RESET
              << std::endl;
    for (size_t i = 0; i < telescopeIds.size(); i++) {
      std::cout << " " << (i + 1) << ". " << telescopeIds[i] << std::endl;
    }

    int choice = 0;
    std::cout << Color::YELLOW << "Select telescope (1-" << telescopeIds.size()
              << "): " << Color::RESET;
    std::cin >> choice;
    std::cin.ignore();

    if (choice < 1 || choice > static_cast<int>(telescopeIds.size())) {
      std::cout << Color::RED << "Invalid selection!" << Color::RESET
                << std::endl;
      return;
    }

    selectedId = telescopeIds[choice - 1];
  }

  std::cout << Color::GREEN << "Using telescope: " << selectedId << Color::RESET
            << std::endl;

  // Telescope control loop
  int choice = -1;
  while (choice != 0) {
    printTelescopeMenu();
    std::cin >> choice;
    std::cin.ignore();

    try {
      switch (choice) {
      case 1: { // Get position
        json result = client->getDeviceProperties(
            selectedId, {"ra", "dec", "altitude", "azimuth"});

        std::cout << Color::GREEN << "Current Position:" << Color::RESET
                  << std::endl;
        std::cout << "  RA: " << std::fixed << std::setprecision(4)
                  << result["properties"]["ra"]["value"] << " hours"
                  << std::endl;
        std::cout << "  DEC: " << std::fixed << std::setprecision(4)
                  << result["properties"]["dec"]["value"] << " degrees"
                  << std::endl;
        std::cout << "  ALT: " << std::fixed << std::setprecision(4)
                  << result["properties"]["altitude"]["value"] << " degrees"
                  << std::endl;
        std::cout << "  AZ: " << std::fixed << std::setprecision(4)
                  << result["properties"]["azimuth"]["value"] << " degrees"
                  << std::endl;
        break;
      }
      case 2: { // GOTO
        double ra, dec;
        std::cout << "Enter RA (hours, 0-24): ";
        std::cin >> ra;
        std::cout << "Enter DEC (degrees, -90 to +90): ";
        std::cin >> dec;
        std::cin.ignore();

        if (ra < 0 || ra >= 24 || dec < -90 || dec > 90) {
          std::cout << Color::RED << "Invalid coordinates!" << Color::RESET
                    << std::endl;
          break;
        }

        json params = {{"ra", ra}, {"dec", dec}};

        json result = client->executeCommand(selectedId, "GOTO", params);

        if (result["payload"]["status"] == "IN_PROGRESS") {
          std::cout << Color::GREEN
                    << "GOTO started. Estimated completion time: "
                    << result["payload"]["details"]["estimatedCompletionTime"]
                    << Color::RESET << std::endl;
        } else {
          std::cout << Color::RED << "GOTO command failed: " << result.dump(2)
                    << Color::RESET << std::endl;
        }
        break;
      }
      case 3: { // Toggle tracking
        json result = client->getDeviceProperties(selectedId, {"tracking"});
        bool currentTracking = result["properties"]["tracking"]["value"];

        json trackingParams = {{"enabled", !currentTracking}};

        client->executeCommand(selectedId, "SET_TRACKING", trackingParams);

        std::cout << Color::GREEN << "Tracking "
                  << (!currentTracking ? "enabled" : "disabled") << Color::RESET
                  << std::endl;
        break;
      }
      case 4: { // Set slew rate
        int rate;
        std::cout << "Enter slew rate (1-10): ";
        std::cin >> rate;
        std::cin.ignore();

        if (rate < 1 || rate > 10) {
          std::cout << Color::RED << "Invalid slew rate!" << Color::RESET
                    << std::endl;
          break;
        }

        json props = {{"slew_rate", rate}};

        client->setDeviceProperties(selectedId, props);

        std::cout << Color::GREEN << "Slew rate set to " << rate << Color::RESET
                  << std::endl;
        break;
      }
      case 5: { // Park/Unpark
        json result = client->getDeviceProperties(selectedId, {"parked"});
        bool isParked = result["properties"]["parked"]["value"];

        json params = {{"action", isParked ? "unpark" : "park"}};

        client->executeCommand(selectedId, "PARK", params);

        std::cout << Color::GREEN << "Telescope "
                  << (isParked ? "unparking" : "parking") << "..."
                  << Color::RESET << std::endl;
        break;
      }
      case 6: { // Abort
        client->executeCommand(selectedId, "ABORT");
        std::cout << Color::GREEN << "Abort command sent" << Color::RESET
                  << std::endl;
        break;
      }
      case 0:
        // Return to main menu
        break;
      default:
        std::cout << Color::RED << "Invalid option!" << Color::RESET
                  << std::endl;
      }
    } catch (const std::exception &e) {
      std::cout << Color::RED << "Error: " << e.what() << Color::RESET
                << std::endl;
    }

    if (choice != 0) {
      std::cout << "\nPress Enter to continue...";
      std::cin.get();
    }
  }
}

// Event and property update callbacks
void onTelescopePropertyChanged(const std::string &property,
                                const json &value) {
  std::cout << Color::CYAN << "\n[Property] " << property << " = " << value.dump()
            << Color::RESET << std::endl;
}

void onTelescopeEvent(const std::string &event,
                      const json &details) {
  std::cout << Color::MAGENTA << "\n[Event] " << event
            << ", details: " << details.dump() << Color::RESET << std::endl;
}

int main(int argc, char *argv[]) {
  // 初始化日志
  initLogger("client.log", LogLevel::INFO);

  // 显示欢迎信息
  printBanner();

  // 解析命令行参数
  std::string host = "localhost";
  uint16_t port = 8000;

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

  std::cout << "Connecting to server at " << host << ":" << port << "..."
            << std::endl;

  try {
    // 创建客户端并连接
    client = std::make_unique<DeviceClient>();

    if (!client->connect(host, port)) {
      std::cout << Color::RED << "Failed to connect to server!" << Color::RESET
                << std::endl;
      return 1;
    }

    std::cout << Color::GREEN << "Connected to server!" << Color::RESET
              << std::endl;

    // 启动后台消息处理
    // Message processing starts automatically when client connects

    // 发现设备
    json devices = client->discoverDevices();
    std::cout << Color::GREEN << "Discovered " << devices.size() << " devices"
              << Color::RESET << std::endl;

    // 设置属性和事件订阅
    for (const auto &device : devices) {
      if (device["deviceType"] == "TELESCOPE") {
        std::string deviceId = device["deviceId"];

        // 订阅位置属性变更
        client->subscribeToProperty(deviceId, "ra", onTelescopePropertyChanged);
        client->subscribeToProperty(deviceId, "dec",
                                    onTelescopePropertyChanged);

        // 订阅重要事件
        client->subscribeToEvent(deviceId, "COMMAND_COMPLETED",
                                 onTelescopeEvent);
        client->subscribeToEvent(deviceId, "PARKED", onTelescopeEvent);
        client->subscribeToEvent(deviceId, "UNPARKED", onTelescopeEvent);
      }
    }

    // 主菜单循环
    int choice = -1;
    while (choice != 0) {
      printMenu();
      std::cin >> choice;
      std::cin.ignore();

      switch (choice) {
      case 1: { // List devices
        json devices = client->getDevices();

        std::cout << Color::GREEN << "\nConnected Devices:" << Color::RESET
                  << std::endl;
        for (const auto &device : devices) {
          std::cout << Color::BOLD << "  " << device["deviceId"] << Color::RESET
                    << std::endl;
          std::cout << "    Type: " << device["deviceType"] << std::endl;
          std::cout << "    Model: " << device["manufacturer"] << " "
                    << device["model"] << std::endl;

          if (device.contains("properties") &&
              device["properties"].is_array()) {
            std::cout << "    Properties: ";
            for (size_t i = 0; i < device["properties"].size(); i++) {
              std::cout << device["properties"][i];
              if (i < device["properties"].size() - 1) {
                std::cout << ", ";
              }
            }
            std::cout << std::endl;
          }

          if (device.contains("capabilities") &&
              device["capabilities"].is_array()) {
            std::cout << "    Capabilities: ";
            for (size_t i = 0; i < device["capabilities"].size(); i++) {
              std::cout << device["capabilities"][i];
              if (i < device["capabilities"].size() - 1) {
                std::cout << ", ";
              }
            }
            std::cout << std::endl;
          }

          std::cout << std::endl;
        }
        break;
      }
      case 2: // Telescope control
        handleTelescopeControl();
        break;
      case 3: // Camera control
        std::cout << "Camera control not implemented yet" << std::endl;
        break;
      case 4: // Focuser control
        std::cout << "Focuser control not implemented yet" << std::endl;
        break;
      case 5: { // Refresh devices
        json devices = client->discoverDevices();
        std::cout << Color::GREEN << "Discovered " << devices.size()
                  << " devices" << Color::RESET << std::endl;
        break;
      }
      case 0:
        std::cout << "Exiting..." << std::endl;
        break;
      default:
        std::cout << Color::RED << "Invalid option!" << Color::RESET
                  << std::endl;
      }

      if (choice != 0 && choice != 2) {
        std::cout << "\nPress Enter to continue...";
        std::cin.get();
      }
    }

    // 停止后台消息处理
    // Message processing stops automatically when client disconnects

    // 断开连接
    client->disconnect();
  } catch (const std::exception &e) {
    logCritical("Error: " + std::string(e.what()), "Main");
    std::cout << Color::RED << "Error: " << e.what() << Color::RESET
              << std::endl;
    return 1;
  }

  return 0;
}