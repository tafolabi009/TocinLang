# Direct build script for Tocin compiler with explicit LLVM linking

$MINGW_DIR = "C:\msys64\mingw64"
$CXX = "$MINGW_DIR\bin\g++.exe"
$LLVM_INCLUDE = "$MINGW_DIR\include"
$LLVM_LIB = "$MINGW_DIR\lib"

# Clean previous builds
if (Test-Path -Path "build") {
    Remove-Item -Path "build" -Recurse -Force
}
New-Item -Path "build" -ItemType Directory -Force
New-Item -Path "build\obj" -ItemType Directory -Force

# Print build environment
Write-Host "Building with compiler: $CXX"
Write-Host "LLVM include directory: $LLVM_INCLUDE"
Write-Host "LLVM library directory: $LLVM_LIB"

# Check if important LLVM library files exist
$llvmLibs = @(
    "libLLVMCore.dll.a",
    "libLLVMSupport.dll.a",
    "libLLVMIRReader.dll.a",
    "libLLVMCodeGen.dll.a"
)

Write-Host "Checking LLVM library files..."
foreach ($lib in $llvmLibs) {
    $libPath = Join-Path -Path $LLVM_LIB -ChildPath $lib
    if (Test-Path -Path $libPath) {
        Write-Host "  Found: $lib"
    }
    else {
        Write-Host "  Missing: $lib" -ForegroundColor Red
    }
}

# Compile IR generator
Write-Host "Compiling IR generator..."
& $CXX -c src/codegen/ir_generator.cpp -o build/obj/ir_generator.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

if (-not $?) {
    Write-Host "Error: Failed to compile IR generator" -ForegroundColor Red
    exit 1
}

# Compile other source files
Write-Host "Compiling main sources..."
& $CXX -c src/main.cpp -o build/obj/main.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

if (-not $?) {
    Write-Host "Error: Failed to compile main.cpp" -ForegroundColor Red
    exit 1
}

& $CXX -c src/compiler/compiler.cpp -o build/obj/compiler.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

if (-not $?) {
    Write-Host "Error: Failed to compile compiler.cpp" -ForegroundColor Red
    exit 1
}

& $CXX -c src/error/error_handler.cpp -o build/obj/error_handler.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

if (-not $?) {
    Write-Host "Error: Failed to compile error_handler.cpp" -ForegroundColor Red
    exit 1
}

# Try to find LLVM libraries - adjust from dll.a to plain .a if needed
Write-Host "Looking for available LLVM libraries..."
$llvmLibPattern = "$LLVM_LIB\libLLVM*.dll.a"
$llvmLibFiles = Get-ChildItem -Path $llvmLibPattern | ForEach-Object { $_.Name -replace 'lib([^.]+)\.dll\.a', '$1' }

if ($llvmLibFiles.Count -gt 0) {
    Write-Host "Found these LLVM libraries (showing first 10):"
    $llvmLibFiles | Select-Object -First 10 | ForEach-Object { Write-Host "  LLVM$_" }
    if ($llvmLibFiles.Count -gt 10) {
        Write-Host "  ... and $($llvmLibFiles.Count - 10) more"
    }
}
else {
    Write-Host "No LLVM libraries found with pattern $llvmLibPattern" -ForegroundColor Red
    # Try alternative pattern
    $llvmLibPattern = "$LLVM_LIB\libLLVM*.a"
    $llvmLibFiles = Get-ChildItem -Path $llvmLibPattern | ForEach-Object { $_.Name -replace 'lib([^.]+)\.a', '$1' }
    
    if ($llvmLibFiles.Count -gt 0) {
        Write-Host "Found these LLVM libraries with alternative pattern (showing first 10):"
        $llvmLibFiles | Select-Object -First 10 | ForEach-Object { Write-Host "  LLVM$_" }
    }
    else {
        Write-Host "No LLVM libraries found with alternative pattern $llvmLibPattern" -ForegroundColor Red
    }
}

# Link everything with dynamic linking to LLVM
Write-Host "Linking Tocin compiler with LLVM shared libraries..."

$LINK_COMMAND = "$CXX build/obj/main.o build/obj/compiler.o build/obj/ir_generator.o build/obj/error_handler.o -o build/tocin.exe " +
"-L$LLVM_LIB " +
"-lLLVM " + # Use the single LLVM library if available
"-lmingw32 -lz -lzlib -lstdc++ " +
"-lpthread -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -lversion"

Write-Host "Running linker command: $LINK_COMMAND"
Invoke-Expression $LINK_COMMAND

if ($?) {
    Write-Host "Build complete. Executable is at build/tocin.exe" -ForegroundColor Green
}
else {
    Write-Host "Error: Failed to link executable" -ForegroundColor Red
    
    # Try alternative linking approach with static libraries
    Write-Host "Trying alternative linking approach..." -ForegroundColor Yellow
    
    $LINK_COMMAND_ALT = "$CXX build/obj/main.o build/obj/compiler.o build/obj/ir_generator.o build/obj/error_handler.o -o build/tocin.exe " +
    "-L$LLVM_LIB " +
    "-Wl,--whole-archive -lLLVM -Wl,--no-whole-archive " +
    "-lmingw32 -lz -lzlib -lstdc++ " +
    "-lpthread -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -lversion"
    
    Write-Host "Running alternative linker command: $LINK_COMMAND_ALT"
    Invoke-Expression $LINK_COMMAND_ALT
    
    if ($?) {
        Write-Host "Build complete with alternative linking. Executable is at build/tocin.exe" -ForegroundColor Green
    }
    else {
        Write-Host "Error: Failed to link executable with alternative approach" -ForegroundColor Red
    }
} 
