#!/usr/bin/env bash
#
# make-deb.sh — turn the staged tocin payload into a Debian/Ubuntu .deb.
# Installs to /opt/tocin with a /usr/bin/tocin launcher; double-clicking the
# .deb opens the distro's GUI software installer (GNOME Software / KDE
# Discover), satisfying "GUI install" on Linux. Programs built with the
# bundled linker are fully static, so the package Depends only on libc6.
#
# Usage: make-deb.sh [--stage DIR] [--out DIR]
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STAGE=""; OUT="$REPO_ROOT/dist"
while [ $# -gt 0 ]; do case "$1" in
    --stage) STAGE="$2"; shift ;;
    --out) OUT="$2"; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
esac; shift; done

[ -n "$STAGE" ] || STAGE="$(ls -d "$OUT"/tocin-*-linux-* 2>/dev/null | head -1)"
[ -d "$STAGE" ] || { echo "no staged payload; run installer/package.sh first" >&2; exit 1; }
VER="$(cat "$STAGE/VERSION" 2>/dev/null || echo 0.1.0)"
ARCH=amd64; [ "$(uname -m)" = aarch64 ] && ARCH=arm64

ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT
mkdir -p "$ROOT/opt/tocin" "$ROOT/usr/bin" "$ROOT/DEBIAN" \
         "$ROOT/usr/share/doc/tocin"

# Payload -> /opt/tocin (libexec incl. the self-contained link bundle).
cp -R "$STAGE/libexec" "$STAGE/lib" "$STAGE/stdlib" "$ROOT/opt/tocin/"
cp "$STAGE/VERSION" "$ROOT/opt/tocin/VERSION"
[ -f "$STAGE/LICENSE" ] && cp "$STAGE/LICENSE" "$ROOT/usr/share/doc/tocin/copyright"

# Launcher (shell builtins only; see install.sh for rationale).
cat > "$ROOT/usr/bin/tocin" <<'EOF'
#!/bin/sh
HERE=/opt/tocin
export TOCIN_PATH="${TOCIN_PATH:-$HERE/stdlib}"
export LD_LIBRARY_PATH="$HERE/lib:${LD_LIBRARY_PATH:-}"
exec "$HERE/libexec/tocin" "$@"
EOF
chmod 755 "$ROOT/usr/bin/tocin"

INSTALLED_SIZE=$(du -sk "$ROOT" | cut -f1)
cat > "$ROOT/DEBIAN/control" <<EOF
Package: tocin
Version: $VER
Section: devel
Priority: optional
Architecture: $ARCH
Depends: libc6
Installed-Size: $INSTALLED_SIZE
Maintainer: Afolabi Oluwatosin <tafolabi009@gmail.com>
Homepage: https://github.com/tafolabi009/TocinLang
Description: Tocin programming language (compiler, JIT, stdlib)
 Self-contained toolchain for the Tocin language: LLVM-based optimizing
 compiler with JIT (--run) and native AOT (-o), 34-module standard library,
 and a bundled linker so native executables build with NO system C
 toolchain installed. Bundle-linked programs are fully static.
EOF

mkdir -p "$OUT"
DEB="$OUT/tocin_${VER}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$ROOT" "$DEB" >/dev/null
echo ">> built $DEB ($(du -h "$DEB" | cut -f1))"
