#!/usr/bin/env bash
#
# make-link-recipe.sh - build the self-contained native-link bundle that lets
# `tocin file.to -o app` link a native executable WITHOUT a system gcc/clang.
#
# It asks the real C compiler what it would pass to the linker (`gcc -v` on a
# throwaway object), then turns that ground-truth link line into a relocatable
# recipe: every absolute input (CRT objects, static/import libs) is copied into
# <out-dir>/sysroot and the link line is rewritten with placeholders the Tocin
# compiler substitutes at link time:
#     %LINKDIR%  -> the bundle directory (.../libexec/link)
#     %OBJ%      -> the program object Tocin just emitted
#     %OUT%      -> the output executable
#
# The result is <out-dir>/link-recipe.txt + <out-dir>/sysroot/*. Drop a matching
# ld.lld(.exe) into <out-dir> and Tocin links natively with no external toolchain.
#
# Usage:
#   make-link-recipe.sh --gcc <gcc> --out-dir <dir> --runtime <libtocin_runtime.a>
#                       [--gc <libgc>] [--extra "<ld args>"]
#
# Run it under the SAME toolchain Tocin was built with (mingw64 on Windows).
set -euo pipefail

GCC=gcc
OUTDIR=""
RUNTIME=""
GC=""
STATIC=0
EXTRA="-lm -lpthread -lstdc++"
# KEEP_SYS=1 keeps the toolchain's original -L search dirs and relies on the
# target's system libs (libc/libgcc_s/...) at link time - correct on Linux/macOS,
# where those always exist. KEEP_SYS=0 (the Windows default) collapses search to
# the vendored sysroot and copies every -l input, so the bundle is fully
# self-contained: on Windows the mingw import libs are NOT present on the target,
# but the system DLLs they import (kernel32, ucrtbase, ...) always are.
KEEP_SYS=0

while [ $# -gt 0 ]; do
    case "$1" in
        --gcc) GCC="$2"; shift ;;
        --static) STATIC=1 ;;
        --out-dir) OUTDIR="$2"; shift ;;
        --runtime) RUNTIME="$2"; shift ;;
        --gc) GC="$2"; shift ;;
        --extra) EXTRA="$2"; shift ;;
        --keep-syslibs) KEEP_SYS=1 ;;
        -h|--help) sed -n '3,32p' "$0"; exit 0 ;;
        *) echo "make-link-recipe: unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

[ -n "$OUTDIR" ]  || { echo "make-link-recipe: --out-dir is required" >&2; exit 2; }
[ -n "$RUNTIME" ] || { echo "make-link-recipe: --runtime is required" >&2; exit 2; }
command -v "$GCC" >/dev/null 2>&1 || { echo "make-link-recipe: '$GCC' not found" >&2; exit 2; }

# Absolute-ify inputs so they appear as absolute paths in the captured link line
# and get vendored into sysroot (a relative path would be left untouched).
abspath() { case "$1" in /*|[A-Za-z]:[/\\]*) printf '%s' "$1" ;; *) printf '%s/%s' "$(pwd)" "$1" ;; esac; }
RUNTIME="$(abspath "$RUNTIME")"
[ -n "$GC" ] && GC="$(abspath "$GC")"

SYSROOT="$OUTDIR/sysroot"
mkdir -p "$SYSROOT"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
printf 'int main(void){return 0;}\n' > "$WORK/td.c"
"$GCC" -c "$WORK/td.c" -o "$WORK/td.o"

# Capture the real linker invocation. `gcc -v` prints the collect2/ld line,
# space-separated and unquoted (toolchain paths contain no spaces).
STATIC_FLAG=""
[ "$STATIC" = 1 ] && STATIC_FLAG="-static"
LINKLINE="$("$GCC" -v $STATIC_FLAG "$WORK/td.o" -o "$WORK/td_out" "$RUNTIME" ${GC:+$GC} $EXTRA 2>&1 \
            | grep -E '/collect2|[/\\]ld(\.exe)?( |$)|[/\\]ld\.lld' | tail -1 || true)"
[ -n "$LINKLINE" ] || { echo "make-link-recipe: could not capture the linker line from '$GCC -v'" >&2; exit 1; }

# Resolve a -l<name> to a real file via the compiler and copy it into sysroot.
# Tries the static archive first, then the import lib.
vendor_lib() {
    local name="$1" f
    for cand in "lib${name}.a" "lib${name}.dll.a"; do
        f="$("$GCC" -print-file-name="$cand")"
        if [ "$f" != "$cand" ] && [ -f "$f" ]; then
            cp -L "$f" "$SYSROOT/$(basename "$f")" 2>/dev/null || true
            return 0
        fi
    done
    return 1
}

# Walk the link line, vendoring inputs and rewriting paths to placeholders.
read -r -a TOKS <<< "$LINKLINE"
OUT_TOKS=()
HAVE_L=0
i=1                          # skip TOKS[0] = the collect2/ld program path
n=${#TOKS[@]}
while [ "$i" -lt "$n" ]; do
    t="${TOKS[$i]}"
    case "$t" in
        -plugin)        i=$((i+2)); continue ;;     # drop gcc LTO plugin (lld-incompatible)
        -plugin-opt*)   i=$((i+1)); continue ;;
        "$WORK/td.o")   OUT_TOKS+=("%OBJ%") ;;
        "$WORK/td_out") OUT_TOKS+=("%OUT%") ;;
        -o)             OUT_TOKS+=("-o" "%OUT%"); i=$((i+2)); continue ;;
        -L*)
            if [ "$HAVE_L" -eq 0 ]; then OUT_TOKS+=("-L%LINKDIR%/sysroot"); HAVE_L=1; fi
            [ "$KEEP_SYS" -eq 1 ] && OUT_TOKS+=("$t")   # keep system search dirs (Linux/macOS)
            ;;
        -l*)
            [ "$KEEP_SYS" -eq 0 ] && { vendor_lib "${t#-l}" || true; }
            OUT_TOKS+=("$t")
            ;;
        /*|[A-Za-z]:[/\\]*)
            # Absolute path: an input file (CRT object or explicit archive).
            if [ -f "$t" ]; then
                cp -L "$t" "$SYSROOT/$(basename "$t")" 2>/dev/null || true
                OUT_TOKS+=("%LINKDIR%/sysroot/$(basename "$t")")
            else
                OUT_TOKS+=("$t")
            fi ;;
        *)              OUT_TOKS+=("$t") ;;
    esac
    i=$((i+1))
done

# Ensure our sysroot is searched even if the captured line had no -L.
[ "$HAVE_L" -eq 0 ] && OUT_TOKS=("-L%LINKDIR%/sysroot" "${OUT_TOKS[@]}")

printf '%s ' "${OUT_TOKS[@]}" > "$OUTDIR/link-recipe.txt"
printf '\n' >> "$OUTDIR/link-recipe.txt"

echo "make-link-recipe: wrote $OUTDIR/link-recipe.txt"
echo "make-link-recipe: vendored $(find "$SYSROOT" -type f | wc -l) input file(s) into $SYSROOT"
