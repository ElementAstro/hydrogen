#pragma once

// Minimal Crow WebSocket stub for compilation
// This is a temporary stub to resolve build errors
// In a real implementation, you would install the actual Crow library

#include <string>
#include <functional>
#include <memory>

namespace crow {

class websocket {
public:
    enum class type {
        TEXT = 1,
        BINARY = 2,
        CLOSE = 8,
        PING = 9,
        PONG = 10
    };
    
    class connection {
    public:
        void send_text(const std::string& msg) {}
        void send_binary(const std::string& msg) {}
        void close(const std::string& reason = "") {}
        bool is_alive() { return true; }
        
        std::function<void(const std::string&)> userdata;
    };
    
    template<typename Func>
    websocket& onopen(Func f) {
        onopen_handler = f;
        return *this;
    }
    
    template<typename Func>
    websocket& onclose(Func f) {
        onclose_handler = f;
        return *this;
    }
    
    template<typename Func>
    websocket& onmessage(Func f) {
        onmessage_handler = f;
        return *this;
    }
    
    template<typename Func>
    websocket& onerror(Func f) {
        onerror_handler = f;
        return *this;
    }
    
private:
    std::function<void(connection&)> onopen_handler;
    std::function<void(connection&, const std::string&)> onclose_handler;
    std::function<void(connection&, const std::string&, type)> onmessage_handler;
    std::function<void(connection&)> onerror_handler;
};

} // namespace crow
