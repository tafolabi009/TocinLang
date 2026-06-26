#!/usr/bin/env bash
#
# get-tocin.sh — one-line installer (Linux/macOS):
#
#   curl -fsSL https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/get-tocin.sh | sh
#
# Detects your OS/arch, downloads the matching release tarball from GitHub, and
# runs its install.sh (per-user, adds to PATH, writes an uninstaller).
#
# Override the version or repo:
#   TOCIN_VERSION=0.1.0 TOCIN_REPO=tafolabi009/TocinLang sh get-tocin.sh
set -eu

REPO="${TOCIN_REPO:-tafolabi009/TocinLang}"
VERSION="${TOCIN_VERSION:-latest}"

case "$(uname -s)" in
    Linux)  OS=linux ;;
    Darwin) OS=macos ;;
    *) echo "Unsupported OS: $(uname -s). On Windows use install.ps1." >&2; exit 1 ;;
esac
case "$(uname -m)" in
    x86_64|amd64) ARCH=x86_64 ;;
    arm64|aarch64) ARCH=arm64 ;;
    *) ARCH="$(uname -m)" ;;
esac

if [ "$VERSION" = latest ]; then
    # Resolve the latest release tag via the GitHub API.
    VERSION="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
        | grep -m1 '"tag_name"' | sed -E 's/.*"tag_name":[[:space:]]*"v?([^"]+)".*/\1/')"
    [ -n "$VERSION" ] || { echo "Could not determine latest release. Set TOCIN_VERSION." >&2; exit 1; }
fi

NAME="tocin-$VERSION-$OS-$ARCH"
URL="https://github.com/$REPO/releases/download/v$VERSION/$NAME.tar.gz"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo ">> downloading $URL"
curl -fSL "$URL" -o "$TMP/pkg.tar.gz" || { echo "download failed (is the release published?)" >&2; exit 1; }
tar xzf "$TMP/pkg.tar.gz" -C "$TMP"
sh "$TMP/$NAME/install.sh"
