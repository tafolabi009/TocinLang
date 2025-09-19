# Complete Windows Installer Builder for Tocin Compiler
# This script builds the compiler and creates a self-contained installer

param(
    [string]$BuildType = "Release",
    [switch]$Clean = $false
)

Write-Host "=== Tocin Compiler Windows Installer Builder ===" -ForegroundColor Green

# Get the script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# Clean build if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Yellow
$cmakeArgs = @(
    "-G", "MinGW Makefiles",
    "-S", ".",
    "-B", "build",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DWITH_DEBUGGER=OFF",
    "-DWITH_WASM=OFF", 
    "-DWITH_PACKAGE_MANAGER=OFF"
)

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
    exit 1
}

# Build the project
Write-Host "Building project..." -ForegroundColor Yellow
& cmake --build build --config $BuildType -- -j4
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit 1
}

# Find the built executable
$tocinExe = Get-ChildItem -Path "build" -Name "tocin.exe" -Recurse | Select-Object -First 1
if (-not $tocinExe) {
    Write-Error "tocin.exe not found in build directory!"
    exit 1
}

$tocinPath = Join-Path "build" $tocinExe
Write-Host "Found tocin.exe at: $tocinPath" -ForegroundColor Green

# Create installer staging directory
$stagingDir = "build/installer_staging"
$binDir = "$stagingDir/bin"
$libDir = "$stagingDir/lib"
$docDir = "$stagingDir/doc"

Write-Host "Creating installer staging directory..." -ForegroundColor Yellow
Remove-Item -Recurse -Force $stagingDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $binDir | Out-Null
New-Item -ItemType Directory -Path $libDir | Out-Null
New-Item -ItemType Directory -Path $docDir | Out-Null

# Copy the main executable
Copy-Item $tocinPath $binDir

# Find and copy all required DLLs
Write-Host "Finding and copying required DLLs..." -ForegroundColor Yellow

# Get the directory containing the compiler (MSYS2 MinGW64 bin)
$compilerPath = (Get-Command gcc).Source
$mingwBinDir = Split-Path $compilerPath

Write-Host "MSYS2 MinGW64 bin directory: $mingwBinDir" -ForegroundColor Cyan

# List of required DLLs
$requiredDlls = @(
    "libwinpthread-1.dll",
    "libstdc++-6.dll", 
    "libgcc_s_seh-1.dll",
    "libzlib.dll"
)

# Copy base DLLs
foreach ($dll in $requiredDlls) {
    $dllPath = Join-Path $mingwBinDir $dll
    if (Test-Path $dllPath) {
        Copy-Item $dllPath $binDir
        Write-Host "Copied: $dll" -ForegroundColor Green
    } else {
        Write-Warning "Missing: $dll"
    }
}

# Copy V8 DLLs if they exist
$v8Dlls = @("libv8.dll", "libv8_libbase.dll", "libv8_libplatform.dll")
foreach ($dll in $v8Dlls) {
    $dllPath = Join-Path $mingwBinDir $dll
    if (Test-Path $dllPath) {
        Copy-Item $dllPath $binDir
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

# Copy ICU DLLs
$icuDlls = Get-ChildItem -Path $mingwBinDir -Name "libicu*.dll"
foreach ($dll in $icuDlls) {
    $dllPath = Join-Path $mingwBinDir $dll
    Copy-Item $dllPath $binDir
    Write-Host "Copied: $dll" -ForegroundColor Green
}

# Copy other common DLLs
$otherDlls = @("libxml2-2.dll", "libffi-*.dll", "libzstd.dll")
foreach ($pattern in $otherDlls) {
    $dlls = Get-ChildItem -Path $mingwBinDir -Name $pattern -ErrorAction SilentlyContinue
    foreach ($dll in $dlls) {
        $dllPath = Join-Path $mingwBinDir $dll
        Copy-Item $dllPath $binDir
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
        Copy-Item $dllPath $binDir
        Write-Host "Copied: $dll" -ForegroundColor Green
    }
}

# Copy documentation
if (Test-Path "README.md") {
    Copy-Item "README.md" $docDir
}
if (Test-Path "docs") {
    Copy-Item -Recurse "docs" $docDir
}

# Create a simple test script
$testScript = @"
@echo off
echo Testing Tocin Compiler...
echo.
echo Version:
bin\tocin.exe --version
echo.
echo Help:
bin\tocin.exe --help
echo.
echo Test completed. Press any key to exit.
pause
"@
$testScript | Out-File -FilePath "$stagingDir/test_tocin.bat" -Encoding ASCII

Write-Host "Installer staging complete!" -ForegroundColor Green
Write-Host "Files staged in: $stagingDir" -ForegroundColor Cyan

# List what we have
Write-Host "`nStaged files:" -ForegroundColor Yellow
Get-ChildItem -Recurse $stagingDir | ForEach-Object {
    Write-Host "  $($_.FullName.Replace((Get-Location).Path + '\', ''))" -ForegroundColor White
}

# Create a simple installer using NSIS if available
$makensis = Get-Command makensis -ErrorAction SilentlyContinue
if ($makensis) {
    Write-Host "`nCreating NSIS installer..." -ForegroundColor Yellow
    
    # Create a simple NSIS script
    $nsisScript = @"
!define PRODUCT_NAME "Tocin Compiler"
!define VERSION "1.0.0"
!define PUBLISHER "Tocin Team"

Name "`${PRODUCT_NAME}"
OutFile "TocinCompiler-`${VERSION}-Setup.exe"
InstallDir "`$PROGRAMFILES64\Tocin"
RequestExecutionLevel admin

Section "Main"
    SetOutPath "`$INSTDIR"
    File /r "installer_staging\*"
    
    WriteUninstaller "`$INSTDIR\uninstall.exe"
    
    CreateDirectory "`$SMPROGRAMS\Tocin"
    CreateShortcut "`$SMPROGRAMS\Tocin\Tocin Compiler.lnk" "`$INSTDIR\bin\tocin.exe"
    CreateShortcut "`$SMPROGRAMS\Tocin\Test Tocin.lnk" "`$INSTDIR\test_tocin.bat"
    CreateShortcut "`$SMPROGRAMS\Tocin\Uninstall.lnk" "`$INSTDIR\uninstall.exe"
    
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "DisplayName" "`${PRODUCT_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "UninstallString" "`$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "DisplayVersion" "`${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin" "Publisher" "`${PUBLISHER}"
    
    EnVar::SetHKLM
    EnVar::AddValue "Path" "`$INSTDIR\bin"
SectionEnd

Section "Uninstall"
    RMDir /r "`$INSTDIR"
    Delete "`$SMPROGRAMS\Tocin\*.*"
    RMDir "`$SMPROGRAMS\Tocin"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tocin"
    EnVar::SetHKLM
    EnVar::DeleteValue "Path" "`$INSTDIR\bin"
SectionEnd
"@
    
    $nsisScript | Out-File -FilePath "build/installer.nsi" -Encoding ASCII
    
    # Run NSIS
    Set-Location build
    & makensis installer.nsi
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nNSIS installer created successfully!" -ForegroundColor Green
        $installerFile = Get-ChildItem -Name "TocinCompiler-*-Setup.exe"
        Write-Host "Installer: build\$installerFile" -ForegroundColor Cyan
    } else {
        Write-Warning "NSIS installer creation failed, but staging is complete."
    }
    Set-Location ..
} else {
    Write-Host "`nNSIS not found. Installer staging is complete." -ForegroundColor Yellow
    Write-Host "You can manually create an installer or run tocin.exe from: $binDir" -ForegroundColor Cyan
}

Write-Host "`n=== Build Complete ===" -ForegroundColor Green
Write-Host "You can test the compiler by running: $binDir\tocin.exe --version" -ForegroundColor Cyan
