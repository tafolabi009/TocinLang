# Building the Tocin Installers

How to produce the native, self-contained Tocin installers. **If you just want
to install Tocin, read [../INSTALL.md](../INSTALL.md) instead** — this page is
for maintainers building the packages.

Every artifact is self-contained: it bundles the compiler, runtime, standard
library, docs/examples, an uninstaller, and the **`ld.lld` link bundle** that
lets `tocin file.to -o app` produce native binaries with **no system C
compiler** on the target machine ([native-linking.md](native-linking.md)).

## What gets built

| Platform | Artifact | Built by |
|---|---|---|
| Windows | `Tocin-<ver>-Setup.exe` — NSIS GUI wizard (welcome → license → components → PATH + `.to` association, Add/Remove Programs uninstaller) | `installer\windows\Build-TocinInstaller.ps1` + `installer\windows\installer.nsi` |
| Windows | `tocin-<ver>-windows-x86_64.zip` — portable zip with `install.ps1` | `installer\windows\Build-TocinInstaller.ps1` |
| Debian/Ubuntu | `tocin_<ver>_amd64.deb` — installs to `/opt/tocin`, `Depends: libc6` only | `installer/linux/make-deb.sh` |
| Any Linux | `tocin-<ver>-linux-<arch>.run` — self-extracting installer (zenity/kdialog GUI or terminal prompt), installs to `~/.tocin`, no root | `installer/linux/make-run.sh` |
| Linux/macOS | `tocin-<ver>-<os>-<arch>.tar.gz` — portable tarball with `install.sh` | `installer/package.sh --bundle-libs` |
| macOS | `tocin-<ver>-macos-<arch>.pkg` / `.dmg` — native Installer GUI, installs to `/usr/local/tocin` | `installer/macos/make-pkg.sh` |

## One command per platform

### Linux

```bash
installer/build-all.sh      # -> dist/: .tar.gz + .run + .deb
```

### macOS

```bash
installer/build-all.sh      # -> dist/: .tar.gz + .pkg + .dmg
```

(Run **on** macOS — Mach-O binaries and `pkgbuild`/`productbuild`/`hdiutil`
are not cross-produced.)

### Windows

```powershell
powershell -ExecutionPolicy Bypass -File installer\windows\Build-TocinInstaller.ps1
```

This is turnkey on a clean machine: it installs MSYS2 (winget), pacman-installs
the mingw64 toolchain (LLVM, GCC, CMake, Ninja, Boehm GC), builds the compiler,
stages a self-contained tree (bundling every dependent DLL next to
`tocin.exe`), zips it, and — if `makensis` is on `PATH` — emits the GUI
`Setup.exe`. Useful extras:

```powershell
winget install --id NSIS.NSIS -e                 # once: enables the Setup.exe step
C:\msys64\usr\bin\bash.exe -lc "pacman -S --needed --noconfirm mingw-w64-x86_64-lld"
                                                 # once: ld.lld for the no-compiler link bundle
powershell -ExecutionPolicy Bypass -File installer\windows\Build-TocinInstaller.ps1 -SkipDeps
                                                 # repeat builds: skip the toolchain step
```

Outputs land in `dist\tocin-<ver>-windows-x86_64.zip` and
`installer\windows\Tocin-<ver>-Setup.exe`.

## Package flavors

| Flavor | Size | Needs on the target machine |
|---|---|---|
| **Portable** (`--bundle-libs`, the default for `build-all.sh`) | ~60 MB | nothing but a libc — no compiler, no LLVM, no GC package |
| **Standard** (`package.sh` without `--bundle-libs`) | <1 MB | system LLVM 18 + libgc already installed |

The portable flavor also vendors the static link recipe
(`installer/make-link-recipe.sh --static`), so on Linux the executables that
`tocin -o` produces are **fully static** — they run on any distro, like Go
binaries.

## Publishing

Upload the artifacts to a GitHub Release tagged `v<ver>`; the quick-install
scripts (`installer/get-tocin.sh`, `installer/install.ps1`) download the
matching release asset. Details: [../installer/README.md](../installer/README.md).
