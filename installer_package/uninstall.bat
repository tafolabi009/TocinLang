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
