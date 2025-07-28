#include "ir_generator.h"
#include "../ast/ast.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"
#include "../compiler/compilation_context.h"
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
      errorHandler(errorHandler), lastValue(nullptr),
      isInAsyncContext(false), currentModuleName("default")
{
    // Create the root scope
    currentScope = new Scope(nullptr);

    // Declare standard library functions
    declareStdLibFunctions();

    // Create a basic main function to make the module valid
    createMainFunction();

    // Declare a print function for debugging
    declarePrintFunction();
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
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
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
    if (auto basicType = std::dynamic_pointer_cast<ast::BasicType>(type))
    {
        auto kind = basicType->getKind();

        if (kind == ast::TypeKind::INT)
        {
            return llvm::Type::getInt64Ty(context);
        }
        else if (kind == ast::TypeKind::FLOAT)
        {
            return llvm::Type::getDoubleTy(context);
        }
        else if (kind == ast::TypeKind::BOOL)
        {
            return llvm::Type::getInt1Ty(context);
        }
        else if (kind == ast::TypeKind::STRING)
        {
            // Use opaque pointer for string (char*)
            return llvm::PointerType::get(context, 0);
        }
        else if (kind == ast::TypeKind::VOID)
        {
            return llvm::Type::getVoidTy(context);
        }
        else
        {
            // For other basic types, use a generic opaque pointer
            return llvm::PointerType::get(context, 0);
        }
    }

    // Handle simple named types
    if (auto simpleType = std::dynamic_pointer_cast<ast::SimpleType>(type))
    {
        std::string typeName = simpleType->toString();

        // Check if it's a class/struct type
        auto it = classTypes.find(typeName);
        if (it != classTypes.end())
        {
            // Return an opaque pointer for the class
            return llvm::PointerType::get(context, 0);
        }

        // Could be an enum or other user-defined type
        // For now, return a generic opaque pointer
        return llvm::PointerType::get(context, 0);
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
                    llvm::PointerType::get(context, 0) // opaque pointer for data array
                };

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
                std::vector<llvm::Type *> fields = {
                    llvm::Type::getInt64Ty(context),
                    llvm::PointerType::get(context, 0), // opaque pointer for keys
                    llvm::PointerType::get(context, 0)  // opaque pointer for values
                };

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
    }

    // If all else fails, return a void type
    return llvm::Type::getVoidTy(context);
}

void IRGenerator::visitLiteralExpr(ast::LiteralExpr *expr)
{
    // Use the literalType field to determine what kind of literal we have
    switch (expr->literalType)
    {
    case ast::LiteralExpr::LiteralType::INTEGER:
    {
        // Convert string to int64
        int64_t value = std::stoll(expr->value);
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), value);
        break;
    }
    case ast::LiteralExpr::LiteralType::FLOAT:
    {
        // Convert string to double
        double value = std::stod(expr->value);
        lastValue = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context), value);
        break;
    }
    case ast::LiteralExpr::LiteralType::STRING:
    {
        // Handle a string literal
        std::string str = expr->value;

        // Remove quotes if they exist
        if (str.size() >= 2 && (str[0] == '"' || str[0] == '\'') &&
            (str.back() == '"' || str.back() == '\''))
        {
            str = str.substr(1, str.size() - 2);
        }

        // Handle escape sequences
        std::string processedStr;
        for (size_t i = 0; i < str.size(); ++i)
        {
            if (str[i] == '\\' && i + 1 < str.size())
            {
                switch (str[i + 1])
                {
                case 'n':
                    processedStr += '\n';
                    break;
                case 't':
                    processedStr += '\t';
                    break;
                case 'r':
                    processedStr += '\r';
                    break;
                case '\\':
                    processedStr += '\\';
                    break;
                case '\"':
                    processedStr += '\"';
                    break;
                case '\'':
                    processedStr += '\'';
                    break;
                default:
                    processedStr += str[i];
                    processedStr += str[i + 1];
                }
                ++i; // Skip the next character
            }
            else
            {
                processedStr += str[i];
            }
        }

        // Create a global string with null terminator
        lastValue = builder.CreateGlobalString(processedStr, "str");
        break;
    }
    case ast::LiteralExpr::LiteralType::BOOLEAN:
    {
        // Boolean value is directly stored in the value field as "true" or "false"
        bool boolValue = (expr->value == "true");
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), boolValue ? 1 : 0);
        break;
    }
    case ast::LiteralExpr::LiteralType::NIL:
    {
        // Nil is represented as a null pointer (use opaque pointer)
        lastValue = llvm::ConstantPointerNull::get(
            llvm::PointerType::get(context, 0));
        break;
    }
    default:
        errorHandler.reportError(error::ErrorCode::C031_TYPECHECK_ERROR,
                                 "Unsupported literal type: " + expr->value,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }
}

void IRGenerator::visitVariableStmt(ast::VariableStmt *stmt)
{
    // Get the variable type
    llvm::Type *varType = nullptr;

    if (stmt->type)
    {
        // If type is explicitly specified
        varType = getLLVMType(stmt->type);
    }
    else if (stmt->initializer)
    {
        // If type is inferred from initializer
        stmt->initializer->accept(*this);
        if (!lastValue)
            return;

        varType = lastValue->getType();
    }
    else
    {
        // No type and no initializer - error
        errorHandler.reportError(error::ErrorCode::T032_CANNOT_INFER_TYPE,
                                 "Cannot infer type for variable '" + stmt->name + "' without initializer",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    if (!varType)
    {
        errorHandler.reportError(error::ErrorCode::T031_UNDEFINED_TYPE,
                                 "Unknown type for variable '" + stmt->name + "'",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    // Create an alloca instruction in the entry block of the current function
    llvm::AllocaInst *alloca = createEntryBlockAlloca(currentFunction, stmt->name, varType);

    // Store the variable in the symbol table
    namedValues[stmt->name] = alloca;

    // If there's an initializer, store its value
    if (stmt->initializer)
    {
        if (!lastValue)
        {
            // If we don't have a value yet, compute the initializer
            stmt->initializer->accept(*this);
            if (!lastValue)
                return;
        }

        // Validate that initializer type matches variable type
        if (lastValue->getType() != varType)
        {
            // Simple cast for numeric values
            if (lastValue->getType()->isIntegerTy() && varType->isIntegerTy())
            {
                lastValue = builder.CreateIntCast(lastValue, varType, true, "cast");
            }
            else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                     (varType->isFloatTy() || varType->isDoubleTy()))
            {
                lastValue = builder.CreateFPCast(lastValue, varType, "cast");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Initializer type does not match variable type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return;
            }
        }

        // Store the initial value
        builder.CreateStore(lastValue, alloca);
    }
}

void IRGenerator::visitFunctionStmt(ast::FunctionStmt *stmt)
{
    // Handle async functions
    if (stmt->isAsync)
    {
        // Transform the async function
        llvm::Function *asyncFunc = transformAsyncFunction(stmt);

        // If transformation failed, return
        if (!asyncFunc)
        {
            return;
        }

        // Also create a regular wrapper function that awaits the async result
        std::string regularFuncName = stmt->name;

        // Create the function signature
        std::vector<llvm::Type *> paramTypes;
        for (const auto &param : stmt->parameters)
        {
            llvm::Type *paramType = getLLVMType(param.type);
            if (!paramType)
            {
                return;
            }
            paramTypes.push_back(paramType);
        }

        llvm::Type *returnType = getLLVMType(stmt->returnType);
        if (!returnType)
        {
            return;
        }

        llvm::FunctionType *funcType = llvm::FunctionType::get(
            returnType, paramTypes, false);

        // Create the function
        llvm::Function *function = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            regularFuncName,
            *module);

        // Set parameter names
        unsigned idx = 0;
        for (auto &arg : function->args())
        {
            if (idx < stmt->parameters.size())
            {
                arg.setName(stmt->parameters[idx].name);
            }
            idx++;
        }

        // Create body that calls the async version and awaits the result
        llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", function);
        builder.SetInsertPoint(block);

        // Call the async function with all arguments
        std::vector<llvm::Value *> args;
        for (auto &arg : function->args())
        {
            args.push_back(&arg);
        }

        llvm::Value *futureResult = builder.CreateCall(asyncFunc, args, "async.call");

        // Call the blocking get() method to await the result
        llvm::Function *getFunc = getStdLibFunction("Future_get");
        if (!getFunc)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Future_get method not found",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        llvm::Value *result = builder.CreateCall(getFunc, {futureResult}, "async.result");

        // Return the result
        builder.CreateRet(result);

        return;
    }

    // Handle generic functions
    if (stmt->isGeneric())
    {
        // ... existing generic function implementation ...
        return;
    }

    // Handle regular functions
    // ... existing function implementation ...
}

void IRGenerator::visitReturnStmt(ast::ReturnStmt *stmt)
{
    // Get return type of the current function
    llvm::Type *returnType = currentFunction->getReturnType();

    if (stmt->value)
    {
        // Evaluate return value
        stmt->value->accept(*this);
        if (!lastValue)
            return;

        // Check return type compatibility
        if (lastValue->getType() != returnType)
        {
            // Try to cast if possible
            if (lastValue->getType()->isIntegerTy() && returnType->isIntegerTy())
            {
                lastValue = builder.CreateIntCast(lastValue, returnType, true, "castret");
            }
            else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                     (returnType->isFloatTy() || returnType->isDoubleTy()))
            {
                lastValue = builder.CreateFPCast(lastValue, returnType, "castret");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Return value type does not match function return type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return;
            }
        }

        // Create return instruction
        builder.CreateRet(lastValue);
    }
    else
    {
        // Void return
        if (!returnType->isVoidTy())
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Missing return value in non-void function",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        builder.CreateRetVoid();
    }
}

void IRGenerator::visitCallExpr(ast::CallExpr *expr)
{
    // Evaluate callee
    expr->callee->accept(*this);
    llvm::Value *callee = lastValue;

    if (!callee)
        return;

    // Handle special case - direct function call by name
    if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr->callee))
    {
        std::string funcName = varExpr->name;

        // Check for standard library functions
        if (stdLibFunctions.find(funcName) != stdLibFunctions.end())
        {
            callee = stdLibFunctions[funcName];
        }
        else
        {
            // Try to find function in the module
            llvm::Function *func = module->getFunction(funcName);

            if (func)
            {
                callee = func;
            }
            else if (funcName.find("repl_expr_") == 0)
            {
                // Special handling for REPL expressions
                llvm::FunctionType *funcType = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(context), false);
                func = llvm::Function::Create(
                    funcType,
                    llvm::Function::ExternalLinkage,
                    funcName,
                    module.get());
                callee = func;

                // Create entry block
                llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", func);
                builder.SetInsertPoint(entryBlock);

                // Ensure the block is properly terminated
                builder.CreateRetVoid();
            }
        }
    }

    // Ensure callee is a function
    llvm::FunctionType *funcType = nullptr;
    if (callee->getType()->isPointerTy())
    {
        if (auto func = llvm::dyn_cast<llvm::Function>(callee))
        {
            funcType = func->getFunctionType();
        }
        else
        {
            // Try to get function type from context
            errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                     "Called value is not a function",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return;
        }
    }
    else
    {
        errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                 "Called value is not a function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate arguments
    std::vector<llvm::Value *> args;
    for (const auto &arg : expr->arguments)
    {
        arg->accept(*this);
        if (!lastValue)
            return;
        args.push_back(lastValue);
    }

    // Create the call instruction
    lastValue = builder.CreateCall(funcType, callee, args);

    // Ensure the current block is terminated if needed
    llvm::BasicBlock *currentBlock = builder.GetInsertBlock();
    if (!currentBlock->getTerminator())
    {
        if (currentBlock->getParent()->getReturnType()->isVoidTy())
        {
            builder.CreateRetVoid();
        }
        else
        {
            // For non-void functions, return a default value
            llvm::Type *returnType = currentBlock->getParent()->getReturnType();
            if (returnType->isIntegerTy())
            {
                builder.CreateRet(llvm::ConstantInt::get(returnType, 0));
            }
            else if (returnType->isFloatingPointTy())
            {
                builder.CreateRet(llvm::ConstantFP::get(returnType, 0.0));
            }
            else if (returnType->isPointerTy())
            {
                builder.CreateRet(llvm::ConstantPointerNull::get(
                    llvm::cast<llvm::PointerType>(returnType)));
            }
            else
            {
                // For other types, create an undef value
                builder.CreateRet(llvm::UndefValue::get(returnType));
            }
        }
    }
}

void IRGenerator::visitIfStmt(ast::IfStmt *stmt)
{
    // Generate condition
    stmt->condition->accept(*this);
    if (!lastValue)
        return;

    // Convert condition to boolean if needed
    llvm::Value *condValue = lastValue;
    if (!condValue->getType()->isIntegerTy(1))
    {
        if (condValue->getType()->isIntegerTy())
        {
            // Compare with zero for integers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantInt::get(condValue->getType(), 0),
                "ifcond");
        }
        else if (condValue->getType()->isFloatTy() || condValue->getType()->isDoubleTy())
        {
            // Compare with zero for floating point
            condValue = builder.CreateFCmpONE(
                condValue,
                llvm::ConstantFP::get(condValue->getType(), 0.0),
                "ifcond");
        }
        else if (condValue->getType()->isPointerTy())
        {
            // Compare with null for pointers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(condValue->getType())),
                "ifcond");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Condition must be convertible to a boolean",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }
    }

    // Create basic blocks for then, else, and continue
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(context, "then", function);
    llvm::BasicBlock *elseBlock = stmt->elseBranch ? llvm::BasicBlock::Create(context, "else") : nullptr;
    llvm::BasicBlock *continueBlock = llvm::BasicBlock::Create(context, "ifcont");

    // Create conditional branch
    if (elseBlock)
        builder.CreateCondBr(condValue, thenBlock, elseBlock);
    else
        builder.CreateCondBr(condValue, thenBlock, continueBlock);

    // Generate then branch
    builder.SetInsertPoint(thenBlock);

    // Save environment before entering new scope
    createEnvironment();

    stmt->thenBranch->accept(*this);

    // Restore environment after exiting scope
    restoreEnvironment();

    // Create branch to continue block if needed
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(continueBlock);

    // Generate else branch if it exists
    if (elseBlock)
    {
        elseBlock->insertInto(function);
        builder.SetInsertPoint(elseBlock);

        // Save environment before entering new scope
        createEnvironment();

        if (stmt->elseBranch)
            stmt->elseBranch->accept(*this);

        // Restore environment after exiting scope
        restoreEnvironment();

        // Create branch to continue block if needed
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(continueBlock);
    }

    // Continue with the next block
    continueBlock->insertInto(function);
    builder.SetInsertPoint(continueBlock);
}

void IRGenerator::visitWhileStmt(ast::WhileStmt *stmt)
{
    // Create basic blocks
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(context, "whilecond", function);
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(context, "whilebody");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(context, "whilecont");

    // Branch to condition
    builder.CreateBr(condBlock);

    // Generate condition
    builder.SetInsertPoint(condBlock);

    stmt->condition->accept(*this);
    if (!lastValue)
        return;

    // Convert condition to boolean if needed
    llvm::Value *condValue = lastValue;
    if (!condValue->getType()->isIntegerTy(1))
    {
        if (condValue->getType()->isIntegerTy())
        {
            // Compare with zero for integers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantInt::get(condValue->getType(), 0),
                "whilecond");
        }
        else if (condValue->getType()->isFloatTy() || condValue->getType()->isDoubleTy())
        {
            // Compare with zero for floating point
            condValue = builder.CreateFCmpONE(
                condValue,
                llvm::ConstantFP::get(condValue->getType(), 0.0),
                "whilecond");
        }
        else if (condValue->getType()->isPointerTy())
        {
            // Compare with null for pointers
            condValue = builder.CreateICmpNE(
                condValue,
                llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(condValue->getType())),
                "whilecond");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Condition must be convertible to a boolean",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }
    }

    // Create conditional branch
    builder.CreateCondBr(condValue, loopBlock, afterBlock);

    // Generate loop body
    loopBlock->insertInto(function);
    builder.SetInsertPoint(loopBlock);

    // Save environment before entering new scope
    createEnvironment();

    stmt->body->accept(*this);

    // Restore environment after exiting scope
    restoreEnvironment();

    // Create branch back to condition
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(condBlock);

    // Continue with the next block
    afterBlock->insertInto(function);
    builder.SetInsertPoint(afterBlock);
}

void IRGenerator::visitForStmt(ast::ForStmt *stmt)
{
    // Use direct field access instead of accessor methods
    std::string variable = stmt->variable;          // Changed from getVariable()
    ast::TypePtr variableType = stmt->variableType; // Changed from getVariableType()

    llvm::Function *function = builder.GetInsertBlock()->getParent();

    // Create blocks for loop
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(context, "loop", function);
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(context, "after");

    // Evaluate the iterable expression
    stmt->iterable->accept(*this);
    if (!lastValue)
        return;
    llvm::Value *iterableValue = lastValue;

    // With opaque pointers, we need to handle type info differently
    bool isValidIterable = true; // Assume valid until proven otherwise

    // Get a type for the iteration variable
    llvm::Type *varType = getLLVMType(variableType);
    llvm::AllocaInst *iterVar = builder.CreateAlloca(varType, nullptr, variable);

    // Store variable in the symbol table
    namedValues[variable] = iterVar;

    // Create a counter variable
    llvm::AllocaInst *indexVar = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "loop.index");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), indexVar);

    // Create a simple struct type for list-like structures
    llvm::StructType *iterableStructType = llvm::StructType::get(
        context,
        {llvm::Type::getInt64Ty(context), llvm::PointerType::get(context, 0)});

    // Get the length of the iterable
    std::vector<llvm::Value *> indices = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)};

    llvm::Value *lengthPtr = builder.CreateGEP(
        iterableStructType, iterableValue, indices, "length.ptr");
    llvm::Value *length = builder.CreateLoad(llvm::Type::getInt64Ty(context), lengthPtr, "length");

    // Check condition (i < length)
    llvm::Value *index = builder.CreateLoad(llvm::Type::getInt64Ty(context), indexVar, "index");
    llvm::Value *cond = builder.CreateICmpSLT(index, length, "loop.cond");
    builder.CreateCondBr(cond, loopBlock, afterBlock);

    // Start the loop body
    builder.SetInsertPoint(loopBlock);

    // Get the element at the current index
    // Load data pointer from iterable (assuming it's the second field)
    indices[1] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
    llvm::Value *dataPtr = builder.CreateGEP(
        iterableStructType, iterableValue, indices, "data.ptr");
    llvm::Value *data = builder.CreateLoad(llvm::PointerType::get(context, 0), dataPtr, "data");

    // Get current element
    index = builder.CreateLoad(llvm::Type::getInt64Ty(context), indexVar);
    llvm::Value *elementPtr = builder.CreateGEP(varType, data, index, "element.ptr");
    llvm::Value *element = builder.CreateLoad(varType, elementPtr, "element");

    // Store element in loop variable
    builder.CreateStore(element, iterVar);

    // Generate loop body
    stmt->body->accept(*this);

    // Increment index
    index = builder.CreateLoad(llvm::Type::getInt64Ty(context), indexVar);
    llvm::Value *nextIndex = builder.CreateAdd(
        index, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1), "next.index");
    builder.CreateStore(nextIndex, indexVar);

    // Check condition for next iteration
    cond = builder.CreateICmpSLT(nextIndex, length, "loop.cond");
    builder.CreateCondBr(cond, loopBlock, afterBlock);

    // After the loop
    afterBlock->insertInto(function);
    builder.SetInsertPoint(afterBlock);

    // Remove the loop variable from the symbol table
    namedValues.erase(variable);
}

// New helper method to infer type name from a value
std::string IRGenerator::inferTypeNameFromValue(llvm::Value *value)
{
    // This is a placeholder implementation that would need to be customized
    // based on your type tracking system

    // For now, just check if there's metadata or a name hint
    if (value->hasName())
    {
        llvm::StringRef name = value->getName();
        // Try to extract type info from the name if it follows a pattern
        // This is just an example and would need adaptation
        if (name.contains("_class_"))
        {
            return name.split("_class_").second.str();
        }
    }

    // Default fallback
    return "unknown";
}

void IRGenerator::visitUnaryExpr(ast::UnaryExpr *expr)
{
    if (!expr->operand) {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Unary expression missing operand",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate the operand first
    expr->operand->accept(*this);
    llvm::Value *operand = lastValue;

    if (!operand) {
        lastValue = nullptr;
        return;
    }

    switch (expr->op.type)
    {
    case TokenType::MINUS:
        if (operand->getType()->isIntegerTy()) {
            lastValue = builder->CreateNeg(operand, "neg");
        } else if (operand->getType()->isFloatTy() || operand->getType()->isDoubleTy()) {
            lastValue = builder->CreateFNeg(operand, "fneg");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_ERROR,
                                     "Cannot apply unary minus to non-numeric type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::BANG:
        if (operand->getType()->isIntegerTy(1)) {
            lastValue = builder->CreateNot(operand, "not");
        } else {
            // Convert to boolean if needed
            llvm::Value *boolVal = builder->CreateICmpNE(operand, 
                llvm::ConstantInt::get(operand->getType(), 0), "tobool");
            lastValue = builder->CreateNot(boolVal, "not");
        }
        break;

    case TokenType::BITWISE_NOT:
        if (operand->getType()->isIntegerTy()) {
            lastValue = builder->CreateNot(operand, "bitnot");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_ERROR,
                                     "Cannot apply bitwise NOT to non-integer type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::INCREMENT:
    case TokenType::DECREMENT:
        // Handle increment/decrement with proper lvalue support
        if (auto varExpr = dynamic_cast<ast::VariableExpr*>(expr->operand.get())) {
            // Get the variable's current value
            llvm::Value* varPtr = getVariable(varExpr->name);
            if (!varPtr) {
                errorHandler.reportError(error::ErrorCode::V001_UNDEFINED_VARIABLE,
                                         "Variable '" + varExpr->name + "' not found",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
                return;
            }

            llvm::Value* currentValue = builder->CreateLoad(operand->getType(), varPtr, "load");
            
            // Create the new value
            llvm::Value* newValue;
            if (expr->op.type == TokenType::INCREMENT) {
                if (currentValue->getType()->isIntegerTy()) {
                    newValue = builder->CreateAdd(currentValue, 
                        llvm::ConstantInt::get(currentValue->getType(), 1), "inc");
                } else if (currentValue->getType()->isFloatTy() || currentValue->getType()->isDoubleTy()) {
                    newValue = builder->CreateFAdd(currentValue, 
                        llvm::ConstantFP::get(currentValue->getType(), 1.0), "finc");
                } else {
                    errorHandler.reportError(error::ErrorCode::T001_TYPE_ERROR,
                                             "Cannot increment non-numeric type",
                                             "", 0, 0, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                    return;
                }
            } else { // DECREMENT
                if (currentValue->getType()->isIntegerTy()) {
                    newValue = builder->CreateSub(currentValue, 
                        llvm::ConstantInt::get(currentValue->getType(), 1), "dec");
                } else if (currentValue->getType()->isFloatTy() || currentValue->getType()->isDoubleTy()) {
                    newValue = builder->CreateFSub(currentValue, 
                        llvm::ConstantFP::get(currentValue->getType(), 1.0), "fdec");
                } else {
                    errorHandler.reportError(error::ErrorCode::T001_TYPE_ERROR,
                                             "Cannot decrement non-numeric type",
                                             "", 0, 0, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                    return;
                }
            }

            // Store the new value
            builder->CreateStore(newValue, varPtr);
            
            // Return the new value for prefix operators, old value for postfix
            // For now, we'll return the new value (prefix behavior)
            lastValue = newValue;
        } else {
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Increment/decrement requires lvalue (variable)",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    default:
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Unhandled or unsupported unary operator",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        break;
    }
}

void IRGenerator::visitLambdaExpr(ast::LambdaExpr *expr)
{
    // Get return type
    llvm::Type *returnType = getLLVMType(expr->returnType);
    if (!returnType)
        return;

    // Get parameter types
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : expr->parameters) // Changed from params to parameters
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
            return;

        paramTypes.push_back(paramType);
    }

    // Create function type
    llvm::FunctionType *functionType = llvm::FunctionType::get(returnType, paramTypes, false);

    // Create unique name for the lambda
    static int lambdaCounter = 0;
    std::string lambdaName = "lambda_" + std::to_string(lambdaCounter++);

    // Create function
    llvm::Function *function = llvm::Function::Create(
        functionType, llvm::Function::InternalLinkage, lambdaName, module.get());

    // Set parameter names
    unsigned idx = 0;
    for (auto &param : function->args())
    {
        param.setName(expr->parameters[idx++].name); // Changed from params to parameters, removed .lexeme
    }

    // Create basic block
    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", function);

    // Save current insert point
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    llvm::Function *savedFunction = currentFunction;

    // Set new insert point
    builder.SetInsertPoint(block);
    currentFunction = function;

    // Save previous variables
    std::map<std::string, llvm::AllocaInst *> savedNamedValues(namedValues);

    // Create allocas for parameters
    idx = 0;
    for (auto &param : function->args())
    {
        llvm::AllocaInst *alloca = createEntryBlockAlloca(
            function, param.getName().str(), param.getType());

        // Store the parameter value
        builder.CreateStore(&param, alloca);

        // Add to symbol table
        namedValues[param.getName().str()] = alloca;

        idx++;
    }

    // Codegen function body
    expr->body->accept(*this);

    // Add implicit return if needed
    if (!builder.GetInsertBlock()->getTerminator())
    {
        if (returnType->isVoidTy())
        {
            builder.CreateRetVoid();
        }
        else if (lastValue && lastValue->getType() == returnType)
        {
            // Use the last expression's value as the return value
            builder.CreateRet(lastValue);
        }
        else
        {
            // Insert a reasonable default return value
            if (returnType->isIntegerTy())
            {
                builder.CreateRet(llvm::ConstantInt::get(returnType, 0));
            }
            else if (returnType->isFloatTy() || returnType->isDoubleTy())
            {
                builder.CreateRet(llvm::ConstantFP::get(returnType, 0.0));
            }
            else if (returnType->isPointerTy())
            {
                builder.CreateRet(llvm::ConstantPointerNull::get(
                    llvm::cast<llvm::PointerType>(returnType)));
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                         "Cannot generate default return value for lambda",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                function->eraseFromParent();
                lastValue = nullptr;
                return;
            }
        }
    }

    // Verify the function
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Lambda verification failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        function->eraseFromParent();
        lastValue = nullptr;
        return;
    }

    // Restore previous state
    namedValues = savedNamedValues;
    currentFunction = savedFunction;

    if (savedBlock)
        builder.SetInsertPoint(savedBlock);

    // Return the function as a value
    lastValue = function;
}

void IRGenerator::visitListExpr(ast::ListExpr *expr)
{
    // Use getType() method for type information
    ast::TypePtr exprType = expr->getType();

    if (expr->elements.empty())
    {
        // Create an empty list using the type info
        createEmptyList(exprType);
        return;
    }

    // Evaluate the first element to determine type
    expr->elements[0]->accept(*this);
    llvm::Value *firstElement = lastValue;
    if (!firstElement)
        return;

    // Get element type
    llvm::Type *elementType = firstElement->getType();

    // Create list struct type: { int64_t length, elementType* data }
    std::vector<llvm::Type *> listFields = {
        llvm::Type::getInt64Ty(context),   // length
        llvm::PointerType::get(context, 0) // opaque pointer for data
    };
    llvm::StructType *listStructType = llvm::StructType::get(context, listFields);

    // Allocate list struct
    llvm::AllocaInst *listAlloc = builder.CreateAlloca(listStructType, nullptr, "list");

    // Set length
    llvm::Value *lengthPtr = builder.CreateStructGEP(listStructType, listAlloc, 0, "list.length");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->elements.size()), lengthPtr);

    // Allocate array for elements using the correct CreateMalloc signature
    llvm::Value *arraySize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->elements.size());

    // Calculate the size of each element
    llvm::Value *elementSize = llvm::ConstantExpr::getSizeOf(elementType);

    // Calculate total allocation size
    llvm::Value *totalSize = builder.CreateMul(arraySize, elementSize);

    // Call malloc
    llvm::FunctionType *mallocType = llvm::FunctionType::get(
        llvm::PointerType::get(context, 0),
        {llvm::Type::getInt64Ty(context)},
        false);
    llvm::Function *mallocFunc = getStdLibFunction("malloc");
    if (!mallocFunc)
    {
        mallocFunc = llvm::Function::Create(
            mallocType, llvm::Function::ExternalLinkage, "malloc", *module);
    }

    llvm::Value *dataPtr = builder.CreateCall(mallocFunc, {totalSize}, "list.data");

    // Store data pointer
    llvm::Value *dataStorePtr = builder.CreateStructGEP(listStructType, listAlloc, 1, "list.data_ptr");
    builder.CreateStore(dataPtr, dataStorePtr);

    // Store first element - bitcast to the right type first
    llvm::Value *typedPtr = builder.CreateBitCast(dataPtr,
                                                  llvm::PointerType::get(elementType, 0), "typed_data");

    llvm::Value *elementPtr = builder.CreateGEP(elementType, typedPtr,
                                                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                                                "list.element");
    builder.CreateStore(firstElement, elementPtr);

    // Process rest of elements
    for (size_t i = 1; i < expr->elements.size(); ++i)
    {
        expr->elements[i]->accept(*this);
        llvm::Value *element = lastValue;
        if (!element)
            return;

        // Validate element type
        if (element->getType() != elementType)
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "List elements must have the same type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        // Store element
        elementPtr = builder.CreateGEP(elementType, typedPtr,
                                       llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                       "list.element");
        builder.CreateStore(element, elementPtr);
    }

    // Return list
    lastValue = listAlloc;
}

void IRGenerator::createEmptyList(ast::TypePtr listTypeArg)
{
    // Get element type from list type
    llvm::Type *elementType = nullptr;

    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(listTypeArg))
    {
        if (genericType->name == "list" && !genericType->typeArguments.empty())
        {
            elementType = getLLVMType(genericType->typeArguments[0]);
        }
    }

    if (!elementType)
    {
        // Default to int if unable to determine
        elementType = llvm::Type::getInt64Ty(context);
    }

    // Create list struct type: { int64_t length, elementType* data }
    std::vector<llvm::Type *> listFields = {
        llvm::Type::getInt64Ty(context),   // length
        llvm::PointerType::get(context, 0) // opaque pointer for data
    };
    llvm::StructType *listStructType = llvm::StructType::get(context, listFields);

    // Allocate list struct
    llvm::AllocaInst *listAlloc = builder.CreateAlloca(listStructType, nullptr, "empty_list");

    // Set length to 0
    llvm::Value *lengthPtr = builder.CreateStructGEP(listStructType, listAlloc, 0, "list.length");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), lengthPtr);

    // Set data pointer to null
    llvm::Value *dataStorePtr = builder.CreateStructGEP(listStructType, listAlloc, 1, "list.data_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)), dataStorePtr);

    // Return list
    lastValue = listAlloc;
}

void IRGenerator::visitDictionaryExpr(ast::DictionaryExpr *expr)
{
    if (expr->entries.empty())
    {
        // Use getType() instead of type
        createEmptyDictionary(expr->getType());
        return;
    }

    // Process first entry
    auto &firstEntry = expr->entries[0];
    firstEntry.first->accept(*this);
    if (!lastValue)
        return;
    llvm::Value *firstKey = lastValue;

    firstEntry.second->accept(*this);
    if (!lastValue)
        return;
    llvm::Value *firstValue = lastValue;

    // Get key and value types
    llvm::Type *keyType = firstKey->getType();
    llvm::Type *valueType = firstValue->getType();

    // For now, we'll implement dictionary as a simple struct with keys and values arrays
    // { int64_t size, keyType* keys, valueType* values }
    std::vector<llvm::Type *> dictFields = {
        llvm::Type::getInt64Ty(context),        // size
        llvm::PointerType::getUnqual(keyType),  // keys
        llvm::PointerType::getUnqual(valueType) // values
    };
    llvm::StructType *dictType = llvm::StructType::get(context, dictFields);

    // Allocate dictionary struct
    llvm::AllocaInst *dictAlloc = builder.CreateAlloca(dictType, nullptr, "dict");

    // Set size
    llvm::Value *sizePtr = builder.CreateStructGEP(dictType, dictAlloc, 0, "dict.size");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->entries.size()), sizePtr);

    // Allocate arrays for keys and values
    llvm::Value *arraySize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->entries.size());

    // Get the malloc function
    llvm::Function *mallocFunc = getStdLibFunction("malloc");
    if (!mallocFunc)
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Could not find malloc function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    // Calculate size for keys
    llvm::Value *keySize = llvm::ConstantExpr::getSizeOf(keyType);
    llvm::Value *totalKeysSize = builder.CreateMul(arraySize, keySize, "keys.size");

    // Call malloc for keys
    llvm::Value *keysPtr = builder.CreateCall(mallocFunc, {totalKeysSize}, "dict.keys");
    llvm::Value *typedKeysPtr = builder.CreateBitCast(keysPtr, llvm::PointerType::get(keyType, 0), "typed_keys");

    // Calculate size for values
    llvm::Value *valueSize = llvm::ConstantExpr::getSizeOf(valueType);
    llvm::Value *totalValuesSize = builder.CreateMul(arraySize, valueSize, "values.size");

    // Call malloc for values
    llvm::Value *valuesPtr = builder.CreateCall(mallocFunc, {totalValuesSize}, "dict.values");
    llvm::Value *typedValuesPtr = builder.CreateBitCast(valuesPtr, llvm::PointerType::get(valueType, 0), "typed_values");

    // Store pointers
    llvm::Value *keysStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 1, "dict.keys_ptr");
    builder.CreateStore(typedKeysPtr, keysStorePtr);

    llvm::Value *valuesStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 2, "dict.values_ptr");
    builder.CreateStore(typedValuesPtr, valuesStorePtr);

    // Store first key-value pair
    llvm::Value *keyPtr = builder.CreateGEP(keyType, typedKeysPtr,
                                            llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                                            "dict.key");
    builder.CreateStore(firstKey, keyPtr);

    llvm::Value *valuePtr = builder.CreateGEP(valueType, typedValuesPtr,
                                              llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                                              "dict.value");
    builder.CreateStore(firstValue, valuePtr);

    // Process rest of key-value pairs
    for (size_t i = 1; i < expr->entries.size(); ++i)
    {
        expr->entries[i].first->accept(*this);
        llvm::Value *key = lastValue;
        if (!key)
            return;

        expr->entries[i].second->accept(*this);
        llvm::Value *value = lastValue;
        if (!value)
            return;

        // Validate types
        if (key->getType() != keyType || value->getType() != valueType)
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Dictionary keys and values must have consistent types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        // Store key-value pair
        keyPtr = builder.CreateGEP(keyType, typedKeysPtr,
                                   llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                   "dict.key");
        builder.CreateStore(key, keyPtr);

        valuePtr = builder.CreateGEP(valueType, typedValuesPtr,
                                     llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                     "dict.value");
        builder.CreateStore(value, valuePtr);
    }

    // Return dictionary
    lastValue = dictAlloc;
}

void IRGenerator::createEmptyDictionary(ast::TypePtr dictType)
{
    // Get key and value types from dictionary type
    llvm::Type *keyType = nullptr;
    llvm::Type *valueType = nullptr;

    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(dictType))
    {
        if (genericType->name == "dict" && genericType->typeArguments.size() >= 2)
        {
            keyType = getLLVMType(genericType->typeArguments[0]);
            valueType = getLLVMType(genericType->typeArguments[1]);
        }
    }

    if (!keyType || !valueType)
    {
        // Default to string->int if unable to determine
        keyType = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)); // string
        valueType = llvm::Type::getInt64Ty(context);                            // int
    }

    // Create dictionary struct type
    std::vector<llvm::Type *> dictFields = {
        llvm::Type::getInt64Ty(context),        // size
        llvm::PointerType::getUnqual(keyType),  // keys
        llvm::PointerType::getUnqual(valueType) // values
    };
    llvm::StructType *dictStructType = llvm::StructType::get(context, dictFields);

    // Allocate dictionary struct
    llvm::AllocaInst *dictAlloc = builder.CreateAlloca(dictStructType, nullptr, "empty_dict");

    // Set size to 0
    llvm::Value *sizePtr = builder.CreateStructGEP(dictStructType, dictAlloc, 0, "dict.size");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), sizePtr);

    // Set pointers to null
    llvm::Value *keysStorePtr = builder.CreateStructGEP(dictStructType, dictAlloc, 1, "dict.keys_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(keyType)), keysStorePtr);

    llvm::Value *valuesStorePtr = builder.CreateStructGEP(dictStructType, dictAlloc, 2, "dict.values_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(valueType)), valuesStorePtr);

    // Return dictionary
    lastValue = dictAlloc;
}

void IRGenerator::visitClassStmt(ast::ClassStmt *stmt)
{
    // Check if this is a generic class
    if (stmt->isGeneric())
    {
        // Similar to generic functions, we don't generate code immediately
        // We'll instantiate them when needed

        // Just register the class for later instantiation

        // No code generation needed here
        return;
    }

    // The rest of the class implementation for non-generic classes
    // ... existing non-generic class implementation ...
}

void IRGenerator::generateMethod(const std::string &className, llvm::StructType *classType, ast::FunctionStmt *method)
{
    // Get return type
    llvm::Type *returnType = getLLVMType(method->returnType);
    if (!returnType)
        return;

    // Get parameter types, add 'this' pointer as first parameter
    std::vector<llvm::Type *> paramTypes;
    paramTypes.push_back(llvm::PointerType::getUnqual(classType)); // 'this' pointer

    for (const auto &param : method->parameters)
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
            return;

        paramTypes.push_back(paramType);
    }

    // Create method name with class prefix to avoid name conflicts
    std::string methodName = className + "_" + method->name;

    // Create function type
    llvm::FunctionType *functionType = llvm::FunctionType::get(returnType, paramTypes, false);

    // Create function
    llvm::Function *function = llvm::Function::Create(
        functionType, llvm::Function::ExternalLinkage, methodName, module.get());

    // Set parameter names, first param is 'this'
    auto argIt = function->arg_begin();
    argIt->setName("this");
    ++argIt;

    unsigned idx = 0;
    for (; argIt != function->arg_end(); ++argIt, ++idx)
    {
        argIt->setName(method->parameters[idx].name);
    }

    // Create basic block
    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", function);

    // Save current insert point
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    llvm::Function *savedFunction = currentFunction;

    // Set new insert point
    builder.SetInsertPoint(block);
    currentFunction = function;

    // Save previous variables
    std::map<std::string, llvm::AllocaInst *> savedNamedValues(namedValues);
    namedValues.clear();

    // Create allocas for parameters, starting with 'this'
    argIt = function->arg_begin();
    llvm::Value *thisValue = argIt;
    llvm::AllocaInst *thisAlloca = createEntryBlockAlloca(function, "this", thisValue->getType());
    builder.CreateStore(thisValue, thisAlloca);
    namedValues["this"] = thisAlloca;
    ++argIt;

    idx = 0;
    for (; argIt != function->arg_end(); ++argIt, ++idx)
    {
        llvm::AllocaInst *alloca = createEntryBlockAlloca(
            function, argIt->getName().str(), argIt->getType());

        // Store the parameter value
        builder.CreateStore(&*argIt, alloca);

        // Add to symbol table
        namedValues[argIt->getName().str()] = alloca;
    }

    // Store the method in the virtual method table
    classMethods[className + "." + method->name] = function;

    // Codegen method body
    method->body->accept(*this);

    // Add implicit return if needed
    if (!builder.GetInsertBlock()->getTerminator())
    {
        if (returnType->isVoidTy())
        {
            builder.CreateRetVoid();
        }
        else
        {
            // Insert a reasonable default return value
            if (returnType->isIntegerTy())
            {
                builder.CreateRet(llvm::ConstantInt::get(returnType, 0));
            }
            else if (returnType->isFloatTy() || returnType->isDoubleTy())
            {
                builder.CreateRet(llvm::ConstantFP::get(returnType, 0.0));
            }
            else if (returnType->isPointerTy())
            {
                builder.CreateRet(llvm::ConstantPointerNull::get(
                    llvm::cast<llvm::PointerType>(returnType)));
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                         "Cannot generate default return value for method",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
            }
        }
    }

    // Verify the function
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Method verification failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        function->eraseFromParent();
        return;
    }

    // Restore previous state
    namedValues = savedNamedValues;
    currentFunction = savedFunction;

    if (savedBlock)
        builder.SetInsertPoint(savedBlock);
}

void IRGenerator::visitGetExpr(ast::GetExpr *expr)
{
    // Evaluate the object expression
    expr->object->accept(*this);
    if (!lastValue)
        return;

    llvm::Value *object = lastValue;

    // With opaque pointers, we need to get the type information from elsewhere
    std::string className;
    if (expr->getType()) // Changed from getTypeInfo() to getType()
    {
        // Try to get the class name from type info if available
        className = expr->getType()->toString();
    }
    else
    {
        // Fallback - try to get from the module
        className = inferTypeNameFromValue(object);
    }

    // Look up the class info
    auto it = classTypes.find(className);
    if (it != classTypes.end())
    {
        // It's a proper class/struct object
        const ClassInfo &classInfo = it->second;
        llvm::StructType *structType = classInfo.classType;

        // Find the field index
        int fieldIndex = -1;
        for (size_t i = 0; i < classInfo.memberNames.size(); i++)
        {
            if (classInfo.memberNames[i] == expr->name)
            {
                fieldIndex = i;
                break;
            }
        }

        if (fieldIndex != -1)
        {
            // Create a GEP instruction to access the field
            std::vector<llvm::Value *> indices = {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), fieldIndex)};

            // Get a pointer to the field (using the known struct type)
            llvm::Value *fieldPtr = builder.CreateGEP(
                structType, object, indices, "field." + expr->name);

            // Load the field value (use the field type from class info)
            llvm::Type *fieldType = structType->getStructElementType(fieldIndex);
            lastValue = builder.CreateLoad(fieldType, fieldPtr);
            return;
        }
        else
        {
            // Check if it's a method call
            std::string methodName = className + "." + expr->name;
            auto methodIt = classMethods.find(methodName);
            if (methodIt != classMethods.end())
            {
                // Return the bound method function pointer
                lastValue = methodIt->second;

                // Store the 'this' pointer for the method call
                llvm::AllocaInst *basePtr = createEntryBlockAlloca(
                    currentFunction, "this",
                    llvm::PointerType::get(context, 0)); // opaque pointer
                builder.CreateStore(object, basePtr);

                // Load it back to attach metadata
                llvm::Value *base = builder.CreateLoad(
                    llvm::PointerType::get(context, 0), basePtr);

                // Add methodThis declaration to class if missing
                if (!module->getGlobalVariable("methodThis"))
                {
                    // Create a global variable for methodThis
                    new llvm::GlobalVariable(
                        *module,
                        llvm::PointerType::get(context, 0),
                        false,
                        llvm::GlobalValue::ExternalLinkage,
                        llvm::Constant::getNullValue(llvm::PointerType::get(context, 0)),
                        "methodThis");
                }

                // Get the methodThis global
                llvm::GlobalVariable *methodThis = module->getGlobalVariable("methodThis");

                // Store the this pointer in a global for the method to access
                builder.CreateStore(base, methodThis);
                return;
            }
        }
    }

    // If we get here, the field or method was not found
    errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                             "Undefined property or method: " + expr->name,
                             "", 0, 0, error::ErrorSeverity::ERROR);
    lastValue = nullptr;
}

void IRGenerator::visitSetExpr(ast::SetExpr *expr)
{
    // Evaluate the object expression
    expr->object->accept(*this);
    if (!lastValue)
        return;

    llvm::Value *object = lastValue;

    // Get the type of the object
    llvm::Type *pointedType = nullptr;
    if (auto ptrType = llvm::dyn_cast<llvm::PointerType>(object->getType()))
    {
        // For opaque pointers, we need structural type information from elsewhere
        // Try to find from class info first
        std::string className = inferTypeNameFromValue(object);
        auto classIt = classTypes.find(className);
        if (classIt != classTypes.end())
        {
            pointedType = classIt->second.classType;
        }
        else
        {
            // As a fallback, see if we have type information through a BitCast
            if (auto bitCast = llvm::dyn_cast<llvm::BitCastInst>(object))
            {
                auto srcTy = bitCast->getSrcTy();
                if (srcTy->isPointerTy() && llvm::cast<llvm::PointerType>(srcTy)->getArrayElementType()->isStructTy())
                {
                    pointedType = llvm::cast<llvm::PointerType>(srcTy)->getArrayElementType();
                }
            }

            // If still no type info, handle as a generic structure
            if (!pointedType)
            {
                errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                         "Cannot determine pointed type for object",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
                return;
            }
        }
    }
    else
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Cannot access field of non-pointer type",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Check if it's a struct/class type
    if (llvm::StructType *structType = llvm::dyn_cast<llvm::StructType>(pointedType))
    {
        // Try to find the class info
        std::string className = structType->getName().str();

        // Sometimes LLVM adds a dot prefix to struct names, so remove it if present
        if (className.size() > 0 && className[0] == '.')
        {
            className = className.substr(1);
        }

        auto it = classTypes.find(className);
        if (it != classTypes.end())
        {
            // It's a proper class/struct object
            const ClassInfo &classInfo = it->second;

            // Find the field index
            int fieldIndex = -1;
            for (size_t i = 0; i < classInfo.memberNames.size(); i++)
            {
                if (classInfo.memberNames[i] == expr->name)
                {
                    fieldIndex = i;
                    break;
                }
            }

            if (fieldIndex != -1)
            {
                // Create a GEP instruction to access the field
                std::vector<llvm::Value *> indices = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), fieldIndex)};

                llvm::Value *fieldPtr = builder.CreateGEP(
                    pointedType, object, indices, "field." + expr->name);

                // Evaluate the value to assign
                expr->value->accept(*this);
                if (!lastValue)
                    return;

                // Get the field type
                llvm::Type *fieldType = nullptr;
                if (auto ptrType = llvm::dyn_cast<llvm::PointerType>(fieldPtr->getType()))
                {
                    // For opaque pointers, we need to use context information
                    if (fieldIndex < structType->getNumElements())
                    {
                        fieldType = structType->getElementType(fieldIndex);
                    }
                    else
                    {
                        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                                 "Field index out of bounds",
                                                 "", 0, 0, error::ErrorSeverity::ERROR);
                        lastValue = nullptr;
                        return;
                    }
                }
                else
                {
                    fieldType = fieldPtr->getType();
                }

                // Check that the types match
                if (lastValue->getType() != fieldType)
                {
                    // Try simple numeric conversions
                    if (lastValue->getType()->isIntegerTy() && fieldType->isIntegerTy())
                    {
                        lastValue = builder.CreateIntCast(lastValue, fieldType, true, "cast");
                    }
                    else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                             (fieldType->isFloatTy() || fieldType->isDoubleTy()))
                    {
                        lastValue = builder.CreateFPCast(lastValue, fieldType, "cast");
                    }
                    else
                    {
                        errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                                 "Type mismatch in field assignment",
                                                 "", 0, 0, error::ErrorSeverity::ERROR);
                        lastValue = nullptr;
                        return;
                    }
                }

                // Store the value in the field
                builder.CreateStore(lastValue, fieldPtr);
                return;
            }
            else if (classInfo.baseClass)
            {
                // If field not found in this class, try base class
                llvm::AllocaInst *basePtr = createEntryBlockAlloca(
                    currentFunction, "base", object->getType());
                builder.CreateStore(object, basePtr);

                // Load the base class pointer
                llvm::Value *base = nullptr;
                if (auto ptrType = llvm::dyn_cast<llvm::PointerType>(basePtr->getType()))
                {
                    // For opaque pointers in LLVM 15+, use the basePtr type directly
                    base = builder.CreateLoad(object->getType(), basePtr);
                }
                else
                {
                    base = builder.CreateLoad(basePtr->getType(), basePtr);
                }

                // Cast to base class type
                llvm::PointerType *baseType = llvm::PointerType::get(classInfo.baseClass, 0);
                llvm::Value *baseCast = builder.CreateBitCast(base, baseType);

                // Set the object to the base object and try again
                lastValue = baseCast;
                visitSetExpr(expr);
                return;
            }
        }
    }

    // If we get here, the field was not found
    errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                             "Undefined property: " + expr->name,
                             "", 0, 0, error::ErrorSeverity::ERROR);
    lastValue = nullptr;
}

void IRGenerator::visitDeleteExpr(ast::DeleteExpr *expr)
{
    // Use getExpr() instead of expression
    expr->getExpr()->accept(*this);
    if (!lastValue)
        return;

    // ... rest of the function ...
}

void IRGenerator::visitStringInterpolationExpr(ast::StringInterpolationExpr *expr)
{
    // Generate code for string interpolation by concatenating text parts with evaluated expressions
    std::vector<llvm::Value *> stringParts;

    // Get text parts and expressions
    const auto &textParts = expr->getTextParts();
    const auto &expressions = expr->getExpressions();

    // Sanity check: textParts.size() should be expressions.size() + 1
    if (textParts.size() != expressions.size() + 1)
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Malformed string interpolation expression",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Add the first text part - use CreateGlobalString instead of CreateGlobalString
    stringParts.push_back(builder.CreateGlobalString(textParts[0], "str_part"));

    // Process each expression and add the corresponding text part
    for (size_t i = 0; i < expressions.size(); i++)
    {
        // Generate code for the expression
        expressions[i]->accept(*this);
        if (!lastValue)
        {
            return;
        }

        // Convert the expression value to a string
        llvm::Value *strValue = convertToString(lastValue);
        stringParts.push_back(strValue);

        // Add the next text part - use CreateGlobalString
        stringParts.push_back(builder.CreateGlobalString(textParts[i + 1], "str_part"));
    }

    // Concatenate all string parts
    lastValue = concatenateStrings(stringParts);
}

llvm::Value *IRGenerator::convertToString(llvm::Value *value)
{
    // Convert a value to a string representation
    llvm::Type *type = value->getType();

    // Get the appropriate string conversion function
    llvm::Function *convertFunc = nullptr;

    if (type->isIntegerTy())
    {
        convertFunc = getStdLibFunction("int_to_string");
    }
    else if (type->isFloatingPointTy())
    {
        convertFunc = getStdLibFunction("float_to_string");
    }
    else if (type->isPointerTy())
    {
        // For opaque pointers, we can't check element type directly
        // Assume it's a string if it's a pointer
        return value;
    }
    else
    {
        // Try to use a generic toString method
        convertFunc = getStdLibFunction("to_string");
    }

    if (!convertFunc)
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Cannot convert value to string - missing conversion function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return builder.CreateGlobalString("[ERROR]", "error_str");
    }

    // Call the conversion function
    return builder.CreateCall(convertFunc, {value}, "to_string");
}

llvm::Value *IRGenerator::concatenateStrings(const std::vector<llvm::Value *> &strings)
{
    // Get the string concatenation function
    llvm::Function *concatFunc = getStdLibFunction("string_concat");
    if (!concatFunc)
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "String concatenation function not found",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return builder.CreateGlobalString("[ERROR]", "error_str");
    }

    // Handle the base case
    if (strings.empty())
    {
        return builder.CreateGlobalString("", "empty_str");
    }

    // Start with the first string
    llvm::Value *result = strings[0];

    // Concatenate the rest
    for (size_t i = 1; i < strings.size(); i++)
    {
        result = builder.CreateCall(concatFunc, {result, strings[i]}, "concat");
    }

    return result;
}

// Scoping related implementation
void IRGenerator::enterScope()
{
    // Create a new scope with the current scope as parent
    currentScope = new Scope(currentScope);
}

void IRGenerator::exitScope()
{
    // Return to parent scope
    if (currentScope)
    {
        Scope *parent = currentScope->parent;
        delete currentScope;
        currentScope = parent;
    }
}

// Implicit type conversion implementation
llvm::Value *IRGenerator::implicitConversion(llvm::Value *value, llvm::Type *targetType)
{
    llvm::Type *sourceType = value->getType();

    // If types are the same, no conversion needed
    if (sourceType == targetType)
    {
        return value;
    }

    // Check if conversion is possible
    if (!canConvertImplicitly(sourceType, targetType))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Cannot implicitly convert between types",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return nullptr;
    }

    // Integer conversions
    if (sourceType->isIntegerTy() && targetType->isIntegerTy())
    {
        unsigned sourceWidth = sourceType->getIntegerBitWidth();
        unsigned targetWidth = targetType->getIntegerBitWidth();

        if (sourceWidth < targetWidth)
        {
            // Integer widening (e.g., i32 to i64)
            return builder.CreateSExt(value, targetType, "int_widen");
        }
        else
        {
            // Integer narrowing (e.g., i64 to i32)
            return builder.CreateTrunc(value, targetType, "int_narrow");
        }
    }

    // Floating point conversions
    if (sourceType->isFloatingPointTy() && targetType->isFloatingPointTy())
    {
        if (sourceType->isDoubleTy() && targetType->isFloatTy())
        {
            // Double to float (narrowing)
            return builder.CreateFPTrunc(value, targetType, "fp_narrow");
        }
        else
        {
            // Float to double (widening)
            return builder.CreateFPExt(value, targetType, "fp_widen");
        }
    }

    // Integer to floating point
    if (sourceType->isIntegerTy() && targetType->isFloatingPointTy())
    {
        // Signed integers to float
        return builder.CreateSIToFP(value, targetType, "int_to_fp");
    }

    // Floating point to integer
    if (sourceType->isFloatingPointTy() && targetType->isIntegerTy())
    {
        // Float to signed integer
        return builder.CreateFPToSI(value, targetType, "fp_to_int");
    }

    // Pointer to integer
    if (sourceType->isPointerTy() && targetType->isIntegerTy())
    {
        return builder.CreatePtrToInt(value, targetType, "ptr_to_int");
    }

    // Integer to pointer
    if (sourceType->isIntegerTy() && targetType->isPointerTy())
    {
        return builder.CreateIntToPtr(value, targetType, "int_to_ptr");
    }

    // Pointer casting
    if (sourceType->isPointerTy() && targetType->isPointerTy())
    {
        return builder.CreateBitCast(value, targetType, "ptr_cast");
    }

    // If we get here, we don't know how to convert
    errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                             "Unsupported implicit conversion",
                             "", 0, 0, error::ErrorSeverity::ERROR);
    return nullptr;
}

bool IRGenerator::canConvertImplicitly(llvm::Type *sourceType, llvm::Type *targetType)
{
    // Same types can always be converted
    if (sourceType == targetType)
    {
        return true;
    }

    // Integer to integer conversions
    if (sourceType->isIntegerTy() && targetType->isIntegerTy())
    {
        // Allow all integer conversions (widening and narrowing)
        return true;
    }

    // Floating point conversions
    if (sourceType->isFloatingPointTy() && targetType->isFloatingPointTy())
    {
        // Allow all floating point conversions
        return true;
    }

    // Integer to floating point
    if (sourceType->isIntegerTy() && targetType->isFloatingPointTy())
    {
        return true;
    }

    // Floating point to integer
    if (sourceType->isFloatingPointTy() && targetType->isIntegerTy())
    {
        return true;
    }

    // Pointer to integer
    if (sourceType->isPointerTy() && targetType->isIntegerTy())
    {
        // Only allow conversion to integer types that can hold a pointer
        return targetType->getIntegerBitWidth() >= 32;
    }

    // Integer to pointer
    if (sourceType->isIntegerTy() && targetType->isPointerTy())
    {
        // Only allow conversion from integer types that can hold a pointer
        return sourceType->getIntegerBitWidth() >= 32;
    }

    // Pointer to pointer conversion (casting)
    if (sourceType->isPointerTy() && targetType->isPointerTy())
    {
        // Allow casting between any pointer types
        return true;
    }

    // All other conversions are not implicitly allowed
    return false;
}

/**
 * @brief Handle variable assignment for assignment expressions
 *
 * @param expr The expression target (must be a VariableExpr)
 * @param rhs The right-hand side value to assign
 * @return true if assignment was successful, false otherwise
 */
bool IRGenerator::handleVariableAssignment(ast::AssignExpr *expr, llvm::Value *rhs)
{
    if (auto varExpr = dynamic_cast<ast::VariableExpr *>(expr->target.get()))
    {
        std::string name = varExpr->name; // Use direct member access instead of getName()

        // Look up the variable in the current scope
        llvm::AllocaInst *alloca = currentScope ? currentScope->lookup(name) : lookupVariable(name);

        if (!alloca)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Undefined variable: " + name,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return false;
        }

        // Validate that initializer type matches variable type
        if (rhs->getType() != alloca->getAllocatedType())
        {
            // Simple cast for numeric values
            if (rhs->getType()->isIntegerTy() && alloca->getAllocatedType()->isIntegerTy())
            {
                rhs = builder.CreateIntCast(rhs, alloca->getAllocatedType(), true, "cast");
            }
            else if ((rhs->getType()->isFloatTy() || rhs->getType()->isDoubleTy()) &&
                     (alloca->getAllocatedType()->isFloatTy() || alloca->getAllocatedType()->isDoubleTy()))
            {
                rhs = builder.CreateFPCast(rhs, alloca->getAllocatedType(), "cast");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Initializer type does not match variable type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }
        }

        // Store the initial value
        builder.CreateStore(rhs, alloca);
        return true;
    }

    return false;
}

std::unique_ptr<llvm::Module> IRGenerator::generate(ast::StmtPtr ast)
{
    if (!ast)
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Null AST passed to IRGenerator",
                                 "", 0, 0, error::ErrorSeverity::FATAL);
        return nullptr;
    }

    // Create a simple main function to start with
    createMainFunction();

    // Create the standard library functions
    declarePrintFunction();

    // Create a global scope
    enterScope();

    // Visit the AST to generate IR
    ast->accept(*this);

    // Exit the global scope
    exitScope();

    // Verify the module
    std::string verificationErrors;
    llvm::raw_string_ostream errStream(verificationErrors);
    if (llvm::verifyModule(*module, &errStream))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Module verification failed: " + verificationErrors,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
    }

    return std::move(module);
}

/**
 * @brief Look up a variable in the current scope and global scope
 *
 * @param name The name of the variable to look up
 * @return llvm::AllocaInst* The allocation instruction for the variable, or nullptr if not found
 */
llvm::AllocaInst *IRGenerator::lookupVariable(const std::string &name)
{
    // First look in the named values table
    auto it = namedValues.find(name);
    if (it != namedValues.end())
    {
        return it->second;
    }

    // If not found and we have a scope, check there
    if (currentScope)
    {
        return currentScope->lookup(name);
    }

    return nullptr;
}

/**
 * @brief Handles assignment expressions in the AST
 *
 * This method handles different types of assignment targets:
 * - Variable assignment (e.g., x = 5)
 * - Property assignment (e.g., obj.prop = 5)
 * - Indexed assignment (e.g., arr[0] = 5)
 * - Array element assignment (e.g., arr[i] = value)
 *
 * @param expr The assignment expression to generate code for
 */
void IRGenerator::visitAssignExpr(ast::AssignExpr *expr)
{
    // First evaluate the right-hand side
    expr->value->accept(*this);
    llvm::Value *rhs = lastValue;

    if (!rhs)
    {
        return;
    }

    // Handle variable assignment
    if (auto varExpr = dynamic_cast<ast::VariableExpr *>(expr->target.get()))
    {
        if (handleVariableAssignment(expr, rhs))
        {
            lastValue = rhs;
            return;
        }
        else
        {
            lastValue = nullptr;
            return;
        }
    }

    // Handle property assignment (obj.prop = value)
    if (auto getExpr = dynamic_cast<ast::GetExpr *>(expr->target.get()))
    {
        // Evaluate the object
        getExpr->object->accept(*this);
        if (!lastValue)
        {
            return;
        }

        llvm::Value *object = lastValue;

        // Create a temporary SetExpr to handle the property assignment
        auto setExpr = std::make_shared<ast::SetExpr>(
            expr->token,
            getExpr->object,
            getExpr->name,
            expr->value);

        // Visit the SetExpr to generate property assignment code
        visitSetExpr(setExpr.get());
        return;
    }

    // Handle indexed assignment (arr[index] = value)
    if (auto indexExpr = dynamic_cast<ast::IndexExpr *>(expr->target.get()))
    {
        // Evaluate the array/object
        indexExpr->object->accept(*this);
        if (!lastValue)
        {
            return;
        }
        llvm::Value *object = lastValue;

        // Evaluate the index
        indexExpr->index->accept(*this);
        if (!lastValue)
        {
            return;
        }
        llvm::Value *index = lastValue;

        // Generate array element assignment
        if (object->getType()->isPointerTy() && 
            object->getType()->getPointerElementType()->isArrayTy())
        {
            // Array type - get element pointer
            std::vector<llvm::Value*> indices = {
                llvm::ConstantInt::get(builder->getInt32Ty(), 0),
                index
            };
            llvm::Value *elementPtr = builder->CreateGEP(
                object->getType()->getPointerElementType(), object, indices, "array_elem_ptr");
            
            // Store the value
            builder->CreateStore(rhs, elementPtr);
            lastValue = rhs;
            return;
        }
        else if (object->getType()->isPointerTy() && 
                 object->getType()->getPointerElementType()->isPointerTy())
        {
            // Pointer to pointer (dynamic array) - get element pointer
            llvm::Value *elementPtr = builder->CreateGEP(
                object->getType()->getPointerElementType(), object, index, "ptr_elem_ptr");
            
            // Store the value
            builder->CreateStore(rhs, elementPtr);
            lastValue = rhs;
            return;
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_ERROR,
                                     "Cannot index non-array type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return;
        }
    }

    // Handle compound assignment operators (e.g., +=, -=, *=, etc.)
    if (auto binaryExpr = dynamic_cast<ast::BinaryExpr *>(expr->target.get()))
    {
        // This would handle cases like (x + y) = z, which should be an error
        errorHandler.reportError(error::ErrorCode::T001_TYPE_ERROR,
                                 "Cannot assign to expression result",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Other forms of assignment not implemented yet
    errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                             "Unsupported assignment target type",
                             "", 0, 0, error::ErrorSeverity::ERROR);
    lastValue = nullptr;
}

// Add this near the top of the file, after the includes
llvm::Type *IRGenerator::createOpaquePtr(llvm::Type *elementType)
{
    return llvm::PointerType::get(context, 0);
}

void IRGenerator::visitArrayLiteralExpr(ast::ArrayLiteralExpr *expr)
{
    if (expr->elements.empty())
    {
        // Create empty array using createEmptyList
        ast::TypePtr exprType = expr->getType();
        if (exprType)
        {
            createEmptyList(exprType);
        }
        else
        {
            // If no type info, create a default empty list of integers
            createEmptyList(std::make_shared<ast::GenericType>(
                ast::DEFAULT_TOKEN,
                "list",
                std::vector<ast::TypePtr>{std::make_shared<ast::SimpleType>(
                    lexer::Token(lexer::TokenType::IDENTIFIER, "int", "", 0, 0))}));
        }
        return;
    }
    // ... rest of the function ...
}

// Empty implementations for types from other namespaces
void IRGenerator::visitMoveExpr(void *expr)
{
    // MoveExpr: just forward the value for now (no real move semantics in LLVM IR)
    auto moveExpr = static_cast<type_checker::MoveExpr *>(expr);
    if (moveExpr) {
        moveExpr->getExpr()->accept(*this);
        // In a real implementation, mark the source as moved-from
        // For now, just forward the value
        // Optionally, could bitcast to rvalue reference type
    } else {
        lastValue = nullptr;
    }
}

void IRGenerator::visitGoExpr(void *expr)
{
    // GoExpr: emit a call to a runtime stub for goroutine launch
    auto goExpr = static_cast<ast::GoStmt *>(expr);
    if (goExpr && goExpr->expression) {
        // Evaluate the function call expression
        goExpr->expression->accept(*this);
        llvm::Value *fnCall = lastValue;
        // Emit a call to a runtime stub (e.g., __tocin_go_launch)
        llvm::Function *goStub = getStdLibFunction("__tocin_go_launch");
        if (!goStub) {
            // Declare the stub if not present
            llvm::FunctionType *goType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context), {fnCall->getType()}, false);
            goStub = llvm::Function::Create(goType, llvm::Function::ExternalLinkage, "__tocin_go_launch", *module);
        }
        builder.CreateCall(goStub, {fnCall});
        lastValue = nullptr;
    } else {
        lastValue = nullptr;
    }
}

void IRGenerator::visitRuntimeChannelSendExpr(void *expr)
{
    // ChannelSendExpr: emit a call to a runtime stub for channel send
    auto sendExpr = static_cast<ast::ChannelSendExpr *>(expr);
    if (sendExpr && sendExpr->channel && sendExpr->value) {
        sendExpr->channel->accept(*this);
        llvm::Value *chan = lastValue;
        sendExpr->value->accept(*this);
        llvm::Value *val = lastValue;
        llvm::Function *sendStub = getStdLibFunction("__tocin_chan_send");
        if (!sendStub) {
            llvm::FunctionType *sendType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context), {chan->getType(), val->getType()}, false);
            sendStub = llvm::Function::Create(sendType, llvm::Function::ExternalLinkage, "__tocin_chan_send", *module);
        }
        builder.CreateCall(sendStub, {chan, val});
        lastValue = nullptr;
    } else {
        lastValue = nullptr;
    }
}

void IRGenerator::visitRuntimeChannelReceiveExpr(void *expr)
{
    // ChannelReceiveExpr: emit a call to a runtime stub for channel receive
    auto recvExpr = static_cast<ast::ChannelReceiveExpr *>(expr);
    if (recvExpr && recvExpr->channel) {
        recvExpr->channel->accept(*this);
        llvm::Value *chan = lastValue;
        llvm::Function *recvStub = getStdLibFunction("__tocin_chan_recv");
        if (!recvStub) {
            llvm::FunctionType *recvType = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0), {chan->getType()}, false);
            recvStub = llvm::Function::Create(recvType, llvm::Function::ExternalLinkage, "__tocin_chan_recv", *module);
        }
        lastValue = builder.CreateCall(recvStub, {chan});
    } else {
        lastValue = nullptr;
    }
}

void IRGenerator::visitRuntimeSelectStmt(void *stmt)
{
    // SelectStmt: emit a call to a runtime stub for select
    auto selectStmt = static_cast<ast::SelectStmt *>(stmt);
    // For now, just emit a call to a stub (real implementation would be more complex)
    llvm::Function *selectStub = getStdLibFunction("__tocin_chan_select");
    if (!selectStub) {
        llvm::FunctionType *selectType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context), {}, false);
        selectStub = llvm::Function::Create(selectType, llvm::Function::ExternalLinkage, "__tocin_chan_select", *module);
    }
    lastValue = builder.CreateCall(selectStub, {});
}

// AST channel visitor methods - empty implementations for now
// AST channel visitor methods - implemented later in the file


void IRGenerator::createMainFunction()
{
    // Create a simple main function that returns 0
    llvm::FunctionType *mainType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context),
        {},
        false);
    
    llvm::Function *mainFunc = llvm::Function::Create(
        mainType,
        llvm::Function::ExternalLinkage,
        "main",
        *module);
    
    // Create a basic block and return 0
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", mainFunc);
    builder.SetInsertPoint(entry);
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
}

// Declare a print function for debugging
void IRGenerator::declarePrintFunction()
{
    // Declare printf function
    llvm::FunctionType *printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
        true);
    
    llvm::Function *printfFunc = llvm::Function::Create(
        printfType,
        llvm::Function::ExternalLinkage,
        "printf",
        *module);
    
    stdLibFunctions["printf"] = printfFunc;
}

// Generic name mangling for template instantiation
std::string IRGenerator::mangleGenericName(const std::string &baseName, const std::vector<ast::TypePtr> &typeArgs)
{
    std::string mangled = baseName + "_";
    for (const auto &type : typeArgs)
    {
        if (type)
        {
            mangled += type->toString() + "_";
        }
    }
    return mangled;
}

// Transform async function to use Future/Promise pattern
llvm::Function *IRGenerator::transformAsyncFunction(ast::FunctionStmt *stmt)
{
    // For now, just return a simple function that returns void
    // This is a placeholder implementation
    llvm::FunctionType *funcType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context),
        {},
        false);
    
    llvm::Function *func = llvm::Function::Create(
        funcType,
        llvm::Function::InternalLinkage,
        stmt->name,
        *module);
    
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    builder.CreateRetVoid();
    
    return func;
}

// Visitor method implementations - basic stubs
void IRGenerator::visitBinaryExpr(ast::BinaryExpr *expr)
{
    if (!expr->left || !expr->right) {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Binary expression missing operands",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate left operand
    expr->left->accept(*this);
    llvm::Value *left = lastValue;
    if (!left) {
        lastValue = nullptr;
        return;
    }

    // Evaluate right operand
    expr->right->accept(*this);
    llvm::Value *right = lastValue;
    if (!right) {
        lastValue = nullptr;
        return;
    }

    // Handle different operators
    switch (expr->op.type) {
    case TokenType::PLUS:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateAdd(left, right, "add");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFAdd(left, right, "fadd");
        } else if (left->getType()->isPointerTy() && right->getType()->isIntegerTy()) {
            // Pointer arithmetic
            lastValue = builder.CreateGEP(left->getType()->getPointerElementType(), left, right, "ptr_add");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot add incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::MINUS:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateSub(left, right, "sub");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFSub(left, right, "fsub");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot subtract incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::STAR:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateMul(left, right, "mul");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFMul(left, right, "fmul");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot multiply incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::SLASH:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateSDiv(left, right, "div");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFDiv(left, right, "fdiv");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot divide incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::MODULO:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateSRem(left, right, "mod");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Modulo only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::EQUAL_EQUAL:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpEQ(left, right, "eq");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpOEQ(left, right, "feq");
        } else if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
            lastValue = builder.CreateICmpEQ(left, right, "ptr_eq");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::BANG_EQUAL:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpNE(left, right, "ne");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpONE(left, right, "fne");
        } else if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
            lastValue = builder.CreateICmpNE(left, right, "ptr_ne");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::LESS:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpSLT(left, right, "lt");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpOLT(left, right, "flt");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::LESS_EQUAL:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpSLE(left, right, "le");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpOLE(left, right, "fle");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::GREATER:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpSGT(left, right, "gt");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpOGT(left, right, "fgt");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::GREATER_EQUAL:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpSGE(left, right, "ge");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpOGE(left, right, "fge");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::AND:
        if (left->getType()->isIntegerTy(1) && right->getType()->isIntegerTy(1)) {
            lastValue = builder.CreateAnd(left, right, "and");
        } else {
            // Convert to boolean if needed
            llvm::Value *leftBool = builder.CreateICmpNE(left, 
                llvm::ConstantInt::get(left->getType(), 0), "left_bool");
            llvm::Value *rightBool = builder.CreateICmpNE(right, 
                llvm::ConstantInt::get(right->getType(), 0), "right_bool");
            lastValue = builder.CreateAnd(leftBool, rightBool, "and");
        }
        break;

    case TokenType::OR:
        if (left->getType()->isIntegerTy(1) && right->getType()->isIntegerTy(1)) {
            lastValue = builder.CreateOr(left, right, "or");
        } else {
            // Convert to boolean if needed
            llvm::Value *leftBool = builder.CreateICmpNE(left, 
                llvm::ConstantInt::get(left->getType(), 0), "left_bool");
            llvm::Value *rightBool = builder.CreateICmpNE(right, 
                llvm::ConstantInt::get(right->getType(), 0), "right_bool");
            lastValue = builder.CreateOr(leftBool, rightBool, "or");
        }
        break;

    case TokenType::BITWISE_AND:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateAnd(left, right, "bitand");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Bitwise AND only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::BITWISE_OR:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateOr(left, right, "bitor");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Bitwise OR only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::BITWISE_XOR:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateXor(left, right, "bitxor");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Bitwise XOR only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::LEFT_SHIFT:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateShl(left, right, "shl");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Left shift only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case TokenType::RIGHT_SHIFT:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateAShr(left, right, "shr");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Right shift only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    default:
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Unsupported binary operator",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        break;
    }
}

void IRGenerator::visitGroupingExpr(ast::GroupingExpr *expr)
{
    if (expr->expression) expr->expression->accept(*this);
}

void IRGenerator::visitVariableExpr(ast::VariableExpr *expr)
{
    // Look up variable in current scope
    llvm::AllocaInst *alloca = lookupVariable(expr->name);
    if (alloca)
    {
        lastValue = builder.CreateLoad(alloca->getAllocatedType(), alloca, expr->name);
    }
    else
    {
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    }
}

void IRGenerator::visitExpressionStmt(ast::ExpressionStmt *stmt)
{
    if (stmt->expression) stmt->expression->accept(*this);
}

void IRGenerator::visitBlockStmt(ast::BlockStmt *stmt)
{
    enterScope();
    for (auto &statement : stmt->statements)
    {
        if (statement) statement->accept(*this);
    }
    exitScope();
}

void IRGenerator::visitImportStmt(ast::ImportStmt *stmt)
{
    // Import statements are handled at compile time, not runtime
}

void IRGenerator::visitMatchStmt(ast::MatchStmt *stmt)
{
    // Basic match statement implementation
    if (stmt->value) stmt->value->accept(*this);
    
    // For now, just visit the first case
    if (!stmt->cases.empty() && stmt->cases[0].second)
    {
        stmt->cases[0].second->accept(*this);
    }
}

void IRGenerator::visitNewExpr(ast::NewExpr *expr)
{
    // Basic new expression - allocate memory
    ast::TypePtr type = expr->getType();
    if (type)
    {
        llvm::Type *llvmType = getLLVMType(type);
        lastValue = builder.CreateAlloca(llvmType);
    }
}

void IRGenerator::visitExportStmt(ast::ExportStmt *stmt)
{
    // Export statements are handled at compile time
}

void IRGenerator::visitModuleStmt(ast::ModuleStmt *stmt)
{
    // Module statements define module scope
    std::string oldModuleName = currentModuleName;
    currentModuleName = stmt->name;
    
    // Visit all statements in the module body
    for (auto &statement : stmt->body)
    {
        if (statement) statement->accept(*this);
    }
    
    currentModuleName = oldModuleName;
}

void IRGenerator::visitAwaitExpr(ast::AwaitExpr *expr)
{
    // Basic await expression - just visit the inner expression
    if (expr->expression) expr->expression->accept(*this);
}

void codegen::IRGenerator::visitGoStmt(ast::GoStmt* stmt) {
    // Generate IR for goroutine launch (go statement)
    
    // First, generate IR for the expression (function call)
    stmt->expression->accept(*this);
    llvm::Value* functionCall = lastValue;
    
    if (!functionCall) {
        errorHandler.reportError(error::ErrorCode::C013_INVALID_SPAWN_OPERATION,
                               "Invalid expression in go statement",
                               std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                               error::ErrorSeverity::ERROR);
        return;
    }
    
    // Get the function type
    llvm::FunctionType* funcType = nullptr;
    if (auto callInst = llvm::dyn_cast<llvm::CallInst>(functionCall)) {
        funcType = callInst->getFunctionType();
    } else {
        errorHandler.reportError(error::ErrorCode::C013_INVALID_SPAWN_OPERATION,
                               "Go statement requires a function call",
                               std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                               error::ErrorSeverity::ERROR);
        return;
    }
    
    // Create a wrapper function that will be executed in a goroutine
    std::string wrapperName = "goroutine_wrapper_" + std::to_string(getNextId());
    llvm::Function* wrapperFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(context), false),
        llvm::Function::InternalLinkage,
        wrapperName,
        module.get()
    );
    
    // Create the wrapper function body
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(context, "entry", wrapperFunc);
    builder.SetInsertPoint(entryBlock);
    
    // Call the original function
    builder.CreateCall(funcType, functionCall);
    
    // Return void
    builder.CreateRetVoid();
    
    // Schedule the wrapper function to run in a goroutine
    // This would typically call a runtime function to schedule the task
    std::vector<llvm::Value*> args = {
        llvm::ConstantExpr::getBitCast(wrapperFunc, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0))
    };
    
    // Get the runtime scheduler function
    llvm::Function* schedulerFunc = module->getFunction("runtime_schedule_goroutine");
    if (!schedulerFunc) {
        // Create the runtime function declaration if it doesn't exist
        llvm::FunctionType* schedulerType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),
            {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
            false
        );
        schedulerFunc = llvm::Function::Create(
            schedulerType,
            llvm::Function::ExternalLinkage,
            "runtime_schedule_goroutine",
            module.get()
        );
    }
    
    // Call the scheduler
    builder.CreateCall(schedulerFunc, args);
    
    // Set the current value to void
    lastValue = llvm::Constant::getNullValue(llvm::Type::getVoidTy(context));
}

void codegen::IRGenerator::visitChannelSendExpr(ast::ChannelSendExpr* expr) {
    // Generate IR for channel send operation (channel <- value)
    
    // Generate IR for the channel
    expr->channel->accept(*this);
    llvm::Value* channel = lastValue;
    
    // Generate IR for the value
    expr->value->accept(*this);
    llvm::Value* value = lastValue;
    
    if (!channel || !value) {
        errorHandler.reportError(error::ErrorCode::C011_INVALID_CHANNEL_OPERATION,
                               "Invalid channel or value in send operation",
                               std::string(expr->token.filename), expr->token.line, expr->token.column,
                               error::ErrorSeverity::ERROR);
        return;
    }
    
    // Get the runtime channel send function
    llvm::Function* sendFunc = module->getFunction("runtime_channel_send");
    if (!sendFunc) {
        // Create the runtime function declaration if it doesn't exist
        llvm::FunctionType* sendType = llvm::FunctionType::get(
            llvm::Type::getInt1Ty(context), // Returns bool (success/failure)
            {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)}, // channel, value
            false
        );
        sendFunc = llvm::Function::Create(
            sendType,
            llvm::Function::ExternalLinkage,
            "runtime_channel_send",
            module.get()
        );
    }
    
    // Cast channel and value to void pointers for the runtime call
    llvm::Value* channelPtr = builder.CreateBitCast(channel, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0));
    llvm::Value* valuePtr = builder.CreateBitCast(value, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0));
    
    // Call the runtime send function
    std::vector<llvm::Value*> args = {channelPtr, valuePtr};
    llvm::Value* result = builder.CreateCall(sendFunc, args);
    
    // Set the current value to the result
    lastValue = result;
}

void codegen::IRGenerator::visitChannelReceiveExpr(ast::ChannelReceiveExpr* expr) {
    // Generate IR for channel receive operation (<-channel)
    
    // Generate IR for the channel
    expr->channel->accept(*this);
    llvm::Value* channel = lastValue;
    
    if (!channel) {
        errorHandler.reportError(error::ErrorCode::C011_INVALID_CHANNEL_OPERATION,
                               "Invalid channel in receive operation",
                               std::string(expr->token.filename), expr->token.line, expr->token.column,
                               error::ErrorSeverity::ERROR);
        return;
    }
    
    // Get the runtime channel receive function
    llvm::Function* receiveFunc = module->getFunction("runtime_channel_receive");
    if (!receiveFunc) {
        // Create the runtime function declaration if it doesn't exist
        llvm::FunctionType* receiveType = llvm::FunctionType::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), // Returns void* (received value)
            {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)}, // channel
            false
        );
        receiveFunc = llvm::Function::Create(
            receiveType,
            llvm::Function::ExternalLinkage,
            "runtime_channel_receive",
            module.get()
        );
    }
    
    // Cast channel to void pointer for the runtime call
    llvm::Value* channelPtr = builder.CreateBitCast(channel, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0));
    
    // Call the runtime receive function
    std::vector<llvm::Value*> args = {channelPtr};
    llvm::Value* result = builder.CreateCall(receiveFunc, args);
    
    // Set the current value to the received value
    lastValue = result;
}

void codegen::IRGenerator::visitSelectStmt(ast::SelectStmt* stmt) {
    // Generate IR for select statement
    
    // Create a function to handle the select logic
    std::string selectFuncName = "select_handler_" + std::to_string(getNextId());
    llvm::Function* selectFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false), // Returns case index
        llvm::Function::InternalLinkage,
        selectFuncName,
        module.get()
    );
    
    // Create the select function body
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(context, "entry", selectFunc);
    builder.SetInsertPoint(entryBlock);
    
    // Get the runtime select function
    llvm::Function* runtimeSelectFunc = module->getFunction("runtime_select");
    if (!runtimeSelectFunc) {
        // Create the runtime function declaration if it doesn't exist
        llvm::FunctionType* selectType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context), // Returns selected case index
            {llvm::Type::getInt32Ty(context)}, // Number of cases
            false
        );
        runtimeSelectFunc = llvm::Function::Create(
            selectType,
            llvm::Function::ExternalLinkage,
            "runtime_select",
            module.get()
        );
    }
    
    // Call the runtime select function with the number of cases
    std::vector<llvm::Value*> args = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), stmt->cases.size())
    };
    llvm::Value* selectedCase = builder.CreateCall(runtimeSelectFunc, args);
    
    // Return the selected case index
    builder.CreateRet(selectedCase);
    
    // Now create the main select logic in the current function
    llvm::BasicBlock* currentBlock = builder.GetInsertBlock();
    llvm::Function* currentFunction = currentBlock->getParent();
    
    // Call the select handler
    std::vector<llvm::Value*> callArgs;
    llvm::Value* caseIndex = builder.CreateCall(selectFunc, callArgs);
    
    // Create a switch statement to handle each case
    llvm::SwitchInst* switchInst = builder.CreateSwitch(caseIndex, nullptr, stmt->cases.size());
    
    // Add cases for each select case
    for (size_t i = 0; i < stmt->cases.size(); ++i) {
        const auto& selectCase = stmt->cases[i];
        
        // Create a basic block for this case
        llvm::BasicBlock* caseBlock = llvm::BasicBlock::Create(context, 
            "select_case_" + std::to_string(i), currentFunction);
        
        // Add the case to the switch
        switchInst->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i), caseBlock);
        
        // Set the insert point to the case block
        builder.SetInsertPoint(caseBlock);
        
        // Execute the case body
        if (selectCase.body) {
            selectCase.body->accept(*this);
        }
        
        // Branch to the end of the select
        llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(context, "select_end", currentFunction);
        builder.CreateBr(endBlock);
        
        // Set the insert point to the end block
        builder.SetInsertPoint(endBlock);
    }
    
    // Set the current value to void
    lastValue = llvm::Constant::getNullValue(llvm::Type::getVoidTy(context));
}

int IRGenerator::getNextId() {
    static int nextId = 0;
    return ++nextId;
}

void codegen::IRGenerator::visitTraitStmt(ast::TraitStmt* stmt) {
    // Generate IR for trait statement
    // Traits are compile-time constructs, so we mainly need to register them
    
    // For now, just visit all method signatures to ensure they're valid
    for (const auto& method : stmt->methods) {
        if (method) {
            // Visit the method to ensure it's syntactically valid
            // In practice, we would register the trait interface
            method->accept(*this);
        }
    }
    
    // Trait statements don't generate runtime code
    lastValue = llvm::Constant::getNullValue(llvm::Type::getVoidTy(context));
}

void codegen::IRGenerator::visitImplStmt(ast::ImplStmt* stmt) {
    // Generate IR for implementation statement
    // This generates the actual method implementations for the trait
    
    // Visit all method implementations
    for (const auto& method : stmt->methods) {
        if (method) {
            // Generate IR for each method implementation
            method->accept(*this);
        }
    }
    
    // Implementation statements don't have a return value
    lastValue = llvm::Constant::getNullValue(llvm::Type::getVoidTy(context));
}

} // namespace codegen
