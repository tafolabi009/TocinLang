# Tocin installer & packaging

This directory holds the cross-platform installer system. End-user instructions
live in [`../INSTALL.md`](../INSTALL.md); this file is for **maintainers**
producing and publishing the release packages.

## Files

| File | Role |
|---|---|
| `linux/Build-TocinInstaller.sh` | **Turnkey Linux builder** — bootstraps the toolchain, builds, bundles, and packages in one run (see below). |
| `windows/Build-TocinInstaller.ps1` | **Turnkey Windows builder** — bootstraps the toolchain, builds, and packages in one run (see below). |
| `package.sh` | Build a Linux/macOS tarball (`dist/tocin-<ver>-<os>-<arch>.tar.gz`); `--bundle-libs` for the self-contained flavor. |
| `package.ps1` | Build a Windows zip when the toolchain is *already* installed. |
| `install.sh` | Per-user installer that ships *inside* each Unix tarball. |
| `install.ps1` | Per-user installer for Windows (also a download bootstrap). |
| `get-tocin.sh` | `curl \| sh` bootstrap: download the latest release + install. |

A package is fully self-contained: compiler + runtime + `stdlib/` + `share/docs`
+ `share/examples` + `install.sh` + an uninstaller. The `--bundle-libs` flavor
also vendors LLVM/GC so it runs with no system dependencies.

## Build the packages

Run the packager **on each target OS** — the compiler links a native LLVM, which
is not cross-compiled from another platform.

```bash
# On Linux  (x86_64):
installer/package.sh --bundle-libs

# On macOS  (run on an Intel mac for x86_64, an Apple-silicon mac for arm64):
installer/package.sh --bundle-libs
```

Each writes its artifact to `dist/`.

### Linux — one turnkey script (no prior setup)

On a clean Linux machine, run **one** script and it does everything — installs
the toolchain (CMake + Ninja + GCC + LLVM 18 + Boehm GC + libffi/zlib via
`apt`/`dnf`/`pacman`/`zypper`), builds the compiler, vendors every non-system
shared library so the result runs with no dependencies beyond glibc, and writes
`dist/tocin-<ver>-linux-x86_64.tar.gz`:

```bash
installer/linux/Build-TocinInstaller.sh
```

Useful flags: `--skip-deps` (toolchain already installed — much faster repeat
builds), `--no-python` (configure `WITH_PYTHON=OFF` for a slimmer, Python-free
package), `--out DIR` (output directory). Extract the tarball and run
`./install.sh` to put `tocin` on `PATH`.

If the toolchain is already present, `installer/package.sh --bundle-libs`
packages an existing `build/` without the bootstrap step.

### Windows — one turnkey script (no prior setup)

On a clean Windows machine, run **one** script and it does everything —
installs the toolchain (MSYS2 + LLVM 18 + GCC + CMake + Ninja + Boehm GC via
`pacman`), builds the compiler, vendors the dependent DLLs so the result runs
without MSYS2, and writes `dist\tocin-<ver>-windows-x86_64.zip`:

```powershell
powershell -ExecutionPolicy Bypass -File installer\windows\Build-TocinInstaller.ps1
```

Useful flags: `-SkipDeps` (toolchain already installed — much faster repeat
builds), `-Msys2Root <path>` (reuse an existing MSYS2), `-WithPython -WithXml
-WithZstd` (enable the optional features; off by default to keep a fresh-machine
build robust), `-NoBundle` (smaller zip that needs mingw64 on PATH). If NSIS
(`makensis`) is on PATH it also emits a `.exe` installer.

If you already have the MSYS2/mingw64 toolchain set up, `installer\package.ps1
-BundleLibs` packages an existing `build\` without the bootstrap step.

## Publish a GitHub Release (no CI required)

CI is not used for releases — build locally and upload. With the GitHub CLI:

```bash
gh release create v0.1.0 \
    dist/tocin-0.1.0-linux-x86_64.tar.gz \
    dist/tocin-0.1.0-macos-x86_64.tar.gz \
    dist/tocin-0.1.0-macos-arm64.tar.gz \
    dist/tocin-0.1.0-windows-x86_64.zip \
    --title "Tocin 0.1.0" \
    --notes "Self-contained installers for Linux, macOS, and Windows."
```

Or via the web UI: **Releases → Draft a new release → tag `v0.1.0` → attach the
files in `dist/`**. Release creation does not require GitHub Actions/billing.

Once a release tagged `vX.Y.Z` exists, the quick-install one-liners in
`INSTALL.md` work automatically (they resolve "latest" or a pinned
`TOCIN_VERSION`).

## Versioning

The version comes from `project(Tocin VERSION x.y.z ...)` in the top-level
`CMakeLists.txt`, baked into the binary (`tocin --version`) and used to name the
package. Bump it there before cutting a release.
