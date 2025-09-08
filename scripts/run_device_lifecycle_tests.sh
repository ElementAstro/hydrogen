#!/bin/bash

# Device Lifecycle Tests Runner Script
# This script provides convenient ways to run the device lifecycle tests
# with different build systems and configurations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_SYSTEM="cmake"
BUILD_TYPE="Debug"
VERBOSE=false
FILTER=""
REPEAT=1
PARALLEL=false

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Run device lifecycle tests with various configurations.

OPTIONS:
    -s, --system SYSTEM     Build system to use (cmake|xmake) [default: cmake]
    -t, --type TYPE         Build type (Debug|Release|RelWithDebInfo) [default: Debug]
    -f, --filter PATTERN    Run only tests matching the pattern
    -r, --repeat COUNT      Repeat tests COUNT times [default: 1]
    -p, --parallel          Run tests in parallel (where supported)
    -v, --verbose           Enable verbose output
    -h, --help              Show this help message

EXAMPLES:
    $0                                          # Run all device lifecycle tests with CMake/Debug
    $0 -s xmake -t Release                     # Run with XMake in Release mode
    $0 -f "StateTransition*"                   # Run only state transition tests
    $0 -r 5 -p                                 # Run tests 5 times in parallel
    $0 -v -f "ConcurrentOperations"            # Run concurrency tests with verbose output

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--system)
            BUILD_SYSTEM="$2"
            shift 2
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -f|--filter)
            FILTER="$2"
            shift 2
            ;;
        -r|--repeat)
            REPEAT="$2"
            shift 2
            ;;
        -p|--parallel)
            PARALLEL=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Validate build system
if [[ "$BUILD_SYSTEM" != "cmake" && "$BUILD_SYSTEM" != "xmake" ]]; then
    print_error "Invalid build system: $BUILD_SYSTEM. Must be 'cmake' or 'xmake'"
    exit 1
fi

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

print_status "Device Lifecycle Tests Runner"
print_status "=============================="
print_status "Build System: $BUILD_SYSTEM"
print_status "Build Type: $BUILD_TYPE"
print_status "Project Root: $PROJECT_ROOT"

if [[ -n "$FILTER" ]]; then
    print_status "Test Filter: $FILTER"
fi

if [[ "$REPEAT" -gt 1 ]]; then
    print_status "Repeat Count: $REPEAT"
fi

if [[ "$PARALLEL" == true ]]; then
    print_status "Parallel Execution: Enabled"
fi

echo ""

# Change to project root
cd "$PROJECT_ROOT"

# Function to run CMake tests
run_cmake_tests() {
    print_status "Building and running tests with CMake..."
    
    # Create build directory
    BUILD_DIR="build_test_${BUILD_TYPE,,}"
    mkdir -p "$BUILD_DIR"
    
    # Configure
    print_status "Configuring CMake build..."
    cmake -B "$BUILD_DIR" \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
          -DHYDROGEN_BUILD_TESTS=ON \
          -DHYDROGEN_BUILD_EXAMPLES=OFF \
          -DHYDROGEN_ENABLE_SSL=ON \
          -DHYDROGEN_ENABLE_COMPRESSION=ON
    
    # Build
    print_status "Building device lifecycle tests..."
    cmake --build "$BUILD_DIR" --target core_device_lifecycle_tests --parallel
    
    # Prepare test command
    TEST_CMD="$BUILD_DIR/tests/core/core_device_lifecycle_tests"
    
    if [[ "$VERBOSE" == true ]]; then
        TEST_CMD="$TEST_CMD --gtest_output=verbose"
    fi
    
    if [[ -n "$FILTER" ]]; then
        TEST_CMD="$TEST_CMD --gtest_filter=$FILTER"
    fi
    
    if [[ "$REPEAT" -gt 1 ]]; then
        TEST_CMD="$TEST_CMD --gtest_repeat=$REPEAT"
    fi
    
    # Run tests
    print_status "Running device lifecycle tests..."
    if [[ "$PARALLEL" == true && "$REPEAT" -gt 1 ]]; then
        # Run multiple instances in parallel
        for ((i=1; i<=REPEAT; i++)); do
            (
                echo "Running test iteration $i..."
                $TEST_CMD --gtest_repeat=1
            ) &
        done
        wait
    else
        $TEST_CMD
    fi
}

# Function to run XMake tests
run_xmake_tests() {
    print_status "Building and running tests with XMake..."
    
    # Configure
    print_status "Configuring XMake build..."
    xmake config --mode="$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')" \
                  --tests=y \
                  --examples=n \
                  --ssl=y \
                  --compression=y
    
    # Build
    print_status "Building core tests..."
    xmake build core_tests
    
    # Run tests
    print_status "Running device lifecycle tests..."
    
    if [[ -n "$FILTER" ]]; then
        print_warning "Test filtering not directly supported with XMake. Running all core tests."
    fi
    
    if [[ "$REPEAT" -gt 1 ]]; then
        for ((i=1; i<=REPEAT; i++)); do
            print_status "Test iteration $i/$REPEAT"
            xmake test core_tests
        done
    else
        xmake test core_tests
    fi
}

# Function to run performance analysis
run_performance_analysis() {
    print_status "Running performance analysis..."
    
    if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
        # Run with timing
        BUILD_DIR="build_test_${BUILD_TYPE,,}"
        "$BUILD_DIR/tests/core/core_device_lifecycle_tests" \
            --gtest_filter="*Performance*" \
            --gtest_output=xml:device_lifecycle_performance.xml
    else
        print_warning "Performance analysis not implemented for XMake yet"
    fi
}

# Function to run memory leak detection
run_memory_check() {
    print_status "Running memory leak detection..."
    
    if command -v valgrind >/dev/null 2>&1; then
        if [[ "$BUILD_SYSTEM" == "cmake" ]]; then
            BUILD_DIR="build_test_${BUILD_TYPE,,}"
            valgrind --tool=memcheck \
                     --leak-check=full \
                     --show-leak-kinds=all \
                     --track-origins=yes \
                     --verbose \
                     "$BUILD_DIR/tests/core/core_device_lifecycle_tests" \
                     --gtest_filter="*MemoryUsage*"
        else
            print_warning "Memory check with XMake not implemented yet"
        fi
    else
        print_warning "Valgrind not found. Skipping memory leak detection."
    fi
}

# Main execution
main() {
    # Check if we're in the right directory
    if [[ ! -f "CMakeLists.txt" && ! -f "xmake.lua" ]]; then
        print_error "Not in project root directory. Please run from the Hydrogen project root."
        exit 1
    fi
    
    # Run tests based on build system
    case "$BUILD_SYSTEM" in
        cmake)
            run_cmake_tests
            ;;
        xmake)
            run_xmake_tests
            ;;
    esac
    
    # Check if tests passed
    if [[ $? -eq 0 ]]; then
        print_success "Device lifecycle tests completed successfully!"
        
        # Offer additional analysis
        echo ""
        print_status "Additional analysis options:"
        echo "  - Run performance analysis: $0 -f '*Performance*'"
        echo "  - Run memory leak detection: valgrind available"
        echo "  - Run stress tests: $0 -f '*Stress*' -r 10"
        echo "  - Run concurrent tests: $0 -f '*Concurrent*' -p"
        
    else
        print_error "Device lifecycle tests failed!"
        exit 1
    fi
}

# Run main function
main

print_status "Test run completed."
