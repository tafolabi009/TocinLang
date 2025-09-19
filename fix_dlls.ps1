# Fix DLL dependencies for tocin.exe
# This script copies all required DLLs to make tocin.exe work standalone

Write-Host "=== Fixing DLL Dependencies for Tocin Compiler ===" -ForegroundColor Green

# MSYS2 paths
$MSYS2_ROOT = "D:\Downloads\msys64"
$MINGW_BIN = "$MSYS2_ROOT\mingw64\bin"

# Find tocin.exe
$tocinExe = Get-ChildItem -Path "." -Name "tocin.exe" -Recurse | Select-Object -First 1
if (-not $tocinExe) {
    Write-Error "tocin.exe not found! Please build the project first."
    exit 1
}

$tocinDir = Split-Path $tocinExe
Write-Host "Found tocin.exe in: $tocinDir" -ForegroundColor Green

# List of required DLLs
$requiredDlls = @(
    "libwinpthread-1.dll",
    "libstdc++-6.dll", 
    "libgcc_s_seh-1.dll",
    "libzlib.dll"
)

Write-Host "Copying required DLLs..." -ForegroundColor Yellow

# Copy base DLLs
foreach ($dll in $requiredDlls) {
    $dllPath = Join-Path $MINGW_BIN $dll
    if (Test-Path $dllPath) {
        Copy-Item $dllPath $tocinDir -Force
        Write-Host "Copied: $dll" -ForegroundColor Green
    } else {
        Write-Warning "Missing: $dll"
    }
}

# Copy V8 DLLs if they exist
$v8Dlls = @("libv8.dll", "libv8_libbase.dll", "libv8_libplatform.dll")
foreach ($dll in $v8Dlls) {
    $dllPath = Join-Path $MINGW_BIN $dll
    if (Test-Path $dllPath) {
        Copy-Item $dllPath $tocinDir -Force
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

# Copy ICU DLLs
$icuDlls = Get-ChildItem -Path $MINGW_BIN -Name "libicu*.dll" -ErrorAction SilentlyContinue
foreach ($dll in $icuDlls) {
    $dllPath = Join-Path $MINGW_BIN $dll
    Copy-Item $dllPath $tocinDir -Force
    Write-Host "Copied: $dll" -ForegroundColor Green
}

# Copy other common DLLs
$otherDlls = @("libxml2-2.dll", "libffi-*.dll", "libzstd.dll", "libLLVM-*.dll", "zlib1.dll", "libiconv-2.dll", "liblzma-5.dll", "libcharset-1.dll")
foreach ($pattern in $otherDlls) {
    $dlls = Get-ChildItem -Path $MINGW_BIN -Name $pattern -ErrorAction SilentlyContinue
    foreach ($dll in $dlls) {
        $dllPath = Join-Path $MINGW_BIN $dll
        Copy-Item $dllPath $tocinDir -Force
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

# Copy Python DLLs if Python is found
$pythonExe = (Get-Command python -ErrorAction SilentlyContinue).Source
if ($pythonExe) {
    $pythonDir = Split-Path $pythonExe
    $pythonDlls = Get-ChildItem -Path $pythonDir -Name "python*.dll" -ErrorAction SilentlyContinue
    foreach ($dll in $pythonDlls) {
        $dllPath = Join-Path $pythonDir $dll
        Copy-Item $dllPath $tocinDir -Force
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

Write-Host "`n=== DLL Copy Complete ===" -ForegroundColor Green

# Test the executable
Write-Host "Testing tocin.exe..." -ForegroundColor Yellow
$testExe = Join-Path $tocinDir "tocin.exe"
if (Test-Path $testExe) {
    try {
        $version = & $testExe --version 2>&1
        Write-Host "Version output: $version" -ForegroundColor Green
        Write-Host "`nSUCCESS! tocin.exe is now working with all required DLLs." -ForegroundColor Green
    } catch {
        Write-Warning "Could not run tocin.exe: $_"
        Write-Host "You may need to install additional dependencies." -ForegroundColor Yellow
    }
} else {
    Write-Warning "tocin.exe not found in expected location"
}

Write-Host "`nYou can now run: $testExe --version" -ForegroundColor Cyan
