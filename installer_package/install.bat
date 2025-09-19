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
