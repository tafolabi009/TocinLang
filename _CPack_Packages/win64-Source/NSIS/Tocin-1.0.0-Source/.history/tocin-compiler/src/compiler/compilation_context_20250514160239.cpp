#include "compilation_context.h"
#include "compiler/stdlib.h"
#include <llvm/IR/Verifier.h>
#include <filesystem>
#include <fstream>
#include <iostream>

// Include V8 headers first
#include "ffi/ffi_javascript.h"

// Then undefine the problematic macro before including Python headers
#ifdef COMPILER
#undef COMPILER
#endif

// Now include Python headers
#include "ffi/ffi_python.h"

namespace compiler
{

    CompilationContext::CompilationContext(const std::string &filename)
        : context(std::make_unique<llvm::LLVMContext>()),
          module(std::make_unique<llvm::Module>("tocin_module", *context)),
          builder(std::make_unique<llvm::IRBuilder<>>(*context)),
          errorHandler(new ErrorHandler(filename)),
          pythonFFI(std::make_unique<ffi::PythonFFI>()),
          cppFFI(std::make_unique<ffi::CppFFI>()),
          jsFFI(std::make_unique<ffi::JavaScriptFFI>()),
          ffi(pythonFFI.get()),
          currentFilename(filename)
    {
        initializeTypes();
        initializeFFI();

        // Initialize predefined module paths
        modulePaths.push_back("./modules");
        modulePaths.push_back("./src/modules");

        // Add current directory
        std::filesystem::path filePath(filename);
        if (filePath.has_parent_path())
        {
            modulePaths.push_back(filePath.parent_path().string());
        }
    }

    CompilationContext::~CompilationContext()
    {
        delete errorHandler;
    }

    void CompilationContext::initializeFFI()
    {
        StdLib::registerFunctions(*cppFFI);
        // Additional FFI initialization can be added here
    }

    void CompilationContext::initializeTypes()
    {
        typeMap["int"] = llvm::Type::getInt32Ty(*context);
        typeMap["float"] = llvm::Type::getDoubleTy(*context);
        typeMap["double"] = llvm::Type::getDoubleTy(*context);
        typeMap["string"] = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context));
        typeMap["bool"] = llvm::Type::getInt1Ty(*context);
        typeMap["List"] = getListType();
        typeMap["Dict"] = getDictType();
    }

    llvm::Type *CompilationContext::getLLVMType(const ast::TypePtr &type)
    {
        if (!type)
            return nullptr;
        std::string typeName = type->toString();
        auto it = typeMap.find(typeName);
        if (it != typeMap.end())
            return it->second;
        if (auto *generic = dynamic_cast<ast::GenericType *>(type.get()))
        {
            if (generic->name == "list")
                return getListType();
            if (generic->name == "dict")
                return getDictType();
        }
        return nullptr;
    }

    llvm::StructType *CompilationContext::getListType()
    {
        if (!listType)
        {
            // Define list as a struct with a pointer to data, a length, and a capacity
            listType = llvm::StructType::create(*context, "List");

            // Create an array of the types for the struct fields
            llvm::Type *elementTypes[] = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                llvm::Type::getInt64Ty(*context),
                llvm::Type::getInt64Ty(*context)};

            // Pass the array as a single ArrayRef argument
            listType->setBody(elementTypes);
        }
        return listType;
    }

    llvm::StructType *CompilationContext::getDictType()
    {
        if (!dictType)
        {
            // Define dict as a struct with a pointer to entries, a count, and a capacity
            dictType = llvm::StructType::create(*context, "Dict");

            // Create an array of the types for the struct fields
            llvm::Type *elementTypes[] = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                llvm::Type::getInt64Ty(*context),
                llvm::Type::getInt64Ty(*context)};

            // Pass the array as a single ArrayRef argument
            dictType->setBody(elementTypes);
        }
        return dictType;
    }

    llvm::StructType *CompilationContext::getStringType()
    {
        if (!stringType)
        {
            // Define string as a struct with a pointer to char and a length
            stringType = llvm::StructType::create(*context, "String");

            // Create an array of the types for the struct fields
            llvm::Type *elementTypes[] = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                llvm::Type::getInt64Ty(*context)};

            // Pass the array as a single ArrayRef argument
            stringType->setBody(elementTypes);
        }
        return stringType;
    }

    // Module path management

    void CompilationContext::addModulePath(const std::string &path)
    {
        modulePaths.push_back(path);
    }

    std::vector<std::string> CompilationContext::getModulePaths() const
    {
        return modulePaths;
    }

    // Module management

    std::shared_ptr<ModuleInfo> CompilationContext::getModule(const std::string &name)
    {
        auto it = modules.find(name);
        if (it != modules.end())
        {
            return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<ModuleInfo> CompilationContext::loadModule(const std::string &name)
    {
        // Check if already loaded
        auto existingModule = getModule(name);
        if (existingModule)
        {
            return existingModule;
        }

        // Find the module file
        std::string filePath = findModuleFile(name);
        if (filePath.empty())
        {
            return nullptr;
        }

        // Create a new module
        auto moduleInfo = std::make_shared<ModuleInfo>(name, filePath);
        modules[name] = moduleInfo;

        return moduleInfo;
    }

    bool CompilationContext::moduleExists(const std::string &name) const
    {
        return modules.find(name) != modules.end();
    }

    void CompilationContext::addModule(const std::string &name, std::shared_ptr<ModuleInfo> module)
    {
        modules[name] = module;
    }

    // Check for circular dependencies

    bool CompilationContext::hasCircularDependency(const std::string &moduleName,
                                                   std::vector<std::string> &path) const
    {
        // If we encounter this module again in the path, there's a circular dependency
        if (std::find(path.begin(), path.end(), moduleName) != path.end())
        {
            path.push_back(moduleName); // Add it to complete the cycle for reporting
            return true;
        }

        auto it = modules.find(moduleName);
        if (it == modules.end())
        {
            return false; // Module not found, no circular dependency
        }

        // Add this module to the path
        path.push_back(moduleName);

        // Check all dependencies
        for (const auto &dep : it->second->dependencies)
        {
            if (hasCircularDependency(dep, path))
            {
                return true;
            }
        }

        // Remove this module from the path before returning
        path.pop_back();
        return false;
    }

    // Symbol management

    void CompilationContext::addGlobalSymbol(const std::string &name, bool exported)
    {
        globalSymbols.insert(name);
        if (exported)
        {
            exportedSymbols.insert(name);
        }
    }

    bool CompilationContext::symbolExists(const std::string &name) const
    {
        return globalSymbols.count(name) > 0;
    }

    // Import symbol from a module

    bool CompilationContext::importSymbol(const std::string &moduleName, const std::string &symbolName)
    {
        auto moduleInfo = getModule(moduleName);
        if (!moduleInfo)
        {
            return false;
        }

        // Check if the symbol is exported by the module
        if (!moduleInfo->isExported(symbolName))
        {
            return false;
        }

        // Add the symbol to global symbols
        addGlobalSymbol(symbolName);
        return true;
    }

    bool CompilationContext::importAllSymbols(const std::string &moduleName)
    {
        auto moduleInfo = getModule(moduleName);
        if (!moduleInfo)
        {
            return false;
        }

        // Import all exported functions
        for (const auto &func : moduleInfo->exportedFunctions)
        {
            addGlobalSymbol(func);
        }

        // Import all exported classes
        for (const auto &cls : moduleInfo->exportedClasses)
        {
            addGlobalSymbol(cls);
        }

        // Import all exported variables
        for (const auto &var : moduleInfo->exportedVariables)
        {
            addGlobalSymbol(var);
        }

        // Import all exported types
        for (const auto &type : moduleInfo->exportedTypes)
        {
            addGlobalSymbol(type);
        }

        return true;
    }

    // Get a qualified name (module::symbol)

    std::string CompilationContext::getQualifiedName(const std::string &moduleName,
                                                     const std::string &symbolName) const
    {
        return moduleName + "::" + symbolName;
    }

    // Find module file

    std::string CompilationContext::findModuleFile(const std::string &moduleName) const
    {
        // Replace dots with path separators for hierarchical modules
        std::string moduleRelPath = moduleName;
        std::replace(moduleRelPath.begin(), moduleRelPath.end(), '.', '/');

        // Add file extension
        moduleRelPath += ".to";

        // Check all module paths
        for (const auto &path : modulePaths)
        {
            std::filesystem::path fullPath = std::filesystem::path(path) / moduleRelPath;
            if (std::filesystem::exists(fullPath))
            {
                return fullPath.string();
            }
        }

        return ""; // Module not found
    }

    // Get module source

    std::string CompilationContext::getModuleSource(const std::string &moduleName) const
    {
        // Find the module
        auto it = modules.find(moduleName);
        if (it == modules.end())
        {
            return "";
        }

        // Get the file path
        const std::string &filePath = it->second->path;

        // Read the file
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            return "";
        }

        // Read the entire file into a string
        std::string source;
        file.seekg(0, std::ios::end);
        source.reserve(file.tellg());
        file.seekg(0, std::ios::beg);
        source.assign((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

        return source;
    }

} // namespace compiler
