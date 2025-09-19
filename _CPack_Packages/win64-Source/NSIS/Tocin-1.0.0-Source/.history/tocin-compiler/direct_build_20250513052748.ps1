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

# Compile IR generator
Write-Host "Compiling IR generator..."
& $CXX -c src/codegen/ir_generator.cpp -o build/obj/ir_generator.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# Compile other source files
Write-Host "Compiling main sources..."
& $CXX -c src/main.cpp -o build/obj/main.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
& $CXX -c src/compiler/compiler.cpp -o build/obj/compiler.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
& $CXX -c src/error/error_handler.cpp -o build/obj/error_handler.o -Isrc "-I$LLVM_INCLUDE" -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

# Link everything with explicit LLVM libraries
Write-Host "Linking Tocin compiler..."
$LINK_COMMAND = "$CXX build/obj/main.o build/obj/compiler.o build/obj/ir_generator.o build/obj/error_handler.o -o build/tocin.exe " +
"-L$LLVM_LIB " +
"-lLLVMCore -lLLVMSupport -lLLVMIRReader -lLLVMCodeGen -lLLVMMC -lLLVMMCParser -lLLVMPasses " +
"-lLLVMTarget -lLLVMX86CodeGen -lLLVMX86AsmParser -lLLVMX86Desc -lLLVMX86Info -lLLVMAsmPrinter " +
"-lLLVMAnalysis -lLLVMTransformUtils -lLLVMInstCombine -lLLVMScalarOpts -lLLVMBitWriter " +
"-lLLVMBitReader -lLLVMObject -lLLVMExecutionEngine -lLLVMMCJIT -lLLVMOrcJIT -lLLVMRuntimeDyld " +
"-lLLVMNativeCodeGen -lLLVMipo -lLLVMLinker -lLLVMDebugInfoDWARF -lLLVMDebugInfoMSF " +
"-lLLVMProfiles -lLLVMruntimedyld -lLLVMSelectionDAG -lmingw32 -lz -lzlib -lstdc++ " +
"-lpthread -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -lversion"

Invoke-Expression $LINK_COMMAND

Write-Host "Build complete. Executable is at build/tocin.exe" 
