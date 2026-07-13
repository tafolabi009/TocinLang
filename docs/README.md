# Tocin Documentation

The documentation set for the Tocin language and compiler. Start here.

## Start here

| Doc | What it is |
|---|---|
| [tutorial.md](tutorial.md) | **The from-scratch walkthrough** — install, first program, then every feature with runnable code. The best way to learn Tocin. |
| [02_Getting_Started.md](02_Getting_Started.md) | Install the toolchain and run/compile your first programs (`--run`, `-o`, `check`, `new`, `doc`). |
| [01_Introduction.md](01_Introduction.md) | What Tocin is and its design goals. |
| [../INSTALL.md](../INSTALL.md) | **Installing a released Tocin** — native installers (Windows Setup.exe, .deb, .run, macOS .pkg/.dmg), quick-install one-liners, and manual installs. |

## Reference

| Doc | What it is |
|---|---|
| [language-reference.md](language-reference.md) | **The authoritative reference** — grammar, types, operators & precedence, every statement and feature, diagnostics, CLI, memory model, and the built-in function reference. Every snippet verified against the in-tree compiler. |
| [stdlib-reference.md](stdlib-reference.md) | Every built-in (no-import) function with signatures and examples. |
| [04_Standard_Library.md](04_Standard_Library.md) | Guided tour of the 34 standard-library modules (`std`, `data`, `math`, `ml`, `web`, `game`, `gui`, …). |
| [STDLIB_GUIDE.md](STDLIB_GUIDE.md) | Practical stdlib guide — imports, `TOCIN_PATH`, writing tests with `std.testing`. |
| [tocin-for-ai.md](tocin-for-ai.md) | A dense, exact spec of the implemented language for LLMs/tooling — write correct Tocin on the first try. |

## Language topics

| Doc | What it is |
|---|---|
| [03_Language_Basics.md](03_Language_Basics.md) | Variables, control flow, functions, classes. |
| [ERROR_HANDLING.md](ERROR_HANDLING.md) | Strict diagnostics, runtime panics/traps, exceptions, `Option`/`Result`. |
| [NULL_SAFETY.md](NULL_SAFETY.md) | `?.`, `?:`, `!!` over nullable references. |
| [OPTION_RESULT_TYPES.md](OPTION_RESULT_TYPES.md) | `Some`/`None`/`Ok`/`Err` and pattern matching. |
| [TRAITS.md](TRAITS.md) | Traits, `impl` blocks, trait objects. |
| [CONCURRENCY.md](CONCURRENCY.md) | Goroutines, channels, `select`. |
| [LINQ.md](LINQ.md) | Query-style collection operations (`std.linq`, `std.functional`). |
| [LANGUAGE_FEATURES.md](LANGUAGE_FEATURES.md) | Feature survey with examples. |
| [05_Advanced_Topics.md](05_Advanced_Topics.md) | Ownership, advanced generics, FFI, WASM (partly design-intent — see the note at its top). |
| [ffi.md](ffi.md) | Calling C from Tocin (`extern def`); Python/JS FFI status. |

## Building, running, performance

| Doc | What it is |
|---|---|
| [BUILDING.md](BUILDING.md) | Building the compiler from source (CMake + LLVM 18–22). |
| [INTERPRETER_GUIDE.md](INTERPRETER_GUIDE.md) | Running programs: the JIT (`--run`), native builds (`-o`), and what the REPL actually does. |
| [INSTALLER_GUIDE.md](INSTALLER_GUIDE.md) | Building the native installers (points at the real `installer/` scripts). |
| [native-linking.md](native-linking.md) | How `tocin file.to -o app` links with **no system compiler** (bundled `ld.lld` + static recipe). |
| [kernel-development.md](kernel-development.md) | **Bare-metal / OS dev**: freestanding mode, cross-compilation, `naked`/`interrupt` functions, module-level asm, volatile MMIO. See also [`examples/kernel/`](../examples/kernel/). |
| [PERFORMANCE.md](PERFORMANCE.md) | Performance practices. |
| [capability-report.md](capability-report.md) | **Honest assessment** of what Tocin can build today, with measurements. |

## Compiler internals

| Doc | What it is |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | The compiler pipeline: lexer → parser → type checker → LLVM IR → JIT/AOT. |
| [ADVANCED_FEATURES.md](ADVANCED_FEATURES.md) | Notes on optional/experimental C++ subsystems. |
| [async-scheduler-design.md](async-scheduler-design.md) | Design for the M:N async scheduler (eager async ships today). |
| [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) | Embedding/interop status: what is wired in and what is scaffolding. |

## Contributing

See [../CONTRIBUTING.md](../CONTRIBUTING.md). When adding a language feature,
include a `.to` test under `tests/` (JIT/runtime tests go in `tests/jit/`,
lli-compatible ones in `tests/cases/`) and update
[language-reference.md](language-reference.md).
