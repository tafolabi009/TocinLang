# Simple build script for the IR generator component

Write-Host "Looking for compiler..."
$compiler = "C:\msys64\mingw64\bin\g++.exe"

if (-not (Test-Path $compiler)) {
    Write-Host "Compiler not found at $compiler" -ForegroundColor Red
    exit 1
}

Write-Host "Compiler found: $compiler" -ForegroundColor Green

# Create output directory
if (-not (Test-Path -Path "build")) {
    New-Item -Path "build" -ItemType Directory | Out-Null
}

# Try to compile just the IR generator to isolate issues
$includeDir = "C:\msys64\mingw64\include"
Write-Host "Using include directory: $includeDir"

# List LLVM include directory contents to see what's available
Write-Host "Checking LLVM headers..."
if (Test-Path -Path "$includeDir\llvm") {
    Write-Host "LLVM directory exists" -ForegroundColor Green
    $llvmHeaders = Get-ChildItem -Path "$includeDir\llvm" -Filter "*.h" | Select-Object -First 5
    if ($llvmHeaders.Count -gt 0) {
        Write-Host "Found LLVM headers (showing first 5):"
        foreach ($header in $llvmHeaders) {
            Write-Host "  $($header.Name)"
        }
    }
    else {
        Write-Host "No LLVM headers found" -ForegroundColor Red
    }
}
else {
    Write-Host "LLVM include directory not found at $includeDir\llvm" -ForegroundColor Red
}

# Compile with verbose output to see the exact error
Write-Host "Attempting to compile IR generator..."
$command = "$compiler -c src\codegen\ir_generator.cpp -o build\ir_generator.o -Isrc -I`"$includeDir`" -std=c++17 -v -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS"
Write-Host "Running: $command"
Invoke-Expression "& $command 2>&1"

$result = $?
if ($result) {
    Write-Host "Successfully compiled IR generator" -ForegroundColor Green
}
else {
    Write-Host "Failed to compile IR generator" -ForegroundColor Red
}

# Use single file compile to see if that works
Write-Host "Trying alternative compilation approach with a test file..."
$testFile = "build\test.cpp"
Set-Content -Path $testFile -Value @"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <iostream>

int main() {
    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("test", context);
    std::cout << "Module created successfully" << std::endl;
    return 0;
}
"@

$testCommand = "$compiler $testFile -o build\test.exe -I`"$includeDir`" -std=c++17 -lLLVM -L`"C:\msys64\mingw64\lib`" -v"
Write-Host "Running: $testCommand"
Invoke-Expression "& $testCommand 2>&1"

$result = $?
if ($result) {
    Write-Host "Successfully compiled test file" -ForegroundColor Green
}
else {
    Write-Host "Failed to compile test file" -ForegroundColor Red
    Write-Host "This indicates that there are issues with the LLVM integration."
    Write-Host "Recommended fix: Install LLVM libraries for MinGW and ensure they match the headers."
}
