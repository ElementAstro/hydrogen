# AstroComm Core Component

The Core component provides the fundamental shared functionality for the AstroComm library. It contains message handling, utilities, and error recovery mechanisms that are used by both server and client components.

## Features

### Message System

- **Message Types**: Support for various message types including commands, responses, events, errors, discovery, registration, and authentication
- **QoS Support**: Quality of Service levels (AT_MOST_ONCE, AT_LEAST_ONCE, EXACTLY_ONCE)
- **Priority Handling**: Message priority levels (LOW, NORMAL, HIGH, CRITICAL)
- **Expiration**: Message expiration support with automatic cleanup

### Message Queue Management

- **Priority Queue**: Messages are processed based on priority and timestamp
- **Retry Logic**: Automatic retry with exponential backoff for failed messages
- **Acknowledgments**: Support for message acknowledgments and tracking
- **Statistics**: Built-in statistics tracking for sent, acknowledged, and failed messages

### Utilities

- **UUID Generation**: Generate universally unique identifiers
- **Timestamp Handling**: ISO 8601 timestamp generation and parsing
- **String Utilities**: Common string manipulation functions (trim, case conversion, splitting)
- **Boolean Parsing**: Flexible boolean value parsing

### Error Recovery

- **Error Handling Strategies**: Multiple strategies (IGNORE, RETRY, NOTIFY, RESTART_DEVICE, FAILOVER, CUSTOM)
- **Device-Specific Handlers**: Register error handlers for specific devices
- **Global Handlers**: Fallback error handling for unhandled errors
- **Statistics**: Error tracking and reporting

## Usage

### Basic Usage

```cpp
#include <astrocomm/core.h>

// Initialize the core component
astrocomm::core::initialize();

// Create a message
auto command = std::make_unique<astrocomm::core::CommandMessage>("get_status");
command->setDeviceId("telescope_01");
command->setPriority(astrocomm::core::Message::Priority::HIGH);

// Use utilities
std::string uuid = astrocomm::core::generateUuid();
std::string timestamp = astrocomm::core::getIsoTimestamp();

// Cleanup when done
astrocomm::core::cleanup();
```

### Message Queue Usage

```cpp
#include <astrocomm/core/message_queue.h>

// Create a message queue with a send callback
auto sendCallback = [](const astrocomm::core::Message& msg) -> bool {
    // Implement actual message sending logic
    return true;
};

astrocomm::core::MessageQueueManager queue(sendCallback);
queue.start();

// Enqueue a message
auto message = std::make_unique<astrocomm::core::CommandMessage>("connect");
queue.enqueue(std::move(message));

// Stop the queue when done
queue.stop();
```

### Error Recovery Usage

```cpp
#include <astrocomm/core/error_recovery.h>

astrocomm::core::ErrorRecoveryManager errorManager;

// Register an error handler
errorManager.registerErrorHandler("CONNECTION_FAILED",
    astrocomm::core::ErrorHandlingStrategy::RETRY);

// Handle an error
astrocomm::core::ErrorMessage errorMsg("CONNECTION_FAILED", "Failed to connect to device");
bool handled = errorManager.handleError(errorMsg);
```

## Public Interface

### Headers

- `astrocomm/core.h` - Main header including all core functionality
- `astrocomm/core/message.h` - Message system classes and types
- `astrocomm/core/message_queue.h` - Message queue management
- `astrocomm/core/utils.h` - Utility functions
- `astrocomm/core/error_recovery.h` - Error handling and recovery

### Namespaces

All core functionality is contained within the `astrocomm::core` namespace.

### Dependencies

- **nlohmann/json**: JSON serialization and deserialization
- **Standard C++17**: Threading, chrono, containers
- **Optional Boost**: Regex support (if USE_BOOST_REGEX is defined)

## CMake Integration

To use the core component in your CMake project:

```cmake
find_package(AstroComm REQUIRED COMPONENTS Core)
target_link_libraries(your_target PRIVATE AstroComm::Core)
```

## Thread Safety

- **Message Classes**: Thread-safe for read operations, external synchronization required for modifications
- **MessageQueueManager**: Fully thread-safe
- **ErrorRecoveryManager**: Thread-safe with shared_mutex for read/write operations
- **Utilities**: Thread-safe (stateless functions)

## Performance Considerations

- Message serialization uses nlohmann/json which is efficient but not the fastest option
- Message queue uses priority queue with O(log n) insertion/removal
- Error recovery uses maps for handler lookup with O(log n) complexity
- String utilities avoid unnecessary allocations where possible
