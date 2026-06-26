# Native linking without an external toolchain

`tocin file.to -o app` (or `app.exe`) emits a native object and then links it
into an executable. Historically that link shelled out to a C compiler driver
(`cc`/`gcc`/`clang`) — so producing a binary required a system toolchain on
`PATH`. This document describes the self-contained linker that removes that
requirement on Windows (and works the same way everywhere).

`.ll`, `.s`, and `.o` outputs never need a linker and are unaffected.

## How it works

Two pieces:

1. **A bundled linker + recipe** shipped beside the compiler in
   `libexec/link/`:
   - `ld.lld(.exe)` — LLVM's linker.
   - `link-recipe.txt` — the linker argument list as **data**, with three
     placeholders the compiler fills in at link time:
     - `%LINKDIR%` → the absolute path of `libexec/link`
     - `%OBJ%` → the object the compiler just emitted
     - `%OUT%` → the output executable
   - `sysroot/` — every input the link needs that is *not* present on a target
     machine: the CRT startup objects and the static/import libs (libgcc,
     libstdc++, libmingw*, the GC, `libtocin_runtime.a`, …).

2. **The compiler engine** (`tryBundledLink` in `src/main.cpp`): if
   `libexec/link/` is present it reads the recipe, substitutes the placeholders,
   prepends its own directory to `PATH` (so `ld.lld` finds the shared libs that
   live next to `tocin` in `libexec`), and runs `ld.lld @response`. If the
   bundle is absent it falls back to the C-driver path — so there is no
   regression where a toolchain is available.

Keeping the link line as data means a wrong argument can be fixed by editing
`link-recipe.txt` in the package — no recompile of the compiler.

## How the recipe is produced

`installer/make-link-recipe.sh` derives the recipe from the **real C compiler's
own link line** rather than guessing it. It compiles a throwaway object, runs
`gcc -v` to capture the exact `collect2`/`ld` invocation, then rewrites that line
into a relocatable recipe: every absolute input is copied into `sysroot/` and
referenced via `%LINKDIR%`, and the object/output tokens become `%OBJ%`/`%OUT%`.

Two modes:

- **self-contained** (default, used on Windows): collapse library search to the
  vendored `sysroot/` and copy every `-l` input. Correct on Windows because the
  mingw import libs are not on the target, but the system DLLs they import
  (`kernel32`, `ucrtbase`, …) always are.
- **`--keep-syslibs`** (Linux/macOS): keep the toolchain's search dirs and rely
  on the target's system `libc`/`libgcc_s`/… These always exist there.

The Windows packager (`installer/windows/Build-TocinInstaller.ps1`) calls this
with the mingw `gcc`, `-static-libgcc -static-libstdc++`, and a static `libgc.a`
when available, so the produced `app.exe` is standalone. The step is non-fatal:
if `ld.lld` or the recipe can't be produced, the package still builds and native
`-o` falls back to requiring `gcc` on `PATH`.

## Status

The compiler engine and `make-link-recipe.sh` are validated end-to-end on Linux:
with `PATH`/`CC` pointed at nothing, `tocin x.to -o x` links through the bundled
`ld.lld` and produces a working binary. The Windows recipe *contents* are derived
from the mingw `gcc` on the build machine; validate on Windows by building the
installer and running:

```
tocin file.to -o app.exe      # should produce app.exe with no gcc/clang on PATH
.\app.exe
```

If a link argument needs adjusting for a particular mingw version, edit the
generated `libexec/link/link-recipe.txt` (or `installer/make-link-recipe.sh`) —
the compiler itself does not need rebuilding.

## Licensing

Everything vendored is redistributable: the mingw-w64 runtime is permissive
(MIT/BSD/public-domain); `libgcc`/`libstdc++` are GPLv3 **with the GCC Runtime
Library Exception**, which explicitly permits shipping them linked into non-GPL
programs; winpthreads is MIT; Boehm GC is MIT-style; `ld.lld` is Apache-2.0 with
the LLVM exception. Ship the corresponding license/exception notices in the
package.
