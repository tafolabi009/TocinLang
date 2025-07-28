@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Tocin Compiler - Standalone Build Script
echo ========================================

:: Check for required tools
echo Checking for required build tools...

:: Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: CMake not found in PATH
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

:: Check for a C++ compiler
where cl >nul 2>&1
if %errorlevel% equ 0 (
    set COMPILER=msvc
    echo Found MSVC compiler
) else (
    where g++ >nul 2>&1
    if %errorlevel% equ 0 (
        set COMPILER=gcc
        echo Found GCC compiler
    ) else (
        where clang++ >nul 2>&1
        if %errorlevel% equ 0 (
            set COMPILER=clang
            echo Found Clang compiler
        ) else (
            echo ERROR: No C++ compiler found
            echo Please install Visual Studio, MinGW, or Clang
            pause
            exit /b 1
        )
    )
)

:: Create build directory
if not exist build mkdir build
cd build

:: Configure with CMake
echo Configuring with CMake...
if "%COMPILER%"=="msvc" (
    cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
) else if "%COMPILER%"=="gcc" (
    cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
) else (
    cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
)

if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)

:: Build the project
echo Building the project...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

:: Create output directory
if not exist ..\dist mkdir ..\dist

:: Copy executables
if exist tocin.exe (
    copy tocin.exe ..\dist\
    echo Copied tocin.exe to dist/
)

if exist tocin-repl.exe (
    copy tocin-repl.exe ..\dist\
    echo Copied tocin-repl.exe to dist/
)

:: Copy libraries if they exist
if exist *.dll (
    copy *.dll ..\dist\
    echo Copied DLL files to dist/
)

if exist *.so (
    copy *.so ..\dist\
    echo Copied SO files to dist/
)

if exist *.dylib (
    copy *.dylib ..\dist\
    echo Copied DYLIB files to dist/
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Executables are available in the dist/ directory.
echo.
echo To run the compiler:
echo   dist\tocin.exe [options] <file>
echo.
echo To run the REPL:
echo   dist\tocin-repl.exe
echo.

pause 