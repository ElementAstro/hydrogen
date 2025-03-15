#include "server/device_server.h"
#include "server/device_manager.h"
#include "server/auth_manager.h"
#include "common/logger.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <nlohmann/json.hpp>

using namespace astrocomm;
using json = nlohmann::json;
namespace fs = std::filesystem;

// 全局服务器实例
std::unique_ptr<DeviceServer> server;

// 状态监控变量
std::atomic<bool> serverRunning{false};
std::atomic<uint64_t> totalConnections{0};
std::atomic<uint64_t> activeConnections{0};
std::atomic<uint64_t> totalMessages{0};
std::time_t startTime;

// 配置
struct ServerConfig {
    uint16_t port = 8000;
    std::string logLevel = "INFO";
    std::string logFile = "server.log";
    bool enableConsoleLog = true;
    std::string deviceConfigFile = "devices.json";
    std::string userConfigFile = "users.json";
    bool enableHttps = false;
    std::string certFile = "";
    std::string keyFile = "";
    int statusReportInterval = 60;  // 状态报告间隔（秒）
};

ServerConfig config;

// 信号处理
void signalHandler(int sig) {
    std::string sigName;
    switch (sig) {
        case SIGINT: sigName = "SIGINT"; break;
        case SIGTERM: sigName = "SIGTERM"; break;
        default: sigName = std::to_string(sig);
    }
    
    logInfo("Received signal " + sigName + ", shutting down gracefully...", "Main");
    std::cout << "\nReceived signal " << sigName << ", shutting down gracefully..." << std::endl;
    
    serverRunning = false;
    if (server) {
        server->stop();
    }
}

// 优雅的终止处理
void setupSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    // 忽略SIGPIPE，防止在客户端断开连接时服务器崩溃
    signal(SIGPIPE, SIG_IGN);
}

// 加载配置文件
bool loadConfig(const std::string& configFile, ServerConfig& config) {
    try {
        if (!fs::exists(configFile)) {
            std::cout << "Config file not found: " << configFile << ". Using default settings." << std::endl;
            return false;
        }
        
        std::ifstream file(configFile);
        json j;
        file >> j;
        
        if (j.contains("port")) {
            config.port = j["port"];
        }
        
        if (j.contains("logging")) {
            auto& logging = j["logging"];
            if (logging.contains("level")) {
                config.logLevel = logging["level"];
            }
            if (logging.contains("file")) {
                config.logFile = logging["file"];
            }
            if (logging.contains("console")) {
                config.enableConsoleLog = logging["console"];
            }
        }
        
        if (j.contains("deviceConfig")) {
            config.deviceConfigFile = j["deviceConfig"];
        }
        
        if (j.contains("userConfig")) {
            config.userConfigFile = j["userConfig"];
        }
        
        if (j.contains("https")) {
            auto& https = j["https"];
            if (https.contains("enabled")) {
                config.enableHttps = https["enabled"];
            }
            if (https.contains("cert")) {
                config.certFile = https["cert"];
            }
            if (https.contains("key")) {
                config.keyFile = https["key"];
            }
        }
        
        if (j.contains("statusReportInterval")) {
            config.statusReportInterval = j["statusReportInterval"];
        }
        
        std::cout << "Configuration loaded from: " << configFile << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return false;
    }
}

// 保存默认配置文件
void saveDefaultConfig(const std::string& configFile) {
    try {
        json j = {
            {"port", config.port},
            {"logging", {
                {"level", "INFO"},
                {"file", "server.log"},
                {"console", true}
            }},
            {"deviceConfig", "devices.json"},
            {"userConfig", "users.json"},
            {"https", {
                {"enabled", false},
                {"cert", "server.crt"},
                {"key", "server.key"}
            }},
            {"statusReportInterval", 60}
        };
        
        std::ofstream file(configFile);
        file << std::setw(4) << j << std::endl;
        
        std::cout << "Created default configuration file: " << configFile << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error creating default configuration: " << e.what() << std::endl;
    }
}

// 设置日志级别
LogLevel getLogLevel(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARNING") return LogLevel::WARNING;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "CRITICAL") return LogLevel::CRITICAL;
    
    std::cout << "Unknown log level: " << level << ". Using INFO level." << std::endl;
    return LogLevel::INFO;
}

// 打印欢迎横幅
void printBanner() {
    std::cout << "\n";
    std::cout << "  ╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "  ║                                                          ║\n";
    std::cout << "  ║        Astro Device Communication Protocol Server        ║\n";
    std::cout << "  ║                                                          ║\n";
    std::cout << "  ║          Modern JSON-based Device Control Server         ║\n";
    std::cout << "  ║                                                          ║\n";
    std::cout << "  ╚══════════════════════════════════════════════════════════╝\n\n";
}

// 状态报告线程函数
void statusReportThread() {
    while (serverRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(config.statusReportInterval));
        
        if (!serverRunning) break;
        
        // 计算运行时间
        std::time_t currentTime = std::time(nullptr);
        double uptimeSeconds = std::difftime(currentTime, startTime);
        
        int days = static_cast<int>(uptimeSeconds) / 86400;
        int hours = (static_cast<int>(uptimeSeconds) % 86400) / 3600;
        int minutes = (static_cast<int>(uptimeSeconds) % 3600) / 60;
        int seconds = static_cast<int>(uptimeSeconds) % 60;
        
        std::stringstream uptimeStr;
        if (days > 0) uptimeStr << days << "d ";
        uptimeStr << std::setfill('0') << std::setw(2) << hours << ":"
                 << std::setfill('0') << std::setw(2) << minutes << ":"
                 << std::setfill('0') << std::setw(2) << seconds;
        
        // 生成状态报告
        std::stringstream report;
        report << "Server Status Report:" << std::endl
               << "  Uptime: " << uptimeStr.str() << std::endl
               << "  Total Connections: " << totalConnections << std::endl
               << "  Active Connections: " << activeConnections << std::endl
               << "  Total Messages Processed: " << totalMessages << std::endl;
        
        // 输出到日志
        logInfo(report.str(), "StatusMonitor");
        
        // 如果启用了控制台日志，也输出到控制台
        if (config.enableConsoleLog) {
            std::cout << "\n" << report.str() << std::endl;
        }
    }
}

// 检查命令行参数
void parseCommandLine(int argc, char* argv[], std::string& configFile) {
    configFile = "server.conf";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                std::cerr << "Error: --config requires a file path" << std::endl;
                exit(1);
            }
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                try {
                    config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
                }
                catch (const std::exception& e) {
                    std::cerr << "Error: Invalid port number" << std::endl;
                    exit(1);
                }
            } else {
                std::cerr << "Error: --port requires a number" << std::endl;
                exit(1);
            }
        }
        else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                config.logLevel = argv[++i];
            } else {
                std::cerr << "Error: --log-level requires a level name" << std::endl;
                exit(1);
            }
        }
        else if (arg == "-f" || arg == "--log-file") {
            if (i + 1 < argc) {
                config.logFile = argv[++i];
            } else {
                std::cerr << "Error: --log-file requires a file path" << std::endl;
                exit(1);
            }
        }
        else if (arg == "--generate-config") {
            if (i + 1 < argc) {
                saveDefaultConfig(argv[++i]);
            } else {
                saveDefaultConfig("server.conf");
            }
            std::cout << "Configuration file generated. Exiting." << std::endl;
            exit(0);
        }
        else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -c, --config FILE       Configuration file path (default: server.conf)" << std::endl;
            std::cout << "  -p, --port PORT         Server port (default: 8000)" << std::endl;
            std::cout << "  -l, --log-level LEVEL   Log level: DEBUG, INFO, WARNING, ERROR, CRITICAL (default: INFO)" << std::endl;
            std::cout << "  -f, --log-file FILE     Log file path (default: server.log)" << std::endl;
            std::cout << "  --generate-config [FILE] Generate a default configuration file and exit" << std::endl;
            std::cout << "  -h, --help              Show this help message and exit" << std::endl;
            exit(0);
        }
    }
}

// 获取当前内存使用情况（简化版，仅限Linux）
std::string getMemoryUsage() {
#ifdef __linux__
    std::ifstream statm("/proc/self/statm");
    if (!statm.is_open()) return "Unknown";
    
    long pages;
    statm >> pages;
    statm.close();
    
    // 页面大小通常是4KB
    double mb = (pages * 4.0) / 1024.0;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << mb << " MB";
    return ss.str();
#else
    return "Not available";
#endif
}

// 初始化统计和计数器
void initStats() {
    totalConnections = 0;
    activeConnections = 0;
    totalMessages = 0;
    startTime = std::time(nullptr);
}

// 服务器事件回调
void onServerEvent(const std::string& event, const json& data) {
    if (event == "connection") {
        totalConnections++;
        activeConnections++;
    }
    else if (event == "disconnection") {
        if (activeConnections > 0) activeConnections--;
    }
    else if (event == "message") {
        totalMessages++;
    }
}

// 主函数
int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string configFile;
    parseCommandLine(argc, argv, configFile);
    
    // 显示欢迎横幅
    printBanner();
    
    // 加载配置文件
    if (fs::exists(configFile)) {
        loadConfig(configFile, config);
    }
    else {
        std::cout << "Configuration file not found: " << configFile << ". Using default settings." << std::endl;
        std::cout << "Use --generate-config to create a default configuration file." << std::endl;
    }
    
    // 设置信号处理器
    setupSignalHandlers();
    
    // 初始化日志
    LogLevel logLevel = getLogLevel(config.logLevel);
    initLogger(config.logFile, logLevel);
    
    // 初始化统计
    initStats();
    serverRunning = true;
    
    // 显示启动信息
    std::cout << "Starting server on port " << config.port << "..." << std::endl;
    logInfo("Starting server on port " + std::to_string(config.port), "Main");
    logInfo("Log level: " + config.logLevel, "Main");
    logInfo("Server version: 1.0.0", "Main");
    
    // 启动状态报告线程
    std::thread statusThread;
    if (config.statusReportInterval > 0) {
        statusThread = std::thread(statusReportThread);
    }
    
    try {
        // 创建服务器实例
        server = std::make_unique<DeviceServer>(config.port);
        
        // 加载设备配置
        DeviceManager deviceManager;
        if (fs::exists(config.deviceConfigFile)) {
            if (deviceManager.loadDeviceConfiguration(config.deviceConfigFile)) {
                logInfo("Loaded device configuration from " + config.deviceConfigFile, "Main");
            } else {
                logWarning("Failed to load device configuration from " + config.deviceConfigFile, "Main");
            }
        }
        
        // 加载用户配置
        AuthManager authManager;
        if (fs::exists(config.userConfigFile)) {
            if (authManager.loadUserConfiguration(config.userConfigFile)) {
                logInfo("Loaded user configuration from " + config.userConfigFile, "Main");
            } else {
                logWarning("Failed to load user configuration from " + config.userConfigFile, "Main");
            }
        }
        
        // 启动服务器
        logInfo("Server started successfully", "Main");
        std::cout << "Server started successfully. Press Ctrl+C to stop." << std::endl;
        
        // 注册事件回调
        // 注意：这是一个示例，实际的DeviceServer类可能需要添加事件回调机制
        // server->setEventCallback(onServerEvent);
        
        // 启动服务器（阻塞调用）
        server->start();
        
        // 服务器停止后的清理
        logInfo("Server stopped", "Main");
        std::cout << "Server stopped." << std::endl;
        
        // 保存配置
        if (!config.deviceConfigFile.empty()) {
            deviceManager.saveDeviceConfiguration(config.deviceConfigFile);
            logInfo("Saved device configuration to " + config.deviceConfigFile, "Main");
        }
        
        if (!config.userConfigFile.empty()) {
            authManager.saveUserConfiguration(config.userConfigFile);
            logInfo("Saved user configuration to " + config.userConfigFile, "Main");
        }
    }
    catch (const std::exception& e) {
        logCritical("Server error: " + std::string(e.what()), "Main");
        std::cerr << "Error: " << e.what() << std::endl;
        
        serverRunning = false;
        if (statusThread.joinable()) {
            statusThread.join();
        }
        
        return 1;
    }
    
    // 等待状态报告线程结束
    serverRunning = false;
    if (statusThread.joinable()) {
        statusThread.join();
    }
    
    // 显示最终统计信息
    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "  Total Connections: " << totalConnections << std::endl;
    std::cout << "  Total Messages Processed: " << totalMessages << std::endl;
    std::cout << "  Memory Used: " << getMemoryUsage() << std::endl;
    
    // 记录关闭日志
    std::time_t endTime = std::time(nullptr);
    double runTime = std::difftime(endTime, startTime);
    int hours = static_cast<int>(runTime) / 3600;
    int minutes = (static_cast<int>(runTime) % 3600) / 60;
    int seconds = static_cast<int>(runTime) % 60;
    
    std::stringstream uptimeStr;
    uptimeStr << hours << "h " << minutes << "m " << seconds << "s";
    
    logInfo("Server shutdown complete. Uptime: " + uptimeStr.str(), "Main");
    
    return 0;
}
