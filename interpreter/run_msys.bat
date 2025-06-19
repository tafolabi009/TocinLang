@echo off
setlocal enabledelayedexpansion

:: Check if MSYS2 is installed
if not exist "C:\msys64" (
    echo MSYS2 is not installed. Please install it from https://www.msys2.org/
    pause
    exit /b 1
)

:: Set up MSYS2 environment
set MSYS2_PATH=C:\msys64
set PATH=%MSYS2_PATH%\usr\bin;%MSYS2_PATH%\mingw64\bin;%PATH%

:: Check if interpreter is built
if not exist "build\tocin.exe" (
    echo Interpreter is not built. Please run setup_msys.bat first.
    pause
    exit /b 1
)

:: Run the interpreter
if "%~1"=="" (
    :: Run REPL if no file is specified
    %MSYS2_PATH%\usr\bin\bash.exe -lc "build/tocin-repl.exe"
) else (
    :: Run the specified file
    %MSYS2_PATH%\usr\bin\bash.exe -lc "build/tocin.exe %~1"
) 