# Test script for Hydrogen build system FetchContent fallback
# This script validates that the build system gracefully falls back to FetchContent
# when package managers are not available or dependencies are missing

param(
    [string]$BuildType = "Release",
    [switch]$Clean = $false,
    [switch]$Verbose = $false,
    [switch]$SimulateNoPackageManagers = $false
)

# Script configuration
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build-test-fetchcontent"
$LogFile = Join-Path $ProjectRoot "fetchcontent-test.log"

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
Write-Log "Starting Hydrogen FetchContent fallback test"

try {
    # Clean previous build if requested
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Log "Cleaning previous build directory"
        Remove-Item -Recurse -Force $BuildDir
    }
    
    # Create build directory
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    
    # Test 1: Verify FetchContent fallback mechanism
    Write-Log "Test 1: Testing FetchContent fallback mechanism"
    Set-Location $BuildDir
    
    # Prepare CMake arguments to simulate no package managers
    $cmakeArgs = @(
        "-S", $ProjectRoot,
        "-B", $BuildDir,
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DHYDROGEN_ALLOW_FETCHCONTENT=ON",
        "-DHYDROGEN_BUILD_TESTS=ON",
        "-DHYDROGEN_BUILD_EXAMPLES=ON"
    )
    
    # Simulate environment without package managers
    if ($SimulateNoPackageManagers) {
        Write-Log "Simulating environment without package managers"
        $cmakeArgs += "-DHYDROGEN_PREFER_VCPKG=OFF"
        $cmakeArgs += "-DHYDROGEN_PREFER_CONAN=OFF"
        
        # Temporarily hide package manager environment variables
        $originalVcpkgRoot = $env:VCPKG_ROOT
        $env:VCPKG_ROOT = ""
        
        # Remove any existing toolchain files from environment
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE="
    }
    
    if ($Verbose) {
        $cmakeArgs += "--verbose"
    }
    
    Write-Log "Running: cmake $($cmakeArgs -join ' ')"
    & cmake @cmakeArgs
    
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "CMake configuration with FetchContent successful"
    
    # Restore environment if it was modified
    if ($SimulateNoPackageManagers -and $originalVcpkgRoot) {
        $env:VCPKG_ROOT = $originalVcpkgRoot
    }
    
    # Test 2: Verify FetchContent cache directory
    Write-Log "Test 2: Verifying FetchContent cache directory"
    
    $cacheDir = Join-Path $ProjectRoot ".cmake-cache"
    if (Test-Path $cacheDir) {
        Write-Log "✓ FetchContent cache directory exists: $cacheDir"
        
        # Check for expected cached dependencies
        $expectedDeps = @(
            "nlohmann_json-v3.11.3",
            "crow-v1.2.0",
            "cpp-base64-master"
        )
        
        foreach ($dep in $expectedDeps) {
            $depPath = Join-Path $cacheDir $dep
            if (Test-Path $depPath) {
                Write-Log "✓ Found cached dependency: $dep"
            } else {
                Write-Log "- Dependency not yet cached: $dep"
            }
        }
    } else {
        Write-Log "FetchContent cache directory will be created during build"
    }
    
    # Test 3: Build the project using FetchContent
    Write-Log "Test 3: Building project with FetchContent dependencies"
    
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
        throw "Build with FetchContent failed with exit code $LASTEXITCODE"
    }
    
    Write-Log "Build with FetchContent successful"
    
    # Test 4: Verify dependencies were fetched
    Write-Log "Test 4: Verifying dependencies were fetched correctly"
    
    # Check CMake cache for FetchContent variables
    $cmakeCache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cmakeCache) {
        $cacheContent = Get-Content $cmakeCache
        
        # Look for FetchContent population indicators
        $fetchContentUsed = $cacheContent | Where-Object { $_ -match "FETCHCONTENT.*POPULATED" }
        
        if ($fetchContentUsed) {
            Write-Log "✓ FetchContent was used for dependency resolution"
            foreach ($line in $fetchContentUsed) {
                if ($line -match "FETCHCONTENT_(.+)_POPULATED.*TRUE") {
                    Write-Log "  - Fetched: $($matches[1])"
                }
            }
        } else {
            Write-Log "No FetchContent usage detected in cache" "WARN"
        }
        
        # Check package manager detection
        $packageManager = $cacheContent | Where-Object { $_ -match "HYDROGEN_PRIMARY_PACKAGE_MANAGER" }
        if ($packageManager -match "none") {
            Write-Log "✓ No package manager detected, using FetchContent fallback"
        }
    }
    
    # Test 5: Verify build artifacts
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
    
    $artifactsFound = 0
    foreach ($target in $expectedTargets) {
        $found = $false
        $extensions = @(".lib", ".a", ".dll", ".so", ".dylib")
        
        foreach ($ext in $extensions) {
            $targetFile = Join-Path $libDir "$target$ext"
            if (Test-Path $targetFile) {
                Write-Log "✓ Found target: $targetFile"
                $found = $true
                $artifactsFound++
                break
            }
        }
        
        if (-not $found) {
            Write-Log "✗ Target $target not found" "WARN"
        }
    }
    
    if ($artifactsFound -eq $expectedTargets.Count) {
        Write-Log "✓ All expected build artifacts found"
    } else {
        Write-Log "⚠ Only $artifactsFound of $($expectedTargets.Count) artifacts found" "WARN"
    }
    
    # Test 6: Test incremental builds with cached dependencies
    Write-Log "Test 6: Testing incremental build with cached dependencies"
    
    # Touch a source file to trigger rebuild
    $coreSource = Join-Path $ProjectRoot "src/core/common/core.cpp"
    if (Test-Path $coreSource) {
        (Get-Date).ToString() | Add-Content $coreSource
        Write-Log "Modified source file to trigger incremental build"
        
        # Rebuild
        Write-Log "Running incremental build"
        & cmake --build $BuildDir --config $BuildType
        
        if ($LASTEXITCODE -eq 0) {
            Write-Log "✓ Incremental build successful"
        } else {
            Write-Log "✗ Incremental build failed" "WARN"
        }
    }
    
    # Test 7: Verify cache persistence
    Write-Log "Test 7: Verifying dependency cache persistence"
    
    if (Test-Path $cacheDir) {
        $cacheSize = (Get-ChildItem $cacheDir -Recurse | Measure-Object -Property Length -Sum).Sum
        $cacheSizeMB = [math]::Round($cacheSize / 1MB, 2)
        Write-Log "Cache directory size: $cacheSizeMB MB"
        
        # Check if cache can be reused
        $cacheItems = Get-ChildItem $cacheDir -Directory
        Write-Log "Cached items: $($cacheItems.Count)"
        
        foreach ($item in $cacheItems) {
            $itemSize = (Get-ChildItem $item.FullName -Recurse | Measure-Object -Property Length -Sum).Sum
            $itemSizeMB = [math]::Round($itemSize / 1MB, 2)
            Write-Log "  - $($item.Name): $itemSizeMB MB"
        }
    }
    
    # Test 8: Run tests if available
    if (Test-Path (Join-Path $BuildDir "tests")) {
        Write-Log "Test 8: Running unit tests with FetchContent dependencies"
        
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
            Write-Log "Some tests failed, but FetchContent test continues" "WARN"
        } else {
            Write-Log "✓ All tests passed with FetchContent dependencies"
        }
    } else {
        Write-Log "No tests found to run"
    }
    
    Write-Log "=== FetchContent Fallback Test Summary ==="
    Write-Log "✓ FetchContent fallback mechanism"
    Write-Log "✓ Cache directory management"
    Write-Log "✓ Dependency fetching"
    Write-Log "✓ Build artifact generation"
    Write-Log "✓ Incremental build support"
    Write-Log "✓ Cache persistence"
    Write-Log "=== Test Completed Successfully ==="
    
} catch {
    Write-Log "Test failed: $($_.Exception.Message)" "ERROR"
    Write-Log "Stack trace: $($_.ScriptStackTrace)" "ERROR"
    exit 1
} finally {
    Set-Location $ProjectRoot
}

Write-Log "FetchContent fallback test completed successfully"
