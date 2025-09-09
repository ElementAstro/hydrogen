#include "filter_wheel.h"
#include "behaviors/movable_behavior.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>

namespace hydrogen {
namespace device {

// FilterWheelMovableBehavior implementation
FilterWheel::FilterWheelMovableBehavior::FilterWheelMovableBehavior(FilterWheel* filterWheel)
    : MovableBehavior("filter_wheel_movable"), filterWheel_(filterWheel) {
}

bool FilterWheel::FilterWheelMovableBehavior::executeMovement(int targetPosition) {
    return filterWheel_->executeFilterChange(targetPosition);
}

bool FilterWheel::FilterWheelMovableBehavior::executeStop() {
    return filterWheel_->executeStop();
}

bool FilterWheel::FilterWheelMovableBehavior::executeHome() {
    return filterWheel_->executeHome();
}

// FilterWheel implementation
FilterWheel::FilterWheel(const std::string& deviceId, const std::string& manufacturer, const std::string& model)
    : ModernDeviceBase(deviceId, "FILTER_WHEEL", manufacturer, model)
    , movableBehavior_(nullptr)
    , filterCount_(5) // Default to 5 filters
    , wheelDiameter_(50.0) // Default 50mm diameter
{
    // Set default filter count based on manufacturer
    if (manufacturer == "ZWO") {
        filterCount_ = 7; // EFW typically has 7 positions
        wheelDiameter_ = 36.0;
    } else if (manufacturer == "QHY") {
        filterCount_ = 5; // CFW typically has 5 positions
        wheelDiameter_ = 31.0;
    } else if (manufacturer == "SBIG") {
        filterCount_ = 5;
        wheelDiameter_ = 50.0;
    }
    
    initializeDefaultFilters();
    
    SPDLOG_INFO("FilterWheel {} created with manufacturer: {}, model: {}, {} filters", 
                deviceId, manufacturer, model, filterCount_.load());
}

FilterWheel::~FilterWheel() {
    stop();
}

bool FilterWheel::initializeDevice() {
    initializeFilterWheelBehaviors();
    
    // Initialize filter wheel properties
    setProperty("filterCount", filterCount_.load());
    setProperty("wheelDiameter", wheelDiameter_.load());
    setProperty("currentFilter", getCurrentPosition());
    
    return true;
}

bool FilterWheel::startDevice() {
    return true; // MovableBehavior handles the movement thread
}

void FilterWheel::stopDevice() {
    // Stop any ongoing filter change
    if (isMoving()) {
        stopMovement();
    }
}

void FilterWheel::initializeFilterWheelBehaviors() {
    // Add movable behavior for filter positioning
    movableBehavior_ = new FilterWheelMovableBehavior(this);
    addBehavior(std::unique_ptr<behaviors::DeviceBehavior>(movableBehavior_));
}

// IMovable interface implementation (delegated to MovableBehavior)
bool FilterWheel::moveToPosition(int position) {
    if (!validateFilterPosition(position)) {
        SPDLOG_ERROR("FilterWheel {} invalid filter position: {}", getDeviceId(), position);
        return false;
    }
    
    if (movableBehavior_) {
        return movableBehavior_->moveToPosition(position);
    }
    return false;
}

bool FilterWheel::moveRelative(int steps) {
    if (movableBehavior_) {
        return movableBehavior_->moveRelative(steps);
    }
    return false;
}

bool FilterWheel::stopMovement() {
    if (movableBehavior_) {
        return movableBehavior_->stopMovement();
    }
    return false;
}

bool FilterWheel::home() {
    if (movableBehavior_) {
        return movableBehavior_->home();
    }
    return false;
}

int FilterWheel::getCurrentPosition() const {
    if (movableBehavior_) {
        return movableBehavior_->getCurrentPosition();
    }
    return 0;
}

bool FilterWheel::isMoving() const {
    if (movableBehavior_) {
        return movableBehavior_->isMoving();
    }
    return false;
}

// IFilterWheel interface implementation
int FilterWheel::getFilterCount() const {
    return filterCount_;
}

int FilterWheel::getCurrentFilter() const {
    return getCurrentPosition();
}

bool FilterWheel::setFilter(int position) {
    return moveToPosition(position);
}

std::string FilterWheel::getFilterName(int position) const {
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    
    auto it = filterInfo_.find(position);
    if (it != filterInfo_.end()) {
        return it->second.name;
    }
    
    return "Filter " + std::to_string(position);
}

bool FilterWheel::setFilterName(int position, const std::string& name) {
    if (!validateFilterPosition(position)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    
    if (filterInfo_.find(position) == filterInfo_.end()) {
        filterInfo_[position] = FilterInfo{};
        filterInfo_[position].position = position;
    }
    
    filterInfo_[position].name = name;
    
    SPDLOG_DEBUG("FilterWheel {} filter {} name set to '{}'", getDeviceId(), position, name);
    return true;
}

// Backward compatibility methods
int FilterWheel::getNumFilters() const {
    return getFilterCount();
}

void FilterWheel::setFilterPosition(int position) {
    setFilter(position);
}

int FilterWheel::getFilterPosition() const {
    return getCurrentFilter();
}

// Extended functionality
bool FilterWheel::setFilterCount(int count) {
    if (count < 1 || count > 12) { // Reasonable limits
        SPDLOG_ERROR("FilterWheel {} invalid filter count: {}", getDeviceId(), count);
        return false;
    }
    
    filterCount_ = count;
    setProperty("filterCount", count);
    
    // Remove filter info for positions beyond the new count
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    auto it = filterInfo_.begin();
    while (it != filterInfo_.end()) {
        if (it->first >= count) {
            it = filterInfo_.erase(it);
        } else {
            ++it;
        }
    }
    
    SPDLOG_INFO("FilterWheel {} filter count set to {}", getDeviceId(), count);
    return true;
}

FilterInfo FilterWheel::getFilterInfo(int position) const {
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    
    auto it = filterInfo_.find(position);
    if (it != filterInfo_.end()) {
        return it->second;
    }
    
    // Return default filter info
    FilterInfo info;
    info.position = position;
    info.name = "Filter " + std::to_string(position);
    info.type = "Unknown";
    info.wavelength = 550.0; // Default to green
    info.bandwidth = 100.0;
    info.exposureFactor = 1.0;
    info.description = "Default filter";
    
    return info;
}

bool FilterWheel::setFilterInfo(int position, const FilterInfo& info) {
    if (!validateFilterPosition(position)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    filterInfo_[position] = info;
    filterInfo_[position].position = position; // Ensure position matches
    
    SPDLOG_DEBUG("FilterWheel {} filter {} info updated", getDeviceId(), position);
    return true;
}

std::vector<FilterInfo> FilterWheel::getAllFilterInfo() const {
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    
    std::vector<FilterInfo> allInfo;
    for (int i = 0; i < filterCount_; ++i) {
        allInfo.push_back(getFilterInfo(i));
    }
    
    return allInfo;
}

int FilterWheel::getFilterByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    
    for (const auto& [position, info] : filterInfo_) {
        if (info.name == name) {
            return position;
        }
    }
    
    return -1; // Not found
}

bool FilterWheel::setFilterByName(const std::string& name) {
    int position = getFilterByName(name);
    if (position >= 0) {
        return setFilter(position);
    }
    
    SPDLOG_ERROR("FilterWheel {} filter '{}' not found", getDeviceId(), name);
    return false;
}

std::vector<std::string> FilterWheel::getFilterNames() const {
    std::vector<std::string> names;
    
    for (int i = 0; i < filterCount_; ++i) {
        names.push_back(getFilterName(i));
    }
    
    return names;
}

bool FilterWheel::setDefaultFilterConfiguration() {
    initializeDefaultFilters();
    return true;
}

double FilterWheel::getWheelDiameter() const {
    return wheelDiameter_;
}

bool FilterWheel::setWheelDiameter(double diameter) {
    if (diameter <= 0) {
        return false;
    }
    
    wheelDiameter_ = diameter;
    setProperty("wheelDiameter", diameter);
    return true;
}

bool FilterWheel::waitForFilterChange(int timeoutMs) {
    if (!isMoving()) {
        return true;
    }
    
    std::unique_lock<std::mutex> lock(filterChangeMutex_);
    
    if (timeoutMs > 0) {
        return filterChangeCV_.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                                       [this] { return !isMoving(); });
    } else {
        filterChangeCV_.wait(lock, [this] { return !isMoving(); });
        return true;
    }
}

// Hardware abstraction methods (simulation)
bool FilterWheel::executeFilterChange(int position) {
    SPDLOG_DEBUG("FilterWheel {} executing filter change to position {}", getDeviceId(), position);
    
    // Simulate filter change delay
    std::thread([this, position]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 + position * 200)); // Simulate movement time
        
        if (movableBehavior_) {
            movableBehavior_->updateCurrentPosition(position);
            movableBehavior_->onMovementComplete(true);
        }
        
        setProperty("currentFilter", position);
        filterChangeCV_.notify_all();
        
        SPDLOG_INFO("FilterWheel {} filter change to position {} completed", getDeviceId(), position);
    }).detach();
    
    return true;
}

bool FilterWheel::executeStop() {
    SPDLOG_DEBUG("FilterWheel {} executing stop", getDeviceId());
    return true;
}

bool FilterWheel::executeHome() {
    SPDLOG_DEBUG("FilterWheel {} executing home", getDeviceId());
    return executeFilterChange(0);
}

int FilterWheel::readCurrentPosition() {
    // In a real implementation, this would read from hardware
    return getCurrentPosition();
}

void FilterWheel::initializeDefaultFilters() {
    std::lock_guard<std::mutex> lock(filterInfoMutex_);
    
    // Clear existing filters
    filterInfo_.clear();
    
    // Set up default LRGB + Ha filters
    std::vector<FilterInfo> defaultFilters = {
        {0, "Luminance", "Luminance", 550.0, 200.0, 1.0, "Clear luminance filter"},
        {1, "Red", "Red", 650.0, 100.0, 2.0, "Red color filter"},
        {2, "Green", "Green", 530.0, 100.0, 1.5, "Green color filter"},
        {3, "Blue", "Blue", 470.0, 100.0, 3.0, "Blue color filter"},
        {4, "Ha", "Narrowband", 656.3, 7.0, 10.0, "Hydrogen-alpha narrowband filter"}
    };
    
    for (int i = 0; i < std::min(static_cast<int>(defaultFilters.size()), filterCount_.load()); ++i) {
        filterInfo_[i] = defaultFilters[i];
    }
    
    // Fill remaining positions with generic filters
    for (int i = defaultFilters.size(); i < filterCount_; ++i) {
        FilterInfo info;
        info.position = i;
        info.name = "Filter " + std::to_string(i);
        info.type = "Generic";
        info.wavelength = 550.0;
        info.bandwidth = 100.0;
        info.exposureFactor = 1.0;
        info.description = "Generic filter";
        filterInfo_[i] = info;
    }
}

bool FilterWheel::validateFilterPosition(int position) const {
    return position >= 0 && position < filterCount_;
}

bool FilterWheel::handleDeviceCommand(const std::string& command, const json& parameters, json& result) {
    if (command == "SET_FILTER") {
        int position = parameters.value("position", 0);
        result["success"] = setFilter(position);
        return true;
    }
    else if (command == "GET_FILTER_COUNT") {
        result["count"] = getFilterCount();
        result["success"] = true;
        return true;
    }
    else if (command == "GET_CURRENT_FILTER") {
        result["position"] = getCurrentFilter();
        result["success"] = true;
        return true;
    }
    else if (command == "SET_FILTER_NAME") {
        int position = parameters.value("position", 0);
        std::string name = parameters.value("name", "");
        result["success"] = setFilterName(position, name);
        return true;
    }
    else if (command == "GET_FILTER_NAME") {
        int position = parameters.value("position", 0);
        result["name"] = getFilterName(position);
        result["success"] = true;
        return true;
    }
    else if (command == "GET_FILTER_INFO") {
        int position = parameters.value("position", 0);
        FilterInfo info = getFilterInfo(position);
        result["info"] = json{
            {"position", info.position},
            {"name", info.name},
            {"type", info.type},
            {"wavelength", info.wavelength},
            {"bandwidth", info.bandwidth},
            {"exposureFactor", info.exposureFactor},
            {"description", info.description}
        };
        result["success"] = true;
        return true;
    }
    else if (command == "SET_FILTER_BY_NAME") {
        std::string name = parameters.value("name", "");
        result["success"] = setFilterByName(name);
        return true;
    }
    else if (command == "HOME") {
        result["success"] = home();
        return true;
    }
    
    return false;
}

void FilterWheel::updateDevice() {
    // Update current filter position
    setProperty("currentFilter", getCurrentFilter());
    setProperty("isMoving", isMoving());
    
    // Update filter names for easy access
    json filterNames = json::array();
    for (int i = 0; i < filterCount_; ++i) {
        filterNames.push_back(getFilterName(i));
    }
    setProperty("filterNames", filterNames);
}

std::vector<std::string> FilterWheel::getCapabilities() const {
    return {
        "SET_FILTER",
        "GET_FILTER_COUNT",
        "GET_CURRENT_FILTER", 
        "SET_FILTER_NAME",
        "GET_FILTER_NAME",
        "GET_FILTER_INFO",
        "SET_FILTER_BY_NAME",
        "HOME",
        "MOVE_TO_POSITION",
        "MOVE_RELATIVE",
        "STOP_MOVEMENT"
    };
}

bool FilterWheel::loadFilterConfiguration(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            SPDLOG_ERROR("FilterWheel {} cannot open config file: {}", getDeviceId(), filename);
            return false;
        }
        
        json config;
        file >> config;
        
        if (config.contains("filters") && config["filters"].is_array()) {
            std::lock_guard<std::mutex> lock(filterInfoMutex_);
            filterInfo_.clear();
            
            for (const auto& filterJson : config["filters"]) {
                FilterInfo info;
                info.position = filterJson.value("position", 0);
                info.name = filterJson.value("name", "");
                info.type = filterJson.value("type", "Generic");
                info.wavelength = filterJson.value("wavelength", 550.0);
                info.bandwidth = filterJson.value("bandwidth", 100.0);
                info.exposureFactor = filterJson.value("exposureFactor", 1.0);
                info.description = filterJson.value("description", "");
                
                if (validateFilterPosition(info.position)) {
                    filterInfo_[info.position] = info;
                }
            }
        }
        
        if (config.contains("filterCount")) {
            setFilterCount(config["filterCount"]);
        }
        
        if (config.contains("wheelDiameter")) {
            setWheelDiameter(config["wheelDiameter"]);
        }
        
        SPDLOG_INFO("FilterWheel {} loaded configuration from {}", getDeviceId(), filename);
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("FilterWheel {} failed to load config: {}", getDeviceId(), e.what());
        return false;
    }
}

bool FilterWheel::saveFilterConfiguration(const std::string& filename) const {
    try {
        json config;
        config["filterCount"] = filterCount_.load();
        config["wheelDiameter"] = wheelDiameter_.load();
        
        json filtersArray = json::array();
        {
            std::lock_guard<std::mutex> lock(filterInfoMutex_);
            for (const auto& [position, info] : filterInfo_) {
                json filterJson = {
                    {"position", info.position},
                    {"name", info.name},
                    {"type", info.type},
                    {"wavelength", info.wavelength},
                    {"bandwidth", info.bandwidth},
                    {"exposureFactor", info.exposureFactor},
                    {"description", info.description}
                };
                filtersArray.push_back(filterJson);
            }
        }
        config["filters"] = filtersArray;
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            SPDLOG_ERROR("FilterWheel {} cannot create config file: {}", getDeviceId(), filename);
            return false;
        }
        
        file << config.dump(2);
        
        SPDLOG_INFO("FilterWheel {} saved configuration to {}", getDeviceId(), filename);
        return true;
        
    } catch (const std::exception& e) {
        SPDLOG_ERROR("FilterWheel {} failed to save config: {}", getDeviceId(), e.what());
        return false;
    }
}

double FilterWheel::getFilterThickness(int position) const {
    std::lock_guard<std::mutex> lock(thicknessMutex_);
    
    auto it = filterThickness_.find(position);
    if (it != filterThickness_.end()) {
        return it->second;
    }
    
    return 3.0; // Default 3mm thickness
}

bool FilterWheel::setFilterThickness(int position, double thickness) {
    if (!validateFilterPosition(position) || thickness < 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(thicknessMutex_);
    filterThickness_[position] = thickness;
    
    return true;
}

double FilterWheel::calculateFocusOffset(int fromFilter, int toFilter) const {
    if (!validateFilterPosition(fromFilter) || !validateFilterPosition(toFilter)) {
        return 0.0;
    }
    
    double fromThickness = getFilterThickness(fromFilter);
    double toThickness = getFilterThickness(toFilter);
    
    // Simple focus offset calculation based on thickness difference
    // Real implementation would use more sophisticated optics calculations
    double thicknessDiff = toThickness - fromThickness;
    double focusOffset = thicknessDiff * 0.3; // Approximate factor
    
    return focusOffset;
}

} // namespace device
} // namespace hydrogen
