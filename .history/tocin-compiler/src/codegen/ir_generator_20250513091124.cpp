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
        std::string funcName = varExpr->name.lexeme;

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

    // Ensure callee is a function
    if (!callee->getType()->isPointerTy() ||
        (!llvm::cast<llvm::PointerType>(callee->getType())->getElementType()->isFunctionTy()))
    {
        errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                 "Called value is not a function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Get function type
    llvm::FunctionType *funcType = llvm::cast<llvm::FunctionType>(
        llvm::cast<llvm::PointerType>(callee->getType())->getElementType());

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
    // Use accessor methods to get values
    std::string variable = stmt->getVariable();
    ast::TypePtr variableType = stmt->getVariableType();
    
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

    // Get the length of the iterable - assuming struct with length at index 0
    std::vector<llvm::Value *> indices = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)};
        
    // With opaque pointers, we need to know the struct type
    // For simplicity, assume it's a standard list struct type { int64, T* }
    llvm::StructType *iterableStructType = llvm::StructType::get(
        context,
        {llvm::Type::getInt64Ty(context), llvm::PointerType::get(context, 0)}
    );
    
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

    // Get current element - with opaque pointers, we need to specify element type
    index = builder.CreateLoad(llvm::Type::getInt64Ty(context), indexVar);
    llvm::Value *elementPtr = builder.CreateGEP(varType, data, index, "element.ptr");
    llvm::Value *element = builder.CreateLoad(varType, elementPtr, "element");

    // Store element in loop variable
    builder.CreateStore(element, iterVar);

    // Generate loop body using the body() method
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
std::string IRGenerator::inferTypeNameFromValue(llvm::Value *value) {
    // This is a placeholder implementation that would need to be customized
    // based on your type tracking system
        // For now, just check if there's metadata or a name hint
    if (value->hasName()) {
        llvm::StringRef name = value->getName();
        // Try to extract type info from the name if it follows a pattern
        // This is just an example and would need adaptation
        if (name.contains("_class_")) {
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

