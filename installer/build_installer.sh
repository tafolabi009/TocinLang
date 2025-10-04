#!/bin/bash
# Enhanced Tocin Compiler Installer Build Script
# Builds professional installers for Linux and macOS platforms
set -e

# Default values
PLATFORM=${1:-linux}
CONFIGURATION=${2:-Release}
VERSION=${3:-1.0.0}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

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
print_info "Building Tocin Compiler for $PLATFORM (Configuration: $CONFIGURATION, Version: $VERSION)..."
pushd "$BUILD_DIR" > /dev/null
cmake -DCMAKE_BUILD_TYPE=$CONFIGURATION ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
popd > /dev/null
print_info "Build complete!"

# Create installer directory structure
print_info "Creating staging directory..."
STAGING_DIR="$BUILD_DIR/installer_staging"
BIN_DIR="$STAGING_DIR/bin"
LIB_DIR="$STAGING_DIR/lib"
DOC_DIR="$STAGING_DIR/doc"
STDLIB_DIR="$STAGING_DIR/stdlib"
EXAMPLES_DIR="$STAGING_DIR/examples"

# Create directories
rm -rf "$STAGING_DIR"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$DOC_DIR" "$STDLIB_DIR" "$EXAMPLES_DIR"

# Copy files
print_info "Copying files to staging directory..."
if [[ -x "$BUILD_DIR/tocin-compiler/src/tocin" ]]; then
  cp "$BUILD_DIR/tocin-compiler/src/tocin" "$BIN_DIR/"
elif [[ -x "$BUILD_DIR/tocin" ]]; then
  cp "$BUILD_DIR/tocin" "$BIN_DIR/"
else
  print_error "Built tocin binary not found."
  exit 1
fi

# Copy documentation
cp "$ROOT_DIR/LICENSE" "$DOC_DIR/" 2>/dev/null || print_warn "LICENSE not found"
cp "$ROOT_DIR/README.md" "$DOC_DIR/" 2>/dev/null || print_warn "README.md not found"
[[ -d "$ROOT_DIR/docs" ]] && cp -r "$ROOT_DIR/docs/"* "$DOC_DIR/" 2>/dev/null || print_warn "docs directory not found"

# Copy standard library
[[ -d "$ROOT_DIR/stdlib" ]] && cp -r "$ROOT_DIR/stdlib/"* "$STDLIB_DIR/" && print_info "Copied standard library"

# Copy examples
[[ -d "$ROOT_DIR/examples" ]] && cp -r "$ROOT_DIR/examples/"* "$EXAMPLES_DIR/" && print_info "Copied examples"

# Bundle shared library dependencies using ldd
EXE_PATH="$BIN_DIR/tocin"
if [[ -x "$EXE_PATH" ]] && command -v ldd >/dev/null 2>&1; then
  print_info "Bundling shared library dependencies..."
  while IFS= read -r line; do
    # lines look like: libxyz.so.1 => /lib/x86_64-linux-gnu/libxyz.so.1 (0x...)
    so_path="$(echo "$line" | awk '/=>/ {print $3} !/=>/ {print $1}' | grep -E '^/')"
    if [[ -n "$so_path" && -f "$so_path" ]]; then
      cp -u "$so_path" "$LIB_DIR/" 2>/dev/null || true
    fi
  done < <(ldd "$EXE_PATH")

  # Try to set rpath so the binary prefers $ORIGIN/../lib at runtime
  if command -v patchelf >/dev/null 2>&1; then
    print_info "Setting rpath to \$ORIGIN/../lib"
    patchelf --set-rpath '$ORIGIN/../lib' "$EXE_PATH" 2>/dev/null || true
  else
    # Create a launcher that sets LD_LIBRARY_PATH
    print_warn "patchelf not found, creating launcher script"
    cat > "$BIN_DIR/tocin.sh" << 'EOF'
#!/bin/sh
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/../lib:${LD_LIBRARY_PATH}"
exec "$SCRIPT_DIR/tocin" "$@"
EOF
    chmod +x "$BIN_DIR/tocin.sh"
  fi
fi

# Platform-specific build
case "$PLATFORM" in
    "linux")
        print_info "Building Linux packages..."
        
        # Build DEB package
        if command -v dpkg-deb &> /dev/null; then
            print_info "Creating DEB package..."
            DEBIAN_DIR="$STAGING_DIR/DEBIAN"
            mkdir -p "$DEBIAN_DIR"
            
            # Create control file
            cat > "$DEBIAN_DIR/control" << EOF
Package: tocin-compiler
Version: $VERSION
Section: devel
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.31), libstdc++6 (>= 10), zlib1g
Recommends: llvm-14 | llvm-13 | llvm-12 | llvm-11
Maintainer: Tocin Development Team <dev@tocin.dev>
Description: Tocin Programming Language Compiler
 A modern, statically-typed programming language with advanced features
 including traits, LINQ, null safety, async/await, and FFI support.
Homepage: https://github.com/tafolabi009/tocin-compiler
EOF
            
            # Create postinst script
            cat > "$DEBIAN_DIR/postinst" << 'EOF'
#!/bin/bash
set -e
ldconfig 2>/dev/null || true
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   Tocin Compiler has been successfully installed!          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Run 'tocin --version' to verify the installation."
echo "Run 'tocin --help' for usage information."
echo ""
echo "Quick start:"
echo "  echo 'print(\"Hello, World!\");' > hello.to"
echo "  tocin hello.to"
EOF
            chmod 755 "$DEBIAN_DIR/postinst"
            
            # Build package
            DEB_FILE="$BUILD_DIR/tocin-compiler_${VERSION}_amd64.deb"
            dpkg-deb --build "$STAGING_DIR" "$DEB_FILE"
            print_info "Created Debian package: $DEB_FILE"
        else
            print_warn "dpkg-deb not found, skipping DEB package creation"
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
/usr/bin/tocin*
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