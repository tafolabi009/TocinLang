#!/usr/bin/env bash
#
# build-all.sh — one command to produce every installer this platform can make.
#
#   Linux : tocin-<ver>-linux-<arch>.tar.gz   (portable, bundled libs)
#           tocin-<ver>-linux-<arch>.run      (self-extracting GUI installer)
#           tocin_<ver>_<arch>.deb            (Debian/Ubuntu; GUI via app store)
#   macOS : tocin-<ver>-macos-<arch>.pkg/.dmg (native Installer GUI)  [on macOS]
#   Windows: run installer/windows/Build-TocinInstaller.ps1 (makes .zip + .exe)
#
# All outputs are self-contained: the bundled ld.lld link recipe means native
# `-o` compilation needs NO system C toolchain, and --bundle-libs vendors LLVM/
# GC/etc. so the compiler itself has no runtime dependency beyond libc.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$REPO_ROOT/dist"
mkdir -p "$OUT"

case "$(uname -s)" in
  Linux)
    echo "== staging portable tarball + link bundle =="
    bash "$REPO_ROOT/installer/package.sh" --bundle-libs --out "$OUT"
    STAGE="$(ls -d "$OUT"/tocin-*-linux-* 2>/dev/null | head -1)"

    echo "== .run self-extracting installer =="
    bash "$REPO_ROOT/installer/linux/make-run.sh" --out "$OUT" || echo "!! .run skipped"

    if command -v dpkg-deb >/dev/null 2>&1; then
      echo "== .deb package =="
      bash "$REPO_ROOT/installer/linux/make-deb.sh" --stage "$STAGE" --out "$OUT" || echo "!! .deb skipped"
    else
      echo "!! dpkg-deb not found; skipping .deb"
    fi
    ;;
  Darwin)
    echo "== staging portable tarball + link bundle =="
    bash "$REPO_ROOT/installer/package.sh" --bundle-libs --out "$OUT"
    echo "== .pkg + .dmg =="
    bash "$REPO_ROOT/installer/macos/make-pkg.sh" --out "$OUT" || echo "!! pkg/dmg skipped"
    ;;
  *)
    echo "On Windows, run:  installer\\windows\\Build-TocinInstaller.ps1" >&2
    exit 1
    ;;
esac

echo
echo "== artifacts in $OUT =="
ls -lh "$OUT" | awk 'NR>1 {print "   " $5 "  " $9}'
