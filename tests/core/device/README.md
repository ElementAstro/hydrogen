# Device Lifecycle Tests

This directory contains comprehensive unit tests for the Hydrogen device lifecycle functionality implemented in `src/core/src/device/device_lifecycle.cpp`.

## Overview

The device lifecycle tests provide thorough coverage of all aspects of device state management, including:

- **State Management**: Device registration, state transitions, and validation
- **Error Handling**: Error scenarios, recovery mechanisms, and invalid operations
- **Event Handling**: Lifecycle event callbacks and notifications
- **Resource Management**: Memory management and cleanup during lifecycle operations
- **Concurrent Operations**: Thread safety and concurrent access patterns
- **Configuration Integration**: Dynamic configuration changes and persistence
- **Performance Monitoring**: Integration with performance optimization components

## Test Structure

### Main Test File

- **`test_device_lifecycle.cpp`**: Comprehensive test suite with 25+ test cases covering all functionality

### Test Categories

#### 1. **Basic Functionality Tests**
- `DeviceRegistrationAndUnregistration`: Device registration and cleanup
- `ValidStateTransitions`: Testing valid state transition sequences
- `InvalidStateTransitions`: Testing invalid transitions and validation
- `StateValidationFunctions`: Testing state validation helper functions

#### 2. **Error Handling Tests**
- `ErrorHandlingAndRecovery`: Error state management and recovery
- `ErrorRecoveryScenarios`: Complex error recovery patterns
- `EdgeCasesAndErrorConditions`: Edge cases and boundary conditions

#### 3. **Event System Tests**
- `EventHandlingAndCallbacks`: Lifecycle event callbacks
- `CallbackEventMetadataAndTiming`: Event timing and metadata validation

#### 4. **State Management Tests**
- `StateHistoryManagement`: State transition history tracking
- `HistoryTrimming`: History size management and cleanup
- `ComplexStateTransitionScenarios`: Full lifecycle scenarios

#### 5. **Concurrency Tests**
- `ConcurrentOperations`: Multi-threaded access and safety
- `StressTestingRapidStateChanges`: High-load concurrent operations

#### 6. **Configuration Tests**
- `ConfigurationManagement`: Dynamic configuration updates
- `ConfigurationIntegration`: Configuration change handling
- `PersistenceOperations`: Save/load lifecycle data

#### 7. **Performance Tests**
- `PerformanceAndScalability`: Large-scale operations
- `PerformanceMonitoringIntegration`: Integration with monitoring system
- `MemoryUsageAndLeakPrevention`: Memory management validation

#### 8. **Integration Tests**
- `ExternalSystemIntegration`: Mock external system integration
- `ResourceManagementAndCleanup`: Resource cleanup validation

#### 9. **Utility Tests**
- `HelperFunctions`: State conversion and classification functions
- `JsonSerialization`: JSON serialization/deserialization
- `SingletonBehavior`: Singleton pattern validation

## Device Lifecycle States

The tests cover all device lifecycle states:

### Core States
- **UNINITIALIZED**: Device object created but not initialized
- **INITIALIZING**: Device is being initialized
- **INITIALIZED**: Device initialized but not connected
- **CONNECTING**: Device is attempting to connect
- **CONNECTED**: Device connected but not started
- **STARTING**: Device is starting up
- **RUNNING**: Device is running and operational
- **STOPPING**: Device is stopping
- **STOPPED**: Device stopped but still connected
- **DISCONNECTING**: Device is disconnecting
- **DISCONNECTED**: Device disconnected
- **SHUTDOWN**: Device permanently shut down

### Special States
- **PAUSING/PAUSED/RESUMING**: Pause/resume functionality
- **ERROR**: Device in error state
- **RECOVERING**: Device is recovering from error
- **MAINTENANCE**: Device in maintenance mode
- **UPDATING**: Device firmware/software updating

## Test Execution

### Using CMake

```bash
# Build and run all device lifecycle tests
mkdir build && cd build
cmake .. -DHYDROGEN_BUILD_TESTS=ON
make core_device_lifecycle_tests
./tests/core/core_device_lifecycle_tests

# Run specific test cases
./tests/core/core_device_lifecycle_tests --gtest_filter="*StateTransition*"

# Run with verbose output
./tests/core/core_device_lifecycle_tests --gtest_output=verbose

# Run performance tests
./tests/core/core_device_lifecycle_tests --gtest_filter="*Performance*"
```

### Using XMake

```bash
# Configure and build
xmake config --tests=y --ssl=y --compression=y
xmake build core_tests

# Run tests
xmake test core_tests

# Run in debug mode
xmake config --mode=debug --tests=y
xmake test core_tests
```

### Using Test Runner Scripts

```bash
# Linux/macOS
./scripts/run_device_lifecycle_tests.sh

# Windows
scripts\run_device_lifecycle_tests.bat

# With options
./scripts/run_device_lifecycle_tests.sh -s cmake -t Release -f "*Concurrent*" -v
```

## Test Coverage

### Functional Coverage
- ✅ **Device Registration**: Registration, unregistration, duplicate handling
- ✅ **State Transitions**: All valid transitions, invalid transition blocking
- ✅ **State Validation**: Transition validation, next state queries
- ✅ **Error Handling**: Error states, recovery mechanisms, force error
- ✅ **Event System**: Callbacks, event metadata, timing validation
- ✅ **History Management**: State history, trimming, persistence
- ✅ **Configuration**: Dynamic updates, validation, persistence

### Non-Functional Coverage
- ✅ **Thread Safety**: Concurrent access, race condition prevention
- ✅ **Performance**: Large-scale operations, timing validation
- ✅ **Memory Management**: Leak prevention, resource cleanup
- ✅ **Scalability**: High device counts, rapid state changes
- ✅ **Reliability**: Error recovery, data persistence

### Integration Coverage
- ✅ **Performance Monitoring**: Metrics collection and reporting
- ✅ **Configuration System**: Dynamic configuration updates
- ✅ **External Systems**: Mock external system notifications
- ✅ **Persistence**: Save/load lifecycle data

## Performance Characteristics

The tests validate the following performance characteristics:

### Response Times
- **State Transitions**: < 1ms for in-memory operations
- **Concurrent Access**: Scales linearly with thread count
- **Large Scale**: 1000 devices with 10 transitions each in < 5 seconds

### Memory Usage
- **Memory Efficiency**: No memory leaks during repeated operations
- **History Management**: Automatic trimming prevents unbounded growth
- **Resource Cleanup**: Complete cleanup on device unregistration

### Scalability
- **Device Count**: Tested with 1000+ concurrent devices
- **Transition Rate**: 10,000+ transitions per second
- **Concurrent Threads**: 10+ threads with safe concurrent access

## Error Scenarios Tested

### Invalid Operations
- Transitions on non-existent devices
- Invalid state transitions with strict validation
- Operations on devices in terminal states
- Null/empty parameter handling

### Resource Constraints
- Memory exhaustion scenarios
- History overflow conditions
- Concurrent access limits
- Configuration validation failures

### Recovery Scenarios
- Error state recovery
- Persistence failure handling
- Callback exception handling
- Configuration reload failures

## Integration Points

### Performance Monitoring System
- Metrics collection during lifecycle operations
- Performance timing validation
- Resource utilization tracking
- Scalability measurements

### Configuration Management
- Dynamic configuration updates
- Validation of configuration changes
- Persistence of configuration state
- Configuration reload handling

### External Systems
- Mock external system notifications
- Event propagation to external systems
- Error reporting to monitoring systems
- Integration with alerting systems

## Best Practices Demonstrated

### Test Design
- **Comprehensive Coverage**: All code paths and edge cases
- **Isolation**: Each test is independent and self-contained
- **Repeatability**: Tests produce consistent results
- **Performance**: Tests complete quickly for rapid feedback

### Error Handling
- **Graceful Degradation**: System continues operating during errors
- **Recovery Mechanisms**: Automatic and manual recovery options
- **Error Reporting**: Clear error messages and diagnostic information
- **Resource Cleanup**: Proper cleanup even during error conditions

### Concurrency
- **Thread Safety**: All operations are thread-safe
- **Deadlock Prevention**: No circular dependencies or blocking
- **Performance**: Concurrent operations scale appropriately
- **Data Consistency**: Consistent state across all threads

## Troubleshooting

### Common Issues

1. **Test Timeouts**: Increase timeout values for slow systems
2. **Memory Issues**: Check for proper cleanup in test teardown
3. **Concurrency Failures**: Verify thread safety in implementation
4. **Persistence Failures**: Check file permissions and disk space

### Debug Options

```bash
# Enable verbose logging
export GTEST_OUTPUT=verbose

# Run with memory checking (Linux)
valgrind --tool=memcheck ./tests/core/core_device_lifecycle_tests

# Run with thread sanitizer
export TSAN_OPTIONS="halt_on_error=1"
./tests/core/core_device_lifecycle_tests

# Run specific failing test
./tests/core/core_device_lifecycle_tests --gtest_filter="FailingTestName" --gtest_break_on_failure
```

## Contributing

When adding new device lifecycle functionality:

1. **Add corresponding tests** for all new features
2. **Update existing tests** if behavior changes
3. **Maintain test coverage** above 95%
4. **Document test scenarios** in this README
5. **Verify performance** characteristics are maintained

### Test Naming Convention

- Use descriptive test names that explain the scenario
- Group related tests in the same test fixture
- Use consistent naming patterns for similar test types
- Include performance and error conditions in test names

### Test Structure

- Follow the existing test fixture pattern
- Use proper setup and teardown for resource management
- Include both positive and negative test cases
- Add performance validation where appropriate

## Future Enhancements

Planned improvements to the test suite:

- **Fuzzing Tests**: Random input validation
- **Load Testing**: Extended high-load scenarios
- **Integration Tests**: Real external system integration
- **Benchmark Tests**: Performance regression detection
- **Property-Based Tests**: Automated test case generation
