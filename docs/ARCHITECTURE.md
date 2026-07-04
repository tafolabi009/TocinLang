# Tocin Compiler Architecture

How the Tocin compiler is put together. Tocin is a **statically typed,
LLVM-compiled** language: programs are either JIT-executed in-process or
compiled ahead-of-time to native binaries. There is **no tree-walking
interpreter and no bytecode VM** — LLVM is the single execution substrate.

## Pipeline

```
Source (.to)
   → Lexer          src/lexer/         tokens (indentation/brace blocks, literals)
   → Macro expander                    token-level name!(args) expansion
   → Parser         src/parser/        recursive descent → AST
   → Import resolver src/main.cpp      merge imported files (TOCIN_PATH, stdlib/)
   → Type checker   src/type/          strict two-pass: hoist signatures, then check
   → IR generator   src/codegen/       LLVM IR (two-pass; DataLayout set up front)
   → Optimizer      LLVM PassBuilder   -O0..-O3 with a configured TargetMachine
   → ┬ JIT          ORCv2 LLJIT        --run: execute main() in-process
     └ AOT          TargetMachine      -o: .ll / .s / .o / linked executable
```

The driver (`src/main.cpp`) orchestrates everything and also implements the
subcommands: `tocin check` (stop after the type checker), `tocin new`
(scaffold), `tocin doc` (signatures + `//` doc comments → Markdown).

## Components

### Lexer (`src/lexer/`)

Hand-written tokenizer: indentation- and brace-blocks, `0x`/`0o`/`0b` and `_`
digit separators, string escapes, line/column tracking on every token.
Historic log-level keywords (`log`, `warn`, `error`, …) were unreserved so
they work as ordinary identifiers.

### Parser (`src/parser/`)

Recursive descent producing the AST in `src/ast/`. Notable mechanics: the
ternary conditional sits between assignment and elvis in the precedence chain;
`>>` is split when it closes nested generic type annotations; parameters may
carry default-value expressions; `switch` parses as an alias of `match`.

### Type checker (`src/type/`)

Two-pass (hoist all signatures, then check bodies) and **strict by default**:
unknown identifiers, wrong arity — including builtins, via the shared registry
in `src/type/builtin_names.h` — and type mismatches are hard errors;
`--permissive` downgrades them. Also here: generic monomorphization typing,
trait-bound enforcement (`T016`), match exhaustiveness (`P001`), `const`
enforcement (`T013`), and the opt-in borrow checker (`B001`/`B002`).

### Diagnostics (`src/error/`)

Rustc-style rendering: the offending source line with a caret underline,
ANSI colors gated on TTY detection and `NO_COLOR`, bounded-edit-distance
"did you mean '…'?" suggestions, and a final `N errors generated.` summary.
Codegen errors carry source locations threaded through the IR generator.

### IR generator (`src/codegen/`)

Two-pass LLVM IR emission (declaration order doesn't matter). Sets the module
triple and DataLayout **before** generating IR so layout-sensitive folding is
correct. Lowers: classes (opaque heap objects), monomorphized generics, trait
objects (`{ i64 typeId, ptr data }` boxes with virtual dispatch), algebraic
enums (`[tag][payload…]`), closures (read captures snapshot, write captures
share a cell), generators (eager collection), exceptions (setjmp/longjmp
handler stack), `defer`/RAII destructors, and the kernel primitives (volatile
loads/stores, `fence`, inline `asm` with operands/constraints/clobbers, raw
memory ops). Emits runtime traps — division/modulo-by-zero and bounds checks —
as branches to a panic path that reports `panic: <msg> at file:line:col`.
Math builtins lower to LLVM intrinsics (`sqrt`, `fabs`, `pow`, …) so loops
vectorize; buffer accesses get `noalias`-style scoped-alias metadata so
independent buffers optimize as if `restrict`-qualified.

### Optimizer (driver, `src/main.cpp`)

LLVM's **new PassBuilder** pipelines at `-O0`..`-O3` (default `-O2`), built
with a **configured TargetMachine** so `--native` CPU features (POPCNT, AVX…)
reach the cost models and vectorizer — not just the backend. For executables
and JIT runs at `-O3`, non-`main` symbols are **internalized** first
(whole-program optimization); object/IR/assembly outputs and `--freestanding`
skip internalization so their symbols stay exported.

### Execution

- **JIT** (`--run`): ORCv2 LLJIT; the runtime is registered in-process, so no
  external tools are needed.
- **AOT** (`-o`): TargetMachine emits the object; executables link through the
  system C compiler **or**, in installed packages, through the **bundled
  `ld.lld` + static link recipe** (`libexec/link/`), which needs no system
  toolchain at all — see [native-linking.md](native-linking.md). On Linux the
  bundled-recipe output is fully static.
- **Freestanding** (`--freestanding`): a relocatable object with no
  libc/GC/runtime references, for kernels and bare metal.

### Runtime (`src/runtime/`, linked as `libtocin_runtime`)

C-ABI symbols prefixed `__tocin_`:

- **Memory**: allocation through the **Boehm conservative GC**
  (`GC_malloc`; `__tocin_alloc_atomic` for pointer-free data like string
  bytes). `--no-gc` degrades allocation to `malloc`. `free`/`vecFree`/
  `mapFree` allow eager release.
- **Concurrency**: `go` spawns **real OS threads** (`__tocin_go`); typed
  channels and `select` synchronize them. (The fiber-based
  `lightweight_scheduler.*` M:N substrate exists in-tree but is **not
  linked** — see [async-scheduler-design.md](async-scheduler-design.md);
  `async`/`await` run eagerly today.)
- **Services**: strings (fast `intToStr`, `bufToStr`), vectors/maps (64-bit
  slots), TCP sockets, time, FNV-1a/splitmix64 hashing, seeded random,
  env/exit, file I/O.
- **Failure**: `__tocin_panic`/`__tocin_oob` print the located panic message,
  flush stdout, and abort.

## Memory model (summary)

Heap values (instances, arrays, strings, closures, Option/Result, channels)
are opaque pointers allocated via the GC. Channels, Option/Result payloads,
vector/map elements, and thrown exception payloads move through a uniform
**64-bit slot** ABI. Strings are immutable NUL-terminated `char*`, compared by
value with `==`. Full details: [language-reference.md §18](language-reference.md#18-memory-model--abi).

## Testing architecture

| Suite | Runner | What it covers |
|---|---|---|
| C++ unit tests (`tests/*.cpp`) | `ctest` | lexer/parser/type-checker/codegen internals |
| `.to` integration programs (`tests/cases/`) | `scripts/run_to_tests.sh` (compiles to IR, executes under `lli`) | language semantics that need no runtime symbols |
| JIT runtime + stdlib suites (`tests/jit/`) | `tests/run_stdlib_tests.sh` (runs `tocin --run`) | GC/vectors/maps/strings, traps, kernel primitives, and all 34 stdlib modules (`// expect: N` convention) |

CI (`.github/workflows/ci.yml`): Linux Release+Debug are authoritative;
macOS/Windows are best-effort; static analysis is advisory.

## Distribution

`installer/` builds self-contained packages per platform — Windows NSIS
`Setup.exe` + zip, Debian `.deb`, self-extracting `.run`, macOS `.pkg`/`.dmg`,
and portable tarballs — each bundling the compiler, runtime, stdlib, and the
`ld.lld` link bundle. See [INSTALLER_GUIDE.md](INSTALLER_GUIDE.md) and
[../INSTALL.md](../INSTALL.md).
