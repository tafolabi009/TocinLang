#!/usr/bin/env bash
#
# install.sh — install Tocin into a per-user prefix, wire up PATH, and write an
# uninstaller. Run this from inside an extracted Tocin package directory:
#
#   tar xzf tocin-<ver>-<os>-<arch>.tar.gz
#   cd tocin-<ver>-<os>-<arch>
#   ./install.sh
#
# Environment / flags:
#   TOCIN_HOME=<dir>   install prefix (default: $HOME/.tocin)
#   --prefix <dir>     same as TOCIN_HOME
#   --no-modify-path   don't touch shell rc files
set -eu

SRC="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${TOCIN_HOME:-$HOME/.tocin}"
MODIFY_PATH=1

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift ;;
        --no-modify-path) MODIFY_PATH=0 ;;
        -h|--help) echo "usage: ./install.sh [--prefix DIR] [--no-modify-path]"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

[ -x "$SRC/libexec/tocin" ] || { echo "error: run this from inside an extracted Tocin package" >&2; exit 1; }
VER="$(cat "$SRC/VERSION" 2>/dev/null || echo unknown)"

case "$(uname -s)" in
    Darwin) LIBVAR=DYLD_LIBRARY_PATH ;;
    *)      LIBVAR=LD_LIBRARY_PATH ;;
esac

echo ">> installing Tocin $VER into $PREFIX"
# Clean any previous payload but keep the prefix (it may be a custom dir).
for d in bin libexec lib stdlib share; do rm -rf "${PREFIX:?}/$d"; done
mkdir -p "$PREFIX/bin"
cp -R "$SRC/libexec" "$PREFIX/libexec"
cp -R "$SRC/lib"     "$PREFIX/lib"
cp -R "$SRC/stdlib"  "$PREFIX/stdlib"
cp -R "$SRC/share"   "$PREFIX/share"
cp    "$SRC/VERSION" "$PREFIX/VERSION"
[ -f "$SRC/LICENSE" ] && cp "$SRC/LICENSE" "$PREFIX/LICENSE" || true

# --- Relocatable launcher -------------------------------------------------
# Resolves its own location, so the install can be moved freely.
cat > "$PREFIX/bin/tocin" <<EOF
#!/bin/sh
HERE="\$(cd "\$(dirname "\$0")/.." && pwd)"
export TOCIN_PATH="\${TOCIN_PATH:-\$HERE/stdlib}"
export $LIBVAR="\$HERE/lib:\${$LIBVAR:-}"
exec "\$HERE/libexec/tocin" "\$@"
EOF
chmod +x "$PREFIX/bin/tocin"

# --- Uninstaller ----------------------------------------------------------
cat > "$PREFIX/uninstall.sh" <<EOF
#!/bin/sh
# Remove Tocin and its PATH entry.
set -e
echo "Removing Tocin from $PREFIX"
for rc in "\$HOME/.bashrc" "\$HOME/.zshrc" "\$HOME/.profile"; do
    [ -f "\$rc" ] || continue
    tmp="\$rc.tocin.tmp"
    sed '/# >>> tocin >>>/,/# <<< tocin <<</d' "\$rc" > "\$tmp" && mv "\$tmp" "\$rc"
done
rm -rf "$PREFIX"
echo "Tocin uninstalled. Open a new shell for PATH changes to take effect."
EOF
chmod +x "$PREFIX/uninstall.sh"

# --- PATH wiring ----------------------------------------------------------
if [ "$MODIFY_PATH" = 1 ]; then
    BLOCK_START="# >>> tocin >>>"
    BLOCK_END="# <<< tocin <<<"
    LINE="export PATH=\"$PREFIX/bin:\$PATH\""
    added=0
    for rc in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
        [ -e "$rc" ] || { [ "$rc" = "$HOME/.profile" ] && touch "$rc" || continue; }
        if ! grep -qF "$BLOCK_START" "$rc" 2>/dev/null; then
            printf '\n%s\n%s\n%s\n' "$BLOCK_START" "$LINE" "$BLOCK_END" >> "$rc"
            added=1
        fi
    done
    [ "$added" = 1 ] && echo ">> added $PREFIX/bin to PATH (in your shell rc files)"
fi

echo
echo "Tocin $VER installed."
echo "  binary : $PREFIX/bin/tocin"
echo "  stdlib : $PREFIX/stdlib"
echo "  docs   : $PREFIX/share/docs   (tutorial.md, language-reference.md, ...)"
echo "  remove : $PREFIX/uninstall.sh"
echo
echo "Open a new terminal (or 'source ~/.profile'), then:  tocin --version"
echo "Note: native '-o' output links via a system C compiler (cc/clang); the"
echo "JIT ('tocin file.to --run') needs no external tools."
