@echo off
setlocal enabledelayedexpansion

:: Create build directory
if not exist build mkdir build
cd build

:: Configure with CMake
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release

:: Build
cmake --build . --config Release

:: Create packages
cpack -G "NSIS;ZIP"

:: Move packages to dist directory
if not exist ..\dist mkdir ..\dist
move *.exe ..\dist\
move *.zip ..\dist\

echo Packages created successfully in the dist directory!
pause 