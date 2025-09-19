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
#include <string>
#include <map>
#include <vector>
#include <set>

namespace ffi {
    class PythonFFI;
    class JavaScriptFFI;
}

namespace compiler {

    using ErrorHandler = error::ErrorHandler; // Alias for compatibility

    /**
     * @brief Represents a compiled module in the Tocin language.
     */
    class ModuleInfo {
    public:
        ModuleInfo(const std::string& name, const std::string& path) 
            : name(name), path(path), ast(nullptr), llvmModule(nullptr), isCompiled(false) {}
        
        std::string name;                    // Module name
        std::string path;                    // File path
        ast::StmtPtr ast;                    // AST of the module
        std::unique_ptr<llvm::Module> llvmModule;   // LLVM module
        bool isCompiled;                     // Whether the module has been compiled
        
        // Exported symbols
        std::set<std::string> exportedFunctions;
        std::set<std::string> exportedClasses;
        std::set<std::string> exportedVariables;
        std::set<std::string> exportedTypes;
        
        // Module dependencies
        std::vector<std::string> dependencies;

        // Check if a symbol is exported
        bool isExported(const std::string& symbol) const {
            return exportedFunctions.count(symbol) > 0 || 
                   exportedClasses.count(symbol) > 0 ||
                   exportedVariables.count(symbol) > 0 ||
                   exportedTypes.count(symbol) > 0;
        }
    };

    /**
     * @brief Provides context for a compilation session.
     */
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
        
        // Path management
        void addModulePath(const std::string& path);
        std::vector<std::string> getModulePaths() const;
        
        // Module management
        std::shared_ptr<ModuleInfo> getModule(const std::string& name);
        std::shared_ptr<ModuleInfo> loadModule(const std::string& name);
        bool moduleExists(const std::string& name) const;
        void addModule(const std::string& name, std::shared_ptr<ModuleInfo> module);
        
        // Check for circular dependencies
        bool hasCircularDependency(const std::string& moduleName, 
                                   std::vector<std::string>& path) const;
        
        // Symbol management for the main module
        void addGlobalSymbol(const std::string& name, bool exported = false);
        bool symbolExists(const std::string& name) const;
        
        // Import symbol from a module
        bool importSymbol(const std::string& moduleName, const std::string& symbolName);
        bool importAllSymbols(const std::string& moduleName);
        
        // Get a qualified name (module::symbol)
        std::string getQualifiedName(const std::string& moduleName, const std::string& symbolName) const;
        
        // Get the source code of a module
        std::string getModuleSource(const std::string& moduleName) const;

        explicit CompilationContext(const std::string& filename = "<unknown>");
        ~CompilationContext();
        void initializeFFI();
        void initializeTypes();
        llvm::Type* getLLVMType(const ast::TypePtr& type);
        llvm::StructType* getListType();
        llvm::StructType* getDictType();

    private:
        std::vector<std::string> modulePaths;
        std::map<std::string, std::shared_ptr<ModuleInfo>> modules;
        std::set<std::string> globalSymbols;
        std::set<std::string> exportedSymbols;
        
        // Find module file by searching module paths
        std::string findModuleFile(const std::string& moduleName) const;
    };

} // namespace compiler

#endif // TOCIN_COMPILATION_CONTEXT_H
