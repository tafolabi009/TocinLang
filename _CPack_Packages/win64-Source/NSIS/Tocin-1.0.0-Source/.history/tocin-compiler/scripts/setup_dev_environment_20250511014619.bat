@echo off
echo Setting up development environment for Tocin compiler...

:: Check for administrator privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo This script requires administrator privileges.
    echo Please run as administrator and try again.
    pause
    exit /b 1
)

:: Install Chocolatey package manager if not already installed
where choco >nul 2>&1
if %errorLevel% neq 0 (
    echo Installing Chocolatey package manager...
    @powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
    if %errorLevel% neq 0 (
        echo Failed to install Chocolatey.
        pause
        exit /b 1
    )
    :: Update PATH to include Chocolatey
    set "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
)

:: Install MSYS2 (which provides MinGW-w64)
echo Installing MSYS2 (which includes MinGW-w64)...
choco install msys2 -y
if %errorLevel% neq 0 (
    echo Failed to install MSYS2.
    pause
    exit /b 1
)

:: Update PATH to include MSYS2/MinGW
set "PATH=%PATH%;C:\msys64\mingw64\bin;C:\msys64\usr\bin"

:: Install CMake
echo Installing CMake...
choco install cmake -y
if %errorLevel% neq 0 (
    echo Failed to install CMake.
    pause
    exit /b 1
)

:: Install LLVM using MSYS2
echo Installing LLVM and Clang using MSYS2...
C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-llvm mingw-w64-x86_64-clang"
if %errorLevel% neq 0 (
    echo Failed to install LLVM.
    pause
    exit /b 1
)

:: Install other dependencies
echo Installing additional dependencies...
C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-zlib mingw-w64-x86_64-zstd mingw-w64-x86_64-libffi mingw-w64-x86_64-libxml2 mingw-w64-x86_64-python mingw-w64-x86_64-v8"

:: Install Ninja for faster builds
echo Installing Ninja build system...
choco install ninja -y

echo.
echo Development environment setup complete!
echo Please restart your command prompt to ensure all PATH updates take effect.
echo To build the project, run: 
echo   mkdir build
echo   cd build
echo   cmake .. -G Ninja
echo   ninja
echo.
pause 
