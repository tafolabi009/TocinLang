#include "type_checker.h"
#include <stdexcept>
#include <vector>
#include "result_option.h"

namespace type_checker
{
    // ---------------------------------------------------------------------------
    // Environment
    // ---------------------------------------------------------------------------
    void Environment::define(const std::string &name, ast::TypePtr type, bool isConstant)
    {
        variables_[name] = std::make_pair(type, isConstant);
    }

    ast::TypePtr Environment::lookup(const std::string &name) const
    {
        auto it = variables_.find(name);
        if (it != variables_.end())
        {
            return it->second.first;
        }
        if (parent_)
        {
            return parent_->lookup(name);
        }
        return nullptr;
    }

    bool Environment::assign(const std::string &name, ast::TypePtr type)
    {
        auto it = variables_.find(name);
        if (it != variables_.end())
        {
            // Keep the original declared type; assignment doesn't change a
            // variable's static type here. We simply confirm it exists.
            (void)type;
            return true;
        }
        if (parent_)
        {
            return parent_->assign(name, type);
        }
        return false;
    }

    // ---------------------------------------------------------------------------
    // Small type helpers
    // ---------------------------------------------------------------------------
    ast::TypePtr TypeChecker::makeBasic(ast::TypeKind kind) const
    {
        return std::make_shared<ast::BasicType>(kind);
    }

    bool TypeChecker::isUnknown(ast::TypePtr type) const
    {
        if (!type)
            return true;
        if (auto basic = std::dynamic_pointer_cast<ast::BasicType>(type))
            return basic->getKind() == ast::TypeKind::UNKNOWN;
        return false;
    }

    ast::TypePtr TypeChecker::canonicalize(ast::TypePtr type) const
    {
        if (!type)
            return type;

        // Already a BasicType: nothing to do.
        if (std::dynamic_pointer_cast<ast::BasicType>(type))
            return type;

        // SimpleType / ClassType and other named types: map primitive names to
        // their canonical BasicType so they unify with inferred types and with
        // the codegen type model.
        const std::string name = type->toString();
        if (name == "int")
            return makeBasic(ast::TypeKind::INT);
        if (name == "float")
            return makeBasic(ast::TypeKind::FLOAT);
        if (name == "bool")
            return makeBasic(ast::TypeKind::BOOL);
        if (name == "string" || name == "str")
            return makeBasic(ast::TypeKind::STRING);
        if (name == "void" || name == "None")
            return makeBasic(ast::TypeKind::VOID);
        return type;
    }

    ast::TypeKind TypeChecker::kindOf(ast::TypePtr type) const
    {
        auto c = canonicalize(type);
        if (auto basic = std::dynamic_pointer_cast<ast::BasicType>(c))
            return basic->getKind();
        return ast::TypeKind::UNKNOWN;
    }

    bool TypeChecker::isNumeric(ast::TypePtr type) const
    {
        auto k = kindOf(type);
        return k == ast::TypeKind::INT || k == ast::TypeKind::FLOAT;
    }

    // Structural equality usable for BasicType (whose equals() is identity-based).
    bool TypeChecker::sameType(ast::TypePtr a, ast::TypePtr b) const
    {
        if (!a || !b)
            return false;
        a = canonicalize(a);
        b = canonicalize(b);

        auto ab = std::dynamic_pointer_cast<ast::BasicType>(a);
        auto bb = std::dynamic_pointer_cast<ast::BasicType>(b);
        if (ab && bb)
            return ab->getKind() == bb->getKind();

        // Fall back to the AST's structural equals() for non-basic types
        // (GenericType, FunctionType, etc. override it correctly).
        if (a->equals(b))
            return true;
        // Last resort: compare string forms (covers SimpleType/ClassType names).
        return a->toString() == b->toString();
    }

    bool TypeChecker::isUnannotatedReturn(ast::TypePtr returnType) const
    {
        if (!returnType)
            return true;
        // The parser uses SimpleType with the literal name "None" as the
        // "no annotation" sentinel.
        if (auto simple = std::dynamic_pointer_cast<ast::SimpleType>(returnType))
        {
            const std::string n = simple->toString();
            if (n == "None" || n.empty())
                return true;
        }
        return false;
    }

    ast::TypePtr TypeChecker::functionTypeOf(ast::FunctionStmt *fn) const
    {
        std::vector<ast::TypePtr> paramTypes;
        for (const auto &p : fn->parameters)
        {
            paramTypes.push_back(canonicalize(p.type));
        }
        // If the return type is unannotated, default to UNKNOWN so callers stay
        // permissive until it is inferred; otherwise canonicalize the annotation.
        // Generic functions also expose UNKNOWN: their declared return type may
        // reference type parameters that are only resolved at instantiation, so
        // callers must not adopt the raw type-parameter as a concrete type.
        ast::TypePtr ret = (isUnannotatedReturn(fn->returnType) || fn->isGeneric())
                               ? makeBasic(ast::TypeKind::UNKNOWN)
                               : canonicalize(fn->returnType);
        return std::make_shared<ast::FunctionType>(fn->token, std::move(paramTypes), ret, fn->isAsync);
    }

    // Remove stale static consts; use ast::OptionType/ResultType toString instead

    void TypeChecker::visitArrayLiteralExpr(ast::ArrayLiteralExpr *expr)
    {
        // Check the type of each element in the array
        ast::TypePtr elementType = nullptr;

        for (const auto &element : expr->elements)
        {
            element->accept(*this);
            if (elementType == nullptr)
            {
                elementType = currentType_;
            }
            else if (!isAssignable(currentType_, elementType))
            {
                // If new element type doesn't match previous, either coerce or report error
                if (isAssignable(elementType, currentType_))
                {
                    elementType = currentType_; // Widen the type
                }
                else
                {
                    errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                              "Array literal has inconsistent element types",
                                              std::string(expr->token.filename), expr->token.line, expr->token.column,
                                              error::ErrorSeverity::ERROR);
                    break;
                }
            }
        }

        if (elementType == nullptr)
        {
            // Empty array, default to int for now
            elementType = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::IDENTIFIER, "int", "", 0, 0));
        }

        // Create an array type with the determined element type
        currentType_ = std::make_shared<ast::GenericType>(
            expr->token,
            "array",
            std::vector<ast::TypePtr>{elementType});
    }

    void TypeChecker::visitIndexExpr(ast::IndexExpr *expr)
    {
        // Type-check the object and index expressions.
        ast::TypePtr objType;
        if (expr->object) { expr->object->accept(*this); objType = currentType_; }
        if (expr->index) expr->index->accept(*this);

        // If the object is a known array/list, the result is its element type;
        // otherwise stay permissive (UNKNOWN) so codegen handles it.
        if (auto generic = std::dynamic_pointer_cast<ast::GenericType>(objType))
        {
            if ((generic->name == "array" || generic->name == "list" || generic->name == "List") &&
                !generic->typeArguments.empty())
            {
                currentType_ = generic->typeArguments[0];
                return;
            }
        }
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::UNKNOWN);
    }

    void TypeChecker::visitEnumStmt(ast::EnumStmt *stmt)
    {
        // Enum members are integer constants; make them visible by name.
        auto intType = makeBasic(ast::TypeKind::INT);
        for (const auto &m : stmt->members)
        {
            if (environment_) environment_->define(m.first, intType, true);
            if (globalEnv_) globalEnv_->define(m.first, intType, true);
        }
        currentType_ = nullptr;
    }

    void TypeChecker::visitTryStmt(ast::TryStmt *stmt)
    {
        // try block in its own scope.
        if (stmt->tryBlock)
        {
            pushScope();
            stmt->tryBlock->accept(*this);
            popScope();
        }
        // catch block: the caught value is bound as an int (exceptions carry a
        // 64-bit value).
        if (stmt->catchBlock)
        {
            pushScope();
            if (environment_ && !stmt->catchVar.empty())
                environment_->define(stmt->catchVar, makeBasic(ast::TypeKind::INT), true);
            stmt->catchBlock->accept(*this);
            popScope();
        }
        // finally block.
        if (stmt->finallyBlock)
        {
            pushScope();
            stmt->finallyBlock->accept(*this);
            popScope();
        }
        currentType_ = nullptr;
    }

    void TypeChecker::visitThrowStmt(ast::ThrowStmt *stmt)
    {
        if (stmt->value)
            stmt->value->accept(*this);
        currentType_ = nullptr;
    }

    void TypeChecker::visitBreakStmt(ast::BreakStmt *stmt)
    {
        (void)stmt;
        currentType_ = nullptr;
    }

    void TypeChecker::visitContinueStmt(ast::ContinueStmt *stmt)
    {
        (void)stmt;
        currentType_ = nullptr;
    }

    void TypeChecker::visitMoveExpr(void *expr)
    {
        // Not supported; no-op
        (void)expr;
        currentType_ = nullptr;
    }

    void TypeChecker::visitGoExpr(void *expr)
    {
        (void)expr;
        currentType_ = std::make_shared<ast::SimpleType>(lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    void TypeChecker::visitRuntimeChannelSendExpr(void *expr)
    {
        // Type check channel send (channel <- value)
        auto sendExpr = static_cast<ast::ChannelSendExpr*>(expr);
        if (!sendExpr) {
            currentType_ = nullptr;
            return;
        }

        // Check channel type
        if (sendExpr->channel) {
            sendExpr->channel->accept(*this);
            auto channelType = currentType_;
            
            // Check value type
            if (sendExpr->value) {
                sendExpr->value->accept(*this);
                auto valueType = currentType_;
                
                // Verify the value type matches the channel element type
                if (channelType && valueType) {
                    auto elementType = getChannelElementType(channelType);
                    if (elementType && !typesCompatible(valueType, elementType)) {
                        errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                                "Cannot send value of type " + valueType->toString() + 
                                                " to channel of type " + channelType->toString());
                    }
                }
            }
        }
        
        // Channel send returns void
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    void TypeChecker::visitRuntimeChannelReceiveExpr(void *expr)
    {
        // Type check channel receive (<-channel)
        auto receiveExpr = static_cast<ast::ChannelReceiveExpr*>(expr);
        if (!receiveExpr) {
            currentType_ = nullptr;
            return;
        }

        // Check channel type
        if (receiveExpr->channel) {
            receiveExpr->channel->accept(*this);
            auto channelType = currentType_;
            
            if (channelType) {
                // The type of a receive expression is the channel's element type
                currentType_ = getChannelElementType(channelType);
            } else {
                currentType_ = nullptr;
            }
        } else {
            currentType_ = nullptr;
        }
    }

    void TypeChecker::visitRuntimeSelectStmt(void *stmt)
    {
        // Type check select statement (concurrency)
        auto selectStmt = static_cast<ast::SelectStmt*>(stmt);
        if (!selectStmt) {
            currentType_ = nullptr;
            return;
        }

        // Check all cases in the select statement
        for (const auto& caseStmt : selectStmt->cases) {
            if (caseStmt.body) {
                caseStmt.body->accept(*this);
            }
        }
        
        // Select statement returns void
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    // Implementation for channel-related visitor methods
    void TypeChecker::visitChannelSendExpr(ast::ChannelSendExpr *expr)
    {
        // Type check channel send (channel <- value)
        if (expr->channel) expr->channel->accept(*this);
        if (expr->value) expr->value->accept(*this);
        
        // Channel send returns void
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
    }

    void TypeChecker::visitChannelReceiveExpr(ast::ChannelReceiveExpr *expr)
    {
        // Type check channel receive (<-channel)
        if (expr->channel) {
            expr->channel->accept(*this);
            auto channelType = currentType_;
            
            if (channelType) {
                // The type of a receive expression is the channel's element type
                currentType_ = getChannelElementType(channelType);
            } else {
                currentType_ = nullptr;
            }
        } else {
            currentType_ = nullptr;
        }
    }

    void TypeChecker::visitSelectStmt(ast::SelectStmt *stmt)
    {
        // Check each case's channel expression and body. The received value is
        // bound as an int (the 64-bit channel slot) in the case body's scope.
        for (auto &c : stmt->cases)
        {
            pushScope();
            if (!c.isDefault && c.channel)
                c.channel->accept(*this);
            if (environment_ && !c.bindName.empty())
                environment_->define(c.bindName, makeBasic(ast::TypeKind::INT), false);
            if (c.body)
                c.body->accept(*this);
            popScope();
        }
        currentType_ = nullptr;
    }

    // Check if one type can be assigned to another
    bool TypeChecker::isAssignable(ast::TypePtr from, ast::TypePtr to)
    {
        // Be permissive: if either side is unknown/null, assume it's fine so we
        // never emit a false error for a type we couldn't resolve.
        if (isUnknown(from) || isUnknown(to))
        {
            return true;
        }

        // Canonicalize named primitives so e.g. SimpleType("int") unifies with
        // BasicType(INT).
        from = canonicalize(from);
        to = canonicalize(to);

        // Same type is always assignable (use structural comparison since
        // BasicType::equals is identity-based).
        if (sameType(from, to))
        {
            return true;
        }

        // Handle basic type conversions
        if (auto fromBasic = std::dynamic_pointer_cast<ast::BasicType>(from))
        {
            if (auto toBasic = std::dynamic_pointer_cast<ast::BasicType>(to))
            {
                // Allow int to float conversion
                if (fromBasic->getKind() == ast::TypeKind::INT && toBasic->getKind() == ast::TypeKind::FLOAT)
                {
                    return true;
                }
            }
        }

        // Handle generic types
        if (auto fromGeneric = std::dynamic_pointer_cast<ast::GenericType>(from))
        {
            if (auto toGeneric = std::dynamic_pointer_cast<ast::GenericType>(to))
            {
                if (fromGeneric->name == toGeneric->name)
                {
                    // Check if type arguments are assignable
                    if (fromGeneric->typeArguments.size() == toGeneric->typeArguments.size())
                    {
                        for (size_t i = 0; i < fromGeneric->typeArguments.size(); ++i)
                        {
                            if (!isAssignable(fromGeneric->typeArguments[i], toGeneric->typeArguments[i]))
                            {
                                return false;
                            }
                        }
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Constructor for TypeChecker
    TypeChecker::TypeChecker(error::ErrorHandler &errorHandler, tocin::compiler::CompilationContext &context, FeatureManager *featureManager)
        : errorHandler_(errorHandler), compilationContext_(context), featureManager_(featureManager)
    {
        globalEnv_ = std::make_shared<Environment>();
        environment_ = globalEnv_;
        registerBuiltins();
    }

    void TypeChecker::registerBuiltins()
    {
        // Register a handful of commonly available builtins as functions so that
        // calling them does not get flagged as "calling a non-function". Their
        // return types are left UNKNOWN to stay permissive.
        auto unknown = makeBasic(ast::TypeKind::UNKNOWN);
        lexer::Token tok(lexer::TokenType::IDENTIFIER, "", "", 0, 0);
        auto builtinFn = [&]() {
            return std::make_shared<ast::FunctionType>(
                tok, std::vector<ast::TypePtr>{}, makeBasic(ast::TypeKind::UNKNOWN), false);
        };
        const char *names[] = {"print", "println", "printf", "len", "str", "int",
                               "float", "bool", "input", "range", "abs", "min", "max"};
        for (const char *n : names)
        {
            if (!globalEnv_->lookup(n))
                globalEnv_->define(n, builtinFn(), false);
        }
    }

    // Pass 1 helper: hoist a single top-level declaration into the global env.
    void TypeChecker::hoistDeclaration(ast::Statement *stmt)
    {
        if (!stmt)
            return;
        if (auto fn = dynamic_cast<ast::FunctionStmt *>(stmt))
        {
            globalEnv_->define(fn->name, functionTypeOf(fn), true);
        }
        else if (auto cls = dynamic_cast<ast::ClassStmt *>(stmt))
        {
            // Register the class name as a class type so references resolve and
            // constructor-style calls don't error.
            globalEnv_->define(cls->name,
                               std::make_shared<ast::ClassType>(cls->token, cls->name), true);
        }
    }

    // Two-pass program checking.
    void TypeChecker::checkProgram(ast::Statement *root)
    {
        // PASS 1: hoist top-level function/class names so forward and
        // mutually-recursive references resolve during body checking.
        if (auto block = dynamic_cast<ast::BlockStmt *>(root))
        {
            for (auto &s : block->statements)
                hoistDeclaration(s.get());
        }
        else if (auto mod = dynamic_cast<ast::ModuleStmt *>(root))
        {
            for (auto &s : mod->body)
                hoistDeclaration(s.get());
        }
        else
        {
            // Single top-level statement (parser returns the statement directly
            // when there's exactly one): hoist it if it is a declaration.
            hoistDeclaration(root);
        }

        // PASS 2: check bodies. Mark that the next block/module is the program
        // root so its direct children live in the global scope.
        atProgramRoot_ = true;
        root->accept(*this);
        atProgramRoot_ = false;
    }

    // Check method for type checking statements
    ast::TypePtr TypeChecker::check(ast::StmtPtr stmt)
    {
        if (!stmt)
        {
            return nullptr;
        }

        try
        {
            // The outermost call is the program root: perform two-pass checking.
            if (checkDepth_ == 0)
            {
                ++checkDepth_;
                checkProgram(stmt.get());
                --checkDepth_;
                return currentType_;
            }

            stmt->accept(*this);
            return currentType_;
        }
        catch (const std::exception &e)
        {
            // Never let an exception escape; stay permissive but record nothing
            // fatal so valid programs are unaffected.
            (void)e;
            return nullptr;
        }
    }

    // Missing virtual function implementations
    void TypeChecker::visitLiteralExpr(ast::LiteralExpr *expr)
    {
        // Set current type based on literal type
        switch (expr->literalType)
        {
        case ast::LiteralExpr::LiteralType::INTEGER:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::INT);
            break;
        case ast::LiteralExpr::LiteralType::FLOAT:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::FLOAT);
            break;
        case ast::LiteralExpr::LiteralType::BOOLEAN:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::BOOL);
            break;
        case ast::LiteralExpr::LiteralType::STRING:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::STRING);
            break;
        case ast::LiteralExpr::LiteralType::NIL:
            // nil's type is unknown here; stay permissive.
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::UNKNOWN);
            break;
        default:
            currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::UNKNOWN);
            break;
        }
    }

    void TypeChecker::visitUnaryExpr(ast::UnaryExpr *expr)
    {
        ast::TypePtr operand;
        if (expr->right)
        {
            expr->right->accept(*this);
            operand = currentType_;
        }

        switch (expr->op.type)
        {
        case lexer::TokenType::BANG:
        case lexer::TokenType::NOT_EQUAL: // defensive; '!' is BANG
            // Logical not yields bool.
            currentType_ = makeBasic(ast::TypeKind::BOOL);
            break;
        case lexer::TokenType::MINUS:
        case lexer::TokenType::PLUS:
            // Unary +/- preserves the operand's (numeric) type.
            currentType_ = operand ? operand : makeBasic(ast::TypeKind::UNKNOWN);
            break;
        default:
            currentType_ = operand ? operand : makeBasic(ast::TypeKind::UNKNOWN);
            break;
        }
    }

    void TypeChecker::visitAssignExpr(ast::AssignExpr *expr)
    {
        ast::TypePtr valueType = makeBasic(ast::TypeKind::UNKNOWN);
        if (expr->value)
        {
            expr->value->accept(*this);
            valueType = currentType_;
        }

        // Confirm the target exists (for simple variable assignments). We don't
        // error on unknown targets to stay permissive.
        if (expr->isVariableAssignment() && environment_)
        {
            environment_->assign(expr->name, valueType);
        }
        else if (expr->target)
        {
            // Visit complex targets (obj.field, arr[i]) for completeness.
            expr->target->accept(*this);
        }

        // An assignment expression evaluates to the assigned value's type.
        currentType_ = valueType;
    }

    void TypeChecker::visitCallExpr(ast::CallExpr *expr)
    {
        // Type-check arguments regardless so their expressions are visited.
        ast::TypePtr calleeType;
        if (expr->callee)
        {
            expr->callee->accept(*this);
            calleeType = currentType_;
        }
        for (auto &arg : expr->arguments)
        {
            if (arg)
                arg->accept(*this);
        }

        // Resolve the callee to a function type and adopt its return type.
        if (auto fnType = std::dynamic_pointer_cast<ast::FunctionType>(calleeType))
        {
            currentType_ = fnType->returnType ? fnType->returnType
                                              : makeBasic(ast::TypeKind::UNKNOWN);
            return;
        }

        // If the callee resolved to a concrete non-function, non-unknown type,
        // that's a clear violation (calling a non-function).
        if (!isUnknown(calleeType) &&
            !std::dynamic_pointer_cast<ast::ClassType>(calleeType))
        {
            // Only report when we have a precise callee identity (a bare name).
            if (auto var = std::dynamic_pointer_cast<ast::VariableExpr>(expr->callee))
            {
                errorHandler_.reportError(
                    error::ErrorCode::T007_INVALID_FUNCTION_CALL,
                    "Cannot call '" + var->name + "' of type " + calleeType->toString() +
                        " because it is not a function",
                    std::string(expr->token.filename), expr->token.line, expr->token.column,
                    error::ErrorSeverity::ERROR);
            }
        }

        // Unknown / class (constructor) callee: stay permissive.
        currentType_ = makeBasic(ast::TypeKind::UNKNOWN);
    }

    void TypeChecker::visitGetExpr(ast::GetExpr *expr)
    {
        if (expr->object) expr->object->accept(*this);
        // Field/method types are not tracked symbolically yet; stay permissive
        // by reporting UNKNOWN so member access never triggers false errors.
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::UNKNOWN);
    }

    void TypeChecker::visitSetExpr(ast::SetExpr *expr)
    {
        if (expr->object) expr->object->accept(*this);
        if (expr->value) expr->value->accept(*this);
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::UNKNOWN);
    }

    void TypeChecker::visitListExpr(ast::ListExpr *expr)
    {
        // For now, return a generic list type
        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "List", std::vector<ast::TypePtr>{});
    }

    void TypeChecker::visitDictionaryExpr(ast::DictionaryExpr *expr)
    {
        // For now, return a generic dictionary type
        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "Dict", std::vector<ast::TypePtr>{});
    }

    void TypeChecker::visitLambdaExpr(ast::LambdaExpr *expr)
    {
        // A lambda has a first-class function type built from its parameter
        // types and (annotated or unknown) return type.
        std::vector<ast::TypePtr> paramTypes;
        for (const auto &p : expr->parameters)
            paramTypes.push_back(p.type ? canonicalize(p.type) : makeBasic(ast::TypeKind::UNKNOWN));

        ast::TypePtr ret;
        auto rb = std::dynamic_pointer_cast<ast::SimpleType>(expr->returnType);
        if (!expr->returnType || (rb && (rb->toString() == "None" || rb->toString().empty())))
            ret = makeBasic(ast::TypeKind::UNKNOWN);
        else
            ret = canonicalize(expr->returnType);

        currentType_ = std::make_shared<ast::FunctionType>(
            expr->token, std::move(paramTypes), ret, false);
    }

    void TypeChecker::visitDeleteExpr(ast::DeleteExpr *expr)
    {
        if (expr->getExpr()) expr->getExpr()->accept(*this);
        // Delete expressions return void
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::VOID);
    }

    void TypeChecker::visitStringInterpolationExpr(ast::StringInterpolationExpr *expr)
    {
        // String interpolation returns a string
        currentType_ = std::make_shared<ast::BasicType>(ast::TypeKind::STRING);
    }

    void TypeChecker::visitVariableStmt(ast::VariableStmt *stmt)
    {
        ast::TypePtr initType;
        if (stmt->initializer)
        {
            stmt->initializer->accept(*this);
            initType = currentType_;
        }

        // Determine the variable's static type: prefer an explicit annotation,
        // otherwise infer from the initializer, otherwise unknown.
        ast::TypePtr varType;
        if (stmt->type)
        {
            varType = canonicalize(stmt->type);
            // If both an annotation and an initializer exist and they clearly
            // disagree (both concrete & known), report a mismatch.
            if (initType && !isUnknown(initType) && !isUnknown(varType) &&
                !isAssignable(initType, varType))
            {
                errorHandler_.reportError(
                    error::ErrorCode::T001_TYPE_MISMATCH,
                    "Cannot initialize variable '" + stmt->name + "' of type " +
                        varType->toString() + " with value of type " + initType->toString(),
                    std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                    error::ErrorSeverity::ERROR);
            }
        }
        else if (initType && !isUnknown(initType))
        {
            varType = initType;
        }
        else
        {
            varType = makeBasic(ast::TypeKind::UNKNOWN);
        }

        if (environment_)
            environment_->define(stmt->name, varType, stmt->isConstant);

        currentType_ = nullptr;
    }

    void TypeChecker::visitIfStmt(ast::IfStmt *stmt)
    {
        if (stmt->condition) stmt->condition->accept(*this);
        if (stmt->thenBranch)
        {
            pushScope();
            stmt->thenBranch->accept(*this);
            popScope();
        }
        for (auto &elif : stmt->elifBranches)
        {
            if (elif.first) elif.first->accept(*this);
            if (elif.second)
            {
                pushScope();
                elif.second->accept(*this);
                popScope();
            }
        }
        if (stmt->elseBranch)
        {
            pushScope();
            stmt->elseBranch->accept(*this);
            popScope();
        }
        // If statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitWhileStmt(ast::WhileStmt *stmt)
    {
        if (stmt->condition) stmt->condition->accept(*this);
        if (stmt->body)
        {
            pushScope();
            stmt->body->accept(*this);
            popScope();
        }
        // While statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitForStmt(ast::ForStmt *stmt)
    {
        if (stmt->iterable) stmt->iterable->accept(*this);
        // Introduce the loop variable in a fresh scope for the body.
        pushScope();
        if (environment_)
        {
            ast::TypePtr loopVarType = stmt->variableType
                                           ? canonicalize(stmt->variableType)
                                           : makeBasic(ast::TypeKind::UNKNOWN);
            environment_->define(stmt->variable, loopVarType, false);
        }
        if (stmt->body) stmt->body->accept(*this);
        popScope();
        // For statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitFunctionStmt(ast::FunctionStmt *stmt)
    {
        // Ensure the function name is visible (it normally is via hoisting, but
        // nested/local functions are hoisted here).
        if (environment_ && !environment_->lookup(stmt->name))
        {
            environment_->define(stmt->name, functionTypeOf(stmt), true);
        }

        // A function body is never the "program root" block, even when this
        // function is the sole top-level statement. Clearing the flag makes the
        // body get its own lexical scope uniformly.
        atProgramRoot_ = false;

        // Save outer context.
        auto savedReturnTypes = currentReturnTypes_;
        bool savedReturnExplicit = returnTypeIsExplicit_;
        ast::TypePtr savedExpectedReturn = expectedReturnType_;
        bool savedAsync = inAsyncContext_;

        // New scope for parameters and body.
        pushScope();

        // Inside a generic function, type parameters are abstract: bind any
        // parameter typed by a type-parameter name to UNKNOWN so the body checks
        // permissively (operations on T are validated at instantiation).
        std::unordered_set<std::string> typeParamNames;
        for (const auto &tp : stmt->typeParameters)
            typeParamNames.insert(tp.getName());
        // Type parameters of the enclosing generic class are abstract here too.
        for (const auto &name : classTypeParams_)
            typeParamNames.insert(name);
        auto resolveParamType = [&](const ast::TypePtr &t) -> ast::TypePtr {
            if (t)
            {
                if (auto s = std::dynamic_pointer_cast<ast::SimpleType>(t))
                    if (typeParamNames.count(s->toString()))
                        return makeBasic(ast::TypeKind::UNKNOWN);
                return canonicalize(t);
            }
            return makeBasic(ast::TypeKind::UNKNOWN);
        };

        for (const auto &param : stmt->parameters)
        {
            if (environment_)
                environment_->define(param.name, resolveParamType(param.type), false);
        }

        // Determine the declared return type (if any) and set up inference.
        std::vector<ast::TypePtr> returnTypes;
        currentReturnTypes_ = &returnTypes;
        inAsyncContext_ = stmt->isAsync;

        // Generic functions (and methods whose return type is a type parameter)
        // return an abstract type; check permissively.
        bool returnIsTypeParam = false;
        if (auto s = std::dynamic_pointer_cast<ast::SimpleType>(stmt->returnType))
            returnIsTypeParam = typeParamNames.count(s->toString()) > 0;
        const bool unannotated =
            isUnannotatedReturn(stmt->returnType) || stmt->isGeneric() || returnIsTypeParam;
        if (!unannotated)
        {
            expectedReturnType_ = canonicalize(stmt->returnType);
            returnTypeIsExplicit_ = true;
        }
        else
        {
            expectedReturnType_ = nullptr;
            returnTypeIsExplicit_ = false;
        }

        // Check the body.
        if (stmt->body)
            stmt->body->accept(*this);

        // If the return type was not annotated, infer it and write it back so
        // codegen emits the correct LLVM return type.
        if (unannotated)
        {
            ast::TypePtr inferred;
            for (auto &rt : returnTypes)
            {
                if (!rt || isUnknown(rt))
                    continue;
                if (!inferred)
                {
                    inferred = canonicalize(rt);
                }
                else
                {
                    auto c = canonicalize(rt);
                    if (!sameType(inferred, c))
                    {
                        // Mixed concrete return types: widen int+float -> float,
                        // otherwise give up on inference (leave default void).
                        if (isNumeric(inferred) && isNumeric(c))
                        {
                            inferred = makeBasic(ast::TypeKind::FLOAT);
                        }
                        else
                        {
                            inferred = nullptr;
                            break;
                        }
                    }
                }
            }
            // Only write back a concrete inferred type. If inference was
            // inconclusive (e.g. the body returns a member/method result the
            // checker can't resolve yet), leave the annotation null so codegen
            // performs its own return-type inference rather than forcing void.
            if (inferred)
                stmt->returnType = inferred;
        }

        popScope();

        // Restore outer context.
        currentReturnTypes_ = savedReturnTypes;
        returnTypeIsExplicit_ = savedReturnExplicit;
        expectedReturnType_ = savedExpectedReturn;
        inAsyncContext_ = savedAsync;

        // Function statements don't have a value type.
        currentType_ = nullptr;
    }

    void TypeChecker::visitReturnStmt(ast::ReturnStmt *stmt)
    {
        ast::TypePtr valueType;
        if (stmt->value)
        {
            stmt->value->accept(*this);
            valueType = currentType_;
        }
        else
        {
            valueType = makeBasic(ast::TypeKind::VOID);
        }

        // Record for return-type inference of the enclosing function.
        if (currentReturnTypes_)
            currentReturnTypes_->push_back(valueType);

        // If the enclosing function has an EXPLICIT return annotation and this
        // return value clearly mismatches it, report a precise error.
        if (returnTypeIsExplicit_ && expectedReturnType_ &&
            !isUnknown(expectedReturnType_))
        {
            if (stmt->value)
            {
                if (!isUnknown(valueType) && !isAssignable(valueType, expectedReturnType_))
                {
                    errorHandler_.reportError(
                        error::ErrorCode::T014_INVALID_RETURN_TYPE,
                        "Return value of type " + canonicalize(valueType)->toString() +
                            " does not match declared return type " +
                            expectedReturnType_->toString(),
                        std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                        error::ErrorSeverity::ERROR);
                }
            }
            else
            {
                // Returning nothing from a non-void function.
                auto expBasic = std::dynamic_pointer_cast<ast::BasicType>(expectedReturnType_);
                if (!(expBasic && expBasic->getKind() == ast::TypeKind::VOID))
                {
                    errorHandler_.reportError(
                        error::ErrorCode::T014_INVALID_RETURN_TYPE,
                        "Missing return value in function declared to return " +
                            expectedReturnType_->toString(),
                        std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                        error::ErrorSeverity::ERROR);
                }
            }
        }

        currentType_ = nullptr;
    }

    void TypeChecker::visitClassStmt(ast::ClassStmt *stmt)
    {
        // Make the class name available as a type.
        if (environment_ && !environment_->lookup(stmt->name))
        {
            environment_->define(stmt->name,
                                 std::make_shared<ast::ClassType>(stmt->token, stmt->name), true);
        }

        // Check field declarations and methods within a class scope. For a
        // generic class, expose its type parameters so methods that mention them
        // (fields of type T, `-> T` returns) are checked permissively.
        auto savedClassTypeParams = classTypeParams_;
        classTypeParams_.clear();
        for (const auto &tp : stmt->typeParameters)
            classTypeParams_.insert(tp.getName());

        pushScope();
        for (auto &field : stmt->fields)
        {
            if (field)
                field->accept(*this);
        }
        for (auto &method : stmt->methods)
        {
            if (method)
                method->accept(*this);
        }
        popScope();

        classTypeParams_ = savedClassTypeParams;
        currentType_ = nullptr;
    }

    void TypeChecker::visitBinaryExpr(ast::BinaryExpr *expr)
    {
        ast::TypePtr leftType, rightType;
        if (expr->left)
        {
            expr->left->accept(*this);
            leftType = currentType_;
        }
        if (expr->right)
        {
            expr->right->accept(*this);
            rightType = currentType_;
        }

        leftType = canonicalize(leftType);
        rightType = canonicalize(rightType);

        switch (expr->op.type)
        {
        // Comparisons -> bool
        case lexer::TokenType::EQUAL_EQUAL:
        case lexer::TokenType::BANG_EQUAL:
        case lexer::TokenType::NOT_EQUAL:
        case lexer::TokenType::STRICT_EQUAL:
        case lexer::TokenType::STRICT_NOT_EQUAL:
        case lexer::TokenType::LESS:
        case lexer::TokenType::LESS_EQUAL:
        case lexer::TokenType::GREATER:
        case lexer::TokenType::GREATER_EQUAL:
            currentType_ = makeBasic(ast::TypeKind::BOOL);
            return;

        // Logical -> bool
        case lexer::TokenType::AND:
        case lexer::TokenType::OR:
            currentType_ = makeBasic(ast::TypeKind::BOOL);
            return;

        default:
            break;
        }

        // Arithmetic operators (+ - * / % etc.)
        const bool isArithmetic =
            expr->op.type == lexer::TokenType::PLUS ||
            expr->op.type == lexer::TokenType::MINUS ||
            expr->op.type == lexer::TokenType::STAR ||
            expr->op.type == lexer::TokenType::SLASH ||
            expr->op.type == lexer::TokenType::PERCENT ||
            expr->op.type == lexer::TokenType::POWER;

        if (isArithmetic)
        {
            // String concatenation with '+': if either operand is a string and
            // the other is a string or unknown (e.g. a builtin like intToStr
            // whose return type the checker sees as UNKNOWN), the result is a
            // string. This keeps `s + intToStr(n) + "-"` chains valid.
            if (expr->op.type == lexer::TokenType::PLUS)
            {
                auto lb = std::dynamic_pointer_cast<ast::BasicType>(leftType);
                auto rb = std::dynamic_pointer_cast<ast::BasicType>(rightType);
                bool lStr = lb && lb->getKind() == ast::TypeKind::STRING;
                bool rStr = rb && rb->getKind() == ast::TypeKind::STRING;
                if ((lStr && (rStr || isUnknown(rightType))) ||
                    (rStr && (lStr || isUnknown(leftType))))
                {
                    currentType_ = makeBasic(ast::TypeKind::STRING);
                    return;
                }
            }

            bool lUnknown = isUnknown(leftType);
            bool rUnknown = isUnknown(rightType);
            bool lNum = isNumeric(leftType);
            bool rNum = isNumeric(rightType);

            // Clear violation: a concrete non-numeric operand mixed with a number
            // (e.g. string * int). Only report when both sides are known.
            if (!lUnknown && !rUnknown)
            {
                if (!lNum || !rNum)
                {
                    errorHandler_.reportError(
                        error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                        "Operator '" + expr->op.value + "' cannot be applied to operands of type " +
                            leftType->toString() + " and " + rightType->toString(),
                        std::string(expr->token.filename), expr->token.line, expr->token.column,
                        error::ErrorSeverity::ERROR);
                    currentType_ = makeBasic(ast::TypeKind::UNKNOWN);
                    return;
                }
            }

            // Numeric result: float if either side is float, else int. If a side
            // is unknown, fall back to the other known side, else int.
            auto isFloat = [&](ast::TypePtr t) {
                auto b = std::dynamic_pointer_cast<ast::BasicType>(t);
                return b && b->getKind() == ast::TypeKind::FLOAT;
            };
            if (isFloat(leftType) || isFloat(rightType))
                currentType_ = makeBasic(ast::TypeKind::FLOAT);
            else
                currentType_ = makeBasic(ast::TypeKind::INT);
            return;
        }

        // Bitwise / shift operators -> int (when numeric), else permissive.
        // Fallback: prefer a known operand type, else unknown.
        if (!isUnknown(leftType))
            currentType_ = leftType;
        else if (!isUnknown(rightType))
            currentType_ = rightType;
        else
            currentType_ = makeBasic(ast::TypeKind::UNKNOWN);
    }

    void TypeChecker::visitGroupingExpr(ast::GroupingExpr *expr)
    {
        if (expr->expression)
            expr->expression->accept(*this);
        else
            currentType_ = makeBasic(ast::TypeKind::UNKNOWN);
    }

    void TypeChecker::visitVariableExpr(ast::VariableExpr *expr)
    {
        // Look up the variable/function in the current scope chain.
        ast::TypePtr type;
        if (environment_)
            type = environment_->lookup(expr->name);

        // Unknown identifiers are treated permissively (could be a builtin or a
        // symbol introduced elsewhere). We do not error here to avoid regressions.
        currentType_ = type ? type : makeBasic(ast::TypeKind::UNKNOWN);
    }

    void TypeChecker::visitExpressionStmt(ast::ExpressionStmt *stmt)
    {
        if (stmt->expression) stmt->expression->accept(*this);
    }

    void TypeChecker::visitBlockStmt(ast::BlockStmt *stmt)
    {
        // The program-root block runs in the global scope (already populated by
        // pass-1 hoisting). Nested blocks get their own lexical scope.
        bool isRoot = atProgramRoot_;
        atProgramRoot_ = false; // children of this block are not the root

        if (!isRoot)
            pushScope();

        for (auto &statement : stmt->statements)
        {
            if (statement)
                statement->accept(*this);
        }

        if (!isRoot)
            popScope();

        currentType_ = nullptr;
    }

    void TypeChecker::visitImportStmt(ast::ImportStmt *stmt)
    {
        // Import statements are handled at compile time, not runtime
    }

    void TypeChecker::visitMatchStmt(ast::MatchStmt *stmt)
    {
        // Basic match statement implementation
        if (stmt->value) stmt->value->accept(*this);
        
        // For now, just visit the first case
        if (!stmt->cases.empty() && stmt->cases[0].second)
        {
            stmt->cases[0].second->accept(*this);
        }
    }

    void TypeChecker::visitNewExpr(ast::NewExpr *expr)
    {
        // Basic new expression - allocate memory
        ast::TypePtr type = expr->getType();
        if (type)
        {
            currentType_ = type;
        }
    }

    void TypeChecker::visitExportStmt(ast::ExportStmt *stmt)
    {
        // Export statements are handled at compile time
    }

    void TypeChecker::visitModuleStmt(ast::ModuleStmt *stmt)
    {
        // If this module is the program root, its members were hoisted into the
        // global env already; otherwise hoist its members now so intra-module
        // references resolve, and check within the global scope.
        bool isRoot = atProgramRoot_;
        atProgramRoot_ = false;

        if (!isRoot)
        {
            for (auto &s : stmt->body)
                hoistDeclaration(s.get());
        }

        for (auto &statement : stmt->body)
        {
            if (statement)
                statement->accept(*this);
        }

        currentType_ = nullptr;
    }

    void TypeChecker::visitAwaitExpr(ast::AwaitExpr *expr)
    {
        // Basic await expression - just visit the inner expression
        if (expr->expression) expr->expression->accept(*this);
    }

    void TypeChecker::visitGoStmt(ast::GoStmt* stmt) {
        // Type check goroutine launch (go statement)
        if (stmt->expression) stmt->expression->accept(*this);
        // In practice, check that the expression is a function call and is goroutine-safe
        currentType_ = nullptr;
    }

    bool TypeChecker::canRunAsGoroutine(ast::ExprPtr expr) {
        // Check if an expression can be run as a goroutine
        // For now, allow any function call
        if (auto callExpr = std::dynamic_pointer_cast<ast::CallExpr>(expr)) {
            // Type check the function call
            callExpr->accept(*this);
            
            // Get the function type
            ast::TypePtr funcType = callExpr->getType();
            if (!funcType) {
                return false;
            }
            
            // Verify it's a function type
            if (auto functionType = std::dynamic_pointer_cast<ast::FunctionType>(funcType)) {
                // For now, allow any function to be run as a goroutine
                // In the future, we could add restrictions (e.g., no variadic functions, etc.)
                return true;
            }
        }
        
        return false;
    }

    bool TypeChecker::validateGoroutineLaunch(ast::ExprPtr function, const std::vector<ast::ExprPtr>& arguments) {
        // Validate that a function can be launched as a goroutine with the given arguments
        if (!canRunAsGoroutine(function)) {
            return false;
        }
        
        // Type check the function call with arguments
        if (auto callExpr = std::dynamic_pointer_cast<ast::CallExpr>(function)) {
            // Verify argument types match function signature
            // This would be done during normal function call type checking
            return true;
        }
        
        return false;
    }

    void TypeChecker::visitTraitStmt(ast::TraitStmt *stmt) {
        // Type check trait statement
        // Register the trait in the trait manager if available
        if (featureManager_) {
            // In practice, would register the trait with the trait manager
            // featureManager_->traitManager.registerTrait(stmt->name, stmt);
        }
        
        // Type check all method signatures in the trait
        for (const auto& method : stmt->methods) {
            if (method) {
                method->accept(*this);
            }
        }
        
        // Trait statements don't have a type
        currentType_ = nullptr;
    }

    void TypeChecker::visitImplStmt(ast::ImplStmt *stmt) {
        // Type check implementation statement
        // Verify that the implementation satisfies the trait requirements
        if (featureManager_) {
            // In practice, would verify the implementation against the trait
            // auto trait = featureManager_->traitManager.getTrait(stmt->traitName);
            // if (trait) {
            //     featureManager_->traitManager.verifyImplementation(trait.get(), stmt);
            // }
        }
        
        // Type check all method implementations
        for (const auto& method : stmt->methods) {
            if (method) {
                method->accept(*this);
            }
        }
        
        // Implementation statements don't have a type
        currentType_ = nullptr;
    }

    // Helper methods for type checking
    // isMovableType removed

    ast::TypePtr TypeChecker::getChannelElementType(ast::TypePtr channelType) {
        if (!channelType) return nullptr;
        if (auto generic = std::dynamic_pointer_cast<ast::GenericType>(channelType)) {
            if (generic->name == "Channel" && !generic->typeArguments.empty()) {
                return generic->typeArguments[0];
            }
        }
        return nullptr;
    }

    bool TypeChecker::typesCompatible(ast::TypePtr type1, ast::TypePtr type2) {
        if (!type1 || !type2) return false;
        if (type1->equals(type2)) return true;
        return isAssignable(type1, type2) || isAssignable(type2, type1);
    }

    // ---------------------------------------------------------------------------
    // Scope management
    // ---------------------------------------------------------------------------
    void TypeChecker::pushScope()
    {
        environment_ = std::make_shared<Environment>(environment_);
    }

    void TypeChecker::popScope()
    {
        if (environment_ && environment_->getParent())
        {
            environment_ = environment_->getParent();
        }
    }

    ast::TypePtr TypeChecker::resolveType(ast::TypePtr type)
    {
        // Canonicalize primitive named types to BasicType; leave others as-is.
        return canonicalize(type);
    }

} // namespace type_checker 