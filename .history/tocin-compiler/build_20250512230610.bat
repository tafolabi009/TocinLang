@echo off
setlocal enabledelayedexpansion

echo Tocin Compiler Build Script
echo ===========================

REM Get the project root directory
set PROJECT_ROOT=%~dp0

REM Set include paths
set INCLUDE_PATHS=-I"%PROJECT_ROOT%src" -IC:/msys64/mingw64/include -IC:/msys64/mingw64/include/llvm

REM Set compiler flags
set COMPILER_FLAGS=-std=c++17 -D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS

REM Source files
set SOURCES="%PROJECT_ROOT%src\main.cpp"

REM Check if error_handler.cpp exists and add it
if exist "%PROJECT_ROOT%src\error\error_handler.cpp" (
    set SOURCES=!SOURCES! "%PROJECT_ROOT%src\error\error_handler.cpp"
)

REM Check if lexer files exist and add them
if exist "%PROJECT_ROOT%src\lexer\lexer.cpp" (
    set SOURCES=!SOURCES! "%PROJECT_ROOT%src\lexer\lexer.cpp"
)
if exist "%PROJECT_ROOT%src\lexer\token.cpp" (
    set SOURCES=!SOURCES! "%PROJECT_ROOT%src\lexer\token.cpp"
)

REM Check if parser files exist and add them
if exist "%PROJECT_ROOT%src\parser\parser.cpp" (
    set SOURCES=!SOURCES! "%PROJECT_ROOT%src\parser\parser.cpp"
)

REM Create build directory if it doesn't exist
if not exist "%PROJECT_ROOT%build" mkdir "%PROJECT_ROOT%build"
cd "%PROJECT_ROOT%build"

REM Check for compiler type
set COMPILER_FOUND=0

REM Try MSVC compiler first
where cl.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using Microsoft Visual C++ compiler
    set COMPILER_FOUND=1
    
    REM Add MSVC-specific flags
    set COMPILER_FLAGS=%COMPILER_FLAGS% /EHsc /MD
    
    REM Compile
    cl.exe %COMPILER_FLAGS% %INCLUDE_PATHS% %SOURCES% /link /OUT:tocin.exe
    
    if %ERRORLEVEL% EQU 0 (
        echo Compilation successful!
    ) else (
        echo Compilation failed!
    )
    goto :EOF
)

REM Try Clang compiler
where clang++.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using Clang compiler
    set COMPILER_FOUND=1
    
    REM Compile
    clang++ %COMPILER_FLAGS% %INCLUDE_PATHS% %SOURCES% -o tocin.exe
    
    if %ERRORLEVEL% EQU 0 (
        echo Compilation successful!
    ) else (
        echo Compilation failed!
    )
    goto :EOF
)

REM Try MinGW g++ compiler in common locations
set GXX_PATHS=C:\msys64\mingw64\bin;C:\MinGW\bin;C:\TDM-GCC\bin

for %%p in (%GXX_PATHS%) do (
    if exist "%%p\g++.exe" (
        echo Found g++ at %%p\g++.exe
        set COMPILER_FOUND=1
        set PATH=%%p;!PATH!
        
        REM Compile
        g++ %COMPILER_FLAGS% %INCLUDE_PATHS% %SOURCES% -o tocin.exe
        
        if %ERRORLEVEL% EQU 0 (
            echo Compilation successful!
        ) else (
            echo Compilation failed!
        )
        goto :EOF
    )
)

if %COMPILER_FOUND% EQU 0 (
    echo No supported compiler found. Please install Visual C++, Clang, or MinGW g++.
    echo Suggested installation paths:
    echo - MSYS2/MinGW: https://www.msys2.org/
    echo - Visual Studio Community: https://visualstudio.microsoft.com/vs/community/
    echo After installation, ensure the compiler is in your PATH.
)

endlocal 
