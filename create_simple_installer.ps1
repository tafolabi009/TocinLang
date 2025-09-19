# Create a simple installer package for Tocin Compiler
# This creates a working installer with all DLLs bundled

Write-Host "=== Creating Simple Tocin Installer ===" -ForegroundColor Green

# Source directory (where the working tocin.exe is)
$sourceDir = "_CPack_Packages\win64-Source\NSIS\Tocin-1.0.0-Source\build"
$installerDir = "installer_package"

# Create installer directory
if (Test-Path $installerDir) {
    Remove-Item -Recurse -Force $installerDir
}
New-Item -ItemType Directory -Path $installerDir | Out-Null
New-Item -ItemType Directory -Path "$installerDir\bin" | Out-Null
New-Item -ItemType Directory -Path "$installerDir\doc" | Out-Null

Write-Host "Copying files from: $sourceDir" -ForegroundColor Yellow

# Copy the main executable and all DLLs
Copy-Item "$sourceDir\tocin.exe" "$installerDir\bin\"
Copy-Item "$sourceDir\*.dll" "$installerDir\bin\"

# Copy documentation
if (Test-Path "README.md") {
    Copy-Item "README.md" "$installerDir\doc\"
}
if (Test-Path "docs") {
    Copy-Item -Recurse "docs" "$installerDir\doc\"
}

# Create a simple installer script
$installerScript = @"
@echo off
echo Installing Tocin Compiler...
echo.

set INSTALL_DIR=%PROGRAMFILES%\Tocin
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
if not exist "%INSTALL_DIR%\bin" mkdir "%INSTALL_DIR%\bin"
if not exist "%INSTALL_DIR%\doc" mkdir "%INSTALL_DIR%\doc"

echo Copying files...
xcopy /E /I /Y "bin" "%INSTALL_DIR%\bin"
xcopy /E /I /Y "doc" "%INSTALL_DIR%\doc"

echo Adding to PATH...
setx PATH "%PATH%;%INSTALL_DIR%\bin" /M

echo.
echo Installation complete!
echo Tocin Compiler has been installed to: %INSTALL_DIR%
echo.
echo You can now run 'tocin --version' from any command prompt.
echo.
pause
"@

$installerScript | Out-File -FilePath "$installerDir\install.bat" -Encoding ASCII

# Create an uninstaller script
$uninstallerScript = @"
@echo off
echo Uninstalling Tocin Compiler...
echo.

set INSTALL_DIR=%PROGRAMFILES%\Tocin

echo Removing files...
if exist "%INSTALL_DIR%" rmdir /S /Q "%INSTALL_DIR%"

echo Removing from PATH...
for /f "tokens=2*" %%A in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PATH') do set SYSTEM_PATH=%%B
set NEW_PATH=%SYSTEM_PATH:;%INSTALL_DIR%\bin;=;%
set NEW_PATH=%NEW_PATH:;%INSTALL_DIR%\bin=%
reg add "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PATH /t REG_EXPAND_SZ /d "%NEW_PATH%" /f

echo.
echo Uninstallation complete!
echo.
pause
"@

$uninstallerScript | Out-File -FilePath "$installerDir\uninstall.bat" -Encoding ASCII

# Create a test script
$testScript = @"
@echo off
echo Testing Tocin Compiler...
echo.

echo Version:
tocin --version
echo.

echo Help:
tocin --help
echo.

echo Test completed!
pause
"@

$testScript | Out-File -FilePath "$installerDir\test_tocin.bat" -Encoding ASCII

# Create a README for the installer
$readmeContent = @"
Tocin Compiler Installer Package
================================

This package contains a working Tocin Compiler with all required DLLs.

Contents:
- bin/tocin.exe - The main compiler executable
- bin/*.dll - All required dynamic libraries
- doc/ - Documentation files
- install.bat - Installation script (run as Administrator)
- uninstall.bat - Uninstallation script (run as Administrator)
- test_tocin.bat - Test script to verify installation

Installation:
1. Run install.bat as Administrator
2. The installer will copy files to Program Files and add to PATH
3. Run test_tocin.bat to verify the installation

Usage:
After installation, you can use 'tocin' from any command prompt:
- tocin --version
- tocin --help
- tocin compile myfile.to

Uninstallation:
Run uninstall.bat as Administrator to remove the compiler.

Note: This installer includes all required DLLs, so the compiler will work
on any Windows system without requiring additional dependencies.
"@

$readmeContent | Out-File -FilePath "$installerDir\README.txt" -Encoding ASCII

Write-Host "`nInstaller package created in: $installerDir" -ForegroundColor Green
Write-Host "`nContents:" -ForegroundColor Yellow
Get-ChildItem -Recurse $installerDir | ForEach-Object {
    Write-Host "  $($_.FullName.Replace((Get-Location).Path + '\', ''))" -ForegroundColor White
}

Write-Host "`nTo install:" -ForegroundColor Cyan
Write-Host "1. Run: $installerDir\install.bat (as Administrator)" -ForegroundColor White
Write-Host "2. Test with: $installerDir\test_tocin.bat" -ForegroundColor White

Write-Host "`n=== Installer Package Complete ===" -ForegroundColor Green
