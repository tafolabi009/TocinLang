# install.ps1 — install Tocin on Windows into a per-user prefix, add it to the
# user PATH, and write an uninstaller. Run from inside an extracted package:
#
#   tar xzf tocin-<ver>-windows-x86_64.zip   (or Expand-Archive)
#   cd tocin-<ver>-windows-x86_64
#   powershell -ExecutionPolicy Bypass -File .\install.ps1
#
# Or one-line bootstrap (downloads the latest release):
#   irm https://raw.githubusercontent.com/tafolabi009/TocinLang/master/installer/install.ps1 | iex
#
[CmdletBinding()]
param(
    [string]$Prefix = "$env:LOCALAPPDATA\Tocin",
    [string]$Version = $env:TOCIN_VERSION,
    [string]$Repo = "tafolabi009/TocinLang",
    [switch]$NoModifyPath
)
$ErrorActionPreference = "Stop"

$src = $PSScriptRoot
# Bootstrap mode: no local payload -> download the release first.
if (-not (Test-Path (Join-Path $src "libexec\tocin.exe"))) {
    if (-not $Version) {
        $rel = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases/latest"
        $Version = $rel.tag_name -replace '^v',''
    }
    $name = "tocin-$Version-windows-x86_64"
    $url  = "https://github.com/$Repo/releases/download/v$Version/$name.zip"
    $tmp  = Join-Path $env:TEMP ("tocin-" + [guid]::NewGuid())
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    Write-Host ">> downloading $url"
    Invoke-WebRequest -Uri $url -OutFile "$tmp\pkg.zip"
    Expand-Archive -Path "$tmp\pkg.zip" -DestinationPath $tmp -Force
    $src = Join-Path $tmp $name
}

$ver = (Get-Content (Join-Path $src "VERSION") -ErrorAction SilentlyContinue) ; if (-not $ver) { $ver = "unknown" }
Write-Host ">> installing Tocin $ver into $Prefix"

foreach ($d in "bin","libexec","lib","stdlib","share") {
    $p = Join-Path $Prefix $d
    if (Test-Path $p) { Remove-Item -Recurse -Force $p }
}
New-Item -ItemType Directory -Force -Path (Join-Path $Prefix "bin") | Out-Null
Copy-Item -Recurse -Force (Join-Path $src "libexec") (Join-Path $Prefix "libexec")
Copy-Item -Recurse -Force (Join-Path $src "lib")     (Join-Path $Prefix "lib")
Copy-Item -Recurse -Force (Join-Path $src "stdlib")  (Join-Path $Prefix "stdlib")
Copy-Item -Recurse -Force (Join-Path $src "share")   (Join-Path $Prefix "share")
Copy-Item -Force (Join-Path $src "VERSION") (Join-Path $Prefix "VERSION")

# Launcher: a .cmd shim that sets TOCIN_PATH + the DLL search path, then runs the exe.
$launcher = @"
@echo off
set "TOCIN_HOME=%~dp0.."
if not defined TOCIN_PATH set "TOCIN_PATH=%TOCIN_HOME%\stdlib"
set "PATH=%TOCIN_HOME%\lib;%PATH%"
"%TOCIN_HOME%\libexec\tocin.exe" %*
"@
Set-Content -Path (Join-Path $Prefix "bin\tocin.cmd") -Value $launcher -Encoding ASCII

# Uninstaller.
$uninstall = @"
`$ErrorActionPreference = 'Stop'
Write-Host 'Removing Tocin from $Prefix'
`$p = [Environment]::GetEnvironmentVariable('Path','User')
`$p = (`$p -split ';' | Where-Object { `$_ -ne '$Prefix\bin' }) -join ';'
[Environment]::SetEnvironmentVariable('Path', `$p, 'User')
Remove-Item -Recurse -Force '$Prefix'
Write-Host 'Tocin uninstalled. Open a new terminal for PATH changes to apply.'
"@
Set-Content -Path (Join-Path $Prefix "uninstall.ps1") -Value $uninstall -Encoding UTF8

if (-not $NoModifyPath) {
    $userPath = [Environment]::GetEnvironmentVariable('Path','User')
    $bin = Join-Path $Prefix "bin"
    if (($userPath -split ';') -notcontains $bin) {
        [Environment]::SetEnvironmentVariable('Path', "$bin;$userPath", 'User')
        Write-Host ">> added $bin to your user PATH"
    }
}

Write-Host ""
Write-Host "Tocin $ver installed."
Write-Host "  binary : $Prefix\bin\tocin.cmd"
Write-Host "  stdlib : $Prefix\stdlib"
Write-Host "  docs   : $Prefix\share\docs"
Write-Host "  remove : powershell -File $Prefix\uninstall.ps1"
Write-Host ""
Write-Host "Open a new terminal, then:  tocin --version"
Write-Host "Note: native '-o' output links via a system C compiler (e.g. clang/MSVC);"
Write-Host "the JIT ('tocin file.to --run') needs no external tools."
