#pragma once

// Minimal Crow HTTP framework stub for compilation
// This is a temporary stub to resolve build errors
// In a real implementation, you would install the actual Crow library

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

namespace crow {

class response {
public:
    response() = default;
    response(int code) : code_(code) {}
    response(const std::string& body) : body_(body) {}
    response(int code, const std::string& body) : code_(code), body_(body) {}

    void write(const std::string& text) { body_ += text; }
    void end() {}
    void end(const std::string& text) { body_ = text; }
    void set_header(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    int code = 200;
    std::string body_;

private:
    int code_ = 200;
    std::unordered_map<std::string, std::string> headers_;
};

class request {
public:
    std::string url;
    std::string method;
    std::string body;
    std::string remote_ip_address;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> url_params;

    std::string get_header_value(const std::string& key) const {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : "";
    }
};

template<typename... Middlewares>
class Crow {
public:
    template<typename Func>
    auto route(const std::string& rule) {
        return [](Func f) { return f; };
    }
    
    void port(int p) { port_ = p; }
    void multithreaded() {}
    void run() {}
    void stop() {}
    
private:
    int port_ = 8080;
};

using App = Crow<>;
using SimpleApp = Crow<>;

// Method literals
constexpr int operator""_method(const char* str, size_t len) {
    return 0; // Stub implementation
}

// Utility functions
std::string method_name(int method) {
    return "GET"; // Stub implementation
}

// Macros (simplified)
#define CROW_ROUTE(app, path) (app)
#define CROW_WEBSOCKET_ROUTE(app, path) (app)

} // namespace crow
