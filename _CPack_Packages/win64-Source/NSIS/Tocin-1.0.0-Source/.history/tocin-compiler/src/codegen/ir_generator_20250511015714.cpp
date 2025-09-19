#include "ir_generator.h"
#include "../ast/ast.h"             // Include full AST definitions
#include "../type/type_checker.h"   // Include full TypeChecker definitions
#include "../error/error_handler.h" // Include ErrorHandler & ErrorCode
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Casting.h> // For llvm::dyn_cast
#include <iostream>               // For debugging output if needed
#include <vector>

namespace ir_generator
{

    IRGenerator::IRGenerator(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> module,
                             error::ErrorHandler &errorHandler)
        : context(context), module(std::move(module)), builder(context), 
          currentFunction(nullptr), errorHandler(errorHandler), lastValue(nullptr)
    {
        declareStdLibFunctions();
    }

    std::unique_ptr<llvm::Module> IRGenerator::generate(ast::StmtPtr ast)
    {
        if (!ast)
        {
            errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                     "Null AST passed to IRGenerator",
                                     "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }

        // Create main function
        llvm::FunctionType *mainType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context), false);
        llvm::Function *mainFunction = llvm::Function::Create(
            mainType, llvm::Function::ExternalLinkage, "main", module.get());

        // Create basic block for the main function
        llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", mainFunction);
        builder.SetInsertPoint(block);

        // Save current function
        currentFunction = mainFunction;

        // Visit the AST
        try
        {
            ast->accept(*this);
        }
        catch (const std::exception &e)
        {
            errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                     "Exception during IR generation: " + std::string(e.what()),
                                     "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }
        catch (...)
        {
            errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                     "Unknown exception during IR generation",
                                     "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }

        // Add return statement to main
        if (!errorHandler.hasFatalErrors())
        {
            builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));
        }

        // Verify the module
        std::string error;
        llvm::raw_string_ostream errorStream(error);
        if (llvm::verifyModule(*module, &errorStream))
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Module verification failed: " + errorStream.str(),
                                     "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }

        return std::move(module);
    }

    llvm::Type *IRGenerator::getLLVMType(ast::TypePtr type)
    {
        if (!type)
        {
            errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                     "Null type passed to getLLVMType",
                                     "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }

        std::string typeName = type->toString();

        // Handle basic types
        if (typeName == "int")
        {
            return llvm::Type::getInt64Ty(context);
        }
        else if (typeName == "float" || typeName == "float64")
        {
            return llvm::Type::getDoubleTy(context);
        }
        else if (typeName == "float32")
        {
            return llvm::Type::getFloatTy(context);
        }
        else if (typeName == "bool")
        {
            return llvm::Type::getInt1Ty(context);
        }
        else if (typeName == "string")
        {
            return llvm::Type::getInt8PtrTy(context);
        }
        else if (typeName == "void")
        {
            return llvm::Type::getVoidTy(context);
        }

        // Handle generic types
        if (auto genericType = std::dynamic_pointer_cast<ast::GenericType>(type))
        {
            if (genericType->name == "list")
            {
                // Implement list as a pointer to an array of elements
                if (genericType->typeArguments.size() != 1)
                {
                    errorHandler.reportError(error::ErrorCode::T004_UNDEFINED_TYPE,
                                             "List requires exactly one type argument",
                                             "", 0, 0, error::ErrorSeverity::ERROR);
                    return nullptr;
                }

                llvm::Type *elementType = getLLVMType(genericType->typeArguments[0]);
                if (!elementType)
                    return nullptr;

                // Create a struct type for the list: { int64_t length, elementType* data }
                std::vector<llvm::Type *> listFields = {
                    llvm::Type::getInt64Ty(context),
                    llvm::PointerType::getUnqual(elementType)};

                return llvm::StructType::create(context, listFields, "list", false);
            }
            else if (genericType->name == "dict")
            {
                // Just a placeholder for now - dictionaries need a more complex implementation
                return llvm::StructType::create(context, "dict", false);
            }
            else if (genericType->name == "Option")
            {
                // Implement Option<T> as a struct { bool hasValue, T value }
                if (genericType->typeArguments.size() != 1)
                {
                    errorHandler.reportError(error::ErrorCode::T004_UNDEFINED_TYPE,
                                             "Option requires exactly one type argument",
                                             "", 0, 0, error::ErrorSeverity::ERROR);
                    return nullptr;
                }

                llvm::Type *valueType = getLLVMType(genericType->typeArguments[0]);
                if (!valueType)
                    return nullptr;

                std::vector<llvm::Type *> optionFields = {
                    llvm::Type::getInt1Ty(context),  // hasValue flag
                    valueType                        // The actual value
                };

                return llvm::StructType::create(context, optionFields, "option", false);
            }
            else if (genericType->name == "Result")
            {
                // Implement Result<T,E> as a struct { bool isOk, T okValue, E errValue }
                if (genericType->typeArguments.size() != 2)
                {
                    errorHandler.reportError(error::ErrorCode::T004_UNDEFINED_TYPE,
                                             "Result requires exactly two type arguments",
                                             "", 0, 0, error::ErrorSeverity::ERROR);
                    return nullptr;
                }

                llvm::Type *okType = getLLVMType(genericType->typeArguments[0]);
                llvm::Type *errType = getLLVMType(genericType->typeArguments[1]);
                if (!okType || !errType)
                    return nullptr;

                std::vector<llvm::Type *> resultFields = {
                    llvm::Type::getInt1Ty(context),  // isOk flag
                    okType,                          // Success value
                    errType                          // Error value
                };

                return llvm::StructType::create(context, resultFields, "result", false);
            }
        }

        // Handle function types
        if (auto functionType = std::dynamic_pointer_cast<ast::FunctionType>(type))
        {
            std::vector<llvm::Type *> paramTypes;
            for (const auto &paramType : functionType->paramTypes)
            {
                llvm::Type *llvmParamType = getLLVMType(paramType);
                if (!llvmParamType)
                    return nullptr;
                paramTypes.push_back(llvmParamType);
            }

            llvm::Type *returnType = getLLVMType(functionType->returnType);
            if (!returnType)
                return nullptr;

            return llvm::PointerType::getUnqual(
                llvm::FunctionType::get(returnType, paramTypes, false));
        }

        // Handle union types (not fully supported yet)
        if (auto unionType = std::dynamic_pointer_cast<ast::UnionType>(type))
        {
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Union types are not fully supported yet: " + unionType->toString(),
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }

        errorHandler.reportError(error::ErrorCode::T004_UNDEFINED_TYPE,
                                 "Unsupported type in IR generation: " + typeName,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return nullptr;
    }

    llvm::FunctionType *IRGenerator::getLLVMFunctionType(ast::TypePtr returnType,
                                                         const std::vector<ast::Parameter> &params)
    {
        llvm::Type *llvmReturnType = getLLVMType(returnType);
        if (!llvmReturnType)
            return nullptr;

        std::vector<llvm::Type *> paramTypes;
        for (const auto &param : params)
        {
            llvm::Type *paramType = getLLVMType(param.type);
            if (!paramType)
                return nullptr;
            paramTypes.push_back(paramType);
        }

        return llvm::FunctionType::get(llvmReturnType, paramTypes, false);
    }

    void IRGenerator::declareStdLibFunctions()
    {
        // Get common LLVM types
        llvm::Type *voidTy = llvm::Type::getVoidTy(context);
        llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
        llvm::Type *doubleTy = llvm::Type::getDoubleTy(context);
        llvm::Type *i8PtrTy = llvm::Type::getInt8PtrTy(context);
        llvm::Type *boolTy = llvm::Type::getInt1Ty(context);

        // Declare print functions
        llvm::FunctionType *printStringType = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
        llvm::Function *printStringFunc = llvm::Function::Create(
            printStringType, llvm::Function::ExternalLinkage, "native_print_string", module.get());
        stdLibFunctions["print_string"] = printStringFunc;

        llvm::FunctionType *printIntType = llvm::FunctionType::get(voidTy, {i64Ty}, false);
        llvm::Function *printIntFunc = llvm::Function::Create(
            printIntType, llvm::Function::ExternalLinkage, "native_print_int", module.get());
        stdLibFunctions["print_int"] = printIntFunc;

        llvm::FunctionType *printFloatType = llvm::FunctionType::get(voidTy, {doubleTy}, false);
        llvm::Function *printFloatFunc = llvm::Function::Create(
            printFloatType, llvm::Function::ExternalLinkage, "native_print_float", module.get());
        stdLibFunctions["print_float"] = printFloatFunc;

        llvm::FunctionType *printBoolType = llvm::FunctionType::get(voidTy, {boolTy}, false);
        llvm::Function *printBoolFunc = llvm::Function::Create(
            printBoolType, llvm::Function::ExternalLinkage, "native_print_bool", module.get());
        stdLibFunctions["print_bool"] = printBoolFunc;

        llvm::FunctionType *printlnType = llvm::FunctionType::get(voidTy, {}, false);
        llvm::Function *printlnFunc = llvm::Function::Create(
            printlnType, llvm::Function::ExternalLinkage, "native_println", module.get());
        stdLibFunctions["println"] = printlnFunc;

        // Add math functions
        llvm::FunctionType *sqrtType = llvm::FunctionType::get(doubleTy, {doubleTy}, false);
        llvm::Function *sqrtFunc = llvm::Function::Create(
            sqrtType, llvm::Function::ExternalLinkage, "native_sqrt", module.get());
        stdLibFunctions["sqrt"] = sqrtFunc;
    }

    llvm::Function *IRGenerator::getStdLibFunction(const std::string &name)
    {
        auto it = stdLibFunctions.find(name);
        if (it == stdLibFunctions.end())
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Standard library function not found: " + name,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }
        return it->second;
    }

    llvm::AllocaInst *IRGenerator::createEntryBlockAlloca(llvm::Function *function,
                                                          const std::string &name,
                                                          llvm::Type *type)
    {
        llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(),
                                     function->getEntryBlock().begin());
        return tmpBuilder.CreateAlloca(type, nullptr, name);
    }

    void IRGenerator::createEnvironment()
    {
        // Used to create a new environment for scoped blocks
        // Save the current environment (to be implemented with scoped variables)
    }

    void IRGenerator::restoreEnvironment()
    {
        // Restore the previous environment (to be implemented with scoped variables)
    }

    // Converts a Tocin::Type to its corresponding llvm::Type.
    llvm::Type *IRGenerator::getLLVMType(std::shared_ptr<type::Type> tocinType)
    {
        if (!tocinType)
        {
            errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                     "Internal Error: Null Tocin type passed to getLLVMType.",
                                     error::ErrorSeverity::FATAL);
            return nullptr;
        }

        // Use type identification
        std::string typeName = tocinType->toString();
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
            return llvm::Type::getInt8PtrTy(context);
        }
        else if (typeName == "void")
        {
            return llvm::Type::getVoidTy(context);
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Unsupported Tocin type encountered in getLLVMType: " + tocinType->toString(),
                                     error::ErrorSeverity::ERROR);
            return nullptr;
        }
    }

    // Block Statement: Executes a sequence of statements within a scope.
    void IRGenerator::visitBlockStmt(ast::BlockStmt *stmt)
    {
        // Save the current environment
        createEnvironment();

        // Process each statement in the block
        for (const auto &statement : stmt->statements)
        {
            statement->accept(*this);
            if (errorHandler.hasFatalErrors())
                return; // Stop if error occurred
        }

        // Restore the previous environment
        restoreEnvironment();
    }

    // Expression Statement: Evaluates an expression, discards the result.
    void IRGenerator::visitExpressionStmt(ast::ExpressionStmt *stmt)
    {
        stmt->expression->accept(*this); // Visit the expression
                                        // The result is stored in 'lastValue' but ignored here unless needed for side effects.
        if (errorHandler.hasFatalErrors())
            return;
    }

    // Print Statement: Evaluates an expression and prints the result.
    void IRGenerator::visitPrintStmt(ast::PrintStmt *stmt)
    {
        // Not implemented yet
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "Print statement not yet implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
    }

    // Variable Declaration: Allocates space and optionally stores an initial value.
    void IRGenerator::visitVarStmt(ast::VarStmt *stmt)
    {
        // Not fully implemented yet - this is a placeholder that will need to be updated
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "Variable declaration not fully implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
    }

    // Function Declaration: Defines a new function.
    void IRGenerator::visitFunctionStmt(ast::FunctionStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "Function declaration not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. Create llvm::FunctionType based on params and return type from TypeChecker.
        // 2. Create llvm::Function object and add it to the module.
        // 3. Create entry basic block.
        // 4. Set builder insert point to the entry block.
        // 5. Create AllocaInst for each parameter and store incoming args. Add to namedValues.
        // 6. Visit the function body (stmt->body).
        // 7. Handle return (ensure terminators, default return for void).
        // 8. Verify the generated function.
    }

    void IRGenerator::visitReturnStmt(ast::ReturnStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "Return statement not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. If stmt->value exists, visit it to get the return value (lastValue).
        // 2. Check if return value type matches function's return type.
        // 3. Generate builder.CreateRet(value) or builder.CreateRetVoid().
        // 4. Ensure the current basic block is terminated.
    }

    void IRGenerator::visitClassStmt(ast::ClassStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "Class declaration not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
    }

    void IRGenerator::visitIfStmt(ast::IfStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "If statement not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. Visit condition (lastValue should be i1).
        // 2. Get current function. Create 'then', 'else' (if exists), and 'merge' basic blocks.
        // 3. Create conditional branch: builder.CreateCondBr(conditionValue, thenBlock, elseOrMergeBlock).
        // 4. Set insert point to 'then' block, visit thenBranch. Create unconditional branch to 'merge'.
        // 5. If elseBranch exists: set insert point to 'else' block, visit elseBranch. Create unconditional branch to 'merge'.
        // 6. Set insert point to 'merge' block. Handle PHI nodes if necessary.
    }

    void IRGenerator::visitWhileStmt(ast::WhileStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "While statement not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. Get current function. Create 'loop_cond', 'loop_body', and 'loop_end' basic blocks.
        // 2. Branch to 'loop_cond'.
        // 3. In 'loop_cond': visit condition (lastValue should be i1). Create CondBr(cond, loop_body, loop_end).
        // 4. In 'loop_body': visit body. Branch back to 'loop_cond'.
        // 5. Set insert point to 'loop_end'.
    }

    void IRGenerator::visitForStmt(ast::ForStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "For statement not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
    }

    void IRGenerator::visitMatchStmt(ast::MatchStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "Match statement not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
    }

    void IRGenerator::visitUnaryExpr(ast::UnaryExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                               "Unary expression not implemented",
                               "", 0, 0, error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. Visit operand (expr->right).
        // 2. Get operand value (lastValue).
        // 3. Based on operator type (MINUS, BANG):
        //    - Generate builder.CreateNeg (for arithmetic negation) or builder.CreateFNeg (for float).
        //    - Generate builder.CreateNot (for boolean negation, usually XOR with true).
        // 4. Check types.
        lastValue = nullptr;
    }

    void IRGenerator::visitBinaryExpr(ast::BinaryExpr *expr)
    {
        expr->left->accept(*this);
        llvm::Value *left = lastValue;
        if (!left)
            return;

        expr->right->accept(*this);
        llvm::Value *right = lastValue;
        if (!right)
            return;

        // Ensure types match
        if (left->getType() != right->getType())
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                   "Operands of binary expression must have the same type",
                                   "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return;
        }

        bool isFloat = left->getType()->isFloatTy() || left->getType()->isDoubleTy();
        bool isInt = left->getType()->isIntegerTy();
        bool isPointer = left->getType()->isPointerTy();

        switch (expr->op.type)
        {
        case lexer::TokenType::PLUS:
            if (isInt)
            {
                lastValue = builder.CreateAdd(left, right, "addtmp");
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFAdd(left, right, "addtmp");
            }
            else if (isPointer && right->getType()->isIntegerTy())
            {
                // Pointer arithmetic
                lastValue = builder.CreateGEP(left, right, "ptradd");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                       "Invalid operands to binary +",
                                       "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            break;
        case lexer::TokenType::MINUS:
            if (isInt)
            {
                lastValue = builder.CreateSub(left, right, "subtmp");
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFSub(left, right, "subtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                       "Invalid operands to binary -",
                                       "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            break;
        case lexer::TokenType::STAR:
            if (isInt)
            {
                lastValue = builder.CreateMul(left, right, "multmp");
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFMul(left, right, "multmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                       "Invalid operands to binary *",
                                       "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            break;
        case lexer::TokenType::SLASH:
            if (isInt)
            {
                lastValue = builder.CreateSDiv(left, right, "divtmp");
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFDiv(left, right, "divtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                       "Invalid operands to binary /",
                                       "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            break;
        case lexer::TokenType::PERCENT:
            if (isInt)
            {
                lastValue = builder.CreateSRem(left, right, "modtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                       "Invalid operands to binary %",
                                       "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            break;
        case lexer::TokenType::EQUAL_EQUAL:
            if (isInt || left->getType()->isPointerTy())
            {
                lastValue = builder.CreateICmpEQ(left, right, "eqtmp");
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFCmpOEQ(left, right, "eqtmp");
                lastValue = builder.CreateFCmpOEQ(left, right, "eqtmp"); // Changed from currentValue to lastValue
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary ==",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                lastValue = nullptr; // Changed from currentValue to lastValue
            }
            break;
        case lexer::TokenType::BANG_EQUAL:
            if (isInt || left->getType()->isPointerTy())
            {
                lastValue = builder.CreateICmpNE(left, right, "neqtmp"); // Changed from currentValue to lastValue
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFCmpONE(left, right, "neqtmp"); // Changed from currentValue to lastValue
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary !=",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                lastValue = nullptr; // Changed from currentValue to lastValue
            }
            break;
        case lexer::TokenType::LESS:
            if (isInt)
            {
                lastValue = builder.CreateICmpSLT(left, right, "lttmp"); // Changed from currentValue to lastValue
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFCmpOLT(left, right, "lttmp"); // Changed from currentValue to lastValue
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary <",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                lastValue = nullptr; // Changed from currentValue to lastValue
            }
            break;
        case lexer::TokenType::LESS_EQUAL:
            if (isInt)
            {
                lastValue = builder.CreateICmpSLE(left, right, "letmp"); // Changed from currentValue to lastValue
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFCmpOLE(left, right, "letmp"); // Changed from currentValue to lastValue
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary <=",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                lastValue = nullptr; // Changed from currentValue to lastValue
            }
            break;
        case lexer::TokenType::GREATER:
            if (isInt)
            {
                lastValue = builder.CreateICmpSGT(left, right, "gttmp"); // Changed from currentValue to lastValue
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFCmpOGT(left, right, "gttmp"); // Changed from currentValue to lastValue
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary >",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                lastValue = nullptr; // Changed from currentValue to lastValue
            }
            break;
        case lexer::TokenType::GREATER_EQUAL:
            if (isInt)
            {
                lastValue = builder.CreateICmpSGE(left, right, "getmp"); // Changed from currentValue to lastValue
            }
            else if (isFloat)
            {
                lastValue = builder.CreateFCmpOGE(left, right, "getmp"); // Changed from currentValue to lastValue
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary >=",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                lastValue = nullptr; // Changed from currentValue to lastValue
            }
            break;
        default:
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Unhandled binary operator: " + expr->op.value,
                                     expr->token.filename, expr->token.line, expr->token.column,
                                     error::ErrorSeverity::ERROR);
            lastValue = nullptr; // Changed from currentValue to lastValue
            break;
        }
    }

    void IRGenerator::visitLogicalExpr(LogicalExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "visitLogicalExpr not implemented.", expr->op.line, expr->op.column,
                                 error::ErrorSeverity::ERROR);
        // Needs to implement short-circuiting logic:
        // For 'and': evaluate left. If false, result is false. If true, evaluate right, result is right's value.
        // For 'or': evaluate left. If true, result is true. If false, evaluate right, result is right's value.
        // Requires creating basic blocks and conditional branches similar to 'if'.
        lastValue = nullptr;
    }

    void IRGenerator::visitCallExpr(CallExpr *expr)
    {
        // --- THIS IS A CRITICAL PART TO IMPLEMENT ---
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "visitCallExpr needs full implementation.", expr->paren.line, expr->paren.column,
                                 error::ErrorSeverity::ERROR);
        lastValue = nullptr; // Indicate failure or no value produced yet

        // General Steps:
        // 1. Determine what is being called (the callee):
        //    - Visit expr->callee.
        //    - Is it a VariableExpr (simple function name)?
        //    - Is it a GetExpr (method call)?
        //    - Is it something else (lambda, function pointer)?

        // 2. If it's a simple VariableExpr (e.g., "println" or "myFunc"):
        //    llvm::Value* calleeValue = lastValue; // Result of visiting expr->callee
        //    std::string calleeName;
        //    if (auto* VE = llvm::dyn_cast<VariableExpr>(expr->callee.get())) { // Check if callee is a simple name
        //        calleeName = VE->name.lexeme;
        //    } else {
        //        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE, "Calling non-identifier functions not supported yet.", expr->paren.line, expr->paren.column);
        //        return;
        //    }

        // 3. Check if it's a Standard Library Function:
        //    llvm::Function* stdLibFunc = getStdLibFunction(calleeName); // Use internal lookup potentially
        //    if (stdLibFunc) {
        //        // It's a standard library call
        //        // a. Visit arguments (expr->arguments) and collect llvm::Value* results in a vector.
        //        std::vector<llvm::Value*> argValues;
        //        for (const auto& arg : expr->arguments) {
        //            arg->accept(this);
        //            if (!lastValue) { /* Handle error */ return; }
        //            argValues.push_back(lastValue);
        //        }
        //
        //        // b. Verify argument count and types against stdLibFunc->getFunctionType().
        //        //    - Get expected types: stdLibFunc->getFunctionType()->params()
        //        //    - Compare counts.
        //        //    - Compare types, potentially insert casts (e.g., int to float if allowed).
        //        if (argValues.size() != stdLibFunc->getFunctionType()->getNumParams()) {
        //             errorHandler.reportError(error::ErrorCode::T007_INCORRECT_ARGUMENT_COUNT, ...); return;
        //        }
        //        // Type checking loop...
        //
        //        // c. Generate the call instruction.
        //        lastValue = builder.CreateCall(stdLibFunc, argValues, "calltmp"); // Name optional
        //
        //    } else {
        //        // 4. It might be a User-Defined Function:
        //        //    a. Look up calleeName in the symbol table (needs to store function pointers/definitions).
        //        //    llvm::Function* userFunc = module.getFunction(calleeName); // Or look up in a function symbol table
        //        //    if (userFunc) {
        //        //        // Similar process: visit args, check types, create call.
        //        //    } else {
        //        //         errorHandler.reportError(error::ErrorCode::T003_UNDEFINED_FUNCTION, ...); return;
        //        //    }
        //    }

        // 5. Handle Method Calls (GetExpr as callee) - More complex.

        // 6. Set lastValue to the result of the call (if not void).
    }

    void IRGenerator::visitGroupingExpr(GroupingExpr *expr)
    {
        // Simply visit the inner expression
        expr->expression->accept(this);
        // The result is already in lastValue
        if (errorHandler.hasFatalErrors())
            lastValue = nullptr;
    }

    // Add implementations or placeholders for all other visit methods...
    // void visitClassStmt(...) override;
    // void visitGetExpr(...) override;
    // void visitSetExpr(...) override;
    // void visitListExpr(...) override;
    // void visitDictionaryExpr(...) override;
    // void visitForStmt(...) override;
    // void visitImportStmt(...) override;
    // void visitLambdaExpr(...) override;
    // void visitAwaitExpr(...) override;
    // void visitMatchStmt(...) override;

} // namespace ir_generator
