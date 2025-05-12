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
          currentFunction(nullptr), errorHandler(errorHandler), lastValue(nullptr),
          typeChecker(errorHandler)
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
            // Fix: Use PointerType::get with Int8Ty instead of getInt8PtrTy
            return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
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
                    llvm::PointerType::get(elementType, 0)};

                return llvm::StructType::create(context, listFields, "list");
            }
            else if (genericType->name == "dict")
            {
                // Just a placeholder for now - dictionaries need a more complex implementation
                return llvm::StructType::create(context, "dict");
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

            return llvm::PointerType::get(
                llvm::FunctionType::get(returnType, paramTypes, false), 0);
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
        // Fix: Use PointerType::get with Int8Ty instead of getInt8PtrTy
        llvm::Type *i8PtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
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

    // Variable Declaration: Allocates space and optionally stores an initial value.
    void IRGenerator::visitVariableStmt(ast::VariableStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Variable statement not fully implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
    }

    // Function Declaration: Defines a new function.
    void IRGenerator::visitFunctionStmt(ast::FunctionStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Function declaration not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
    }

    void IRGenerator::visitReturnStmt(ast::ReturnStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Return statement not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
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
    }

    void IRGenerator::visitWhileStmt(ast::WhileStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "While statement not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
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

    void IRGenerator::visitImportStmt(ast::ImportStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Import statement not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
    }

    // Binary expressions like 1 + 2, "hello" + "world"
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
                llvm::Type *pointeeType = llvm::cast<llvm::PointerType>(left->getType())->getArrayElementType();
                lastValue = builder.CreateGEP(pointeeType, left, right, "ptradd");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary +",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            // Check if we're dealing with strings (string concatenation)
            // For now, assume numeric addition
            lastValue = builder.CreateAdd(left, right, "addtmp");
            break;
        case lexer::TokenType::MINUS:
            lastValue = builder.CreateSub(left, right, "subtmp");
            break;
        case lexer::TokenType::STAR:
            lastValue = builder.CreateMul(left, right, "multmp");
            break;
        case lexer::TokenType::SLASH:
            // Check if we're dealing with integers or floats
            lastValue = builder.CreateSDiv(left, right, "divtmp"); // Integer division
            // lastValue = builder.CreateFDiv(left, right, "divtmp"); // Float division
            break;
        // Add other operators as needed
        default:
            // If you need to create a GEP instruction, use the correct signature:
            // lastValue = builder.CreateGEP(elementType, left, {right}, "ptradd");

            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Unsupported binary operator: " + expr->op.value,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
    }

    void IRGenerator::visitGroupingExpr(ast::GroupingExpr *expr)
    {
        expr->expression->accept(*this);
        // lastValue is already set by the nested expression
    }

    void IRGenerator::visitLiteralExpr(ast::LiteralExpr *expr)
    {
        // Based on the literal type, create an appropriate LLVM constant
        switch (expr->literalType)
        {
        case ast::LiteralExpr::LiteralType::INTEGER:
            lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context),
                                               std::stoll(expr->value), true);
            break;
        case ast::LiteralExpr::LiteralType::FLOAT:
            lastValue = llvm::ConstantFP::get(llvm::Type::getDoubleTy(context),
                                              std::stod(expr->value));
            break;
        case ast::LiteralExpr::LiteralType::BOOLEAN:
            lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context),
                                               expr->value == "true" ? 1 : 0);
            break;
        case ast::LiteralExpr::LiteralType::STRING:
            // String literals are more complex in LLVM - this is simplified
            lastValue = builder.CreateGlobalStringPtr(expr->value);
            break;
        case ast::LiteralExpr::LiteralType::NIL:
            // Nil/null is typically represented as a null pointer
            lastValue = llvm::ConstantPointerNull::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0));
            break;
        }
    }

    void IRGenerator::visitUnaryExpr(ast::UnaryExpr *expr)
    {
        expr->right->accept(*this);
        llvm::Value *operand = lastValue;

        if (!operand)
            return;

        if (expr->op.value == "-")
        {
            // Check if we're dealing with integers or floats
            lastValue = builder.CreateNeg(operand, "negtmp");
        }
        else if (expr->op.value == "!")
        {
            lastValue = builder.CreateNot(operand, "nottmp");
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Unsupported unary operator: " + expr->op.value,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
    }

    void IRGenerator::visitVariableExpr(ast::VariableExpr *expr)
    {
        // Look up the variable in the current scope
        auto it = namedValues.find(expr->name);
        if (it != namedValues.end())
        {
            // Variable found, load its value
            lastValue = builder.CreateLoad(it->second->getAllocatedType(), it->second, expr->name);
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Unknown variable name: " + expr->name,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
    }

    void IRGenerator::visitAssignExpr(ast::AssignExpr *expr)
    {
        // Evaluate the right-hand side
        expr->value->accept(*this);
        llvm::Value *value = lastValue;

        if (!value)
            return;

        // Look up the variable in the current scope
        auto it = namedValues.find(expr->name);
        if (it != namedValues.end())
        {
            // Store the new value
            builder.CreateStore(value, it->second);
            lastValue = value; // The result of an assignment is the assigned value
        }
        else
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Unknown variable name in assignment: " + expr->name,
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
    }

    void IRGenerator::visitCallExpr(ast::CallExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Function call not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }

    void IRGenerator::visitGetExpr(ast::GetExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Property access not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }

    void IRGenerator::visitSetExpr(ast::SetExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Property setting not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }

    void IRGenerator::visitLambdaExpr(ast::LambdaExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Lambda expressions not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }

    void IRGenerator::visitListExpr(ast::ListExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "List expressions not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }

    void IRGenerator::visitDictionaryExpr(ast::DictionaryExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Dictionary expressions not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }

    void IRGenerator::visitAwaitExpr(ast::AwaitExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Await expressions not implemented",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
    }

} // namespace ir_generator
