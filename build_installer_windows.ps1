# Complete Windows Installer Builder for Tocin Compiler
# This script builds the compiler and creates a self-contained installer

param(
    [string]$BuildType = "Release",
    [switch]$Clean = $false
)

Write-Host "=== Tocin Compiler Windows Installer Builder ===" -ForegroundColor Green

# Get the script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# MSYS2 paths
$MSYS2_ROOT = "D:\Downloads\msys64"
$CMAKE_PATH = "$MSYS2_ROOT\mingw64\bin\cmake.exe"
$MINGW_BIN = "$MSYS2_ROOT\mingw64\bin"

# Clean build if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Yellow
$cmakeArgs = @(
    "-G", "MinGW Makefiles",
    "-S", ".",
    "-B", "build",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_C_COMPILER=$MINGW_BIN\gcc.exe",
    "-DCMAKE_CXX_COMPILER=$MINGW_BIN\g++.exe",
    "-DCMAKE_MAKE_PROGRAM=$MINGW_BIN\mingw32-make.exe",
    "-DWITH_DEBUGGER=OFF",
    "-DWITH_WASM=OFF", 
    "-DWITH_PACKAGE_MANAGER=OFF"
)

& $CMAKE_PATH @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
    exit 1
}

# Build the project
Write-Host "Building project..." -ForegroundColor Yellow
& $CMAKE_PATH --build build --config $BuildType -- -j4
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit 1
}

# Find the built executable
$tocinExe = Get-ChildItem -Path "build" -Name "tocin.exe" -Recurse | Select-Object -First 1
if (-not $tocinExe) {
    Write-Error "tocin.exe not found in build directory!"
    exit 1
}

$tocinPath = Join-Path "build" $tocinExe
Write-Host "Found tocin.exe at: $tocinPath" -ForegroundColor Green

# Create installer staging directory
$stagingDir = "build/installer_staging"
$binDir = "$stagingDir/bin"
$libDir = "$stagingDir/lib"
$docDir = "$stagingDir/doc"

Write-Host "Creating installer staging directory..." -ForegroundColor Yellow
Remove-Item -Recurse -Force $stagingDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $binDir | Out-Null
New-Item -ItemType Directory -Path $libDir | Out-Null
New-Item -ItemType Directory -Path $docDir | Out-Null

# Copy the main executable
Copy-Item $tocinPath $binDir

# Find and copy all required DLLs
Write-Host "Finding and copying required DLLs..." -ForegroundColor Yellow

Write-Host "MSYS2 MinGW64 bin directory: $MINGW_BIN" -ForegroundColor Cyan

# List of required DLLs
$requiredDlls = @(
    "libwinpthread-1.dll",
    "libstdc++-6.dll", 
    "libgcc_s_seh-1.dll",
    "libzlib.dll"
)

# Copy base DLLs
foreach ($dll in $requiredDlls) {
    $dllPath = Join-Path $MINGW_BIN $dll
    if (Test-Path $dllPath) {
        Copy-Item $dllPath $binDir
        Write-Host "Copied: $dll" -ForegroundColor Green
    } else {
        Write-Warning "Missing: $dll"
    }
}

# Copy V8 DLLs if they exist
$v8Dlls = @("libv8.dll", "libv8_libbase.dll", "libv8_libplatform.dll")
foreach ($dll in $v8Dlls) {
    $dllPath = Join-Path $MINGW_BIN $dll
    if (Test-Path $dllPath) {
        Copy-Item $dllPath $binDir
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

# Copy ICU DLLs
$icuDlls = Get-ChildItem -Path $MINGW_BIN -Name "libicu*.dll" -ErrorAction SilentlyContinue
foreach ($dll in $icuDlls) {
    $dllPath = Join-Path $MINGW_BIN $dll
    Copy-Item $dllPath $binDir
    Write-Host "Copied: $dll" -ForegroundColor Green
}

# Copy other common DLLs
$otherDlls = @("libxml2-2.dll", "libffi-*.dll", "libzstd.dll", "libLLVM-*.dll", "zlib1.dll", "libiconv-2.dll", "liblzma-5.dll", "libcharset-1.dll")
foreach ($pattern in $otherDlls) {
    $dlls = Get-ChildItem -Path $MINGW_BIN -Name $pattern -ErrorAction SilentlyContinue
    foreach ($dll in $dlls) {
        $dllPath = Join-Path $MINGW_BIN $dll
        Copy-Item $dllPath $binDir
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

# Copy Python DLLs if Python is found
$pythonExe = (Get-Command python -ErrorAction SilentlyContinue).Source
if ($pythonExe) {
    $pythonDir = Split-Path $pythonExe
    $pythonDlls = Get-ChildItem -Path $pythonDir -Name "python*.dll" -ErrorAction SilentlyContinue
    foreach ($dll in $pythonDlls) {
        $dllPath = Join-Path $pythonDir $dll
        Copy-Item $dllPath $binDir
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

# Copy documentation
if (Test-Path "README.md") {
    Copy-Item "README.md" $docDir
}
if (Test-Path "docs") {
    Copy-Item -Recurse "docs" $docDir
}

# Create a simple test script
$testScript = @"
@echo off
echo Testing Tocin Compiler...
echo.
echo Version:
bin\tocin.exe --version
echo.
echo Help:
bin\tocin.exe --help
echo.
echo Test completed. Press any key to exit.
pause
"@
$testScript | Out-File -FilePath "$stagingDir/test_tocin.bat" -Encoding ASCII

Write-Host "Installer staging complete!" -ForegroundColor Green
Write-Host "Files staged in: $stagingDir" -ForegroundColor Cyan

# List what we have
Write-Host "`nStaged files:" -ForegroundColor Yellow
Get-ChildItem -Recurse $stagingDir | ForEach-Object {
    Write-Host "  $($_.FullName.Replace((Get-Location).Path + '\', ''))" -ForegroundColor White
}

# Test the executable
Write-Host "`nTesting the executable..." -ForegroundColor Yellow
$testExe = Join-Path $binDir "tocin.exe"
if (Test-Path $testExe) {
    try {
        $version = & $testExe --version 2>&1
        Write-Host "Version output: $version" -ForegroundColor Green
    } catch {
        Write-Warning "Could not run tocin.exe: $_"
    }
} else {
    Write-Warning "tocin.exe not found in staging directory"
}

Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "You can test the compiler by running: $binDir\tocin.exe --version" -ForegroundColor Cyan
Write-Host "All required DLLs have been bundled with the executable." -ForegroundColor Cyan
