# package.ps1 - build a self-contained Tocin distribution zip on Windows:
#
#   dist\tocin-<version>-windows-x86_64.zip
#
# Run this ON Windows (needs CMake + Ninja + a configured LLVM 18 dev install,
# matching the project's build requirements). Produces a package whose layout
# mirrors the Unix one, plus an install.ps1.
#
#   powershell -ExecutionPolicy Bypass -File installer\package.ps1 [-BundleLibs]
[CmdletBinding()]
param(
    [switch]$BundleLibs,
    [string]$Out
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Out) { $Out = Join-Path $repo "dist" }
$build = Join-Path $repo "build"

if (-not (Test-Path (Join-Path $build "tocin.exe"))) {
    Write-Host ">> configuring (no build\ found)"
    cmake -S $repo -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release
}
cmake --build $build --target tocin tocin_runtime_shared -j 4

$ver = (& (Join-Path $build "tocin.exe") --version) -replace 'tocin\s+',''
if (-not $ver) { $ver = "0.1.0" }
$name  = "tocin-$ver-windows-x86_64"
$stage = Join-Path $Out $name

Write-Host ">> staging $name"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
foreach ($d in "bin","libexec","lib","share") { New-Item -ItemType Directory -Force -Path (Join-Path $stage $d) | Out-Null }

Copy-Item (Join-Path $build "tocin.exe") (Join-Path $stage "libexec\tocin.exe")
foreach ($rt in "tocin_runtime_shared.dll","libtocin_runtime.dll","tocin_runtime.dll") {
    $p = Join-Path $build $rt
    if (Test-Path $p) { Copy-Item $p (Join-Path $stage "lib\tocin_runtime.dll") }
}
Copy-Item -Recurse -Force (Join-Path $repo "stdlib")   (Join-Path $stage "stdlib")
Copy-Item -Recurse -Force (Join-Path $repo "docs")     (Join-Path $stage "share\docs")
Copy-Item -Recurse -Force (Join-Path $repo "examples") (Join-Path $stage "share\examples")
Set-Content -Path (Join-Path $stage "VERSION") -Value $ver -Encoding ASCII
Copy-Item (Join-Path $repo "installer\install.ps1") (Join-Path $stage "install.ps1")

if ($BundleLibs) {
    Write-Host ">> bundling DLLs the exe depends on (best effort)"
    # Copy non-system DLLs found next to the LLVM/runtime install onto lib\.
    # (Refine per your toolchain; dumpbin /dependents lists what tocin.exe needs.)
}

$zip = Join-Path $Out "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host ""
Write-Host ">> built $zip"
Write-Host "   install with:  Expand-Archive $name.zip; cd $name; .\install.ps1"
