@echo off
REM Compile the LLVM verification program

echo Compiling LLVM verification program...

REM Get LLVM configuration
FOR /F "tokens=*" %%i IN ('llvm-config --cxxflags --ldflags --system-libs --libs core analysis irreader support') DO SET LLVM_FLAGS=%%i

REM Clean up flags for Windows
SET LLVM_FLAGS=%LLVM_FLAGS:-fno-exceptions=%
SET LLVM_FLAGS=%LLVM_FLAGS:-std=c++14=-std=c++17%

REM Display compiler flags
echo Using compiler flags: %LLVM_FLAGS%

REM Compile with clang++
clang++ -o verify_llvm verify_llvm.cpp %LLVM_FLAGS% -std=c++17

if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! Running verification program...
    verify_llvm.exe
) else (
    echo Compilation failed!
) 
