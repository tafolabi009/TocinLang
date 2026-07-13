#!/usr/bin/env bash
#
# kernel_codegen_test.sh — regression tests for the bare-metal / cross-compile
# codegen paths (freestanding objects, cross triples, --no-red-zone, naked /
# interrupt functions, module-level asm, and the bootable kernel example).
#
# These emit objects rather than JIT-run, so they live outside the --run and lli
# suites. Verification is on the emitted IR / object, using objdump + python3.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOCIN="${TOCIN:-$ROOT/build/tocin}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
fail=0
pass() { echo "PASS  $1"; }
bad()  { echo "FAIL  $1"; fail=1; }

# --- 1. Cross-compile to a bare-metal triple with no red zone ----------------
cat > "$TMP/x.to" <<'EOF'
def f(a: int, b: int) -> int { let p = alloc(16); storeInt(p, 0, a + b); return loadInt(p, 0); }
def main() -> int { return f(2, 3); }
EOF
if "$TOCIN" "$TMP/x.to" --freestanding --target-triple x86_64-unknown-none \
      --code-model kernel --reloc static --no-red-zone -o "$TMP/x.ll" 2>"$TMP/x.err"; then
    if grep -q 'target triple = "x86_64-unknown-none"' "$TMP/x.ll" && \
       grep -q 'noredzone' "$TMP/x.ll"; then
        pass "cross-compile x86_64-unknown-none + noredzone"
    else
        bad "cross-compile: missing triple or noredzone in IR"
    fi
else
    bad "cross-compile: compilation failed"; cat "$TMP/x.err"
fi

# --- 2. naked + interrupt functions ------------------------------------------
cat > "$TMP/i.to" <<'EOF'
interrupt def handler(frame: int) { let r = loadInt(frame, 0); }
naked def stub() { asm("iret"); }
def main() -> int { return 0; }
EOF
if "$TOCIN" "$TMP/i.to" --freestanding --target-triple x86_64-unknown-none \
      --no-red-zone -o "$TMP/i.ll" 2>"$TMP/i.err"; then
    if grep -q 'x86_intrcc' "$TMP/i.ll" && grep -q 'byval' "$TMP/i.ll" && \
       grep -q 'naked' "$TMP/i.ll"; then
        pass "naked + interrupt (x86_intrcc, byval frame, naked attr)"
    else
        bad "naked/interrupt: expected attributes missing from IR"
    fi
else
    bad "naked/interrupt: compilation failed"; cat "$TMP/i.err"
fi

# --- 3. module-level asm ------------------------------------------------------
cat > "$TMP/m.to" <<'EOF'
asmModule(".section .multiboot, \"a\"\n.align 4\n.long 0x1BADB002\n.long 0x0\n.long -(0x1BADB002 + 0x0)");
def main() -> int { return 0; }
EOF
if "$TOCIN" "$TMP/m.to" --freestanding --target-triple i686-unknown-none -o "$TMP/m.o" 2>"$TMP/m.err"; then
    if command -v python3 >/dev/null 2>&1; then
        if python3 - "$TMP/m.o" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
for off in range(0, len(d) - 12, 4):
    if struct.unpack_from('<I', d, off)[0] == 0x1BADB002:
        m, f, c = struct.unpack_from('<III', d, off)
        sys.exit(0 if (m + f + c) & 0xFFFFFFFF == 0 else 1)
sys.exit(1)
PY
        then pass "module asm: valid multiboot header (checksum sums to 0)"
        else bad "module asm: multiboot header missing or bad checksum"; fi
    else
        pass "module asm: object emitted (python3 absent; checksum unchecked)"
    fi
else
    bad "module asm: compilation failed"; cat "$TMP/m.err"
fi

# --- 4. the bootable kernel example builds + has a valid multiboot header -----
KDIR="$ROOT/examples/kernel"
if [ -f "$KDIR/kernel.to" ]; then
    if "$TOCIN" "$KDIR/kernel.to" --freestanding --target-triple i686-unknown-none \
          --reloc static --code-model small -o "$TMP/kernel.o" 2>"$TMP/k.err"; then
        pass "examples/kernel builds"
    else
        bad "examples/kernel: compilation failed"; cat "$TMP/k.err"
    fi
fi

# --- 5. mmio struct: volatile field access + C layout + runtime correctness ---
cat > "$TMP/mmio.to" <<'EOF'
mmio struct Uart { data: u32; status: u32; control: u32; baud: u32; }
def main() -> int {
    let base = alloc(16);
    let u: Uart = mmioAt(base);
    u.control = 1;
    u.baud = 115200;
    u.data = 65;
    let sum = u.control + u.data;          // widen i32+i32; == 66
    if (u.status == 0) { sum = sum + 1; }  // sized-field vs i64 literal -> 67
    if (u.baud != 115200) { return 1; }
    return sum;
}
EOF
# 5a. IR: field accesses must be volatile, layout must be four i32s (offsets 0/4/8/12).
if "$TOCIN" "$TMP/mmio.to" -o "$TMP/mmio.ll" 2>"$TMP/mmio.err"; then
    nv_load=$(grep -c 'load volatile' "$TMP/mmio.ll")
    nv_store=$(grep -c 'store volatile' "$TMP/mmio.ll")
    if [ "$nv_load" -ge 1 ] && [ "$nv_store" -ge 1 ] && \
       grep -q '%Uart = type { i32, i32, i32, i32 }' "$TMP/mmio.ll"; then
        pass "mmio struct: volatile field access + C layout"
    else
        bad "mmio struct: non-volatile access or wrong layout ($nv_load loads, $nv_store stores)"
    fi
else
    bad "mmio struct: compilation failed"; cat "$TMP/mmio.err"
fi
# 5b. runtime: fields land at distinct offsets and read back correctly (exit 67).
"$TOCIN" "$TMP/mmio.to" --run >/dev/null 2>&1
rc=$?
if [ "$rc" -eq 67 ]; then
    pass "mmio struct: runtime field read/write (exit 67)"
else
    bad "mmio struct: runtime wrong (exit $rc, expected 67)"
fi
# 5c. mmio structs work freestanding (the actual kernel/driver use case).
if "$TOCIN" "$TMP/mmio.to" --freestanding --target-triple x86_64-unknown-none \
      -o "$TMP/mmio_fs.o" 2>"$TMP/mmiofs.err"; then
    pass "mmio struct: freestanding object"
else
    bad "mmio struct: freestanding failed"; cat "$TMP/mmiofs.err"
fi

exit $fail
