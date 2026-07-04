# Running Tocin Programs

Tocin is a **compiled** language with two execution paths — an in-process JIT
for fast iteration and ahead-of-time native builds for deployment. (There is no
tree-walking interpreter; this guide replaces an older document that described
one.)

## JIT: run it now

```bash
tocin app.to --run          # compile in memory and execute immediately
echo $?                     # the exit code is main()'s return value
```

`--run` (alias `--jit`) lowers the program to LLVM IR, JIT-compiles it with
ORCv2, and calls `main` in-process. The runtime (GC, goroutines, channels,
TCP/time/hash builtins) is registered automatically — no external tools are
involved.

## AOT: build a native executable

```bash
tocin app.to -o app         # native executable
./app

tocin app.to -o app.ll      # LLVM IR (text)
tocin app.to -o app.s       # assembly
tocin app.to -o app.o       # object file
```

The `-o` extension selects the output format. Executables link through the
system C compiler when one is present — or through the **bundled `ld.lld` +
static link recipe** in installed packages, which needs **no system toolchain
at all** (see [native-linking.md](native-linking.md)).

## Optimization

```bash
tocin app.to -o app -O3 --native
```

- `-O0`/`-O1`/`-O2`/`-O3` — LLVM's standard pipelines; the default is `-O2`.
- `--native` — tune for the build machine's CPU (POPCNT/AVX reach the
  vectorizer). The binary is not portable to older CPUs.
- At `-O3`, executables and JIT runs get whole-program internalization
  (non-`main` symbols become internal, unlocking cross-function optimization).

## Checking without running

```bash
tocin check app.to          # typecheck only; exit 0 if clean
```

Fast pre-commit/CI gate: full strict diagnostics (caret underlines,
`did you mean …?` suggestions, `N errors generated.`), no code generation.
See [ERROR_HANDLING.md](ERROR_HANDLING.md).

## Project scaffolding and docs

```bash
tocin new myapp             # creates myapp/ (main.to, README.md, .gitignore)
tocin doc app.to > API.md   # Markdown API docs from signatures + // comments
```

## The REPL (experimental)

Running `tocin` with **no arguments** starts an experimental REPL:

```text
Tocin Enhanced REPL (type 'exit' to quit, 'clear' to reset)
Commands: debug, package, async, macro
>
```

Today it **compiles each line and prints the resulting LLVM IR** — useful for
inspecting codegen, but it does *not* evaluate expressions or print their
values, and it has no history or completion. For interactive experimentation,
prefer editing a file and re-running `tocin file.to --run` (the JIT makes this
near-instant).

## Environment

| Variable | Effect |
|---|---|
| `TOCIN_PATH` | Root directory searched for `import` resolution (installers set it to the bundled `stdlib/`). |
| `NO_COLOR` | Disable ANSI colors in diagnostics. |
| `CC` | Choose the C-compiler driver for `-o` linking when the system path is used. |

## See also

- [language-reference.md §19](language-reference.md#19-compilation--execution) — the full CLI table.
- [ERROR_HANDLING.md](ERROR_HANDLING.md) — diagnostics and runtime traps.
- [../INSTALL.md](../INSTALL.md) — installing a released Tocin.
