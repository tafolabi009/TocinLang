#!/bin/bash
rm -rf build/*
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build --target package 
