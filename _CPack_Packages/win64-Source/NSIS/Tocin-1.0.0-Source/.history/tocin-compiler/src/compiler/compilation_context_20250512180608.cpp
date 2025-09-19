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

namespace compiler {

    CompilationContext::CompilationContext(const std::string& filename)
        : context(std::make_unique<llvm::LLVMContext>()),
        module(std::make_unique<llvm::Module>("tocin_module", *context)),
        builder(std::make_unique<llvm::IRBuilder<>>(*context)),
        errorHandler(new ErrorHandler(filename)),
        pythonFFI(std::make_unique<ffi::PythonFFI>()),
        cppFFI(std::make_unique<ffi::CppFFI>()),
        jsFFI(std::make_unique<ffi::JavaScriptFFI>()),
        ffi(pythonFFI.get()),
        currentFilename(filename) {
        initializeTypes();
        initializeFFI();
        
        // Initialize predefined module paths
        modulePaths.push_back("./modules");
        modulePaths.push_back("./src/modules");
        
        // Add current directory
        std::filesystem::path filePath(filename);
        if (filePath.has_parent_path()) {
            modulePaths.push_back(filePath.parent_path().string());
        }
    }

    CompilationContext::~CompilationContext() {
        delete errorHandler;
    }

    void CompilationContext::initializeFFI() {
        StdLib::registerFunctions(*cppFFI);
        // Additional FFI initialization can be added here
    }

    void CompilationContext::initializeTypes() {
        typeMap["int"] = llvm::Type::getInt32Ty(*context);
        typeMap["float"] = llvm::Type::getDoubleTy(*context);
        typeMap["double"] = llvm::Type::getDoubleTy(*context);
        typeMap["string"] = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context));
        typeMap["bool"] = llvm::Type::getInt1Ty(*context);
        typeMap["List"] = getListType();
        typeMap["Dict"] = getDictType();
    }

    llvm::Type* CompilationContext::getLLVMType(const ast::TypePtr& type) {
        if (!type) return nullptr;
        std::string typeName = type->toString();
        auto it = typeMap.find(typeName);
        if (it != typeMap.end()) return it->second;
        if (auto* generic = dynamic_cast<ast::GenericType*>(type.get())) {
            if (generic->name == "list") return getListType();
            if (generic->name == "dict") return getDictType();
        }
        return nullptr;
    }

    llvm::StructType* CompilationContext::getListType() {
        if (!listType) {
            // Define list as a struct with a pointer to data, a length, and a capacity
            listType = llvm::StructType::create(*context, "List");
            listType->setBody(
                llvm::PointerType::get(llvm::Type::getInt8Ty(*context), 0),
                llvm::Type::getInt64Ty(*context),
                llvm::Type::getInt64Ty(*context));
        }
        return listType;
    }

    llvm::StructType* CompilationContext::getDictType() {
        if (!dictType) {
            // Define dict as a struct with a pointer to entries, a count, and a capacity
            dictType = llvm::StructType::create(*context, "Dict");
        std::vector<llvm::Type*> elements = {
            llvm::Type::getInt32Ty(*context), // size
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context)), // keys
            llvm::PointerType::getUnqual(llvm::Type::getInt32Ty(*context)) // values
        };
        return llvm::StructType::create(*context, elements, "Dict");
    }

} // namespace compiler
