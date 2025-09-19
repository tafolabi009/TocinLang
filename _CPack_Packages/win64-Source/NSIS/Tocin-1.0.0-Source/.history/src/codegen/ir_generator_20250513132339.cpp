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

// Initialize the static type registry
std::map<llvm::Value*, llvm::Type*> IRGenerator::typeRegistry;

// Implement the registerType method
void IRGenerator::registerType(llvm::Value* value, llvm::Type* type) {
    typeRegistry[value] = type;
}

// Implement the getRegisteredType method
llvm::Type* IRGenerator::getRegisteredType(llvm::Value* value) {
    auto it = typeRegistry.find(value);
    if (it != typeRegistry.end()) {
        return it->second;
    }
    return nullptr;
}

// Implement createLoad for opaque pointers
llvm::Value* IRGenerator::createLoad(llvm::Value* ptr) {
    // Get the type from our registry or try to infer it
    llvm::Type* pointedType = getRegisteredType(ptr);
    
    if (!pointedType) {
        // If we don't have it registered, try to infer it from context
        // This is a simplified fallback - in a real implementation, you'd want more sophisticated type tracking
        pointedType = builder.getInt8Ty();
        
        // Try to warn about missing type information
        std::string valueName = ptr->hasName() ? ptr->getName().str() : "unnamed";
        std::cerr << "Warning: Missing type information for pointer " << valueName 
                 << " when creating load instruction. Using default type." << std::endl;
    }
    
    // Create the load instruction with the specified type
    return builder.CreateLoad(pointedType, ptr);
}

// Implement createStore for opaque pointers
llvm::Value* IRGenerator::createStore(llvm::Value* value, llvm::Value* ptr) {
    // With opaque pointers, we don't need the type for the store instruction
    // The value being stored already has type information
    return builder.CreateStore(value, ptr);
}

// Implement createGEP for opaque pointers
llvm::Value* IRGenerator::createGEP(llvm::Value* ptr, std::vector<llvm::Value*> indices, 
                                   llvm::Type* elementType, const std::string& name) {
    // With opaque pointers, we need to pass the element type explicitly
    // This is different from the old approach where the element type was part of the pointer type
    
    if (!elementType) {
        // If no element type provided, try to get it from registry
        elementType = getRegisteredType(ptr);
        
        if (!elementType) {
            // If still no type, default to i8
            elementType = builder.getInt8Ty();
            std::cerr << "Warning: Missing element type for GEP instruction. Using default type." << std::endl;
        }
    }
    
    // Create GEP instruction with explicit type
    auto gep = builder.CreateGEP(elementType, ptr, indices, name);
    
    // Register the result type as well if we have element type information
    if (elementType) {
        registerType(gep, elementType);
    }
    
    return gep;
}

// For handling object field access with opaque pointers
llvm::Value* IRGenerator::visitGetExpr(GetExpr* expr) {
    // Generate code for object expression
    llvm::Value* object = generateExpression(expr->object);
    if (!object) return nullptr;
    
    // Get the pointed type using opaque pointer handling
    llvm::Type* pointedType = getRegisteredType(object);
    if (!pointedType) {
        // If type isn't in the registry, try to infer it from context
        if (auto structType = llvm::dyn_cast<llvm::StructType>(object->getType())) {
            pointedType = structType;
        } else {
            // Default to a generic pointer type if we can't determine it
            pointedType = builder.getInt8Ty();
            errorHandler.error("Type information lost for object expression. Using default type.");
        }
    }
    
    // Now we can proceed with field access using the known type
    // This might involve looking up the field index and using createGEP
    
    // Get the field name
    const std::string& fieldName = expr->name;
    
    // Example field access code (simplified)
    // In a real implementation, you'd use class/struct metadata to find the field index
    int fieldIndex = 0; // This would be determined based on the type and field name
    
    // Create a GEP to access the field
    auto indexList = std::vector<llvm::Value*> {
        builder.getInt32(0),      // first index is always 0 for struct access
        builder.getInt32(fieldIndex)  // field index
    };
    
    // Get field pointer using our helper
    llvm::Value* fieldPtr = createGEP(object, indexList, pointedType, fieldName + "_ptr");
    
    // Load the field value using our helper
    llvm::Value* fieldValue = createLoad(fieldPtr);
    
    // Return the loaded value
    return fieldValue;
}

// For handling object field setting with opaque pointers
llvm::Value* IRGenerator::visitSetExpr(SetExpr* expr) {
    // Generate code for object expression
    llvm::Value* object = generateExpression(expr->object);
    if (!object) return nullptr;
    
    // Generate code for value expression
    llvm::Value* value = generateExpression(expr->value);
    if (!value) return nullptr;
    
    // Get the pointed type using our type registry
    llvm::Type* pointedType = getRegisteredType(object);
    if (!pointedType) {
        // If type isn't in the registry, try to infer it from context
        if (auto structType = llvm::dyn_cast<llvm::StructType>(object->getType())) {
            pointedType = structType;
        } else {
            // Default to a generic pointer type if we can't determine it
            pointedType = builder.getInt8Ty();
            errorHandler.error("Type information lost for object in set expression. Using default type.");
        }
    }
    
    // Get the field name
    const std::string& fieldName = expr->name;
    
    // Example field access code (simplified)
    // In a real implementation, you'd use class/struct metadata to find the field index
    int fieldIndex = 0; // This would be determined based on the type and field name
    
    // Create a GEP to access the field
    auto indexList = std::vector<llvm::Value*> {
        builder.getInt32(0),      // first index is always 0 for struct access
        builder.getInt32(fieldIndex)  // field index
    };
    
    // Get field pointer using our helper
    llvm::Value* fieldPtr = createGEP(object, indexList, pointedType, fieldName + "_ptr");
    
    // Store the value using our helper
    createStore(value, fieldPtr);
    
    // Return the set value
    return value;
}

// For handling pointer dereferencing operations with opaque pointers
llvm::Value* IRGenerator::visitPointerDereference(llvm::Value* pointerValue) {
    // Check if we have the value in our type registry
    llvm::Type* pointedType = getRegisteredType(pointerValue);
    if (!pointedType) {
        // If not in registry, we need to infer it
        // This is a simplified approach - in practice, you'd want better type tracking
        pointedType = builder.getInt8Ty();
        errorHandler.error("Type information lost for pointer dereference. Using default type.");
    }
    
    // Create load instruction with the determined type
    return createLoad(pointerValue);
}

// For handling array indexing with opaque pointers
llvm::Value* IRGenerator::createArrayIndexGEP(llvm::Value* arrayPtr, llvm::Value* index, llvm::Type* elementType) {
    // Create a GEP instruction for array indexing
    // With opaque pointers, we need to provide the element type explicitly
    if (!elementType) {
        elementType = getRegisteredType(arrayPtr);
        if (!elementType) {
            elementType = builder.getInt8Ty();
            errorHandler.error("Type information lost for array indexing. Using default type.");
        }
    }
    
    // For arrays, we use a different GEP pattern
    auto indexList = std::vector<llvm::Value*> {
        builder.getInt32(0),  // First index is always 0
        index                 // Second index is the dynamic array index
    };
    
    // Create the GEP instruction
    auto result = createGEP(arrayPtr, indexList, elementType, "arrayidx");
    
    // Return the pointer to the array element
    return result;
}

// Method to generate instructions for malloc with opaque pointers
llvm::Value* IRGenerator::createMalloc(llvm::Type* allocType, llvm::Value* numElements) {
    // With opaque pointers, malloc just returns i8* which can point to any type
    // We need to keep track of the allocated type ourselves
    
    // Calculate the size of the allocation
    llvm::Value* allocSize = builder.CreateMul(
        builder.CreateIntCast(numElements, builder.getInt64Ty(), false),
        builder.getInt64(context.getTypeSize(allocType)),
        "allocsize"
    );
    
    // Get the malloc function from standard library
    llvm::Function* mallocFunc = getStdLibFunction("malloc");
    if (!mallocFunc) {
        errorHandler.error("Failed to find malloc function");
        return nullptr;
    }
    
    // Call malloc function
    llvm::Value* rawPtr = builder.CreateCall(mallocFunc, {allocSize}, "rawmalloc");
    
    // Register the allocated memory with its type
    registerType(rawPtr, allocType);
    
    // Return the allocated pointer
    return rawPtr;
}

// Method for load operations from virtual registers
llvm::Value* IRGenerator::generateLoadFromVariable(const std::string& name) {
    // Look up the variable in the current scope
    llvm::AllocaInst* varAlloca = currentScope ? currentScope->lookup(name) : nullptr;
    if (!varAlloca) {
        errorHandler.error("Variable '" + name + "' not found");
        return nullptr;
    }
    
    // Get the type of the variable from our registry
    llvm::Type* varType = getRegisteredType(varAlloca);
    if (!varType) {
        // If not in registry, try to get it from alloca instruction
        varType = varAlloca->getAllocatedType();
        if (!varType) {
            // If still no type, use a default
            varType = builder.getInt8Ty();
            errorHandler.error("Type information lost for variable '" + name + "'. Using default type.");
        }
        
        // Register it for future use
        registerType(varAlloca, varType);
    }
    
    // Create load instruction
    return createLoad(varAlloca);
}

// Rest of your IR generator implementation
// ...

} // namespace codegen 
