# Build script for Tocin compiler with LLVM library
# Comprehensive build script for Tocin compiler with LLVM linking
# This script handles both compilation and linking directly

# Environment setup
$MINGW_DIR = "C:\msys64\mingw64"
$CXX = "$MINGW_DIR\bin\g++.exe"
$LLVM_INCLUDE = "$MINGW_DIR\include"
$LLVM_LIB = "$MINGW_DIR\lib"

# Set compiler flags
$CPP_FLAGS = "-std=c++17 -O2 -Wall -Wextra -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS"
$INCLUDE_FLAGS = "-Isrc -I`"$LLVM_INCLUDE`""
$LINK_FLAGS = "-L`"$LLVM_LIB`" -lLLVM -lmingw32 -lstdc++ -lpthread -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -lversion -lz"

# Clean and create directories
Write-Host "Setting up build directories..." -ForegroundColor Cyan
if (Test-Path -Path "build") {
    Remove-Item -Path "build" -Recurse -Force
}
New-Item -Path "build" -ItemType Directory | Out-Null
New-Item -Path "build\obj" -ItemType Directory | Out-Null

# Source files to compile
$SOURCE_FILES = @(
    "src/main.cpp",
    "src/compiler/compiler.cpp",
    "src/codegen/ir_generator.cpp",
    "src/error/error_handler.cpp",
    "src/type/type_checker.cpp",
    "src/parser/parser.cpp",
    "src/lexer/lexer.cpp",
    "src/lexer/token.cpp"
)

# Compile each source file
Write-Host "Compiling source files..." -ForegroundColor Cyan
$OBJ_FILES = @()

foreach ($src in $SOURCE_FILES) {
    if (Test-Path $src) {
        $objName = "build/obj/$($src -replace '/', '_' -replace '\.cpp$', '.o')"
        $OBJ_FILES += $objName
        
        Write-Host "Compiling $src -> $objName" -ForegroundColor Yellow
        $compileCmd = "$CXX -c $src -o $objName $INCLUDE_FLAGS $CPP_FLAGS"
        Write-Host $compileCmd
        Invoke-Expression $compileCmd
        
        if (-not $?) {
            Write-Host "Error: Failed to compile $src" -ForegroundColor Red
            exit 1
        }
    }
    else {
        Write-Host "Warning: Source file $src not found, skipping" -ForegroundColor Yellow
    }
}

# Link everything
Write-Host "Linking executable..." -ForegroundColor Cyan
$objFilesStr = $OBJ_FILES -join " "
$linkCmd = "$CXX $objFilesStr -o build/tocin.exe $LINK_FLAGS"
Write-Host $linkCmd
Invoke-Expression $linkCmd

if ($?) {
    Write-Host "Build successful! Executable is at build/tocin.exe" -ForegroundColor Green
}
else {
    Write-Host "Error: Linking failed" -ForegroundColor Red
    exit 1
}

# Test run the executable
Write-Host "Testing executable..." -ForegroundColor Cyan
if (Test-Path "build/tocin.exe") {
    try {
        $testCmd = "build/tocin.exe --version"
        Write-Host "Running: $testCmd"
        Invoke-Expression $testCmd
    }
    catch {
        Write-Host "Error: Failed to run executable. It may have unresolved dependencies." -ForegroundColor Red
        Write-Host "Error details: $_" -ForegroundColor Red
    }
}
else {
    Write-Host "Error: Executable not found" -ForegroundColor Red
}

Write-Host "Build process complete" -ForegroundColor Cyan 
