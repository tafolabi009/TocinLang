#include "ir_generator.h"
#include "../ast/ast.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"
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
#include <iostream>
#include <vector>

using namespace codegen;

IRGenerator::IRGenerator(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> module,
                         error::ErrorHandler &errorHandler)
    : context(context), module(std::move(module)), builder(context),
      errorHandler(errorHandler), lastValue(nullptr), typeChecker(),
      isInAsyncContext(false), currentModuleName("default")
{
    // Create the root scope
    currentScope = new Scope(nullptr);

    // Declare standard library functions
    declareStdLibFunctions();
}

IRGenerator::~IRGenerator()
{
    // Clean up scopes
    while (currentScope)
    {
        Scope *parent = currentScope->parent;
        delete currentScope;
        currentScope = parent;
    }
}

// Environment management
void IRGenerator::createEnvironment()
{
    // Save the current environment before entering a new scope
    enterScope();
}

void IRGenerator::restoreEnvironment()
{
    // Restore the environment after exiting a scope
    exitScope();
}

// Create an allocation instruction in the entry block for a local variable
llvm::AllocaInst *IRGenerator::createEntryBlockAlloca(llvm::Function *function,
                                                      const std::string &name,
                                                      llvm::Type *type)
{
    if (!function)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Cannot create allocation outside of function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return nullptr;
    }

    // Get the entry block
    llvm::BasicBlock &entryBlock = function->getEntryBlock();

    // Create an IRBuilder positioned at the beginning of the entry block
    llvm::IRBuilder<> tempBuilder(&entryBlock, entryBlock.begin());

    // Create the alloca instruction
    return tempBuilder.CreateAlloca(type, nullptr, name);
}

// Declare standard library functions that can be called from Tocin code
void IRGenerator::declareStdLibFunctions()
{
    // Print function for debugging
    llvm::FunctionType *printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
        true);
    llvm::Function *printfFunc = llvm::Function::Create(
        printfType, llvm::Function::ExternalLinkage, "printf", *module);
    stdLibFunctions["printf"] = printfFunc;

    // Memory management functions
    llvm::FunctionType *mallocType = llvm::FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
        {llvm::Type::getInt64Ty(context)},
        false);
    llvm::Function *mallocFunc = llvm::Function::Create(
        mallocType, llvm::Function::ExternalLinkage, "malloc", *module);
    stdLibFunctions["malloc"] = mallocFunc;

    llvm::FunctionType *freeType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
        false);
    llvm::Function *freeFunc = llvm::Function::Create(
        freeType, llvm::Function::ExternalLinkage, "free", *module);
    stdLibFunctions["free"] = freeFunc;

    // Future/Promise functions for async/await
    // These would be implemented in the runtime
    // For now, just declare the interfaces

    // Example: Promise_create
    llvm::FunctionType *promiseCreateType = llvm::FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), // Opaque promise pointer
        {},
        false);
    llvm::Function *promiseCreateFunc = llvm::Function::Create(
        promiseCreateType, llvm::Function::ExternalLinkage, "Promise_create", *module);
    stdLibFunctions["Promise_create"] = promiseCreateFunc;

    // Example: Promise_getFuture
    llvm::FunctionType *promiseGetFutureType = llvm::FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),   // Opaque future pointer
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)}, // Promise pointer
        false);
    llvm::Function *promiseGetFutureFunc = llvm::Function::Create(
        promiseGetFutureType, llvm::Function::ExternalLinkage, "Promise_getFuture", *module);
    stdLibFunctions["Promise_getFuture"] = promiseGetFutureFunc;

    // Example: Future_get
    llvm::FunctionType *futureGetType = llvm::FunctionType::get(
        llvm::Type::getInt8Ty(context),                              // Generic return type, will be cast
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)}, // Future pointer
        false);
    llvm::Function *futureGetFunc = llvm::Function::Create(
        futureGetType, llvm::Function::ExternalLinkage, "Future_get", *module);
    stdLibFunctions["Future_get"] = futureGetFunc;

    // Create a basic main function to make the module valid
    createMainFunction();

    // Declare a print function for debugging
    declarePrintFunction();
}

// Get a standard library function by name
llvm::Function *IRGenerator::getStdLibFunction(const std::string &name)
{
    auto it = stdLibFunctions.find(name);
    if (it != stdLibFunctions.end())
    {
        return it->second;
    }
    return nullptr;
}

// Convert Tocin type to LLVM type
llvm::Type *IRGenerator::getLLVMType(ast::TypePtr type)
{
    if (!type)
    {
        return llvm::Type::getVoidTy(context);
    }

    // Handle basic types
    if (auto basicType = std::dynamic_pointer_cast<ast::TypeReference>(type))
    {
        std::string typeName = basicType->getName();

        if (typeName == "int")
        {
            return llvm::Type::getInt64Ty(context);
        }
        else if (typeName == "float")
        {
            return llvm::Type::getDoubleTy(context);
        }
        else if (typeName == "bool")
        {
            return llvm::Type::getInt1Ty(context);
        }
        else if (typeName == "string")
        {
            return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        }
        else if (typeName == "void")
        {
            return llvm::Type::getVoidTy(context);
        }
        else
        {
            // Check if it's a class/struct type
            auto it = classTypes.find(typeName);
            if (it != classTypes.end())
            {
                return llvm::PointerType::get(it->second.classType, 0);
            }

            // Could be an enum or other user-defined type
            // For now, return a generic pointer
            return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        }
    }

    // Handle generic types
    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
    {
        std::string baseName = genericType->name;
        const auto &typeArgs = genericType->typeArguments;

        if (baseName == "list")
        {
            // list<T> is represented as { int64 length, T* data }
            if (!typeArgs.empty())
            {
                llvm::Type *elementType = getLLVMType(typeArgs[0]);
                std::vector<llvm::Type *> fields = {
                    llvm::Type::getInt64Ty(context),
                    llvm::PointerType::get(elementType, 0)};

                // Create or get a struct type for this list
                std::string mangledName = mangleGenericName("list", typeArgs);
                llvm::StructType *listType = llvm::StructType::getTypeByName(context, mangledName);
                if (!listType)
                {
                    listType = llvm::StructType::create(context, fields, mangledName);
                }

                return listType;
            }
        }
        else if (baseName == "dict")
        {
            // dict<K,V> is represented as { int64 size, K* keys, V* values }
            if (typeArgs.size() >= 2)
            {
                llvm::Type *keyType = getLLVMType(typeArgs[0]);
                llvm::Type *valueType = getLLVMType(typeArgs[1]);

                std::vector<llvm::Type *> fields = {
                    llvm::Type::getInt64Ty(context),
                    llvm::PointerType::get(keyType, 0),
                    llvm::PointerType::get(valueType, 0)};

                // Create or get a struct type for this dictionary
                std::string mangledName = mangleGenericName("dict", typeArgs);
                llvm::StructType *dictType = llvm::StructType::getTypeByName(context, mangledName);
                if (!dictType)
                {
                    dictType = llvm::StructType::create(context, fields, mangledName);
                }

                return dictType;
            }
        }
        else
        {
            // Other generic types - instantiate if possible
            return instantiateGenericType(baseName, typeArgs);
        }
    }

    // Handle function types
    if (auto funcType = std::dynamic_pointer_cast<ast::FunctionType>(type))
    {
        std::vector<llvm::Type *> paramTypes;
        for (const auto &paramType : funcType->paramTypes)
        {
            paramTypes.push_back(getLLVMType(paramType));
        }

        llvm::Type *returnType = getLLVMType(funcType->returnType);

        llvm::FunctionType *functionType = llvm::FunctionType::get(
            returnType, paramTypes, false);

        return llvm::PointerType::get(functionType, 0);
    }

    // For other types, use a generic pointer
    return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
}

// Create an LLVM function type from Tocin types
llvm::FunctionType *IRGenerator::getLLVMFunctionType(ast::TypePtr returnType,
                                                     const std::vector<ast::Parameter> &params)
{
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : params)
    {
        paramTypes.push_back(getLLVMType(param.type));
    }

    llvm::Type *llvmReturnType = getLLVMType(returnType);

    return llvm::FunctionType::get(llvmReturnType, paramTypes, false);
}

// Create a basic main function to make the module valid
void IRGenerator::createMainFunction()
{
    // Define a simple main function as the entry point
    llvm::FunctionType *mainFuncType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context),                                                        // return type: int
        {llvm::Type::getInt32Ty(context),                                                       // argc
         llvm::PointerType::get(llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), 0)}, // argv
        false);

    llvm::Function *mainFunction = llvm::Function::Create(
        mainFuncType,
        llvm::Function::ExternalLinkage,
        "main",
        *module);

    // Create entry block
    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", mainFunction);
    builder.SetInsertPoint(block);

    // Just return 0
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));

    // Set as current function
    currentFunction = mainFunction;
}

// Declare a print function for debugging
void IRGenerator::declarePrintFunction()
{
    // Declare printf from C standard library
    llvm::FunctionType *printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
        true);

    llvm::Function *printfFunc = llvm::Function::Create(
        printfType,
        llvm::Function::ExternalLinkage,
        "printf",
        *module);

    // Also add a simple print wrapper function
    llvm::FunctionType *printType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
        false);

    llvm::Function *printFunc = llvm::Function::Create(
        printType,
        llvm::Function::ExternalLinkage,
        "print",
        *module);

    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", printFunc);
    builder.SetInsertPoint(block);

    // Get the string argument
    llvm::Argument *str = printFunc->arg_begin();
    str->setName("str");

    // Call printf with the string
    builder.CreateCall(printfFunc, {str});

    // Return void
    builder.CreateRetVoid();

    // Add to standard library functions
    stdLibFunctions["print"] = printFunc;
}

// Add PatternVisitor implementation (corrected to use proper member variable names)
bool PatternVisitor::visitWildcardPattern(ast::WildcardPattern *pattern,
                                          llvm::BasicBlock *successBlock,
                                          llvm::BasicBlock *failBlock)
{
    // Wildcard pattern always matches
    llvm::IRBuilder<> &builder = generator.builder;
    builder.CreateBr(successBlock);
    bindingSuccess = true;
    return true;
}

// The rest of the PatternVisitor implementation methods...
// ... existing code ...
