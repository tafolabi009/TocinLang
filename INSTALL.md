# Installing Tocin

Tocin ships as a self-contained package per platform — it bundles the compiler,
the runtime, the **standard library**, the **docs/tutorials**, the **examples**,
a launcher that wires everything up, and an **uninstaller**. Installation is
per-user (no root/admin needed) and adds `tocin` to your `PATH`.

There are two package flavors per platform:

| Flavor | Size | Needs on the machine |
|---|---|---|
| **Portable** (`--bundle-libs`) | ~62 MB | nothing but a libc — **not even a C compiler** |
| **Standard** | <1 MB | system **LLVM 18** + **libgc** already installed |

The portable flavor is what most people want — it "just works". It ships a
vendored `ld.lld` plus a static link recipe, so `tocin file.to -o app`
produces a native binary **with no gcc/clang/ld installed** — and those output
binaries are fully static (they run on any Linux, like a Go binary).

## Native installers (GUI, double-click)

Download the file for your platform and open it:

| Platform | File | How it installs |
|---|---|---|
| **Windows** | `Tocin-<ver>-Setup.exe` | GUI wizard (welcome → license → components → PATH). NSIS; adds `tocin` to PATH and a `.to` file association. |
| **Debian/Ubuntu** | `tocin_<ver>_amd64.deb` | Double-click → GNOME Software / KDE Discover, or `sudo apt install ./tocin_<ver>_amd64.deb`. Installs to `/opt/tocin`. |
| **Any Linux** | `tocin-<ver>-linux-<arch>.run` | `sh tocin-*.run` → GUI dialog (zenity/kdialog) or terminal prompt; installs to `~/.tocin`, no root. |
| **macOS** | `tocin-<ver>-macos-<arch>.pkg` / `.dmg` | Native macOS Installer GUI; installs to `/usr/local/tocin`. |

Build them all yourself from a source checkout:

```
installer/build-all.sh     # Linux: .tar.gz + .run + .deb   (macOS: .pkg + .dmg)
```

On Windows: `powershell installer\windows\Build-TocinInstaller.ps1` (produces the
`.zip` and, if NSIS is on PATH, `Setup.exe`).

---

## Quick install

### Linux / macOS

```bash
curl -fsSL https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/get-tocin.sh | sh
```

This downloads the matching release tarball, installs into `~/.tocin`, adds
`~/.tocin/bin` to your `PATH`, and writes an uninstaller. Open a new terminal:

```bash
tocin --version
tocin ~/.tocin/share/examples/hello.to --run
```

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/install.ps1 | iex
```

Installs into `%LOCALAPPDATA%\Tocin`, adds it to your user `PATH`, and writes an
uninstaller. Open a new terminal and run `tocin --version`.

> The quick-install commands fetch a **published GitHub Release**. If no release
> exists yet, use *Manual install* below (or build one with `installer/package.*`
> and publish it — see [`installer/README.md`](installer/README.md)).

---

## Manual install (from a downloaded package)

**Linux / macOS**

```bash
tar xzf tocin-<version>-<os>-<arch>.tar.gz
cd tocin-<version>-<os>-<arch>
./install.sh                 # or: TOCIN_HOME=/opt/tocin ./install.sh
```

**Windows**

```powershell
Expand-Archive tocin-<version>-windows-x86_64.zip
cd tocin-<version>-windows-x86_64
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

Useful flags: `--prefix DIR` / `-Prefix DIR` (install location),
`--no-modify-path` / `-NoModifyPath` (don't touch PATH).

---

## What gets installed

```
~/.tocin/                 (%LOCALAPPDATA%\Tocin on Windows)
├── bin/tocin             launcher (sets TOCIN_PATH + library path, then runs the compiler)
├── libexec/tocin         the compiler binary
├── lib/                  the runtime (+ bundled LLVM/GC in the portable flavor)
├── stdlib/               the standard library (import std.math; ...)
├── share/docs/           tutorial.md, language-reference.md, tocin-for-ai.md, ...
├── share/examples/       runnable example programs
├── VERSION
└── uninstall.sh          (uninstall.ps1 on Windows)
```

The launcher is relocatable — you can move `~/.tocin` anywhere and it still works.

---

## Uninstall

```bash
~/.tocin/uninstall.sh                                  # Linux / macOS
```
```powershell
powershell -File $env:LOCALAPPDATA\Tocin\uninstall.ps1  # Windows
```

This removes the install directory and the `PATH` entry added by the installer.

---

## Build a package yourself

You need the project's build deps (CMake, Ninja, LLVM 18 dev, libgc). Then:

```bash
installer/package.sh --bundle-libs      # Linux/macOS -> dist/tocin-<ver>-<os>-<arch>.tar.gz
```
```powershell
installer\package.ps1 -BundleLibs       # Windows     -> dist\tocin-<ver>-windows-x86_64.zip
```

Run the packager **on each OS you want to target** — a native LLVM toolchain is
not cross-compiled. See [`installer/README.md`](installer/README.md) for how to
publish the packages as a GitHub Release.

---

## Notes

- **Native binaries** (`tocin prog.to -o prog`) link through a system C compiler
  (`cc`/`clang`/MSVC), exactly like Rust uses the system linker. The JIT
  (`tocin prog.to --run`) needs no external tools.
- Set `CC` to choose the linker driver for `-o` output.
