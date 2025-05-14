# Direct build script for Tocin with minimal dependencies
# This script handles both compilation and linking directly

# Configuration
$compiler = "C:\msys64\mingw64\bin\g++.exe"
$includes = "-Isrc -I`"C:\msys64\mingw64\include`""
$libs = "-L`"C:\msys64\mingw64\lib`""
$cppflags = "-std=c++17 -O2 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -DLLVM_ENABLE_NEW_PASS_MANAGER=0"

# LLVM libraries
$llvm_libs = "-lLLVM"

# Windows-specific libraries needed for LLVM
$win_libs = "-lmingw32 -lstdc++ -lpthread -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -lversion -lz"

# Setup build directories
Write-Host "Setting up build directories..."
$buildDir = "build"
$objDir = "$buildDir/obj"

# Create build directories if they don't exist
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

if (-not (Test-Path $objDir)) {
    New-Item -ItemType Directory -Path $objDir | Out-Null
}

# Define source files explicitly
$sourceFiles = @(
    "src/main.cpp",
    "src/compiler/compiler.cpp",
    "src/codegen/ir_generator.cpp",
    "src/error/error_handler.cpp",
    "src/type/type_checker.cpp",
    "src/parser/parser.cpp",
    "src/lexer/lexer.cpp",
    "src/lexer/token.cpp",
    "src/ast/ast.cpp",
    "src/ast/match_stmt.cpp",
    "src/ast/option_result_expr.cpp"
)

Write-Host "Compiling source files..."

# Compile each source file
$objFiles = @()
foreach ($file in $sourceFiles) {
    # Skip files that don't exist
    if (-not (Test-Path $file)) {
        Write-Host "File does not exist: $file - skipping" -ForegroundColor Yellow
        continue
    }
    
    $objFileName = $file -replace "[\/\\]", "_" -replace "\.cpp$", ".o"
    $objFilePath = "$objDir/$objFileName"
    $objFiles += $objFilePath
    
    Write-Host "Compiling $file -> $objFilePath"
    $compileCmd = "& `"$compiler`" -c `"$file`" -o `"$objFilePath`" $includes $cppflags"
    
    try {
        Invoke-Expression $compileCmd
        
        # Check if compilation was successful
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: Compilation failed for $file" -ForegroundColor Red
            exit 1
        }
    }
    catch {
        Write-Host "Exception during compilation of $file : $_" -ForegroundColor Red
        exit 1
    }
}

# Link all object files into executable
Write-Host "Linking executable..."
$exePath = "$buildDir/tocin.exe"
$objFilesStr = $objFiles -join " "
$linkCmd = "& `"$compiler`" $objFilesStr -o `"$exePath`" $libs $llvm_libs $win_libs"

try {
    Write-Host $linkCmd
    Invoke-Expression $linkCmd
    
    # Check if linking was successful
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Linking failed" -ForegroundColor Red
        exit 1
    }
    else {
        Write-Host "Build successful! Executable is at $exePath" -ForegroundColor Green
    }
}
catch {
    Write-Host "Exception during linking: $_" -ForegroundColor Red
    exit 1
}

# Test executable
Write-Host "Testing executable..."
if (Test-Path $exePath) {
    try {
        & $exePath --version
    }
    catch {
        Write-Host "Error running executable: $_" -ForegroundColor Red
    }
}
else {
    Write-Host "Error: Executable not found" -ForegroundColor Red
}

Write-Host "Build process complete" 
