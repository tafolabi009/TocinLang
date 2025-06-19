# Build script for Tocin Compiler installer
param(
    [string]$Platform = "windows",
    [string]$Configuration = "Release",
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = "Stop"
$rootDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $rootDir "build"
$installerDir = $PSScriptRoot
$commonDir = Join-Path $installerDir "common"
$platformDir = Join-Path $installerDir $Platform

# Create build directory if it doesn't exist
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

# Build the compiler
Write-Host "Building Tocin Compiler..."
Push-Location $buildDir
try {
    cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=$Configuration ..
    cmake --build . --config $Configuration
} finally {
    Pop-Location
}

# Create installer directory structure
$stagingDir = Join-Path $buildDir "installer_staging"
$binDir = Join-Path $stagingDir "bin"
$libDir = Join-Path $stagingDir "lib"
$docDir = Join-Path $stagingDir "doc"

# Create directories
@($stagingDir, $binDir, $libDir, $docDir) | ForEach-Object {
    if (Test-Path $_) {
        Remove-Item -Recurse -Force $_
    }
    New-Item -ItemType Directory -Path $_ | Out-Null
}

# Copy files
Copy-Item (Join-Path $buildDir "tocin-compiler/src/$Configuration/tocin.exe") $binDir
Copy-Item (Join-Path $rootDir "LICENSE") $docDir
Copy-Item (Join-Path $rootDir "README.md") $docDir
Copy-Item (Join-Path $rootDir "docs/*") $docDir -Recurse

# Copy platform-specific files
if ($Platform -eq "windows") {
    # Build Windows installer using NSIS
    $nsisScript = Join-Path $platformDir "installer.nsi"
    
    # Update version in NSIS script
    $nsisContent = Get-Content $nsisScript -Raw
    $nsisContent = $nsisContent -replace '!define VERSION ".*"', "!define VERSION `"$Version`""
    Set-Content $nsisScript $nsisContent

    # Build installer
    Write-Host "Building Windows installer..."
    & "C:\Program Files (x86)\NSIS\makensis.exe" /V4 $nsisScript

    # Move installer to build directory
    Move-Item (Join-Path $platformDir "TocingCompiler-$Version-Setup.exe") $buildDir -Force
} elseif ($Platform -eq "linux") {
    # Build Linux packages
    Write-Host "Linux installer build not supported on Windows"
    exit 1
} elseif ($Platform -eq "macos") {
    # Build macOS packages
    Write-Host "macOS installer build not supported on Windows"
    exit 1
}

Write-Host "Installer build complete!" 