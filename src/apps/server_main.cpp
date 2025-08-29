#include <astrocomm/server.h>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

using namespace astrocomm::server;
namespace fs = std::filesystem;

// 信号处理相关
std::atomic<bool> running = true;
void signalHandler(int signum) {
    spdlog::info("接收到信号 {}, 准备关闭服务器", signum);
    running = false;
}

// 显示帮助信息
void showHelp() {
    std::cout << "Device Server 使用方法:" << std::endl;
    std::cout << "--port <port>          指定服务器监听端口 (默认: 8000)" << std::endl;
    std::cout << "--config <path>        指定配置文件目录路径" << std::endl;
    std::cout << "--log-level <level>    设置日志级别 (trace/debug/info/warn/error/critical)" << std::endl;
    std::cout << "--log-dir <path>       指定日志文件保存目录" << std::endl;
    std::cout << "--enable-access-control 启用访问控制" << std::endl;
    std::cout << "--enable-command-queue  启用命令队列" << std::endl;
    std::cout << "--heartbeat <seconds>   设置心跳间隔秒数 (默认: 30)" << std::endl;
    std::cout << "--help                  显示此帮助信息" << std::endl;
}

// 设置日志系统
void setupLogging(const std::string& logLevel, const std::string& logDir) {
    // 创建控制台日志
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    std::vector<spdlog::sink_ptr> sinks {console_sink};
    
    // 如果指定了日志目录，添加文件日志
    if (!logDir.empty()) {
        try {
            // 确保日志目录存在
            fs::create_directories(logDir);
            
            // 创建每日滚动文件日志
            auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                logDir + "/device_server_%Y-%m-%d.log", 0, 0);
            sinks.push_back(file_sink);
            
            spdlog::info("日志将保存到: {}", logDir);
        }
        catch (const std::exception& e) {
            std::cerr << "无法创建日志目录: " << e.what() << std::endl;
        }
    }
    
    // 创建并设置默认日志器
    auto logger = std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());
    
    // 设置日志级别
    if (logLevel == "trace") {
        logger->set_level(spdlog::level::trace);
    }
    else if (logLevel == "debug") {
        logger->set_level(spdlog::level::debug);
    }
    else if (logLevel == "info") {
        logger->set_level(spdlog::level::info);
    }
    else if (logLevel == "warn") {
        logger->set_level(spdlog::level::warn);
    }
    else if (logLevel == "error") {
        logger->set_level(spdlog::level::err);
    }
    else if (logLevel == "critical") {
        logger->set_level(spdlog::level::critical);
    }
    
    // 设置为全局默认日志器
    spdlog::set_default_logger(logger);
    
    // 设置日志模式
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    spdlog::info("日志系统初始化完成，日志级别: {}", logLevel);
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

int main(int argc, char *argv[]) {
    // 解析命令行参数
    uint16_t port = 8000;
    std::string configPath;
    std::string logLevel = "info";
    std::string logDir;
    bool enableAccessControl = false;
    bool enableCommandQueue = false;
    int heartbeatInterval = 30;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--port" && i + 1 < argc) {
            try {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
            catch (const std::exception& e) {
                std::cerr << "无效的端口号: " << argv[i] << std::endl;
                return 1;
            }
        }
        else if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
        else if (arg == "--log-level" && i + 1 < argc) {
            logLevel = argv[++i];
        }
        else if (arg == "--log-dir" && i + 1 < argc) {
            logDir = argv[++i];
        }
        else if (arg == "--enable-access-control") {
            enableAccessControl = true;
        }
        else if (arg == "--enable-command-queue") {
            enableCommandQueue = true;
        }
        else if (arg == "--heartbeat" && i + 1 < argc) {
            try {
                heartbeatInterval = std::stoi(argv[++i]);
                if (heartbeatInterval < 5) {
                    std::cerr << "警告: 心跳间隔太短，最小为5秒" << std::endl;
                    heartbeatInterval = 5;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "无效的心跳间隔: " << argv[i] << std::endl;
                return 1;
            }
        }
        else if (arg == "--help") {
            showHelp();
            return 0;
        }
    }
    
    // 显示欢迎横幅
    printBanner();
    
    // 设置日志系统
    setupLogging(logLevel, logDir);
    
    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // 创建设备服务器
        DeviceServer server(port);
        
        // 配置服务器
        if (!configPath.empty()) {
            // 确保配置目录存在
            fs::create_directories(configPath);
            server.setConfigPath(configPath);
            server.loadConfiguration();
        }
        
        // 设置心跳间隔
        server.setHeartbeatInterval(heartbeatInterval);
        
        // 设置访问控制
        server.setAccessControlEnabled(enableAccessControl);
        
        // 设置命令队列
        server.setCommandQueueEnabled(enableCommandQueue);
        
        // 显示服务器配置
        spdlog::info("设备服务器已启动: ");
        spdlog::info("  - 监听端口: {}", port);
        spdlog::info("  - 配置目录: {}", configPath.empty() ? "未设置" : configPath);
        spdlog::info("  - 访问控制: {}", enableAccessControl ? "已启用" : "未启用");
        spdlog::info("  - 命令队列: {}", enableCommandQueue ? "已启用" : "未启用");
        spdlog::info("  - 心跳间隔: {}秒", heartbeatInterval);
        spdlog::info("  - 日志级别: {}", logLevel);
        
        // 启动一个线程运行服务器
        std::thread serverThread([&server]() {
            server.start();
        });
        
        // 主线程监控运行状态
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // 停止服务器
        spdlog::info("正在停止设备服务器...");
        server.stop();
        
        // 等待服务器线程完成
        if (serverThread.joinable()) {
            serverThread.join();
        }
        
        spdlog::info("设备服务器已安全关闭");
    }
    catch (const std::exception& e) {
        spdlog::error("服务器错误: {}", e.what());
        return 1;
    }
    
    return 0;
}
