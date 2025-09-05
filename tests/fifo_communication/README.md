# FIFO Communication Tests

This directory contains comprehensive tests for the Hydrogen FIFO (First-In-First-Out) communication system. The test suite covers unit tests, integration tests, and performance benchmarks to ensure the reliability, correctness, and performance of the FIFO communication implementation.

## Test Structure

### Test Categories

1. **Unit Tests** - Test individual components in isolation
   - Configuration system tests
   - Communicator functionality tests
   - Message transformer tests
   - Logger tests

2. **Integration Tests** - Test component interactions
   - Server-client communication
   - Multi-client scenarios
   - Protocol handler integration
   - End-to-end message flow

3. **Performance Tests** - Benchmark system performance
   - Throughput measurements
   - Latency analysis
   - Memory usage tests
   - Concurrent operation tests

### Test Files

- **`test_fifo_config.cpp`** - Configuration system unit tests
- **`test_fifo_communicator.cpp`** - Communicator unit tests
- **`test_fifo_integration.cpp`** - Integration tests
- **`test_fifo_performance.cpp`** - Performance benchmarks
- **`CMakeLists.txt`** - Build configuration for tests
- **`README.md`** - This documentation file

## Building and Running Tests

### Prerequisites

- CMake 3.16 or later
- Google Test (GTest) framework
- nlohmann/json library
- C++17 compatible compiler
- Platform-specific requirements:
  - Unix/Linux: POSIX named pipes support
  - Windows: Named pipes support (Windows Vista or later)

### Building Tests

```bash
# From the hydrogen project root
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make fifo_tests
```

### Running Tests

#### Run All FIFO Tests
```bash
make run_all_fifo_tests
# or
ctest -L "fifo" --output-on-failure
```

#### Run Specific Test Categories
```bash
# Unit tests only
make run_fifo_unit_tests
ctest -L "unit" --output-on-failure

# Integration tests only
make run_fifo_integration_tests
ctest -L "integration" --output-on-failure

# Performance tests only
make run_fifo_performance_tests
ctest -L "performance" --output-on-failure
```

#### Run Individual Test Executables
```bash
# Configuration tests
./tests/test_fifo_config

# Communicator tests
./tests/test_fifo_communicator

# Integration tests
./tests/test_fifo_integration

# Performance tests
./tests/test_fifo_performance
```

## Test Details

### Configuration Tests (`test_fifo_config.cpp`)

Tests the FIFO configuration system including:

- **Configuration Creation**: Default and preset configurations
- **Serialization**: JSON serialization/deserialization
- **Validation**: Configuration validation and error reporting
- **Merging**: Configuration merging and overrides
- **Platform Optimization**: Platform-specific optimizations
- **File I/O**: Configuration file loading/saving
- **Schema Validation**: Configuration schema compliance

**Key Test Cases:**
- `CreateDefaultConfig` - Basic configuration creation
- `CreatePresetConfigs` - All preset configurations
- `ConfigSerialization` - JSON serialization round-trip
- `ConfigValidation` - Validation with invalid configurations
- `ConfigMerging` - Configuration merging functionality
- `PlatformOptimizedConfig` - Platform-specific settings

### Communicator Tests (`test_fifo_communicator.cpp`)

Tests the FIFO communicator functionality including:

- **Lifecycle Management**: Start/stop operations
- **Message Handling**: Send/receive operations
- **Event Handling**: Connection, message, and error events
- **Statistics Collection**: Performance metrics
- **Health Monitoring**: Health status reporting
- **Configuration Updates**: Runtime configuration changes

**Key Test Cases:**
- `CreateCommunicator` - Basic communicator creation
- `CommunicatorLifecycle` - Start/stop lifecycle
- `EventHandlers` - Event callback functionality
- `StatisticsCollection` - Performance metrics
- `MessageSizeValidation` - Message size limits
- `ConnectionStateManagement` - Connection state tracking

### Integration Tests (`test_fifo_integration.cpp`)

Tests the complete FIFO system integration including:

- **Server-Client Communication**: End-to-end message flow
- **Multi-Client Support**: Multiple concurrent clients
- **Client Management**: Connection/disconnection handling
- **Command Processing**: Server command handling
- **Error Handling**: Error propagation and recovery
- **Performance Monitoring**: System-wide metrics

**Key Test Cases:**
- `BasicServerClientCommunication` - Basic message exchange
- `MultipleClients` - Multiple client handling
- `ClientLimitEnforcement` - Connection limit enforcement
- `CommandFiltering` - Command validation
- `ServerStatistics` - Server metrics collection
- `ConcurrentOperations` - Thread safety

### Performance Tests (`test_fifo_performance.cpp`)

Benchmarks the FIFO system performance including:

- **Throughput Testing**: Message/second measurements
- **Latency Analysis**: Message processing delays
- **Memory Usage**: Memory consumption patterns
- **Concurrent Performance**: Multi-threaded operations
- **Configuration Impact**: Performance vs. configuration
- **Stress Testing**: High-load scenarios

**Key Test Cases:**
- `MessageSendingPerformance` - Throughput benchmarks
- `MessageSizePerformance` - Size vs. performance analysis
- `ConcurrentCommunicatorsPerformance` - Multi-threaded tests
- `ServerMultiClientPerformance` - Server scalability
- `ConfigurationImpactTest` - Configuration performance impact
- `StartStopStressTest` - Lifecycle stress testing

## Test Configuration

### Environment Variables

- `FIFO_TEST_TIMEOUT` - Test timeout in seconds (default: varies by test)
- `FIFO_TEST_VERBOSE` - Enable verbose test output (0/1)
- `FIFO_TEST_PIPE_PATH` - Custom pipe path for tests
- `FIFO_TEST_SKIP_PERFORMANCE` - Skip performance tests (0/1)

### Test Data

Tests use temporary files and pipes that are automatically cleaned up:

- Unix: `/tmp/test_fifo_*`
- Windows: `\\.\pipe\test_fifo_*`

### Platform-Specific Notes

#### Unix/Linux
- Tests create temporary FIFO files in `/tmp/`
- Requires write permissions to `/tmp/`
- May require elevated privileges for some permission tests

#### Windows
- Uses Windows named pipes
- Requires Windows Vista or later
- May require administrator privileges for some tests

#### macOS
- Similar to Linux with some BSD-specific behaviors
- May require additional permissions for `/tmp/` access

## Advanced Testing

### Memory Testing with Valgrind

```bash
make fifo_memcheck
```

### Address Sanitizer

```bash
make fifo_tests_asan
```

### Thread Sanitizer

```bash
make fifo_tests_tsan
```

### Coverage Analysis

```bash
make fifo_coverage
# Results in build/fifo_coverage_html/
```

## Continuous Integration

### GitHub Actions Example

```yaml
name: FIFO Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake libgtest-dev nlohmann-json3-dev
    - name: Build and test
      run: |
        mkdir build && cd build
        cmake .. -DBUILD_TESTS=ON
        make fifo_tests
        ctest -L "fifo" --output-on-failure
```

### Test Metrics

The test suite tracks several metrics:

- **Test Coverage**: Line and branch coverage
- **Performance Baselines**: Throughput and latency benchmarks
- **Memory Usage**: Peak memory consumption
- **Test Duration**: Execution time for each test suite

## Troubleshooting

### Common Issues

1. **Permission Denied**
   - Ensure write access to temporary directories
   - Check file permissions on Unix systems
   - Run with appropriate privileges

2. **Pipe Already Exists**
   - Previous test run may not have cleaned up properly
   - Manually remove stale pipe files
   - Use unique test identifiers

3. **Timeout Errors**
   - Increase test timeout values
   - Check system load and available resources
   - Verify network connectivity for remote tests

4. **Memory Leaks**
   - Run with Valgrind to identify leaks
   - Check proper resource cleanup in tests
   - Verify communicator shutdown procedures

### Debug Mode

Enable debug output for troubleshooting:

```bash
export FIFO_TEST_VERBOSE=1
./tests/test_fifo_integration --gtest_verbose
```

### Test Isolation

Each test uses unique identifiers to avoid conflicts:

- Unique pipe names per test
- Separate temporary directories
- Independent configuration files

## Contributing

When adding new tests:

1. Follow the existing test structure and naming conventions
2. Add appropriate test labels for categorization
3. Include both positive and negative test cases
4. Add performance tests for new features
5. Update this documentation

### Test Guidelines

- Use descriptive test names
- Include setup and teardown for resource management
- Test both success and failure scenarios
- Verify error handling and edge cases
- Add performance benchmarks for new features

## Performance Baselines

Current performance baselines (may vary by system):

- **Message Throughput**: >1000 messages/second (1KB messages)
- **Connection Setup**: <100ms per connection
- **Memory Usage**: <10MB for 100 concurrent clients
- **Latency**: <1ms for local communication

These baselines are used in performance tests to detect regressions.

## License

These tests are part of the Hydrogen project and are subject to the project's license terms.
