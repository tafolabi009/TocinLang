#pragma once
#ifndef TOCIN_COMPILATION_CONTEXT_H
#define TOCIN_COMPILATION_CONTEXT_H

#include "../ast/ast.h"
#include "../ffi/ffi_cpp.h"
#include "../ffi/ffi_interface.h"
#include "../error/error_handler.h" // Use error::ErrorHandler
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>
#include <unordered_map>

namespace ffi {
    class PythonFFI;
    class JavaScriptFFI;
}

namespace compiler {

    using ErrorHandler = error::ErrorHandler; // Alias for compatibility

    class CompilationContext {
    public:
        std::unique_ptr<llvm::LLVMContext> context;
        std::unique_ptr<llvm::Module> module;
        std::unique_ptr<llvm::IRBuilder<>> builder;
        ErrorHandler* errorHandler;
        std::unique_ptr<ffi::PythonFFI> pythonFFI;
        std::unique_ptr<ffi::CppFFI> cppFFI;
        std::unique_ptr<ffi::JavaScriptFFI> jsFFI;
        ffi::FFIInterface* ffi;
        std::unordered_map<std::string, llvm::Type*> typeMap;

        explicit CompilationContext(const std::string& filename = "<unknown>");
        ~CompilationContext();
        void initializeFFI();
        void initializeTypes();
        llvm::Type* getLLVMType(const ast::TypePtr& type);
        llvm::StructType* getListType();
        llvm::StructType* getDictType();
    };

} // namespace compiler

#endif // TOCIN_COMPILATION_CONTEXT_H