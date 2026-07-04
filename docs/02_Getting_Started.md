# Getting Started with Tocin

Install the compiler, run your first program, and learn the everyday commands.

## 1. Install Tocin

### Option A — native installers (recommended)

Download the file for your platform and open it (full details in
[../INSTALL.md](../INSTALL.md)):

| Platform | File |
|---|---|
| Windows | `Tocin-<ver>-Setup.exe` — GUI wizard; adds `tocin` to PATH and a `.to` file association |
| Debian/Ubuntu | `tocin_<ver>_amd64.deb` — double-click or `sudo apt install ./tocin_<ver>_amd64.deb` |
| Any Linux | `tocin-<ver>-linux-<arch>.run` — `sh tocin-*.run`; installs to `~/.tocin`, no root |
| macOS | `tocin-<ver>-macos-<arch>.pkg` / `.dmg` — native Installer GUI |

Or use the quick-install one-liners (they fetch the latest GitHub Release):

```bash
# Linux / macOS
curl -fsSL https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/get-tocin.sh | sh
```

```powershell
# Windows (PowerShell)
irm https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/install.ps1 | iex
```

The installed packages are **self-contained**: compiler, runtime, the full
standard library, docs, examples, an uninstaller — and a bundled linker, so
compiling to native executables needs **no C compiler on your machine**.

### Option B — build from source

You need CMake ≥ 3.16, a C++20 compiler, and LLVM 18–22 dev packages
(see [BUILDING.md](BUILDING.md)):

```bash
git clone https://github.com/tafolabi009/TocinLang.git
cd TocinLang
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DWITH_V8=OFF -DLLVM_DIR=$(llvm-config-18 --cmakedir)
cmake --build build -j
```

This produces `build/tocin`.

### Verify

```bash
tocin --version
```

## 2. Your first program

```tocin
// hello.to
def main() {
    println("Hello from Tocin!");
    return 0;
}
```

Run it immediately with the JIT — no separate build step:

```bash
tocin hello.to --run
```

Or compile a native executable:

```bash
tocin hello.to -o hello
./hello                     # (hello.exe on Windows)
```

The process exit code is whatever `main` returns.

## 3. Start a project

```bash
tocin new myapp
cd myapp
tocin main.to --run
```

`tocin new` scaffolds a directory with `main.to`, a `README.md`, and a
`.gitignore`.

## 4. The commands you'll use daily

| Command | What it does |
|---|---|
| `tocin app.to --run` | JIT-compile and run right now (fastest iteration loop). |
| `tocin app.to -o app` | Build a native executable. `-o app.ll` / `app.s` / `app.o` emit IR / assembly / an object instead. |
| `tocin check app.to` | Typecheck only — full strict diagnostics, no codegen. Great as a pre-commit or CI gate. |
| `tocin doc app.to` | Print Markdown API docs generated from signatures and the `//` comments above them. |
| `tocin app.to -o app -O3 --native` | Maximum optimization, tuned to your CPU. Default is `-O2`. |

When something is wrong, the compiler tells you precisely where and often what
you meant:

```text
app.to:3:8: error [T013]: Cannot assign to constant 'X' (declared with `const`). Use `let` for a mutable binding.
    3 |     X = 6;
      |        ^
1 error generated.
```

## 5. Use the standard library

The stdlib ships with the installer (34 modules — math, data structures,
strings, JSON, testing, ML, web, and more). Import what you need; names are
global after import:

```tocin
import std.math;

def main() {
    println("{}", clamp(15, 0, 10));   // 10
    return 0;
}
```

The installer's launcher sets `TOCIN_PATH` so imports just work; when running
a source checkout, `export TOCIN_PATH=/path/to/TocinLang/stdlib`.

## 6. Where to next

- **Learn the language**: [tutorial.md](tutorial.md) — every feature, with
  runnable code.
- **Look something up**: [language-reference.md](language-reference.md) and
  [stdlib-reference.md](stdlib-reference.md).
- **Explore the stdlib**: [04_Standard_Library.md](04_Standard_Library.md).
- **Run the examples**: `tocin ~/.tocin/share/examples/hello.to --run`
  (installed) or the `examples/` directory of a checkout.
