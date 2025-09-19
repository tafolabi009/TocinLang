#include "../pch.h"
#include "ir_generator.h"
#include "../ast/ast.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/Host.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace codegen {

using namespace ast;
using namespace error;

// IR Generator implementation for Tocin language
// Following methods handle opaque pointer operations instead of using getPointerElementType()

// For handling object field access
llvm::Value* IRGenerator::visitGetExpr(GetExpr* expr) {
    // Generate code for object expression
    llvm::Value* object = generateExpression(expr->object);
    if (!object) return nullptr;
    
    // Get the pointed type using opaque pointer handling
    llvm::Type* pointedType = nullptr;
    // Instead of: pointedType = ptrType->getPointerElementType();
    
    // With opaque pointers, we need to track types in our code, not rely on pointer types
    // We can use a type registry, cast operations, or context-aware type tracking
    pointedType = typeRegistry[object];
    if (!pointedType) {
        // If type isn't in the registry, we can try to infer it from context
        if (auto structType = llvm::dyn_cast<llvm::StructType>(object->getType())) {
            pointedType = structType;
        } else {
            // Default to a generic pointer type if we can't determine it
            pointedType = builder.getInt8Ty();
        }
    }
    
    // Similarly for other uses of getPointerElementType
    // Replace:
    // fieldType = ptrType->getPointerElementType();
    // With context-aware type tracking
    
    // For CreateLoad operations that used getPointerElementType:
    // Instead of: base = builder.CreateLoad(ptrType->getPointerElementType(), basePtr);
    // Use the tracked type or specify the type explicitly:
    llvm::Value* basePtr = nullptr; // This would be computed earlier in your code
    llvm::Value* base = builder.CreateLoad(pointedType, basePtr);
    
    // Rest of your implementation
    return base;
}

// Additional helper for opaque pointer handling
void IRGenerator::registerType(llvm::Value* value, llvm::Type* type) {
    typeRegistry[value] = type;
}

// Map to track values and their associated types
std::map<llvm::Value*, llvm::Type*> IRGenerator::typeRegistry;

// Rest of your IR generator implementation
// ...

} // namespace codegen 
