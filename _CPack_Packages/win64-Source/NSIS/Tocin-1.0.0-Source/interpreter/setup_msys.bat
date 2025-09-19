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

:: Update MSYS2 packages
echo Updating MSYS2 packages...
%MSYS2_PATH%\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"

:: Install required packages
echo Installing required packages...
%MSYS2_PATH%\usr\bin\bash.exe -lc "pacman -S --noconfirm mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-llvm mingw-w64-x86_64-boost mingw-w64-x86_64-nsis mingw-w64-x86_64-imagemagick mingw-w64-x86_64-librsvg mingw-w64-x86_64-make"

:: Convert SVG to ICO
echo Converting icon...
%MSYS2_PATH%\usr\bin\bash.exe -lc "cd '%CD%/resources' && mkdir -p temp && /mingw64/bin/magick.exe convert -background none -size 16x16 icon.svg temp/icon_16.png && /mingw64/bin/magick.exe convert -background none -size 32x32 icon.svg temp/icon_32.png && /mingw64/bin/magick.exe convert -background none -size 48x48 icon.svg temp/icon_48.png && /mingw64/bin/magick.exe convert -background none -size 64x64 icon.svg temp/icon_64.png && /mingw64/bin/magick.exe convert -background none -size 128x128 icon.svg temp/icon_128.png && /mingw64/bin/magick.exe convert -background none -size 256x256 icon.svg temp/icon_256.png && /mingw64/bin/magick.exe convert temp/icon_*.png icon.ico && rm -rf temp"

:: Create build directory
if not exist build mkdir build
cd build

:: Configure with CMake
echo Configuring with CMake...
%MSYS2_PATH%\usr\bin\bash.exe -lc "cd '%CD%' && export PATH=/mingw64/bin:$PATH && /mingw64/bin/cmake.exe .. -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=/mingw64/bin/mingw32-make.exe"

:: Build
echo Building...
%MSYS2_PATH%\usr\bin\bash.exe -lc "cd '%CD%' && export PATH=/mingw64/bin:$PATH && /mingw64/bin/cmake.exe --build . --config Release"

:: Create packages
echo Creating packages...
%MSYS2_PATH%\usr\bin\bash.exe -lc "cd '%CD%' && export PATH=/mingw64/bin:$PATH && /mingw64/bin/cpack.exe -G 'NSIS;ZIP'"

:: Move packages to dist directory
if not exist ..\dist mkdir ..\dist
move *.exe ..\dist\
move *.zip ..\dist\

echo Build completed successfully!
echo Packages are available in the dist directory.
pause 