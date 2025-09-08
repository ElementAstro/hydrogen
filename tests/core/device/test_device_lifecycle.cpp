#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <vector>
#include <future>
#include <atomic>
#include <fstream>
#include <filesystem>

#include "hydrogen/core/device/device_lifecycle.h"

using namespace hydrogen::core;
using namespace std::chrono_literals;

class DeviceLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a fresh instance for each test
        manager_ = std::make_unique<DeviceLifecycleManager>();
        
        // Reset callback tracking
        callbackEvents_.clear();
        callbackCount_ = 0;
        
        // Set up callback to track events
        manager_->setStateChangeCallback([this](const LifecycleEvent& event) {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callbackEvents_.push_back(event);
            callbackCount_++;
        });
        
        // Test device IDs
        testDeviceId_ = "test_device_001";
        testDeviceId2_ = "test_device_002";
        
        // Temporary file for persistence tests
        tempFilename_ = "test_lifecycle_data.json";
    }
    
    void TearDown() override {
        manager_.reset();
        
        // Clean up temporary files
        if (std::filesystem::exists(tempFilename_)) {
            std::filesystem::remove(tempFilename_);
        }
    }
    
    void waitForCallback(size_t expectedCount, std::chrono::milliseconds timeout = 1000ms) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (callbackEvents_.size() >= expectedCount) {
                    return;
                }
            }
            std::this_thread::sleep_for(10ms);
        }
    }
    
    std::unique_ptr<DeviceLifecycleManager> manager_;
    std::string testDeviceId_;
    std::string testDeviceId2_;
    std::string tempFilename_;
    
    // Callback tracking
    std::vector<LifecycleEvent> callbackEvents_;
    std::atomic<size_t> callbackCount_{0};
    std::mutex callbackMutex_;
};

// Test Device Registration and Basic State Management
TEST_F(DeviceLifecycleTest, DeviceRegistrationAndUnregistration) {
    // Test device registration
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::UNINITIALIZED);
    
    // Test registration with different initial state
    manager_->registerDevice(testDeviceId2_, DeviceLifecycleState::INITIALIZED);
    EXPECT_EQ(manager_->getCurrentState(testDeviceId2_), DeviceLifecycleState::INITIALIZED);
    
    // Test double registration (should not change state)
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::CONNECTED);
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::UNINITIALIZED);
    
    // Test unregistration
    manager_->unregisterDevice(testDeviceId_);
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::UNINITIALIZED);
    
    // Test getting state of unregistered device
    EXPECT_EQ(manager_->getCurrentState("nonexistent_device"), DeviceLifecycleState::UNINITIALIZED);
}

// Test Valid State Transitions
TEST_F(DeviceLifecycleTest, ValidStateTransitions) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    // Test valid transition sequence: UNINITIALIZED -> INITIALIZING -> INITIALIZED
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZING, "START_INIT", "Starting initialization"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::INITIALIZING);
    
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZED, "INIT_COMPLETE", "Initialization completed"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::INITIALIZED);
    
    // Test connection sequence: INITIALIZED -> CONNECTING -> CONNECTED
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTING, "START_CONNECT", "Starting connection"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::CONNECTING);
    
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTED, "CONNECT_SUCCESS", "Connection established"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::CONNECTED);
    
    // Test running sequence: CONNECTED -> STARTING -> RUNNING
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::STARTING, "START_DEVICE", "Starting device"));
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "DEVICE_READY", "Device is running"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::RUNNING);
}

// Test Invalid State Transitions
TEST_F(DeviceLifecycleTest, InvalidStateTransitions) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    // Test invalid transitions with strict validation enabled
    manager_->setStrictValidation(true);
    
    // Cannot go directly from UNINITIALIZED to CONNECTED
    EXPECT_FALSE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTED, "INVALID", "Invalid transition"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::UNINITIALIZED);
    
    // Cannot go from UNINITIALIZED to RUNNING
    EXPECT_FALSE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "INVALID", "Invalid transition"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::UNINITIALIZED);
    
    // Test with strict validation disabled
    manager_->setStrictValidation(false);
    
    // Should allow invalid transitions when strict validation is off
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "FORCE", "Forced transition"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::RUNNING);
}

// Test State Validation Functions
TEST_F(DeviceLifecycleTest, StateValidationFunctions) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    // Test isValidTransition
    EXPECT_TRUE(manager_->isValidTransition(testDeviceId_, DeviceLifecycleState::UNINITIALIZED, DeviceLifecycleState::INITIALIZING));
    EXPECT_FALSE(manager_->isValidTransition(testDeviceId_, DeviceLifecycleState::UNINITIALIZED, DeviceLifecycleState::RUNNING));
    
    // Test getValidNextStates
    auto validStates = manager_->getValidNextStates(testDeviceId_);
    EXPECT_GT(validStates.size(), 0);
    EXPECT_TRUE(std::find(validStates.begin(), validStates.end(), DeviceLifecycleState::INITIALIZING) != validStates.end());
    EXPECT_TRUE(std::find(validStates.begin(), validStates.end(), DeviceLifecycleState::ERROR) != validStates.end());
    
    // Test for non-existent device
    auto emptyStates = manager_->getValidNextStates("nonexistent");
    EXPECT_TRUE(emptyStates.empty());
}

// Test Error Handling and Recovery
TEST_F(DeviceLifecycleTest, ErrorHandlingAndRecovery) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::RUNNING);
    
    // Test force error state
    manager_->forceErrorState(testDeviceId_, "Critical system failure");
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::ERROR);
    
    // Wait for callback
    waitForCallback(2); // Registration + error transition
    
    // Verify error event was generated
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        EXPECT_GE(callbackEvents_.size(), 2);
        auto errorEvent = callbackEvents_.back();
        EXPECT_EQ(errorEvent.deviceId, testDeviceId_);
        EXPECT_EQ(errorEvent.previousState, DeviceLifecycleState::RUNNING);
        EXPECT_EQ(errorEvent.newState, DeviceLifecycleState::ERROR);
        EXPECT_EQ(errorEvent.trigger, "FORCE_ERROR");
        EXPECT_EQ(errorEvent.reason, "Critical system failure");
    }
    
    // Test recovery attempt
    EXPECT_TRUE(manager_->attemptRecovery(testDeviceId_));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::RECOVERING);
    
    // Test recovery to initialized state
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZED, "RECOVERY_SUCCESS", "Recovery completed"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::INITIALIZED);
    
    // Test force error on non-existent device (should not crash)
    manager_->forceErrorState("nonexistent", "Test error");
}

// Test Event Handling and Callbacks
TEST_F(DeviceLifecycleTest, EventHandlingAndCallbacks) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    // Perform several state transitions
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZING, "START", "Starting");
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZED, "COMPLETE", "Completed");
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTING, "CONNECT", "Connecting");
    
    // Wait for callbacks
    waitForCallback(4); // Registration + 3 transitions
    
    // Verify all events were captured
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        EXPECT_EQ(callbackEvents_.size(), 4);
        
        // Check first transition event
        auto& firstEvent = callbackEvents_[1]; // Skip registration event
        EXPECT_EQ(firstEvent.deviceId, testDeviceId_);
        EXPECT_EQ(firstEvent.previousState, DeviceLifecycleState::UNINITIALIZED);
        EXPECT_EQ(firstEvent.newState, DeviceLifecycleState::INITIALIZING);
        EXPECT_EQ(firstEvent.trigger, "START");
        EXPECT_EQ(firstEvent.reason, "Starting");
        
        // Check last transition event
        auto& lastEvent = callbackEvents_.back();
        EXPECT_EQ(lastEvent.deviceId, testDeviceId_);
        EXPECT_EQ(lastEvent.previousState, DeviceLifecycleState::INITIALIZED);
        EXPECT_EQ(lastEvent.newState, DeviceLifecycleState::CONNECTING);
        EXPECT_EQ(lastEvent.trigger, "CONNECT");
        EXPECT_EQ(lastEvent.reason, "Connecting");
    }
    
    // Test callback removal
    manager_->setStateChangeCallback(nullptr);
    size_t previousCount = callbackCount_.load();
    
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTED, "FINAL", "Final transition");
    
    // Should not receive new callbacks
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(callbackCount_.load(), previousCount);
}

// Test State History Management
TEST_F(DeviceLifecycleTest, StateHistoryManagement) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    // Perform multiple transitions
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZING, "T1", "Transition 1");
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZED, "T2", "Transition 2");
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTING, "T3", "Transition 3");
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTED, "T4", "Transition 4");
    
    // Get full history
    auto fullHistory = manager_->getStateHistory(testDeviceId_);
    EXPECT_EQ(fullHistory.size(), 5); // Registration + 4 transitions
    
    // Verify history order (should be chronological)
    EXPECT_EQ(fullHistory[0].fromState, DeviceLifecycleState::UNINITIALIZED);
    EXPECT_EQ(fullHistory[0].toState, DeviceLifecycleState::UNINITIALIZED);
    EXPECT_EQ(fullHistory[0].trigger, "REGISTRATION");
    
    EXPECT_EQ(fullHistory[1].fromState, DeviceLifecycleState::UNINITIALIZED);
    EXPECT_EQ(fullHistory[1].toState, DeviceLifecycleState::INITIALIZING);
    EXPECT_EQ(fullHistory[1].trigger, "T1");
    
    EXPECT_EQ(fullHistory[4].fromState, DeviceLifecycleState::CONNECTING);
    EXPECT_EQ(fullHistory[4].toState, DeviceLifecycleState::CONNECTED);
    EXPECT_EQ(fullHistory[4].trigger, "T4");
    
    // Get limited history
    auto limitedHistory = manager_->getStateHistory(testDeviceId_, 3);
    EXPECT_EQ(limitedHistory.size(), 3);
    
    // Should get the most recent entries
    EXPECT_EQ(limitedHistory[0].trigger, "T2");
    EXPECT_EQ(limitedHistory[2].trigger, "T4");
    
    // Test history for non-existent device
    auto emptyHistory = manager_->getStateHistory("nonexistent");
    EXPECT_TRUE(emptyHistory.empty());
}

// Test History Trimming
TEST_F(DeviceLifecycleTest, HistoryTrimming) {
    manager_->setMaxHistoryEntries(3);
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    // Perform more transitions than max history
    for (int i = 0; i < 5; ++i) {
        DeviceLifecycleState nextState = (i % 2 == 0) ? DeviceLifecycleState::INITIALIZING : DeviceLifecycleState::INITIALIZED;
        manager_->transitionTo(testDeviceId_, nextState, "T" + std::to_string(i), "Transition " + std::to_string(i));
    }
    
    // History should be trimmed to max entries
    auto history = manager_->getStateHistory(testDeviceId_);
    EXPECT_EQ(history.size(), 3);
    
    // Should contain the most recent entries
    EXPECT_EQ(history[0].trigger, "T2");
    EXPECT_EQ(history[2].trigger, "T4");
}

// Test Concurrent Operations and Thread Safety
TEST_F(DeviceLifecycleTest, ConcurrentOperations) {
    const int numThreads = 10;
    const int operationsPerThread = 50;
    std::vector<std::future<void>> futures;
    std::atomic<int> successfulTransitions{0};
    
    // Register multiple devices
    for (int i = 0; i < numThreads; ++i) {
        manager_->registerDevice("device_" + std::to_string(i), DeviceLifecycleState::UNINITIALIZED);
    }
    
    // Launch concurrent threads performing state transitions
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i, operationsPerThread, &successfulTransitions]() {
            std::string deviceId = "device_" + std::to_string(i);
            
            for (int j = 0; j < operationsPerThread; ++j) {
                // Cycle through some basic states
                DeviceLifecycleState targetState;
                std::string trigger;
                
                switch (j % 4) {
                    case 0:
                        targetState = DeviceLifecycleState::INITIALIZING;
                        trigger = "INIT_START";
                        break;
                    case 1:
                        targetState = DeviceLifecycleState::INITIALIZED;
                        trigger = "INIT_DONE";
                        break;
                    case 2:
                        targetState = DeviceLifecycleState::CONNECTING;
                        trigger = "CONNECT_START";
                        break;
                    case 3:
                        targetState = DeviceLifecycleState::CONNECTED;
                        trigger = "CONNECT_DONE";
                        break;
                }
                
                if (manager_->transitionTo(deviceId, targetState, trigger, "Concurrent test")) {
                    successfulTransitions.fetch_add(1);
                }
                
                // Small delay to increase chance of race conditions
                std::this_thread::sleep_for(1ms);
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Verify that operations completed successfully
    EXPECT_GT(successfulTransitions.load(), numThreads * operationsPerThread * 0.5); // At least 50% success rate
    
    // Verify all devices are in valid states
    for (int i = 0; i < numThreads; ++i) {
        std::string deviceId = "device_" + std::to_string(i);
        DeviceLifecycleState state = manager_->getCurrentState(deviceId);
        EXPECT_NE(state, DeviceLifecycleState::UNINITIALIZED); // Should have moved from initial state
    }
}

// Test Device Queries and Statistics
TEST_F(DeviceLifecycleTest, DeviceQueriesAndStatistics) {
    // Register devices in different states
    manager_->registerDevice("device_running_1", DeviceLifecycleState::RUNNING);
    manager_->registerDevice("device_running_2", DeviceLifecycleState::RUNNING);
    manager_->registerDevice("device_error_1", DeviceLifecycleState::ERROR);
    manager_->registerDevice("device_stopped_1", DeviceLifecycleState::STOPPED);
    
    // Test getDevicesInState
    auto runningDevices = manager_->getDevicesInState(DeviceLifecycleState::RUNNING);
    EXPECT_EQ(runningDevices.size(), 2);
    EXPECT_TRUE(std::find(runningDevices.begin(), runningDevices.end(), "device_running_1") != runningDevices.end());
    EXPECT_TRUE(std::find(runningDevices.begin(), runningDevices.end(), "device_running_2") != runningDevices.end());
    
    auto errorDevices = manager_->getDevicesInState(DeviceLifecycleState::ERROR);
    EXPECT_EQ(errorDevices.size(), 1);
    EXPECT_EQ(errorDevices[0], "device_error_1");
    
    auto uninitializedDevices = manager_->getDevicesInState(DeviceLifecycleState::UNINITIALIZED);
    EXPECT_TRUE(uninitializedDevices.empty());
    
    // Test lifecycle statistics
    auto stats = manager_->getLifecycleStatistics();
    EXPECT_TRUE(stats.contains("totalDevices"));
    EXPECT_TRUE(stats.contains("totalTransitions"));
    EXPECT_TRUE(stats.contains("stateDistribution"));
    
    EXPECT_EQ(stats["totalDevices"], 4);
    EXPECT_GE(stats["totalTransitions"], 4); // At least one transition per device (registration)
    
    auto stateDistribution = stats["stateDistribution"];
    EXPECT_EQ(stateDistribution["RUNNING"], 2);
    EXPECT_EQ(stateDistribution["ERROR"], 1);
    EXPECT_EQ(stateDistribution["STOPPED"], 1);
}

// Test Pause/Resume Functionality
TEST_F(DeviceLifecycleTest, PauseResumeOperations) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::RUNNING);
    
    // Test pause sequence
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::PAUSING, "USER_PAUSE", "User requested pause"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::PAUSING);
    
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::PAUSED, "PAUSE_COMPLETE", "Device paused"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::PAUSED);
    
    // Test resume sequence
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RESUMING, "USER_RESUME", "User requested resume"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::RESUMING);
    
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "RESUME_COMPLETE", "Device resumed"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::RUNNING);
    
    // Verify history contains all transitions
    auto history = manager_->getStateHistory(testDeviceId_);
    EXPECT_GE(history.size(), 5); // Registration + 4 transitions
}

// Test Maintenance and Update Operations
TEST_F(DeviceLifecycleTest, MaintenanceAndUpdateOperations) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::RUNNING);
    
    // Test maintenance mode
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::MAINTENANCE, "SCHEDULED_MAINTENANCE", "Scheduled maintenance"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::MAINTENANCE);
    
    // Test update mode from running state
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "MAINTENANCE_COMPLETE", "Maintenance completed");
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::UPDATING, "FIRMWARE_UPDATE", "Firmware update"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::UPDATING);
    
    // Test return to running after update
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "UPDATE_COMPLETE", "Update completed"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::RUNNING);
}

// Test Shutdown Operations
TEST_F(DeviceLifecycleTest, ShutdownOperations) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::RUNNING);
    
    // Test graceful shutdown sequence
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::STOPPING, "SHUTDOWN_REQUEST", "Shutdown requested"));
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::STOPPED, "STOP_COMPLETE", "Device stopped"));
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::DISCONNECTING, "DISCONNECT_START", "Disconnecting"));
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::DISCONNECTED, "DISCONNECT_COMPLETE", "Disconnected"));
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::SHUTDOWN, "FINAL_SHUTDOWN", "Final shutdown"));
    
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::SHUTDOWN);
    
    // Test that no transitions are allowed from SHUTDOWN state
    EXPECT_FALSE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "INVALID", "Should not work"));
    EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::SHUTDOWN);
    
    // Test valid next states from shutdown (should be empty)
    auto validStates = manager_->getValidNextStates(testDeviceId_);
    EXPECT_TRUE(validStates.empty());
}

// Test Helper Functions
TEST_F(DeviceLifecycleTest, HelperFunctions) {
    // Test state to string conversion
    EXPECT_EQ(lifecycleStateToString(DeviceLifecycleState::UNINITIALIZED), "UNINITIALIZED");
    EXPECT_EQ(lifecycleStateToString(DeviceLifecycleState::RUNNING), "RUNNING");
    EXPECT_EQ(lifecycleStateToString(DeviceLifecycleState::ERROR), "ERROR");
    EXPECT_EQ(lifecycleStateToString(DeviceLifecycleState::SHUTDOWN), "SHUTDOWN");
    
    // Test string to state conversion
    EXPECT_EQ(stringToLifecycleState("UNINITIALIZED"), DeviceLifecycleState::UNINITIALIZED);
    EXPECT_EQ(stringToLifecycleState("RUNNING"), DeviceLifecycleState::RUNNING);
    EXPECT_EQ(stringToLifecycleState("ERROR"), DeviceLifecycleState::ERROR);
    EXPECT_EQ(stringToLifecycleState("INVALID_STATE"), DeviceLifecycleState::UNINITIALIZED);
    
    // Test state classification functions
    EXPECT_TRUE(isErrorState(DeviceLifecycleState::ERROR));
    EXPECT_FALSE(isErrorState(DeviceLifecycleState::RUNNING));
    
    EXPECT_TRUE(isTransitionalState(DeviceLifecycleState::INITIALIZING));
    EXPECT_TRUE(isTransitionalState(DeviceLifecycleState::CONNECTING));
    EXPECT_FALSE(isTransitionalState(DeviceLifecycleState::RUNNING));
    EXPECT_FALSE(isTransitionalState(DeviceLifecycleState::ERROR));
    
    EXPECT_TRUE(isStableState(DeviceLifecycleState::RUNNING));
    EXPECT_TRUE(isStableState(DeviceLifecycleState::STOPPED));
    EXPECT_FALSE(isStableState(DeviceLifecycleState::INITIALIZING));
    EXPECT_FALSE(isStableState(DeviceLifecycleState::ERROR));
}

// Test JSON Serialization
TEST_F(DeviceLifecycleTest, JsonSerialization) {
    // Test StateTransition serialization
    StateTransition transition;
    transition.fromState = DeviceLifecycleState::RUNNING;
    transition.toState = DeviceLifecycleState::PAUSED;
    transition.trigger = "USER_PAUSE";
    transition.reason = "User requested pause";
    transition.timestamp = std::chrono::system_clock::now();
    
    json transitionJson = transition.toJson();
    EXPECT_EQ(transitionJson["fromState"], "RUNNING");
    EXPECT_EQ(transitionJson["toState"], "PAUSED");
    EXPECT_EQ(transitionJson["trigger"], "USER_PAUSE");
    EXPECT_EQ(transitionJson["reason"], "User requested pause");
    EXPECT_TRUE(transitionJson.contains("timestamp"));
    
    // Test deserialization
    StateTransition deserializedTransition = StateTransition::fromJson(transitionJson);
    EXPECT_EQ(deserializedTransition.fromState, DeviceLifecycleState::RUNNING);
    EXPECT_EQ(deserializedTransition.toState, DeviceLifecycleState::PAUSED);
    EXPECT_EQ(deserializedTransition.trigger, "USER_PAUSE");
    EXPECT_EQ(deserializedTransition.reason, "User requested pause");
    
    // Test LifecycleEvent serialization
    LifecycleEvent event;
    event.deviceId = "test_device";
    event.previousState = DeviceLifecycleState::RUNNING;
    event.newState = DeviceLifecycleState::PAUSED;
    event.trigger = "USER_PAUSE";
    event.reason = "User requested pause";
    event.timestamp = std::chrono::system_clock::now();
    event.metadata = json{{"user", "admin"}, {"priority", "high"}};
    
    json eventJson = event.toJson();
    EXPECT_EQ(eventJson["deviceId"], "test_device");
    EXPECT_EQ(eventJson["previousState"], "RUNNING");
    EXPECT_EQ(eventJson["newState"], "PAUSED");
    EXPECT_TRUE(eventJson.contains("metadata"));
    EXPECT_EQ(eventJson["metadata"]["user"], "admin");
    
    // Test deserialization
    LifecycleEvent deserializedEvent = LifecycleEvent::fromJson(eventJson);
    EXPECT_EQ(deserializedEvent.deviceId, "test_device");
    EXPECT_EQ(deserializedEvent.previousState, DeviceLifecycleState::RUNNING);
    EXPECT_EQ(deserializedEvent.newState, DeviceLifecycleState::PAUSED);
    EXPECT_EQ(deserializedEvent.metadata["user"], "admin");
}

// Test Persistence (Save/Load)
TEST_F(DeviceLifecycleTest, PersistenceOperations) {
    // Set up some device states
    manager_->registerDevice("device1", DeviceLifecycleState::RUNNING);
    manager_->registerDevice("device2", DeviceLifecycleState::ERROR);
    manager_->transitionTo("device1", DeviceLifecycleState::PAUSED, "TEST", "Test transition");
    manager_->forceErrorState("device2", "Test error");
    
    // Save lifecycle data
    EXPECT_TRUE(manager_->saveLifecycleData(tempFilename_));
    EXPECT_TRUE(std::filesystem::exists(tempFilename_));
    
    // Create new manager and load data
    auto newManager = std::make_unique<DeviceLifecycleManager>();
    EXPECT_TRUE(newManager->loadLifecycleData(tempFilename_));
    
    // Verify loaded states
    EXPECT_EQ(newManager->getCurrentState("device1"), DeviceLifecycleState::PAUSED);
    EXPECT_EQ(newManager->getCurrentState("device2"), DeviceLifecycleState::ERROR);
    
    // Verify history was loaded
    auto history1 = newManager->getStateHistory("device1");
    EXPECT_GT(history1.size(), 1); // Should have registration + transition
    
    auto history2 = newManager->getStateHistory("device2");
    EXPECT_GT(history2.size(), 1); // Should have registration + error
    
    // Test loading non-existent file
    EXPECT_FALSE(newManager->loadLifecycleData("nonexistent_file.json"));
    
    // Test saving to invalid path
    EXPECT_FALSE(manager_->saveLifecycleData("/invalid/path/file.json"));
}

// Test Configuration Management
TEST_F(DeviceLifecycleTest, ConfigurationManagement) {
    // Test strict validation setting
    manager_->setStrictValidation(false);
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    
    // Should allow invalid transition when strict validation is off
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "FORCE", "Forced"));
    
    manager_->setStrictValidation(true);
    
    // Should not allow invalid transition when strict validation is on
    EXPECT_FALSE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::SHUTDOWN, "INVALID", "Invalid"));
    
    // Test max history entries setting
    manager_->setMaxHistoryEntries(2);
    manager_->registerDevice(testDeviceId2_, DeviceLifecycleState::UNINITIALIZED);
    
    // Add more transitions than max history
    manager_->transitionTo(testDeviceId2_, DeviceLifecycleState::INITIALIZING, "T1", "T1");
    manager_->transitionTo(testDeviceId2_, DeviceLifecycleState::INITIALIZED, "T2", "T2");
    manager_->transitionTo(testDeviceId2_, DeviceLifecycleState::CONNECTING, "T3", "T3");
    
    // History should be limited
    auto history = manager_->getStateHistory(testDeviceId2_);
    EXPECT_EQ(history.size(), 2);
}

// Test Singleton Behavior
TEST_F(DeviceLifecycleTest, SingletonBehavior) {
    auto& instance1 = DeviceLifecycleManager::getInstance();
    auto& instance2 = DeviceLifecycleManager::getInstance();
    
    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);
    
    // Test that singleton maintains state
    instance1.registerDevice("singleton_test", DeviceLifecycleState::RUNNING);
    EXPECT_EQ(instance2.getCurrentState("singleton_test"), DeviceLifecycleState::RUNNING);
}

// Test Edge Cases and Error Conditions
TEST_F(DeviceLifecycleTest, EdgeCasesAndErrorConditions) {
    // Test operations on non-existent device
    EXPECT_FALSE(manager_->transitionTo("nonexistent", DeviceLifecycleState::RUNNING, "TEST", "Test"));
    EXPECT_EQ(manager_->getCurrentState("nonexistent"), DeviceLifecycleState::UNINITIALIZED);
    EXPECT_FALSE(manager_->attemptRecovery("nonexistent"));
    
    // Test empty device ID
    manager_->registerDevice("", DeviceLifecycleState::RUNNING);
    EXPECT_EQ(manager_->getCurrentState(""), DeviceLifecycleState::RUNNING);
    
    // Test very long device ID
    std::string longId(1000, 'x');
    manager_->registerDevice(longId, DeviceLifecycleState::RUNNING);
    EXPECT_EQ(manager_->getCurrentState(longId), DeviceLifecycleState::RUNNING);
    
    // Test empty trigger and reason
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZING, "", ""));
    
    // Test null callback handling
    manager_->setStateChangeCallback(nullptr);
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZED, "TEST", "Test"));
}

// Test Performance and Scalability
TEST_F(DeviceLifecycleTest, PerformanceAndScalability) {
    const int numDevices = 1000;
    const int transitionsPerDevice = 10;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Register many devices
    for (int i = 0; i < numDevices; ++i) {
        manager_->registerDevice("perf_device_" + std::to_string(i), DeviceLifecycleState::UNINITIALIZED);
    }
    
    // Perform many transitions
    for (int i = 0; i < numDevices; ++i) {
        std::string deviceId = "perf_device_" + std::to_string(i);
        for (int j = 0; j < transitionsPerDevice; ++j) {
            DeviceLifecycleState nextState = (j % 2 == 0) ? DeviceLifecycleState::INITIALIZING : DeviceLifecycleState::INITIALIZED;
            manager_->transitionTo(deviceId, nextState, "PERF_TEST", "Performance test");
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Verify all operations completed
    auto stats = manager_->getLifecycleStatistics();
    EXPECT_EQ(stats["totalDevices"], numDevices);
    
    // Performance should be reasonable (adjust threshold as needed)
    EXPECT_LT(duration.count(), 5000); // Should complete within 5 seconds
    
    std::cout << "Performance test: " << numDevices << " devices, "
              << (numDevices * transitionsPerDevice) << " transitions in "
              << duration.count() << "ms" << std::endl;
}

// Test Integration with Performance Monitoring
TEST_F(DeviceLifecycleTest, PerformanceMonitoringIntegration) {
    // Test metrics collection during lifecycle operations
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);

    auto startTime = std::chrono::high_resolution_clock::now();

    // Perform a series of transitions and measure timing
    std::vector<std::chrono::milliseconds> transitionTimes;

    auto states = {
        DeviceLifecycleState::INITIALIZING,
        DeviceLifecycleState::INITIALIZED,
        DeviceLifecycleState::CONNECTING,
        DeviceLifecycleState::CONNECTED,
        DeviceLifecycleState::STARTING,
        DeviceLifecycleState::RUNNING
    };

    for (auto state : states) {
        auto transitionStart = std::chrono::high_resolution_clock::now();

        EXPECT_TRUE(manager_->transitionTo(testDeviceId_, state, "PERF_MONITOR", "Performance monitoring test"));

        auto transitionEnd = std::chrono::high_resolution_clock::now();
        auto transitionDuration = std::chrono::duration_cast<std::chrono::milliseconds>(transitionEnd - transitionStart);
        transitionTimes.push_back(transitionDuration);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Verify performance characteristics
    EXPECT_LT(totalDuration.count(), 100); // Should be very fast for in-memory operations

    // Each individual transition should be sub-millisecond
    for (auto duration : transitionTimes) {
        EXPECT_LT(duration.count(), 10); // Very generous threshold
    }

    // Test statistics collection
    auto stats = manager_->getLifecycleStatistics();
    EXPECT_EQ(stats["totalDevices"], 1);
    EXPECT_EQ(stats["totalTransitions"], 7); // Registration + 6 transitions

    auto stateDistribution = stats["stateDistribution"];
    EXPECT_EQ(stateDistribution["RUNNING"], 1);
}

// Test Resource Management and Cleanup
TEST_F(DeviceLifecycleTest, ResourceManagementAndCleanup) {
    const int numDevices = 100;

    // Register many devices and perform transitions
    for (int i = 0; i < numDevices; ++i) {
        std::string deviceId = "resource_test_" + std::to_string(i);
        manager_->registerDevice(deviceId, DeviceLifecycleState::UNINITIALIZED);

        // Perform some transitions to build up history
        manager_->transitionTo(deviceId, DeviceLifecycleState::INITIALIZING, "INIT", "Init");
        manager_->transitionTo(deviceId, DeviceLifecycleState::INITIALIZED, "DONE", "Done");
        manager_->transitionTo(deviceId, DeviceLifecycleState::ERROR, "ERROR", "Test error");
    }

    // Verify all devices are registered
    auto stats = manager_->getLifecycleStatistics();
    EXPECT_EQ(stats["totalDevices"], numDevices);

    // Test bulk unregistration
    for (int i = 0; i < numDevices; ++i) {
        std::string deviceId = "resource_test_" + std::to_string(i);
        manager_->unregisterDevice(deviceId);
    }

    // Verify cleanup
    auto finalStats = manager_->getLifecycleStatistics();
    EXPECT_EQ(finalStats["totalDevices"], 0);

    // Test that unregistered devices return default state
    EXPECT_EQ(manager_->getCurrentState("resource_test_0"), DeviceLifecycleState::UNINITIALIZED);
}

// Test Configuration Integration and Dynamic Updates
TEST_F(DeviceLifecycleTest, ConfigurationIntegration) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);

    // Test dynamic configuration changes
    manager_->setStrictValidation(true);
    manager_->setMaxHistoryEntries(5);

    // Perform transitions with strict validation
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZING, "CONFIG_TEST", "Config test"));
    EXPECT_FALSE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "INVALID", "Should fail"));

    // Change configuration during runtime
    manager_->setStrictValidation(false);
    EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "NOW_VALID", "Now valid"));

    // Test history limit enforcement
    for (int i = 0; i < 10; ++i) {
        DeviceLifecycleState nextState = (i % 2 == 0) ? DeviceLifecycleState::PAUSED : DeviceLifecycleState::RUNNING;
        manager_->transitionTo(testDeviceId_, nextState, "HISTORY_TEST_" + std::to_string(i), "History test");
    }

    auto history = manager_->getStateHistory(testDeviceId_);
    EXPECT_EQ(history.size(), 5); // Should be limited by maxHistoryEntries
}

// Test Complex State Transition Scenarios
TEST_F(DeviceLifecycleTest, ComplexStateTransitionScenarios) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);

    // Test full device lifecycle scenario
    std::vector<std::pair<DeviceLifecycleState, std::string>> lifecycle = {
        {DeviceLifecycleState::INITIALIZING, "Starting initialization"},
        {DeviceLifecycleState::INITIALIZED, "Initialization complete"},
        {DeviceLifecycleState::CONNECTING, "Establishing connection"},
        {DeviceLifecycleState::CONNECTED, "Connection established"},
        {DeviceLifecycleState::STARTING, "Starting device operations"},
        {DeviceLifecycleState::RUNNING, "Device operational"},
        {DeviceLifecycleState::PAUSING, "Pausing for maintenance"},
        {DeviceLifecycleState::PAUSED, "Device paused"},
        {DeviceLifecycleState::RESUMING, "Resuming operations"},
        {DeviceLifecycleState::RUNNING, "Operations resumed"},
        {DeviceLifecycleState::UPDATING, "Firmware update"},
        {DeviceLifecycleState::RUNNING, "Update complete"},
        {DeviceLifecycleState::STOPPING, "Stopping device"},
        {DeviceLifecycleState::STOPPED, "Device stopped"},
        {DeviceLifecycleState::DISCONNECTING, "Disconnecting"},
        {DeviceLifecycleState::DISCONNECTED, "Disconnected"},
        {DeviceLifecycleState::SHUTDOWN, "Final shutdown"}
    };

    // Execute full lifecycle
    for (const auto& [state, reason] : lifecycle) {
        EXPECT_TRUE(manager_->transitionTo(testDeviceId_, state, "LIFECYCLE", reason))
            << "Failed to transition to " << lifecycleStateToString(state);
        EXPECT_EQ(manager_->getCurrentState(testDeviceId_), state);
    }

    // Verify complete history
    auto history = manager_->getStateHistory(testDeviceId_);
    EXPECT_EQ(history.size(), lifecycle.size() + 1); // +1 for registration

    // Verify final state is terminal
    auto validNextStates = manager_->getValidNextStates(testDeviceId_);
    EXPECT_TRUE(validNextStates.empty());
}

// Test Error Recovery Scenarios
TEST_F(DeviceLifecycleTest, ErrorRecoveryScenarios) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::RUNNING);

    // Test error during different states
    std::vector<DeviceLifecycleState> testStates = {
        DeviceLifecycleState::INITIALIZING,
        DeviceLifecycleState::CONNECTING,
        DeviceLifecycleState::STARTING,
        DeviceLifecycleState::RUNNING,
        DeviceLifecycleState::UPDATING
    };

    for (auto state : testStates) {
        // Set device to test state
        manager_->setStrictValidation(false);
        manager_->transitionTo(testDeviceId_, state, "SETUP", "Setup for error test");
        manager_->setStrictValidation(true);

        // Force error
        std::string errorReason = "Error during " + lifecycleStateToString(state);
        manager_->forceErrorState(testDeviceId_, errorReason);
        EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::ERROR);

        // Attempt recovery
        EXPECT_TRUE(manager_->attemptRecovery(testDeviceId_));
        EXPECT_EQ(manager_->getCurrentState(testDeviceId_), DeviceLifecycleState::RECOVERING);

        // Complete recovery
        EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZED, "RECOVERY_COMPLETE", "Recovery completed"));

        // Verify device can continue normal operation
        EXPECT_TRUE(manager_->transitionTo(testDeviceId_, DeviceLifecycleState::CONNECTING, "CONTINUE", "Continue operation"));
    }
}

// Test Callback Event Metadata and Timing
TEST_F(DeviceLifecycleTest, CallbackEventMetadataAndTiming) {
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);

    std::vector<std::chrono::system_clock::time_point> transitionTimes;

    // Set up callback to capture timing information
    manager_->setStateChangeCallback([this, &transitionTimes](const LifecycleEvent& event) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callbackEvents_.push_back(event);
        transitionTimes.push_back(event.timestamp);
        callbackCount_++;
    });

    // Perform transitions with timing
    auto beforeTransition = std::chrono::system_clock::now();
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::INITIALIZING, "TIMING_TEST", "Timing test");
    auto afterTransition = std::chrono::system_clock::now();

    waitForCallback(2); // Registration + transition

    // Verify timing information
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        EXPECT_EQ(callbackEvents_.size(), 2);

        auto& transitionEvent = callbackEvents_.back();
        EXPECT_GE(transitionEvent.timestamp, beforeTransition);
        EXPECT_LE(transitionEvent.timestamp, afterTransition);

        // Verify event details
        EXPECT_EQ(transitionEvent.deviceId, testDeviceId_);
        EXPECT_EQ(transitionEvent.previousState, DeviceLifecycleState::UNINITIALIZED);
        EXPECT_EQ(transitionEvent.newState, DeviceLifecycleState::INITIALIZING);
        EXPECT_EQ(transitionEvent.trigger, "TIMING_TEST");
        EXPECT_EQ(transitionEvent.reason, "Timing test");
    }

    // Test chronological ordering of events
    EXPECT_GE(transitionTimes.size(), 2);
    for (size_t i = 1; i < transitionTimes.size(); ++i) {
        EXPECT_GE(transitionTimes[i], transitionTimes[i-1]);
    }
}

// Test Memory Usage and Leak Prevention
TEST_F(DeviceLifecycleTest, MemoryUsageAndLeakPrevention) {
    const int iterations = 1000;

    // Test repeated registration/unregistration
    for (int i = 0; i < iterations; ++i) {
        std::string deviceId = "memory_test_" + std::to_string(i);

        // Register device
        manager_->registerDevice(deviceId, DeviceLifecycleState::UNINITIALIZED);

        // Perform some transitions to build history
        manager_->transitionTo(deviceId, DeviceLifecycleState::INITIALIZING, "MEM_TEST", "Memory test");
        manager_->transitionTo(deviceId, DeviceLifecycleState::INITIALIZED, "MEM_TEST", "Memory test");

        // Unregister device (should clean up all associated data)
        manager_->unregisterDevice(deviceId);

        // Verify device is gone
        EXPECT_EQ(manager_->getCurrentState(deviceId), DeviceLifecycleState::UNINITIALIZED);
    }

    // Verify no devices remain
    auto stats = manager_->getLifecycleStatistics();
    EXPECT_EQ(stats["totalDevices"], 0);
}

// Test Stress Testing with Rapid State Changes
TEST_F(DeviceLifecycleTest, StressTestingRapidStateChanges) {
    const int numDevices = 50;
    const int changesPerDevice = 100;

    // Register devices
    for (int i = 0; i < numDevices; ++i) {
        manager_->registerDevice("stress_device_" + std::to_string(i), DeviceLifecycleState::UNINITIALIZED);
    }

    // Perform rapid state changes
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<std::future<int>> futures;
    for (int i = 0; i < numDevices; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i, changesPerDevice]() {
            std::string deviceId = "stress_device_" + std::to_string(i);
            int successfulChanges = 0;

            for (int j = 0; j < changesPerDevice; ++j) {
                DeviceLifecycleState targetState;
                switch (j % 6) {
                    case 0: targetState = DeviceLifecycleState::INITIALIZING; break;
                    case 1: targetState = DeviceLifecycleState::INITIALIZED; break;
                    case 2: targetState = DeviceLifecycleState::CONNECTING; break;
                    case 3: targetState = DeviceLifecycleState::CONNECTED; break;
                    case 4: targetState = DeviceLifecycleState::STARTING; break;
                    case 5: targetState = DeviceLifecycleState::RUNNING; break;
                }

                if (manager_->transitionTo(deviceId, targetState, "STRESS", "Stress test")) {
                    successfulChanges++;
                }
            }

            return successfulChanges;
        }));
    }

    // Wait for completion and collect results
    int totalSuccessfulChanges = 0;
    for (auto& future : futures) {
        totalSuccessfulChanges += future.get();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Verify results
    EXPECT_GT(totalSuccessfulChanges, numDevices * changesPerDevice * 0.7); // At least 70% success rate
    EXPECT_LT(duration.count(), 10000); // Should complete within 10 seconds

    std::cout << "Stress test: " << numDevices << " devices, "
              << totalSuccessfulChanges << " successful changes in "
              << duration.count() << "ms" << std::endl;
}

// Test Integration with External Systems (Mock)
TEST_F(DeviceLifecycleTest, ExternalSystemIntegration) {
    // Mock external system notifications
    std::vector<std::string> externalNotifications;
    std::mutex notificationMutex;

    // Set up callback to simulate external system integration
    manager_->setStateChangeCallback([&externalNotifications, &notificationMutex](const LifecycleEvent& event) {
        std::lock_guard<std::mutex> lock(notificationMutex);

        // Simulate sending notification to external monitoring system
        std::string notification = "EXTERNAL_NOTIFY: Device " + event.deviceId +
                                 " changed from " + lifecycleStateToString(event.previousState) +
                                 " to " + lifecycleStateToString(event.newState);
        externalNotifications.push_back(notification);

        // Simulate external system response based on state
        if (event.newState == DeviceLifecycleState::ERROR) {
            // External system might trigger additional actions
            externalNotifications.push_back("EXTERNAL_ACTION: Alert sent for device " + event.deviceId);
        }
    });

    // Perform lifecycle operations
    manager_->registerDevice(testDeviceId_, DeviceLifecycleState::UNINITIALIZED);
    manager_->transitionTo(testDeviceId_, DeviceLifecycleState::RUNNING, "START", "Start device");
    manager_->forceErrorState(testDeviceId_, "Simulated failure");
    manager_->attemptRecovery(testDeviceId_);

    // Wait for all notifications
    std::this_thread::sleep_for(100ms);

    // Verify external system integration
    {
        std::lock_guard<std::mutex> lock(notificationMutex);
        EXPECT_GE(externalNotifications.size(), 4); // At least 4 notifications

        // Check for error alert
        bool errorAlertFound = false;
        for (const auto& notification : externalNotifications) {
            if (notification.find("EXTERNAL_ACTION: Alert sent") != std::string::npos) {
                errorAlertFound = true;
                break;
            }
        }
        EXPECT_TRUE(errorAlertFound);
    }
}
