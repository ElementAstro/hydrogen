#include "indi_bridge.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include <regex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace astrocomm {
namespace device {
namespace interfaces {
namespace indi {

// INDIMessage Implementation
std::string INDIMessage::toXML() const {
    std::ostringstream xml;
    
    switch (type) {
        case INDIMessageType::DEF_TEXT_VECTOR:
            xml << "<defTextVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::DEF_NUMBER_VECTOR:
            xml << "<defNumberVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::DEF_SWITCH_VECTOR:
            xml << "<defSwitchVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::DEF_LIGHT_VECTOR:
            xml << "<defLightVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::SET_TEXT_VECTOR:
            xml << "<setTextVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::SET_NUMBER_VECTOR:
            xml << "<setNumberVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::SET_SWITCH_VECTOR:
            xml << "<setSwitchVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::SET_LIGHT_VECTOR:
            xml << "<setLightVector device=\"" << device << "\" name=\"" << property << "\" state=\"";
            break;
        case INDIMessageType::MESSAGE:
            xml << "<message device=\"" << device << "\" timestamp=\"" << timestamp << "\">";
            xml << value << "</message>";
            return xml.str();
        default:
            xml << "<unknown>";
            break;
    }
    
    // Add state
    switch (state) {
        case PropertyState::IDLE: xml << "Idle"; break;
        case PropertyState::OK: xml << "Ok"; break;
        case PropertyState::BUSY: xml << "Busy"; break;
        case PropertyState::ALERT: xml << "Alert"; break;
    }
    
    xml << "\"";
    
    // Add timestamp if present
    if (!timestamp.empty()) {
        xml << " timestamp=\"" << timestamp << "\"";
    }
    
    // Add attributes
    for (const auto& [key, val] : attributes) {
        xml << " " << key << "=\"" << val << "\"";
    }
    
    xml << ">";
    
    // Add element if present
    if (!element.empty() && !value.empty()) {
        if (type == INDIMessageType::DEF_TEXT_VECTOR || type == INDIMessageType::SET_TEXT_VECTOR) {
            xml << "<defText name=\"" << element << "\">" << value << "</defText>";
        } else if (type == INDIMessageType::DEF_NUMBER_VECTOR || type == INDIMessageType::SET_NUMBER_VECTOR) {
            xml << "<defNumber name=\"" << element << "\">" << value << "</defNumber>";
        } else if (type == INDIMessageType::DEF_SWITCH_VECTOR || type == INDIMessageType::SET_SWITCH_VECTOR) {
            xml << "<defSwitch name=\"" << element << "\">" << value << "</defSwitch>";
        } else if (type == INDIMessageType::DEF_LIGHT_VECTOR || type == INDIMessageType::SET_LIGHT_VECTOR) {
            xml << "<defLight name=\"" << element << "\">" << value << "</defLight>";
        }
    }
    
    // Close tag
    switch (type) {
        case INDIMessageType::DEF_TEXT_VECTOR:
            xml << "</defTextVector>";
            break;
        case INDIMessageType::DEF_NUMBER_VECTOR:
            xml << "</defNumberVector>";
            break;
        case INDIMessageType::DEF_SWITCH_VECTOR:
            xml << "</defSwitchVector>";
            break;
        case INDIMessageType::DEF_LIGHT_VECTOR:
            xml << "</defLightVector>";
            break;
        case INDIMessageType::SET_TEXT_VECTOR:
            xml << "</setTextVector>";
            break;
        case INDIMessageType::SET_NUMBER_VECTOR:
            xml << "</setNumberVector>";
            break;
        case INDIMessageType::SET_SWITCH_VECTOR:
            xml << "</setSwitchVector>";
            break;
        case INDIMessageType::SET_LIGHT_VECTOR:
            xml << "</setLightVector>";
            break;
        default:
            xml << "</unknown>";
            break;
    }
    
    return xml.str();
}

INDIMessage INDIMessage::fromXML(const std::string& xml) {
    INDIMessage message;
    
    // Simple XML parsing - in real implementation would use proper XML parser
    std::regex deviceRegex(R"(device="([^"]*)")");
    std::regex nameRegex(R"(name="([^"]*)")");
    std::regex stateRegex(R"(state="([^"]*)")");
    std::regex timestampRegex(R"(timestamp="([^"]*)")");
    
    std::smatch match;
    
    if (std::regex_search(xml, match, deviceRegex)) {
        message.device = match[1].str();
    }
    
    if (std::regex_search(xml, match, nameRegex)) {
        message.property = match[1].str();
    }
    
    if (std::regex_search(xml, match, stateRegex)) {
        std::string stateStr = match[1].str();
        if (stateStr == "Idle") message.state = PropertyState::IDLE;
        else if (stateStr == "Ok") message.state = PropertyState::OK;
        else if (stateStr == "Busy") message.state = PropertyState::BUSY;
        else if (stateStr == "Alert") message.state = PropertyState::ALERT;
    }
    
    if (std::regex_search(xml, match, timestampRegex)) {
        message.timestamp = match[1].str();
    }
    
    // Determine message type from XML tag
    if (xml.find("<defTextVector") != std::string::npos) {
        message.type = INDIMessageType::DEF_TEXT_VECTOR;
    } else if (xml.find("<defNumberVector") != std::string::npos) {
        message.type = INDIMessageType::DEF_NUMBER_VECTOR;
    } else if (xml.find("<defSwitchVector") != std::string::npos) {
        message.type = INDIMessageType::DEF_SWITCH_VECTOR;
    } else if (xml.find("<defLightVector") != std::string::npos) {
        message.type = INDIMessageType::DEF_LIGHT_VECTOR;
    } else if (xml.find("<setTextVector") != std::string::npos) {
        message.type = INDIMessageType::SET_TEXT_VECTOR;
    } else if (xml.find("<setNumberVector") != std::string::npos) {
        message.type = INDIMessageType::SET_NUMBER_VECTOR;
    } else if (xml.find("<setSwitchVector") != std::string::npos) {
        message.type = INDIMessageType::SET_SWITCH_VECTOR;
    } else if (xml.find("<setLightVector") != std::string::npos) {
        message.type = INDIMessageType::SET_LIGHT_VECTOR;
    } else if (xml.find("<newTextVector") != std::string::npos) {
        message.type = INDIMessageType::NEW_TEXT_VECTOR;
    } else if (xml.find("<newNumberVector") != std::string::npos) {
        message.type = INDIMessageType::NEW_NUMBER_VECTOR;
    } else if (xml.find("<newSwitchVector") != std::string::npos) {
        message.type = INDIMessageType::NEW_SWITCH_VECTOR;
    } else if (xml.find("<message") != std::string::npos) {
        message.type = INDIMessageType::MESSAGE;
        
        // Extract message content
        std::regex messageRegex(R"(<message[^>]*>([^<]*)</message>)");
        if (std::regex_search(xml, match, messageRegex)) {
            message.value = match[1].str();
        }
    }
    
    return message;
}

// INDIClientConnection Implementation
bool INDIClientConnection::sendMessage(const INDIMessage& message) {
    if (!connected_.load()) return false;
    
    std::string xml = message.toXML();
    return sendXML(xml);
}

bool INDIClientConnection::sendXML(const std::string& xml) {
    if (!connected_.load()) return false;
    
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    try {
        // In real implementation, would send over socket
        SPDLOG_DEBUG("Sending INDI XML to client {}: {}", clientId_, xml);
        
        // Simulate network send
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to send INDI message to client {}: {}", clientId_, e.what());
        connected_ = false;
        return false;
    }
}

std::optional<INDIMessage> INDIClientConnection::receiveMessage() {
    if (!connected_.load()) return std::nullopt;
    
    std::lock_guard<std::mutex> lock(receiveMutex_);
    
    try {
        // In real implementation, would receive from socket
        // For simulation, return empty optional
        return std::nullopt;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Failed to receive INDI message from client {}: {}", clientId_, e.what());
        connected_ = false;
        return std::nullopt;
    }
}

void INDIClientConnection::disconnect() {
    if (!connected_.load()) return;
    
    connected_ = false;
    
    // In real implementation, would close socket
#ifdef _WIN32
    if (socketFd_ != INVALID_SOCKET) {
        closesocket(socketFd_);
        socketFd_ = INVALID_SOCKET;
    }
#else
    if (socketFd_ >= 0) {
        close(socketFd_);
        socketFd_ = -1;
    }
#endif
    
    SPDLOG_DEBUG("INDI client {} disconnected", clientId_);
}

// Template specializations for INDI property creation
template<>
indi::PropertyVector INDIAutomaticAdapter<ICamera>::createINDIProperty(const std::string& name, const json& value) const {
    indi::PropertyVector property;
    property.name = name;
    property.device = "Camera";
    property.state = indi::PropertyState::OK;
    
    if (name == "CCD_EXPOSURE") {
        property.type = indi::PropertyType::NUMBER;
        property.elements.push_back({"EXPOSURE", value.get<double>()});
    } else if (name == "CCD_TEMPERATURE") {
        property.type = indi::PropertyType::NUMBER;
        property.elements.push_back({"TEMPERATURE", value.get<double>()});
    } else if (name == "CCD_COOLER") {
        property.type = indi::PropertyType::SWITCH;
        property.elements.push_back({"COOLER_ON", value.get<bool>() ? "On" : "Off"});
    } else if (name == "CCD_FRAME") {
        property.type = indi::PropertyType::NUMBER;
        property.elements.push_back({"X", value.get<int>()});
        property.elements.push_back({"Y", value.get<int>()});
        property.elements.push_back({"WIDTH", value.get<int>()});
        property.elements.push_back({"HEIGHT", value.get<int>()});
    } else {
        property.type = indi::PropertyType::TEXT;
        property.elements.push_back({name, value.dump()});
    }
    
    return property;
}

template<>
indi::PropertyVector INDIAutomaticAdapter<ITelescope>::createINDIProperty(const std::string& name, const json& value) const {
    indi::PropertyVector property;
    property.name = name;
    property.device = "Telescope";
    property.state = indi::PropertyState::OK;
    
    if (name == "EQUATORIAL_EOD_COORD") {
        property.type = indi::PropertyType::NUMBER;
        property.elements.push_back({"RA", value.get<double>()});
        property.elements.push_back({"DEC", value.get<double>()});
    } else if (name == "HORIZONTAL_COORD") {
        property.type = indi::PropertyType::NUMBER;
        property.elements.push_back({"ALT", value.get<double>()});
        property.elements.push_back({"AZ", value.get<double>()});
    } else if (name == "TELESCOPE_TRACK_STATE") {
        property.type = indi::PropertyType::SWITCH;
        property.elements.push_back({"TRACK_ON", value.get<bool>() ? "On" : "Off"});
    } else if (name == "TELESCOPE_PARK") {
        property.type = indi::PropertyType::SWITCH;
        property.elements.push_back({"PARK", value.get<bool>() ? "On" : "Off"});
    } else {
        property.type = indi::PropertyType::TEXT;
        property.elements.push_back({name, value.dump()});
    }
    
    return property;
}

template<>
indi::PropertyVector INDIAutomaticAdapter<IFocuser>::createINDIProperty(const std::string& name, const json& value) const {
    indi::PropertyVector property;
    property.name = name;
    property.device = "Focuser";
    property.state = indi::PropertyState::OK;
    
    if (name == "ABS_FOCUS_POSITION") {
        property.type = indi::PropertyType::NUMBER;
        property.elements.push_back({"FOCUS_ABSOLUTE_POSITION", value.get<int>()});
    } else if (name == "FOCUS_TEMPERATURE") {
        property.type = indi::PropertyType::NUMBER;
        property.elements.push_back({"TEMPERATURE", value.get<double>()});
    } else if (name == "FOCUS_MOTION") {
        property.type = indi::PropertyType::SWITCH;
        property.elements.push_back({"FOCUS_INWARD", "Off"});
        property.elements.push_back({"FOCUS_OUTWARD", "Off"});
    } else {
        property.type = indi::PropertyType::TEXT;
        property.elements.push_back({name, value.dump()});
    }
    
    return property;
}

// Explicit template instantiations
template class INDIProtocolBridge<ICamera>;
template class INDIProtocolBridge<ITelescope>;
template class INDIProtocolBridge<IFocuser>;

} // namespace indi
} // namespace interfaces
} // namespace device
} // namespace astrocomm
