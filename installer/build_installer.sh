#!/bin/bash
set -e

# Default values
PLATFORM=${1:-linux}
CONFIGURATION=${2:-Release}
VERSION=${3:-1.0.0}

# Directory setup
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"
INSTALLER_DIR="$SCRIPT_DIR"
COMMON_DIR="$INSTALLER_DIR/common"
PLATFORM_DIR="$INSTALLER_DIR/$PLATFORM"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Build the compiler
echo "Building Tocin Compiler..."
pushd "$BUILD_DIR" > /dev/null
cmake -DCMAKE_BUILD_TYPE=$CONFIGURATION ..
cmake --build .
popd > /dev/null

# Create installer directory structure
STAGING_DIR="$BUILD_DIR/installer_staging"
BIN_DIR="$STAGING_DIR/bin"
LIB_DIR="$STAGING_DIR/lib"
DOC_DIR="$STAGING_DIR/doc"

# Create directories
rm -rf "$STAGING_DIR"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$DOC_DIR"

# Copy files
cp "$BUILD_DIR/tocin-compiler/src/tocin" "$BIN_DIR/"
cp "$ROOT_DIR/LICENSE" "$DOC_DIR/"
cp "$ROOT_DIR/README.md" "$DOC_DIR/"
cp -r "$ROOT_DIR/docs/"* "$DOC_DIR/"

# Platform-specific build
case "$PLATFORM" in
    "linux")
        echo "Building Linux packages..."
        
        # Build DEB package
        if command -v dpkg-deb &> /dev/null; then
            DEBIAN_DIR="$STAGING_DIR/DEBIAN"
            mkdir -p "$DEBIAN_DIR"
            
            # Create control file
            cat > "$DEBIAN_DIR/control" << EOF
Package: tocin-compiler
Version: $VERSION
Section: development
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.17)
Maintainer: Tocin Team <support@tocin.dev>
Description: Tocin Programming Language Compiler
 A modern systems programming language compiler
 with advanced features and cross-platform support.
EOF
            
            # Create postinst script
            cat > "$DEBIAN_DIR/postinst" << EOF
#!/bin/sh
set -e
ldconfig
EOF
            chmod 755 "$DEBIAN_DIR/postinst"
            
            # Build package
            DEB_FILE="$BUILD_DIR/tocin-compiler_${VERSION}_amd64.deb"
            dpkg-deb --build "$STAGING_DIR" "$DEB_FILE"
            echo "Created Debian package: $DEB_FILE"
        fi
        
        # Build RPM package
        if command -v rpmbuild &> /dev/null; then
            RPM_DIR="$BUILD_DIR/rpm"
            mkdir -p "$RPM_DIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
            
            # Create spec file
            cat > "$RPM_DIR/SPECS/tocin-compiler.spec" << EOF
Name:           tocin-compiler
Version:        $VERSION
Release:        1%{?dist}
Summary:        Tocin Programming Language Compiler
License:        MIT
URL:            https://tocin.dev
BuildArch:      x86_64

%description
A modern systems programming language compiler with advanced features and cross-platform support.

%prep
# Nothing to do

%build
# Nothing to do

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/doc/tocin-compiler
cp -r $BIN_DIR/* %{buildroot}/usr/bin/
cp -r $DOC_DIR/* %{buildroot}/usr/share/doc/tocin-compiler/

%files
/usr/bin/tocin
/usr/share/doc/tocin-compiler/*

%changelog
* $(date '+%a %b %d %Y') Tocin Team <support@tocin.dev> - $VERSION-1
- Initial package
EOF
            
            # Build package
            rpmbuild --define "_topdir $RPM_DIR" -bb "$RPM_DIR/SPECS/tocin-compiler.spec"
            echo "Created RPM package: $RPM_DIR/RPMS/x86_64/tocin-compiler-${VERSION}-1.x86_64.rpm"
        fi
        ;;
        
    "macos")
        echo "Building macOS package..."
        
        # Create DMG
        DMG_DIR="$BUILD_DIR/dmg"
        APP_DIR="$DMG_DIR/Tocin Compiler.app"
        mkdir -p "$APP_DIR/Contents/"{MacOS,Resources}
        
        # Copy files
        cp "$BIN_DIR/tocin" "$APP_DIR/Contents/MacOS/"
        cp "$COMMON_DIR/icon.icns" "$APP_DIR/Contents/Resources/"
        
        # Create Info.plist
        cat > "$APP_DIR/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>tocin</string>
    <key>CFBundleIconFile</key>
    <string>icon</string>
    <key>CFBundleIdentifier</key>
    <string>dev.tocin.compiler</string>
    <key>CFBundleName</key>
    <string>Tocin Compiler</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
</dict>
</plist>
EOF
        
        # Create DMG
        DMG_FILE="$BUILD_DIR/TocingCompiler-$VERSION.dmg"
        hdiutil create -volname "Tocin Compiler" -srcfolder "$DMG_DIR" -ov -format UDZO "$DMG_FILE"
        echo "Created DMG: $DMG_FILE"
        ;;
        
    *)
        echo "Unsupported platform: $PLATFORM"
        exit 1
        ;;
esac

echo "Installer build complete!" 