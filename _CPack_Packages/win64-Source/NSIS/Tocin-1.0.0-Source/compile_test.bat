@echo off
cd /d D:\Downloads\tocin-compiler
D:\Downloads\msys64\mingw64\bin\c++.exe -c src/codegen/ir_generator.cpp -o ir_generator_test.o -Isrc -std=c++17 -ID:/Downloads/msys64/mingw64/include -Isrc/ast -Isrc/lexer -Isrc/type -Isrc/error -Isrc/runtime -Isrc/compiler -D_FILE_OFFSET_BITS=64 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS 2>&1
