<#
.SYNOPSIS
    Turnkey Tocin installer builder for Windows. Run this ONE script on a clean
    Windows machine and it bootstraps the toolchain, builds the compiler, and
    produces a self-contained installer package - no other setup required.

.DESCRIPTION
    Steps, each idempotent (re-running skips finished work):
      1. Ensure MSYS2 is installed (via winget, or point at an existing install
         with -Msys2Root). MSYS2/mingw64 is the project's supported Windows
         toolchain: it provides LLVM 18, GCC, CMake, Ninja and Boehm GC, all
         ABI-matched, which CMakeLists.txt already expects on Windows.
      2. pacman-install the mingw64 toolchain + libraries.
      3. Configure + build tocin (Release) with Ninja from the mingw64 shell.
      4. Stage a self-contained tree: the compiler, runtime DLL, the dependent
         mingw/LLVM DLLs (so it runs on a machine without MSYS2), the standard
         library, docs, examples, an installer (install.ps1, which adds Tocin to
         PATH and writes an uninstaller), and a VERSION file.
      5. Zip it to  dist\tocin-<version>-windows-x86_64.zip
      6. If makensis (NSIS) is on PATH, also emit a .exe installer.

    The produced .zip is what you upload to a GitHub Release (Tocin's installs
    do not depend on CI - this script is the build).

.PARAMETER Msys2Root
    MSYS2 install root. Default C:\msys64. If MSYS2 is missing here it is
    installed (winget). Point this at an existing MSYS2 to reuse it.

.PARAMETER SkipDeps
    Skip the MSYS2 install + pacman step (use when the toolchain is already set
    up - much faster for repeat builds).

.PARAMETER WithPython
    Enable the optional Python FFI feature (off by default).

.PARAMETER WithXml
    Enable the optional LibXml2 feature (off by default).

.PARAMETER WithZstd
    Enable the optional zstd feature (off by default).

.PARAMETER NoBundle
    Do not copy dependent DLLs into the package (smaller zip that requires
    MSYS2/mingw64 on PATH to run - not self-contained).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File installer\windows\Build-TocinInstaller.ps1

.EXAMPLE
    .\installer\windows\Build-TocinInstaller.ps1 -SkipDeps -WithPython -WithXml -WithZstd
#>
[CmdletBinding()]
param(
    [string]$Msys2Root = "C:\msys64",
    [switch]$SkipDeps,
    [switch]$WithPython,
    [switch]$WithXml,
    [switch]$WithZstd,
    [switch]$NoBundle
)

$ErrorActionPreference = "Stop"
function Info($m) { Write-Host (">> " + $m) -ForegroundColor Cyan }
function Warn($m) { Write-Host (("!! " + $m)) -ForegroundColor Yellow }
function Die($m)  { Write-Host (("XX " + $m)) -ForegroundColor Red; exit 1 }

# Repo root = two levels up from installer\windows\.
$repo  = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$build = Join-Path $repo "build"
$dist  = Join-Path $repo "dist"
$bash  = Join-Path $Msys2Root "usr\bin\bash.exe"

# Select the mingw64 toolchain for every bash -l invocation.
$env:MSYSTEM = "MINGW64"
$env:CHERE_INVOKING = "1"

# Convert a Windows path (C:\a\b) to an MSYS path (/c/a/b).
function ConvertTo-Msys([string]$p) {
    $drive = $p.Substring(0,1).ToLowerInvariant()
    $rest  = ($p.Substring(2) -replace '\\','/')
    return "/$drive$rest"
}
$repoMsys = ConvertTo-Msys $repo

# Run a command inside the MSYS2 mingw64 login shell, in the repo dir.
function Invoke-Mingw([string]$cmdline) {
    if (-not (Test-Path $bash)) { Die "MSYS2 bash not found at $bash" }
    & $bash -lc "cd '$repoMsys' && $cmdline"
    if ($LASTEXITCODE -ne 0) { Die "mingw64 command failed (exit $LASTEXITCODE): $cmdline" }
}

# ---------------------------------------------------------------------------
# 1. Ensure MSYS2
# ---------------------------------------------------------------------------
if (-not $SkipDeps) {
    if (-not (Test-Path $bash)) {
        Info "MSYS2 not found at $Msys2Root - installing via winget"
        if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
            Warn "winget is not available and MSYS2 is not installed."
            Warn "Install MSYS2 from https://www.msys2.org then re-run with"
            Warn "  -Msys2Root <path>  (default C:\msys64), or -SkipDeps if ready."
            Die  "missing prerequisite: MSYS2"
        }
        winget install -e --id MSYS2.MSYS2 --accept-source-agreements --accept-package-agreements
        if (-not (Test-Path $bash)) {
            # winget may install to the default location regardless of -Msys2Root.
            $default = "C:\msys64\usr\bin\bash.exe"
            if (Test-Path $default) {
                $Msys2Root = "C:\msys64"
                $bash = $default
            } else {
                Die "MSYS2 install did not produce $bash. Set -Msys2Root to the install path."
            }
        }
    } else {
        Info "MSYS2 found at $Msys2Root"
    }

    # 2. Toolchain + libraries. -S --needed is idempotent (skips installed).
    #    A fresh MSYS2 wants a full upgrade first; its two-phase core upgrade is
    #    handled naturally here because each Invoke-Mingw call is a separate bash
    #    process (the package-install call below is the post-upgrade restart).
    Info "updating pacman and installing the mingw64 toolchain (first run can take a while)"
    Invoke-Mingw "pacman -Syuu --noconfirm"
    $pkgs = @(
        "mingw-w64-x86_64-gcc",
        "mingw-w64-x86_64-cmake",
        "mingw-w64-x86_64-ninja",
        "mingw-w64-x86_64-llvm",
        "mingw-w64-x86_64-clang",
        "mingw-w64-x86_64-zlib",
        "mingw-w64-x86_64-gc",
        "mingw-w64-x86_64-ntldd-git"
    )
    if ($WithZstd)   { $pkgs += "mingw-w64-x86_64-zstd" }
    if ($WithXml)    { $pkgs += "mingw-w64-x86_64-libxml2" }
    if ($WithPython) { $pkgs += "mingw-w64-x86_64-python" }
    Invoke-Mingw ("pacman -S --needed --noconfirm " + ($pkgs -join " "))
} else {
    Info "skipping dependency install (-SkipDeps)"
    if (-not (Test-Path $bash)) { Die "-SkipDeps set but MSYS2 bash not at $bash" }
}

# ---------------------------------------------------------------------------
# 3. Configure + build (Release) in the mingw64 environment.
# ---------------------------------------------------------------------------
$py = "OFF"; if ($WithPython) { $py = "ON" }
$xml = "OFF"; if ($WithXml)   { $xml = "ON" }
$zstd = "OFF"; if ($WithZstd) { $zstd = "ON" }

Info "configuring Ninja Release. Python=$py XML=$xml zstd=$zstd"
Invoke-Mingw "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DWITH_PYTHON=$py -DWITH_XML=$xml -DWITH_ZSTD=$zstd"

Info "building tocin + runtime (this is the long step)"
Invoke-Mingw "cmake --build build --target tocin tocin_runtime_shared -j"

$exe = Join-Path $build "tocin.exe"
if (-not (Test-Path $exe)) { Die "build did not produce $exe" }

# ---------------------------------------------------------------------------
# 4. Stage a self-contained package.
# ---------------------------------------------------------------------------
$ver = (& $exe --version) 2>$null
if ($ver) { $ver = ($ver -replace '(?i)tocin\s+','').Trim() }
if (-not $ver) { $ver = "0.1.0" }
$name  = "tocin-$ver-windows-x86_64"
$stage = Join-Path $dist $name
Info "staging $name"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
foreach ($d in @("bin","libexec","lib","stdlib","share\docs","share\examples")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $stage $d) | Out-Null
}

# Compiler goes in libexec; a bin\ shim is written by install.ps1 on the target.
Copy-Item $exe (Join-Path $stage "libexec\tocin.exe")

# Runtime shared library (name varies by toolchain).
$rtCopied = $false
foreach ($rt in @("tocin_runtime_shared.dll","libtocin_runtime_shared.dll","libtocin_runtime.dll","tocin_runtime.dll")) {
    $p = Join-Path $build $rt
    if (Test-Path $p) {
        Copy-Item $p (Join-Path $stage "lib\tocin_runtime.dll")
        $rtCopied = $true
        break
    }
}
if (-not $rtCopied) { Warn "no runtime DLL found in build - JIT may still work if statically linked" }

# Payload: stdlib, docs (tutorials), examples, version, installer.
Copy-Item -Recurse -Force (Join-Path $repo "stdlib\*")   (Join-Path $stage "stdlib")
Copy-Item -Recurse -Force (Join-Path $repo "docs\*")     (Join-Path $stage "share\docs")
Copy-Item -Recurse -Force (Join-Path $repo "examples\*") (Join-Path $stage "share\examples")
Set-Content -Path (Join-Path $stage "VERSION") -Value $ver -Encoding ASCII
Copy-Item (Join-Path $repo "installer\install.ps1") (Join-Path $stage "install.ps1")
if (Test-Path (Join-Path $repo "INSTALL.md")) {
    Copy-Item (Join-Path $repo "INSTALL.md") (Join-Path $stage "README.txt")
}

# ---------------------------------------------------------------------------
# 5. Bundle dependent DLLs so the package runs without MSYS2 on PATH.
# ---------------------------------------------------------------------------
if (-not $NoBundle) {
    Info "bundling dependent DLLs (self-contained)"
    $mingwBinDir = Join-Path $Msys2Root "mingw64\bin"
    $libOut = Join-Path $stage "lib"
    $bundled = @{}
    # Copy a DLL out of mingw64\bin by name (only mingw DLLs live there, so
    # system DLLs like KERNEL32.dll are naturally skipped).
    function Copy-MingwDll([string]$fname) {
        if ($bundled.ContainsKey($fname)) { return }
        $srcDll = Join-Path $mingwBinDir $fname
        if (Test-Path $srcDll) {
            Copy-Item $srcDll $libOut -Force
            $bundled[$fname] = $true
        }
    }

    # (a) Best effort: walk ntldd's recursive dependency tree and grab each DLL
    #     name (the token before "=>"), then resolve it in mingw64\bin.
    $exeMsys = ConvertTo-Msys $exe
    $listed = & $bash -lc "ntldd -R '$exeMsys' 2>/dev/null"
    foreach ($line in $listed) {
        if ($line -match '([^\s/\\]+\.dll)\s*=>') { Copy-MingwDll $matches[1] }
    }

    # (b) Safety net: the runtime DLLs a mingw64 LLVM build always needs.
    foreach ($llvm in (Get-ChildItem $mingwBinDir -Filter "libLLVM*.dll" -ErrorAction SilentlyContinue)) {
        Copy-MingwDll $llvm.Name
    }
    foreach ($n in @(
        "libstdc++-6.dll","libgcc_s_seh-1.dll","libwinpthread-1.dll",
        "libgc-1.dll","libgc.dll","zlib1.dll","libzstd.dll","liblzma-5.dll",
        "libiconv-2.dll","libxml2-2.dll")) { Copy-MingwDll $n }

    Info ("bundled " + $bundled.Count + " DLL(s) into lib")
    if ($bundled.Count -eq 0) { Warn "no DLLs bundled - check the mingw64 toolchain under $mingwBinDir" }
} else {
    Info "skipping DLL bundling (-NoBundle): package will require MSYS2/mingw64 on PATH"
}

# ---------------------------------------------------------------------------
# 6. Zip, and optionally an NSIS .exe.
# ---------------------------------------------------------------------------
$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Info "compressing $zip"
Compress-Archive -Path $stage -DestinationPath $zip
$zipMB = "{0:N1}" -f ((Get-Item $zip).Length / 1MB)

$nsi = Join-Path $PSScriptRoot "installer.nsi"
if ((Get-Command makensis -ErrorAction SilentlyContinue) -and (Test-Path $nsi)) {
    Info "building NSIS .exe installer"
    & makensis "/DVERSION=$ver" "/DSTAGE=$stage" $nsi
    if ($LASTEXITCODE -ne 0) { Warn "makensis failed; the .zip is still valid" }
} else {
    Info "makensis not found - skipping .exe (the .zip installer is complete)"
}

Write-Host ""
Info "DONE. Built Tocin $ver"
Write-Host ("   package:  " + $zip + "  (" + $zipMB + " MB)") -ForegroundColor Green
Write-Host ""
Write-Host "   Install locally:" -ForegroundColor Green
Write-Host "     Expand-Archive '$zip' -DestinationPath ."
Write-Host "     cd $name; powershell -ExecutionPolicy Bypass -File .\install.ps1"
Write-Host ""
Write-Host "   Publish: upload $name.zip to a GitHub Release tagged v$ver." -ForegroundColor Green
