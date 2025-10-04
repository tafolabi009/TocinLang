# Tocin Installer Guide

## Overview

The Tocin compiler provides professional, cross-platform installers similar to Go, Python, and Adobe installers. This guide explains how to build and use installers for Windows, Linux, and macOS.

## Installer Features

### Windows Installer
- **Technology**: NSIS (Nullsoft Scriptable Install System)
- **Format**: .exe installer
- **Features**:
  - GUI installation wizard
  - Custom installation directory
  - PATH environment variable configuration
  - Start menu shortcuts
  - Uninstall support
  - Version detection and upgrade handling

### Linux Installer
- **Formats**: 
  - DEB (Debian/Ubuntu)
  - RPM (Red Hat/Fedora)
  - AppImage (Universal)
  - Snap package
- **Features**:
  - System-wide or user installation
  - Automatic dependency resolution
  - Desktop integration
  - Man page installation
  - Clean uninstallation

### macOS Installer
- **Formats**:
  - DMG disk image
  - PKG installer
  - Homebrew formula
- **Features**:
  - Drag-and-drop installation
  - Code signing support
  - Gatekeeper compatibility
  - Automatic PATH configuration
  - Native uninstaller

## Building Installers

### Prerequisites

#### Windows
```powershell
# Install NSIS
choco install nsis

# Or download from: https://nsis.sourceforge.io/

# Install CMake
choco install cmake

# Install LLVM
choco install llvm
```

#### Linux
```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake llvm-dev nsis

# Fedora/RHEL
sudo dnf install gcc-c++ cmake llvm-devel nsis

# For package building
sudo apt-get install dpkg-dev rpm alien
```

#### macOS
```bash
# Install Homebrew
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake llvm create-dmg
```

### Building Windows Installer

#### Method 1: Using PowerShell Script

```powershell
# Navigate to repository root
cd tocin-compiler

# Run the installer build script
.\installer\build_installer.ps1 -Platform windows -Version "1.0.0"

# Installer will be created in: build\TocingCompiler-1.0.0-Setup.exe
```

#### Method 2: Manual Build

```powershell
# Step 1: Build the compiler
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

# Step 2: Create staging directory
mkdir installer_staging
mkdir installer_staging\bin
mkdir installer_staging\lib
mkdir installer_staging\doc

# Step 3: Copy files
copy Release\tocin.exe installer_staging\bin\
copy ..\README.md installer_staging\doc\
copy ..\LICENSE installer_staging\doc\

# Step 4: Build installer
cd ..\installer\windows
makensis installer.nsi
```

### Building Linux Installer

#### DEB Package

```bash
# Navigate to repository root
cd tocin-compiler

# Run the installer build script
./installer/build_installer.sh --platform linux --format deb --version "1.0.0"

# Package will be created in: build/tocin-compiler_1.0.0_amd64.deb
```

#### Manual DEB Build

```bash
# Step 1: Build the compiler
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Step 2: Create package directory structure
mkdir -p tocin-compiler-1.0.0/DEBIAN
mkdir -p tocin-compiler-1.0.0/usr/bin
mkdir -p tocin-compiler-1.0.0/usr/share/doc/tocin-compiler
mkdir -p tocin-compiler-1.0.0/usr/share/man/man1

# Step 3: Copy files
cp tocin tocin-compiler-1.0.0/usr/bin/
cp ../README.md tocin-compiler-1.0.0/usr/share/doc/tocin-compiler/
cp ../LICENSE tocin-compiler-1.0.0/usr/share/doc/tocin-compiler/

# Step 4: Create control file
cat > tocin-compiler-1.0.0/DEBIAN/control << EOF
Package: tocin-compiler
Version: 1.0.0
Section: devel
Priority: optional
Architecture: amd64
Depends: llvm-11 | llvm-12 | llvm-13 | llvm-14, libc6 (>= 2.31)
Maintainer: Tocin Team <team@tocin.dev>
Description: Tocin Programming Language Compiler
 A modern, statically-typed programming language with advanced features
 including traits, LINQ, null safety, and async/await support.
EOF

# Step 5: Build package
dpkg-deb --build tocin-compiler-1.0.0
```

#### RPM Package

```bash
# Create RPM spec file and build
./installer/build_installer.sh --platform linux --format rpm --version "1.0.0"
```

#### AppImage (Universal Linux Package)

```bash
# Download AppImage tools
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

# Build AppImage
./installer/build_installer.sh --platform linux --format appimage --version "1.0.0"
```

### Building macOS Installer

#### DMG Installer

```bash
# Navigate to repository root
cd tocin-compiler

# Run the installer build script
./installer/build_installer.sh --platform macos --format dmg --version "1.0.0"

# DMG will be created in: build/Tocin-1.0.0.dmg
```

#### Manual DMG Build

```bash
# Step 1: Build the compiler
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)

# Step 2: Create application bundle
mkdir -p Tocin.app/Contents/MacOS
mkdir -p Tocin.app/Contents/Resources

cp tocin Tocin.app/Contents/MacOS/
cp ../installer/macos/Info.plist Tocin.app/Contents/
cp ../Tocin_Logo.icns Tocin.app/Contents/Resources/

# Step 3: Create DMG
create-dmg \
  --volname "Tocin Compiler" \
  --volicon "../Tocin_Logo.icns" \
  --window-pos 200 120 \
  --window-size 800 400 \
  --icon-size 100 \
  --icon "Tocin.app" 200 190 \
  --hide-extension "Tocin.app" \
  --app-drop-link 600 185 \
  "Tocin-1.0.0.dmg" \
  "Tocin.app"
```

#### Homebrew Formula

```bash
# Create Homebrew tap
brew tap tocin/tap

# Install via Homebrew
brew install tocin
```

## Installation

### Windows

#### GUI Installation

1. Download `TocingCompiler-1.0.0-Setup.exe`
2. Double-click to run the installer
3. Follow the installation wizard:
   - Accept license agreement
   - Choose installation directory (default: `C:\Program Files\Tocin`)
   - Select components to install
   - Choose whether to add to PATH
4. Click Install
5. Installation complete!

#### Silent Installation

```powershell
# Install silently with default options
TocingCompiler-1.0.0-Setup.exe /S

# Install to custom directory
TocingCompiler-1.0.0-Setup.exe /S /D=C:\MyPrograms\Tocin
```

#### Verification

```powershell
# Open new terminal
tocin --version
# Output: Tocin Compiler v1.0.0

# Test compilation
tocin test.to
```

### Linux

#### DEB Package (Debian/Ubuntu)

```bash
# Download the DEB package
wget https://github.com/tocin/releases/download/v1.0.0/tocin-compiler_1.0.0_amd64.deb

# Install
sudo dpkg -i tocin-compiler_1.0.0_amd64.deb

# Fix dependencies if needed
sudo apt-get install -f

# Verify installation
tocin --version
```

#### RPM Package (Fedora/RHEL)

```bash
# Download the RPM package
wget https://github.com/tocin/releases/download/v1.0.0/tocin-compiler-1.0.0.x86_64.rpm

# Install
sudo rpm -i tocin-compiler-1.0.0.x86_64.rpm

# Or using dnf
sudo dnf install tocin-compiler-1.0.0.x86_64.rpm

# Verify installation
tocin --version
```

#### AppImage (Universal)

```bash
# Download AppImage
wget https://github.com/tocin/releases/download/v1.0.0/Tocin-1.0.0-x86_64.AppImage

# Make executable
chmod +x Tocin-1.0.0-x86_64.AppImage

# Run (no installation needed!)
./Tocin-1.0.0-x86_64.AppImage --version

# Optional: Integrate with system
./Tocin-1.0.0-x86_64.AppImage --appimage-integrate
```

#### Snap Package

```bash
# Install from Snap Store
sudo snap install tocin-compiler

# Verify installation
tocin --version
```

### macOS

#### DMG Installation

1. Download `Tocin-1.0.0.dmg`
2. Double-click to mount the disk image
3. Drag `Tocin.app` to the Applications folder
4. Eject the disk image
5. Open Terminal and add to PATH:

```bash
echo 'export PATH="/Applications/Tocin.app/Contents/MacOS:$PATH"' >> ~/.zshrc
source ~/.zshrc

# Verify
tocin --version
```

#### Homebrew Installation

```bash
# Add Tocin tap
brew tap tocin/tap

# Install
brew install tocin

# Verify
tocin --version
```

## Uninstallation

### Windows

#### Using Uninstaller

1. Open Settings → Apps → Apps & features
2. Search for "Tocin Compiler"
3. Click Uninstall
4. Follow the uninstall wizard

#### Silent Uninstall

```powershell
# Find uninstaller
$uninstaller = "C:\Program Files\Tocin\Uninstall.exe"

# Run silently
& $uninstaller /S
```

### Linux

#### DEB Package

```bash
# Remove package
sudo dpkg -r tocin-compiler

# Remove package and configuration
sudo dpkg -P tocin-compiler

# Or using apt
sudo apt-get remove tocin-compiler
```

#### RPM Package

```bash
# Remove package
sudo rpm -e tocin-compiler

# Or using dnf
sudo dnf remove tocin-compiler
```

#### AppImage

```bash
# Simply delete the file
rm Tocin-1.0.0-x86_64.AppImage

# If integrated with system
./Tocin-1.0.0-x86_64.AppImage --appimage-unintegrate
```

### macOS

#### Manual Uninstall

```bash
# Remove application
rm -rf /Applications/Tocin.app

# Remove from PATH (if manually added)
# Edit ~/.zshrc and remove the PATH line
```

#### Homebrew Uninstall

```bash
brew uninstall tocin
```

## Upgrade/Update

### Windows

1. Download new installer
2. Run installer (will detect existing installation)
3. Choose "Upgrade" option
4. Previous version will be replaced

### Linux

```bash
# DEB
sudo dpkg -i tocin-compiler_NEW_VERSION_amd64.deb

# RPM
sudo rpm -U tocin-compiler-NEW_VERSION.x86_64.rpm
```

### macOS

```bash
# Homebrew
brew upgrade tocin

# Manual: Install new DMG (overwrites old version)
```

## Configuration

### Post-Installation Setup

#### Set Default Editor

```bash
# Bash
echo 'export EDITOR=tocin' >> ~/.bashrc

# Zsh
echo 'export EDITOR=tocin' >> ~/.zshrc
```

#### Configure Environment

```bash
# Set Tocin home directory
export TOCIN_HOME=/usr/local/tocin

# Set standard library path
export TOCIN_STDLIB=$TOCIN_HOME/stdlib

# Enable JIT by default
export TOCIN_JIT=1
```

## Troubleshooting

### Common Issues

#### Windows: "tocin is not recognized"

**Solution**: Add to PATH manually
```powershell
$env:Path += ";C:\Program Files\Tocin\bin"
[Environment]::SetEnvironmentVariable("Path", $env:Path, [EnvironmentVariableTarget]::User)
```

#### Linux: Missing dependencies

**Solution**: Install LLVM
```bash
# Ubuntu
sudo apt-get install llvm-14

# Fedora
sudo dnf install llvm
```

#### macOS: "tocin cannot be opened because it is from an unidentified developer"

**Solution**: Allow in System Preferences
```bash
# Or use command line
xattr -d com.apple.quarantine /Applications/Tocin.app
```

### Verification

```bash
# Check installation
tocin --version

# Check system integration
which tocin

# Test compilation
echo 'print("Hello, World!");' > test.to
tocin test.to
```

## Advanced Installation Options

### Custom Installation Location

#### Windows
```powershell
TocingCompiler-Setup.exe /D=C:\MyPath\Tocin
```

#### Linux
```bash
# Extract DEB and install manually
dpkg -x tocin-compiler.deb ~/tocin
export PATH="$HOME/tocin/usr/bin:$PATH"
```

### Minimal Installation

Install only core components without optional features:

```bash
# During build
cmake -DWITH_PYTHON=OFF -DWITH_V8=OFF -DWITH_DEBUGGER=OFF ..

# Or use minimal installer variant
./TocingCompiler-1.0.0-Minimal-Setup.exe
```

### Development Installation

Install additional development tools:

```bash
# Include headers and libraries
cmake -DINSTALL_HEADERS=ON -DINSTALL_LIBS=ON ..

# Or install dev package
sudo apt-get install tocin-compiler-dev
```

## System Requirements

### Windows
- Windows 10 or later (64-bit)
- 2 GB RAM minimum, 4 GB recommended
- 500 MB disk space
- Visual C++ Redistributable 2017 or later

### Linux
- glibc 2.31 or later
- 2 GB RAM minimum, 4 GB recommended
- 500 MB disk space
- LLVM 11 or later

### macOS
- macOS 10.15 (Catalina) or later
- 2 GB RAM minimum, 4 GB recommended
- 500 MB disk space
- Xcode Command Line Tools

## Building Custom Installers

### Customizing Windows Installer

Edit `installer/windows/installer.nsi`:

```nsis
; Custom branding
!define PRODUCT_NAME "My Custom Tocin"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "My Company"

; Custom installation directory
InstallDir "$PROGRAMFILES64\MyTocin"

; Add custom shortcuts
CreateShortCut "$DESKTOP\Tocin.lnk" "$INSTDIR\bin\tocin.exe"
```

### Customizing Linux Packages

Edit `installer/linux/control` (for DEB) or `installer/linux/tocin.spec` (for RPM).

### Customizing macOS Installer

Edit `installer/macos/Info.plist` for app bundle configuration.

## CI/CD Integration

### Automated Installer Building

```yaml
# GitHub Actions example
name: Build Installers

on: [push, pull_request]

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build Windows Installer
        run: |
          .\installer\build_installer.ps1 -Platform windows
      - name: Upload Artifact
        uses: actions/upload-artifact@v2
        with:
          name: windows-installer
          path: build/*.exe

  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build Linux Packages
        run: |
          ./installer/build_installer.sh --platform linux --format all
      - name: Upload Artifacts
        uses: actions/upload-artifact@v2
        with:
          name: linux-packages
          path: build/*.deb build/*.rpm build/*.AppImage

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build macOS Installer
        run: |
          ./installer/build_installer.sh --platform macos
      - name: Upload Artifact
        uses: actions/upload-artifact@v2
        with:
          name: macos-installer
          path: build/*.dmg
```

## Conclusion

The Tocin installer system provides professional, cross-platform installation experience comparable to major programming languages and commercial software. The installers handle all aspects of installation, configuration, and maintenance automatically.

For issues or questions, visit: https://github.com/tocin/tocin-compiler/issues
