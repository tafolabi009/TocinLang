#include "ir_generator.h"
#include "../ast/ast.h"             // Include full AST definitions
#include "../type/type_checker.h"   // Include full TypeChecker definitions
#include "../error/error_handler.h" // Include ErrorHandler & ErrorCode
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Casting.h" // For llvm::dyn_cast
#include <iostream>               // For debugging output if needed
#include <vector>

namespace codegen
{

    IRGenerator::IRGenerator(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> module,
                             error::ErrorHandler &errorHandler)
        : context(context), module(std::move(module)), builder(context), errorHandler(errorHandler)
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

    // --- Helper Functions ---

    // Helper to create an Alloca instruction in the entry block of the function.
    // Used for mutable variables.
    llvm::AllocaInst *IRGenerator::createEntryBlockAlloca(llvm::Function *theFunction,
                                                          const std::string &varName,
                                                          llvm::Type *varType)
    {
        // Create a temporary builder pointing to the beginning of the function's entry block
        llvm::IRBuilder<> TmpB(&theFunction->getEntryBlock(),
                               theFunction->getEntryBlock().begin());
        // CreateAlloca(Type*, AddrSpace*, ArraySize*, Name*)
        // ArraySize = nullptr means allocate space for a single element.
        return TmpB.CreateAlloca(varType, nullptr, varName.c_str());
    }

    // Converts a Tocin::Type to its corresponding llvm::Type.
    // *** THIS NEEDS TO BE FULLY IMPLEMENTED ***
    llvm::Type *IRGenerator::getLLVMType(std::shared_ptr<Tocin::Type> tocinType)
    {
        if (!tocinType)
        {
            errorHandler.reportError(error::ErrorCode::C003_TYPECHECK_ERROR, // Or a specific Codegen error
                                     "Internal Error: Null Tocin type passed to getLLVMType.",
                                     error::ErrorSeverity::FATAL);
            return nullptr;
        }

        // Use RTTI or a type kind enum to determine the Tocin type
        // For now, handle basic built-in types based on name (simplistic)
        // TODO: Replace string comparison with proper type checking (e.g., using TypeKind enum)
        if (tocinType->toString() == "int")
        {                                           // Assuming Type has a toString() or similar identifier
            return llvm::Type::getInt64Ty(context); // Use i64 for Tocin 'int'
        }
        else if (tocinType->toString() == "float")
        {
            return llvm::Type::getDoubleTy(context); // Use double for Tocin 'float'
        }
        else if (tocinType->toString() == "bool")
        {
            return llvm::Type::getInt1Ty(context); // Use i1 for Tocin 'bool'
        }
        else if (tocinType->toString() == "string")
        {
            // Represent Tocin strings as pointers to characters (char*)
            // This assumes a null-terminated C-string representation at runtime.
            // More complex string types might need a struct (pointer + length).
            return llvm::Type::getInt8PtrTy(context);
        }
        else if (tocinType->toString() == "void")
        {
            return llvm::Type::getVoidTy(context);
        }
        // Add cases for:
        // - User-defined classes (llvm::StructType*)
        // - Lists/Arrays (llvm::StructType* or llvm::ArrayType* depending on implementation)
        // - Dictionaries/Maps (llvm::StructType*)
        // - Function types (llvm::FunctionType*)
        // - Null/Any/Unknown types?

        else
        {
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Unsupported Tocin type encountered in getLLVMType: " + tocinType->toString(),
                                     error::ErrorSeverity::ERROR);
            return nullptr;
        }
    }

    // --- Standard Library Function Declaration ---

    void IRGenerator::declareStandardLibraryFunctions()
    {
        // Get common LLVM types needed for signatures
        llvm::Type *voidTy = llvm::Type::getVoidTy(context);
        llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
        llvm::Type *doubleTy = llvm::Type::getDoubleTy(context);
        llvm::Type *i8PtrTy = llvm::Type::getInt8PtrTy(context); // Represents C-style char*
        llvm::Type *boolTy = llvm::Type::getInt1Ty(context);     // Represents bool

        // --- Declare print functions matching native_functions.h ---
        // Format: llvm::FunctionType::get(ReturnType, {ParamType1, ...}, IsVarArg)

        // void native_print_string(const char* str);
        llvm::FunctionType *printStringType = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
        llvm::Function *printStringFunc = llvm::Function::Create(
            printStringType, llvm::Function::ExternalLinkage, "native_print_string", &module);
        standardLibraryFunctions["print_string"] = printStringFunc; // Map internal name

        // void native_print_int(int64_t value);
        llvm::FunctionType *printIntType = llvm::FunctionType::get(voidTy, {i64Ty}, false);
        llvm::Function *printIntFunc = llvm::Function::Create(
            printIntType, llvm::Function::ExternalLinkage, "native_print_int", &module);
        standardLibraryFunctions["print_int"] = printIntFunc;

        // void native_print_float(double value);
        llvm::FunctionType *printFloatType = llvm::FunctionType::get(voidTy, {doubleTy}, false);
        llvm::Function *printFloatFunc = llvm::Function::Create(
            printFloatType, llvm::Function::ExternalLinkage, "native_print_float", &module);
        standardLibraryFunctions["print_float"] = printFloatFunc;

        // void native_print_bool(bool value);
        llvm::FunctionType *printBoolType = llvm::FunctionType::get(voidTy, {boolTy}, false);
        llvm::Function *printBoolFunc = llvm::Function::Create(
            printBoolType, llvm::Function::ExternalLinkage, "native_print_bool", &module);
        standardLibraryFunctions["print_bool"] = printBoolFunc;

        // void native_println();
        llvm::FunctionType *printlnType = llvm::FunctionType::get(voidTy, {}, false); // No arguments
        llvm::Function *printlnFunc = llvm::Function::Create(
            printlnType, llvm::Function::ExternalLinkage, "native_println", &module);
        // Provide a user-facing name "println" for the Tocin language
        standardLibraryFunctions["println"] = printlnFunc;

        // --- Declare other functions (examples) ---
        /*
        // double native_sqrt(double value);
        llvm::FunctionType* sqrtType = llvm::FunctionType::get(doubleTy, {doubleTy}, false);
        llvm::Function* sqrtFunc = llvm::Function::Create(
            sqrtType, llvm::Function::ExternalLinkage, "native_sqrt", &module);
        standardLibraryFunctions["sqrt"] = sqrtFunc; // User-facing name in Tocin
        */

        // --- Verification (Optional but Recommended) ---
        // Verify all declared functions to catch signature mismatches early.
        for (auto const &[name, func] : standardLibraryFunctions)
        {
            if (!func)
            {
                errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                         "Internal Error: Null function pointer stored for stdlib func: " + name,
                                         error::ErrorSeverity::FATAL);
                continue;
            }
            if (llvm::verifyFunction(*func, &llvm::errs()))
            {
                // verifyFunction prints errors to llvm::errs()
                errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                         "LLVM function declaration failed verification for stdlib func: " + name,
                                         error::ErrorSeverity::FATAL);
            }
        }
    }

    // Helper to get a declared standard library function by its Tocin name
    llvm::Function *IRGenerator::getStdLibFunction(const std::string &name)
    {
        auto it = standardLibraryFunctions.find(name);
        if (it == standardLibraryFunctions.end())
        {
            // This indicates an internal error - the type checker should have caught
            // calls to undefined functions before codegen.
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Internal Error: Standard library function '" + name + "' requested but not declared during codegen.",
                                     error::ErrorSeverity::FATAL);
            return nullptr;
        }
        if (!it->second)
        {
            errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                     "Internal Error: Null function pointer found for stdlib func: " + name,
                                     error::ErrorSeverity::FATAL);
            return nullptr;
        }
        return it->second;
    }

    // --- Main Generation Function ---

    // Generates code for a sequence of statements (e.g., top level of a file or REPL input)
    void IRGenerator::generate(const std::vector<std::unique_ptr<Stmt>> &statements)
    {
        // Potentially create a main function wrapper if compiling an executable
        // For now, assume statements are at top level or inside an implicit main

        for (const auto &stmt : statements)
        {
            try
            {
                stmt->accept(this);
            }
            catch (const std::exception &e)
            {
                // Catch potential exceptions during visitation (though errors should ideally be reported via ErrorHandler)
                errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                         "Internal Exception during CodeGen: " + std::string(e.what()),
                                         error::ErrorSeverity::FATAL);
            }
            catch (...)
            {
                errorHandler.reportError(error::ErrorCode::C004_INTERNAL_ASSERTION_FAILED,
                                         "Unknown Internal Exception during CodeGen.",
                                         error::ErrorSeverity::FATAL);
            }
            // Check for fatal errors reported during the visit
            if (errorHandler.hasFatalErrors())
            {
                std::cerr << "Code generation aborted due to fatal errors." << std::endl;
                break; // Stop processing further statements
            }
        }
    }

    // --- Visitor Method Implementations ---

    // Block Statement: Executes a sequence of statements within a scope.
    // TODO: Implement proper scoping for variables declared within the block.
    void IRGenerator::visitBlockStmt(BlockStmt *stmt)
    {
        // Save the current environment
        createEnvironment();

        // Process each statement in the block
        for (const auto &statement : stmt->statements)
        {
            statement->accept(this);
            if (errorHandler.hasFatalErrors())
                return; // Stop if error occurred
        }

        // Restore the previous environment
        restoreEnvironment();
    }

    // Expression Statement: Evaluates an expression, discards the result.
    void IRGenerator::visitExpressionStmt(ExpressionStmt *stmt)
    {
        stmt->expression->accept(this); // Visit the expression
                                        // The result is stored in 'lastValue' but ignored here unless needed for side effects.
        if (errorHandler.hasFatalErrors())
            return;
    }

    // Literal Expression: Represents constant values like numbers, strings, booleans.
    void IRGenerator::visitLiteralExpr(LiteralExpr *expr)
    {
        switch (expr->value.type)
        {
        case lexer::TokenType::NUMBER:
        {
            // Determine if it's an integer or float based on the literal value string
            // This is a simplification; type checker should provide the actual type.
            std::string numStr = expr->value.literal_value;                    // Assuming literal_value holds the string
            llvm::Type *expectedType = getLLVMType(typeChecker.getType(expr)); // Get type from TypeChecker

            if (!expectedType)
            {
                errorHandler.reportError(error::ErrorCode::T009_CANNOT_INFER_TYPE, // Or Codegen error
                                         "Cannot determine LLVM type for literal.",
                                         expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
                return;
            }

            if (expectedType->isIntegerTy())
            {
                try
                {
                    // Use stoll for long long (matches i64)
                    long long intVal = std::stoll(numStr);
                    lastValue = llvm::ConstantInt::get(expectedType, intVal, true); // true = signed
                }
                catch (const std::out_of_range &)
                {
                    errorHandler.reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT, // Or a runtime limit error
                                             "Integer literal out of range for 64 bits.",
                                             expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                }
                catch (const std::invalid_argument &)
                {
                    errorHandler.reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT,
                                             "Invalid integer literal format.",
                                             expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                }
            }
            else if (expectedType->isDoubleTy())
            {
                try
                {
                    double floatVal = std::stod(numStr);
                    lastValue = llvm::ConstantFP::get(expectedType, floatVal);
                }
                catch (const std::out_of_range &)
                {
                    errorHandler.reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT, // Or runtime limit
                                             "Floating point literal out of range.",
                                             expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                }
                catch (const std::invalid_argument &)
                {
                    errorHandler.reportError(error::ErrorCode::L003_INVALID_NUMBER_FORMAT,
                                             "Invalid floating point literal format.",
                                             expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                }
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                         "Internal Error: Numeric literal has unexpected LLVM type.",
                                         expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            break;
        }
        case lexer::TokenType::STRING:
        {
            // Create a global string constant for the literal value
            // The literal_value from the token includes the quotes, remove them.
            std::string strValue = expr->value.literal_value;
            if (strValue.length() >= 2 && strValue.front() == '"' && strValue.back() == '"')
            {
                strValue = strValue.substr(1, strValue.length() - 2);
            }
            // TODO: Handle escape sequences (\n, \t, \", \\, etc.) within strValue

            // Create a global string constant in the module
            llvm::Constant *strConstant = builder.CreateGlobalStringPtr(strValue, ".str");
            lastValue = strConstant;
            break;
        }
        case lexer::TokenType::TRUE:
            lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1, false); // true is 1
            break;
        case lexer::TokenType::FALSE:
            lastValue = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0, false); // false is 0
            break;
        case lexer::TokenType::NIL: // Assuming NIL maps to a null concept
            // How to represent nil? Depends on the type system.
            // Option 1: Use a specific null pointer constant if the expected type is a pointer.
            // Option 2: Use a specific value (e.g., 0 for numbers, maybe?) - less safe.
            // Option 3: Have a dedicated 'nil' type (complex).
            // For now, let's represent it as a null pointer for pointer types, error otherwise.
            llvm::Type *expectedType = getLLVMType(typeChecker.getType(expr));
            if (expectedType && expectedType->isPointerTy())
            {
                lastValue = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(expectedType));
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH, // Or specific nil error
                                         "'nil' literal used in non-pointer context (or type unknown).",
                                         expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
            }
            break;
        default:
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Unsupported literal type in codegen: " + lexer::tokenTypeToString(expr->value.type),
                                     expr->value.line, expr->value.column, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            break;
        }
        if (errorHandler.hasFatalErrors())
            lastValue = nullptr; // Ensure lastValue is null on error
    }

    // Variable Declaration: Allocates space and optionally stores an initial value.
    void IRGenerator::visitVariableDecl(VariableDecl *stmt)
    {
        llvm::Function *theFunction = builder.GetInsertBlock()->getParent();
        if (!theFunction)
        {
            // TODO: Handle global variable declarations
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Global variable declarations not yet supported.",
                                     stmt->name.line, stmt->name.column, error::ErrorSeverity::ERROR);
            return;
        }

        // 1. Get Type Info
        std::shared_ptr<Tocin::Type> tocinVarType = typeChecker.getType(stmt);
        if (!tocinVarType)
        {
            errorHandler.reportError(error::ErrorCode::T009_CANNOT_INFER_TYPE,
                                     "Could not determine type for variable '" + stmt->name.lexeme + "' during codegen.",
                                     stmt->name.line, stmt->name.column, error::ErrorSeverity::FATAL); // Fatal, cannot proceed
            return;
        }
        llvm::Type *llvmVarType = getLLVMType(tocinVarType);
        if (!llvmVarType)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to get LLVM type for Tocin type '" + tocinVarType->toString() + "'.",
                                     stmt->name.line, stmt->name.column, error::ErrorSeverity::FATAL);
            return;
        }

        // 2. Allocate Memory (Alloca)
        llvm::AllocaInst *alloca = createEntryBlockAlloca(theFunction, stmt->name.lexeme, llvmVarType);
        if (!alloca)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to create allocation for variable '" + stmt->name.lexeme + "'.",
                                     stmt->name.line, stmt->name.column, error::ErrorSeverity::FATAL);
            return;
        }

        // 3. Handle Initializer
        if (stmt->initializer)
        {
            // Visit the initializer expression to generate its value
            stmt->initializer->accept(this);
            llvm::Value *initialValue = lastValue;
            if (errorHandler.hasFatalErrors() || !initialValue)
            {
                errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                         "Failed to generate IR for initializer of variable '" + stmt->name.lexeme + "'.",
                                         stmt->name.line, stmt->name.column, error::ErrorSeverity::ERROR);
                // Don't store if initializer failed
            }
            else
            {
                // Check if initializer type matches variable type (LLVM requires exact match for store)
                if (initialValue->getType() != llvmVarType)
                {
                    // TODO: Implement implicit conversions based on type checker rules
                    // e.g., builder.CreateSIToFP, builder.CreateFPToSI, etc.
                    // For now, report error if types don't match exactly.
                    errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                             "Initializer type (" + typeChecker.getType(stmt->initializer)->toString() +
                                                 ") does not match variable type (" + tocinVarType->toString() +
                                                 ") for '" + stmt->name.lexeme + "'. Implicit conversion TBD.",
                                             stmt->name.line, stmt->name.column, error::ErrorSeverity::ERROR);
                }
                else
                {
                    // Store the initial value
                    builder.CreateStore(initialValue, alloca);
                }
            }
        }
        else
        {
            // Handle default initialization if the language requires it (e.g., zero-initialization)
            // builder.CreateStore(llvm::Constant::getNullValue(llvmVarType), alloca);
            // Or report error if uninitialized variables are not allowed / have no default
            errorHandler.reportError(error::ErrorCode::G004_GENERAL_SEMANTIC_ERROR, // Or specific error code
                                     "Variable '" + stmt->name.lexeme + "' declared without initializer (default initialization TBD).",
                                     stmt->name.line, stmt->name.column, error::ErrorSeverity::WARNING); // Or ERROR
        }

        // 4. Update Symbol Table (for the current scope)
        // TODO: Handle shadowing and scope properly. This simple map overwrites.
        if (namedValues.count(stmt->name.lexeme))
        {
            errorHandler.reportError(error::ErrorCode::M001_DUPLICATE_DEFINITION,
                                     "Variable '" + stmt->name.lexeme + "' already defined in this scope.",
                                     stmt->name.line, stmt->name.column, error::ErrorSeverity::ERROR);
            // Don't overwrite in case of error? Or allow shadowing? Depends on language rules.
        }
        namedValues[stmt->name.lexeme] = alloca; // Store the memory location (AllocaInst)
    }

    // Variable Expression: Loads the value from a variable's memory location.
    void IRGenerator::visitVariableExpr(VariableExpr *expr)
    {
        // Look up the variable name in the symbol table
        auto it = namedValues.find(expr->name.lexeme);
        if (it == namedValues.end())
        {
            errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                     "Undefined variable '" + expr->name.lexeme + "'.",
                                     expr->name.line, expr->name.column, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return;
        }

        // Get the memory location (AllocaInst)
        llvm::AllocaInst *alloca = it->second;
        // Get the type stored in the alloca
        llvm::Type *storedType = alloca->getAllocatedType();

        // Generate a Load instruction
        // LoadInst* CreateLoad(Type* Ty, Value* Ptr, const Twine& Name = "");
        lastValue = builder.CreateLoad(storedType, alloca, expr->name.lexeme.c_str());
        if (errorHandler.hasFatalErrors())
            lastValue = nullptr;
    }

    // Assignment Expression: Stores a value into a variable's memory location.
    void IRGenerator::visitAssignmentExpr(AssignmentExpr *expr)
    {
        // 1. Look up the variable to assign to
        auto it = namedValues.find(expr->name.lexeme);
        if (it == namedValues.end())
        {
            errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                     "Cannot assign to undefined variable '" + expr->name.lexeme + "'.",
                                     expr->name.line, expr->name.column, error::ErrorSeverity::ERROR);
            lastValue = nullptr; // Assignment itself has no value or propagates error
            return;
        }
        llvm::AllocaInst *targetAlloca = it->second; // The memory location to store into
        llvm::Type *targetType = targetAlloca->getAllocatedType();

        // 2. Visit the right-hand side expression to get the value to store
        expr->value->accept(this);
        llvm::Value *valueToStore = lastValue;

        if (errorHandler.hasFatalErrors() || !valueToStore)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Failed to generate IR for right-hand side of assignment to '" + expr->name.lexeme + "'.",
                                     expr->name.line, expr->name.column, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
            return;
        }

        // 3. Check Type Compatibility and Store
        if (valueToStore->getType() != targetType)
        {
            // TODO: Implement implicit conversions if allowed by type system
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot assign value of type " + typeChecker.getType(expr->value)->toString() +
                                         " to variable '" + expr->name.lexeme + "' of type " + typeChecker.getType(expr)->toString() + // Assuming getType works on AssignExpr
                                         ". Implicit conversion TBD.",
                                     expr->name.line, expr->name.column, error::ErrorSeverity::ERROR);
            lastValue = nullptr; // Indicate error
            return;
        }

        // Generate the Store instruction
        builder.CreateStore(valueToStore, targetAlloca);

        // The result of an assignment expression is typically the assigned value
        lastValue = valueToStore;
        if (errorHandler.hasFatalErrors())
            lastValue = nullptr;
    }

    // --- Placeholder Implementations for other Visitors ---
    // These need to be fully implemented based on Tocin's semantics.

    void IRGenerator::visitFunctionStmt(FunctionStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "visitFunctionStmt not implemented.", stmt->name.line, stmt->name.column,
                                 error::ErrorSeverity::ERROR);
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

    void IRGenerator::visitReturnStmt(ReturnStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "visitReturnStmt not implemented.", stmt->keyword.line, stmt->keyword.column,
                                 error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. If stmt->value exists, visit it to get the return value (lastValue).
        // 2. Check if return value type matches function's return type.
        // 3. Generate builder.CreateRet(value) or builder.CreateRetVoid().
        // 4. Ensure the current basic block is terminated.
    }

    void IRGenerator::visitIfStmt(IfStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "visitIfStmt not implemented.", stmt->condition ? stmt->condition->getToken().line : 0, 0, // Approx location
                                 error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. Visit condition (lastValue should be i1).
        // 2. Get current function. Create 'then', 'else' (if exists), and 'merge' basic blocks.
        // 3. Create conditional branch: builder.CreateCondBr(conditionValue, thenBlock, elseOrMergeBlock).
        // 4. Set insert point to 'then' block, visit thenBranch. Create unconditional branch to 'merge'.
        // 5. If elseBranch exists: set insert point to 'else' block, visit elseBranch. Create unconditional branch to 'merge'.
        // 6. Set insert point to 'merge' block. Handle PHI nodes if necessary.
    }

    void IRGenerator::visitWhileStmt(WhileStmt *stmt)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "visitWhileStmt not implemented.", stmt->condition ? stmt->condition->getToken().line : 0, 0, // Approx location
                                 error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. Get current function. Create 'loop_cond', 'loop_body', and 'loop_end' basic blocks.
        // 2. Branch to 'loop_cond'.
        // 3. In 'loop_cond': visit condition (lastValue should be i1). Create CondBr(cond, loop_body, loop_end).
        // 4. In 'loop_body': visit body. Branch back to 'loop_cond'.
        // 5. Set insert point to 'loop_end'.
    }

    void IRGenerator::visitUnaryExpr(UnaryExpr *expr)
    {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "visitUnaryExpr not implemented.", expr->op.line, expr->op.column,
                                 error::ErrorSeverity::ERROR);
        // Needs to:
        // 1. Visit operand (expr->right).
        // 2. Get operand value (lastValue).
        // 3. Based on operator type (MINUS, BANG):
        //    - Generate builder.CreateNeg (for arithmetic negation) or builder.CreateFNeg (for float).
        //    - Generate builder.CreateNot (for boolean negation, usually XOR with true).
        // 4. Check types.
        lastValue = nullptr;
    }

    void IRGenerator::visitBinaryExpr(BinaryExpr *expr)
    {
        expr->left->accept(*this);
        llvm::Value *left = currentValue;
        if (!left)
            return;

        expr->right->accept(*this);
        llvm::Value *right = currentValue;
        if (!right)
            return;

        // Ensure types match
        if (left->getType() != right->getType())
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Operands of binary expression must have the same type",
                                     expr->token.filename, expr->token.line, expr->token.column,
                                     error::ErrorSeverity::ERROR);
            currentValue = nullptr;
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
                currentValue = builder.CreateAdd(left, right, "addtmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFAdd(left, right, "addtmp");
            }
            else if (isPointer && right->getType()->isIntegerTy())
            {
                // Pointer arithmetic
                currentValue = builder.CreateGEP(left, right, "ptradd");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary +",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::MINUS:
            if (isInt)
            {
                currentValue = builder.CreateSub(left, right, "subtmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFSub(left, right, "subtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary -",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::STAR:
            if (isInt)
            {
                currentValue = builder.CreateMul(left, right, "multmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFMul(left, right, "multmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary *",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::SLASH:
            if (isInt)
            {
                currentValue = builder.CreateSDiv(left, right, "divtmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFDiv(left, right, "divtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary /",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::PERCENT:
            if (isInt)
            {
                currentValue = builder.CreateSRem(left, right, "modtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary %",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::EQUAL_EQUAL:
            if (isInt || left->getType()->isPointerTy())
            {
                currentValue = builder.CreateICmpEQ(left, right, "eqtmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFCmpOEQ(left, right, "eqtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary ==",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::BANG_EQUAL:
            if (isInt || left->getType()->isPointerTy())
            {
                currentValue = builder.CreateICmpNE(left, right, "neqtmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFCmpONE(left, right, "neqtmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary !=",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::LESS:
            if (isInt)
            {
                currentValue = builder.CreateICmpSLT(left, right, "lttmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFCmpOLT(left, right, "lttmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary <",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::LESS_EQUAL:
            if (isInt)
            {
                currentValue = builder.CreateICmpSLE(left, right, "letmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFCmpOLE(left, right, "letmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary <=",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::GREATER:
            if (isInt)
            {
                currentValue = builder.CreateICmpSGT(left, right, "gttmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFCmpOGT(left, right, "gttmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary >",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        case lexer::TokenType::GREATER_EQUAL:
            if (isInt)
            {
                currentValue = builder.CreateICmpSGE(left, right, "getmp");
            }
            else if (isFloat)
            {
                currentValue = builder.CreateFCmpOGE(left, right, "getmp");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                         "Invalid operands to binary >=",
                                         expr->token.filename, expr->token.line, expr->token.column,
                                         error::ErrorSeverity::ERROR);
                currentValue = nullptr;
            }
            break;
        default:
            errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                     "Unhandled binary operator: " + expr->op.value,
                                     expr->token.filename, expr->token.line, expr->token.column,
                                     error::ErrorSeverity::ERROR);
            currentValue = nullptr;
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

} // namespace codegen
