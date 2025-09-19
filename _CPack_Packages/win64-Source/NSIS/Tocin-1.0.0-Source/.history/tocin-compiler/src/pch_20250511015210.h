#pragma once

// Standard library headers
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <variant>
#include <functional>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <cassert>

// Disable LLVM warnings (MSYS2/MinGW specific)
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

// LLVM headers
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>

// Re-enable warnings
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// Project-specific headers
#include "error/error_handler.h"

// Useful macros
#define TOCIN_UNUSED(x) (void)(x)

// Version information
#define TOCIN_VERSION_MAJOR 1
#define TOCIN_VERSION_MINOR 0
#define TOCIN_VERSION_PATCH 0 
