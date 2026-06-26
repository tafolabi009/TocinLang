#!/usr/bin/env bash
#
# Build-TocinInstaller.sh - turnkey, self-contained Tocin builder + packager for
# Linux. The counterpart to installer/windows/Build-TocinInstaller.ps1.
#
# On a clean machine this single command:
#   1. installs the build toolchain (apt / dnf / pacman / zypper),
#   2. configures and builds the compiler and runtime,
#   3. bundles every non-system shared library (LLVM, GC, libffi, ...), and
#   4. produces dist/tocin-<ver>-linux-<arch>.tar.gz
# that runs on any compatible Linux with no dependencies beyond glibc. Extract
# it and run ./install.sh to put `tocin` on PATH.
#
# Usage:
#   installer/linux/Build-TocinInstaller.sh [--skip-deps] [--no-python] [--out DIR]
#
#   --skip-deps   Toolchain already present; do not install system packages.
#   --no-python   Configure WITH_PYTHON=OFF for a slimmer, Python-free package.
#   --out DIR     Output directory for the tarball (default: ./dist).
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SKIP_DEPS=0
NO_PYTHON=0
OUT="$REPO_ROOT/dist"

log()  { printf '\033[0;32m>>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m!!\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[0;31mxx\033[0m %s\n' "$*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --skip-deps) SKIP_DEPS=1 ;;
        --no-python) NO_PYTHON=1 ;;
        --out) OUT="${2:?--out needs a directory}"; shift ;;
        -h|--help) sed -n '3,26p' "$0"; exit 0 ;;
        *) die "unknown option: $1 (try --help)" ;;
    esac
    shift
done

# ---------------------------------------------------------------------------
# 1. Toolchain. Best-effort: a wrong package name on one distro must not abort
#    the whole build, so each manager's install is allowed to fail with a warning.
# ---------------------------------------------------------------------------
install_deps() {
    local SUDO=""
    [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1 && SUDO=sudo

    if command -v apt-get >/dev/null 2>&1; then
        log "installing dependencies via apt"
        local pkgs="build-essential cmake ninja-build git patchelf \
            llvm-18-dev libllvm18 libgc-dev libffi-dev zlib1g-dev"
        [ "$NO_PYTHON" = 0 ] && pkgs="$pkgs libpython3-dev python3-dev"
        $SUDO apt-get update -y || warn "apt-get update failed; continuing"
        # shellcheck disable=SC2086
        $SUDO apt-get install -y --no-install-recommends $pkgs \
            || warn "some apt packages failed; if llvm-18-dev is unavailable, install your distro's llvm-dev"
    elif command -v dnf >/dev/null 2>&1; then
        log "installing dependencies via dnf"
        local pkgs="gcc-c++ cmake ninja-build git patchelf llvm-devel libffi-devel zlib-devel gc-devel"
        [ "$NO_PYTHON" = 0 ] && pkgs="$pkgs python3-devel"
        # shellcheck disable=SC2086
        $SUDO dnf install -y $pkgs || warn "some dnf packages failed; continuing"
    elif command -v pacman >/dev/null 2>&1; then
        log "installing dependencies via pacman"
        local pkgs="base-devel cmake ninja git patchelf llvm libffi zlib gc"
        [ "$NO_PYTHON" = 0 ] && pkgs="$pkgs python"
        # shellcheck disable=SC2086
        $SUDO pacman -Sy --needed --noconfirm $pkgs || warn "some pacman packages failed; continuing"
    elif command -v zypper >/dev/null 2>&1; then
        log "installing dependencies via zypper"
        local pkgs="gcc-c++ cmake ninja git patchelf llvm-devel libffi-devel zlib-devel gc-devel"
        [ "$NO_PYTHON" = 0 ] && pkgs="$pkgs python3-devel"
        # shellcheck disable=SC2086
        $SUDO zypper install -y $pkgs || warn "some zypper packages failed; continuing"
    else
        warn "no known package manager (apt/dnf/pacman/zypper); assuming the toolchain is already installed"
    fi
}

if [ "$SKIP_DEPS" = 0 ]; then
    install_deps
else
    log "skipping dependency install (--skip-deps)"
fi

command -v cmake >/dev/null 2>&1 || die "cmake not found. Re-run without --skip-deps, or install it manually."

# ---------------------------------------------------------------------------
# 2. Locate an LLVM CMake package. Prefer 18 (the version Tocin targets), then
#    fall back to whatever llvm-config advertises.
# ---------------------------------------------------------------------------
LLVM_DIR_ARG=""
for d in /usr/lib/llvm-18/lib/cmake/llvm /usr/lib/llvm-18/cmake \
         /usr/lib/cmake/llvm /usr/lib64/cmake/llvm; do
    if [ -d "$d" ]; then LLVM_DIR_ARG="-DLLVM_DIR=$d"; break; fi
done
if [ -z "$LLVM_DIR_ARG" ]; then
    for cfg in llvm-config-18 llvm-config; do
        if command -v "$cfg" >/dev/null 2>&1; then
            d="$("$cfg" --cmakedir 2>/dev/null || true)"
            [ -n "$d" ] && { LLVM_DIR_ARG="-DLLVM_DIR=$d"; break; }
        fi
    done
fi
[ -n "$LLVM_DIR_ARG" ] && log "using ${LLVM_DIR_ARG#-DLLVM_DIR=}" || warn "no explicit LLVM_DIR found; letting CMake search"

# ---------------------------------------------------------------------------
# 3. Configure + build.
# ---------------------------------------------------------------------------
# Reuse the generator an existing build/ was created with (avoids CMake's
# "does not match the generator used previously" error on a dev checkout);
# otherwise prefer Ninja, falling back to Make.
GEN=""
if [ -f "$REPO_ROOT/build/CMakeCache.txt" ]; then
    GEN="$(grep -m1 '^CMAKE_GENERATOR:' "$REPO_ROOT/build/CMakeCache.txt" | cut -d= -f2- || true)"
fi
if [ -z "$GEN" ]; then
    GEN="Unix Makefiles"
    command -v ninja >/dev/null 2>&1 && GEN="Ninja"
fi
PY_ARG=""
[ "$NO_PYTHON" = 1 ] && PY_ARG="-DWITH_PYTHON=OFF"

log "configuring (generator: $GEN)"
# shellcheck disable=SC2086
cmake -S "$REPO_ROOT" -B "$REPO_ROOT/build" -G "$GEN" \
    -DCMAKE_BUILD_TYPE=Release $LLVM_DIR_ARG $PY_ARG

log "building tocin + runtime"
cmake --build "$REPO_ROOT/build" --target tocin tocin_runtime_shared \
    -j"$(nproc 2>/dev/null || echo 4)"

# ---------------------------------------------------------------------------
# 4. Bundle every non-system shared library and tar it up. Delegates to the
#    proven packager so there is a single source of truth for the layout.
# ---------------------------------------------------------------------------
log "packaging self-contained tarball"
bash "$REPO_ROOT/installer/package.sh" --bundle-libs --out "$OUT"

echo
log "done - self-contained Linux package written to: $OUT"
log "test it with:  tar xzf $OUT/tocin-*-linux-*.tar.gz && cd tocin-* && ./install.sh"
