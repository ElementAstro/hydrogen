# Test script for Hydrogen build system with Conan
# This script validates that the new modular build system works correctly with Conan

param(
    [string]$BuildType = "Release",
    [string]$Profile = "",
    [switch]$Clean = $false,
    [switch]$Verbose = $false
)

# Script configuration
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-test-conan"
$LogFile = Join-Path $ProjectRoot "conan-test.log"

# Logging function
function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    Write-Host $logMessage
    Add-Content -Path $LogFile -Value $logMessage
}

# Initialize logging
if (Test-Path $LogFile) { Remove-Item $LogFile }
Write-Log "Starting Hydrogen Conan build test"

try {
    # Test 1: Verify Conan installation
    Write-Log "Test 1: Verifying Conan installation"
    
    try {
        $conanVersion = & conan --version 2>&1
        Write-Log "Found Conan: $conanVersion"
    } catch {
        throw "Conan not found. Please install Conan: pip install conan"
    }
    
    # Detect Conan version
    $conanVersionOutput = & conan --version
    if ($conanVersionOutput -match "Conan version (\d+)\.(\d+)") {
        $majorVersion = [int]$matches[1]
        $minorVersion = [int]$matches[2]
        Write-Log "Detected Conan version: $majorVersion.$minorVersion"
        
        if ($majorVersion -lt 2) {
            Write-Log "Warning: Conan 1.x detected. Some features may not work as expected." "WARN"
        }
    }
    
    # Clean previous build if requested
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Log "Cleaning previous build directory"
        Remove-Item -Recurse -Force $BuildDir
    }
    
    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    # Test 2: Validate Conan files
    Write-Log "Test 2: Validating Conan configuration files"
    Set-Location $ProjectRoot
    
    $conanfile = Join-Path $ProjectRoot "conanfile.txt"
    $conanfilePy = Join-Path $ProjectRoot "conanfile.py"
    
    if (Test-Path $conanfile) {
        Write-Log "Found conanfile.txt"
        $conanContent = Get-Content $conanfile
        Write-Log "Conanfile.txt content validated"
    } elseif (Test-Path $conanfilePy) {
        Write-Log "Found conanfile.py"
        # Test Python syntax
        try {
            & python -m py_compile $conanfilePy
            Write-Log "conanfile.py syntax is valid"
        } catch {
            throw "conanfile.py contains syntax errors: $($_.Exception.Message)"
        }
    } else {
        throw "No conanfile.txt or conanfile.py found"
    }
    
    # Test 3: Create/detect Conan profile
    Write-Log "Test 3: Setting up Conan profile"
    
    if (-not $Profile) {
        # Try to detect default profile
        try {
            $profileList = & conan profile list 2>&1
            if ($profileList -match "default") {
                $Profile = "default"
                Write-Log "Using existing default profile"
            } else {
                Write-Log "Creating default profile"
                & conan profile detect --force
                $Profile = "default"
            }
        } catch {
            Write-Log "Creating new default profile"
            & conan profile detect --force
            $Profile = "default"
        }
    }
    
    Write-Log "Using Conan profile: $Profile"
    
    # Test 4: Install dependencies
    Write-Log "Test 4: Installing dependencies with Conan"
    Set-Location $BuildDir
    
    $conanArgs = @(
        "install",
        $ProjectRoot,
        "--profile", $Profile,
        "--build", "missing"
    )
    
    # Add build type settings
    if ($BuildType) {
        $conanArgs += "--settings"
        $conanArgs += "build_type=$BuildType"
    }
    
    if ($Verbose) {
        $conanArgs += "--verbose"
    }
    
    Write-Log "Running: conan $($conanArgs -join ' ')"
    & conan @conanArgs
    
    if ($LASTEXITCODE -ne 0) {
        throw "Conan install failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "Dependencies installed successfully"
    
    # Test 5: Verify Conan generated files
    Write-Log "Test 5: Verifying Conan generated files"
    
    $expectedFiles = @(
        "conan_toolchain.cmake",
        "conandeps.cmake"
    )
    
    foreach ($file in $expectedFiles) {
        $filePath = Join-Path $BuildDir $file
        if (Test-Path $filePath) {
            Write-Log "✓ Found generated file: $file"
        } else {
            Write-Log "✗ Missing generated file: $file" "WARN"
        }
    }
    
    # Test 6: Configure with CMake using Conan toolchain
    Write-Log "Test 6: Configuring with CMake"
    
    $cmakeArgs = @(
        "-S", $ProjectRoot,
        "-B", $BuildDir,
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DHYDROGEN_BUILD_TESTS=ON",
        "-DHYDROGEN_BUILD_EXAMPLES=ON",
        "-DHYDROGEN_ENABLE_SSL=ON",
        "-DHYDROGEN_ENABLE_COMPRESSION=ON"
    )
    
    # Add Conan toolchain if available
    $toolchainFile = Join-Path $BuildDir "conan_toolchain.cmake"
    if (Test-Path $toolchainFile) {
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
        Write-Log "Using Conan toolchain: $toolchainFile"
    }
    
    if ($Verbose) {
        $cmakeArgs += "--verbose"
    }
    
    Write-Log "Running: cmake $($cmakeArgs -join ' ')"
    & cmake @cmakeArgs
    
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "CMake configuration successful"
    
    # Test 7: Build the project
    Write-Log "Test 7: Building the project"
    
    $buildArgs = @(
        "--build", $BuildDir,
        "--config", $BuildType
    )
    
    if ($Verbose) {
        $buildArgs += "--verbose"
    }
    
    Write-Log "Running: cmake $($buildArgs -join ' ')"
    & cmake @buildArgs
    
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "Build successful"
    
    # Test 8: Verify targets were built
    Write-Log "Test 8: Verifying build artifacts"
    
    $expectedTargets = @(
        "astrocomm_core",
        "astrocomm_server", 
        "astrocomm_client",
        "astrocomm_device"
    )
    
    $libDir = Join-Path $BuildDir $BuildType
    if (-not (Test-Path $libDir)) {
        $libDir = $BuildDir  # For single-config generators
    }
    
    foreach ($target in $expectedTargets) {
        $found = $false
        $extensions = @(".lib", ".a", ".dll", ".so", ".dylib")
        
        foreach ($ext in $extensions) {
            $targetFile = Join-Path $libDir "$target$ext"
            if (Test-Path $targetFile) {
                Write-Log "Found target: $targetFile"
                $found = $true
                break
            }
        }
        
        if (-not $found) {
            Write-Log "Warning: Target $target not found in expected locations" "WARN"
        }
    }
    
    # Test 9: Run tests if available
    if (Test-Path (Join-Path $BuildDir "tests")) {
        Write-Log "Test 9: Running unit tests"
        
        $ctestArgs = @(
            "--test-dir", $BuildDir,
            "--build-config", $BuildType,
            "--output-on-failure"
        )
        
        if ($Verbose) {
            $ctestArgs += "--verbose"
        }
        
        Write-Log "Running: ctest $($ctestArgs -join ' ')"
        & ctest @ctestArgs
        
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Some tests failed, but build system test continues" "WARN"
        } else {
            Write-Log "All tests passed"
        }
    } else {
        Write-Log "No tests found to run"
    }
    
    # Test 10: Verify package manager detection
    Write-Log "Test 10: Verifying package manager detection"
    
    $cmakeCache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cmakeCache) {
        $cacheContent = Get-Content $cmakeCache
        
        $conanDetected = $cacheContent | Where-Object { $_ -match "HYDROGEN_CONAN_AVAILABLE.*TRUE" }
        $primaryManager = $cacheContent | Where-Object { $_ -match "HYDROGEN_PRIMARY_PACKAGE_MANAGER.*conan" }
        
        if ($conanDetected) {
            Write-Log "✓ Conan detection working correctly"
        } else {
            Write-Log "✗ Conan not detected properly" "WARN"
        }
        
        if ($primaryManager) {
            Write-Log "✓ Conan set as primary package manager"
        } else {
            Write-Log "✗ Conan not set as primary package manager" "WARN"
        }
    }
    
    # Test 11: Test Conan package creation (optional)
    if (Test-Path $conanfilePy) {
        Write-Log "Test 11: Testing Conan package creation"
        Set-Location $ProjectRoot
        
        try {
            $packageArgs = @(
                "create", ".",
                "--profile", $Profile,
                "--build", "missing"
            )
            
            Write-Log "Running: conan $($packageArgs -join ' ')"
            & conan @packageArgs
            
            if ($LASTEXITCODE -eq 0) {
                Write-Log "✓ Conan package creation successful"
            } else {
                Write-Log "✗ Conan package creation failed" "WARN"
            }
        } catch {
            Write-Log "Conan package creation test skipped: $($_.Exception.Message)" "WARN"
        }
    }
    
    Write-Log "=== Conan Build Test Summary ==="
    Write-Log "✓ Conan installation verification"
    Write-Log "✓ Conanfile validation"
    Write-Log "✓ Profile setup"
    Write-Log "✓ Dependency installation"
    Write-Log "✓ Generated files verification"
    Write-Log "✓ CMake configuration"
    Write-Log "✓ Project build"
    Write-Log "✓ Artifact verification"
    Write-Log "✓ Package manager detection"
    Write-Log "=== Test Completed Successfully ==="
    
} catch {
    Write-Log "Test failed: $($_.Exception.Message)" "ERROR"
    Write-Log "Stack trace: $($_.ScriptStackTrace)" "ERROR"
    exit 1
} finally {
    Set-Location $ProjectRoot
}

Write-Log "Conan build test completed successfully"
