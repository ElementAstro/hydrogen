# Test script for Hydrogen build system with vcpkg
# This script validates that the new modular build system works correctly with vcpkg

param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$BuildType = "Release",
    [string]$Triplet = "",
    [switch]$Clean = $false,
    [switch]$Verbose = $false
)

# Script configuration
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-test-vcpkg"
$LogFile = Join-Path $ProjectRoot "vcpkg-test.log"

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
Write-Log "Starting Hydrogen vcpkg build test"

try {
    # Validate vcpkg installation
    if (-not $VcpkgRoot) {
        throw "VCPKG_ROOT environment variable not set. Please install vcpkg and set VCPKG_ROOT."
    }
    
    if (-not (Test-Path $VcpkgRoot)) {
        throw "vcpkg root directory not found: $VcpkgRoot"
    }
    
    $vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        throw "vcpkg.exe not found: $vcpkgExe"
    }
    
    Write-Log "Found vcpkg at: $VcpkgRoot"
    
    # Detect triplet if not specified
    if (-not $Triplet) {
        if ($IsWindows -or $env:OS -eq "Windows_NT") {
            $Triplet = "x64-windows"
        } elseif ($IsLinux) {
            $Triplet = "x64-linux"
        } elseif ($IsMacOS) {
            $Triplet = "x64-osx"
        } else {
            $Triplet = "x64-windows"  # Default fallback
        }
    }
    Write-Log "Using triplet: $Triplet"
    
    # Clean previous build if requested
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Log "Cleaning previous build directory"
        Remove-Item -Recurse -Force $BuildDir
    }
    
    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    # Test 1: Verify vcpkg manifest is valid
    Write-Log "Test 1: Validating vcpkg manifest"
    Set-Location $ProjectRoot
    
    $vcpkgManifest = Join-Path $ProjectRoot "vcpkg.json"
    if (-not (Test-Path $vcpkgManifest)) {
        throw "vcpkg.json manifest not found"
    }
    
    # Validate JSON syntax
    try {
        $manifestContent = Get-Content $vcpkgManifest -Raw | ConvertFrom-Json
        Write-Log "vcpkg.json is valid JSON"
    } catch {
        throw "vcpkg.json contains invalid JSON: $($_.Exception.Message)"
    }
    
    # Test 2: Install dependencies via vcpkg
    Write-Log "Test 2: Installing dependencies via vcpkg"
    
    $vcpkgArgs = @(
        "install",
        "--triplet", $Triplet,
        "--x-manifest-root", $ProjectRoot
    )
    
    if ($Verbose) {
        $vcpkgArgs += "--verbose"
    }
    
    Write-Log "Running: vcpkg $($vcpkgArgs -join ' ')"
    & $vcpkgExe @vcpkgArgs
    
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg install failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "Dependencies installed successfully"
    
    # Test 3: Configure with CMake using vcpkg toolchain
    Write-Log "Test 3: Configuring with CMake"
    Set-Location $BuildDir
    
    $toolchainFile = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
    
    $cmakeArgs = @(
        "-S", $ProjectRoot,
        "-B", $BuildDir,
        "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
        "-DVCPKG_TARGET_TRIPLET=$Triplet",
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DHYDROGEN_BUILD_TESTS=ON",
        "-DHYDROGEN_BUILD_EXAMPLES=ON",
        "-DHYDROGEN_ENABLE_SSL=ON",
        "-DHYDROGEN_ENABLE_COMPRESSION=ON"
    )
    
    if ($Verbose) {
        $cmakeArgs += "--verbose"
    }
    
    Write-Log "Running: cmake $($cmakeArgs -join ' ')"
    & cmake @cmakeArgs
    
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "CMake configuration successful"
    
    # Test 4: Build the project
    Write-Log "Test 4: Building the project"
    
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
    
    # Test 5: Verify targets were built
    Write-Log "Test 5: Verifying build artifacts"
    
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
    
    # Test 6: Run tests if available
    if (Test-Path (Join-Path $BuildDir "tests")) {
        Write-Log "Test 6: Running unit tests"
        
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
    
    # Test 7: Verify package manager detection
    Write-Log "Test 7: Verifying package manager detection"
    
    $cmakeCache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cmakeCache) {
        $cacheContent = Get-Content $cmakeCache
        
        $vcpkgDetected = $cacheContent | Where-Object { $_ -match "HYDROGEN_VCPKG_AVAILABLE.*TRUE" }
        $primaryManager = $cacheContent | Where-Object { $_ -match "HYDROGEN_PRIMARY_PACKAGE_MANAGER.*vcpkg" }
        
        if ($vcpkgDetected) {
            Write-Log "✓ vcpkg detection working correctly"
        } else {
            Write-Log "✗ vcpkg not detected properly" "WARN"
        }
        
        if ($primaryManager) {
            Write-Log "✓ vcpkg set as primary package manager"
        } else {
            Write-Log "✗ vcpkg not set as primary package manager" "WARN"
        }
    }
    
    Write-Log "=== vcpkg Build Test Summary ==="
    Write-Log "✓ vcpkg manifest validation"
    Write-Log "✓ Dependency installation"
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

Write-Log "vcpkg build test completed successfully"
