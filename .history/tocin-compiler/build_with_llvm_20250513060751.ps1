# Build script for Tocin compiler with LLVM library

# Configuration
$compiler = "C:\msys64\mingw64\bin\g++.exe"
$includes = "-Isrc -I`"C:\msys64\mingw64\include`""
$libs = "-L`"C:\msys64\mingw64\lib`""
$cppflags = "-std=c++17 -O2 -Wall -Wextra -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS"

# LLVM libraries (using the consolidated -lLLVM option for MSYS2/MinGW)
$llvm_libs = "-lLLVM"

# Windows-specific libraries needed for LLVM
$win_libs = "-lmingw32 -lstdc++ -lpthread -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -lversion -lz"

# Setup build directories
$buildDir = "build"
$objDir = "$buildDir/obj"

Write-Host "Setting up build directories..."

# Create build directories if they don't exist
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

if (-not (Test-Path $objDir)) {
    New-Item -ItemType Directory -Path $objDir | Out-Null
}

# Collect all the .cpp files recursively
$sourceFiles = Get-ChildItem -Path "src" -Filter "*.cpp" -Recurse

Write-Host "Compiling source files..."

# Compile each source file
$objFiles = @()
foreach ($file in $sourceFiles) {
    $relativePath = $file.FullName.Substring((Get-Location).Path.Length + 1)
    $objFileName = $relativePath -replace "[\/\\]", "_" -replace "\.cpp$", ".o"
    $objFilePath = "$objDir/$objFileName"
    $objFiles += $objFilePath
    
    Write-Host "Compiling $relativePath -> $objFilePath"
    $compileCmd = "$compiler -c $relativePath -o $objFilePath $includes $cppflags"
    Invoke-Expression $compileCmd
    
    # Check if compilation was successful
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Compilation failed for $relativePath" -ForegroundColor Red
        exit 1
    }
}

# Link all object files into executable
Write-Host "Linking executable..."
$exePath = "$buildDir/tocin.exe"
$linkCmd = "$compiler $objFiles -o $exePath $libs $llvm_libs $win_libs"
Invoke-Expression $linkCmd

# Check if linking was successful
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Linking failed" -ForegroundColor Red
    exit 1
}
else {
    Write-Host "Build successful! Executable is at $exePath" -ForegroundColor Green
}

# Test executable
Write-Host "Testing executable..."
if (Test-Path $exePath) {
    & $exePath --version
}
else {
    Write-Host "Error: Executable not found" -ForegroundColor Red
}

Write-Host "Build process complete" 
