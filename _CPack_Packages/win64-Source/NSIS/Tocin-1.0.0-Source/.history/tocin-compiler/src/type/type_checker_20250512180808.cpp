#include "type_checker.h"
#include <stdexcept>

namespace type_checker
{

    void Environment::define(const std::string &name, ast::TypePtr type, bool isConstant)
    {
        if (!type)
        {
            throw std::runtime_error("Cannot define variable with null type");
        }
        variables_[name] = {type, isConstant};
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
            if (it->second.second)
            {
                return false; // Cannot assign to constant
            }
            it->second.first = type;
            return true;
        }
        if (parent_)
        {
            return parent_->assign(name, type);
        }
        return false;
    }

    TypeChecker::TypeChecker(error::ErrorHandler &errorHandler)
        : errorHandler_(errorHandler), environment_(std::make_shared<Environment>()),
          globalEnv_(environment_) {}

    ast::TypePtr TypeChecker::check(ast::StmtPtr stmt)
    {
        if (!stmt)
        {
            errorHandler_.reportError(error::ErrorCode::T009_CANNOT_INFER_TYPE,
                                      "Cannot type check null statement",
                                      "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }

        try
        {
            // Create a global environment if not already created
            if (!globalEnv_)
            {
                globalEnv_ = std::make_shared<Environment>();
                environment_ = globalEnv_;

                // Register built-in types and functions
                registerBuiltins();
            }

            // Visit the statement
            stmt->accept(*this);

            return currentType_;
        }
        catch (const std::exception &e)
        {
            errorHandler_.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                      "Type checking error: " + std::string(e.what()),
                                      "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }
    }

    void TypeChecker::pushScope()
    {
        environment_ = std::make_shared<Environment>(environment_);
    }

    void TypeChecker::popScope()
    {
        if (environment_ && environment_ != globalEnv_)
        {
            environment_ = environment_->getParent();
            if (!environment_)
            {
                environment_ = globalEnv_;
            }
        }
    }

    bool TypeChecker::isAssignable(ast::TypePtr from, ast::TypePtr to)
    {
        if (!from || !to)
            return false;
        if (from == to)
            return true;

        if (auto fromUnion = std::dynamic_pointer_cast<ast::UnionType>(from))
        {
            for (const auto &t : fromUnion->types)
            {
                if (!isAssignable(t, to))
                    return false;
            }
            return true;
        }
        if (auto toUnion = std::dynamic_pointer_cast<ast::UnionType>(to))
        {
            for (const auto &t : toUnion->types)
            {
                if (isAssignable(from, t))
                    return true;
            }
            return false;
        }

        auto fromSimple = std::dynamic_pointer_cast<ast::SimpleType>(from);
        auto toSimple = std::dynamic_pointer_cast<ast::SimpleType>(to);
        if (fromSimple && toSimple)
        {
            return fromSimple->token.value == toSimple->token.value ||
                   (fromSimple->token.value == "int" && toSimple->token.value == "float");
        }
        return false;
    }

    ast::TypePtr TypeChecker::resolveType(ast::TypePtr type)
    {
        if (!type)
            return nullptr;
        if (auto simple = std::dynamic_pointer_cast<ast::SimpleType>(type))
        {
            return type;
        }
        if (auto generic = std::dynamic_pointer_cast<ast::GenericType>(type))
        {
            std::vector<ast::TypePtr> resolvedArgs;
            for (const auto &arg : generic->typeArguments)
            {
                resolvedArgs.push_back(resolveType(arg));
            }
            return std::make_shared<ast::GenericType>(generic->token, generic->name, resolvedArgs);
        }
        if (auto func = std::dynamic_pointer_cast<ast::FunctionType>(type))
        {
            std::vector<ast::TypePtr> resolvedParams;
            for (const auto &param : func->paramTypes)
            {
                resolvedParams.push_back(resolveType(param));
            }
            return std::make_shared<ast::FunctionType>(
                func->token, resolvedParams, resolveType(func->returnType));
        }
        if (auto unionType = std::dynamic_pointer_cast<ast::UnionType>(type))
        {
            std::vector<ast::TypePtr> resolvedTypes;
            for (const auto &t : unionType->types)
            {
                resolvedTypes.push_back(resolveType(t));
            }
            return std::make_shared<ast::UnionType>(unionType->token, resolvedTypes);
        }
        return type;
    }

    void TypeChecker::visitBinaryExpr(ast::BinaryExpr *expr)
    {
        expr->left->accept(*this);
        ast::TypePtr leftType = currentType_;

        expr->right->accept(*this);
        ast::TypePtr rightType = currentType_;

        // Handle numerical operations
        if (expr->op.type == lexer::TokenType::PLUS ||
            expr->op.type == lexer::TokenType::MINUS ||
            expr->op.type == lexer::TokenType::STAR ||
            expr->op.type == lexer::TokenType::SLASH ||
            expr->op.type == lexer::TokenType::PERCENT)
        {

            if (leftType->toString() == "int" && rightType->toString() == "int")
            {
                currentType_ = std::make_shared<ast::SimpleType>(expr->token);
                currentType_->toString() = "int";
                return;
            }

            if ((leftType->toString() == "float" || leftType->toString() == "float64" ||
                 leftType->toString() == "float32") &&
                (rightType->toString() == "float" || rightType->toString() == "float64" ||
                 rightType->toString() == "float32" || rightType->toString() == "int"))
            {

                currentType_ = std::make_shared<ast::SimpleType>(expr->token);
                currentType_->toString() = "float";
                return;
            }

            // String concatenation
            if (expr->op.type == lexer::TokenType::PLUS &&
                leftType->toString() == "string" && rightType->toString() == "string")
            {
                currentType_ = std::make_shared<ast::SimpleType>(expr->token);
                currentType_->toString() = "string";
                return;
            }

            errorHandler_.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                      "Invalid operands for binary operator " + expr->op.value +
                                          ": " + leftType->toString() + " and " + rightType->toString(),
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
        }

        // Handle comparison operations
        if (expr->op.type == lexer::TokenType::LESS ||
            expr->op.type == lexer::TokenType::LESS_EQUAL ||
            expr->op.type == lexer::TokenType::GREATER ||
            expr->op.type == lexer::TokenType::GREATER_EQUAL)
        {

            if ((leftType->toString() == "int" || leftType->toString() == "float" ||
                 leftType->toString() == "float64" || leftType->toString() == "float32") &&
                (rightType->toString() == "int" || rightType->toString() == "float" ||
                 rightType->toString() == "float64" || rightType->toString() == "float32"))
            {

                currentType_ = std::make_shared<ast::SimpleType>(expr->token);
                currentType_->toString() = "bool";
                return;
            }

            errorHandler_.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                      "Invalid operands for comparison operator " + expr->op.value +
                                          ": " + leftType->toString() + " and " + rightType->toString(),
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
        }

        // Handle equality operations
        if (expr->op.type == lexer::TokenType::EQUAL_EQUAL ||
            expr->op.type == lexer::TokenType::BANG_EQUAL)
        {

            // Most types can be compared for equality
            currentType_ = std::make_shared<ast::SimpleType>(expr->token);
            currentType_->toString() = "bool";
            return;
        }

        // Default to "error" type
        currentType_ = std::make_shared<ast::SimpleType>(expr->token);
    }

    void TypeChecker::visitGroupingExpr(ast::GroupingExpr *expr)
    {
        expr->expression->accept(*this);
    }

    void TypeChecker::visitLiteralExpr(ast::LiteralExpr *expr)
    {
        switch (expr->literalType)
        {
        case ast::LiteralExpr::LiteralType::INTEGER:
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::INT, "int", "", 0, 0));
            break;
        case ast::LiteralExpr::LiteralType::FLOAT:
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::FLOAT64, "float", "", 0, 0));
            break;
        case ast::LiteralExpr::LiteralType::BOOLEAN:
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0));
            break;
        case ast::LiteralExpr::LiteralType::STRING:
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::STRING, "string", "", 0, 0));
            break;
        case ast::LiteralExpr::LiteralType::NIL:
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::NIL, "None", "", 0, 0));
            break;
        }
    }

    void TypeChecker::visitUnaryExpr(ast::UnaryExpr *expr)
    {
        expr->right->accept(*this);
        auto rightType = currentType_;
        if (!rightType)
        {
            currentType_ = nullptr;
            return;
        }

        if (expr->op.value == "-")
        {
            if (isAssignable(rightType, std::make_shared<ast::SimpleType>(
                                            lexer::Token(lexer::TokenType::INT, "int", "", 0, 0))))
            {
                currentType_ = rightType;
            }
            else
            {
                errorHandler_.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                          "Unary minus requires a number", std::string(expr->token.filename),
                                          expr->token.line, expr->token.column,
                                          error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
            }
        }
        else if (expr->op.value == "!")
        {
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0));
        }
        else
        {
            errorHandler_.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                      "Invalid unary operator: " + expr->op.value,
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
        }
    }

    void TypeChecker::visitVariableExpr(ast::VariableExpr *expr)
    {
        currentType_ = environment_->lookup(expr->name);

        if (!currentType_)
        {
            errorHandler_.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                      "Undefined variable: " + expr->name,
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);

            // Set to an "error" type to continue type checking
            currentType_ = std::make_shared<ast::SimpleType>(expr->token);
        }
    }

    void TypeChecker::visitAssignExpr(ast::AssignExpr *expr)
    {
        // Type check the value being assigned
        expr->value->accept(*this);
        ast::TypePtr valueType = currentType_;

        // Look up the variable type
        ast::TypePtr varType = environment_->lookup(expr->name);

        if (!varType)
        {
            errorHandler_.reportError(error::ErrorCode::T002_UNDEFINED_VARIABLE,
                                      "Undefined variable in assignment: " + expr->name,
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);

            // Continue with the value's type
            currentType_ = valueType;
            return;
        }

        // Check if the types are compatible
        if (!isAssignable(valueType, varType))
        {
            errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                      "Cannot assign " + valueType->toString() +
                                          " to variable of type " + varType->toString(),
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
        }

        currentType_ = varType;
    }

    void TypeChecker::visitCallExpr(ast::CallExpr *expr)
    {
        expr->callee->accept(*this);
        auto calleeType = currentType_;
        if (!calleeType)
        {
            currentType_ = nullptr;
            return;
        }

        auto funcType = std::dynamic_pointer_cast<ast::FunctionType>(calleeType);
        if (!funcType)
        {
            errorHandler_.reportError(error::ErrorCode::T003_UNDEFINED_FUNCTION,
                                      "Callee is not a function", std::string(expr->token.filename),
                                      expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        if (funcType->paramTypes.size() != expr->arguments.size())
        {
            errorHandler_.reportError(error::ErrorCode::T007_INCORRECT_ARGUMENT_COUNT,
                                      "Incorrect number of arguments", std::string(expr->token.filename),
                                      expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        for (size_t i = 0; i < expr->arguments.size(); ++i)
        {
            expr->arguments[i]->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, funcType->paramTypes[i]))
            {
                errorHandler_.reportError(error::ErrorCode::T008_INCORRECT_ARGUMENT_TYPE,
                                          "Argument type mismatch", std::string(expr->token.filename),
                                          expr->token.line, expr->token.column,
                                          error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
        }

        currentType_ = funcType->returnType;
    }

    void TypeChecker::visitGetExpr(ast::GetExpr *expr)
    {
        expr->object->accept(*this);
        // Simplified: Assume object is a class type with fields
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0));
    }

    void TypeChecker::visitSetExpr(ast::SetExpr *expr)
    {
        expr->value->accept(*this);
        auto valueType = currentType_;
        expr->object->accept(*this);
        if (!valueType)
        {
            currentType_ = nullptr;
            return;
        }
        currentType_ = valueType;
    }

    void TypeChecker::visitListExpr(ast::ListExpr *expr)
    {
        if (expr->elements.empty())
        {
            currentType_ = std::make_shared<ast::GenericType>(
                lexer::Token(lexer::TokenType::IDENTIFIER, "list", "", 0, 0), "list",
                std::vector<ast::TypePtr>{std::make_shared<ast::SimpleType>(
                    lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0))});
            return;
        }

        expr->elements[0]->accept(*this);
        auto elementType = currentType_;
        if (!elementType)
        {
            currentType_ = nullptr;
            return;
        }

        for (size_t i = 1; i < expr->elements.size(); ++i)
        {
            expr->elements[i]->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, elementType))
            {
                errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                          "Inconsistent list element types", std::string(expr->token.filename),
                                          expr->token.line, expr->token.column,
                                          error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
        }

        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "list", std::vector<ast::TypePtr>{elementType});
    }

    void TypeChecker::visitDictionaryExpr(ast::DictionaryExpr *expr)
    {
        if (expr->entries.empty())
        {
            currentType_ = std::make_shared<ast::GenericType>(
                lexer::Token(lexer::TokenType::IDENTIFIER, "dict", "", 0, 0), "dict",
                std::vector<ast::TypePtr>{
                    std::make_shared<ast::SimpleType>(
                        lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0)),
                    std::make_shared<ast::SimpleType>(
                        lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0))});
            return;
        }

        expr->entries[0].first->accept(*this);
        auto keyType = currentType_;
        expr->entries[0].second->accept(*this);
        auto valueType = currentType_;
        if (!keyType || !valueType)
        {
            currentType_ = nullptr;
            return;
        }

        for (size_t i = 1; i < expr->entries.size(); ++i)
        {
            expr->entries[i].first->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, keyType))
            {
                errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                          "Inconsistent dictionary key types", std::string(expr->token.filename),
                                          expr->token.line, expr->token.column,
                                          error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
            expr->entries[i].second->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, valueType))
            {
                errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                          "Inconsistent dictionary value types", std::string(expr->token.filename),
                                          expr->token.line, expr->token.column,
                                          error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
        }

        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "dict", std::vector<ast::TypePtr>{keyType, valueType});
    }

    void TypeChecker::visitLambdaExpr(ast::LambdaExpr *expr)
    {
        pushScope();
        for (const auto &param : expr->parameters)
        {
            environment_->define(param.name, param.type, false);
        }
        expr->body->accept(*this);
        auto bodyType = currentType_;
        popScope();

        if (!bodyType || !isAssignable(bodyType, expr->returnType))
        {
            errorHandler_.reportError(error::ErrorCode::T010_RETURN_TYPE_MISMATCH,
                                      "Lambda body type does not match return type",
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        currentType_ = std::make_shared<ast::FunctionType>(
            expr->token, std::vector<ast::TypePtr>{}, expr->returnType);
    }

    void TypeChecker::visitAwaitExpr(ast::AwaitExpr *expr)
    {
        if (!inAsyncContext_)
        {
            errorHandler_.reportError(error::ErrorCode::M003_INVALID_RETURN,
                                      "Await expression outside async function",
                                      std::string(expr->token.filename), expr->token.line, expr->token.column,
                                      error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        expr->expression->accept(*this);
        if (!currentType_)
        {
            currentType_ = nullptr;
            return;
        }

        // Simplified: Assume await unwraps to the inner type
        currentType_ = currentType_;
    }

    void TypeChecker::visitExpressionStmt(ast::ExpressionStmt *stmt)
    {
        stmt->expression->accept(*this);
    }

    void TypeChecker::visitVariableStmt(ast::VariableStmt *stmt)
    {
        if (stmt->initializer)
        {
            stmt->initializer->accept(*this);
            auto initType = currentType_;
            if (!initType || (stmt->type && !isAssignable(initType, stmt->type)))
            {
                errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                          "Initializer type does not match declared type",
                                          std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                                          error::ErrorSeverity::ERROR);
                return;
            }
            environment_->define(stmt->name, stmt->type ? stmt->type : initType, stmt->isConstant);
        }
        else if (stmt->type)
        {
            environment_->define(stmt->name, stmt->type, stmt->isConstant);
        }
        else
        {
            errorHandler_.reportError(error::ErrorCode::T009_CANNOT_INFER_TYPE,
                                      "Variable declaration requires type or initializer",
                                      std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                                      error::ErrorSeverity::ERROR);
        }
    }

    void TypeChecker::visitBlockStmt(ast::BlockStmt *stmt)
    {
        pushScope();
        for (const auto &s : stmt->statements)
        {
            check(s);
        }
        popScope();
    }

    void TypeChecker::visitIfStmt(ast::IfStmt *stmt)
    {
        stmt->condition->accept(*this);
        if (!currentType_ || !isAssignable(currentType_, std::make_shared<ast::SimpleType>(
                                                             lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0))))
        {
            errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                      "If condition must be boolean", std::string(stmt->token.filename),
                                      stmt->token.line, stmt->token.column,
                                      error::ErrorSeverity::ERROR);
        }
        check(stmt->thenBranch);
        for (const auto &elif : stmt->elifBranches)
        {
            elif.first->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, std::make_shared<ast::SimpleType>(
                                                                 lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0))))
            {
                errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                          "Elif condition must be boolean", std::string(stmt->token.filename),
                                          stmt->token.line, stmt->token.column,
                                          error::ErrorSeverity::ERROR);
            }
            check(elif.second);
        }
        if (stmt->elseBranch)
        {
            check(stmt->elseBranch);
        }
    }

    void TypeChecker::visitWhileStmt(ast::WhileStmt *stmt)
    {
        stmt->condition->accept(*this);
        if (!currentType_ || !isAssignable(currentType_, std::make_shared<ast::SimpleType>(
                                                             lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0))))
        {
            errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                      "While condition must be boolean", std::string(stmt->token.filename),
                                      stmt->token.line, stmt->token.column,
                                      error::ErrorSeverity::ERROR);
        }
        check(stmt->body);
    }

    void TypeChecker::visitForStmt(ast::ForStmt *stmt)
    {
        stmt->iterable->accept(*this);
        auto iterableType = currentType_;
        if (!iterableType)
        {
            errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                      "Invalid iterable type", std::string(stmt->token.filename),
                                      stmt->token.line, stmt->token.column,
                                      error::ErrorSeverity::ERROR);
            return;
        }

        pushScope();
        environment_->define(stmt->variable, stmt->variableType ? stmt->variableType : std::make_shared<ast::SimpleType>(lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0)), false);
        check(stmt->body);
        popScope();
    }

    void TypeChecker::visitFunctionStmt(ast::FunctionStmt *stmt)
    {
        std::vector<ast::TypePtr> paramTypes;
        for (const auto &param : stmt->parameters)
        {
            paramTypes.push_back(param.type);
        }
        auto funcType = std::make_shared<ast::FunctionType>(
            stmt->token, paramTypes, stmt->returnType);

        environment_->define(stmt->name, funcType, true);

        pushScope();
        inAsyncContext_ = stmt->isAsync;
        expectedReturnType_ = stmt->returnType;
        for (const auto &param : stmt->parameters)
        {
            environment_->define(param.name, param.type, false);
        }
        check(stmt->body);
        inAsyncContext_ = false;
        expectedReturnType_ = nullptr;
        popScope();
    }

    void TypeChecker::visitReturnStmt(ast::ReturnStmt *stmt)
    {
        if (!expectedReturnType_)
        {
            errorHandler_.reportError(error::ErrorCode::M003_INVALID_RETURN,
                                      "Return statement outside function",
                                      std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                                      error::ErrorSeverity::ERROR);
            return;
        }
        if (stmt->value)
        {
            stmt->value->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, expectedReturnType_))
            {
                errorHandler_.reportError(error::ErrorCode::T010_RETURN_TYPE_MISMATCH,
                                          "Return type does not match function signature",
                                          std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                                          error::ErrorSeverity::ERROR);
            }
        }
        else if (!isAssignable(std::make_shared<ast::SimpleType>(
                                   lexer::Token(lexer::TokenType::NIL, "None", "", 0, 0)),
                               expectedReturnType_))
        {
            errorHandler_.reportError(error::ErrorCode::T010_RETURN_TYPE_MISMATCH,
                                      "Missing return value", std::string(stmt->token.filename),
                                      stmt->token.line, stmt->token.column,
                                      error::ErrorSeverity::ERROR);
        }
    }

    void TypeChecker::visitClassStmt(ast::ClassStmt *stmt)
    {
        auto classType = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, stmt->name, "", 0, 0));
        environment_->define(stmt->name, classType, true);

        pushScope();
        for (const auto &field : stmt->fields)
        {
            check(field);
        }
        for (const auto &method : stmt->methods)
        {
            check(method);
        }
        popScope();
    }

    void TypeChecker::visitImportStmt(ast::ImportStmt *stmt)
    {
        if (stmt->importAll) {
            // Import entire module
            if (!loadModule(stmt->moduleName)) {
                return;
            }
            
            if (stmt->moduleAlias.empty()) {
                // Import all symbols from the module
                compilationContext_.importAllSymbols(stmt->moduleName);
            } else {
                // Create a namespace for the module
                // TODO: Implement namespace support
            }
        } else {
            // Import specific symbols
            for (const auto& [symbol, alias] : stmt->symbols) {
                importSymbol(stmt->moduleName, symbol, alias);
            }
        }
    }

    void TypeChecker::visitMatchStmt(ast::MatchStmt *stmt)
    {
        stmt->value->accept(*this);
        auto valueType = currentType_;
        if (!valueType)
        {
            return;
        }

        for (const auto &case_ : stmt->cases)
        {
            case_.first->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, valueType))
            {
                errorHandler_.reportError(error::ErrorCode::T001_TYPE_MISMATCH,
                                          "Case pattern type does not match match value",
                                          std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
                                          error::ErrorSeverity::ERROR);
            }
            check(case_.second);
        }
        if (stmt->defaultCase)
        {
            check(stmt->defaultCase);
        }
    }

    void TypeChecker::visitNewExpr(ast::NewExpr *expr)
    {
        // Check the type expression
        expr->getTypeExpr()->accept(*this);
        ast::TypePtr typeExpr = currentType_;

        // If it's an array allocation, check the size expression
        if (expr->getSizeExpr())
        {
            expr->getSizeExpr()->accept(*this);
            ast::TypePtr sizeType = currentType_;

            // Size must be an integer
            if (!sizeType || sizeType->toString() != "int")
            {
                errorHandler_.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                          "Array size must be an integer, got " + (sizeType ? sizeType->toString() : "unknown"),
                                          std::string(expr->getKeyword().filename), expr->getKeyword().line, expr->getKeyword().column,
                                          error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }

            // For array allocation, the type is an array of the specified type
            currentType_ = std::make_shared<ast::GenericType>(expr->getKeyword(), "Array", std::vector<ast::TypePtr>{typeExpr});
        }
        else
        {
            // For non-array allocation, the type is the same as the type expression
            currentType_ = typeExpr;
        }
    }

    void TypeChecker::visitDeleteExpr(ast::DeleteExpr *expr)
    {
        // Check the expression to be deleted
        expr->getExpr()->accept(*this);
        ast::TypePtr exprType = currentType_;

        // The expression must be a pointer or array type
        if (auto generic = std::dynamic_pointer_cast<ast::GenericType>(exprType))
        {
            if (generic->name != "Array" && generic->name != "Ptr")
            {
                errorHandler_.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                          "Can only delete array or pointer types, got " + exprType->toString(),
                                          std::string(expr->getKeyword().filename), expr->getKeyword().line, expr->getKeyword().column,
                                          error::ErrorSeverity::ERROR);
            }
        }
        else
        {
            errorHandler_.reportError(error::ErrorCode::T006_INVALID_OPERATOR_FOR_TYPE,
                                      "Can only delete array or pointer types, got " + (exprType ? exprType->toString() : "unknown"),
                                      std::string(expr->getKeyword().filename), expr->getKeyword().line, expr->getKeyword().column,
                                      error::ErrorSeverity::ERROR);
        }

        // Delete expression has no type
        currentType_ = nullptr;
    }

    void TypeChecker::registerBuiltins()
    {
        // Register built-in types
        auto intType = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "int", "", 0, 0));
        intType->toString() = "int";

        auto floatType = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "float", "", 0, 0));
        floatType->toString() = "float";

        auto boolType = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "bool", "", 0, 0));
        boolType->toString() = "bool";

        auto stringType = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "string", "", 0, 0));
        stringType->toString() = "string";

        auto voidType = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));
        voidType->toString() = "void";

        // Register built-in functions
        // Example: print(string) -> void
        std::vector<ast::Parameter> printParams = {
            ast::Parameter("text", stringType)};
        auto printFuncType = std::make_shared<ast::FunctionType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "print", "", 0, 0),
            std::vector<ast::TypePtr>{stringType}, voidType);

        environment_->define("print", printFuncType, true);
    }

    bool TypeChecker::loadModule(const std::string& moduleName) {
        // Check if the module exists in the compilation context
        if (!compilationContext_.moduleExists(moduleName)) {
            // Try to load it
            auto moduleInfo = compilationContext_.loadModule(moduleName);
            if (!moduleInfo) {
                errorHandler_.reportError(
                    error::ErrorCode::M006_MODULE_NOT_FOUND,
                    "Module '" + moduleName + "' not found",
                    error::ErrorSeverity::ERROR
                );
                return false;
            }
            
            // Check for circular imports
            if (checkCircularImports(moduleName)) {
                errorHandler_.reportError(
                    error::ErrorCode::M007_CIRCULAR_DEPENDENCY,
                    "Circular dependency detected when importing module '" + moduleName + "'",
                    error::ErrorSeverity::ERROR
                );
                return false;
            }
        }
        
        return true;
    }

    bool TypeChecker::importSymbol(const std::string& moduleName, const std::string& symbolName, 
                                  const std::string& alias) {
        // Load the module if needed
        if (!loadModule(moduleName)) {
            return false;
        }
        
        // Check if the symbol is exported by the module
        if (!compilationContext_.importSymbol(moduleName, symbolName)) {
            errorHandler_.reportError(
                error::ErrorCode::T002_UNDEFINED_VARIABLE,
                "Symbol '" + symbolName + "' not exported by module '" + moduleName + "'",
                error::ErrorSeverity::ERROR
            );
            return false;
        }
        
        // Add the symbol to the current environment
        std::string localName = alias.empty() ? symbolName : alias;
        
        // TODO: Get the actual type of the imported symbol
        ast::TypePtr symbolType = std::make_shared<ast::SimpleType>(lexer::Token());
        
        environment_->define(localName, symbolType, true);
        return true;
    }

    void TypeChecker::setCurrentModule(const std::string& moduleName) {
        currentModuleName_ = moduleName;
        environment_->setModule(moduleName);
    }

    std::string TypeChecker::getCurrentModule() const {
        return currentModuleName_;
    }

    void TypeChecker::addExport(const std::string& name) {
        // Add the symbol to the set of exported symbols
        environment_->addExport(name);
        
        // Register it in the compilation context
        compilationContext_.addGlobalSymbol(name, true);
    }

    bool TypeChecker::checkCircularImports(const std::string& moduleName) {
        std::vector<std::string> path;
        return compilationContext_.hasCircularDependency(moduleName, path);
    }

    // Export statement visitor
    void TypeChecker::visitExportStmt(ast::ExportStmt* stmt) {
        if (stmt->exportAll) {
            // Export all declarations in the current scope
            // This is handled when declarations are processed
            return;
        }
        
        if (stmt->declaration) {
            // Export a specific declaration
            // First type check the declaration
            stmt->declaration->accept(*this);
            
            // Then mark it as exported
            // The actual symbol name depends on the declaration type
            if (auto* varStmt = dynamic_cast<ast::VariableStmt*>(stmt->declaration.get())) {
                addExport(varStmt->name);
            } else if (auto* funcStmt = dynamic_cast<ast::FunctionStmt*>(stmt->declaration.get())) {
                addExport(funcStmt->name);
            } else if (auto* classStmt = dynamic_cast<ast::ClassStmt*>(stmt->declaration.get())) {
                addExport(classStmt->name);
            } else {
                errorHandler_.reportError(
                    error::ErrorCode::G004_GENERAL_SEMANTIC_ERROR,
                    "Cannot export this type of declaration",
                    error::ErrorSeverity::ERROR
                );
            }
        } else {
            // Export specific symbols
            for (const auto& symbol : stmt->symbols) {
                // Check if the symbol exists
                if (environment_->lookup(symbol) == nullptr) {
                    errorHandler_.reportError(
                        error::ErrorCode::T002_UNDEFINED_VARIABLE,
                        "Exported symbol '" + symbol + "' is not defined",
                        error::ErrorSeverity::ERROR
                    );
                    continue;
                }
                
                addExport(symbol);
            }
        }
    }

    // Module statement visitor
    void TypeChecker::visitModuleStmt(ast::ModuleStmt* stmt) {
        // Set the current module name
        setCurrentModule(stmt->name);
        
        // Register the module in the compilation context
        if (!compilationContext_.moduleExists(stmt->name)) {
            auto moduleInfo = std::make_shared<compiler::ModuleInfo>(stmt->name, "");
            compilationContext_.addModule(stmt->name, moduleInfo);
        }
        
        // Create a new scope for the module
        pushScope();
        
        // Process the module body
        for (const auto& statement : stmt->body) {
            statement->accept(*this);
        }
        
        // Record exported symbols
        auto exportedSymbols = environment_->getExportedSymbols();
        auto moduleInfo = compilationContext_.getModule(stmt->name);
        
        if (moduleInfo) {
            for (const auto& symbol : exportedSymbols) {
                // Determine the type of symbol (function, class, variable, type)
                // This is a simplification - we should really track what kind of thing each symbol is
                moduleInfo->exportedFunctions.insert(symbol);
            }
        }
        
        // Close the module scope
        popScope();
        
        // Reset the current module name
        setCurrentModule("");
    }
} // namespace type_checker

