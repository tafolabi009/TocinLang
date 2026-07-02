#!/usr/bin/env bash
#
# make-run.sh — build a single self-extracting installer:  tocin-<ver>-linux.run
# Distro-agnostic (works where .deb/.rpm don't). Double-run or `sh foo.run`:
# it shows a GUI license/confirm dialog when zenity/kdialog exist, otherwise a
# terminal prompt, then extracts the embedded tarball and runs install.sh. No
# root required (installs to $HOME/.tocin by default).
#
# Usage: make-run.sh [--stage-tar FILE] [--out DIR]
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$REPO_ROOT/dist"; TARBALL=""
while [ $# -gt 0 ]; do case "$1" in
    --stage-tar) TARBALL="$2"; shift ;;
    --out) OUT="$2"; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
esac; shift; done
[ -n "$TARBALL" ] || TARBALL="$(ls -t "$OUT"/tocin-*-linux-*.tar.gz 2>/dev/null | head -1)"
[ -f "$TARBALL" ] || { echo "no tarball; run installer/package.sh --bundle-libs first" >&2; exit 1; }
VER="$(basename "$TARBALL" | sed -E 's/tocin-([0-9.]+)-.*/\1/')"
RUN="$OUT/tocin-${VER}-linux-x86_64.run"

# The stub prepended to the tarball. Everything below __ARCHIVE__ is the .tar.gz.
cat > "$RUN" <<'STUB'
#!/bin/sh
# Tocin self-extracting installer. The gzip tarball is appended after the
# __ARCHIVE__ marker; we find its byte offset and stream it to tar.
set -e
say() { printf '%s\n' "$*"; }
GUI=""
command -v zenity  >/dev/null 2>&1 && GUI=zenity
[ -z "$GUI" ] && command -v kdialog >/dev/null 2>&1 && GUI=kdialog

confirm() {  # $1 = message; returns 0 to proceed
    case "$GUI" in
        zenity)  zenity --question --title="Install Tocin" --width=420 --text="$1" ;;
        kdialog) kdialog --yesno "$1" ;;
        *) printf '%s\n[Enter] to install, Ctrl-C to cancel: ' "$1"; read _ </dev/tty 2>/dev/null || true ;;
    esac
}
notify() {
    case "$GUI" in
        zenity)  zenity --info --title="Tocin" --width=420 --text="$1" ;;
        kdialog) kdialog --msgbox "$1" ;;
        *) say "$1" ;;
    esac
}

confirm "Install the Tocin programming language into your home directory (~/.tocin)?

Includes the compiler, JIT, and a bundled linker — no system C compiler needed.
Click Yes to continue." || { say "Cancelled."; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
OFFSET=$(awk '/^__ARCHIVE__$/{print NR+1; exit}' "$0")
tail -n +"$OFFSET" "$0" | tar xz -C "$TMP"
DIR="$(ls -d "$TMP"/tocin-* | head -1)"
( cd "$DIR" && ./install.sh )
notify "Tocin $(cat "$DIR/VERSION" 2>/dev/null) installed to ~/.tocin.

Open a new terminal and run:  tocin --version"
exit 0
__ARCHIVE__
STUB
cat "$TARBALL" >> "$RUN"
chmod +x "$RUN"
echo ">> built $RUN ($(du -h "$RUN" | cut -f1))"
