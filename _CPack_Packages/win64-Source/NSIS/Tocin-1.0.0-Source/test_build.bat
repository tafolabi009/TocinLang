@echo off
echo Building Tocin Compiler with MSYS2 MinGW64...
echo.

REM Set MSYS2 environment
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
C:\msys64\mingw64\bin\cmake.exe .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo Building project...
C:\msys64\mingw64\bin\mingw32-make.exe -j4
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

REM Test the compiler
echo Testing compiler...
if exist tocin.exe (
    echo Compiler built successfully!
    .\tocin.exe --help
    echo.
    echo Creating test file...
    echo print("Hello from Tocin!"); > test.to
    echo Running test...
    .\tocin.exe test.to --jit
) else (
    echo Compiler executable not found!
)

echo.
echo Build process completed!
pause 