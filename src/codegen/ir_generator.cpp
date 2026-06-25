#include "ir_generator.h"
#include "../ast/ast.h"
#include "../lexer/token.h"
#include "../type/type_checker.h"
#include "../error/error_handler.h"
#include "../compiler/compilation_context.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/InlineAsm.h>
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

    // Don't create a dummy main - let the user provide their own
    // createMainFunction();

    // Don't call declarePrintFunction - already done in declareStdLibFunctions()
    // declarePrintFunction();
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
    
    // Add print and println as aliases that call printf
    // print - just prints the message
    stdLibFunctions["print"] = printfFunc;
    // println - prints the message with a newline
    stdLibFunctions["println"] = printfFunc;

    // Memory management functions. All compiler-emitted allocations go through
    // __tocin_alloc, the runtime's central allocator: when the runtime is built
    // with Boehm GC it returns collected memory (no leaks); otherwise it is a
    // thin malloc wrapper. Keyed as "malloc" so existing call sites are unchanged.
    llvm::FunctionType *mallocType = llvm::FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
        {llvm::Type::getInt64Ty(context)},
        false);
    llvm::Function *mallocFunc = llvm::Function::Create(
        mallocType, llvm::Function::ExternalLinkage, "__tocin_alloc", *module);
    stdLibFunctions["malloc"] = mallocFunc;

    llvm::FunctionType *freeType = llvm::FunctionType::get(
        llvm::Type::getVoidTy(context),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
        false);
    // Pair with __tocin_alloc: under GC this is GC_free, otherwise libc free.
    // Calling libc free() on a GC pointer would abort ("invalid pointer").
    llvm::Function *freeFunc = llvm::Function::Create(
        freeType, llvm::Function::ExternalLinkage, "__tocin_free", *module);
    stdLibFunctions["free"] = freeFunc;

    // libc string helpers (used for string concatenation with '+').
    llvm::Type *charPtr = llvm::PointerType::get(context, 0);
    llvm::Type *i64Ty = llvm::Type::getInt64Ty(context);
    auto declare = [&](const std::string &name, llvm::Type *ret,
                       std::vector<llvm::Type *> params) {
        llvm::FunctionType *ft = llvm::FunctionType::get(ret, params, false);
        stdLibFunctions[name] =
            llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, *module);
    };
    declare("strlen", i64Ty, {charPtr});
    declare("strcpy", charPtr, {charPtr, charPtr});
    declare("strcat", charPtr, {charPtr, charPtr});

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

        // Resolve active generic type-parameter bindings first (e.g. T -> i64).
        auto bound = typeBindings.find(typeName);
        if (bound != typeBindings.end())
            return bound->second;

        // Check for basic type names
        if (typeName == "int" || typeName == "i64")
        {
            return llvm::Type::getInt64Ty(context);
        }
        else if (typeName == "i32")
        {
            return llvm::Type::getInt32Ty(context);
        }
        else if (typeName == "i16")
        {
            return llvm::Type::getInt16Ty(context);
        }
        else if (typeName == "i8")
        {
            return llvm::Type::getInt8Ty(context);
        }
        else if (typeName == "float" || typeName == "f64")
        {
            return llvm::Type::getDoubleTy(context);
        }
        else if (typeName == "f32" || typeName == "float32")
        {
            return llvm::Type::getFloatTy(context);
        }
        else if (typeName == "bool")
        {
            return llvm::Type::getInt1Ty(context);
        }
        else if (typeName == "string" || typeName == "str")
        {
            // Use opaque pointer for string (char*)
            return llvm::PointerType::get(context, 0);
        }
        else if (typeName == "void" || typeName == "None")
        {
            return llvm::Type::getVoidTy(context);
        }

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

        // Channels, lists and arrays are opaque heap handles (pointers).
        // Arrays use the flat [i64 length][elems...] layout produced by
        // visitListExpr, so a list/array value is just a pointer.
        if (baseName == "channel" || baseName == "Channel" || baseName == "chan" ||
            baseName == "list" || baseName == "array" || baseName == "List" ||
            baseName == "Array")
            return llvm::PointerType::get(context, 0);

        if (false)
        {
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

    // Function types lower to an (opaque) function pointer.
    if (std::dynamic_pointer_cast<ast::FunctionType>(type))
        return llvm::PointerType::get(context, 0);

    // If all else fails, return a void type
    return llvm::Type::getVoidTy(context);
}

llvm::Type *IRGenerator::inferExprType(const ast::ExprPtr &expr,
                                       const std::map<std::string, llvm::Type *> &localTypes)
{
    if (!expr)
        return llvm::Type::getVoidTy(context);

    if (auto lit = std::dynamic_pointer_cast<ast::LiteralExpr>(expr))
    {
        switch (lit->literalType)
        {
        case ast::LiteralExpr::LiteralType::INTEGER:
            return llvm::Type::getInt64Ty(context);
        case ast::LiteralExpr::LiteralType::FLOAT:
            return llvm::Type::getDoubleTy(context);
        case ast::LiteralExpr::LiteralType::BOOLEAN:
            return llvm::Type::getInt1Ty(context);
        case ast::LiteralExpr::LiteralType::STRING:
        case ast::LiteralExpr::LiteralType::NIL:
        default:
            return llvm::PointerType::get(context, 0);
        }
    }
    if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
    {
        auto it = localTypes.find(var->name);
        if (it != localTypes.end())
            return it->second;
        auto nv = namedValues.find(var->name);
        if (nv != namedValues.end() && nv->second)
            return nv->second->getAllocatedType();
        return llvm::Type::getInt64Ty(context);
    }
    if (auto grp = std::dynamic_pointer_cast<ast::GroupingExpr>(expr))
        return inferExprType(grp->expression, localTypes);
    if (auto un = std::dynamic_pointer_cast<ast::UnaryExpr>(expr))
    {
        if (un->op.type == lexer::TokenType::BANG)
            return llvm::Type::getInt1Ty(context);
        return inferExprType(un->right, localTypes);
    }
    if (auto bin = std::dynamic_pointer_cast<ast::BinaryExpr>(expr))
    {
        switch (bin->op.type)
        {
        case lexer::TokenType::EQUAL_EQUAL:
        case lexer::TokenType::BANG_EQUAL:
        case lexer::TokenType::LESS:
        case lexer::TokenType::LESS_EQUAL:
        case lexer::TokenType::GREATER:
        case lexer::TokenType::GREATER_EQUAL:
        case lexer::TokenType::AND:
        case lexer::TokenType::OR:
            return llvm::Type::getInt1Ty(context);
        default:
            break;
        }
        llvm::Type *lt = inferExprType(bin->left, localTypes);
        llvm::Type *rt = inferExprType(bin->right, localTypes);
        if (lt->isDoubleTy() || rt->isDoubleTy() || lt->isFloatTy() || rt->isFloatTy())
            return llvm::Type::getDoubleTy(context);
        if (lt->isPointerTy())
            return lt; // e.g. string concatenation
        return llvm::Type::getInt64Ty(context);
    }
    if (auto call = std::dynamic_pointer_cast<ast::CallExpr>(expr))
    {
        if (auto callee = std::dynamic_pointer_cast<ast::VariableExpr>(call->callee))
        {
            if (llvm::Function *f = module->getFunction(callee->name))
                return f->getReturnType();
        }
        return llvm::Type::getInt64Ty(context);
    }
    if (std::dynamic_pointer_cast<ast::StringInterpolationExpr>(expr))
        return llvm::PointerType::get(context, 0);
    if (auto asgn = std::dynamic_pointer_cast<ast::AssignExpr>(expr))
        return inferExprType(asgn->value, localTypes);

    // Conservative default for everything else.
    return llvm::Type::getInt64Ty(context);
}

llvm::Type *IRGenerator::inferFunctionReturnType(ast::FunctionStmt *stmt)
{
    std::map<std::string, llvm::Type *> localTypes;
    for (const auto &p : stmt->parameters)
    {
        if (p.type)
            localTypes[p.name] = getLLVMType(p.type);
    }

    llvm::Type *found = nullptr;
    std::function<void(const ast::StmtPtr &)> scan = [&](const ast::StmtPtr &s)
    {
        if (!s || found)
            return;
        if (auto ret = std::dynamic_pointer_cast<ast::ReturnStmt>(s))
        {
            if (ret->value)
                found = inferExprType(ret->value, localTypes);
            return;
        }
        if (auto blk = std::dynamic_pointer_cast<ast::BlockStmt>(s))
        {
            for (const auto &st : blk->statements)
            {
                scan(st);
                if (found)
                    return;
            }
        }
        else if (auto iff = std::dynamic_pointer_cast<ast::IfStmt>(s))
        {
            scan(iff->thenBranch);
            for (const auto &eb : iff->elifBranches)
                if (!found)
                    scan(eb.second);
            if (!found)
                scan(iff->elseBranch);
        }
        else if (auto wh = std::dynamic_pointer_cast<ast::WhileStmt>(s))
            scan(wh->body);
        else if (auto fr = std::dynamic_pointer_cast<ast::ForStmt>(s))
            scan(fr->body);
        else if (auto mt = std::dynamic_pointer_cast<ast::MatchStmt>(s))
        {
            for (const auto &c : mt->cases)
                if (!found)
                    scan(c.second);
            if (!found)
                scan(mt->defaultCase);
        }
    };
    scan(stmt->body);
    return found ? found : llvm::Type::getVoidTy(context);
}

void IRGenerator::visitLiteralExpr(ast::LiteralExpr *expr)
{
    // Use the literalType field to determine what kind of literal we have
    switch (expr->literalType)
    {
    case ast::LiteralExpr::LiteralType::INTEGER:
    {
        // Convert string to int64 (handles 0x/0o/0b and digit separators).
        int64_t value = lexer::parseIntegerLiteral(expr->value);
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), value);
        break;
    }
    case ast::LiteralExpr::LiteralType::FLOAT:
    {
        // Convert string to double (drop any '_' digit separators / 'f' suffix).
        std::string fv;
        for (char c : expr->value)
            if (c != '_' && c != 'f' && c != 'F')
                fv += c;
        double value = fv.empty() ? 0.0 : std::stod(fv);
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
    llvm::Type *varType = stmt->type ? getLLVMType(stmt->type) : nullptr;

    // Evaluate the initializer exactly once, here. Never reuse a stale
    // `lastValue` left over from a previous statement (it may even belong to a
    // different function, which produced "does not dominate all uses"). For an
    // inferred type, the evaluated value also determines the variable's type.
    llvm::Value *initVal = nullptr;
    if (stmt->initializer)
    {
        lastValue = nullptr;
        stmt->initializer->accept(*this);
        if (!lastValue)
            return;
        initVal = wrapIfRawFunction(lastValue); // `let f = someFunc;` -> closure
        if (!varType)
            varType = initVal->getType();
    }
    else if (!stmt->type)
    {
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

    // Track the variable's class (for field/method resolution). Done after the
    // initializer is evaluated so generic-constructor calls have already been
    // resolved to a concrete mangled class name.
    std::string vcls = getExprClassName(stmt->initializer);
    if (vcls.empty() && stmt->type && classTypes.count(stmt->type->toString()))
        vcls = stmt->type->toString();
    if (!vcls.empty())
        varClasses[stmt->name] = vcls;

    // Track array element type so indexing into this variable loads/stores the
    // right element type.
    if (auto lst = std::dynamic_pointer_cast<ast::ListExpr>(stmt->initializer))
    {
        varArrayElem[stmt->name] =
            lst->elements.empty() ? llvm::Type::getInt64Ty(context)
                                  : inferExprType(lst->elements[0], {});
    }
    else if (auto v = std::dynamic_pointer_cast<ast::VariableExpr>(stmt->initializer))
    {
        auto it = varArrayElem.find(v->name);
        if (it != varArrayElem.end())
            varArrayElem[stmt->name] = it->second;
    }

    // Track a function-pointer-typed local so calls through it recover the
    // signature: from an explicit annotation, or copied from the initializer
    // variable's own signature.
    if (auto ft = std::dynamic_pointer_cast<ast::FunctionType>(stmt->type))
        varFuncSig[stmt->name] = ft;
    else if (auto v = std::dynamic_pointer_cast<ast::VariableExpr>(stmt->initializer))
    {
        auto it = varFuncSig.find(v->name);
        if (it != varFuncSig.end())
            varFuncSig[stmt->name] = it->second;
    }
    else if (auto call = std::dynamic_pointer_cast<ast::CallExpr>(stmt->initializer))
    {
        // `let f = makeAdder(..)` — f's signature is makeAdder's return type.
        if (auto cv = std::dynamic_pointer_cast<ast::VariableExpr>(call->callee))
        {
            auto it = funcReturnFnType.find(cv->name);
            if (it != funcReturnFnType.end())
                varFuncSig[stmt->name] = it->second;
        }
    }
    else if (auto lam = std::dynamic_pointer_cast<ast::LambdaExpr>(stmt->initializer))
    {
        // `let f = lambda (x: int) -> int ...` — record f's signature so it can
        // be called through the variable.
        std::vector<ast::TypePtr> pts;
        for (auto &p : lam->parameters)
            pts.push_back(p.type);
        varFuncSig[stmt->name] =
            std::make_shared<ast::FunctionType>(lam->token, pts, lam->returnType);
    }

    // Track string-typed locals so == / != compare by value, not pointer.
    bool declaredString = stmt->type && stmt->type->toString() == "string";
    if (declaredString || isStringExpr(stmt->initializer))
        varIsString.insert(stmt->name);
    else
        varIsString.erase(stmt->name);

    // Store the initializer value (already evaluated above).
    if (initVal)
    {
        if (initVal->getType() != varType)
        {
            if (initVal->getType()->isIntegerTy() && varType->isIntegerTy())
                initVal = builder.CreateIntCast(initVal, varType, true, "cast");
            else if ((initVal->getType()->isFloatTy() || initVal->getType()->isDoubleTy()) &&
                     (varType->isFloatTy() || varType->isDoubleTy()))
                initVal = builder.CreateFPCast(initVal, varType, "cast");
            else if (initVal->getType()->isIntegerTy() && !initVal->getType()->isIntegerTy(1) &&
                     (varType->isFloatTy() || varType->isDoubleTy()))
                // Implicit int -> float when the variable is declared float.
                initVal = builder.CreateSIToFP(initVal, varType, "promote");
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Initializer type does not match variable type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return;
            }
        }
        builder.CreateStore(initVal, alloca);
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

    // Generic functions are templates: record them and instantiate lazily
    // (monomorphize) when called with concrete argument types.
    if (stmt->isGeneric())
    {
        genericFunctions[stmt->name] = stmt;
        return;
    }

    // A body-less function is an external (C/FFI) declaration: emit only the
    // prototype and let the linker/JIT resolve the real symbol.
    if (!stmt->body)
    {
        declareFunctionProto(stmt);
        return;
    }

    // Handle regular functions
    std::string funcName = stmt->name;
    
    // Build parameter types
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : stmt->parameters)
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
        {
            errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                     "Invalid parameter type in function '" + funcName + "'",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }
        paramTypes.push_back(paramType);
    }

    // Get return type. When the function has no explicit return annotation,
    // infer it from the body's return statements (defaulting to void).
    llvm::Type *returnType = stmt->returnType
                                 ? getLLVMType(stmt->returnType)
                                 : inferFunctionReturnType(stmt);
    if (!returnType)
    {
        returnType = llvm::Type::getVoidTy(context);
    }

    // Create function type
    llvm::FunctionType *funcType = llvm::FunctionType::get(
        returnType, paramTypes, false);

    // Reuse a forward-declared prototype if present, else create it.
    llvm::Function *function = module->getFunction(funcName);
    if (!function)
    {
        function = llvm::Function::Create(
            funcType, llvm::Function::ExternalLinkage, funcName, *module);
    }

    // Set parameter names and store them in symbol table
    unsigned idx = 0;
    for (auto &arg : function->args())
    {
        if (idx < stmt->parameters.size())
        {
            arg.setName(stmt->parameters[idx].name);
        }
        idx++;
    }

    // Save the enclosing codegen scope so a nested `def` inside another
    // function body does not clobber the outer function's insert point or
    // local symbol tables. (For top-level functions these are empty / a
    // terminated block, so the save/restore is a harmless no-op.)
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    auto savedNamedValues = namedValues;
    auto savedVarClasses = varClasses;
    auto savedVarArrayElem = varArrayElem;
    auto savedVarFuncSig = varFuncSig;
    auto savedVarIsString = varIsString;

    // Create entry basic block
    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    builder.SetInsertPoint(entryBlock);

    // Save the current function context
    llvm::Function *previousFunction = currentFunction;
    currentFunction = function;

    // Fresh symbol tables for this function body.
    namedValues.clear();
    varClasses.clear();
    varArrayElem.clear();
    varFuncSig.clear();
    varIsString.clear();

    // Create allocas for parameters and store values
    idx = 0;
    for (auto &arg : function->args())
    {
        if (idx < stmt->parameters.size())
        {
            llvm::AllocaInst *alloca = builder.CreateAlloca(
                arg.getType(), nullptr, stmt->parameters[idx].name);
            builder.CreateStore(&arg, alloca);
            namedValues[stmt->parameters[idx].name] = alloca;
            recordArrayParam(stmt->parameters[idx].name, stmt->parameters[idx].type);
        }
        idx++;
    }

    // Generate function body
    if (stmt->body)
    {
        stmt->body->accept(*this);
    }

    // If the function doesn't have an explicit return and returns void, add one
    if (returnType->isVoidTy() && !builder.GetInsertBlock()->getTerminator())
    {
        builder.CreateRetVoid();
    }
    // If the function returns a value but no return was generated, add a default return
    else if (!returnType->isVoidTy() && !builder.GetInsertBlock()->getTerminator())
    {
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
            builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(returnType)));
        }
        else
        {
            // For other types, just create a return with undef
            builder.CreateRet(llvm::UndefValue::get(returnType));
        }
    }

    // Verify the function
    std::string errorStr;
    llvm::raw_string_ostream errorStream(errorStr);
    if (llvm::verifyFunction(*function, &errorStream))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Invalid LLVM IR generated for function '" + funcName + "': " + errorStr,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
    }

    // Restore the enclosing function/codegen scope.
    currentFunction = previousFunction;
    namedValues = savedNamedValues;
    varClasses = savedVarClasses;
    varArrayElem = savedVarArrayElem;
    varFuncSig = savedVarFuncSig;
    varIsString = savedVarIsString;
    if (savedBlock)
        builder.SetInsertPoint(savedBlock);
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
        lastValue = wrapIfRawFunction(lastValue); // `return someFunc;` -> closure

        // Coerce the value to the function's return type when needed.
        if (lastValue->getType() != returnType)
        {
            llvm::Type *vt = lastValue->getType();
            if (returnType->isVoidTy())
            {
                // Function returns void; discard the value.
                runPendingFinally();
                builder.CreateRetVoid();
                return;
            }
            else if (vt->isIntegerTy() && returnType->isIntegerTy())
            {
                lastValue = builder.CreateIntCast(lastValue, returnType, true, "castret");
            }
            else if (vt->isFloatingPointTy() && returnType->isFloatingPointTy())
            {
                lastValue = builder.CreateFPCast(lastValue, returnType, "castret");
            }
            else if (vt->isIntegerTy() && returnType->isFloatingPointTy())
            {
                lastValue = builder.CreateSIToFP(lastValue, returnType, "castret");
            }
            else if (vt->isFloatingPointTy() && returnType->isIntegerTy())
            {
                lastValue = builder.CreateFPToSI(lastValue, returnType, "castret");
            }
            else if (vt->isPointerTy() && returnType->isPointerTy())
            {
                lastValue = builder.CreateBitCast(lastValue, returnType, "castret");
            }
            else
            {
                errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                         "Return value type does not match function return type",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                return;
            }
        }

        // Run any pending finally blocks, then return. Save the value first:
        // the finally block emission may overwrite lastValue.
        llvm::Value *retVal = lastValue;
        runPendingFinally();
        builder.CreateRet(retVal);
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
        runPendingFinally();

        builder.CreateRetVoid();
    }
}

void IRGenerator::visitCallExpr(ast::CallExpr *expr)
{
    // Constructor call: ClassName(args) allocates an instance and initializes
    // its fields positionally from the arguments.
    if (auto ctorName = std::dynamic_pointer_cast<ast::VariableExpr>(expr->callee))
    {
        auto cit = classTypes.find(ctorName->name);
        if (cit != classTypes.end())
        {
            llvm::StructType *st = cit->second.classType;
            llvm::Function *mallocFn = stdLibFunctions["malloc"];
            llvm::Value *size = llvm::ConstantExpr::getSizeOf(st);
            llvm::Value *obj = builder.CreateCall(mallocFn->getFunctionType(), mallocFn,
                                                  {size}, ctorName->name + ".obj");
            for (size_t i = 0; i < expr->arguments.size() &&
                               i < cit->second.memberNames.size();
                 ++i)
            {
                expr->arguments[i]->accept(*this);
                if (!lastValue) return;
                llvm::Value *fieldVal = lastValue;
                llvm::Type *fieldTy = st->getStructElementType(i);
                if (fieldVal->getType() != fieldTy)
                {
                    if (fieldVal->getType()->isIntegerTy() && fieldTy->isIntegerTy())
                        fieldVal = builder.CreateIntCast(fieldVal, fieldTy, true, "fcast");
                    else if (fieldVal->getType()->isFloatingPointTy() && fieldTy->isFloatingPointTy())
                        fieldVal = builder.CreateFPCast(fieldVal, fieldTy, "fcast");
                }
                std::vector<llvm::Value *> idx = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), (unsigned)i)};
                llvm::Value *fp = builder.CreateGEP(st, obj, idx, "init." + cit->second.memberNames[i]);
                builder.CreateStore(fieldVal, fp);
            }
            lastValue = obj;
            lastExprClassName = ctorName->name;
            return;
        }
    }

    // Generic constructor call: Box(args) infers the type arguments from the
    // argument types, monomorphizes the class, and constructs an instance.
    if (auto ctorName = std::dynamic_pointer_cast<ast::VariableExpr>(expr->callee))
    {
        auto git = genericClasses.find(ctorName->name);
        if (git != genericClasses.end())
        {
            ast::ClassStmt *tmpl = git->second;

            std::vector<llvm::Value *> argVals;
            for (const auto &a : expr->arguments)
            {
                a->accept(*this);
                if (!lastValue) return;
                argVals.push_back(lastValue);
            }

            // Infer bindings by matching each field's declared type-parameter
            // name against the corresponding argument's concrete type.
            std::set<std::string> tparams;
            for (const auto &tp : tmpl->typeParameters)
                tparams.insert(tp.getName());
            std::map<std::string, llvm::Type *> bindings;
            size_t fi = 0;
            for (const auto &fieldStmt : tmpl->fields)
            {
                auto var = std::dynamic_pointer_cast<ast::VariableStmt>(fieldStmt);
                if (!var) continue;
                std::string ftName = var->type ? var->type->toString() : "";
                if (tparams.count(ftName) && fi < argVals.size() && !bindings.count(ftName))
                    bindings[ftName] = argVals[fi]->getType();
                ++fi;
            }
            for (const auto &tp : tmpl->typeParameters)
                if (!bindings.count(tp.getName()))
                    bindings[tp.getName()] = llvm::Type::getInt64Ty(context);

            std::string mangled = instantiateGenericClass(tmpl, bindings);
            const ClassInfo &ci = classTypes[mangled];
            llvm::StructType *st = ci.classType;

            llvm::Function *mallocFn = stdLibFunctions["malloc"];
            llvm::Value *size = llvm::ConstantExpr::getSizeOf(st);
            llvm::Value *obj = builder.CreateCall(mallocFn->getFunctionType(), mallocFn,
                                                  {size}, ctorName->name + ".obj");
            for (size_t i = 0; i < argVals.size() && i < ci.memberNames.size(); ++i)
            {
                llvm::Value *fieldVal = argVals[i];
                llvm::Type *fieldTy = st->getStructElementType(i);
                if (fieldVal->getType() != fieldTy)
                {
                    if (fieldVal->getType()->isIntegerTy() && fieldTy->isIntegerTy())
                        fieldVal = builder.CreateIntCast(fieldVal, fieldTy, true, "fcast");
                    else if (fieldVal->getType()->isFloatingPointTy() && fieldTy->isFloatingPointTy())
                        fieldVal = builder.CreateFPCast(fieldVal, fieldTy, "fcast");
                }
                std::vector<llvm::Value *> idx = {
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), (unsigned)i)};
                llvm::Value *fp = builder.CreateGEP(st, obj, idx, "init." + ci.memberNames[i]);
                builder.CreateStore(fieldVal, fp);
            }
            lastValue = obj;
            lastExprClassName = mangled;
            genericCtorClass[expr] = mangled;
            return;
        }
    }

    // Method call: receiver.method(args) -> ClassName_method(receiver, args).
    if (auto methodCallee = std::dynamic_pointer_cast<ast::GetExpr>(expr->callee))
    {
        std::string cls = getExprClassName(methodCallee->object);
        if (!cls.empty())
        {
            if (llvm::Function *mfn = module->getFunction(cls + "_" + methodCallee->name))
            {
                methodCallee->object->accept(*this);
                llvm::Value *recv = lastValue;
                if (!recv) return;
                std::vector<llvm::Value *> args;
                args.push_back(recv);
                for (const auto &a : expr->arguments)
                {
                    a->accept(*this);
                    if (!lastValue) return;
                    args.push_back(lastValue);
                }
                lastValue = builder.CreateCall(mfn->getFunctionType(), mfn, args);
                return;
            }
        }
    }

    // Evaluate callee
    expr->callee->accept(*this);
    llvm::Value *callee = lastValue;

    if (!callee)
        return;

    // Handle special case - direct function call by name
    if (auto varExpr = std::dynamic_pointer_cast<ast::VariableExpr>(expr->callee))
    {
        std::string funcName = varExpr->name;

        // Option/Result constructors. Some(x)/Ok(v) are tag 1; Err(e) is tag 0.
        // None is the nil literal (a null pointer), handled in visitLiteralExpr.
        if ((funcName == "Some" || funcName == "Ok") && expr->arguments.size() == 1)
        {
            expr->arguments[0]->accept(*this);
            if (!lastValue) return;
            lastValue = makeOptRes(1, lastValue);
            return;
        }
        if (funcName == "Err" && expr->arguments.size() == 1)
        {
            expr->arguments[0]->accept(*this);
            if (!lastValue) return;
            lastValue = makeOptRes(0, lastValue);
            return;
        }

        // len(arr) reads the length stored in the array header (offset 0).
        if (funcName == "len" && expr->arguments.size() == 1)
        {
            expr->arguments[0]->accept(*this);
            llvm::Value *base = lastValue;
            if (!base) return;
            lastValue = builder.CreateLoad(llvm::Type::getInt64Ty(context), base, "len");
            return;
        }

        // channel() creation -> __tocin_chan_new().
        if (funcName == "__chan_new")
        {
            llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
            llvm::Function *f = module->getFunction("__tocin_chan_new");
            if (!f)
                f = llvm::Function::Create(llvm::FunctionType::get(ptrTy, {}, false),
                                           llvm::Function::ExternalLinkage, "__tocin_chan_new", *module);
            lastValue = builder.CreateCall(f, {}, "chan");
            return;
        }

        // --- Runtime builtins: dynamic collections, strings, file I/O ------
        // Each lazily declares its __tocin_* prototype and emits the call.
        // Handles/strings are ptr; scalar values/results are i64.
        {
            llvm::Type *i64b = llvm::Type::getInt64Ty(context);
            llvm::Type *ptrb = llvm::PointerType::get(context, 0);
            llvm::Type *voidb = llvm::Type::getVoidTy(context);
            auto rt = [&](const char *nm, llvm::Type *ret,
                          std::vector<llvm::Type *> ps) -> llvm::Function * {
                llvm::Function *f = module->getFunction(nm);
                if (!f)
                    f = llvm::Function::Create(llvm::FunctionType::get(ret, ps, false),
                                               llvm::Function::ExternalLinkage, nm, *module);
                return f;
            };
            // Evaluate argument i and coerce to a 64-bit slot.
            auto slot = [&](size_t i) -> llvm::Value * {
                expr->arguments[i]->accept(*this);
                llvm::Value *v = lastValue;
                if (!v) return nullptr;
                if (v->getType()->isPointerTy()) return builder.CreatePtrToInt(v, i64b, "slot");
                if (v->getType()->isDoubleTy()) return builder.CreateBitCast(v, i64b, "slot");
                if (v->getType()->isFloatTy())
                    return builder.CreateBitCast(builder.CreateFPExt(v, llvm::Type::getDoubleTy(context)), i64b, "slot");
                if (v->getType()->isIntegerTy() && v->getType() != i64b)
                    return builder.CreateIntCast(v, i64b, true, "slot");
                return v;
            };
            // Evaluate argument i as a pointer (handle or string).
            auto pptr = [&](size_t i) -> llvm::Value * {
                expr->arguments[i]->accept(*this);
                llvm::Value *v = lastValue;
                if (!v) return nullptr;
                if (v->getType()->isIntegerTy()) return builder.CreateIntToPtr(v, ptrb, "asptr");
                return v;
            };
            auto na = expr->arguments.size();

            // ---- vector ----
            if (funcName == "vecNew" && na == 0) {
                lastValue = builder.CreateCall(rt("__tocin_vec_new", ptrb, {}), {}, "vec"); return; }
            if (funcName == "vecPush" && na == 2) {
                auto h = pptr(0); auto x = slot(1); if (!h || !x) return;
                builder.CreateCall(rt("__tocin_vec_push", voidb, {ptrb, i64b}), {h, x});
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }
            if (funcName == "vecGet" && na == 2) {
                auto h = pptr(0); auto i = slot(1); if (!h || !i) return;
                lastValue = builder.CreateCall(rt("__tocin_vec_get", i64b, {ptrb, i64b}), {h, i}, "vget"); return; }
            if (funcName == "vecSet" && na == 3) {
                auto h = pptr(0); auto i = slot(1); auto x = slot(2); if (!h || !i || !x) return;
                builder.CreateCall(rt("__tocin_vec_set", voidb, {ptrb, i64b, i64b}), {h, i, x});
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }
            if (funcName == "vecLen" && na == 1) {
                auto h = pptr(0); if (!h) return;
                lastValue = builder.CreateCall(rt("__tocin_vec_len", i64b, {ptrb}), {h}, "vlen"); return; }
            if (funcName == "vecPop" && na == 1) {
                auto h = pptr(0); if (!h) return;
                lastValue = builder.CreateCall(rt("__tocin_vec_pop", i64b, {ptrb}), {h}, "vpop"); return; }
            if (funcName == "vecFree" && na == 1) {
                auto h = pptr(0); if (!h) return;
                builder.CreateCall(rt("__tocin_vec_free", voidb, {ptrb}), {h});
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }

            // ---- hashmap ----
            if (funcName == "mapNew" && na == 0) {
                lastValue = builder.CreateCall(rt("__tocin_map_new", ptrb, {}), {}, "map"); return; }
            if (funcName == "mapPut" && na == 3) {
                auto h = pptr(0); auto k = slot(1); auto v = slot(2); if (!h || !k || !v) return;
                builder.CreateCall(rt("__tocin_map_put", voidb, {ptrb, i64b, i64b}), {h, k, v});
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }
            if (funcName == "mapGet" && na == 2) {
                auto h = pptr(0); auto k = slot(1); if (!h || !k) return;
                lastValue = builder.CreateCall(rt("__tocin_map_get", i64b, {ptrb, i64b}), {h, k}, "mget"); return; }
            if (funcName == "mapHas" && na == 2) {
                auto h = pptr(0); auto k = slot(1); if (!h || !k) return;
                lastValue = builder.CreateCall(rt("__tocin_map_has", i64b, {ptrb, i64b}), {h, k}, "mhas"); return; }
            if (funcName == "mapPutStr" && na == 3) {
                auto h = pptr(0); auto k = pptr(1); auto v = slot(2); if (!h || !k || !v) return;
                builder.CreateCall(rt("__tocin_map_put_str", voidb, {ptrb, ptrb, i64b}), {h, k, v});
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }
            if (funcName == "mapGetStr" && na == 2) {
                auto h = pptr(0); auto k = pptr(1); if (!h || !k) return;
                lastValue = builder.CreateCall(rt("__tocin_map_get_str", i64b, {ptrb, ptrb}), {h, k}, "mgets"); return; }
            if (funcName == "mapHasStr" && na == 2) {
                auto h = pptr(0); auto k = pptr(1); if (!h || !k) return;
                lastValue = builder.CreateCall(rt("__tocin_map_has_str", i64b, {ptrb, ptrb}), {h, k}, "mhass"); return; }
            if (funcName == "mapLen" && na == 1) {
                auto h = pptr(0); if (!h) return;
                lastValue = builder.CreateCall(rt("__tocin_map_len", i64b, {ptrb}), {h}, "mlen"); return; }
            if (funcName == "mapFree" && na == 1) {
                auto h = pptr(0); if (!h) return;
                builder.CreateCall(rt("__tocin_map_free", voidb, {ptrb}), {h});
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }

            // ---- strings ----
            if (funcName == "strLen" && na == 1) {
                auto s = pptr(0); if (!s) return;
                lastValue = builder.CreateCall(rt("__tocin_str_len", i64b, {ptrb}), {s}, "strlen"); return; }
            if (funcName == "charAt" && na == 2) {
                auto s = pptr(0); auto i = slot(1); if (!s || !i) return;
                lastValue = builder.CreateCall(rt("__tocin_str_char_at", i64b, {ptrb, i64b}), {s, i}, "charat"); return; }
            if (funcName == "substring" && na == 3) {
                auto s = pptr(0); auto a = slot(1); auto b = slot(2); if (!s || !a || !b) return;
                lastValue = builder.CreateCall(rt("__tocin_str_substring", ptrb, {ptrb, i64b, i64b}), {s, a, b}, "substr"); return; }
            if (funcName == "strEq" && na == 2) {
                auto a = pptr(0); auto b = pptr(1); if (!a || !b) return;
                lastValue = builder.CreateCall(rt("__tocin_str_eq", i64b, {ptrb, ptrb}), {a, b}, "streq"); return; }
            if (funcName == "strCmp" && na == 2) {
                auto a = pptr(0); auto b = pptr(1); if (!a || !b) return;
                lastValue = builder.CreateCall(rt("__tocin_str_cmp", i64b, {ptrb, ptrb}), {a, b}, "strcmp"); return; }
            if (funcName == "indexOfChar" && na == 2) {
                auto s = pptr(0); auto c = slot(1); if (!s || !c) return;
                lastValue = builder.CreateCall(rt("__tocin_str_index_of_char", i64b, {ptrb, i64b}), {s, c}, "idxof"); return; }
            if (funcName == "intToStr" && na == 1) {
                auto n = slot(0); if (!n) return;
                lastValue = builder.CreateCall(rt("__tocin_int_to_str", ptrb, {i64b}), {n}, "itos"); return; }
            if (funcName == "strToInt" && na == 1) {
                auto s = pptr(0); if (!s) return;
                lastValue = builder.CreateCall(rt("__tocin_str_to_int", i64b, {ptrb}), {s}, "stoi"); return; }
            if (funcName == "charToStr" && na == 1) {
                auto c = slot(0); if (!c) return;
                lastValue = builder.CreateCall(rt("__tocin_char_to_str", ptrb, {i64b}), {c}, "ctos"); return; }
            if (funcName == "toUpper" && na == 1) {
                auto s = pptr(0); if (!s) return;
                lastValue = builder.CreateCall(rt("__tocin_str_to_upper", ptrb, {ptrb}), {s}, "upper"); return; }
            if (funcName == "toLower" && na == 1) {
                auto s = pptr(0); if (!s) return;
                lastValue = builder.CreateCall(rt("__tocin_str_to_lower", ptrb, {ptrb}), {s}, "lower"); return; }
            if (funcName == "strIndexOf" && na == 2) {
                auto s = pptr(0); auto sub = pptr(1); if (!s || !sub) return;
                lastValue = builder.CreateCall(rt("__tocin_str_index_of", i64b, {ptrb, ptrb}), {s, sub}, "idxof2"); return; }
            if (funcName == "strContains" && na == 2) {
                auto s = pptr(0); auto sub = pptr(1); if (!s || !sub) return;
                lastValue = builder.CreateCall(rt("__tocin_str_contains", i64b, {ptrb, ptrb}), {s, sub}, "contains"); return; }
            if (funcName == "startsWith" && na == 2) {
                auto s = pptr(0); auto pre = pptr(1); if (!s || !pre) return;
                lastValue = builder.CreateCall(rt("__tocin_str_starts_with", i64b, {ptrb, ptrb}), {s, pre}, "startsw"); return; }
            if (funcName == "endsWith" && na == 2) {
                auto s = pptr(0); auto suf = pptr(1); if (!s || !suf) return;
                lastValue = builder.CreateCall(rt("__tocin_str_ends_with", i64b, {ptrb, ptrb}), {s, suf}, "endsw"); return; }
            if (funcName == "free" && na == 1) {
                auto p = pptr(0); if (!p) return;
                builder.CreateCall(rt("__tocin_free", voidb, {ptrb}), {p});
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }

            // ---- file I/O ----
            if (funcName == "readFile" && na == 1) {
                auto p = pptr(0); if (!p) return;
                lastValue = builder.CreateCall(rt("__tocin_read_file", ptrb, {ptrb}), {p}, "rdf"); return; }
            if (funcName == "writeFile" && na == 2) {
                auto p = pptr(0); auto c = pptr(1); if (!p || !c) return;
                lastValue = builder.CreateCall(rt("__tocin_write_file", i64b, {ptrb, ptrb}), {p, c}, "wrf"); return; }
            if (funcName == "appendFile" && na == 2) {
                auto p = pptr(0); auto c = pptr(1); if (!p || !c) return;
                lastValue = builder.CreateCall(rt("__tocin_append_file", i64b, {ptrb, ptrb}), {p, c}, "apf"); return; }
            if (funcName == "readLine" && na == 0) {
                lastValue = builder.CreateCall(rt("__tocin_read_line", ptrb, {}), {}, "rdl"); return; }

            // ---- low-level / systems: raw memory ----
            llvm::Type *i8b = llvm::Type::getInt8Ty(context);
            llvm::Type *i32b = llvm::Type::getInt32Ty(context);
            if (funcName == "alloc" && na == 1) {       // raw heap buffer (GC-managed)
                auto n = slot(0); if (!n) return;
                // Return the address as an int so buffers pass cleanly through
                // `int` parameters; the load/store builtins inttoptr it back.
                llvm::Value *mem = builder.CreateCall(rt("__tocin_alloc", ptrb, {i64b}), {n}, "rawbuf");
                lastValue = builder.CreatePtrToInt(mem, i64b, "rawaddr"); return; }
            if (funcName == "memcpy" && na == 3) {
                auto d = pptr(0); auto s = pptr(1); auto n = slot(2); if (!d || !s || !n) return;
                builder.CreateCall(rt("memcpy", ptrb, {ptrb, ptrb, i64b}), {d, s, n});
                lastValue = builder.CreatePtrToInt(d, i64b, "dstaddr"); return; }
            if (funcName == "memset" && na == 3) {
                auto d = pptr(0); auto v = slot(1); auto n = slot(2); if (!d || !n) return;
                llvm::Value *b = builder.CreateTrunc(v, i32b, "byte");
                builder.CreateCall(rt("memset", ptrb, {ptrb, i32b, i64b}), {d, b, n});
                lastValue = builder.CreatePtrToInt(d, i64b, "dstaddr"); return; }
            if (funcName == "ptrAdd" && na == 2) {      // address + byte offset
                auto p = pptr(0); auto off = slot(1); if (!p || !off) return;
                llvm::Value *g = builder.CreateGEP(i8b, p, off, "ptradd");
                lastValue = builder.CreatePtrToInt(g, i64b, "ptraddr"); return; }
            if (funcName == "loadByte" && na == 2) {    // *(u8*)(p+off) zero-extended
                auto p = pptr(0); auto off = slot(1); if (!p || !off) return;
                llvm::Value *g = builder.CreateGEP(i8b, p, off, "lb.p");
                lastValue = builder.CreateZExt(builder.CreateLoad(i8b, g, "lb"), i64b, "lb64"); return; }
            if (funcName == "storeByte" && na == 3) {   // *(u8*)(p+off) = v
                auto p = pptr(0); auto off = slot(1); auto v = slot(2); if (!p || !off || !v) return;
                llvm::Value *g = builder.CreateGEP(i8b, p, off, "sb.p");
                builder.CreateStore(builder.CreateTrunc(v, i8b, "sb.v"), g);
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }
            if (funcName == "loadInt" && na == 2) {     // *(i64*)(p+off)
                auto p = pptr(0); auto off = slot(1); if (!p || !off) return;
                llvm::Value *g = builder.CreateGEP(i8b, p, off, "li.p");
                lastValue = builder.CreateLoad(i64b, g, "li"); return; }
            if (funcName == "storeInt" && na == 3) {    // *(i64*)(p+off) = v
                auto p = pptr(0); auto off = slot(1); auto v = slot(2); if (!p || !off || !v) return;
                llvm::Value *g = builder.CreateGEP(i8b, p, off, "si.p");
                builder.CreateStore(v, g);
                lastValue = llvm::ConstantInt::get(i64b, 0); return; }

            // ---- inline assembly (OS/kernel) ----
            // asm("instructions") emits raw side-effecting inline assembly with
            // no operands/clobbers (e.g. "cli", "hlt", "nop", "sti"). The
            // argument must be a string literal (the template is needed at
            // compile time, not as a runtime pointer).
            if (funcName == "asm" && na == 1) {
                auto lit = std::dynamic_pointer_cast<ast::LiteralExpr>(expr->arguments[0]);
                if (!lit || lit->literalType != ast::LiteralExpr::LiteralType::STRING) {
                    errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                        "asm(...) requires a string-literal instruction template",
                        "", 0, 0, error::ErrorSeverity::ERROR);
                    lastValue = nullptr; return;
                }
                auto *fnty = llvm::FunctionType::get(voidb, false);
                llvm::InlineAsm *ia = llvm::InlineAsm::get(
                    fnty, lit->value, /*constraints=*/"",
                    /*hasSideEffects=*/true, /*isAlignStack=*/false,
                    llvm::InlineAsm::AD_ATT);
                builder.CreateCall(fnty, ia, {});
                lastValue = llvm::ConstantInt::get(i64b, 0); return;
            }
        }

        // Math standard library: unary libm functions (double -> double).
        static const std::set<std::string> unaryMath = {
            "sqrt", "sin", "cos", "tan", "asin", "acos", "atan",
            "exp", "log", "log2", "log10", "floor", "ceil", "round", "fabs"};
        llvm::Type *dbl = llvm::Type::getDoubleTy(context);
        if (unaryMath.count(funcName) && expr->arguments.size() == 1)
        {
            expr->arguments[0]->accept(*this);
            if (!lastValue) return;
            llvm::Value *x = lastValue;
            if (x->getType()->isIntegerTy())
                x = builder.CreateSIToFP(x, dbl, "tofp");
            llvm::Function *f = module->getFunction(funcName);
            if (!f)
                f = llvm::Function::Create(llvm::FunctionType::get(dbl, {dbl}, false),
                                           llvm::Function::ExternalLinkage, funcName, *module);
            lastValue = builder.CreateCall(f, {x}, funcName);
            return;
        }
        if (funcName == "pow" && expr->arguments.size() == 2)
        {
            expr->arguments[0]->accept(*this); llvm::Value *a = lastValue;
            expr->arguments[1]->accept(*this); llvm::Value *b = lastValue;
            if (!a || !b) return;
            if (a->getType()->isIntegerTy()) a = builder.CreateSIToFP(a, dbl, "tofp");
            if (b->getType()->isIntegerTy()) b = builder.CreateSIToFP(b, dbl, "tofp");
            llvm::Function *f = module->getFunction("pow");
            if (!f)
                f = llvm::Function::Create(llvm::FunctionType::get(dbl, {dbl, dbl}, false),
                                           llvm::Function::ExternalLinkage, "pow", *module);
            lastValue = builder.CreateCall(f, {a, b}, "pow");
            return;
        }
        if (funcName == "abs" && expr->arguments.size() == 1)
        {
            expr->arguments[0]->accept(*this);
            if (!lastValue) return;
            llvm::Value *x = lastValue;
            if (x->getType()->isFloatingPointTy())
            {
                llvm::Value *neg = builder.CreateFNeg(x, "fneg");
                llvm::Value *lt = builder.CreateFCmpOLT(x, llvm::ConstantFP::get(x->getType(), 0.0), "ltz");
                lastValue = builder.CreateSelect(lt, neg, x, "fabs");
            }
            else
            {
                llvm::Value *neg = builder.CreateNeg(x, "neg");
                llvm::Value *lt = builder.CreateICmpSLT(x, llvm::ConstantInt::get(x->getType(), 0), "ltz");
                lastValue = builder.CreateSelect(lt, neg, x, "abs");
            }
            return;
        }
        if ((funcName == "min" || funcName == "max") && expr->arguments.size() == 2)
        {
            expr->arguments[0]->accept(*this); llvm::Value *a = lastValue;
            expr->arguments[1]->accept(*this); llvm::Value *b = lastValue;
            if (!a || !b) return;
            bool wantMin = (funcName == "min");
            llvm::Value *cmp;
            if (a->getType()->isFloatingPointTy() || b->getType()->isFloatingPointTy())
                cmp = wantMin ? builder.CreateFCmpOLT(a, b, "cmp") : builder.CreateFCmpOGT(a, b, "cmp");
            else
                cmp = wantMin ? builder.CreateICmpSLT(a, b, "cmp") : builder.CreateICmpSGT(a, b, "cmp");
            lastValue = builder.CreateSelect(cmp, a, b, funcName);
            return;
        }

        // print()/println() are built-ins forwarded to printf. println appends
        // a newline. Two calling styles are supported:
        //   * format style: print("x = {}, y = {}", x, y)  (first arg is a
        //     string literal containing {} placeholders)
        //   * sequential style: print(a, b, c)  (each argument printed in turn)
        if (funcName == "print" || funcName == "println")
        {
            llvm::Function *printfFunc = stdLibFunctions["printf"];
            llvm::Type *i64 = llvm::Type::getInt64Ty(context);
            llvm::Type *dbl = llvm::Type::getDoubleTy(context);
            std::vector<llvm::Value *> printfArgs;
            printfArgs.push_back(nullptr); // reserved for the format string
            std::string format;

            // Helper: coerce a value for a varargs printf slot and append its
            // conversion specifier to the format string.
            auto appendValue = [&](llvm::Value *v) {
                llvm::Type *t = v->getType();
                if (t->isIntegerTy())
                {
                    v = builder.CreateIntCast(v, i64, !t->isIntegerTy(1), "printi");
                    format += "%lld";
                }
                else if (t->isFloatTy() || t->isDoubleTy())
                {
                    if (t->isFloatTy())
                        v = builder.CreateFPExt(v, dbl, "printf_d");
                    format += "%g";
                }
                else if (t->isPointerTy())
                    format += "%s";
                else
                    format += "%p";
                printfArgs.push_back(v);
            };

            // Detect format style: first argument is a string literal with {}.
            ast::LiteralExpr *fmtLit = nullptr;
            if (!expr->arguments.empty())
                if (auto l = std::dynamic_pointer_cast<ast::LiteralExpr>(expr->arguments[0]))
                    if (l->literalType == ast::LiteralExpr::LiteralType::STRING &&
                        l->value.find("{}") != std::string::npos)
                        fmtLit = l.get();

            if (fmtLit)
            {
                // Evaluate the substitution arguments first.
                std::vector<llvm::Value *> argVals;
                for (size_t i = 1; i < expr->arguments.size(); ++i)
                {
                    expr->arguments[i]->accept(*this);
                    if (!lastValue) return;
                    argVals.push_back(lastValue);
                }
                std::string raw = fmtLit->value;
                if (raw.size() >= 2 && (raw.front() == '"' || raw.front() == '\''))
                    raw = raw.substr(1, raw.size() - 2);
                size_t ai = 0;
                for (size_t i = 0; i < raw.size(); ++i)
                {
                    if (raw[i] == '{' && i + 1 < raw.size() && raw[i + 1] == '}')
                    {
                        if (ai < argVals.size()) appendValue(argVals[ai++]);
                        else format += "{}";
                        ++i;
                    }
                    else if (raw[i] == '\\' && i + 1 < raw.size())
                    {
                        char n = raw[++i];
                        format += (n == 'n') ? '\n' : (n == 't') ? '\t' : (n == 'r') ? '\r' : n;
                    }
                    else if (raw[i] == '%') format += "%%";
                    else format += raw[i];
                }
            }
            else
            {
                for (const auto &arg : expr->arguments)
                {
                    arg->accept(*this);
                    if (!lastValue) return;
                    appendValue(lastValue);
                }
            }

            if (funcName == "println")
                format += "\n";
            printfArgs[0] = builder.CreateGlobalString(format, "fmt");
            lastValue = builder.CreateCall(printfFunc->getFunctionType(), printfFunc, printfArgs);
            return;
        }

        // Generic function call: monomorphize for the concrete argument types.
        auto git = genericFunctions.find(funcName);
        if (git != genericFunctions.end())
        {
            ast::FunctionStmt *tmpl = git->second;
            std::vector<llvm::Value *> argVals;
            for (const auto &a : expr->arguments)
            {
                a->accept(*this);
                if (!lastValue) return;
                argVals.push_back(lastValue);
            }
            // Infer type-parameter bindings from argument types.
            std::map<std::string, llvm::Type *> bindings;
            std::set<std::string> tparams;
            for (const auto &tp : tmpl->typeParameters)
                tparams.insert(tp.getName());
            for (size_t i = 0; i < tmpl->parameters.size() && i < argVals.size(); ++i)
            {
                std::string pt = tmpl->parameters[i].type ? tmpl->parameters[i].type->toString() : "";
                if (tparams.count(pt) && !bindings.count(pt))
                    bindings[pt] = argVals[i]->getType();
            }
            for (const auto &tp : tmpl->typeParameters)
                if (!bindings.count(tp.getName()))
                    bindings[tp.getName()] = llvm::Type::getInt64Ty(context);

            llvm::Function *inst = emitGenericInstance(tmpl, bindings);
            llvm::FunctionType *ift = inst->getFunctionType();
            std::vector<llvm::Value *> callArgs;
            for (size_t i = 0; i < argVals.size(); ++i)
            {
                llvm::Value *v = argVals[i];
                if (i < ift->getNumParams() && v->getType() != ift->getParamType(i))
                {
                    llvm::Type *pt = ift->getParamType(i);
                    if (v->getType()->isIntegerTy() && pt->isIntegerTy())
                        v = builder.CreateIntCast(v, pt, true, "argcast");
                    else if (v->getType()->isFloatingPointTy() && pt->isFloatingPointTy())
                        v = builder.CreateFPCast(v, pt, "argcast");
                }
                callArgs.push_back(v);
            }
            lastValue = builder.CreateCall(ift, inst, callArgs);
            return;
        }

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

    // Ensure callee is a function (direct) or a closure value (indirect).
    bool isClosure = callee->getType()->isPointerTy() && !llvm::isa<llvm::Function>(callee);
    llvm::FunctionType *funcType = nullptr;
    if (auto func = llvm::dyn_cast<llvm::Function>(callee))
        funcType = func->getFunctionType();           // direct / named function value
    else if (callee->getType()->isPointerTy())
        funcType = recoverCalleeFnType(expr->callee); // closure value: declared (params)->ret
    if (!funcType)
    {
        errorHandler.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                 "Called value is not a function",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Coerce an evaluated argument to a declared parameter type.
    auto coerceArg = [&](llvm::Value *v, llvm::Type *pt) -> llvm::Value * {
        v = wrapIfRawFunction(v); // a bare function passed by value -> closure
        if (pt && v->getType() != pt)
        {
            if (v->getType()->isIntegerTy() && pt->isIntegerTy())
                v = builder.CreateIntCast(v, pt, true, "argcast");
            else if (v->getType()->isFloatingPointTy() && pt->isFloatingPointTy())
                v = builder.CreateFPCast(v, pt, "argcast");
            else if (v->getType()->isPointerTy() && pt->isPointerTy())
                v = builder.CreateBitCast(v, pt, "argcast");
        }
        return v;
    };

    if (isClosure)
    {
        // Closure ABI: fn lives at offset 0; the closure itself is the env arg.
        llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
        llvm::Value *fnPtr = builder.CreateLoad(ptrTy, callee, "clo.fn");
        std::vector<llvm::Type *> cps;
        cps.push_back(ptrTy);
        for (auto *p : funcType->params())
            cps.push_back(p);
        llvm::FunctionType *cloType =
            llvm::FunctionType::get(funcType->getReturnType(), cps, false);

        std::vector<llvm::Value *> args;
        args.push_back(callee); // env
        for (size_t i = 0; i < expr->arguments.size(); ++i)
        {
            expr->arguments[i]->accept(*this);
            if (!lastValue)
                return;
            llvm::Type *pt = i < funcType->getNumParams() ? funcType->getParamType(i) : nullptr;
            args.push_back(coerceArg(lastValue, pt));
        }
        lastValue = builder.CreateCall(cloType, fnPtr, args);
        return;
    }

    // Direct call.
    std::vector<llvm::Value *> args;
    for (size_t i = 0; i < expr->arguments.size(); ++i)
    {
        expr->arguments[i]->accept(*this);
        if (!lastValue)
            return;
        llvm::Type *pt = i < funcType->getNumParams() ? funcType->getParamType(i) : nullptr;
        args.push_back(coerceArg(lastValue, pt));
    }
    lastValue = builder.CreateCall(funcType, callee, args);
}

void IRGenerator::visitIfStmt(ast::IfStmt *stmt)
{
    llvm::Function *function = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *mergeBlock = llvm::BasicBlock::Create(context, "ifcont");

    // Collect all (condition, body) clauses: the leading `if` plus each `elif`.
    std::vector<std::pair<ast::ExprPtr, ast::StmtPtr>> clauses;
    clauses.emplace_back(stmt->condition, stmt->thenBranch);
    for (auto &eb : stmt->elifBranches)
        clauses.emplace_back(eb.first, eb.second);

    auto toBool = [&](llvm::Value *v) -> llvm::Value * {
        if (!v) return nullptr;
        if (v->getType()->isIntegerTy(1)) return v;
        if (v->getType()->isIntegerTy())
            return builder.CreateICmpNE(v, llvm::ConstantInt::get(v->getType(), 0), "ifcond");
        if (v->getType()->isFloatTy() || v->getType()->isDoubleTy())
            return builder.CreateFCmpONE(v, llvm::ConstantFP::get(v->getType(), 0.0), "ifcond");
        if (v->getType()->isPointerTy())
            return builder.CreateICmpNE(
                v, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(v->getType())), "ifcond");
        return nullptr;
    };

    for (size_t i = 0; i < clauses.size(); ++i)
    {
        // Evaluate this clause's condition in the current insert block.
        clauses[i].first->accept(*this);
        llvm::Value *cond = toBool(lastValue);
        if (!cond)
        {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Condition must be convertible to a boolean",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            return;
        }

        llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(context, "then", function);
        bool lastClause = (i + 1 == clauses.size());
        llvm::BasicBlock *falseBlock;
        if (!lastClause)
            falseBlock = llvm::BasicBlock::Create(context, "elif.cond", function);
        else if (stmt->elseBranch)
            falseBlock = llvm::BasicBlock::Create(context, "else", function);
        else
            falseBlock = mergeBlock;

        builder.CreateCondBr(cond, thenBlock, falseBlock);

        // Then body.
        builder.SetInsertPoint(thenBlock);
        createEnvironment();
        if (clauses[i].second)
            clauses[i].second->accept(*this);
        restoreEnvironment();
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(mergeBlock);

        // Emit the next condition (or the else body) into the false block.
        if (falseBlock != mergeBlock)
            builder.SetInsertPoint(falseBlock);
        if (lastClause && stmt->elseBranch)
        {
            createEnvironment();
            stmt->elseBranch->accept(*this);
            restoreEnvironment();
            if (!builder.GetInsertBlock()->getTerminator())
                builder.CreateBr(mergeBlock);
        }
    }

    mergeBlock->insertInto(function);
    builder.SetInsertPoint(mergeBlock);
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

    // break -> afterBlock, continue -> condBlock (re-checks the condition).
    loopStack.push_back({condBlock, afterBlock});
    stmt->body->accept(*this);
    loopStack.pop_back();

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

    // Range-based for loop: `for v in start..end` -> integer counting loop.
    if (auto rangeExpr = std::dynamic_pointer_cast<ast::BinaryExpr>(stmt->iterable))
    {
        if (rangeExpr->op.type == lexer::TokenType::RANGE)
        {
            llvm::Type *i64 = llvm::Type::getInt64Ty(context);
            rangeExpr->left->accept(*this);
            llvm::Value *startV = lastValue;
            if (!startV) return;
            if (!startV->getType()->isIntegerTy(64))
                startV = builder.CreateIntCast(startV, i64, true, "range.start");
            rangeExpr->right->accept(*this);
            llvm::Value *endV = lastValue;
            if (!endV) return;
            if (!endV->getType()->isIntegerTy(64))
                endV = builder.CreateIntCast(endV, i64, true, "range.end");

            llvm::Function *fn = builder.GetInsertBlock()->getParent();
            llvm::AllocaInst *iterVar = builder.CreateAlloca(i64, nullptr, variable);
            builder.CreateStore(startV, iterVar);
            namedValues[variable] = iterVar;

            llvm::BasicBlock *condBB = llvm::BasicBlock::Create(context, "for.cond", fn);
            llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(context, "for.body", fn);
            llvm::BasicBlock *incBB = llvm::BasicBlock::Create(context, "for.inc", fn);
            llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(context, "for.after", fn);

            builder.CreateBr(condBB);
            builder.SetInsertPoint(condBB);
            llvm::Value *cur = builder.CreateLoad(i64, iterVar, variable);
            llvm::Value *cond = builder.CreateICmpSLT(cur, endV, "for.cmp");
            builder.CreateCondBr(cond, bodyBB, afterBB);

            builder.SetInsertPoint(bodyBB);
            // continue -> incBB (advances the counter), break -> afterBB
            loopStack.push_back({incBB, afterBB});
            if (stmt->body) stmt->body->accept(*this);
            loopStack.pop_back();
            if (!builder.GetInsertBlock()->getTerminator())
                builder.CreateBr(incBB);

            builder.SetInsertPoint(incBB);
            llvm::Value *c2 = builder.CreateLoad(i64, iterVar, variable);
            llvm::Value *next = builder.CreateAdd(c2, llvm::ConstantInt::get(i64, 1), "for.next");
            builder.CreateStore(next, iterVar);
            builder.CreateBr(condBB);

            builder.SetInsertPoint(afterBB);
            return;
        }
    }

    llvm::Function *function = builder.GetInsertBlock()->getParent();
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Type *i8 = llvm::Type::getInt8Ty(context);

    // Evaluate the iterable expression -> base pointer of a flat array
    // laid out as [i64 length][elem0][elem1]... (same layout as visitIndexExpr).
    stmt->iterable->accept(*this);
    if (!lastValue)
        return;
    llvm::Value *iterableValue = lastValue;

    // Element type: prefer the declared loop-variable type, else infer from the
    // iterable's recorded element type.
    llvm::Type *elemTy = (variableType && getLLVMType(variableType))
                             ? getLLVMType(variableType)
                             : getArrayElemType(stmt->iterable);
    if (!elemTy) elemTy = i64;
    llvm::Value *elemSize = llvm::ConstantExpr::getSizeOf(elemTy);

    llvm::AllocaInst *iterVar = builder.CreateAlloca(elemTy, nullptr, variable);
    namedValues[variable] = iterVar;

    // Counter variable.
    llvm::AllocaInst *indexVar = builder.CreateAlloca(i64, nullptr, "loop.index");
    builder.CreateStore(llvm::ConstantInt::get(i64, 0), indexVar);

    // Length lives at offset 0 of the array.
    llvm::Value *length = builder.CreateLoad(i64, iterableValue, "length");

    // Proper cond / body / inc / after structure so break & continue work.
    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(context, "loop.cond", function);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(context, "loop.body", function);
    llvm::BasicBlock *incBlock = llvm::BasicBlock::Create(context, "loop.inc", function);
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(context, "loop.after", function);

    builder.CreateBr(condBlock);

    // Condition: index < length
    builder.SetInsertPoint(condBlock);
    llvm::Value *index = builder.CreateLoad(i64, indexVar, "index");
    llvm::Value *cond = builder.CreateICmpSLT(index, length, "loop.cond");
    builder.CreateCondBr(cond, bodyBlock, afterBlock);

    // Body: load element at offset 8 + index*sizeof(elem) into the loop var.
    builder.SetInsertPoint(bodyBlock);
    index = builder.CreateLoad(i64, indexVar);
    llvm::Value *off = builder.CreateAdd(
        llvm::ConstantInt::get(i64, 8),
        builder.CreateMul(index, elemSize), "elem.off");
    llvm::Value *elementPtr = builder.CreateGEP(i8, iterableValue, off, "element.ptr");
    llvm::Value *element = builder.CreateLoad(elemTy, elementPtr, "element");
    builder.CreateStore(element, iterVar);

    // continue -> incBlock (advances the index), break -> afterBlock
    loopStack.push_back({incBlock, afterBlock});
    if (stmt->body) stmt->body->accept(*this);
    loopStack.pop_back();
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(incBlock);

    // Increment index, loop back.
    builder.SetInsertPoint(incBlock);
    index = builder.CreateLoad(llvm::Type::getInt64Ty(context), indexVar);
    llvm::Value *nextIndex = builder.CreateAdd(
        index, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1), "next.index");
    builder.CreateStore(nextIndex, indexVar);
    builder.CreateBr(condBlock);

    // After the loop
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
    if (!expr->right) {
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Unary expression missing operand",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate the operand first
    expr->right->accept(*this);
    llvm::Value *operand = lastValue;

    if (!operand) {
        lastValue = nullptr;
        return;
    }

    switch (expr->op.type)
    {
    case lexer::TokenType::MINUS:
        if (operand->getType()->isIntegerTy()) {
            lastValue = builder.CreateNeg(operand, "neg");
        } else if (operand->getType()->isFloatTy() || operand->getType()->isDoubleTy()) {
            lastValue = builder.CreateFNeg(operand, "fneg");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot apply unary minus to non-numeric type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::BANG:
        if (operand->getType()->isIntegerTy(1)) {
            lastValue = builder.CreateNot(operand, "not");
        } else {
            // Convert to boolean if needed
            llvm::Value *boolVal = builder.CreateICmpNE(operand, 
                llvm::ConstantInt::get(operand->getType(), 0), "tobool");
            lastValue = builder.CreateNot(boolVal, "not");
        }
        break;

    case lexer::TokenType::BANG_BANG:
    {
        // Force-unwrap: abort if the operand is null, otherwise yield it.
        if (!operand->getType()->isPointerTy()) {
            // Non-pointer values can never be null; identity.
            lastValue = operand;
            break;
        }
        llvm::Function *fn = builder.GetInsertBlock()->getParent();
        llvm::Value *isNull = builder.CreateICmpEQ(
            operand,
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(operand->getType())),
            "uw.isnull");
        llvm::BasicBlock *nullBB = llvm::BasicBlock::Create(context, "unwrap.null", fn);
        llvm::BasicBlock *okBB = llvm::BasicBlock::Create(context, "unwrap.ok", fn);
        builder.CreateCondBr(isNull, nullBB, okBB);
        builder.SetInsertPoint(nullBB);
        llvm::Function *abortFn = module->getFunction("abort");
        if (!abortFn)
            abortFn = llvm::Function::Create(
                llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false),
                llvm::Function::ExternalLinkage, "abort", *module);
        builder.CreateCall(abortFn, {});
        builder.CreateUnreachable();
        builder.SetInsertPoint(okBB);
        lastValue = operand;
        break;
    }

    case lexer::TokenType::BITWISE_NOT:
        if (operand->getType()->isIntegerTy()) {
            lastValue = builder.CreateNot(operand, "bitnot");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot apply bitwise NOT to non-integer type",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::INCREMENT:
    case lexer::TokenType::DECREMENT:
        // Handle increment/decrement with proper lvalue support
        if (auto varExpr = dynamic_cast<ast::VariableExpr*>(expr->right.get())) {
            // Get the variable's current value
            llvm::Value* varPtr = getVariable(varExpr->name);
            if (!varPtr) {
                errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                         "Variable '" + varExpr->name + "' not found",
                                         "", 0, 0, error::ErrorSeverity::ERROR);
                lastValue = nullptr;
                return;
            }

            llvm::Value* currentValue = builder.CreateLoad(operand->getType(), varPtr, "load");
            
            // Create the new value
            llvm::Value* newValue;
            if (expr->op.type == lexer::TokenType::INCREMENT) {
                if (currentValue->getType()->isIntegerTy()) {
                    newValue = builder.CreateAdd(currentValue, 
                        llvm::ConstantInt::get(currentValue->getType(), 1), "inc");
                } else if (currentValue->getType()->isFloatTy() || currentValue->getType()->isDoubleTy()) {
                    newValue = builder.CreateFAdd(currentValue, 
                        llvm::ConstantFP::get(currentValue->getType(), 1.0), "finc");
                } else {
                    errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                             "Cannot increment non-numeric type",
                                             "", 0, 0, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                    return;
                }
            } else { // DECREMENT
                if (currentValue->getType()->isIntegerTy()) {
                    newValue = builder.CreateSub(currentValue, 
                        llvm::ConstantInt::get(currentValue->getType(), 1), "dec");
                } else if (currentValue->getType()->isFloatTy() || currentValue->getType()->isDoubleTy()) {
                    newValue = builder.CreateFSub(currentValue, 
                        llvm::ConstantFP::get(currentValue->getType(), 1.0), "fdec");
                } else {
                    errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                             "Cannot decrement non-numeric type",
                                             "", 0, 0, error::ErrorSeverity::ERROR);
                    lastValue = nullptr;
                    return;
                }
            }

            // Store the new value
            builder.CreateStore(newValue, varPtr);
            
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

// --- Free-variable analysis for closure capture --------------------------
// Collect every identifier referenced (`used`) and every name bound locally
// (`bound`) within an expression / statement subtree. The capture set is then
// `used - bound - params`, intersected with the enclosing scope by the caller.
namespace
{
    void collectStmtNames(const ast::StmtPtr &stmt,
                          std::set<std::string> &used,
                          std::set<std::string> &bound);

    void collectExprNames(const ast::ExprPtr &expr,
                          std::set<std::string> &used,
                          std::set<std::string> &bound)
    {
        if (!expr)
            return;
        if (auto v = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
            used.insert(v->name);
        else if (auto b = std::dynamic_pointer_cast<ast::BinaryExpr>(expr))
        {
            collectExprNames(b->left, used, bound);
            collectExprNames(b->right, used, bound);
        }
        else if (auto u = std::dynamic_pointer_cast<ast::UnaryExpr>(expr))
            collectExprNames(u->right, used, bound);
        else if (auto g = std::dynamic_pointer_cast<ast::GroupingExpr>(expr))
            collectExprNames(g->expression, used, bound);
        else if (auto c = std::dynamic_pointer_cast<ast::CallExpr>(expr))
        {
            collectExprNames(c->callee, used, bound);
            for (auto &a : c->arguments)
                collectExprNames(a, used, bound);
        }
        else if (auto gx = std::dynamic_pointer_cast<ast::GetExpr>(expr))
            collectExprNames(gx->object, used, bound);
        else if (auto sx = std::dynamic_pointer_cast<ast::SetExpr>(expr))
        {
            collectExprNames(sx->object, used, bound);
            collectExprNames(sx->value, used, bound);
        }
        else if (auto ix = std::dynamic_pointer_cast<ast::IndexExpr>(expr))
        {
            collectExprNames(ix->object, used, bound);
            collectExprNames(ix->index, used, bound);
        }
        else if (auto ax = std::dynamic_pointer_cast<ast::AssignExpr>(expr))
        {
            if (!ax->name.empty())
                used.insert(ax->name);
            collectExprNames(ax->target, used, bound);
            collectExprNames(ax->value, used, bound);
        }
        else if (auto lx = std::dynamic_pointer_cast<ast::ListExpr>(expr))
        {
            for (auto &e : lx->elements)
                collectExprNames(e, used, bound);
        }
        else if (auto aw = std::dynamic_pointer_cast<ast::AwaitExpr>(expr))
            collectExprNames(aw->expression, used, bound);
        else if (auto si = std::dynamic_pointer_cast<ast::StringInterpolationExpr>(expr))
        {
            for (auto &e : si->getExpressions())
                collectExprNames(e, used, bound);
        }
        else if (auto nested = std::dynamic_pointer_cast<ast::LambdaExpr>(expr))
        {
            // A nested lambda's own params are bound within it; its free vars
            // are still free here (and may need capturing transitively).
            std::set<std::string> innerBound = bound;
            for (auto &p : nested->parameters)
                innerBound.insert(p.name);
            collectExprNames(nested->body, used, innerBound);
        }
    }

    void collectStmtNames(const ast::StmtPtr &stmt,
                          std::set<std::string> &used,
                          std::set<std::string> &bound)
    {
        if (!stmt)
            return;
        if (auto es = std::dynamic_pointer_cast<ast::ExpressionStmt>(stmt))
            collectExprNames(es->expression, used, bound);
        else if (auto vs = std::dynamic_pointer_cast<ast::VariableStmt>(stmt))
        {
            collectExprNames(vs->initializer, used, bound);
            bound.insert(vs->name);
        }
        else if (auto bs = std::dynamic_pointer_cast<ast::BlockStmt>(stmt))
        {
            for (auto &s : bs->statements)
                collectStmtNames(s, used, bound);
        }
        else if (auto is = std::dynamic_pointer_cast<ast::IfStmt>(stmt))
        {
            collectExprNames(is->condition, used, bound);
            collectStmtNames(is->thenBranch, used, bound);
            for (auto &eb : is->elifBranches)
            {
                collectExprNames(eb.first, used, bound);
                collectStmtNames(eb.second, used, bound);
            }
            collectStmtNames(is->elseBranch, used, bound);
        }
        else if (auto ws = std::dynamic_pointer_cast<ast::WhileStmt>(stmt))
        {
            collectExprNames(ws->condition, used, bound);
            collectStmtNames(ws->body, used, bound);
        }
        else if (auto fs = std::dynamic_pointer_cast<ast::ForStmt>(stmt))
        {
            collectExprNames(fs->iterable, used, bound);
            bound.insert(fs->variable);
            collectStmtNames(fs->body, used, bound);
        }
        else if (auto rs = std::dynamic_pointer_cast<ast::ReturnStmt>(stmt))
            collectExprNames(rs->value, used, bound);
    }
} // namespace

void IRGenerator::visitLambdaExpr(ast::LambdaExpr *expr)
{
    llvm::Type *ptrTy = llvm::PointerType::get(context, 0);

    // Return type.
    llvm::Type *returnType = getLLVMType(expr->returnType);
    if (!returnType)
        return;

    // Declared parameter types.
    std::vector<llvm::Type *> declParamTypes;
    for (const auto &param : expr->parameters)
    {
        llvm::Type *paramType = getLLVMType(param.type);
        if (!paramType)
            return;
        declParamTypes.push_back(paramType);
    }

    // --- Determine captured variables --------------------------------------
    // Free identifiers referenced in the body that resolve to a variable in the
    // enclosing scope (and are not parameters or locally bound) are captured by
    // value into the closure environment.
    std::set<std::string> used, bound;
    for (auto &p : expr->parameters)
        bound.insert(p.name);
    collectExprNames(expr->body, used, bound);

    std::vector<std::string> captureNames;
    std::vector<llvm::Value *> captureVals; // loaded in the *enclosing* scope
    std::vector<llvm::Type *> captureTypes;
    for (const auto &name : used)
    {
        if (bound.count(name))
            continue;
        llvm::AllocaInst *slot = lookupVariable(name);
        if (!slot)
            continue; // not a local (e.g. a global function called inside)
        captureNames.push_back(name);
        captureTypes.push_back(slot->getAllocatedType());
        captureVals.push_back(
            builder.CreateLoad(slot->getAllocatedType(), slot, name + ".cap"));
    }

    // --- Build the lambda function with the env-first ABI ------------------
    std::vector<llvm::Type *> fnParamTypes;
    fnParamTypes.push_back(ptrTy); // hidden environment pointer
    for (auto *t : declParamTypes)
        fnParamTypes.push_back(t);
    llvm::FunctionType *functionType =
        llvm::FunctionType::get(returnType, fnParamTypes, false);

    static int lambdaCounter = 0;
    std::string lambdaName = "lambda_" + std::to_string(lambdaCounter++);
    llvm::Function *function = llvm::Function::Create(
        functionType, llvm::Function::InternalLinkage, lambdaName, module.get());

    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", function);
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    llvm::Function *savedFunction = currentFunction;
    auto savedNamedValues = namedValues;
    auto savedVarClasses = varClasses;
    auto savedVarArrayElem = varArrayElem;
    auto savedVarFuncSig = varFuncSig;
    auto savedVarIsString = varIsString;

    builder.SetInsertPoint(block);
    currentFunction = function;
    // Inside the lambda only params + captures are in scope.
    namedValues.clear();
    varFuncSig.clear();
    varIsString.clear();

    // arg 0 is the environment; load each captured value back out of it.
    llvm::Argument *envArg = function->getArg(0);
    envArg->setName("env");
    for (size_t i = 0; i < captureNames.size(); ++i)
    {
        // Captures live at offset 8*(i+1) (offset 0 holds the fn pointer).
        llvm::Value *off = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 8 * (i + 1));
        llvm::Value *p = builder.CreateGEP(llvm::Type::getInt8Ty(context), envArg, off, captureNames[i] + ".slot");
        llvm::Value *val = builder.CreateLoad(captureTypes[i], p, captureNames[i]);
        llvm::AllocaInst *a = createEntryBlockAlloca(function, captureNames[i], captureTypes[i]);
        builder.CreateStore(val, a);
        namedValues[captureNames[i]] = a;
    }

    // Bind declared parameters (args 1..N).
    for (size_t i = 0; i < expr->parameters.size(); ++i)
    {
        llvm::Argument *arg = function->getArg(i + 1);
        arg->setName(expr->parameters[i].name);
        llvm::AllocaInst *alloca = createEntryBlockAlloca(
            function, expr->parameters[i].name, arg->getType());
        builder.CreateStore(arg, alloca);
        namedValues[expr->parameters[i].name] = alloca;
        recordArrayParam(expr->parameters[i].name, expr->parameters[i].type);
    }

    // Codegen body.
    expr->body->accept(*this);

    // Implicit return.
    if (!builder.GetInsertBlock()->getTerminator())
    {
        if (returnType->isVoidTy())
            builder.CreateRetVoid();
        else if (lastValue && lastValue->getType() == returnType)
            builder.CreateRet(lastValue);
        else if (returnType->isIntegerTy())
            builder.CreateRet(llvm::ConstantInt::get(returnType, 0));
        else if (returnType->isFloatTy() || returnType->isDoubleTy())
            builder.CreateRet(llvm::ConstantFP::get(returnType, 0.0));
        else if (returnType->isPointerTy())
            builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(returnType)));
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

    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Lambda verification failed",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        function->eraseFromParent();
        lastValue = nullptr;
        return;
    }

    // Restore the enclosing scope.
    namedValues = savedNamedValues;
    varClasses = savedVarClasses;
    varArrayElem = savedVarArrayElem;
    varFuncSig = savedVarFuncSig;
    varIsString = savedVarIsString;
    currentFunction = savedFunction;
    if (savedBlock)
        builder.SetInsertPoint(savedBlock);

    // Box the function + captured values into a heap closure object.
    lastValue = makeClosure(function, captureVals);
}

void IRGenerator::visitListExpr(ast::ListExpr *expr)
{
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Function *mallocFunc = getStdLibFunction("malloc");

    // Determine the element type from the first element (default to i64).
    llvm::Type *elemTy = i64;
    std::vector<llvm::Value *> elems;
    for (auto &e : expr->elements)
    {
        e->accept(*this);
        if (!lastValue) return;
        elems.push_back(lastValue);
    }
    if (!elems.empty())
        elemTy = elems[0]->getType();

    // Layout: [ i64 length ][ elem0 ][ elem1 ]...  in one heap block.
    llvm::Value *count = llvm::ConstantInt::get(i64, elems.size());
    llvm::Value *elemSize = llvm::ConstantExpr::getSizeOf(elemTy);
    llvm::Value *header = llvm::ConstantInt::get(i64, 8);
    llvm::Value *total = builder.CreateAdd(header, builder.CreateMul(count, elemSize), "arr.size");
    llvm::Value *base = builder.CreateCall(mallocFunc->getFunctionType(), mallocFunc, {total}, "arr");

    // Store the length in the header.
    builder.CreateStore(count, base);

    // Store each element at offset 8 + i*sizeof(elem).
    llvm::Type *i8 = llvm::Type::getInt8Ty(context);
    for (size_t i = 0; i < elems.size(); ++i)
    {
        llvm::Value *v = elems[i];
        if (v->getType() != elemTy)
        {
            if (v->getType()->isIntegerTy() && elemTy->isIntegerTy())
                v = builder.CreateIntCast(v, elemTy, true, "ecast");
            else if (v->getType()->isFloatingPointTy() && elemTy->isFloatingPointTy())
                v = builder.CreateFPCast(v, elemTy, "ecast");
        }
        llvm::Value *off = builder.CreateAdd(
            header, builder.CreateMul(llvm::ConstantInt::get(i64, i), elemSize), "arr.off");
        llvm::Value *slot = builder.CreateGEP(i8, base, off, "arr.slot");
        builder.CreateStore(v, slot);
    }

    lastValue = base;
    lastExprArrayElem = elemTy;
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
        // Generic classes are instantiated on demand; nothing to emit yet.
        return;
    }

    // Build the LLVM struct type from the declared fields (in order).
    ClassInfo info;
    std::vector<llvm::Type *> fieldTypes;
    for (const auto &fieldStmt : stmt->fields)
    {
        auto var = std::dynamic_pointer_cast<ast::VariableStmt>(fieldStmt);
        if (!var)
            continue;
        llvm::Type *ft = var->type ? getLLVMType(var->type) : llvm::Type::getInt64Ty(context);
        info.memberNames.push_back(var->name);
        info.memberTypes[var->name] = ft;
        fieldTypes.push_back(ft);
    }

    llvm::StructType *structType = llvm::StructType::getTypeByName(context, stmt->name);
    if (!structType)
        structType = llvm::StructType::create(context, fieldTypes, stmt->name);
    else
        structType->setBody(fieldTypes);

    info.classType = structType;
    info.baseClass = nullptr;
    classTypes[stmt->name] = info;

    // Generate each method (with an implicit leading 'this' parameter).
    for (const auto &methodStmt : stmt->methods)
    {
        if (auto method = std::dynamic_pointer_cast<ast::FunctionStmt>(methodStmt))
        {
            generateMethod(stmt->name, structType, method.get());
        }
    }
}

std::string IRGenerator::getExprClassName(const ast::ExprPtr &expr)
{
    if (!expr)
        return "";
    if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
    {
        if (var->name == "self" || var->name == "this")
            return currentClassName;
        auto it = varClasses.find(var->name);
        if (it != varClasses.end())
            return it->second;
        return "";
    }
    if (auto call = std::dynamic_pointer_cast<ast::CallExpr>(expr))
    {
        // Generic constructor call resolved earlier to a mangled class name.
        auto gc = genericCtorClass.find(call.get());
        if (gc != genericCtorClass.end())
            return gc->second;
        // Constructor call: ClassName(...)
        if (auto callee = std::dynamic_pointer_cast<ast::VariableExpr>(call->callee))
            if (classTypes.count(callee->name))
                return callee->name;
    }
    if (auto get = std::dynamic_pointer_cast<ast::GetExpr>(expr))
    {
        // Field whose declared type is itself a class.
        std::string objClass = getExprClassName(get->object);
        auto cit = classTypes.find(objClass);
        if (cit != classTypes.end())
        {
            // Field type names aren't tracked symbolically; best effort only.
            (void)cit;
        }
    }
    // Null-safety operators preserve the operand's class: `a ?: b` and `a!!`.
    if (auto bin = std::dynamic_pointer_cast<ast::BinaryExpr>(expr))
    {
        if (bin->op.type == lexer::TokenType::ELVIS ||
            bin->op.type == lexer::TokenType::NULL_COALESCE)
        {
            std::string c = getExprClassName(bin->left);
            return !c.empty() ? c : getExprClassName(bin->right);
        }
    }
    if (auto un = std::dynamic_pointer_cast<ast::UnaryExpr>(expr))
    {
        if (un->op.type == lexer::TokenType::BANG_BANG)
            return getExprClassName(un->right);
    }
    return "";
}

llvm::StructType *IRGenerator::getOptResType()
{
    if (!optResTy)
    {
        llvm::Type *i64 = llvm::Type::getInt64Ty(context);
        optResTy = llvm::StructType::create(context, {i64, i64}, "tocin.optres");
    }
    return optResTy;
}

llvm::Value *IRGenerator::normalizeToSlot(llvm::Value *v)
{
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    if (v->getType()->isPointerTy())
        return builder.CreatePtrToInt(v, i64, "slot");
    if (v->getType()->isDoubleTy())
        return builder.CreateBitCast(v, i64, "slot");
    if (v->getType()->isIntegerTy() && v->getType() != i64)
        return builder.CreateSExt(v, i64, "slot");
    return v;
}

// Allocate a { tag, payload } Option/Result object on the heap.
llvm::Value *IRGenerator::makeOptRes(int64_t tag, llvm::Value *payload)
{
    llvm::StructType *st = getOptResType();
    llvm::Type *i32 = llvm::Type::getInt32Ty(context);
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Function *mallocFn = stdLibFunctions["malloc"];
    llvm::Value *size = llvm::ConstantExpr::getSizeOf(st);
    llvm::Value *obj = builder.CreateCall(mallocFn->getFunctionType(), mallocFn, {size}, "optres");
    llvm::Value *tagP = builder.CreateGEP(st, obj,
        {llvm::ConstantInt::get(i32, 0), llvm::ConstantInt::get(i32, 0)}, "tagp");
    builder.CreateStore(llvm::ConstantInt::get(i64, tag), tagP);
    llvm::Value *payP = builder.CreateGEP(st, obj,
        {llvm::ConstantInt::get(i32, 0), llvm::ConstantInt::get(i32, 1)}, "payp");
    builder.CreateStore(payload ? normalizeToSlot(payload) : llvm::ConstantInt::get(i64, 0), payP);
    return obj;
}

llvm::FunctionType *IRGenerator::llvmFnTypeOf(const std::shared_ptr<ast::FunctionType> &ft)
{
    if (!ft)
        return nullptr;
    std::vector<llvm::Type *> params;
    for (const auto &p : ft->parameterTypes)
    {
        llvm::Type *t = getLLVMType(p);
        params.push_back(t ? t : llvm::Type::getInt64Ty(context));
    }
    llvm::Type *ret = ft->returnType ? getLLVMType(ft->returnType) : llvm::Type::getVoidTy(context);
    if (!ret)
        ret = llvm::Type::getVoidTy(context);
    return llvm::FunctionType::get(ret, params, false);
}

// Recover the LLVM function type for an indirect call from the callee
// expression: a variable whose declared type is a function type.
llvm::FunctionType *IRGenerator::recoverCalleeFnType(const ast::ExprPtr &callee)
{
    if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(callee))
    {
        auto it = varFuncSig.find(var->name);
        if (it != varFuncSig.end())
            return llvmFnTypeOf(it->second);
    }
    return nullptr;
}

void IRGenerator::recordArrayParam(const std::string &name, const ast::TypePtr &type)
{
    if (auto g = std::dynamic_pointer_cast<ast::GenericType>(type))
    {
        if ((g->name == "list" || g->name == "array" || g->name == "List" || g->name == "Array") &&
            !g->typeArguments.empty())
            varArrayElem[name] = getLLVMType(g->typeArguments[0]);
    }
    // Record a function-typed binding so indirect calls through it can recover
    // the callee signature.
    if (auto ft = std::dynamic_pointer_cast<ast::FunctionType>(type))
        varFuncSig[name] = ft;
    // Record string parameters so == / != compare by value.
    if (type && type->toString() == "string")
        varIsString.insert(name);
    // Record class-typed parameters so field/method access resolves the class.
    if (type && classTypes.count(type->toString()))
        varClasses[name] = type->toString();
}

llvm::Value *IRGenerator::makeClosure(llvm::Function *fn,
                                      const std::vector<llvm::Value *> &caps)
{
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Type *i8 = llvm::Type::getInt8Ty(context);
    // Layout: [ ptr fn ][ cap0 ][ cap1 ]...  one 8-byte slot each.
    llvm::Value *size = llvm::ConstantInt::get(i64, 8 * (1 + caps.size()));
    llvm::Function *mallocF = getStdLibFunction("malloc");
    llvm::Value *mem = builder.CreateCall(mallocF->getFunctionType(), mallocF, {size}, "closure");
    builder.CreateStore(fn, mem); // fn pointer at offset 0
    for (size_t i = 0; i < caps.size(); ++i)
    {
        llvm::Value *off = llvm::ConstantInt::get(i64, 8 * (i + 1));
        llvm::Value *slot = builder.CreateGEP(i8, mem, off, "cap.slot");
        builder.CreateStore(caps[i], slot);
    }
    return mem;
}

llvm::Function *IRGenerator::getOrCreateThunk(llvm::Function *target)
{
    auto it = thunks.find(target);
    if (it != thunks.end())
        return it->second;

    llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
    llvm::FunctionType *tft = target->getFunctionType();
    // Thunk signature: (ptr env, <target params...>) -> target ret.
    std::vector<llvm::Type *> ps;
    ps.push_back(ptrTy);
    for (auto *p : tft->params())
        ps.push_back(p);
    llvm::FunctionType *thunkType =
        llvm::FunctionType::get(tft->getReturnType(), ps, false);
    llvm::Function *thunk = llvm::Function::Create(
        thunkType, llvm::Function::InternalLinkage,
        target->getName() + "$thunk", module.get());

    // Save/restore the builder state — thunks are emitted lazily mid-codegen.
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    llvm::Function *savedFunction = currentFunction;
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", thunk);
    builder.SetInsertPoint(entry);
    currentFunction = thunk;

    std::vector<llvm::Value *> callArgs;
    for (unsigned i = 1; i < thunk->arg_size(); ++i) // skip env (arg 0)
        callArgs.push_back(thunk->getArg(i));
    llvm::Value *r = builder.CreateCall(tft, target, callArgs);
    if (tft->getReturnType()->isVoidTy())
        builder.CreateRetVoid();
    else
        builder.CreateRet(r);

    currentFunction = savedFunction;
    if (savedBlock)
        builder.SetInsertPoint(savedBlock);

    thunks[target] = thunk;
    return thunk;
}

llvm::Value *IRGenerator::wrapIfRawFunction(llvm::Value *v)
{
    // A bare top-level function used as a value becomes a closure {thunk}.
    if (auto fn = llvm::dyn_cast_or_null<llvm::Function>(v))
        return makeClosure(getOrCreateThunk(fn), {});
    return v;
}

void IRGenerator::runPendingFinally()
{
    if (finallyStack.empty())
        return;
    // Emit each pending cleanup inline, innermost first. Clear the stack for
    // the duration so a `return` inside a finally does not re-run it.
    auto pending = finallyStack;
    finallyStack.clear();
    llvm::Value *savedLast = lastValue;
    for (auto it = pending.rbegin(); it != pending.rend(); ++it)
    {
        if (builder.GetInsertBlock()->getTerminator())
            break;
        // Pop the still-active exception handler for this try, if any.
        if (it->popHandler)
            if (llvm::Function *popF = module->getFunction("__tocin_try_pop"))
                builder.CreateCall(popF, {});
        if (it->block)
        {
            createEnvironment();
            it->block->accept(*this);
            restoreEnvironment();
        }
    }
    lastValue = savedLast;
    finallyStack = pending;
}

bool IRGenerator::isStringExpr(const ast::ExprPtr &expr)
{
    if (!expr)
        return false;
    if (auto lit = std::dynamic_pointer_cast<ast::LiteralExpr>(expr))
        return lit->literalType == ast::LiteralExpr::LiteralType::STRING;
    if (auto grp = std::dynamic_pointer_cast<ast::GroupingExpr>(expr))
        return isStringExpr(grp->expression);
    if (std::dynamic_pointer_cast<ast::StringInterpolationExpr>(expr))
        return true;
    if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
        return varIsString.count(var->name) > 0;
    if (auto bin = std::dynamic_pointer_cast<ast::BinaryExpr>(expr))
        return bin->op.type == lexer::TokenType::PLUS &&
               (isStringExpr(bin->left) || isStringExpr(bin->right));
    if (auto call = std::dynamic_pointer_cast<ast::CallExpr>(expr))
    {
        if (auto callee = std::dynamic_pointer_cast<ast::VariableExpr>(call->callee))
        {
            // Builtins whose return value is a freshly-allocated string.
            static const std::set<std::string> strFns = {
                "substring", "intToStr", "charToStr", "readFile", "readLine",
                "toUpper", "toLower"};
            return strFns.count(callee->name) > 0;
        }
    }
    return false;
}

llvm::Type *IRGenerator::getArrayElemType(const ast::ExprPtr &expr)
{
    if (!expr)
        return llvm::Type::getInt64Ty(context);
    if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(expr))
    {
        auto it = varArrayElem.find(var->name);
        if (it != varArrayElem.end())
            return it->second;
    }
    if (auto lst = std::dynamic_pointer_cast<ast::ListExpr>(expr))
    {
        if (!lst->elements.empty())
            return inferExprType(lst->elements[0], {});
    }
    if (auto idx = std::dynamic_pointer_cast<ast::IndexExpr>(expr))
    {
        // Nested arrays: element of an element is the same scalar type for now.
        return getArrayElemType(idx->object);
    }
    return llvm::Type::getInt64Ty(context);
}

void IRGenerator::visitIndexExpr(ast::IndexExpr *expr)
{
    // Evaluate the array base pointer and the index.
    expr->object->accept(*this);
    llvm::Value *base = lastValue;
    if (!base) return;
    expr->index->accept(*this);
    llvm::Value *index = lastValue;
    if (!index) return;

    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Type *i8 = llvm::Type::getInt8Ty(context);
    if (!index->getType()->isIntegerTy(64))
        index = builder.CreateIntCast(index, i64, true, "idx");

    llvm::Type *elemTy = getArrayElemType(expr->object);
    llvm::Value *elemSize = llvm::ConstantExpr::getSizeOf(elemTy);
    // Offset = 8 (length header) + index * sizeof(elem).
    llvm::Value *off = builder.CreateAdd(
        llvm::ConstantInt::get(i64, 8),
        builder.CreateMul(index, elemSize), "idx.off");
    llvm::Value *slot = builder.CreateGEP(i8, base, off, "idx.slot");
    lastValue = builder.CreateLoad(elemTy, slot, "idx.val");
}

std::string IRGenerator::llvmTypeName(llvm::Type *t)
{
    if (t->isIntegerTy(1)) return "i1";
    if (t->isIntegerTy(64)) return "i64";
    if (t->isIntegerTy()) return "i" + std::to_string(t->getIntegerBitWidth());
    if (t->isDoubleTy()) return "f64";
    if (t->isFloatTy()) return "f32";
    if (t->isPointerTy()) return "ptr";
    return "t";
}

llvm::Function *IRGenerator::emitGenericInstance(ast::FunctionStmt *stmt,
                                                const std::map<std::string, llvm::Type *> &bindings)
{
    // Mangle the instance name from the concrete type arguments.
    std::string mangled = stmt->name;
    for (const auto &tp : stmt->typeParameters)
    {
        auto it = bindings.find(tp.getName());
        mangled += "$" + (it != bindings.end() ? llvmTypeName(it->second) : "t");
    }
    if (llvm::Function *existing = module->getFunction(mangled))
        return existing;

    // Save codegen state.
    auto savedBindings = typeBindings;
    auto savedNamed = namedValues;
    auto savedVarClasses = varClasses;
    auto savedArrayElem = varArrayElem;
    llvm::Function *savedFunction = currentFunction;
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    typeBindings = bindings;

    // Build the concrete signature (getLLVMType resolves type params).
    std::vector<llvm::Type *> paramTypes;
    for (const auto &p : stmt->parameters)
    {
        llvm::Type *t = getLLVMType(p.type);
        paramTypes.push_back(t ? t : llvm::Type::getInt64Ty(context));
    }
    llvm::Type *retType = stmt->returnType ? getLLVMType(stmt->returnType)
                                           : inferFunctionReturnType(stmt);
    if (!retType)
        retType = llvm::Type::getVoidTy(context);
    auto *ft = llvm::FunctionType::get(retType, paramTypes, false);
    auto *function = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, mangled, module.get());

    // Generate the body.
    auto *entry = llvm::BasicBlock::Create(context, "entry", function);
    builder.SetInsertPoint(entry);
    currentFunction = function;
    namedValues.clear();
    varFuncSig.clear();
    varIsString.clear();
    varClasses.clear();
    varArrayElem.clear();
    unsigned i = 0;
    for (auto &arg : function->args())
    {
        const std::string &pname = stmt->parameters[i].name;
        arg.setName(pname);
        llvm::AllocaInst *alloca = createEntryBlockAlloca(function, pname, arg.getType());
        builder.CreateStore(&arg, alloca);
        namedValues[pname] = alloca;
        recordArrayParam(pname, stmt->parameters[i].type);
        ++i;
    }
    if (stmt->body)
        stmt->body->accept(*this);
    if (!builder.GetInsertBlock()->getTerminator())
    {
        if (retType->isVoidTy())
            builder.CreateRetVoid();
        else if (retType->isIntegerTy())
            builder.CreateRet(llvm::ConstantInt::get(retType, 0));
        else if (retType->isFloatingPointTy())
            builder.CreateRet(llvm::ConstantFP::get(retType, 0.0));
        else if (retType->isPointerTy())
            builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(retType)));
        else
            builder.CreateRet(llvm::UndefValue::get(retType));
    }
    llvm::verifyFunction(*function);

    // Restore state.
    typeBindings = savedBindings;
    namedValues = savedNamed;
    varClasses = savedVarClasses;
    varArrayElem = savedArrayElem;
    currentFunction = savedFunction;
    if (savedBlock)
        builder.SetInsertPoint(savedBlock);
    return function;
}

std::string IRGenerator::instantiateGenericClass(ast::ClassStmt *stmt,
                                                 const std::map<std::string, llvm::Type *> &bindings)
{
    // Mangle the concrete class name from its type arguments (e.g. Box$i64).
    std::string mangled = stmt->name;
    for (const auto &tp : stmt->typeParameters)
    {
        auto it = bindings.find(tp.getName());
        mangled += "$" + (it != bindings.end() ? llvmTypeName(it->second) : "t");
    }
    if (classTypes.count(mangled))
        return mangled; // already instantiated

    // Bind type parameters while we build the struct and method signatures.
    auto savedBindings = typeBindings;
    typeBindings = bindings;

    // Build the concrete struct from the fields (type params now resolved).
    ClassInfo info;
    std::vector<llvm::Type *> fieldTypes;
    for (const auto &fieldStmt : stmt->fields)
    {
        auto var = std::dynamic_pointer_cast<ast::VariableStmt>(fieldStmt);
        if (!var)
            continue;
        llvm::Type *ft = var->type ? getLLVMType(var->type) : llvm::Type::getInt64Ty(context);
        info.memberNames.push_back(var->name);
        info.memberTypes[var->name] = ft;
        fieldTypes.push_back(ft);
    }
    llvm::StructType *structType = llvm::StructType::getTypeByName(context, mangled);
    if (!structType)
        structType = llvm::StructType::create(context, fieldTypes, mangled);
    else
        structType->setBody(fieldTypes);
    info.classType = structType;
    info.baseClass = nullptr;
    classTypes[mangled] = info; // register before methods so they can reference it

    // Declare all method prototypes first (so methods can call each other),
    // then generate their bodies — all under the mangled class name.
    for (const auto &methodStmt : stmt->methods)
        if (auto method = std::dynamic_pointer_cast<ast::FunctionStmt>(methodStmt))
            declareMethodProto(mangled, structType, method.get());
    for (const auto &methodStmt : stmt->methods)
        if (auto method = std::dynamic_pointer_cast<ast::FunctionStmt>(methodStmt))
            generateMethod(mangled, structType, method.get());

    typeBindings = savedBindings;
    return mangled;
}

void IRGenerator::visitEnumStmt(ast::EnumStmt *stmt)
{
    // Enum members are integer constants, registered both bare (Red) and
    // qualified (Color.Red) for use in expressions.
    for (const auto &m : stmt->members)
    {
        enumConstants[m.first] = m.second;
        enumConstants[stmt->name + "." + m.first] = m.second;
    }
    lastValue = nullptr;
}

void IRGenerator::visitThrowStmt(ast::ThrowStmt *stmt)
{
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Type *voidTy = llvm::Type::getVoidTy(context);

    llvm::Value *val = nullptr;
    if (stmt->value)
    {
        stmt->value->accept(*this);
        val = lastValue;
    }
    if (!val)
        val = llvm::ConstantInt::get(i64, 0);

    // Normalize the thrown value into a 64-bit slot (matching the channel ABI).
    if (val->getType()->isPointerTy())
        val = builder.CreatePtrToInt(val, i64, "throwv");
    else if (val->getType()->isDoubleTy())
        val = builder.CreateBitCast(val, i64, "throwv");
    else if (val->getType()->isIntegerTy() && val->getType() != i64)
        val = builder.CreateSExt(val, i64, "throwv");

    llvm::Function *throwF = module->getFunction("__tocin_throw");
    if (!throwF)
        throwF = llvm::Function::Create(
            llvm::FunctionType::get(voidTy, {i64}, false),
            llvm::Function::ExternalLinkage, "__tocin_throw", *module);
    builder.CreateCall(throwF, {val});
    // __tocin_throw never returns (it longjmps); terminate the block.
    builder.CreateUnreachable();
    lastValue = nullptr;
}

void IRGenerator::visitBreakStmt(ast::BreakStmt *stmt)
{
    (void)stmt;
    if (loopStack.empty())
    {
        errorHandler.reportError(error::ErrorCode::S004_INVALID_STATEMENT,
                                 "'break' used outside of a loop",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(loopStack.back().second); // break target
    lastValue = nullptr;
}

void IRGenerator::visitContinueStmt(ast::ContinueStmt *stmt)
{
    (void)stmt;
    if (loopStack.empty())
    {
        errorHandler.reportError(error::ErrorCode::S004_INVALID_STATEMENT,
                                 "'continue' used outside of a loop",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        return;
    }
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(loopStack.back().first); // continue target
    lastValue = nullptr;
}

void IRGenerator::visitTryStmt(ast::TryStmt *stmt)
{
    llvm::Function *function = builder.GetInsertBlock()->getParent();
    llvm::Type *i8 = llvm::Type::getInt8Ty(context);
    llvm::Type *i32 = llvm::Type::getInt32Ty(context);
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Type *ptr = llvm::PointerType::get(context, 0);
    llvm::Type *voidTy = llvm::Type::getVoidTy(context);

    // Runtime helpers.
    auto getFn = [&](const char *name, llvm::FunctionType *ty) {
        llvm::Function *f = module->getFunction(name);
        if (!f)
            f = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, name, *module);
        return f;
    };
    llvm::Function *regF = getFn("__tocin_try_register",
                                 llvm::FunctionType::get(voidTy, {ptr}, false));
    llvm::Function *popF = getFn("__tocin_try_pop",
                                 llvm::FunctionType::get(voidTy, {}, false));
    llvm::Function *excValF = getFn("__tocin_exc_value",
                                    llvm::FunctionType::get(i64, {}, false));
    llvm::Function *throwF = getFn("__tocin_throw",
                                   llvm::FunctionType::get(voidTy, {i64}, false));
    // libc setjmp; the call site must be marked returns_twice.
    llvm::Function *setjmpF = module->getFunction("setjmp");
    if (!setjmpF)
    {
        setjmpF = llvm::Function::Create(
            llvm::FunctionType::get(i32, {ptr}, false),
            llvm::Function::ExternalLinkage, "setjmp", *module);
        setjmpF->addFnAttr(llvm::Attribute::ReturnsTwice);
    }

    // jmp_buf storage (256 bytes covers glibc's jmp_buf), in the entry block.
    llvm::ArrayType *bufTy = llvm::ArrayType::get(i8, 256);
    llvm::AllocaInst *buf = createEntryBlockAlloca(function, "try.jmpbuf", bufTy);

    // Emits the finally block (if present) inline. Returns true if the current
    // block is still open (not terminated) afterward.
    auto emitFinally = [&]() -> bool {
        if (stmt->finallyBlock)
        {
            createEnvironment();
            stmt->finallyBlock->accept(*this);
            restoreEnvironment();
        }
        return builder.GetInsertBlock()->getTerminator() == nullptr;
    };

    // Register the handler and arm setjmp.
    builder.CreateCall(regF, {buf});
    llvm::CallInst *sj = builder.CreateCall(setjmpF, {buf}, "sj");
    sj->addFnAttr(llvm::Attribute::ReturnsTwice);
    llvm::Value *isExc = builder.CreateICmpNE(sj, llvm::ConstantInt::get(i32, 0), "is.exc");

    llvm::BasicBlock *tryBB = llvm::BasicBlock::Create(context, "try.body", function);
    llvm::BasicBlock *excBB = llvm::BasicBlock::Create(context, "try.land", function);
    llvm::BasicBlock *contBB = llvm::BasicBlock::Create(context, "try.cont", function);

    builder.CreateCondBr(isExc, excBB, tryBB);

    // --- try body (setjmp returned 0) ---
    builder.SetInsertPoint(tryBB);
    createEnvironment();
    // While generating the try body, an early `return` must pop this handler
    // and run finally first.
    finallyStack.push_back({stmt->finallyBlock, true});
    if (stmt->tryBlock)
        stmt->tryBlock->accept(*this);
    finallyStack.pop_back();
    restoreEnvironment();
    if (!builder.GetInsertBlock()->getTerminator())
    {
        builder.CreateCall(popF, {});       // handler consumed: normal completion
        if (emitFinally())
            builder.CreateBr(contBB);
    }

    // --- landing pad (setjmp returned non-zero: an exception unwound here) ---
    // __tocin_throw has already popped this handler.
    builder.SetInsertPoint(excBB);
    if (stmt->catchBlock)
    {
        auto savedNamed = namedValues;
        createEnvironment();
        if (!stmt->catchVar.empty())
        {
            llvm::Value *exc = builder.CreateCall(excValF, {}, "exc.val");
            llvm::AllocaInst *slot = builder.CreateAlloca(i64, nullptr, stmt->catchVar);
            builder.CreateStore(exc, slot);
            namedValues[stmt->catchVar] = slot;
        }
        // The handler was already popped when the exception unwound here, so an
        // early `return` from the catch only needs to run finally.
        finallyStack.push_back({stmt->finallyBlock, false});
        stmt->catchBlock->accept(*this);
        finallyStack.pop_back();
        restoreEnvironment();
        namedValues = savedNamed;
        if (!builder.GetInsertBlock()->getTerminator())
        {
            if (emitFinally())
                builder.CreateBr(contBB);
        }
    }
    else
    {
        // No catch: run finally, then re-propagate to the enclosing handler.
        if (emitFinally())
        {
            llvm::Value *exc = builder.CreateCall(excValF, {}, "exc.val");
            builder.CreateCall(throwF, {exc});
            builder.CreateUnreachable();
        }
    }

    // --- continuation ---
    builder.SetInsertPoint(contBB);
    lastValue = nullptr;
}

void IRGenerator::generateMethod(const std::string &className, llvm::StructType *classType, ast::FunctionStmt *method)
{
    llvm::Type *opaquePtr = llvm::PointerType::get(context, 0);

    // Return type (inferred from the body when not annotated).
    llvm::Type *returnType = method->returnType
                                 ? getLLVMType(method->returnType)
                                 : inferFunctionReturnType(method);
    if (!returnType)
        returnType = llvm::Type::getVoidTy(context);

    // Parameter types. A leading `self` parameter becomes the receiver pointer.
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : method->parameters)
    {
        if (param.name == "self")
            paramTypes.push_back(opaquePtr);
        else
        {
            llvm::Type *paramType = getLLVMType(param.type);
            paramTypes.push_back(paramType ? paramType : opaquePtr);
        }
    }

    std::string methodName = className + "_" + method->name;
    llvm::FunctionType *functionType = llvm::FunctionType::get(returnType, paramTypes, false);
    // Reuse the forward-declared prototype if present.
    llvm::Function *function = module->getFunction(methodName);
    if (!function)
        function = llvm::Function::Create(
            functionType, llvm::Function::ExternalLinkage, methodName, module.get());

    unsigned ai = 0;
    for (auto &arg : function->args())
        arg.setName(method->parameters[ai++].name);

    llvm::BasicBlock *block = llvm::BasicBlock::Create(context, "entry", function);
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    llvm::Function *savedFunction = currentFunction;
    std::string savedClassName = currentClassName;
    std::map<std::string, llvm::AllocaInst *> savedNamedValues(namedValues);
    std::map<std::string, std::string> savedVarClasses(varClasses);

    builder.SetInsertPoint(block);
    currentFunction = function;
    currentClassName = className;
    namedValues.clear();
    varFuncSig.clear();
    varIsString.clear();

    // Bind parameters (including `self`) into the method scope.
    ai = 0;
    for (auto &arg : function->args())
    {
        const std::string &pname = method->parameters[ai].name;
        llvm::AllocaInst *alloca = createEntryBlockAlloca(function, pname, arg.getType());
        builder.CreateStore(&arg, alloca);
        namedValues[pname] = alloca;
        recordArrayParam(pname, method->parameters[ai].type);
        if (pname == "self")
            varClasses["self"] = className;
        ++ai;
    }

    // Store the method in the method table.
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
    varClasses = savedVarClasses;
    currentFunction = savedFunction;
    currentClassName = savedClassName;

    if (savedBlock)
        builder.SetInsertPoint(savedBlock);
}

void IRGenerator::visitGetExpr(ast::GetExpr *expr)
{
    // Qualified enum access: EnumName.Member -> integer constant.
    if (auto v = std::dynamic_pointer_cast<ast::VariableExpr>(expr->object))
    {
        auto e = enumConstants.find(v->name + "." + expr->name);
        if (e != enumConstants.end())
        {
            lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), e->second);
            return;
        }
    }

    // Evaluate the object expression
    expr->object->accept(*this);
    if (!lastValue)
        return;

    llvm::Value *object = lastValue;

    // Resolve the class name from the AST (self, typed locals, constructors).
    std::string className = getExprClassName(expr->object);
    if (className.empty())
    {
        if (expr->getType())
            className = expr->getType()->toString();
        else
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
            std::vector<llvm::Value *> indices = {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), fieldIndex)};
            llvm::Type *fieldType = structType->getStructElementType(fieldIndex);

            // Safe navigation `a?.b`: when the base is null, yield a null/zero
            // value of the field type instead of dereferencing.
            if (expr->isSafe && object->getType()->isPointerTy())
            {
                llvm::Function *fn = builder.GetInsertBlock()->getParent();
                llvm::Value *isNull = builder.CreateICmpEQ(
                    object,
                    llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(object->getType())),
                    "safe.isnull");
                llvm::BasicBlock *nullBB = builder.GetInsertBlock();
                llvm::BasicBlock *loadBB = llvm::BasicBlock::Create(context, "safe.load", fn);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "safe.merge", fn);
                builder.CreateCondBr(isNull, mergeBB, loadBB);

                builder.SetInsertPoint(loadBB);
                llvm::Value *fieldPtr = builder.CreateGEP(structType, object, indices, "field." + expr->name);
                llvm::Value *loaded = builder.CreateLoad(fieldType, fieldPtr);
                llvm::BasicBlock *loadEnd = builder.GetInsertBlock();
                builder.CreateBr(mergeBB);

                builder.SetInsertPoint(mergeBB);
                llvm::PHINode *phi = builder.CreatePHI(fieldType, 2, "safe");
                phi->addIncoming(llvm::Constant::getNullValue(fieldType), nullBB);
                phi->addIncoming(loaded, loadEnd);
                lastValue = phi;
                return;
            }

            // Get a pointer to the field (using the known struct type)
            llvm::Value *fieldPtr = builder.CreateGEP(
                structType, object, indices, "field." + expr->name);
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
    // Resolve the class of the object being assigned into.
    std::string className = getExprClassName(expr->object);
    auto it = classTypes.find(className);
    if (it == classTypes.end())
    {
        errorHandler.reportError(error::ErrorCode::C002_CODEGEN_ERROR,
                                 "Cannot determine class of object in field assignment",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }
    const ClassInfo &info = it->second;
    llvm::StructType *structType = info.classType;

    // Locate the field.
    int fieldIndex = -1;
    for (size_t i = 0; i < info.memberNames.size(); ++i)
        if (info.memberNames[i] == expr->name) { fieldIndex = (int)i; break; }
    if (fieldIndex < 0)
    {
        errorHandler.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                 "Unknown field '" + expr->name + "' on class " + className,
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Evaluate the receiver pointer and the value.
    expr->object->accept(*this);
    llvm::Value *object = lastValue;
    if (!object) return;
    expr->value->accept(*this);
    llvm::Value *val = lastValue;
    if (!val) return;

    llvm::Type *fieldType = structType->getStructElementType(fieldIndex);
    if (val->getType() != fieldType)
    {
        if (val->getType()->isIntegerTy() && fieldType->isIntegerTy())
            val = builder.CreateIntCast(val, fieldType, true, "cast");
        else if (val->getType()->isFloatingPointTy() && fieldType->isFloatingPointTy())
            val = builder.CreateFPCast(val, fieldType, "cast");
    }

    std::vector<llvm::Value *> indices = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), (unsigned)fieldIndex)};
    llvm::Value *fieldPtr = builder.CreateGEP(structType, object, indices, "field." + expr->name);
    builder.CreateStore(val, fieldPtr);
    lastValue = val;
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

        // Look up the variable. Variables are tracked in the flat namedValues
        // table (populated by variable declarations and function parameters),
        // so use lookupVariable which consults it before any Scope chain.
        llvm::AllocaInst *alloca = lookupVariable(name);

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

llvm::Function *IRGenerator::declareFunctionProto(ast::FunctionStmt *stmt)
{
    if (!stmt || stmt->isAsync || stmt->isGeneric())
        return nullptr;
    if (llvm::Function *existing = module->getFunction(stmt->name))
        return existing;
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    std::vector<llvm::Type *> paramTypes;
    for (const auto &p : stmt->parameters)
    {
        llvm::Type *t = p.type ? getLLVMType(p.type) : i64;
        if (!t || t->isVoidTy())
            t = i64;
        paramTypes.push_back(t);
    }
    llvm::Type *retType = stmt->returnType ? getLLVMType(stmt->returnType)
                                           : inferFunctionReturnType(stmt);
    if (!retType)
        retType = llvm::Type::getVoidTy(context);
    llvm::FunctionType *ft = llvm::FunctionType::get(retType, paramTypes, false);
    return llvm::Function::Create(ft, llvm::Function::ExternalLinkage, stmt->name, module.get());
}

void IRGenerator::registerClassType(ast::ClassStmt *stmt)
{
    if (!stmt || stmt->isGeneric() || classTypes.count(stmt->name))
        return;
    ClassInfo info;
    std::vector<llvm::Type *> fieldTypes;
    for (const auto &fieldStmt : stmt->fields)
    {
        auto var = std::dynamic_pointer_cast<ast::VariableStmt>(fieldStmt);
        if (!var)
            continue;
        llvm::Type *ft = var->type ? getLLVMType(var->type) : llvm::Type::getInt64Ty(context);
        info.memberNames.push_back(var->name);
        info.memberTypes[var->name] = ft;
        fieldTypes.push_back(ft);
    }
    llvm::StructType *structType = llvm::StructType::getTypeByName(context, stmt->name);
    if (!structType)
        structType = llvm::StructType::create(context, fieldTypes, stmt->name);
    else
        structType->setBody(fieldTypes);
    info.classType = structType;
    info.baseClass = nullptr;
    classTypes[stmt->name] = info;
}

llvm::Function *IRGenerator::declareMethodProto(const std::string &className,
                                                llvm::StructType *classType,
                                                ast::FunctionStmt *method)
{
    std::string methodName = className + "_" + method->name;
    if (llvm::Function *existing = module->getFunction(methodName))
        return existing;
    llvm::Type *opaquePtr = llvm::PointerType::get(context, 0);
    llvm::Type *retType = method->returnType ? getLLVMType(method->returnType)
                                             : inferFunctionReturnType(method);
    if (!retType)
        retType = llvm::Type::getVoidTy(context);
    std::vector<llvm::Type *> paramTypes;
    for (const auto &param : method->parameters)
    {
        if (param.name == "self")
            paramTypes.push_back(opaquePtr);
        else
        {
            llvm::Type *t = getLLVMType(param.type);
            paramTypes.push_back(t ? t : opaquePtr);
        }
    }
    llvm::FunctionType *ft = llvm::FunctionType::get(retType, paramTypes, false);
    llvm::Function *fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                               methodName, module.get());
    classMethods[className + "." + method->name] = fn;
    return fn;
}

void IRGenerator::predeclareTopLevel(ast::StmtPtr ast)
{
    std::vector<ast::StmtPtr> stmts;
    if (auto blk = std::dynamic_pointer_cast<ast::BlockStmt>(ast))
        stmts = blk->statements;
    else if (auto mod = std::dynamic_pointer_cast<ast::ModuleStmt>(ast))
        stmts = mod->body;
    else
        stmts.push_back(ast);

    // Register class types and enum constants first (so they can be referenced
    // regardless of declaration order).
    for (auto &s : stmts)
    {
        if (auto cls = std::dynamic_pointer_cast<ast::ClassStmt>(s))
        {
            if (cls->isGeneric())
                genericClasses[cls->name] = cls.get(); // instantiated lazily per type argument
            else
                registerClassType(cls.get());
        }
        else if (auto en = std::dynamic_pointer_cast<ast::EnumStmt>(s))
            visitEnumStmt(en.get());
    }

    // Then forward-declare free functions and method prototypes.
    for (auto &s : stmts)
    {
        if (auto fn = std::dynamic_pointer_cast<ast::FunctionStmt>(s))
        {
            if (fn->isGeneric())
                genericFunctions[fn->name] = fn.get();  // record template for lazy instantiation
            else
                declareFunctionProto(fn.get());
            // Record a function-typed return so callers can recover the
            // signature of a closure produced by calling this function.
            if (auto rft = std::dynamic_pointer_cast<ast::FunctionType>(fn->returnType))
                funcReturnFnType[fn->name] = rft;
        }
        else if (auto cls = std::dynamic_pointer_cast<ast::ClassStmt>(s))
        {
            auto cit = classTypes.find(cls->name);
            if (cit == classTypes.end())
                continue;
            for (auto &m : cls->methods)
                if (auto method = std::dynamic_pointer_cast<ast::FunctionStmt>(m))
                    declareMethodProto(cls->name, cit->second.classType, method.get());
        }
        else if (auto impl = std::dynamic_pointer_cast<ast::ImplStmt>(s))
        {
            std::string typeName = impl->type ? impl->type->toString() : "";
            auto cit = classTypes.find(typeName);
            if (cit == classTypes.end())
                continue;
            for (auto &m : impl->methods)
                if (m && m->body)
                    declareMethodProto(typeName, cit->second.classType, m.get());
        }
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

    // Create a global scope
    enterScope();

    // Pass 1: forward-declare all top-level functions, classes and methods so
    // declaration order doesn't matter (mutual recursion, use-before-def).
    predeclareTopLevel(ast);

    // Pass 2: visit the AST to generate IR
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

    // Handle indexed assignment (arr[i] = value)
    if (auto idxExpr = std::dynamic_pointer_cast<ast::IndexExpr>(expr->target))
    {
        idxExpr->object->accept(*this);
        llvm::Value *base = lastValue;
        if (!base) return;
        idxExpr->index->accept(*this);
        llvm::Value *index = lastValue;
        if (!index) return;

        llvm::Type *i64 = llvm::Type::getInt64Ty(context);
        llvm::Type *i8 = llvm::Type::getInt8Ty(context);
        if (!index->getType()->isIntegerTy(64))
            index = builder.CreateIntCast(index, i64, true, "idx");
        llvm::Type *elemTy = getArrayElemType(idxExpr->object);
        if (rhs->getType() != elemTy)
        {
            if (rhs->getType()->isIntegerTy() && elemTy->isIntegerTy())
                rhs = builder.CreateIntCast(rhs, elemTy, true, "ecast");
            else if (rhs->getType()->isFloatingPointTy() && elemTy->isFloatingPointTy())
                rhs = builder.CreateFPCast(rhs, elemTy, "ecast");
        }
        llvm::Value *elemSize = llvm::ConstantExpr::getSizeOf(elemTy);
        llvm::Value *off = builder.CreateAdd(
            llvm::ConstantInt::get(i64, 8),
            builder.CreateMul(index, elemSize), "idx.off");
        llvm::Value *slot = builder.CreateGEP(i8, base, off, "idx.slot");
        builder.CreateStore(rhs, slot);
        lastValue = rhs;
        return;
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

    // Note: Indexed assignments are not supported in current AST (no IndexExpr)
    // Emit clear diagnostic when this limitation is encountered
    
    // Check if target looks like it might be an index expression
    if (auto callExpr = dynamic_cast<ast::CallExpr *>(expr->target.get())) {
        // This might be an attempted index operation
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Indexed assignments (e.g., arr[i] = value) are not yet supported. "
                                 "Use array methods like 'set(index, value)' instead.",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }

    // Handle compound assignment operators (e.g., +=, -=, *=, etc.)
    if (auto binaryExpr = dynamic_cast<ast::BinaryExpr *>(expr->target.get()))
    {
        // This would handle cases like (x + y) = z, which should be an error
        errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
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
    // MoveExpr: Emit diagnostic that real move semantics are not yet implemented
    auto moveExpr = static_cast<type_checker::MoveExpr *>(expr);
    if (moveExpr) {
        // Log warning that move semantics are simplified
        // In a full implementation, we would:
        // 1. Mark the source value as moved-from
        // 2. Prevent further use of the moved-from value
        // 3. Optimize away unnecessary copies
        
        moveExpr->getExpr()->accept(*this);
        
        // For now, just forward the value (acts like a copy, not a move)
        // TODO: Implement proper move semantics with ownership tracking
        
        // Optionally emit a warning to the user
        errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                 "Move semantics are currently simplified - value is copied, not moved",
                                 "", 0, 0, error::ErrorSeverity::WARNING);
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
    // Log diagnostic that async functions are not fully implemented
    errorHandler.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                             "Async function transformation is simplified - returned as synchronous function",
                             "", 0, 0, error::ErrorSeverity::WARNING);
    
    // For now, create a simple synchronous function
    // Full implementation would:
    // 1. Create a coroutine structure
    // 2. Generate state machine for suspend points
    // 3. Return a Future/Promise object
    // 4. Integrate with async runtime
    
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

    // Elvis / null-coalescing: `a ?: b` / `a ?? b`. Short-circuit — only
    // evaluate `b` when `a` is null. Handled before the eager both-operands
    // evaluation below.
    if (expr->op.type == lexer::TokenType::ELVIS ||
        expr->op.type == lexer::TokenType::NULL_COALESCE)
    {
        expr->left->accept(*this);
        llvm::Value *lhs = lastValue;
        if (!lhs) { lastValue = nullptr; return; }

        if (!lhs->getType()->isPointerTy()) {
            // A non-pointer left operand is never null; result is the left.
            lastValue = lhs;
            return;
        }

        llvm::Function *fn = builder.GetInsertBlock()->getParent();
        llvm::Value *isNull = builder.CreateICmpEQ(
            lhs, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(lhs->getType())),
            "elvis.isnull");
        llvm::BasicBlock *lhsBB = builder.GetInsertBlock();
        llvm::BasicBlock *rhsBB = llvm::BasicBlock::Create(context, "elvis.rhs", fn);
        llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "elvis.merge", fn);
        builder.CreateCondBr(isNull, rhsBB, mergeBB);

        builder.SetInsertPoint(rhsBB);
        expr->right->accept(*this);
        llvm::Value *rhs = lastValue;
        if (!rhs) { lastValue = nullptr; return; }
        if (rhs->getType() != lhs->getType() && rhs->getType()->isPointerTy())
            rhs = builder.CreateBitCast(rhs, lhs->getType(), "elvis.cast");
        llvm::BasicBlock *rhsEnd = builder.GetInsertBlock();
        builder.CreateBr(mergeBB);

        builder.SetInsertPoint(mergeBB);
        llvm::PHINode *phi = builder.CreatePHI(lhs->getType(), 2, "elvis");
        phi->addIncoming(lhs, lhsBB);
        phi->addIncoming(rhs, rhsEnd);
        lastValue = phi;
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

    // Mixed int/float arithmetic and comparison: promote the integer side to
    // floating point so e.g. `5 + 3.0` and `n < 2.5` work.
    if (left->getType()->isIntegerTy() && !left->getType()->isIntegerTy(1) &&
        (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
        left = builder.CreateSIToFP(left, right->getType(), "promote");
    } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
               right->getType()->isIntegerTy() && !right->getType()->isIntegerTy(1)) {
        right = builder.CreateSIToFP(right, left->getType(), "promote");
    }

    // Handle different operators
    switch (expr->op.type) {
    case lexer::TokenType::PLUS:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateAdd(left, right, "add");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFAdd(left, right, "fadd");
        } else if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
            // String concatenation: malloc(strlen(a)+strlen(b)+1); strcpy; strcat.
            llvm::Function *strlenF = stdLibFunctions["strlen"];
            llvm::Function *strcpyF = stdLibFunctions["strcpy"];
            llvm::Function *strcatF = stdLibFunctions["strcat"];
            llvm::Function *mallocF = stdLibFunctions["malloc"];
            llvm::Type *i64 = llvm::Type::getInt64Ty(context);
            llvm::Value *la = builder.CreateCall(strlenF->getFunctionType(), strlenF, {left}, "lenl");
            llvm::Value *lb = builder.CreateCall(strlenF->getFunctionType(), strlenF, {right}, "lenr");
            llvm::Value *total = builder.CreateAdd(builder.CreateAdd(la, lb),
                                                   llvm::ConstantInt::get(i64, 1), "len");
            llvm::Value *buf = builder.CreateCall(mallocF->getFunctionType(), mallocF, {total}, "concat");
            builder.CreateCall(strcpyF->getFunctionType(), strcpyF, {buf, left});
            builder.CreateCall(strcatF->getFunctionType(), strcatF, {buf, right});
            lastValue = buf;
        } else if (left->getType()->isPointerTy() && right->getType()->isIntegerTy()) {
            // Pointer arithmetic - use element type if available, else i8
            llvm::Type* elemTy = nullptr;
            if (auto* ptrTy = llvm::dyn_cast<llvm::PointerType>(left->getType())) {
                elemTy = ptrTy->getArrayElementType();
            }
            if (elemTy == nullptr) {
                elemTy = llvm::Type::getInt8Ty(context);
            }
            lastValue = builder.CreateGEP(elemTy, left, right, "ptr_add");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot add incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::MINUS:
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

    case lexer::TokenType::STAR:
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

    case lexer::TokenType::SLASH:
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

    case lexer::TokenType::PERCENT:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateSRem(left, right, "mod");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Modulo only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::EQUAL_EQUAL:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpEQ(left, right, "eq");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpOEQ(left, right, "feq");
        } else if (left->getType()->isPointerTy() && right->getType()->isPointerTy() &&
                   isStringExpr(expr->left) && isStringExpr(expr->right)) {
            // String value equality: __tocin_str_eq(a,b) != 0
            llvm::Type *i64 = llvm::Type::getInt64Ty(context);
            llvm::Type *ptr = llvm::PointerType::get(context, 0);
            auto callee = module->getOrInsertFunction(
                "__tocin_str_eq", llvm::FunctionType::get(i64, {ptr, ptr}, false));
            llvm::Value *r = builder.CreateCall(callee, {left, right}, "streq");
            lastValue = builder.CreateICmpNE(r, llvm::ConstantInt::get(i64, 0), "eq");
        } else if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
            lastValue = builder.CreateICmpEQ(left, right, "ptr_eq");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::BANG_EQUAL:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateICmpNE(left, right, "ne");
        } else if ((left->getType()->isFloatTy() || left->getType()->isDoubleTy()) &&
                   (right->getType()->isFloatTy() || right->getType()->isDoubleTy())) {
            lastValue = builder.CreateFCmpONE(left, right, "fne");
        } else if (left->getType()->isPointerTy() && right->getType()->isPointerTy() &&
                   isStringExpr(expr->left) && isStringExpr(expr->right)) {
            // String value inequality: __tocin_str_eq(a,b) == 0
            llvm::Type *i64 = llvm::Type::getInt64Ty(context);
            llvm::Type *ptr = llvm::PointerType::get(context, 0);
            auto callee = module->getOrInsertFunction(
                "__tocin_str_eq", llvm::FunctionType::get(i64, {ptr, ptr}, false));
            llvm::Value *r = builder.CreateCall(callee, {left, right}, "streq");
            lastValue = builder.CreateICmpEQ(r, llvm::ConstantInt::get(i64, 0), "ne");
        } else if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
            lastValue = builder.CreateICmpNE(left, right, "ptr_ne");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Cannot compare incompatible types",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::LESS:
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

    case lexer::TokenType::LESS_EQUAL:
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

    case lexer::TokenType::GREATER:
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

    case lexer::TokenType::GREATER_EQUAL:
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

    case lexer::TokenType::AND:
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

    case lexer::TokenType::OR:
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

    case lexer::TokenType::BITWISE_AND:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateAnd(left, right, "bitand");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Bitwise AND only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::BITWISE_OR:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateOr(left, right, "bitor");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Bitwise OR only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::BITWISE_XOR:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateXor(left, right, "bitxor");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Bitwise XOR only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::LEFT_SHIFT:
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            lastValue = builder.CreateShl(left, right, "shl");
        } else {
            errorHandler.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                     "Left shift only supported for integers",
                                     "", 0, 0, error::ErrorSeverity::ERROR);
            lastValue = nullptr;
        }
        break;

    case lexer::TokenType::RIGHT_SHIFT:
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
    // Enum members resolve to their integer constant.
    auto e = enumConstants.find(expr->name);
    if (e != enumConstants.end())
    {
        lastValue = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), e->second);
        return;
    }
    // Look up variable in current scope
    llvm::AllocaInst *alloca = lookupVariable(expr->name);
    if (alloca)
    {
        lastValue = builder.CreateLoad(alloca->getAllocatedType(), alloca, expr->name);
        return;
    }
    // A bare top-level function name used as a value is a first-class function
    // pointer (all top-level functions are pre-declared by predeclareTopLevel).
    if (llvm::Function *fn = module->getFunction(expr->name))
    {
        lastValue = fn;
        return;
    }
    lastValue = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
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
        if (!statement)
            continue;
        // Stop at the first terminator within the current function: statements
        // after a return/throw are unreachable, and emitting them would place
        // instructions after a block terminator (invalid IR). The parent check
        // avoids tripping at the top level, where the builder is left pointing
        // at the previous function's (terminated) block between declarations.
        if (auto *bb = builder.GetInsertBlock())
            if (bb->getTerminator() && currentFunction && bb->getParent() == currentFunction)
                break;
        statement->accept(*this);
    }
    exitScope();
}

void IRGenerator::visitImportStmt(ast::ImportStmt *stmt)
{
    // Import statements are handled at compile time, not runtime
}

void IRGenerator::visitMatchStmt(ast::MatchStmt *stmt)
{
    if (!stmt->value)
        return;
    stmt->value->accept(*this);
    llvm::Value *matchVal = lastValue;
    if (!matchVal)
        return;

    llvm::Function *function = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *mergeBlock = llvm::BasicBlock::Create(context, "matchcont");
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);

    for (size_t i = 0; i < stmt->cases.size(); ++i)
    {
        std::string ctor = i < stmt->caseCtor.size() ? stmt->caseCtor[i] : "";

        // ---- Option/Result constructor pattern (Some/Ok/Err/None) ----
        if (!ctor.empty())
        {
            llvm::StructType *st = getOptResType();
            llvm::Type *i32 = llvm::Type::getInt32Ty(context);
            llvm::Type *ptrTy = llvm::PointerType::get(context, 0);
            llvm::Value *mvPtr = matchVal->getType()->isPointerTy()
                                     ? matchVal
                                     : builder.CreateIntToPtr(matchVal, ptrTy, "mv.ptr");
            llvm::Value *nullp = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(mvPtr->getType()));

            llvm::BasicBlock *caseBlock = llvm::BasicBlock::Create(context, "case", function);
            llvm::BasicBlock *nextBlock = llvm::BasicBlock::Create(context, "casenext", function);

            if (ctor == "None")
            {
                // None is the null pointer.
                builder.CreateCondBr(builder.CreateICmpEQ(mvPtr, nullp, "is.none"),
                                     caseBlock, nextBlock);
            }
            else
            {
                // Some/Ok/Err are non-null; distinguish by tag (Some/Ok=1, Err=0).
                int64_t wantTag = (ctor == "Some" || ctor == "Ok") ? 1 : 0;
                llvm::BasicBlock *tagBlock = llvm::BasicBlock::Create(context, "casetag", function);
                builder.CreateCondBr(builder.CreateICmpNE(mvPtr, nullp, "nonnull"),
                                     tagBlock, nextBlock);
                builder.SetInsertPoint(tagBlock);
                llvm::Value *tagP = builder.CreateGEP(st, mvPtr,
                    {llvm::ConstantInt::get(i32, 0), llvm::ConstantInt::get(i32, 0)}, "tagp");
                llvm::Value *tag = builder.CreateLoad(i64, tagP, "tag");
                builder.CreateCondBr(
                    builder.CreateICmpEQ(tag, llvm::ConstantInt::get(i64, wantTag), "tagcmp"),
                    caseBlock, nextBlock);
            }

            builder.SetInsertPoint(caseBlock);
            auto savedNamed = namedValues;
            createEnvironment();
            std::string bind = i < stmt->caseBind.size() ? stmt->caseBind[i] : "";
            if (!bind.empty() && ctor != "None")
            {
                llvm::Value *payP = builder.CreateGEP(st, mvPtr,
                    {llvm::ConstantInt::get(i32, 0), llvm::ConstantInt::get(i32, 1)}, "payp");
                llvm::Value *pay = builder.CreateLoad(i64, payP, bind);
                llvm::AllocaInst *slot = builder.CreateAlloca(i64, nullptr, bind);
                builder.CreateStore(pay, slot);
                namedValues[bind] = slot;
            }
            if (stmt->cases[i].second)
                stmt->cases[i].second->accept(*this);
            restoreEnvironment();
            namedValues = savedNamed;
            if (!builder.GetInsertBlock()->getTerminator())
                builder.CreateBr(mergeBlock);

            builder.SetInsertPoint(nextBlock);
            continue;
        }

        // ---- Value-equality pattern (case 5:, case "x":) ----
        stmt->cases[i].first->accept(*this);
        llvm::Value *pat = lastValue;
        if (!pat)
            continue;

        llvm::Value *eq;
        if (matchVal->getType()->isIntegerTy() && pat->getType()->isIntegerTy())
        {
            if (pat->getType() != matchVal->getType())
                pat = builder.CreateIntCast(pat, matchVal->getType(), true, "patcast");
            eq = builder.CreateICmpEQ(matchVal, pat, "matcheq");
        }
        else if (matchVal->getType()->isFloatingPointTy() && pat->getType()->isFloatingPointTy())
        {
            eq = builder.CreateFCmpOEQ(matchVal, pat, "matcheq");
        }
        else
        {
            // Fallback: compare as raw addresses/values.
            llvm::Value *lv = matchVal->getType()->isPointerTy()
                                  ? builder.CreatePtrToInt(matchVal, i64)
                                  : matchVal;
            llvm::Value *rv = pat->getType()->isPointerTy()
                                  ? builder.CreatePtrToInt(pat, i64)
                                  : pat;
            eq = builder.CreateICmpEQ(lv, rv, "matcheq");
        }

        llvm::BasicBlock *caseBlock = llvm::BasicBlock::Create(context, "case", function);
        llvm::BasicBlock *nextBlock = llvm::BasicBlock::Create(context, "casenext", function);
        builder.CreateCondBr(eq, caseBlock, nextBlock);

        builder.SetInsertPoint(caseBlock);
        createEnvironment();
        if (stmt->cases[i].second)
            stmt->cases[i].second->accept(*this);
        restoreEnvironment();
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(mergeBlock);

        builder.SetInsertPoint(nextBlock);
    }

    // Default case runs on the fall-through of all case comparisons.
    if (stmt->defaultCase)
    {
        createEnvironment();
        stmt->defaultCase->accept(*this);
        restoreEnvironment();
    }
    if (!builder.GetInsertBlock()->getTerminator())
        builder.CreateBr(mergeBlock);

    mergeBlock->insertInto(function);
    builder.SetInsertPoint(mergeBlock);
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
    // go f(args): run f(args) on a new OS thread. Arguments are packed into a
    // heap struct; a thunk unpacks them and calls f, then frees the pack.
    llvm::Type *ptr = llvm::PointerType::get(context, 0);
    auto call = std::dynamic_pointer_cast<ast::CallExpr>(stmt->expression);
    if (!call) {
        errorHandler.reportError(error::ErrorCode::C013_INVALID_SPAWN_OPERATION,
                                 "'go' requires a function call",
                                 std::string(stmt->token.filename), stmt->token.line,
                                 stmt->token.column, error::ErrorSeverity::ERROR);
        return;
    }
    auto calleeVar = std::dynamic_pointer_cast<ast::VariableExpr>(call->callee);
    llvm::Function *target = calleeVar ? module->getFunction(calleeVar->name) : nullptr;
    if (!target) {
        errorHandler.reportError(error::ErrorCode::C013_INVALID_SPAWN_OPERATION,
                                 "'go' target must be a known function",
                                 std::string(stmt->token.filename), stmt->token.line,
                                 stmt->token.column, error::ErrorSeverity::ERROR);
        return;
    }

    // Evaluate arguments in the current function.
    std::vector<llvm::Value *> args;
    std::vector<llvm::Type *> argTypes;
    for (auto &a : call->arguments) {
        a->accept(*this);
        if (!lastValue) return;
        args.push_back(lastValue);
        argTypes.push_back(lastValue->getType());
    }

    llvm::Function *mallocF = stdLibFunctions["malloc"];
    llvm::Function *freeF = stdLibFunctions["free"];
    llvm::StructType *packTy = llvm::StructType::get(context, argTypes);
    llvm::Value *pack = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr));
    if (!argTypes.empty()) {
        llvm::Value *sz = llvm::ConstantExpr::getSizeOf(packTy);
        pack = builder.CreateCall(mallocF->getFunctionType(), mallocF, {sz}, "gopack");
        for (size_t i = 0; i < args.size(); ++i) {
            llvm::Value *fp = builder.CreateStructGEP(packTy, pack, (unsigned)i, "go.arg");
            builder.CreateStore(args[i], fp);
        }
    }

    // Build the thunk: void thunk(ptr p) { args = unpack(p); f(args); free(p); }
    std::string thunkName = "go_thunk_" + std::to_string(getNextId());
    llvm::Function *thunk = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(context), {ptr}, false),
        llvm::Function::InternalLinkage, thunkName, module.get());
    llvm::BasicBlock *savedBlock = builder.GetInsertBlock();
    llvm::Function *savedFn = currentFunction;
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", thunk);
    builder.SetInsertPoint(entry);
    currentFunction = thunk;
    llvm::Value *p = thunk->getArg(0);
    std::vector<llvm::Value *> callArgs;
    for (size_t i = 0; i < argTypes.size(); ++i) {
        llvm::Value *fp = builder.CreateStructGEP(packTy, p, (unsigned)i, "th.arg");
        callArgs.push_back(builder.CreateLoad(argTypes[i], fp, "th.val"));
    }
    builder.CreateCall(target->getFunctionType(), target, callArgs);
    if (!argTypes.empty() && freeF)
        builder.CreateCall(freeF->getFunctionType(), freeF, {p});
    builder.CreateRetVoid();
    builder.SetInsertPoint(savedBlock);
    currentFunction = savedFn;

    // __tocin_go(thunk, pack)
    llvm::Function *goF = module->getFunction("__tocin_go");
    if (!goF)
        goF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context), {ptr, ptr}, false),
            llvm::Function::ExternalLinkage, "__tocin_go", *module);
    builder.CreateCall(goF, {thunk, pack});
    lastValue = nullptr;
}

void codegen::IRGenerator::visitChannelSendExpr(ast::ChannelSendExpr* expr) {
    llvm::Type *ptr = llvm::PointerType::get(context, 0);
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    expr->channel->accept(*this);
    llvm::Value *ch = lastValue;
    expr->value->accept(*this);
    llvm::Value *v = lastValue;
    if (!ch || !v) { lastValue = nullptr; return; }
    // Normalize the value into a 64-bit slot.
    if (v->getType()->isPointerTy())
        v = builder.CreatePtrToInt(v, i64, "chv");
    else if (v->getType()->isDoubleTy())
        v = builder.CreateBitCast(v, i64, "chv");
    else if (v->getType()->isFloatTy())
        v = builder.CreateBitCast(builder.CreateFPExt(v, llvm::Type::getDoubleTy(context)), i64, "chv");
    else if (v->getType()->isIntegerTy() && !v->getType()->isIntegerTy(64))
        v = builder.CreateIntCast(v, i64, true, "chv");
    llvm::Function *sendF = module->getFunction("__tocin_chan_send");
    if (!sendF)
        sendF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context), {ptr, i64}, false),
            llvm::Function::ExternalLinkage, "__tocin_chan_send", *module);
    builder.CreateCall(sendF, {ch, v});
    lastValue = nullptr;
}

void codegen::IRGenerator::visitChannelReceiveExpr(ast::ChannelReceiveExpr* expr) {
    llvm::Type *ptr = llvm::PointerType::get(context, 0);
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    expr->channel->accept(*this);
    llvm::Value *ch = lastValue;
    if (!ch) { lastValue = nullptr; return; }
    llvm::Function *recvF = module->getFunction("__tocin_chan_recv");
    if (!recvF)
        recvF = llvm::Function::Create(
            llvm::FunctionType::get(i64, {ptr}, false),
            llvm::Function::ExternalLinkage, "__tocin_chan_recv", *module);
    lastValue = builder.CreateCall(recvF, {ch}, "recv");
}

void codegen::IRGenerator::visitSelectStmt(ast::SelectStmt* stmt) {
    // `select` waits on multiple channel receives. Lowered to a poll loop over
    // the cases using the non-blocking __tocin_chan_try_recv: try each channel;
    // run the first ready case's body; with a `default`, run it when none are
    // ready; otherwise park briefly and retry (blocking).
    llvm::Function *function = builder.GetInsertBlock()->getParent();
    llvm::Type *i8 = llvm::Type::getInt8Ty(context);
    llvm::Type *i64 = llvm::Type::getInt64Ty(context);
    llvm::Type *ptr = llvm::PointerType::get(context, 0);

    llvm::Function *tryF = module->getFunction("__tocin_chan_try_recv");
    if (!tryF)
        tryF = llvm::Function::Create(
            llvm::FunctionType::get(i8, {ptr, ptr}, false),
            llvm::Function::ExternalLinkage, "__tocin_chan_try_recv", *module);
    llvm::Function *parkF = module->getFunction("__tocin_chan_park");
    if (!parkF)
        parkF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false),
            llvm::Function::ExternalLinkage, "__tocin_chan_park", *module);

    // Split receive cases from an optional default.
    std::vector<const ast::SelectStmt::Case *> recvCases;
    const ast::SelectStmt::Case *defCase = nullptr;
    for (const auto &c : stmt->cases) {
        if (c.isDefault) defCase = &c;
        else recvCases.push_back(&c);
    }

    // Evaluate channel handles once; one received-value slot per case.
    std::vector<llvm::Value *> handles, slots;
    for (auto *c : recvCases) {
        c->channel->accept(*this);
        llvm::Value *h = lastValue;
        if (!h) { lastValue = nullptr; return; }
        if (!h->getType()->isPointerTy())
            h = builder.CreateIntToPtr(h, ptr, "ch.ptr");
        handles.push_back(h);
        slots.push_back(createEntryBlockAlloca(function, "sel.slot", i64));
    }

    llvm::BasicBlock *pollBlock = llvm::BasicBlock::Create(context, "select.poll", function);
    llvm::BasicBlock *noMatch = llvm::BasicBlock::Create(context, "select.nomatch", function);
    llvm::BasicBlock *contBlock = llvm::BasicBlock::Create(context, "select.cont", function);
    std::vector<llvm::BasicBlock *> tryBlocks, bodyBlocks;
    for (size_t i = 0; i < recvCases.size(); ++i) {
        tryBlocks.push_back(llvm::BasicBlock::Create(context, "select.try", function));
        bodyBlocks.push_back(llvm::BasicBlock::Create(context, "select.body", function));
    }

    builder.CreateBr(pollBlock);
    builder.SetInsertPoint(pollBlock);
    builder.CreateBr(recvCases.empty() ? noMatch : tryBlocks[0]);

    // Per-case readiness checks, chained.
    for (size_t i = 0; i < recvCases.size(); ++i) {
        builder.SetInsertPoint(tryBlocks[i]);
        llvm::Value *got = builder.CreateCall(tryF, {handles[i], slots[i]}, "got");
        llvm::Value *ready = builder.CreateICmpNE(got, llvm::ConstantInt::get(i8, 0), "ready");
        llvm::BasicBlock *nextTry = (i + 1 < recvCases.size()) ? tryBlocks[i + 1] : noMatch;
        builder.CreateCondBr(ready, bodyBlocks[i], nextTry);
    }

    // No case ready: run default, or park and retry (blocking).
    builder.SetInsertPoint(noMatch);
    if (defCase) {
        createEnvironment();
        if (defCase->body) defCase->body->accept(*this);
        restoreEnvironment();
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(contBlock);
    } else {
        builder.CreateCall(parkF, {});
        builder.CreateBr(pollBlock);
    }

    // Case bodies: bind the received value (if named), then run the body.
    for (size_t i = 0; i < recvCases.size(); ++i) {
        builder.SetInsertPoint(bodyBlocks[i]);
        auto savedNamed = namedValues;
        createEnvironment();
        if (!recvCases[i]->bindName.empty()) {
            llvm::AllocaInst *bindSlot = createEntryBlockAlloca(function, recvCases[i]->bindName, i64);
            builder.CreateStore(builder.CreateLoad(i64, slots[i], "recv"), bindSlot);
            namedValues[recvCases[i]->bindName] = bindSlot;
        }
        if (recvCases[i]->body) recvCases[i]->body->accept(*this);
        restoreEnvironment();
        namedValues = savedNamed;
        if (!builder.GetInsertBlock()->getTerminator())
            builder.CreateBr(contBlock);
    }

    builder.SetInsertPoint(contBlock);
    lastValue = nullptr;
}

int IRGenerator::getNextId() {
    static int nextId = 0;
    return ++nextId;
}

void codegen::IRGenerator::visitTraitStmt(ast::TraitStmt* stmt) {
    // Traits are interfaces: signature-only methods generate no code. Default
    // methods (with bodies) are materialized per implementing type at the impl
    // site, so nothing is emitted here.
    (void)stmt;
    lastValue = nullptr;
}

void codegen::IRGenerator::visitImplStmt(ast::ImplStmt* stmt) {
    // An impl block adds methods to a concrete type. Generate each method as a
    // method of that type (TypeName_methodName) so receiver.method(...) resolves.
    std::string typeName = stmt->type ? stmt->type->toString() : "";
    auto it = classTypes.find(typeName);
    if (it == classTypes.end()) {
        errorHandler.reportError(error::ErrorCode::T031_UNDEFINED_TYPE,
                                 "impl target '" + typeName + "' is not a known type",
                                 "", 0, 0, error::ErrorSeverity::ERROR);
        lastValue = nullptr;
        return;
    }
    llvm::StructType* st = it->second.classType;
    for (const auto& method : stmt->methods) {
        if (method && method->body)
            generateMethod(typeName, st, method.get());
    }
    lastValue = nullptr;
}

llvm::Value *IRGenerator::getVariable(const std::string &name) {
    // Look up the variable in the current scope
    llvm::AllocaInst *alloca = lookupVariable(name);
    if (alloca) {
        return alloca;
    }
    
    // If not found in current scope, check if it's a global variable
    llvm::GlobalVariable *global = module->getGlobalVariable(name);
    if (global) {
        return global;
    }
    
    // If not found, return nullptr
    return nullptr;
} // namespace codegen
