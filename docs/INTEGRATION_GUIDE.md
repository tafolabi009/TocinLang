# Integration & Interop Status

What is actually wired into the shipped `tocin` binary versus what exists as
scaffolding in the tree. This page replaces an older guide that described
unbuilt subsystems as if they were live.

## Interop matrix

| Integration | Status | Notes |
|---|---|---|
| **C FFI** | ✅ **Works** (JIT + native) | `extern def name(...) -> T;` then call like any function. Under `--run` symbols resolve from the process (libc/libm available out of the box); native builds link them normally. See [ffi.md](ffi.md). |
| **Python FFI** | ⚠️ Experimental scaffold | Behind the `WITH_PYTHON` CMake option (embeds CPython). Off by default in release packages; not a supported surface yet. |
| **JavaScript / V8** | ❌ Not functional | All real builds configure `-DWITH_V8=OFF`. The `src/v8_integration/` sources exist but are not part of a working feature. |
| **WebAssembly target** | ⚠️ Parsed, not a supported path | `--target wasm` exists; native is the supported target. |

## Concurrency runtime (what `go` really does)

`go f(args)` spawns a **real OS thread** via the `__tocin_go` runtime
(1:1 threading), and channels/`select` synchronize threads. The fiber-based
`src/runtime/lightweight_scheduler.*` ("millions of goroutines" M:N substrate)
is **not linked into the runtime** — it is design scaffolding; the plan for
wiring it under `async`/`await` is written up in
[async-scheduler-design.md](async-scheduler-design.md). `async def`/`await`
run today with eager semantics (correct results, no suspension).

## Optimization pipeline (what `-O3` really runs)

The driver uses LLVM's **new PassBuilder** pipeline with a configured
TargetMachine (so `--native` CPU features reach the vectorizer), plus
whole-program internalization at `-O3` for executables and JIT runs. The
separate `src/compiler/advanced_optimizations.cpp` (PGO/polyhedral
scaffolding) is **not on the default path**.

## Embedding the compiler

The build produces reusable pieces:

- `tocin` — the CLI (JIT, AOT, `check`/`new`/`doc`).
- `libtocin_runtime.a` / `tocin_runtime_shared` — the runtime (GC allocator,
  goroutines/channels, strings/vectors/maps, TCP/time/hash/random), linkable
  from C/C++; every entry point is a C symbol prefixed `__tocin_`.
- The C++ compiler libraries (lexer/parser/type/codegen) that `tocin` itself
  is built from — usable in-tree (e.g. the unit tests link them), though the
  C++ API is not a stability-guaranteed surface.

## Calling Tocin from other languages

Compile to an object file and link it like C:

```bash
tocin lib.to -o lib.o
cc main.c lib.o -o app        # Tocin functions are plain C symbols
```

Exported Tocin functions use the C ABI for `int`/`float`/pointer parameters
(see [language-reference.md §17–18](language-reference.md#17-ffi)); pair with
`--freestanding` for runtime-free objects in kernels or embedded targets.
