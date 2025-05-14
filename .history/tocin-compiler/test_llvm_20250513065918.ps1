# Script to test LLVM integration

# Configure paths
$LLVM_INCLUDE = "C:/msys64/mingw64/include"
$LLVM_LIB = "C:/msys64/mingw64/lib"
$COMPILER = "C:/msys64/mingw64/bin/g++.exe"

Write-Host "Testing LLVM integration with minimal example..."
Write-Host "=============================================="
Write-Host ""

# Compile the test file
Write-Host "Compiling test_llvm.cpp..."
& $COMPILER -o test_llvm.exe test_llvm.cpp -std=c++17 `
    -I"$LLVM_INCLUDE" `
    -L"$LLVM_LIB" `
    -lLLVM `
    -lz -lzstd -lffi -lxml2 -ladvapi32 -lole32 -lshell32 -luuid -lws2_32 -lpsapi `
    -D_FILE_OFFSET_BITS=64 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

if ($LASTEXITCODE -eq 0) {
    Write-Host "Compilation successful!" -ForegroundColor Green
    
    # Run the test
    Write-Host ""
    Write-Host "Running test_llvm.exe..."
    Write-Host ""
    & ./test_llvm.exe
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "LLVM integration test PASSED!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Your LLVM installation is working correctly."
        Write-Host "You can now proceed with fixing the IR generator."
    } else {
        Write-Host ""
        Write-Host "LLVM integration test FAILED!" -ForegroundColor Red
        Write-Host ""
        Write-Host "The test compiled successfully but crashed when run."
        Write-Host "This might indicate missing runtime dependencies or DLLs."
        Write-Host "Try copying all LLVM DLLs to the current directory."
    }
} else {
    Write-Host "Compilation failed!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Possible issues:"
    Write-Host "1. LLVM include/lib paths are incorrect"
    Write-Host "2. Missing LLVM development packages"
    Write-Host "3. Incompatible LLVM version"
    Write-Host ""
    Write-Host "Try running with verbose mode to see more details:"
    Write-Host "g++ -v -o test_llvm.exe test_llvm.cpp ..."
} 
