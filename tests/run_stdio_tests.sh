#!/bin/bash

# Stdio Communication Test Runner Script
# This script runs all stdio communication tests with various configurations

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
TEST_TIMEOUT="${TEST_TIMEOUT:-60}"
VERBOSE="${VERBOSE:-0}"

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

# Function to run a test with timeout and error handling
run_test() {
    local test_name="$1"
    local test_executable="$2"
    local timeout="${3:-$TEST_TIMEOUT}"
    
    print_status "Running $test_name..."
    
    if [ ! -f "$test_executable" ]; then
        print_error "Test executable not found: $test_executable"
        return 1
    fi
    
    local start_time=$(date +%s)
    
    if [ "$VERBOSE" -eq 1 ]; then
        timeout "$timeout" "$test_executable" --gtest_output=xml:"${test_name}_results.xml"
    else
        timeout "$timeout" "$test_executable" --gtest_output=xml:"${test_name}_results.xml" > "${test_name}_output.log" 2>&1
    fi
    
    local exit_code=$?
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    if [ $exit_code -eq 0 ]; then
        print_success "$test_name completed successfully in ${duration}s"
        return 0
    elif [ $exit_code -eq 124 ]; then
        print_error "$test_name timed out after ${timeout}s"
        return 1
    else
        print_error "$test_name failed with exit code $exit_code"
        if [ "$VERBOSE" -eq 0 ] && [ -f "${test_name}_output.log" ]; then
            echo "Last 20 lines of output:"
            tail -20 "${test_name}_output.log"
        fi
        return 1
    fi
}

# Function to check build directory and executables
check_build() {
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory not found: $BUILD_DIR"
        print_status "Please build the project first:"
        print_status "  mkdir -p $BUILD_DIR && cd $BUILD_DIR"
        print_status "  cmake .. -DBUILD_TESTS=ON"
        print_status "  make -j\$(nproc)"
        exit 1
    fi
    
    cd "$BUILD_DIR"
    
    if [ ! -f "tests/test_stdio_communicator" ]; then
        print_error "Test executables not found. Please build tests first:"
        print_status "  make test_stdio_all"
        exit 1
    fi
}

# Function to run unit tests
run_unit_tests() {
    print_status "Running stdio unit tests..."
    
    local tests=(
        "stdio_communicator:tests/test_stdio_communicator"
        "stdio_message_transformer:tests/test_stdio_message_transformer"
        "stdio_server:tests/test_stdio_server"
    )
    
    local failed_tests=()
    
    for test_info in "${tests[@]}"; do
        IFS=':' read -r test_name test_path <<< "$test_info"
        
        if ! run_test "$test_name" "$test_path" 30; then
            failed_tests+=("$test_name")
        fi
    done
    
    if [ ${#failed_tests[@]} -eq 0 ]; then
        print_success "All unit tests passed!"
        return 0
    else
        print_error "Failed unit tests: ${failed_tests[*]}"
        return 1
    fi
}

# Function to run integration tests
run_integration_tests() {
    print_status "Running stdio integration tests..."
    
    if ! run_test "stdio_integration" "tests/test_stdio_integration" 120; then
        print_error "Integration tests failed"
        return 1
    fi
    
    print_success "Integration tests passed!"
    return 0
}

# Function to run performance tests
run_performance_tests() {
    print_status "Running stdio performance tests..."
    
    # Run specific performance test cases
    local perf_tests=(
        "stdio_communicator:tests/test_stdio_communicator:--gtest_filter=*Performance*"
        "stdio_server:tests/test_stdio_server:--gtest_filter=*Performance*"
        "stdio_integration:tests/test_stdio_integration:--gtest_filter=*Stress*"
    )
    
    for test_info in "${perf_tests[@]}"; do
        IFS=':' read -r test_name test_path test_filter <<< "$test_info"
        
        print_status "Running performance test: $test_name"
        if [ -n "$test_filter" ]; then
            timeout 180 "$test_path" "$test_filter" || print_warning "Performance test $test_name had issues"
        fi
    done
    
    print_success "Performance tests completed!"
}

# Function to generate test report
generate_report() {
    print_status "Generating test report..."
    
    local report_file="stdio_test_report.html"
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Stdio Communication Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; border-radius: 5px; }
        .success { color: green; }
        .error { color: red; }
        .warning { color: orange; }
        .test-section { margin: 20px 0; padding: 10px; border: 1px solid #ddd; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Stdio Communication Test Report</h1>
        <p>Generated on: $(date)</p>
        <p>Build Directory: $BUILD_DIR</p>
    </div>
    
    <div class="test-section">
        <h2>Test Results</h2>
EOF
    
    # Add XML test results if available
    for xml_file in *_results.xml; do
        if [ -f "$xml_file" ]; then
            echo "        <h3>$xml_file</h3>" >> "$report_file"
            echo "        <pre>$(cat "$xml_file")</pre>" >> "$report_file"
        fi
    done
    
    cat >> "$report_file" << EOF
    </div>
</body>
</html>
EOF
    
    print_success "Test report generated: $report_file"
}

# Function to clean up test artifacts
cleanup() {
    print_status "Cleaning up test artifacts..."
    rm -f *_output.log *_results.xml
    print_success "Cleanup completed"
}

# Main function
main() {
    local run_unit=1
    local run_integration=1
    local run_performance=0
    local generate_report_flag=0
    local cleanup_flag=0
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --unit-only)
                run_integration=0
                shift
                ;;
            --integration-only)
                run_unit=0
                shift
                ;;
            --performance)
                run_performance=1
                shift
                ;;
            --report)
                generate_report_flag=1
                shift
                ;;
            --cleanup)
                cleanup_flag=1
                shift
                ;;
            --verbose|-v)
                VERBOSE=1
                shift
                ;;
            --timeout)
                TEST_TIMEOUT="$2"
                shift 2
                ;;
            --build-dir)
                BUILD_DIR="$2"
                shift 2
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo "Options:"
                echo "  --unit-only          Run only unit tests"
                echo "  --integration-only   Run only integration tests"
                echo "  --performance        Run performance tests"
                echo "  --report             Generate HTML test report"
                echo "  --cleanup            Clean up test artifacts"
                echo "  --verbose, -v        Verbose output"
                echo "  --timeout SECONDS    Test timeout (default: 60)"
                echo "  --build-dir DIR      Build directory (default: build)"
                echo "  --help, -h           Show this help"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    print_status "Starting stdio communication tests..."
    print_status "Build directory: $BUILD_DIR"
    print_status "Test timeout: ${TEST_TIMEOUT}s"
    
    # Check build environment
    check_build
    
    local overall_success=1
    
    # Run tests based on options
    if [ $run_unit -eq 1 ]; then
        if ! run_unit_tests; then
            overall_success=0
        fi
    fi
    
    if [ $run_integration -eq 1 ]; then
        if ! run_integration_tests; then
            overall_success=0
        fi
    fi
    
    if [ $run_performance -eq 1 ]; then
        run_performance_tests
    fi
    
    # Generate report if requested
    if [ $generate_report_flag -eq 1 ]; then
        generate_report
    fi
    
    # Cleanup if requested
    if [ $cleanup_flag -eq 1 ]; then
        cleanup
    fi
    
    # Final status
    if [ $overall_success -eq 1 ]; then
        print_success "All stdio communication tests completed successfully!"
        exit 0
    else
        print_error "Some tests failed. Check the output above for details."
        exit 1
    fi
}

# Run main function with all arguments
main "$@"
