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
        lastValue = builder.CreateGlobalStringPtr(processedStr, "str");
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
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
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
        errorHandler.reportError(error::ErrorCode::T009_CANNOT_INFER_TYPE,
                                 "Cannot infer type for variable '" + stmt->name + "' without initializer",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    if (!varType)
    {
        errorHandler.reportError(error::ErrorCode::T004_UNDEFINED_TYPE,
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
        std::string funcName = varExpr->name; // Changed from name.lexeme to name

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
        }
    }

    // Ensure callee is a function - adapted for opaque pointers
    llvm::FunctionType *funcType = nullptr;
    if (callee->getType()->isPointerTy()) {
        // For opaque pointers, we need to get function type differently
        if (auto func = llvm::dyn_cast<llvm::Function>(callee)) {
            funcType = func->getFunctionType();
        } else {
            // Try to get function type from context
            // This is a simplified approach and may need enhancement
            errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                   "Called value is not a function",
                                   "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return;
        }
    } else {
        errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                               "Called value is not a function",
                               "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Check argument count
    if (funcType->getNumParams() != expr->arguments.size())
    {
        errorHandler.reportError(error::ErrorCode::T007_INCORRECT_ARGUMENT_COUNT,
                                 "Wrong number of arguments to function call",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate arguments
    std::vector<llvm::Value *> args;
    for (size_t i = 0; i < expr->arguments.size(); ++i)
    {
        expr->arguments[i]->accept(*this);
        if (!lastValue)
            return;

        // Check argument type compatibility and cast if needed
        if (lastValue->getType() != funcType->getParamType(i))
        {
            if (lastValue->getType()->isIntegerTy() && funcType->getParamType(i)->isIntegerTy())
            {
                lastValue = builder.CreateIntCast(lastValue, funcType->getParamType(i),
                                                  true, "castarg");
            }
            else if ((lastValue->getType()->isFloatTy() || lastValue->getType()->isDoubleTy()) &&
                     (funcType->getParamType(i)->isFloatTy() || funcType->getParamType(i)->isDoubleTy()))
            {
                lastValue = builder.CreateFPCast(lastValue, funcType->getParamType(i), "castarg");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Argument type does not match parameter type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
                return;
            }
        }

        args.push_back(lastValue);
    }

    // Create call instruction
    lastValue = builder.CreateCall(funcType, callee, args);
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
    std::string variable = stmt->variable; // Changed from getVariable()
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
        {llvm::Type::getInt64Ty(context), llvm::PointerType::get(context, 0)}
    );
    
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
    // Evaluate the operand
    expr->right->accept(*this);
    llvm::Value *operand = lastValue;

    if (!operand)
        return;

    switch (expr->op.type)
    {
    case lexer::TokenType::MINUS:
        if (operand->getType()->isIntegerTy())
        {
            lastValue = builder.CreateNeg(operand, "negtmp");
        }
        else if (operand->getType()->isFloatTy() || operand->getType()->isDoubleTy())
        {
            lastValue = builder.CreateFNeg(operand, "fnegtmp");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                     "Invalid operand to unary -",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::BANG:
        if (operand->getType()->isIntegerTy(1))
        {
            // Boolean negation
            lastValue = builder.CreateNot(operand, "nottmp");
        }
        else if (operand->getType()->isIntegerTy())
        {
            // Compare with zero for integers
            lastValue = builder.CreateICmpEQ(
                operand,
                llvm::ConstantInt::get(operand->getType(), 0),
                "nottmp");
        }
        else if (operand->getType()->isFloatTy() || operand->getType()->isDoubleTy())
        {
            // Compare with zero for floating point
            lastValue = builder.CreateFCmpOEQ(
                operand,
                llvm::ConstantFP::get(operand->getType(), 0.0),
                "nottmp");
        }
        else if (operand->getType()->isPointerTy())
        {
            // Compare with null for pointers
            lastValue = builder.CreateICmpEQ(
                operand,
                llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(operand->getType())),
                "nottmp");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                     "Invalid operand to unary !",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    default:
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Unhandled unary operator",
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
    llvm::StructType *listType = llvm::StructType::get(context, listFields);

    // Allocate list struct
    llvm::AllocaInst *listAlloc = builder.CreateAlloca(listType, nullptr, "list");

    // Set length
    llvm::Value *lengthPtr = builder.CreateStructGEP(listType, listAlloc, 0, "list.length");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->elements.size()), lengthPtr);

    // Allocate array for elements
    llvm::Value *arraySize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), expr->elements.size());
    llvm::Value *dataPtr = builder.CreateMalloc(
        llvm::Type::getInt8Ty(context), // Type to allocate
        arraySize,                      // Number of elements
        elementType,                    // Type of elements
        nullptr,                        // Size of each element (nullptr for default)
        nullptr,                        // ArraySize (used above)
        "list.data");                   // Name

    // Store data pointer
    llvm::Value *dataStorePtr = builder.CreateStructGEP(listType, listAlloc, 1, "list.data_ptr");
    builder.CreateStore(dataPtr, dataStorePtr);

    // Store first element
    llvm::Value *elementPtr = builder.CreateGEP(elementType, dataPtr,
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
        elementPtr = builder.CreateGEP(elementType, dataPtr,
                                       llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                       "list.element");
        builder.CreateStore(element, elementPtr);
    }

    // Return list
    lastValue = listAlloc;
}

void IRGenerator::createEmptyList(ast::TypePtr listType)
{
    // Get element type from list type
    llvm::Type *elementType = nullptr;

    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(listType))
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
        llvm::Type::getInt64Ty(context),          // length
        llvm::PointerType::getUnqual(elementType) // data
    };
    llvm::StructType *listType = llvm::StructType::get(context, listFields);

    // Allocate list struct
    llvm::AllocaInst *listAlloc = builder.CreateAlloca(listType, nullptr, "empty_list");

    // Set length to 0
    llvm::Value *lengthPtr = builder.CreateStructGEP(listType, listAlloc, 0, "list.length");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), lengthPtr);

    // Set data pointer to null
    llvm::Value *dataStorePtr = builder.CreateStructGEP(listType, listAlloc, 1, "list.data_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(elementType)), dataStorePtr);

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
    llvm::Value *keysPtr = builder.CreateMalloc(
        llvm::Type::getInt8Ty(context),
        arraySize,
        keyType,
        nullptr,
        nullptr,
        "dict.keys");
    llvm::Value *valuesPtr = builder.CreateMalloc(
        llvm::Type::getInt8Ty(context),
        arraySize,
        valueType,
        nullptr,
        nullptr,
        "dict.values");

    // Store pointers
    llvm::Value *keysStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 1, "dict.keys_ptr");
    builder.CreateStore(keysPtr, keysStorePtr);

    llvm::Value *valuesStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 2, "dict.values_ptr");
    builder.CreateStore(valuesPtr, valuesStorePtr);

    // Store first key-value pair
    llvm::Value *keyPtr = builder.CreateGEP(keyType, keysPtr,
                                            llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                                            "dict.key");
    builder.CreateStore(firstKey, keyPtr);

    llvm::Value *valuePtr = builder.CreateGEP(valueType, valuesPtr,
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
        keyPtr = builder.CreateGEP(keyType, keysPtr,
                                   llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i),
                                   "dict.key");
        builder.CreateStore(key, keyPtr);

        valuePtr = builder.CreateGEP(valueType, valuesPtr,
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
    llvm::StructType *dictType = llvm::StructType::get(context, dictFields);

    // Allocate dictionary struct
    llvm::AllocaInst *dictAlloc = builder.CreateAlloca(dictType, nullptr, "empty_dict");

    // Set size to 0
    llvm::Value *sizePtr = builder.CreateStructGEP(dictType, dictAlloc, 0, "dict.size");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), sizePtr);

    // Set pointers to null
    llvm::Value *keysStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 1, "dict.keys_ptr");
    builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(keyType)), keysStorePtr);

    llvm::Value *valuesStorePtr = builder.CreateStructGEP(dictType, dictAlloc, 2, "dict.values_ptr");
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

    for (const auto &param : method->params)
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
            return;

        paramTypes.push_back(paramType);
    }

    // Create method name with class prefix to avoid name conflicts
    std::string methodName = className + "_" + method->name.lexeme;

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
        argIt->setName(method->params[idx].name.lexeme);
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
    classMethods[className + "." + method->name.lexeme] = function;

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
    // First, check if we have a struct pointer

    // Get the expected class name from the type context if available
    std::string className;
    if (expr->getTypeInfo())
    {
        // Try to get the class name from type info if available
        className = expr->getTypeInfo()->toString();
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
        // Use getElementType() instead of getPointerElementType()
        pointedType = ptrType->getElementType();
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
                    fieldType = ptrType->getElementType();
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
                    base = builder.CreateLoad(ptrType->getElementType(), basePtr);
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

    // Add the first text part
    stringParts.push_back(builder.CreateGlobalStringPtr(textParts[0], "str_part"));

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

        // Add the next text part
        stringParts.push_back(builder.CreateGlobalStringPtr(textParts[i + 1], "str_part"));
    }

    // Concatenate all string parts
    lastValue = concatenateStrings(stringParts);
}

llvm::Value *IRGenerator::convertToString(llvm::Value *value)
{
    // Convert a value to a string representation
    // We'll need to handle different types differently

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
    else if (type->isPointerTy() &&
             type->getPointerElementType()->isIntegerTy(8))
    {
        // Already a string (char*), just return it
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
        return builder.CreateGlobalStringPtr("[ERROR]", "error_str");
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
        return builder.CreateGlobalStringPtr("[ERROR]", "error_str");
    }

    // Handle the base case
    if (strings.empty())
    {
        return builder.CreateGlobalStringPtr("", "empty_str");
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

// Update the variable lookup/assignment logic to use scopes
Handle variable assignment if (auto varExpr = dynamic_cast<ast::VariableExpr *>(expr->target.get()))
{
    std::string name = varExpr->getName();

    // Look up the variable in the current scope
    llvm::AllocaInst *alloca = currentScope->lookup(name);

    if (!alloca)
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Undefined variable: " + name,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
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
            return;
        }
    }

    // Store the initial value
    builder.CreateStore(rhs, alloca);
}
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
