@echo off
echo Building Tocin Compiler with MSYS2 MinGW64...
cd /d "%~dp0"
set PATH=C:\msys64\mingw64\bin;%PATH%

echo Configuring with CMake...
C:\msys64\mingw64\bin\cmake.exe -S . -B build -G "MinGW Makefiles" -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe

echo Building the project...
C:\msys64\mingw64\bin\cmake.exe --build build

echo Done!
pause 
