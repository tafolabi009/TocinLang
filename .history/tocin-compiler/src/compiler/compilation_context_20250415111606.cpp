#include "compilation_context.h"
#include "compiler/stdlib.h"
#include <llvm/IR/Verifier.h>

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
        ffi(pythonFFI.get()) {
        initializeTypes();
        initializeFFI();
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
        std::vector<llvm::Type*> elements = {
            llvm::Type::getInt32Ty(*context), // size
            llvm::PointerType::getUnqual(llvm::Type::getInt32Ty(*context)) // data
        };
        return llvm::StructType::create(*context, elements, "List");
    }

    llvm::StructType* CompilationContext::getDictType() {
        std::vector<llvm::Type*> elements = {
            llvm::Type::getInt32Ty(*context), // size
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context)), // keys
            llvm::PointerType::getUnqual(llvm::Type::getInt32Ty(*context)) // values
        };
        return llvm::StructType::create(*context, elements, "Dict");
    }

} // namespace compiler