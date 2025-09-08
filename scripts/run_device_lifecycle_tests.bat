@echo off
setlocal enabledelayedexpansion

REM Device Lifecycle Tests Runner Script for Windows
REM This script provides convenient ways to run the device lifecycle tests
REM with different build systems and configurations

REM Default values
set "BUILD_SYSTEM=cmake"
set "BUILD_TYPE=Debug"
set "VERBOSE=false"
set "FILTER="
set "REPEAT=1"
set "PARALLEL=false"

REM Function to print colored output (simplified for Windows)
:print_status
echo [INFO] %~1
goto :eof

:print_success
echo [SUCCESS] %~1
goto :eof

:print_warning
echo [WARNING] %~1
goto :eof

:print_error
echo [ERROR] %~1
goto :eof

:show_usage
echo Usage: %~nx0 [OPTIONS]
echo.
echo Run device lifecycle tests with various configurations.
echo.
echo OPTIONS:
echo     -s, --system SYSTEM     Build system to use (cmake^|xmake) [default: cmake]
echo     -t, --type TYPE         Build type (Debug^|Release^|RelWithDebInfo) [default: Debug]
echo     -f, --filter PATTERN    Run only tests matching the pattern
echo     -r, --repeat COUNT      Repeat tests COUNT times [default: 1]
echo     -p, --parallel          Run tests in parallel (where supported)
echo     -v, --verbose           Enable verbose output
echo     -h, --help              Show this help message
echo.
echo EXAMPLES:
echo     %~nx0                                          # Run all device lifecycle tests with CMake/Debug
echo     %~nx0 -s xmake -t Release                     # Run with XMake in Release mode
echo     %~nx0 -f "StateTransition*"                   # Run only state transition tests
echo     %~nx0 -r 5 -p                                 # Run tests 5 times in parallel
echo     %~nx0 -v -f "ConcurrentOperations"            # Run concurrency tests with verbose output
echo.
goto :eof

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :args_done
if "%~1"=="-s" (
    set "BUILD_SYSTEM=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--system" (
    set "BUILD_SYSTEM=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="-t" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--type" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="-f" (
    set "FILTER=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--filter" (
    set "FILTER=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="-r" (
    set "REPEAT=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--repeat" (
    set "REPEAT=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="-p" (
    set "PARALLEL=true"
    shift
    goto :parse_args
)
if "%~1"=="--parallel" (
    set "PARALLEL=true"
    shift
    goto :parse_args
)
if "%~1"=="-v" (
    set "VERBOSE=true"
    shift
    goto :parse_args
)
if "%~1"=="--verbose" (
    set "VERBOSE=true"
    shift
    goto :parse_args
)
if "%~1"=="-h" (
    call :show_usage
    exit /b 0
)
if "%~1"=="--help" (
    call :show_usage
    exit /b 0
)
call :print_error "Unknown option: %~1"
call :show_usage
exit /b 1

:args_done

REM Validate build system
if not "%BUILD_SYSTEM%"=="cmake" if not "%BUILD_SYSTEM%"=="xmake" (
    call :print_error "Invalid build system: %BUILD_SYSTEM%. Must be 'cmake' or 'xmake'"
    exit /b 1
)

REM Get script directory and project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

call :print_status "Device Lifecycle Tests Runner"
call :print_status "=============================="
call :print_status "Build System: %BUILD_SYSTEM%"
call :print_status "Build Type: %BUILD_TYPE%"
call :print_status "Project Root: %PROJECT_ROOT%"

if not "%FILTER%"=="" (
    call :print_status "Test Filter: %FILTER%"
)

if not "%REPEAT%"=="1" (
    call :print_status "Repeat Count: %REPEAT%"
)

if "%PARALLEL%"=="true" (
    call :print_status "Parallel Execution: Enabled"
)

echo.

REM Change to project root
cd /d "%PROJECT_ROOT%"

REM Function to run CMake tests
:run_cmake_tests
call :print_status "Building and running tests with CMake..."

REM Create build directory
set "BUILD_DIR=build_test_%BUILD_TYPE%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Configure
call :print_status "Configuring CMake build..."
cmake -B "%BUILD_DIR%" ^
      -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" ^
      -DHYDROGEN_BUILD_TESTS=ON ^
      -DHYDROGEN_BUILD_EXAMPLES=OFF ^
      -DHYDROGEN_ENABLE_SSL=ON ^
      -DHYDROGEN_ENABLE_COMPRESSION=ON

if errorlevel 1 (
    call :print_error "CMake configuration failed"
    exit /b 1
)

REM Build
call :print_status "Building device lifecycle tests..."
cmake --build "%BUILD_DIR%" --target core_device_lifecycle_tests --parallel

if errorlevel 1 (
    call :print_error "Build failed"
    exit /b 1
)

REM Prepare test command
set "TEST_CMD=%BUILD_DIR%\tests\core\core_device_lifecycle_tests.exe"

if "%VERBOSE%"=="true" (
    set "TEST_CMD=!TEST_CMD! --gtest_output=verbose"
)

if not "%FILTER%"=="" (
    set "TEST_CMD=!TEST_CMD! --gtest_filter=%FILTER%"
)

if not "%REPEAT%"=="1" (
    set "TEST_CMD=!TEST_CMD! --gtest_repeat=%REPEAT%"
)

REM Run tests
call :print_status "Running device lifecycle tests..."
!TEST_CMD!

if errorlevel 1 (
    call :print_error "Tests failed"
    exit /b 1
)

goto :eof

REM Function to run XMake tests
:run_xmake_tests
call :print_status "Building and running tests with XMake..."

REM Configure
call :print_status "Configuring XMake build..."
xmake config --mode=%BUILD_TYPE% --tests=y --examples=n --ssl=y --compression=y

if errorlevel 1 (
    call :print_error "XMake configuration failed"
    exit /b 1
)

REM Build
call :print_status "Building core tests..."
xmake build core_tests

if errorlevel 1 (
    call :print_error "Build failed"
    exit /b 1
)

REM Run tests
call :print_status "Running device lifecycle tests..."

if not "%FILTER%"=="" (
    call :print_warning "Test filtering not directly supported with XMake. Running all core tests."
)

if not "%REPEAT%"=="1" (
    for /l %%i in (1,1,%REPEAT%) do (
        call :print_status "Test iteration %%i/%REPEAT%"
        xmake test core_tests
        if errorlevel 1 (
            call :print_error "Test iteration %%i failed"
            exit /b 1
        )
    )
) else (
    xmake test core_tests
    if errorlevel 1 (
        call :print_error "Tests failed"
        exit /b 1
    )
)

goto :eof

REM Main execution
:main
REM Check if we're in the right directory
if not exist "CMakeLists.txt" if not exist "xmake.lua" (
    call :print_error "Not in project root directory. Please run from the Hydrogen project root."
    exit /b 1
)

REM Run tests based on build system
if "%BUILD_SYSTEM%"=="cmake" (
    call :run_cmake_tests
) else if "%BUILD_SYSTEM%"=="xmake" (
    call :run_xmake_tests
)

if errorlevel 1 (
    call :print_error "Device lifecycle tests failed!"
    exit /b 1
)

call :print_success "Device lifecycle tests completed successfully!"

echo.
call :print_status "Additional analysis options:"
echo   - Run performance analysis: %~nx0 -f "*Performance*"
echo   - Run stress tests: %~nx0 -f "*Stress*" -r 10
echo   - Run concurrent tests: %~nx0 -f "*Concurrent*" -p

call :print_status "Test run completed."
exit /b 0

REM Parse arguments and run main
call :parse_args %*
call :main
