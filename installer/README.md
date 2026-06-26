# Tocin installer & packaging

This directory holds the cross-platform installer system. End-user instructions
live in [`../INSTALL.md`](../INSTALL.md); this file is for **maintainers**
producing and publishing the release packages.

## Files

| File | Role |
|---|---|
| `package.sh` | Build a Linux/macOS tarball (`dist/tocin-<ver>-<os>-<arch>.tar.gz`). |
| `package.ps1` | Build a Windows zip (`dist/tocin-<ver>-windows-x86_64.zip`). |
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

# On Windows:
installer\package.ps1 -BundleLibs
```

Each writes its artifact to `dist/`.

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
