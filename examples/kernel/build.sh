#!/usr/bin/env bash
#
# Build the Tocin multiboot kernel into a bootable ELF.
#
#   ./build.sh            # produces kernel.elf
#   TOCIN=/path/to/tocin ./build.sh
#
# Needs: the tocin compiler + an LLVM lld (ld.lld). No gcc/binutils required —
# tocin cross-compiles the object and ld.lld links it. If ld.lld is missing the
# script falls back to GNU ld with an i386 emulation.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TOCIN="${TOCIN:-tocin}"
OUT="${OUT:-$HERE/kernel.elf}"

echo ">> compiling kernel.to -> kernel.o (i686 freestanding)"
"$TOCIN" "$HERE/kernel.to" \
    --freestanding \
    --target-triple i686-unknown-none \
    --reloc static \
    --code-model small \
    -o "$HERE/kernel.o"

echo ">> linking kernel.elf (multiboot, load at 1 MiB)"
if command -v ld.lld >/dev/null 2>&1; then
    ld.lld -m elf_i386 -T "$HERE/linker.ld" -o "$OUT" "$HERE/kernel.o"
elif command -v ld >/dev/null 2>&1; then
    ld -m elf_i386 -T "$HERE/linker.ld" -o "$OUT" "$HERE/kernel.o"
else
    echo "!! no linker found (install lld or binutils)" >&2
    exit 1
fi

echo ">> done: $OUT"
echo "   boot it:  qemu-system-i386 -kernel $OUT -serial stdio"
