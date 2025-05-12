@echo off
REM Script to install dependencies for Tocin Compiler

echo Installing dependencies for Tocin Compiler...

REM Check if chocolatey is installed
where choco >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Chocolatey is not installed. Installing now...
    @"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
) else (
    echo Chocolatey is already installed.
)

REM Install LLVM using chocolatey
echo Installing LLVM...
choco install llvm -y

REM Configure environment variables
echo Setting environment variables...
setx LLVM_HOME "C:\Program Files\LLVM"
setx Path "%PATH%;C:\Program Files\LLVM\bin"

REM Create vcpkg directory if it doesn't exist
if not exist "C:\vcpkg" (
    echo Installing vcpkg...
    cd /d C:\
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    .\bootstrap-vcpkg.bat
    .\vcpkg integrate install
) else (
    echo vcpkg is already installed.
)

REM Install abseil using vcpkg
echo Installing abseil...
cd /d C:\vcpkg
.\vcpkg install abseil:x64-windows

echo Dependencies installation complete!
echo Please restart your terminal to apply environment variable changes. 
