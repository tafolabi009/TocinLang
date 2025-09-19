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
            return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        }
        else if (kind == ast::TypeKind::VOID)
        {
            return llvm::Type::getVoidTy(context);
        }
        else
        {
            // For other basic types, use a generic pointer for now
            return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
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
            return llvm::PointerType::get(it->second.classType, 0);
        }

        // Could be an enum or other user-defined type
        // For now, return a generic pointer
        return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
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
        // Nil is represented as a null pointer
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

void IRGenerator::visitVariableExpr(ast::VariableExpr *expr)
{
    // Look up the variable in the symbol table
    auto it = namedValues.find(expr->name);
    if (it == namedValues.end())
    {
        errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                 "Undefined variable '" + expr->name + "'",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Load the value
    lastValue = builder.CreateLoad(it->second->getAllocatedType(), it->second, expr->name);
}

void IRGenerator::visitAssignExpr(ast::AssignExpr *expr)
{
    // Look up the variable in the symbol table
    auto it = namedValues.find(expr->name);
    if (it == namedValues.end())
    {
        errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                 "Undefined variable for assignment '" + expr->name + "'",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate the value
    expr->value->accept(*this);
    if (!lastValue)
        return;

    // Store the value
    builder.CreateStore(lastValue, it->second);

    // Return the value as the result of the assignment
    lastValue = builder.CreateLoad(it->second->getAllocatedType(), it->second);
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
            errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
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
        function->getBasicBlockList().push_back(elseBlock);
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
    function->getBasicBlockList().push_back(continueBlock);
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
    function->getBasicBlockList().push_back(loopBlock);
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
    function->getBasicBlockList().push_back(afterBlock);
    builder.SetInsertPoint(afterBlock);
}

void IRGenerator::visitForStmt(ast::ForStmt *stmt)
{
    // ForStmt structure has been changed, no initializer/condition/increment
    // Modern AST has: variable, variableType, iterable, body
    
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    // Create blocks for loop
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(context, "loop", function);
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(context, "after");

    // Evaluate the iterable expression
    stmt->iterable->accept(*this);
    if (!lastValue)
        return;
    llvm::Value *iterableValue = lastValue;

    // We need to check if the iterable is a valid collection type
    // This is a simplified implementation - you might need a more sophisticated approach
    llvm::Type *iterableType = iterableValue->getType();
    bool isValidIterable = false;

    // Check if it's a pointer to a struct with length and data fields (like list/array)
    if (iterableType->isPointerTy())
    {
        llvm::Type *pointedType = llvm::cast<llvm::PointerType>(iterableType)->getElementType();
        if (pointedType->isStructTy())
        {
            llvm::StructType *structType = llvm::cast<llvm::StructType>(pointedType);
            isValidIterable = (structType->getNumElements() >= 2);
        }
    }

    if (!isValidIterable)
    {
        errorHandler.reportError(error::ErrorCode::T003_UNDEFINED_FUNCTION,
                                "Expression is not iterable",
                                "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    // Allocate variable for the iteration
    llvm::Type *varType = getLLVMType(stmt->variableType);
    llvm::AllocaInst *iterVar = builder.CreateAlloca(varType, nullptr, stmt->variable);
    
    // Store variable in the symbol table
    namedValues[stmt->variable] = iterVar;

    // Create a counter variable
    llvm::AllocaInst *indexVar = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "loop.index");
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0), indexVar);

    // Get the length of the iterable
    std::vector<llvm::Value *> indices = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)
    };
    llvm::Value *lengthPtr = builder.CreateGEP(
        llvm::cast<llvm::PointerType>(iterableType)->getElementType(),
        iterableValue, indices, "length.ptr");
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
        llvm::cast<llvm::PointerType>(iterableType)->getElementType(),
        iterableValue, indices, "data.ptr");
    llvm::Value *data = builder.CreateLoad(
        llvm::PointerType::get(varType, 0), dataPtr, "data");

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
    namedValues.erase(stmt->variable);
}

void IRGenerator::visitUnaryExpr(ast::UnaryExpr *expr)
{
    // Evaluate the operand
    expr->right->accept(*this);
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), fieldIndex)};

                llvm::Value *fieldPtr = builder.CreateGEP(
                    pointedType, object, indices, "field." + expr->name);

                // Load the field value
                if (auto ptrType = llvm::dyn_cast<llvm::PointerType>(fieldPtr->getType()))
                {
                    lastValue = builder.CreateLoad(ptrType->getElementType(), fieldPtr);
                }
                else
                {
                    lastValue = builder.CreateLoad(fieldPtr->getType(), fieldPtr);
                }
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
                        currentFunction, "this", object->getType());
                    builder.CreateStore(object, basePtr);

                    // Load it back to attach metadata
                    llvm::Value *base = nullptr;
                    if (auto ptrType = llvm::dyn_cast<llvm::PointerType>(basePtr->getType()))
                    {
                        base = builder.CreateLoad(ptrType->getElementType(), basePtr);
                    }
                    else
                    {
                        base = builder.CreateLoad(basePtr->getType(), basePtr);
                    }

                    // Store the this pointer in a global for the method to access
                    // This is a simple approach - a better one would be to pass it as a parameter
                    // But that would require changing all function signatures
                    builder.CreateStore(base, methodThis);
                    return;
                }
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

void IRGenerator::visitNewExpr(ast::NewExpr *expr)
{
    // Check if it's a class type
    if (auto typeExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr->getTypeExpr()))
    {
        std::string typeName = typeExpr->name.lexeme;
        auto classIt = classTypes.find(typeName);

        if (classIt != classTypes.end())
        {
            // Allocate the class
            llvm::StructType *classType = classIt->second.classType;
            llvm::AllocaInst *classAlloca = builder.CreateAlloca(classType, nullptr, "new_" + typeName);

            // Initialize with constructor if available
            std::string constructorName = typeName + "." + typeName;
            auto ctorIt = classMethods.find(constructorName);

            if (ctorIt != classMethods.end())
            {
                // Call constructor
                std::vector<llvm::Value *> args;
                args.push_back(classAlloca); // this pointer

                // Add any constructor arguments
                if (expr->getArguments())
                {
                    for (const auto &arg : *expr->getArguments())
                    {
                        arg->accept(*this);
                        if (!lastValue)
                            return;
                        args.push_back(lastValue);
                    }
                }

                // Call the constructor
                builder.CreateCall(ctorIt->second, args);
            }

            // Initialize base class if there is one
            if (classIt->second.baseClass)
            {
                // Allocate base class
                llvm::AllocaInst *baseAlloca = builder.CreateAlloca(classIt->second.baseClass, nullptr, "base_" + typeName);

                // Store base class pointer in the first field
                llvm::Value *baseField = builder.CreateStructGEP(classType, classAlloca, 0, "base_field");
                builder.CreateStore(baseAlloca, baseField);

                // Initialize base class with its constructor if available
                // (would need to know the base class name)
            }

            lastValue = classAlloca;
            return;
        }
    }

    // Fall back to the original implementation for non-class types
    expr->getTypeExpr()->accept(*this);
    llvm::Value *typeExpr = lastValue;
    if (!typeExpr)
        return;

    llvm::Type *llvmType = typeExpr->getType();
    if (!llvmType)
    {
        lastValue = nullptr;
        return;
    }

    // Compute allocation size
    llvm::Value *allocSize = nullptr;
    if (expr->getSizeExpr())
    {
        // Array allocation: size = sizeof(T) * count
        expr->getSizeExpr()->accept(*this);
        llvm::Value *count = lastValue;
        llvm::Type *int64Ty = llvm::Type::getInt64Ty(context);
        llvm::Constant *typeSize = llvm::ConstantExpr::getSizeOf(llvmType);
        if (typeSize->getType() != int64Ty)
            typeSize = llvm::ConstantExpr::getTruncOrBitCast(typeSize, int64Ty);
        if (count->getType() != int64Ty)
            count = builder.CreateIntCast(count, int64Ty, false);
        allocSize = builder.CreateMul(typeSize, count, "arraysize");
    }
    else
    {
        // Single object: size = sizeof(T)
        llvm::Constant *typeSize = llvm::ConstantExpr::getSizeOf(llvmType);
        llvm::Type *int64Ty = llvm::Type::getInt64Ty(context);
        if (typeSize->getType() != int64Ty)
            typeSize = llvm::ConstantExpr::getTruncOrBitCast(typeSize, int64Ty);
        allocSize = typeSize;
    }

    // Declare or get malloc
    llvm::Function *mallocFunc = module->getFunction("malloc");
    if (!mallocFunc)
    {
        llvm::FunctionType *mallocType = llvm::FunctionType::get(
            llvm::PointerType::get(context, 0), // Opaque pointer type
            {llvm::Type::getInt64Ty(context)},
            false);
        mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", module.get());
    }

    llvm::Value *rawPtr = builder.CreateCall(mallocFunc, {allocSize}, "newmem");
    // No need for bitcast with opaque pointers
    lastValue = rawPtr;
}

void IRGenerator::visitMatchStmt(ast::MatchStmt *stmt)
{
    // Evaluate the expression being matched
    stmt->getExpression()->accept(*this);
    llvm::Value *matchValue = lastValue;

    if (!matchValue)
    {
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                 "Match expression evaluation failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    // Save the current insertion block
    llvm::BasicBlock *currentBlock = builder.GetInsertBlock();
    llvm::Function *function = currentBlock->getParent();

    // Create a continuation block to branch to after the match
    llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(context, "match.end", function);

    // Create blocks for each match case
    std::vector<llvm::BasicBlock *> caseBlocks;
    for (const auto &matchCase : stmt->getCases())
    {
        caseBlocks.push_back(llvm::BasicBlock::Create(context, "match.case", function));
    }

    // Add a default block if needed
    llvm::BasicBlock *defaultBlock = nullptr;
    if (caseBlocks.empty())
    {
        defaultBlock = endBlock;
    }
    else
    {
        defaultBlock = llvm::BasicBlock::Create(context, "match.default", function);
    }

    // Store the match value in a temporary variable to avoid recomputation
    llvm::AllocaInst *matchTemp = createEntryBlockAlloca(function, "match.value", matchValue->getType());
    builder.CreateStore(matchValue, matchTemp);

    // Create the pattern visitor
    patternVisitor = std::make_unique<PatternVisitor>(*this, builder.CreateLoad(matchTemp->getAllocatedType(), matchTemp));

    // Process each case with pattern matching
    for (size_t i = 0; i < stmt->getCases().size(); i++)
    {
        const auto &matchCase = stmt->getCases()[i];
        llvm::BasicBlock *nextCaseBlock = (i < caseBlocks.size() - 1) ? caseBlocks[i + 1] : defaultBlock;

        // Set up the pattern matching for this case
        builder.SetInsertPoint(caseBlocks[i]);

        // Generate pattern matching code
        bool patternSuccess = patternVisitor->visitPattern(
            matchCase->getPattern(),
            caseBlocks[i], // Success block
            nextCaseBlock  // Failure block
        );

        if (patternSuccess)
        {
            // Generate the body of the case if pattern matched
            // Get bindings that were created during pattern matching
            const auto &bindings = patternVisitor->getBindings();

            // Save the current environment
            std::map<std::string, llvm::AllocaInst *> savedNamedValues(namedValues);

            // Add the pattern bindings to the environment
            for (const auto &binding : bindings)
            {
                llvm::Type *bindingType = binding.second->getType();
                llvm::AllocaInst *alloca = createEntryBlockAlloca(function, binding.first, bindingType);
                builder.CreateStore(binding.second, alloca);
                namedValues[binding.first] = alloca;
            }

            // Generate code for the case body
            matchCase->getBody()->accept(*this);

            // Restore the environment
            namedValues = savedNamedValues;

            // Branch to the end block
            if (!builder.GetInsertBlock()->getTerminator())
            {
                builder.CreateBr(endBlock);
            }
        }
    }

    // Generate default case
    builder.SetInsertPoint(defaultBlock);

    // Branch to end block unless the block already has a terminator
    if (defaultBlock != endBlock && !builder.GetInsertBlock()->getTerminator())
    {
        builder.CreateBr(endBlock);
    }

    // Continue codegen at the end block
    builder.SetInsertPoint(endBlock);
}

void IRGenerator::visitWildcardPattern(ast::WildcardPattern *pattern)
{
    // Wildcard pattern always matches, no code generation needed
    lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
}

void IRGenerator::visitLiteralPattern(ast::LiteralPattern *pattern)
{
    // Evaluate the literal
    pattern->getLiteral()->accept(*this);
    llvm::Value *literalValue = lastValue;

    if (!literalValue)
    {
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                 "Pattern literal evaluation failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }

    // Create the comparison
    lastValue = builder.CreateICmpEQ(lastValue, literalValue, "literal.cmp");
}

void IRGenerator::visitVariablePattern(ast::VariablePattern *pattern)
{
    // Variable pattern always matches and binds the value
    // The binding is handled by the PatternVisitor class
    lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
}

void IRGenerator::visitConstructorPattern(ast::ConstructorPattern *pattern)
{
    // This is more complex and depends on the type system
    // We need to:
    // 1. Check if the value is of the correct constructor type
    // 2. Extract the constructor arguments
    // 3. Apply the argument patterns recursively

    // Get the constructor information from the type system
    std::string constructorName = pattern->getName();

    // Create the type check
    // For now, we'll implement a simple approach for enums/variants
    // that checks a tag field

    llvm::Value *value = lastValue; // The value being matched

    // Get the tag field from the value structure
    llvm::Value *tagValue = nullptr;
    if (value->getType()->isPointerTy() &&
        value->getType()->getPointerElementType()->isStructTy())
    {

        // Extract the tag value (assuming first field is the tag)
        tagValue = builder.CreateStructGEP(
            value->getType()->getPointerElementType(),
            value, 0, "variant.tag");
        tagValue = builder.CreateLoad(
            builder.getInt32Ty(), tagValue, "tag.value");
    }

    if (!tagValue)
    {
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                 "Cannot match constructor pattern on non-variant type",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        return;
    }

    // Compare the tag with the expected constructor tag
    // The tag value should be determined based on the constructor name
    // For simplicity, let's assume we have a mapping
    int expectedTag = 0; // This should be determined from constructor name

    llvm::Value *tagMatch = builder.CreateICmpEQ(
        tagValue,
        llvm::ConstantInt::get(builder.getInt32Ty(), expectedTag),
        "tag.match");

    // If we have argument patterns, we need to check those too
    if (!pattern->getArguments().empty())
    {
        // Extract the payload fields for the matched arguments
        // This is dependent on the specific variant structure

        // For each argument pattern
        for (size_t i = 0; i < pattern->getArguments().size(); i++)
        {
            // Extract the corresponding field from the variant
            llvm::Value *fieldValue = builder.CreateStructGEP(
                value->getType()->getPointerElementType(),
                value, i + 1, // +1 to skip tag field
                "field" + std::to_string(i));

            fieldValue = builder.CreateLoad(
                fieldValue->getType()->getPointerElementType(),
                fieldValue);

            // Match against the argument pattern
            llvm::Value *savedLastValue = lastValue;
            lastValue = fieldValue;

            pattern->getArguments()[i]->accept(*this);
            llvm::Value *argMatch = lastValue;

            lastValue = savedLastValue;

            // Combine with the overall match result
            tagMatch = builder.CreateAnd(tagMatch, argMatch, "combined.match");
        }
    }

    lastValue = tagMatch;
}

void IRGenerator::visitTuplePattern(ast::TuplePattern *pattern)
{
    // Match a tuple value against a tuple pattern
    llvm::Value *value = lastValue; // The value being matched

    // Verify we're matching against a tuple
    if (!value->getType()->isPointerTy() ||
        !value->getType()->getPointerElementType()->isStructTy())
    {
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                 "Cannot match tuple pattern on non-tuple type",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        return;
    }

    // Start with a match success
    llvm::Value *tupleMatch = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);

    // Match each element pattern against the corresponding tuple element
    for (size_t i = 0; i < pattern->getElements().size(); i++)
    {
        // Extract the tuple element
        llvm::Value *elementValue = builder.CreateStructGEP(
            value->getType()->getPointerElementType(),
            value, i, "tuple.element" + std::to_string(i));

        elementValue = builder.CreateLoad(
            elementValue->getType()->getPointerElementType(),
            elementValue);

        // Match against the element pattern
        llvm::Value *savedLastValue = lastValue;
        lastValue = elementValue;

        pattern->getElements()[i]->accept(*this);
        llvm::Value *elementMatch = lastValue;

        lastValue = savedLastValue;

        // Combine with the overall match result
        tupleMatch = builder.CreateAnd(tupleMatch, elementMatch, "tuple.match");
    }

    lastValue = tupleMatch;
}

void IRGenerator::visitStructPattern(ast::StructPattern *pattern)
{
    // Match a struct value against a struct pattern
    llvm::Value *value = lastValue; // The value being matched

    // Verify we're matching against a struct
    if (!value->getType()->isPointerTy() ||
        !value->getType()->getPointerElementType()->isStructTy())
    {
        errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                 "Cannot match struct pattern on non-struct type",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        return;
    }

    // Get the struct type from the pattern
    std::string structTypeName = pattern->getTypeName();

    // Check if the struct type matches
    // This would require type information which might not be directly available
    // Assuming the type check passes for now

    // Start with a match success
    llvm::Value *structMatch = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);

    // Match each field pattern against the corresponding struct field
    for (const auto &field : pattern->getFields())
    {
        // Find the field index based on the name
        // This requires type information that might not be directly available
        // For now, assume we have a mapping of field names to indices
        int fieldIndex = 0; // Should be determined from field.name

        // Extract the struct field
        llvm::Value *fieldValue = builder.CreateStructGEP(
            value->getType()->getPointerElementType(),
            value, fieldIndex, "struct.field." + field.name);

        fieldValue = builder.CreateLoad(
            fieldValue->getType()->getPointerElementType(),
            fieldValue);

        // Match against the field pattern
        llvm::Value *savedLastValue = lastValue;
        lastValue = fieldValue;

        field.pattern->accept(*this);
        llvm::Value *fieldMatch = lastValue;

        lastValue = savedLastValue;

        // Combine with the overall match result
        structMatch = builder.CreateAnd(structMatch, fieldMatch, "struct.match");
    }

    lastValue = structMatch;
}

void IRGenerator::visitOrPattern(ast::OrPattern *pattern)
{
    // Match either of the two patterns
    llvm::Value *value = lastValue; // The value being matched

    // Match against the left pattern
    llvm::Value *savedLastValue = lastValue;
    pattern->getLeft()->accept(*this);
    llvm::Value *leftMatch = lastValue;

    // Restore the value for matching against the right pattern
    lastValue = savedLastValue;

    // Match against the right pattern
    pattern->getRight()->accept(*this);
    llvm::Value *rightMatch = lastValue;

    // Combine with OR logic
    lastValue = builder.CreateOr(leftMatch, rightMatch, "or.match");
}

// PatternVisitor implementation
bool PatternVisitor::visitPattern(ast::PatternPtr pattern, llvm::BasicBlock *successBlock, llvm::BasicBlock *failBlock)
{
    // Dispatch to the appropriate pattern visitor method based on the pattern kind
    switch (pattern->getKind())
    {
    case ast::Pattern::Kind::WILDCARD:
        return visitWildcardPattern(static_cast<ast::WildcardPattern *>(pattern.get()),
                                    successBlock, failBlock);
    case ast::Pattern::Kind::LITERAL:
        return visitLiteralPattern(static_cast<ast::LiteralPattern *>(pattern.get()),
                                   successBlock, failBlock);
    case ast::Pattern::Kind::VARIABLE:
        return visitVariablePattern(static_cast<ast::VariablePattern *>(pattern.get()),
                                    successBlock, failBlock);
    case ast::Pattern::Kind::CONSTRUCTOR:
        return visitConstructorPattern(static_cast<ast::ConstructorPattern *>(pattern.get()),
                                       successBlock, failBlock);
    case ast::Pattern::Kind::TUPLE:
        return visitTuplePattern(static_cast<ast::TuplePattern *>(pattern.get()),
                                 successBlock, failBlock);
    case ast::Pattern::Kind::STRUCT:
        return visitStructPattern(static_cast<ast::StructPattern *>(pattern.get()),
                                  successBlock, failBlock);
    case ast::Pattern::Kind::OR:
        return visitOrPattern(static_cast<ast::OrPattern *>(pattern.get()),
                              successBlock, failBlock);
    default:
        // Unknown pattern type
        return false;
    }
}

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

bool PatternVisitor::visitLiteralPattern(ast::LiteralPattern *pattern,
                                         llvm::BasicBlock *successBlock,
                                         llvm::BasicBlock *failBlock)
{
    llvm::IRBuilder<> &builder = generator.builder;

    // Evaluate the literal
    pattern->getLiteral()->accept(generator);
    llvm::Value *literalValue = generator.lastValue;

    // Compare with the value being matched
    llvm::Value *cmp = nullptr;
    if (literalValue->getType()->isFloatingPointTy())
    {
        // For floating point literals
        cmp = builder.CreateFCmpOEQ(valueToMatch, literalValue, "literal.cmp");
    }
    else
    {
        // For integer, boolean, etc.
        cmp = builder.CreateICmpEQ(valueToMatch, literalValue, "literal.cmp");
    }

    // Create conditional branch
    builder.CreateCondBr(cmp, successBlock, failBlock);

    bindingSuccess = false; // Literal patterns don't create bindings
    return true;
}

bool PatternVisitor::visitVariablePattern(ast::VariablePattern *pattern,
                                          llvm::BasicBlock *successBlock,
                                          llvm::BasicBlock *failBlock)
{
    llvm::IRBuilder<> &builder = generator.builder;

    // Variable pattern always matches and creates a binding
    bindings[pattern->getName()] = valueToMatch;
    builder.CreateBr(successBlock);

    bindingSuccess = true;
    return true;
}

bool PatternVisitor::visitConstructorPattern(ast::ConstructorPattern *pattern,
                                             llvm::BasicBlock *successBlock,
                                             llvm::BasicBlock *failBlock)
{
    llvm::IRBuilder<> &builder = generator.builder;
    llvm::LLVMContext &context = builder.getContext();

    // Get the constructor information from the type system
    std::string constructorName = pattern->getName();

    // Extract the tag from the variant/enum
    llvm::Value *tagValue = nullptr;
    if (valueToMatch->getType()->isPointerTy() &&
        valueToMatch->getType()->getPointerElementType()->isStructTy())
    {

        // Extract the tag value (assuming first field is the tag)
        tagValue = builder.CreateStructGEP(
            valueToMatch->getType()->getPointerElementType(),
            valueToMatch, 0, "variant.tag");
        tagValue = builder.CreateLoad(
            builder.getInt32Ty(), tagValue, "tag.value");
    }

    if (!tagValue)
    {
        // Cannot match on this type
        builder.CreateBr(failBlock);
        bindingSuccess = false;
        return false;
    }

    // Compare the tag with the expected constructor tag
    // This would require type information to map constructor names to tag values
    int expectedTag = 0; // This should be determined from constructor name

    llvm::Value *tagMatch = builder.CreateICmpEQ(
        tagValue,
        llvm::ConstantInt::get(builder.getInt32Ty(), expectedTag),
        "tag.match");

    // Create a new basic block for processing arguments if the tag matches
    llvm::Function *function = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *argsBlock = nullptr;

    if (!pattern->getArguments().empty())
    {
        argsBlock = llvm::BasicBlock::Create(context, "constructor.args", function);
        builder.CreateCondBr(tagMatch, argsBlock, failBlock);
        builder.SetInsertPoint(argsBlock);

        // Process each argument pattern
        bool allArgsMatch = true;
        for (size_t i = 0; i < pattern->getArguments().size(); i++)
        {
            // Extract the corresponding field from the variant
            llvm::Value *fieldValue = builder.CreateStructGEP(
                valueToMatch->getType()->getPointerElementType(),
                valueToMatch, i + a1, // +1 to skip tag field
                "field" + std::to_string(i));

            fieldValue = builder.CreateLoad(
                fieldValue->getType()->getPointerElementType(),
                fieldValue);

            // Create blocks for this argument's success/failure
            llvm::BasicBlock *argSuccessBlock = llvm::BasicBlock::Create(
                context, "arg" + std::to_string(i) + ".success", function);
            llvm::BasicBlock *argFailBlock = failBlock;

            // Temporarily replace valueToMatch
            llvm::Value *savedValue = valueToMatch;
            valueToMatch = fieldValue;

            // Visit the argument pattern
            bool argSuccess = visitPattern(
                pattern->getArguments()[i],
                argSuccessBlock,
                argFailBlock);

            // Restore valueToMatch
            valueToMatch = savedValue;

            if (!argSuccess)
            {
                allArgsMatch = false;
                break;
            }

            // Continue with the next argument
            builder.SetInsertPoint(argSuccessBlock);
        }

        if (allArgsMatch)
        {
            // All arguments matched, branch to success block
            builder.CreateBr(successBlock);
            bindingSuccess = true;
            return true;
        }
    }
    else
    {
        // No arguments, just branch on the tag match
        builder.CreateCondBr(tagMatch, successBlock, failBlock);
        bindingSuccess = true;
        return true;
    }

    bindingSuccess = false;
    return false;
}

bool PatternVisitor::visitTuplePattern(ast::TuplePattern *pattern,
                                       llvm::BasicBlock *successBlock,
                                       llvm::BasicBlock *failBlock)
{
    llvm::IRBuilder<> &builder = generator.builder;
    llvm::LLVMContext &context = builder.getContext();

    // Verify we're matching against a tuple
    if (!valueToMatch->getType()->isPointerTy() ||
        !valueToMatch->getType()->getPointerElementType()->isStructTy())
    {
        // Not a tuple type
        builder.CreateBr(failBlock);
        bindingSuccess = false;
        return false;
    }

    // Get the number of tuple elements
    llvm::StructType *tupleType = llvm::cast<llvm::StructType>(
        valueToMatch->getType()->getPointerElementType());
    unsigned numElements = tupleType->getNumElements();

    // Check if number of elements matches
    if (numElements != pattern->getElements().size())
    {
        // Tuple size mismatch
        builder.CreateBr(failBlock);
        bindingSuccess = false;
        return false;
    }

    // Process each element pattern
    llvm::Function *function = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *currentBlock = builder.GetInsertBlock();

    for (size_t i = 0; i < pattern->getElements().size(); i++)
    {
        // Extract the tuple element
        llvm::Value *elementValue = builder.CreateStructGEP(
            tupleType, valueToMatch, i, "tuple.element" + std::to_string(i));

        elementValue = builder.CreateLoad(
            elementValue->getType()->getPointerElementType(),
            elementValue);

        // Match against the element pattern
        llvm::Value *savedLastValue = valueToMatch;
        valueToMatch = elementValue;

        pattern->getElements()[i]->accept(*this);
        llvm::Value *elementMatch = lastValue;

        valueToMatch = savedLastValue;

        // Combine with the overall match result
        llvm::Value *combinedMatch = builder.CreateAnd(
            tagMatch, elementMatch, "combined.match");

        tagMatch = combinedMatch;
    }

    // Create conditional branch
    builder.CreateCondBr(tagMatch, successBlock, failBlock);

    bindingSuccess = true;
    return true;
}

bool PatternVisitor::visitStructPattern(ast::StructPattern *pattern,
                                        llvm::BasicBlock *successBlock,
                                        llvm::BasicBlock *failBlock)
{
    llvm::IRBuilder<> &builder = generator.builder;
    llvm::LLVMContext &context = builder.getContext();

    // Verify we're matching against a struct
    if (!valueToMatch->getType()->isPointerTy() ||
        !valueToMatch->getType()->getPointerElementType()->isStructTy())
    {
        // Not a struct type
        builder.CreateBr(failBlock);
        bindingSuccess = false;
        return false;
    }

    // Get the struct type
    std::string structTypeName = pattern->getTypeName();

    // Check if the struct type matches
    // This would require type information which might not be directly available
    // Assuming the type check passes for now

    // Process each field pattern
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    for (const auto &field : pattern->getFields())
    {
        // Find the field index based on the name
        // This requires type information that might not be directly available
        // For now, assume we have a mapping of field names to indices
        int fieldIndex = 0; // Should be determined from field.name

        // Extract the struct field
        llvm::Value *fieldValue = builder.CreateStructGEP(
            valueToMatch->getType()->getPointerElementType(),
            valueToMatch, fieldIndex, "struct.field." + field.name);

        fieldValue = builder.CreateLoad(
            fieldValue->getType()->getPointerElementType(),
            fieldValue);

        // Match against the field pattern
        llvm::Value *savedLastValue = valueToMatch;
        valueToMatch = fieldValue;

        field.pattern->accept(*this);
        llvm::Value *fieldMatch = lastValue;

        valueToMatch = savedLastValue;

        // Combine with the overall match result
        llvm::Value *combinedMatch = builder.CreateAnd(
            tagMatch, fieldMatch, "combined.match");

        tagMatch = combinedMatch;
    }

    // Create conditional branch
    builder.CreateCondBr(tagMatch, successBlock, failBlock);

    bindingSuccess = true;
    return true;
}

bool PatternVisitor::visitOrPattern(ast::OrPattern *pattern,
                                    llvm::BasicBlock *successBlock,
                                    llvm::BasicBlock *failBlock)
{
    llvm::IRBuilder<> &builder = generator.builder;
    llvm::LLVMContext &context = builder.getContext();
    llvm::Function *function = builder.GetInsertBlock()->getParent();

    // Create a block for the right pattern
    llvm::BasicBlock *rightBlock = llvm::BasicBlock::Create(
        context, "or.right", function);

    // Try the left pattern first
    bool leftSuccess = visitPattern(
        pattern->getLeft(),
        successBlock, // If left pattern matches, go to success
        rightBlock);  // If left pattern fails, try right pattern

    // Check the right pattern
    builder.SetInsertPoint(rightBlock);
    bool rightSuccess = visitPattern(
        pattern->getRight(),
        successBlock, // If right pattern matches, go to success
        failBlock);   // If right pattern fails, go to fail

    // Either branch worked if we got here
    bindingSuccess = leftSuccess || rightSuccess;
    return bindingSuccess;
}

// Generic type instantiation implementation
llvm::StructType *IRGenerator::instantiateGenericType(const std::string &name,
                                                      const std::vector<ast::TypePtr> &typeArgs)
{
    // Create a mangled name for this generic instantiation
    std::string mangledName = mangleGenericName(name, typeArgs);

    // Check if we've already instantiated this type
    auto it = genericInstances.find(mangledName);
    if (it != genericInstances.end())
    {
        return it->second.instantiatedType;
    }

    // We need to create a new instantiation
    // First, look up the template class definition
    // This would require accessing the AST for the class definition
    // For simplicity, we'll create a placeholder type for now

    // Create a new struct type
    llvm::StructType *instantiatedType = llvm::StructType::create(context, mangledName);

    // Store in the instantiation map before setting body (for recursive types)
    GenericInstance instance;
    instance.baseName = name;
    instance.typeArgs = typeArgs;
    instance.instantiatedType = instantiatedType;
    genericInstances[mangledName] = instance;

    // Create field types based on the template parameters
    std::vector<llvm::Type *> fieldTypes;

    // TODO: Properly substitute type parameters in field types
    // This requires more complex type substitution logic

    // For demonstration, create a simple struct with the type arguments
    for (const auto &typeArg : typeArgs)
    {
        fieldTypes.push_back(getLLVMType(typeArg));
    }

    // Set the body of the struct
    instantiatedType->setBody(fieldTypes);

    return instantiatedType;
}

llvm::Function *IRGenerator::instantiateGenericFunction(ast::FunctionStmt *func,
                                                        const std::vector<ast::TypePtr> &typeArgs)
{
    // Create a mangled name for this generic instantiation
    std::string mangledName = mangleGenericName(func->name, typeArgs);

    // Check if the function already exists in the module
    llvm::Function *instantiatedFunc = module->getFunction(mangledName);
    if (instantiatedFunc)
    {
        return instantiatedFunc;
    }

    // We need to create a specialized version of this function

    // First, create a substitution map from type parameters to concrete types
    std::map<std::string, ast::TypePtr> substitutionMap;
    for (size_t i = 0; i < func->typeParameters.size() && i < typeArgs.size(); i++)
    {
        substitutionMap[func->typeParameters[i].getName()] = typeArgs[i];
    }

    // Apply substitution to parameter types and return type
    std::vector<ast::Parameter> specializedParams;
    for (const auto &param : func->parameters)
    {
        // Apply type substitution to the parameter type
        ast::TypePtr specializedType = substituteTypeParameters(param.type, substitutionMap);
        specializedParams.push_back(ast::Parameter(param.name, specializedType));
    }

    // Apply substitution to return type
    ast::TypePtr specializedReturnType = substituteTypeParameters(func->returnType, substitutionMap);

    // Create the LLVM function type
    llvm::FunctionType *funcType = getLLVMFunctionType(specializedReturnType, specializedParams);

    // Create the function
    llvm::Function *function = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        mangledName,
        *module);

    // Set parameter names
    unsigned idx = 0;
    for (auto &arg : function->args())
    {
        if (idx < specializedParams.size())
        {
            arg.setName(specializedParams[idx].name);
        }
        idx++;
    }

    // Create entry block
    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    builder.SetInsertPoint(entryBlock);

    // Save the current function and create a new environment
    llvm::Function *savedFunction = currentFunction;
    currentFunction = function;
    std::map<std::string, llvm::AllocaInst *> savedNamedValues(namedValues);
    namedValues.clear();

    // Create allocas for all parameters
    idx = 0;
    for (auto &arg : function->args())
    {
        llvm::AllocaInst *alloca = createEntryBlockAlloca(
            function, arg.getName().str(), arg.getType());
        builder.CreateStore(&arg, alloca);
        namedValues[arg.getName().str()] = alloca;
        idx++;
    }

    // Generate the specialized function body
    if (func->body)
    {
        func->body->accept(*this);
    }

    // If we don't have a terminator (e.g., return), add one
    if (!builder.GetInsertBlock()->getTerminator())
    {
        if (function->getReturnType()->isVoidTy())
        {
            builder.CreateRetVoid();
        }
        else
        {
            // Create a default return value
            llvm::Value *defaultValue = createDefaultValue(function->getReturnType());
            builder.CreateRet(defaultValue);
        }
    }

    // Restore the previous environment
    namedValues = savedNamedValues;
    currentFunction = savedFunction;

    // Verify the function
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Function verification failed for " + mangledName,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        function->eraseFromParent();
        return nullptr;
    }

    return function;
}

std::string IRGenerator::mangleGenericName(const std::string &baseName,
                                           const std::vector<ast::TypePtr> &typeArgs)
{
    std::string result = baseName + "_";

    for (const auto &typeArg : typeArgs)
    {
        // Append a sanitized type name
        std::string typeName = typeArg->toString();

        // Replace invalid characters
        std::replace(typeName.begin(), typeName.end(), '<', '_');
        std::replace(typeName.begin(), typeName.end(), '>', '_');
        std::replace(typeName.begin(), typeName.end(), ',', '_');
        std::replace(typeName.begin(), typeName.end(), ' ', '_');

        result += typeName + "_";
    }

    return result;
}

// Helper to substitute type parameters with concrete types
ast::TypePtr IRGenerator::substituteTypeParameters(
    ast::TypePtr type,
    const std::map<std::string, ast::TypePtr> &substitutions)
{
    if (!type)
    {
        return nullptr;
    }

    // Handle type parameter references
    if (auto typeParam = std::dynamic_pointer_cast<ast::TypeParameterType>(type))
    {
        auto it = substitutions.find(typeParam->getName());
        if (it != substitutions.end())
        {
            return it->second;
        }
        return type; // Keep as is if no substitution found
    }

    // Handle generic types (recursive substitution)
    if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
    {
        std::vector<ast::TypePtr> newArgs;
        for (const auto &arg : genericType->typeArguments)
        {
            newArgs.push_back(substituteTypeParameters(arg, substitutions));
        }
        return std::make_shared<ast::GenericType>(
            genericType->token, genericType->name, newArgs);
    }

    // Handle function types
    if (auto funcType = std::dynamic_pointer_cast<ast::FunctionType>(type))
    {
        std::vector<ast::TypePtr> newParamTypes;
        for (const auto &paramType : funcType->paramTypes)
        {
            newParamTypes.push_back(substituteTypeParameters(paramType, substitutions));
        }
        auto newReturnType = substituteTypeParameters(funcType->returnType, substitutions);
        return std::make_shared<ast::FunctionType>(
            funcType->token, newParamTypes, newReturnType);
    }

    // Handle union types
    if (auto unionType = std::dynamic_pointer_cast<ast::UnionType>(type))
    {
        std::vector<ast::TypePtr> newTypes;
        for (const auto &t : unionType->types)
        {
            newTypes.push_back(substituteTypeParameters(t, substitutions));
        }
        return std::make_shared<ast::UnionType>(unionType->token, newTypes);
    }

    // Simple types and others pass through unchanged
    return type;
}

// Helper to create default values for return types
llvm::Value *IRGenerator::createDefaultValue(llvm::Type *type)
{
    if (type->isIntegerTy())
    {
        return llvm::ConstantInt::get(type, 0);
    }
    else if (type->isFloatingPointTy())
    {
        return llvm::ConstantFP::get(type, 0.0);
    }
    else if (type->isPointerTy())
    {
        return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
    }
    else if (type->isStructTy())
    {
        // Create a zero-initialized struct
        return llvm::ConstantAggregateZero::get(type);
    }
    else if (type->isArrayTy())
    {
        // Create a zero-initialized array
        return llvm::ConstantAggregateZero::get(type);
    }
    else if (type->isVectorTy())
    {
        // Create a zero-initialized vector
        return llvm::ConstantAggregateZero::get(type);
    }

    // For other types, return null
    return llvm::UndefValue::get(type);
}

void IRGenerator::visitAwaitExpr(ast::AwaitExpr *expr)
{
    // Check if we're in an async context
    if (!isInAsyncContext)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "await used outside of async function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Generate code for the expression being awaited
    expr->expression->accept(*this);
    llvm::Value *futureValue = lastValue;

    if (!futureValue)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Await expression evaluation failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Check if the expression is a Future
    // This assumes a Future<T> type with a get() method

    // Get the Future's value type
    llvm::Type *futureValueType = nullptr;
    if (futureValue->getType()->isPointerTy() &&
        futureValue->getType()->getPointerElementType()->isStructTy())
    {

        llvm::StructType *futureType = llvm::cast<llvm::StructType>(
            futureValue->getType()->getPointerElementType());

        // Try to extract the value type from Future<T>
        // This depends on the Future implementation
        // For simplicity, assume the value is the first field
        if (futureType->getNumElements() > 0)
        {
            futureValueType = futureType->getElementType(0);
        }
    }

    if (!futureValueType)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Cannot await a non-Future type",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Call the Future's get() method to extract the value
    // This is where we would emit the state machine transformation
    // for suspending and resuming the function

    // In a full implementation, we'd create a suspension point here
    // and transform the function into a state machine

    // For now, just call get() which will block
    // This is not truly async, just a placeholder
    std::vector<llvm::Value *> args;
    llvm::Function *getFunc = module->getFunction("Future_get");
    if (!getFunc)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Future_get method not found",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    lastValue = builder.CreateCall(getFunc, {futureValue}, "await.result");
}

llvm::Function *IRGenerator::transformAsyncFunction(ast::FunctionStmt *func)
{
    // Create a specialized async function that returns a Future<ReturnType>

    // 1. Create the Future return type
    ast::TypePtr returnType = func->returnType;
    llvm::Type *llvmReturnType = getLLVMType(returnType);
    if (!llvmReturnType)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Cannot determine return type for async function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return nullptr;
    }

    // 2. Get or create the Future type for this return type
    llvm::StructType *futureType = getFutureType(llvmReturnType);

    // 3. Create a new function that returns the Future type
    std::string asyncFuncName = func->name + "$async";
    std::vector<llvm::Type *> paramTypes;

    for (const auto &param : func->parameters)
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
        {
            errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                     "Invalid parameter type in async function",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }
        paramTypes.push_back(paramType);
    }

    llvm::FunctionType *asyncFuncType = llvm::FunctionType::get(
        llvm::PointerType::get(futureType, 0), // Return Future<T>*
        paramTypes,
        false // Not varargs
    );

    llvm::Function *asyncFunc = llvm::Function::Create(
        asyncFuncType,
        llvm::Function::ExternalLinkage,
        asyncFuncName,
        *module);

    // Set parameter names
    unsigned idx = 0;
    for (auto &arg : asyncFunc->args())
    {
        if (idx < func->parameters.size())
        {
            arg.setName(func->parameters[idx].name);
        }
        idx++;
    }

    // 4. Create the function body that builds the state machine
    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", asyncFunc);
    builder.SetInsertPoint(entryBlock);

    // Save current function state
    llvm::Function *savedFunction = currentFunction;
    bool savedIsAsync = isInAsyncContext;
    currentFunction = asyncFunc;
    isInAsyncContext = true;

    // Create a Promise for the return value
    // Create a Promise<T> on the heap
    llvm::Function *createPromiseFunc = getStdLibFunction("Promise_create");
    if (!createPromiseFunc)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Promise_create function not found",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        // Restore state and return
        currentFunction = savedFunction;
        isInAsyncContext = savedIsAsync;
        return nullptr;
    }

    llvm::Value *promise = builder.CreateCall(createPromiseFunc, {}, "promise");

    // Create a Future from the Promise
    llvm::Function *getFutureFunc = getStdLibFunction("Promise_getFuture");
    if (!getFutureFunc)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Promise_getFuture function not found",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        // Restore state and return
        currentFunction = savedFunction;
        isInAsyncContext = savedIsAsync;
        return nullptr;
    }

    llvm::Value *future = builder.CreateCall(getFutureFunc, {promise}, "future");

    // Save the current environment
    std::map<std::string, llvm::AllocaInst *> savedNamedValues(namedValues);
    namedValues.clear();

    // Create allocas for all parameters
    idx = 0;
    for (auto &arg : asyncFunc->args())
    {
        llvm::AllocaInst *alloca = createEntryBlockAlloca(
            asyncFunc, arg.getName().str(), arg.getType());
        builder.CreateStore(&arg, alloca);
        namedValues[arg.getName().str()] = alloca;
        idx++;
    }

    // Store Promise in the function for use with await expressions
    llvm::AllocaInst *promiseAlloca = createEntryBlockAlloca(
        asyncFunc, "$promise", promise->getType());
    builder.CreateStore(promise, promiseAlloca);

    // 5. Generate the body code, which will transform await points
    if (func->body)
    {
        func->body->accept(*this);
    }

    // 6. Create the return statement that returns the Future
    if (!builder.GetInsertBlock()->getTerminator())
    {
        builder.CreateRet(future);
    }

    // 7. Restore the original environment
    namedValues = savedNamedValues;
    currentFunction = savedFunction;
    isInAsyncContext = savedIsAsync;

    // 8. Verify the function
    if (llvm::verifyFunction(*asyncFunc, &llvm::errs()))
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Async function verification failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        asyncFunc->eraseFromParent();
        return nullptr;
    }

    return asyncFunc;
}

// Helper function to get or create a Future type for a given value type
llvm::StructType *IRGenerator::getFutureType(llvm::Type *valueType)
{
    // Create a unique name for this Future instantiation
    std::string typeName = "Future";
    if (valueType->isVoidTy())
    {
        typeName += "_Void";
    }
    else
    {
        std::string valueTypeName;
        llvm::raw_string_ostream os(valueTypeName);
        valueType->print(os);
        os.flush();

        // Sanitize the type name
        std::replace(valueTypeName.begin(), valueTypeName.end(), ' ', '_');
        std::replace(valueTypeName.begin(), valueTypeName.end(), '*', 'P');
        std::replace(valueTypeName.begin(), valueTypeName.end(), '<', '_');
        std::replace(valueTypeName.begin(), valueTypeName.end(), '>', '_');

        typeName += "_" + valueTypeName;
    }

    // Check if we've already created this Future type
    llvm::StructType *futureType = llvm::StructType::getTypeByName(context, typeName);
    if (futureType)
    {
        return futureType;
    }

    // Create a new Future type
    futureType = llvm::StructType::create(context, typeName);

    // Define the Future's fields:
    // - Value (or placeholder for void)
    // - State (enum: pending, fulfilled, rejected)
    // - Error (if any)
    // - Continuations list
    std::vector<llvm::Type *> fields;

    // 1. Value field - use valueType or a placeholder for void
    if (valueType->isVoidTy())
    {
        // Use a char as placeholder for void
        fields.push_back(builder.getInt8Ty());
    }
    else
    {
        fields.push_back(valueType);
    }

    // 2. State enum (int32)
    fields.push_back(builder.getInt32Ty());

    // 3. Error pointer (generic error type)
    fields.push_back(llvm::PointerType::get(builder.getInt8Ty(), 0));

    // 4. Continuations list (pointer to array of callbacks)
    fields.push_back(llvm::PointerType::get(builder.getInt8Ty(), 0));

    // Set the body of the struct
    futureType->setBody(fields);

    return futureType;
}

// Helper function to get or create a Promise type for a given value type
llvm::StructType *IRGenerator::getPromiseType(llvm::Type *valueType)
{
    // Create a unique name for this Promise instantiation
    std::string typeName = "Promise";
    if (valueType->isVoidTy())
    {
        typeName += "_Void";
    }
    else
    {
        std::string valueTypeName;
        llvm::raw_string_ostream os(valueTypeName);
        valueType->print(os);
        os.flush();

        // Sanitize the type name
        std::replace(valueTypeName.begin(), valueTypeName.end(), ' ', '_');
        std::replace(valueTypeName.begin(), valueTypeName.end(), '*', 'P');
        std::replace(valueTypeName.begin(), valueTypeName.end(), '<', '_');
        std::replace(valueTypeName.begin(), valueTypeName.end(), '>', '_');

        typeName += "_" + valueTypeName;
    }

    // Check if we've already created this Promise type
    llvm::StructType *promiseType = llvm::StructType::getTypeByName(context, typeName);
    if (promiseType)
    {
        return promiseType;
    }

    // Create a new Promise type
    promiseType = llvm::StructType::create(context, typeName);

    // Define the Promise's fields:
    // - Future (pointer to the associated Future)
    // - Other implementation details
    std::vector<llvm::Type *> fields;

    // 1. Future pointer
    llvm::StructType *futureType = getFutureType(valueType);
    fields.push_back(llvm::PointerType::get(futureType, 0));

    // 2. Other implementation fields as needed
    // ...

    // Set the body of the struct
    promiseType->setBody(fields);

    return promiseType;
}

void IRGenerator::visitImportStmt(ast::ImportStmt *stmt)
{
    // Get the module name
    std::string moduleName = stmt->getModuleName();

    // Check if the module exists
    // This would require runtime module lookup or compile-time knowledge

    // For demonstration, we'll create a stub implementation
    // that assumes the module exists

    // Save the current module name
    std::string savedModuleName = currentModuleName;

    // For each imported symbol
    for (const auto &symbol : stmt->getSymbols())
    {
        // If there's an alias, bind the imported symbol to the alias
        std::string localName = symbol.second.empty() ? symbol.first : symbol.second;

        // Resolve the module symbol
        llvm::Value *value = getModuleSymbol(moduleName, symbol.first);

        if (!value)
        {
            errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                     "Cannot import undefined symbol: " +
                                         moduleName + "." + symbol.first,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            continue;
        }

        // Add the symbol to the current scope
        // Depending on what the value is, we might need to handle it differently
        if (auto func = llvm::dyn_cast<llvm::Function>(value))
        {
            // For functions, just add them to the module with a potentially new name
            // if there's an alias
            if (localName != symbol.first)
            {
                // Create an alias or wrapper function
                // For simplicity, we'll create a wrapper function
                std::vector<llvm::Type *> paramTypes;
                for (auto &arg : func->args())
                {
                    paramTypes.push_back(arg.getType());
                }

                llvm::FunctionType *funcType = llvm::FunctionType::get(
                    func->getReturnType(), paramTypes, func->isVarArg());

                llvm::Function *aliasFunc = llvm::Function::Create(
                    funcType, llvm::Function::ExternalLinkage, localName, *module);

                // Create the wrapper body
                llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", aliasFunc);
                builder.SetInsertPoint(block);

                // Call the original function
                std::vector<llvm::Value *> args;
                for (auto &arg : aliasFunc->args())
                {
                    args.push_back(&arg);
                }

                llvm::Value *result = builder.CreateCall(func, args);
                if (func->getReturnType()->isVoidTy())
                {
                    builder.CreateRetVoid();
                }
                else
                {
                    builder.CreateRet(result);
                }
            }
        }
        else if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(value))
        {
            // For global variables, create a reference or alias
            // For simplicity, we'll create an alias in the current module
            if (localName != symbol.first)
            {
                new llvm::GlobalAlias(
                    global->getValueType(),
                    global->getAddressSpace(),
                    llvm::GlobalValue::ExternalLinkage,
                    localName,
                    global,
                    module.get());
            }
        }
        else
        {
            // For other symbols, report an error
            errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                     "Unsupported import symbol type: " +
                                         moduleName + "." + symbol.first,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
        }
    }

    // Restore the current module name
    currentModuleName = savedModuleName;
}

void IRGenerator::visitExportStmt(ast::ExportStmt *stmt)
{
    // Add symbols to the module's export list
    for (const auto &symbol : stmt->getSymbols())
    {
        // Resolve the symbol in the current scope
        llvm::Value *value = nullptr;

        // Check if it's a local variable
        auto it = namedValues.find(symbol);
        if (it != namedValues.end())
        {
            // Load the value from the alloca
            value = builder.CreateLoad(
                it->second->getAllocatedType(),
                it->second,
                symbol);
        }
        else
        {
            // Check if it's a function
            value = module->getFunction(symbol);

            if (!value)
            {
                // Check if it's a global variable
                value = module->getGlobalVariable(symbol);
            }
        }

        if (!value)
        {
            errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                     "Cannot export undefined symbol: " + symbol,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            continue;
        }

        // Add the symbol to the module's export list
        addModuleSymbol(currentModuleName, symbol, value);
    }
}

void IRGenerator::visitModuleStmt(ast::ModuleStmt *stmt)
{
    // Save the current module name
    std::string savedModuleName = currentModuleName;

    // Set the new module name
    currentModuleName = stmt->getName();

    // Process the module body
    stmt->getBody()->accept(*this);

    // Restore the previous module name
    currentModuleName = savedModuleName;
}

// Module system helper functions
void IRGenerator::addModuleSymbol(const std::string &moduleName,
                                  const std::string &symbolName,
                                  llvm::Value *value)
{
    // Ensure the module exists in our map
    if (moduleSymbols.find(moduleName) == moduleSymbols.end())
    {
        moduleSymbols[moduleName] = std::map<std::string, llvm::Value *>();
    }

    // Add or update the symbol
    moduleSymbols[moduleName][symbolName] = value;

    // For global access, we might also want to create a qualified name in the module
    std::string qualifiedName = getQualifiedName(moduleName, symbolName);

    // Handle different types of values
    if (auto func = llvm::dyn_cast<llvm::Function>(value))
    {
        // For functions, create an alias or wrapper with the qualified name
        // if it doesn't already exist
        if (!module->getFunction(qualifiedName))
        {
            // Create a function alias
            llvm::Function *aliasFunc = llvm::Function::Create(
                func->getFunctionType(),
                llvm::Function::ExternalLinkage,
                qualifiedName,
                *module);

            // Link it to the original function
            aliasFunc->setLinkage(llvm::Function::LinkOnceAnyLinkage);
            aliasFunc->setAliasee(func);
        }
    }
    else if (auto global = llvm::dyn_cast<llvm::GlobalVariable>(value))
    {
        // For global variables, create an alias with the qualified name
        if (!module->getGlobalVariable(qualifiedName))
        {
            new llvm::GlobalAlias(
                global->getValueType(),
                global->getAddressSpace(),
                llvm::GlobalValue::ExternalLinkage,
                qualifiedName,
                global,
                module.get());
        }
    }
    // Other types of values would need special handling
}

llvm::Value *IRGenerator::getModuleSymbol(const std::string &moduleName,
                                          const std::string &symbolName)
{
    // Check if we have this module in our map
    auto moduleIt = moduleSymbols.find(moduleName);
    if (moduleIt == moduleSymbols.end())
    {
        return nullptr;
    }

    // Check if the module has this symbol
    auto symbolIt = moduleIt->second.find(symbolName);
    if (symbolIt == moduleIt->second.end())
    {
        // Try to find it by qualified name
        std::string qualifiedName = getQualifiedName(moduleName, symbolName);

        // Check for a function with this qualified name
        llvm::Function *func = module->getFunction(qualifiedName);
        if (func)
        {
            return func;
        }

        // Check for a global variable with this qualified name
        llvm::GlobalVariable *global = module->getGlobalVariable(qualifiedName);
        if (global)
        {
            return global;
        }

        return nullptr;
    }

    return symbolIt->second;
}

std::string IRGenerator::getQualifiedName(const std::string &moduleName,
                                          const std::string &symbolName)
{
    // Create a unique qualified name that won't clash with regular names
    return moduleName + "$" + symbolName;
}

// Add memory management implementations
void IRGenerator::visitNewExpr(ast::NewExpr *expr)
{
    // Get the type to allocate
    ast::TypePtr type = expr->getType();
    llvm::Type *llvmType = getLLVMType(type);

    if (!llvmType)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Invalid type for new expression",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Call malloc to allocate memory
    llvm::Function *mallocFunc = getStdLibFunction("malloc");
    if (!mallocFunc)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "malloc function not found",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Get the size of the type using sizeof operator
    llvm::Value *typeSize = builder.CreateCall(
        module->getFunction("sizeof"),
        {builder.getInt32(0)}, // Placeholder since LLVM doesn't have a direct sizeof
        "sizeof");

    // Call malloc with the type size
    llvm::Value *rawMemory = builder.CreateCall(
        mallocFunc,
        {typeSize},
        "malloc.call");

    // Cast the raw memory to the appropriate type pointer
    lastValue = builder.CreateBitCast(
        rawMemory,
        llvm::PointerType::get(llvmType, 0),
        "new.ptr");

    // If the expression has constructor arguments, call the constructor
    if (!expr->getArguments().empty())
    {
        // Generate constructor call
        std::vector<llvm::Value *> ctorArgs;
        ctorArgs.push_back(lastValue); // 'this' pointer as first argument

        // Add the constructor arguments
        for (const auto &arg : expr->getArguments())
        {
            arg->accept(*this);
            if (!lastValue)
            {
                return;
            }
            ctorArgs.push_back(lastValue);
        }

        // Call the constructor
        std::string ctorName = "";
        if (auto typeRef = std::dynamic_pointer_cast<ast::TypeReference>(type))
        {
            ctorName = typeRef->getName() + "_constructor";
        }
        else
        {
            ctorName = "constructor"; // Generic fallback
        }

        llvm::Function *ctorFunc = module->getFunction(ctorName);
        if (ctorFunc)
        {
            builder.CreateCall(ctorFunc, ctorArgs);
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                     "Constructor not found: " + ctorName,
                                     "", 0, 0, error::ErrorSeverity::WARNING);
        }
    }
}

void IRGenerator::visitDeleteExpr(ast::DeleteExpr *expr)
{
    // Generate code for the expression to delete
    expr->expression->accept(*this);
    llvm::Value *ptr = lastValue;

    if (!ptr || !ptr->getType()->isPointerTy())
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Invalid pointer for delete expression",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Call the destructor if needed
    if (expr->callDestructor)
    {
        // Get the type
        llvm::Type *pointeeType = ptr->getType()->getPointerElementType();
        if (pointeeType->isStructTy())
        {
            // Try to call the destructor
            std::string typeName = "";
            if (llvm::StructType *structType = llvm::dyn_cast<llvm::StructType>(pointeeType))
            {
                typeName = structType->getName().str();
            }

            // Remove type namespace prefixes if any
            size_t lastDot = typeName.find_last_of('.');
            if (lastDot != std::string::npos)
            {
                typeName = typeName.substr(lastDot + 1);
            }

            std::string dtorName = typeName + "_destructor";
            llvm::Function *dtorFunc = module->getFunction(dtorName);

            if (dtorFunc)
            {
                builder.CreateCall(dtorFunc, {ptr});
            }
        }
    }

    // Call free to release the memory
    llvm::Function *freeFunc = getStdLibFunction("free");
    if (!freeFunc)
    {
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "free function not found",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Cast the pointer to void* for free
    llvm::Value *voidPtr = builder.CreateBitCast(
        ptr,
        llvm::PointerType::get(builder.getInt8Ty(), 0),
        "void.ptr");

    builder.CreateCall(freeFunc, {voidPtr});

    // delete expression doesn't return a value
    lastValue = nullptr;
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
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
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
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
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
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
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
        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
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
    errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
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
void IRGenerator::visitVariableExpr(ast::VariableExpr *expr)
{
    std::string name = expr->getName();

    // Look up the variable in the current scope
    llvm::AllocaInst *alloca = currentScope->lookup(name);

    if (!alloca)
    {
        // Check if it's a global variable
        llvm::GlobalVariable *global = module->getGlobalVariable(name);
        if (global)
        {
            lastValue = builder.CreateLoad(global->getValueType(), global, name.c_str());
            return;
        }

        errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
                                 "Undefined variable: " + name,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Load the variable value
    lastValue = builder.CreateLoad(alloca->getAllocatedType(), alloca, name.c_str());
}

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
        std::string name = varExpr->getName();

        // Look up the variable in the current scope
        llvm::AllocaInst *alloca = currentScope->lookup(name);

        if (!alloca)
        {
            errorHandler.reportError(error::ErrorCode::C004_CODEGEN_ERROR,
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
