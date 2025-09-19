# Build script for Tocin Compiler on Windows with MSYS2

$ErrorActionPreference = "Stop"

Write-Host "Building Tocin Compiler on Windows with MSYS2..." -ForegroundColor Green
Write-Host "MSYS2 Path: D:\Downloads\msys64" -ForegroundColor Cyan
Write-Host ""

# Clean build directory
Write-Host "Cleaning build directory..." -ForegroundColor Yellow
if (Test-Path "build") {
    Remove-Item -Path "build" -Recurse -Force
}

# Create build directory
New-Item -ItemType Directory -Path "build" | Out-Null
Set-Location -Path "build"

# Set environment variables for MSYS2
$env:Path = "D:\Downloads\msys64\mingw64\bin;D:\Downloads\msys64\usr\bin;$env:Path"
$env:CC = "D:\Downloads\msys64\mingw64\bin\gcc.exe"
$env:CXX = "D:\Downloads\msys64\mingw64\bin\g++.exe"
$env:CMAKE_MAKE_PROGRAM = "D:\Downloads\msys64\mingw64\bin\mingw32-make.exe"

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Yellow
& "D:\Downloads\msys64\mingw64\bin\cmake.exe" .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$env:CC" -DCMAKE_CXX_COMPILER="$env:CXX"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build the project
Write-Host "Building project..." -ForegroundColor Yellow
& "D:\Downloads\msys64\mingw64\bin\mingw32-make.exe" -j4

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Build completed successfully!" -ForegroundColor Green

# Check if executable exists
if (Test-Path "tocin.exe") {
    Write-Host "Compiler executable found: build\tocin.exe" -ForegroundColor Green
} else {
    Write-Host "Warning: Compiler executable not found!" -ForegroundColor Yellow
}

Set-Location -Path ..
