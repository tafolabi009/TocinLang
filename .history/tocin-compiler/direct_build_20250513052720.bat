@echo off
REM Direct build script for Tocin compiler with explicit LLVM linking

set MINGW_DIR=C:\msys64\mingw64
set CXX=%MINGW_DIR%\bin\g++.exe
set LLVM_INCLUDE=%MINGW_DIR%\include
set LLVM_LIB=%MINGW_DIR%\lib

REM Clean previous builds
if exist build\NUL rd /s /q build
mkdir build
mkdir build\obj

REM Print build environment
echo Building with compiler: %CXX%
echo LLVM include directory: %LLVM_INCLUDE%
echo LLVM library directory: %LLVM_LIB%

REM Compile IR generator
echo Compiling IR generator...
%CXX% -c src/codegen/ir_generator.cpp -o build/obj/ir_generator.o -Isrc -I%LLVM_INCLUDE% -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

REM Compile other source files
echo Compiling main sources...
%CXX% -c src/main.cpp -o build/obj/main.o -Isrc -I%LLVM_INCLUDE% -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
%CXX% -c src/compiler/compiler.cpp -o build/obj/compiler.o -Isrc -I%LLVM_INCLUDE% -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
%CXX% -c src/error/error_handler.cpp -o build/obj/error_handler.o -Isrc -I%LLVM_INCLUDE% -std=c++17 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

REM Link everything with explicit LLVM libraries
echo Linking Tocin compiler...
%CXX% build/obj/main.o build/obj/compiler.o build/obj/ir_generator.o build/obj/error_handler.o -o build/tocin.exe ^
  -L%LLVM_LIB% ^
  -lLLVMCore -lLLVMSupport -lLLVMIRReader -lLLVMCodeGen -lLLVMMC -lLLVMMCParser -lLLVMPasses ^
  -lLLVMTarget -lLLVMX86CodeGen -lLLVMX86AsmParser -lLLVMX86Desc -lLLVMX86Info -lLLVMAsmPrinter ^
  -lLLVMAnalysis -lLLVMTransformUtils -lLLVMInstCombine -lLLVMScalarOpts -lLLVMBitWriter ^
  -lLLVMBitReader -lLLVMObject -lLLVMExecutionEngine -lLLVMMCJIT -lLLVMOrcJIT -lLLVMRuntimeDyld ^
  -lLLVMNativeCodeGen -lLLVMipo -lLLVMLinker -lLLVMDebugInfoDWARF -lLLVMDebugInfoMSF ^
  -lLLVMProfiles -lLLVMruntimedyld -lLLVMSelectionDAG -lmingw32 -lz -lzlib -lstdc++ ^
  -lpthread -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lole32 -loleaut32 -luuid -lversion

echo Build complete. Executable is at build/tocin.exe 
