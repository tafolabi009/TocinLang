#!/usr/bin/env bash
#
# make-pkg.sh — build a macOS installer: a signed-capable .pkg (double-click →
# the native macOS Installer GUI) and a .dmg wrapping it for drag-to-open
# distribution. Run this ON macOS after installer/package.sh --bundle-libs has
# staged the payload (Mach-O binaries can't be cross-produced on Linux).
#
# The .pkg installs to /usr/local/tocin with a /usr/local/bin/tocin symlink and
# bundles the compiler, runtime, stdlib, and the ld.lld link bundle, so native
# `-o` output builds without Xcode/clang. Uses only the macOS-builtin
# pkgbuild/productbuild/hdiutil — no third-party tools.
#
# Usage: make-pkg.sh [--stage DIR] [--out DIR] [--sign "Developer ID Installer: NAME"]
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$REPO_ROOT/dist"; STAGE=""; SIGN=""
while [ $# -gt 0 ]; do case "$1" in
    --stage) STAGE="$2"; shift ;;
    --out) OUT="$2"; shift ;;
    --sign) SIGN="$2"; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
esac; shift; done

[ "$(uname -s)" = Darwin ] || { echo "make-pkg.sh must run on macOS" >&2; exit 1; }
[ -n "$STAGE" ] || STAGE="$(ls -d "$OUT"/tocin-*-macos-* 2>/dev/null | head -1)"
[ -d "$STAGE" ] || { echo "no staged payload; run installer/package.sh --bundle-libs first" >&2; exit 1; }
VER="$(cat "$STAGE/VERSION" 2>/dev/null || echo 0.1.0)"
ARCH="$(uname -m)"; [ "$ARCH" = x86_64 ] && ARCH=x86_64 || ARCH=arm64

# --- Payload root: everything under /usr/local/tocin -----------------------
ROOT="$(mktemp -d)"; SCRIPTS="$(mktemp -d)"
trap 'rm -rf "$ROOT" "$SCRIPTS"' EXIT
DEST="$ROOT/usr/local/tocin"
mkdir -p "$DEST" "$ROOT/usr/local/bin"
cp -R "$STAGE/libexec" "$STAGE/lib" "$STAGE/stdlib" "$DEST/"
[ -d "$STAGE/share" ] && cp -R "$STAGE/share" "$DEST/share"
cp "$STAGE/VERSION" "$DEST/VERSION"

# Launcher on PATH; sets TOCIN_PATH + the dylib search dir.
cat > "$ROOT/usr/local/bin/tocin" <<'EOF'
#!/bin/sh
HERE=/usr/local/tocin
export TOCIN_PATH="${TOCIN_PATH:-$HERE/stdlib}"
export DYLD_LIBRARY_PATH="$HERE/lib:${DYLD_LIBRARY_PATH:-}"
exec "$HERE/libexec/tocin" "$@"
EOF
chmod 755 "$ROOT/usr/local/bin/tocin"

# postinstall: make the launcher executable (belt & suspenders) + greet.
cat > "$SCRIPTS/postinstall" <<'EOF'
#!/bin/sh
chmod 755 /usr/local/bin/tocin 2>/dev/null || true
echo "Tocin installed. Run: tocin --version"
exit 0
EOF
chmod 755 "$SCRIPTS/postinstall"

mkdir -p "$OUT"
COMPONENT="$OUT/tocin-component.pkg"
PKG="$OUT/tocin-${VER}-macos-${ARCH}.pkg"

pkgbuild --root "$ROOT" --identifier "dev.tocin.compiler" --version "$VER" \
         --scripts "$SCRIPTS" --install-location "/" "$COMPONENT"

# Wrap in a product archive (adds the GUI welcome/license flow).
DISTRIB="$(mktemp)"
cat > "$DISTRIB" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>Tocin ${VER}</title>
  <organization>dev.tocin</organization>
  <options customize="never" require-scripts="false"/>
  <license file="LICENSE.txt"/>
  <pkg-ref id="dev.tocin.compiler"/>
  <choices-outline><line choice="default"/></choices-outline>
  <choice id="default"><pkg-ref id="dev.tocin.compiler"/></choice>
  <pkg-ref id="dev.tocin.compiler" version="${VER}" onConclusion="none">tocin-component.pkg</pkg-ref>
</installer-gui-script>
EOF
cp "$REPO_ROOT/LICENSE" "$OUT/LICENSE.txt" 2>/dev/null || echo "Tocin" > "$OUT/LICENSE.txt"

if [ -n "$SIGN" ]; then
    productbuild --distribution "$DISTRIB" --package-path "$OUT" --resources "$OUT" \
                 --sign "$SIGN" "$PKG"
else
    productbuild --distribution "$DISTRIB" --package-path "$OUT" --resources "$OUT" "$PKG"
fi
rm -f "$COMPONENT" "$OUT/LICENSE.txt" "$DISTRIB"
echo ">> built $PKG"

# --- .dmg wrapping the .pkg for drag-to-open distribution ------------------
DMGDIR="$(mktemp -d)"; trap 'rm -rf "$ROOT" "$SCRIPTS" "$DMGDIR"' EXIT
cp "$PKG" "$DMGDIR/"
DMG="$OUT/tocin-${VER}-macos-${ARCH}.dmg"
hdiutil create -volname "Tocin ${VER}" -srcfolder "$DMGDIR" -ov -format UDZO "$DMG"
echo ">> built $DMG"
