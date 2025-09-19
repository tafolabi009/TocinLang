Write-Host "Tocin Compiler Build Script" -ForegroundColor Green
Write-Host "===========================" -ForegroundColor Green

# Set include paths
$includePaths = "-I..\src", "-IC:\msys64\mingw64\include", "-IC:\msys64\mingw64\include\llvm"

# Set compiler flags
$compilerFlags = "-std=c++17", "-D_WIN32_WINNT=0x0601", "-DWIN32_LEAN_AND_MEAN", "-DNOMINMAX", "-D_CRT_SECURE_NO_WARNINGS", "-D__STDC_CONSTANT_MACROS", "-D__STDC_FORMAT_MACROS", "-D__STDC_LIMIT_MACROS"

# Source files
$sources = @("..\src\main.cpp")

# Check if error_handler.cpp exists and add it
if (Test-Path "..\src\error\error_handler.cpp") {
    $sources += "..\src\error\error_handler.cpp"
}

# Check if lexer files exist and add them
if (Test-Path "..\src\lexer\lexer.cpp") {
    $sources += "..\src\lexer\lexer.cpp"
}
if (Test-Path "..\src\lexer\token.cpp") {
    $sources += "..\src\lexer\token.cpp"
}

# Check if parser files exist and add them
if (Test-Path "..\src\parser\parser.cpp") {
    $sources += "..\src\parser\parser.cpp"
}

# Check if type checker files exist and add them
if (Test-Path "..\src\type\type_checker.cpp") {
    $sources += "..\src\type\type_checker.cpp"
}

# Check if codegen files exist and add them
if (Test-Path "..\src\codegen\ir_generator.cpp") {
    $sources += "..\src\codegen\ir_generator.cpp"
}

# Check for compiler type
$compilerFound = $false

# Try MSVC compiler first
try {
    $null = Get-Command cl.exe -ErrorAction Stop
    Write-Host "Using Microsoft Visual C++ compiler" -ForegroundColor Cyan
    $compilerFound = $true
    
    # Add MSVC-specific flags
    $msvcFlags = "/EHsc", "/MD"
    
    # Compile
    $arguments = $compilerFlags + $msvcFlags + $includePaths + $sources + "/link", "/OUT:tocin.exe"
    & cl.exe $arguments
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Compilation successful!" -ForegroundColor Green
    }
    else {
        Write-Host "Compilation failed!" -ForegroundColor Red
    }
    exit
}
catch {
    # MSVC not found, continue to next option
}

# Try Clang compiler
try {
    $null = Get-Command clang++.exe -ErrorAction Stop
    Write-Host "Using Clang compiler" -ForegroundColor Cyan
    $compilerFound = $true
    
    # Compile
    $arguments = $compilerFlags + $includePaths + $sources + "-o", "tocin.exe"
    & clang++.exe $arguments
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Compilation successful!" -ForegroundColor Green
    }
    else {
        Write-Host "Compilation failed!" -ForegroundColor Red
    }
    exit
}
catch {
    # Clang not found, continue to next option
}

# Try MinGW g++ compiler in common locations
$gxxPaths = @(
    "C:\msys64\mingw64\bin\g++.exe",
    "C:\MinGW\bin\g++.exe",
    "C:\TDM-GCC\bin\g++.exe"
)

foreach ($gxxPath in $gxxPaths) {
    if (Test-Path $gxxPath) {
        Write-Host "Found g++ at $gxxPath" -ForegroundColor Cyan
        $compilerFound = $true
        
        # Add the directory to PATH temporarily
        $env:PATH = "$(Split-Path -Parent $gxxPath);$($env:PATH)"
        
        # Compile
        $arguments = $compilerFlags + $includePaths + $sources + "-o", "tocin.exe"
        & $gxxPath $arguments
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Compilation successful!" -ForegroundColor Green
        }
        else {
            Write-Host "Compilation failed!" -ForegroundColor Red
        }
        exit
    }
}

if (-not $compilerFound) {
    Write-Host "No supported compiler found. Please install Visual C++, Clang, or MinGW g++." -ForegroundColor Red
    Write-Host "Suggested installation paths:" -ForegroundColor Yellow
    Write-Host "- MSYS2/MinGW: https://www.msys2.org/" -ForegroundColor Yellow
    Write-Host "- Visual Studio Community: https://visualstudio.microsoft.com/vs/community/" -ForegroundColor Yellow
    Write-Host "After installation, ensure the compiler is in your PATH." -ForegroundColor Yellow
} 
