@echo off
echo Building Tocin compiler...

:: Check if build directory exists
if not exist build (
    mkdir build
    echo Created build directory.
)

:: Navigate to build directory
cd build

:: Configure with CMake
echo Configuring with CMake...
cmake .. -G Ninja ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DLLVM_DIR=C:/msys64/mingw64/lib/cmake/llvm

if %errorLevel% neq 0 (
    echo CMake configuration failed.
    cd ..
    exit /b 1
)

:: Build with Ninja
echo Building with Ninja...
ninja

if %errorLevel% neq 0 (
    echo Build failed.
    cd ..
    exit /b 1
)

echo Build completed successfully.
cd ..

:: Copy executable to bin directory
if not exist bin (
    mkdir bin
    echo Created bin directory.
)

copy build\tocin.exe bin\tocin.exe

echo Successfully copied executable to bin directory.
echo To run the compiler, use: bin\tocin.exe <input_file>

exit /b 0 
