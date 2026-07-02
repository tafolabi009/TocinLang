#!/usr/bin/env bash
#
# package.sh — build a self-contained Tocin distribution tarball for the current
# platform (Linux or macOS). Produces:
#
#   dist/tocin-<version>-<os>-<arch>.tar.gz
#
# The tarball contains the compiler, runtime, standard library, docs, examples,
# and an install.sh that wires up PATH + an uninstaller — everything a user
# needs, the way the Python/Rust installers bundle their pieces.
#
# Usage:
#   installer/package.sh [--bundle-libs] [--out DIR]
#
#   --bundle-libs   Also copy the compiler's non-system shared libraries
#                   (LLVM, GC, ...) into lib/ so the package runs with NO system
#                   dependencies (larger, fully portable). Without it, the
#                   installer expects LLVM 18 + libgc to be installed and tells
#                   the user how if they are missing.
#   --out DIR       Output directory (default: ./dist).
#
# Run this ON each target OS to produce that platform's package (cross-building
# a native LLVM toolchain is not supported).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUNDLE_LIBS=0
OUT="$REPO_ROOT/dist"

while [ $# -gt 0 ]; do
    case "$1" in
        --bundle-libs) BUNDLE_LIBS=1 ;;
        --out) OUT="$2"; shift ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

# --- Platform detection ---------------------------------------------------
case "$(uname -s)" in
    Linux)  OS=linux ;;
    Darwin) OS=macos ;;
    *) echo "Unsupported OS: $(uname -s). Use package.ps1 on Windows." >&2; exit 1 ;;
esac
case "$(uname -m)" in
    x86_64|amd64) ARCH=x86_64 ;;
    arm64|aarch64) ARCH=arm64 ;;
    *) ARCH="$(uname -m)" ;;
esac
LIBEXT=so; [ "$OS" = macos ] && LIBEXT=dylib

# --- Build if needed ------------------------------------------------------
BUILD="$REPO_ROOT/build"
if [ ! -x "$BUILD/tocin" ]; then
    echo ">> building tocin (no build/ found) ..."
    cmake -S "$REPO_ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
fi
cmake --build "$BUILD" --target tocin tocin_runtime_shared -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

VER="$("$BUILD/tocin" --version 2>/dev/null | awk '{print $2}')"
[ -n "$VER" ] || VER=0.1.0
NAME="tocin-$VER-$OS-$ARCH"
STAGE="$OUT/$NAME"

echo ">> staging $NAME"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/libexec" "$STAGE/lib" "$STAGE/share"

# --- Compiler + runtime ---------------------------------------------------
cp "$BUILD/tocin" "$STAGE/libexec/tocin"
for rt in "$BUILD/libtocin_runtime_shared.$LIBEXT" "$BUILD/libtocin_runtime.$LIBEXT"; do
    [ -f "$rt" ] && cp "$rt" "$STAGE/lib/libtocin_runtime.$LIBEXT"
done

# --- Bundled payload: stdlib, docs, examples ------------------------------
cp -R "$REPO_ROOT/stdlib"   "$STAGE/stdlib"
cp -R "$REPO_ROOT/docs"     "$STAGE/share/docs"
cp -R "$REPO_ROOT/examples" "$STAGE/share/examples"
[ -f "$REPO_ROOT/LICENSE" ] && cp "$REPO_ROOT/LICENSE" "$STAGE/LICENSE" || true
echo "$VER" > "$STAGE/VERSION"

# --- The installer that ships inside the tarball --------------------------
cp "$REPO_ROOT/installer/install.sh" "$STAGE/install.sh"
chmod +x "$STAGE/install.sh"

# --- Self-contained native linking bundle ---------------------------------
# Stage libexec/link/ (vendored ld.lld + a link recipe generated from the
# build toolchain) so `tocin file.to -o app` links native executables on
# machines with NO gcc/clang installed. tryBundledLink() looks for link/
# next to the compiler binary (libexec/tocin -> libexec/link).
LLD_BIN="$(command -v ld.lld || echo /usr/lib/llvm-18/bin/ld.lld)"
if [ -x "$LLD_BIN" ] && [ "$OS" = linux ]; then
    echo ">> staging self-contained link bundle (ld.lld + recipe)"
    # Fully-static recipe: bundle-linked programs carry their own libc/libgc/
    # libstdc++, so the output runs on ANY Linux (Go-style static binaries)
    # and never touches a system toolchain OR runtime package.
    GC_A="$("${CC:-gcc}" -print-file-name=libgc.a 2>/dev/null)"
    [ -f "$GC_A" ] || GC_A=""
    if KEEP_SYS=0 bash "$REPO_ROOT/installer/make-link-recipe.sh" \
        --gcc "${CC:-gcc}" --static \
        --out-dir "$STAGE/libexec/link" \
        --runtime "$BUILD/libtocin_runtime.a" \
        ${GC_A:+--gc "$GC_A"}; then
        cp -L "$LLD_BIN" "$STAGE/libexec/link/ld.lld"
    else
        echo "!! link-recipe generation failed; -o will fall back to system cc"
    fi
else
    echo "!! no ld.lld found; skipping self-contained link bundle"
fi

# --- Optionally vendor non-system shared libraries ------------------------
if [ "$BUNDLE_LIBS" = 1 ]; then
    echo ">> bundling shared libraries (fully portable build)"
    if [ "$OS" = linux ]; then
        # Copy every dependency except the core glibc/loader libs that exist on
        # every Linux. Each is copied under the exact soname the binary requests.
        ldd "$BUILD/tocin" | awk '/=>/ {print $3} !/=>/ {print $1}' | grep '^/' | while read -r so; do
            base="$(basename "$so")"
            case "$base" in
                libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|ld-linux*|linux-vdso*) continue ;;
            esac
            cp -L "$so" "$STAGE/lib/$base" 2>/dev/null || true
        done
    else
        # macOS: copy non-system dylibs (Homebrew/local), rewrite install names.
        otool -L "$BUILD/tocin" | awk 'NR>1 {print $1}' | while read -r dy; do
            case "$dy" in
                /usr/lib/*|/System/*) continue ;;
            esac
            [ -f "$dy" ] && cp -L "$dy" "$STAGE/lib/$(basename "$dy")" 2>/dev/null || true
        done
    fi
fi

# --- Tar it up ------------------------------------------------------------
( cd "$OUT" && tar czf "$NAME.tar.gz" "$NAME" )
echo
echo ">> built $OUT/$NAME.tar.gz"
du -h "$OUT/$NAME.tar.gz" | awk '{print "   size: " $1}'
echo "   install with:  tar xzf $NAME.tar.gz && cd $NAME && ./install.sh"
