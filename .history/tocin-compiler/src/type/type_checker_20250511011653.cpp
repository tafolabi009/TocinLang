#include "type_checker.h"
#include <stdexcept>

namespace type_checker {

    void Environment::define(const std::string& name, ast::TypePtr type, bool isConstant) {
        if (!type) {
            throw std::runtime_error("Cannot define variable with null type");
        }
        variables_[name] = { type, isConstant };
    }

    ast::TypePtr Environment::lookup(const std::string& name) const {
        auto it = variables_.find(name);
        if (it != variables_.end()) {
            return it->second.first;
        }
        if (parent_) {
            return parent_->lookup(name);
        }
        return nullptr;
    }

    bool Environment::assign(const std::string& name, ast::TypePtr type) {
        auto it = variables_.find(name);
        if (it != variables_.end()) {
            if (it->second.second) {
                return false; // Cannot assign to constant
            }
            it->second.first = type;
            return true;
        }
        if (parent_) {
            return parent_->assign(name, type);
        }
        return false;
    }

    TypeChecker::TypeChecker(error::ErrorHandler& errorHandler)
        : errorHandler_(errorHandler), environment_(std::make_shared<Environment>()),
        globalEnv_(environment_) {}

    ast::TypePtr TypeChecker::check(ast::StmtPtr stmt) {
        if (!stmt) {
            errorHandler_.reportError(error::ErrorCode::T009_CANNOT_INFER_TYPE,
                                    "Cannot type check null statement",
                                    "", 0, 0, error::ErrorSeverity::ERROR);
            return nullptr;
        }
        
        try {
            // Create a global environment if not already created
            if (!globalEnv_) {
                globalEnv_ = std::make_shared<Environment>();
                environment_ = globalEnv_;
                
                // Register built-in types and functions
                registerBuiltins();
            }
            
            // Visit the statement
            stmt->accept(*this);
            
            return currentType_;
        } catch (const std::exception& e) {
            errorHandler_.reportError(error::ErrorCode::C003_TYPECHECK_ERROR,
                                    "Type checking error: " + std::string(e.what()),
                                    "", 0, 0, error::ErrorSeverity::FATAL);
            return nullptr;
        }
    }

    void TypeChecker::pushScope() {
        environment_ = std::make_shared<Environment>(environment_);
    }

    void TypeChecker::popScope() {
        environment_ = environment_->parent_;
    }

    bool TypeChecker::isAssignable(ast::TypePtr from, ast::TypePtr to) {
        if (!from || !to) return false;
        if (from == to) return true;

        if (auto fromUnion = std::dynamic_pointer_cast<ast::UnionType>(from)) {
            for (const auto& t : fromUnion->types) {
                if (!isAssignable(t, to)) return false;
            }
            return true;
        }
        if (auto toUnion = std::dynamic_pointer_cast<ast::UnionType>(to)) {
            for (const auto& t : toUnion->types) {
                if (isAssignable(from, t)) return true;
            }
            return false;
        }

        auto fromSimple = std::dynamic_pointer_cast<ast::SimpleType>(from);
        auto toSimple = std::dynamic_pointer_cast<ast::SimpleType>(to);
        if (fromSimple && toSimple) {
            return fromSimple->token.value == toSimple->token.value ||
                (fromSimple->token.value == "int" && toSimple->token.value == "float");
        }
        return false;
    }

    ast::TypePtr TypeChecker::resolveType(ast::TypePtr type) {
        if (!type) return nullptr;
        if (auto simple = std::dynamic_pointer_cast<ast::SimpleType>(type)) {
            return type;
        }
        if (auto generic = std::dynamic_pointer_cast<ast::GenericType>(type)) {
            std::vector<ast::TypePtr> resolvedArgs;
            for (const auto& arg : generic->typeArguments) {
                resolvedArgs.push_back(resolveType(arg));
            }
            return std::make_shared<ast::GenericType>(generic->token, generic->name, resolvedArgs);
        }
        if (auto func = std::dynamic_pointer_cast<ast::FunctionType>(type)) {
            std::vector<ast::TypePtr> resolvedParams;
            for (const auto& param : func->paramTypes) {
                resolvedParams.push_back(resolveType(param));
            }
            return std::make_shared<ast::FunctionType>(
                func->token, resolvedParams, resolveType(func->returnType));
        }
        if (auto unionType = std::dynamic_pointer_cast<ast::UnionType>(type)) {
            std::vector<ast::TypePtr> resolvedTypes;
            for (const auto& t : unionType->types) {
                resolvedTypes.push_back(resolveType(t));
            }
            return std::make_shared<ast::UnionType>(unionType->token, resolvedTypes);
        }
        return type;
    }

    void TypeChecker::visitBinaryExpr(ast::BinaryExpr* expr) {
        expr->left->accept(*this);
        auto leftType = currentType_;
        expr->right->accept(*this);
        auto rightType = currentType_;

        if (!leftType || !rightType) {
            currentType_ = nullptr;
            return;
        }

        std::string op = expr->op.value;
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
            if (isAssignable(leftType, std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::INT, "int", "", 0, 0))) &&
                isAssignable(rightType, std::make_shared<ast::SimpleType>(
                    lexer::Token(lexer::TokenType::INT, "int", "", 0, 0)))) {
                currentType_ = leftType;
            }
            else {
                errorHandler_.reportError("Operands must be numbers", expr->token.filename,
                    expr->token.line, expr->token.column,
                    error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
            }
        }
        else if (op == "==" || op == "!=") {
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0));
        }
        else {
            errorHandler_.reportError("Invalid binary operator: " + op, expr->token.filename,
                expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
        }
    }

    void TypeChecker::visitGroupingExpr(ast::GroupingExpr* expr) {
        expr->expression->accept(*this);
    }

    void TypeChecker::visitLiteralExpr(ast::LiteralExpr* expr) {
        switch (expr->literalType) {
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

    void TypeChecker::visitUnaryExpr(ast::UnaryExpr* expr) {
        expr->right->accept(*this);
        auto rightType = currentType_;
        if (!rightType) {
            currentType_ = nullptr;
            return;
        }

        if (expr->op.value == "-") {
            if (isAssignable(rightType, std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::INT, "int", "", 0, 0)))) {
                currentType_ = rightType;
            }
            else {
                errorHandler_.reportError("Unary minus requires a number", expr->token.filename,
                    expr->token.line, expr->token.column,
                    error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
            }
        }
        else if (expr->op.value == "!") {
            currentType_ = std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0));
        }
        else {
            errorHandler_.reportError("Invalid unary operator: " + expr->op.value,
                expr->token.filename, expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
        }
    }

    void TypeChecker::visitVariableExpr(ast::VariableExpr* expr) {
        currentType_ = environment_->lookup(expr->name);
        if (!currentType_) {
            errorHandler_.reportError("Undefined variable: " + expr->name, expr->token.filename,
                expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
        }
    }

    void TypeChecker::visitAssignExpr(ast::AssignExpr* expr) {
        expr->value->accept(*this);
        auto valueType = currentType_;
        if (!valueType) {
            currentType_ = nullptr;
            return;
        }

        auto varType = environment_->lookup(expr->name);
        if (!varType) {
            errorHandler_.reportError("Undefined variable: " + expr->name, expr->token.filename,
                expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        if (!environment_->assign(expr->name, valueType)) {
            errorHandler_.reportError("Cannot assign to constant: " + expr->name,
                expr->token.filename, expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        if (!isAssignable(valueType, varType)) {
            errorHandler_.reportError("Type mismatch in assignment", expr->token.filename,
                expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }
        currentType_ = valueType;
    }

    void TypeChecker::visitCallExpr(ast::CallExpr* expr) {
        expr->callee->accept(*this);
        auto calleeType = currentType_;
        if (!calleeType) {
            currentType_ = nullptr;
            return;
        }

        auto funcType = std::dynamic_pointer_cast<ast::FunctionType>(calleeType);
        if (!funcType) {
            errorHandler_.reportError("Callee is not a function", expr->token.filename,
                expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        if (funcType->paramTypes.size() != expr->arguments.size()) {
            errorHandler_.reportError("Incorrect number of arguments", expr->token.filename,
                expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        for (size_t i = 0; i < expr->arguments.size(); ++i) {
            expr->arguments[i]->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, funcType->paramTypes[i])) {
                errorHandler_.reportError("Argument type mismatch", expr->token.filename,
                    expr->token.line, expr->token.column,
                    error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
        }

        currentType_ = funcType->returnType;
    }

    void TypeChecker::visitGetExpr(ast::GetExpr* expr) {
        expr->object->accept(*this);
        // Simplified: Assume object is a class type with fields
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0));
    }

    void TypeChecker::visitSetExpr(ast::SetExpr* expr) {
        expr->value->accept(*this);
        auto valueType = currentType_;
        expr->object->accept(*this);
        if (!valueType) {
            currentType_ = nullptr;
            return;
        }
        currentType_ = valueType;
    }

    void TypeChecker::visitListExpr(ast::ListExpr* expr) {
        if (expr->elements.empty()) {
            currentType_ = std::make_shared<ast::GenericType>(
                lexer::Token(lexer::TokenType::IDENTIFIER, "list", "", 0, 0), "list",
                std::vector<ast::TypePtr>{std::make_shared<ast::SimpleType>(
                    lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0))});
            return;
        }

        expr->elements[0]->accept(*this);
        auto elementType = currentType_;
        if (!elementType) {
            currentType_ = nullptr;
            return;
        }

        for (size_t i = 1; i < expr->elements.size(); ++i) {
            expr->elements[i]->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, elementType)) {
                errorHandler_.reportError("Inconsistent list element types", expr->token.filename,
                    expr->token.line, expr->token.column,
                    error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
        }

        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "list", std::vector<ast::TypePtr>{elementType});
    }

    void TypeChecker::visitDictionaryExpr(ast::DictionaryExpr* expr) {
        if (expr->entries.empty()) {
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
        if (!keyType || !valueType) {
            currentType_ = nullptr;
            return;
        }

        for (size_t i = 1; i < expr->entries.size(); ++i) {
            expr->entries[i].first->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, keyType)) {
                errorHandler_.reportError("Inconsistent dictionary key types", expr->token.filename,
                    expr->token.line, expr->token.column,
                    error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
            expr->entries[i].second->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, valueType)) {
                errorHandler_.reportError("Inconsistent dictionary value types", expr->token.filename,
                    expr->token.line, expr->token.column,
                    error::ErrorSeverity::ERROR);
                currentType_ = nullptr;
                return;
            }
        }

        currentType_ = std::make_shared<ast::GenericType>(
            expr->token, "dict", std::vector<ast::TypePtr>{keyType, valueType});
    }

    void TypeChecker::visitLambdaExpr(ast::LambdaExpr* expr) {
        pushScope();
        for (const auto& param : expr->parameters) {
            environment_->define(param.name, param.type, false);
        }
        expr->body->accept(*this);
        auto bodyType = currentType_;
        popScope();

        if (!bodyType || !isAssignable(bodyType, expr->returnType)) {
            errorHandler_.reportError("Lambda body type does not match return type",
                expr->token.filename, expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        currentType_ = std::make_shared<ast::FunctionType>(
            expr->token, std::vector<ast::TypePtr>{}, expr->returnType);
    }

    void TypeChecker::visitAwaitExpr(ast::AwaitExpr* expr) {
        if (!inAsyncContext_) {
            errorHandler_.reportError("Await expression outside async function",
                expr->token.filename, expr->token.line, expr->token.column,
                error::ErrorSeverity::ERROR);
            currentType_ = nullptr;
            return;
        }

        expr->expression->accept(*this);
        if (!currentType_) {
            currentType_ = nullptr;
            return;
        }

        // Simplified: Assume await unwraps to the inner type
        currentType_ = currentType_;
    }

    void TypeChecker::visitExpressionStmt(ast::ExpressionStmt* stmt) {
        stmt->expression->accept(*this);
    }

    void TypeChecker::visitVariableStmt(ast::VariableStmt* stmt) {
        if (stmt->initializer) {
            stmt->initializer->accept(*this);
            auto initType = currentType_;
            if (!initType || (stmt->type && !isAssignable(initType, stmt->type))) {
                errorHandler_.reportError("Initializer type does not match declared type",
                    stmt->token.filename, stmt->token.line, stmt->token.column,
                    error::ErrorSeverity::ERROR);
                return;
            }
            environment_->define(stmt->name, stmt->type ? stmt->type : initType, stmt->isConstant);
        }
        else if (stmt->type) {
            environment_->define(stmt->name, stmt->type, stmt->isConstant);
        }
        else {
            errorHandler_.reportError("Variable declaration requires type or initializer",
                stmt->token.filename, stmt->token.line, stmt->token.column,
                error::ErrorSeverity::ERROR);
        }
    }

    void TypeChecker::visitBlockStmt(ast::BlockStmt* stmt) {
        pushScope();
        for (const auto& s : stmt->statements) {
            check(s);
        }
        popScope();
    }

    void TypeChecker::visitIfStmt(ast::IfStmt* stmt) {
        stmt->condition->accept(*this);
        if (!currentType_ || !isAssignable(currentType_, std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0)))) {
            errorHandler_.reportError("If condition must be boolean", stmt->token.filename,
                stmt->token.line, stmt->token.column,
                error::ErrorSeverity::ERROR);
        }
        check(stmt->thenBranch);
        for (const auto& elif : stmt->elifBranches) {
            elif.first->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0)))) {
                errorHandler_.reportError("Elif condition must be boolean", stmt->token.filename,
                    stmt->token.line, stmt->token.column,
                    error::ErrorSeverity::ERROR);
            }
            check(elif.second);
        }
        if (stmt->elseBranch) {
            check(stmt->elseBranch);
        }
    }

    void TypeChecker::visitWhileStmt(ast::WhileStmt* stmt) {
        stmt->condition->accept(*this);
        if (!currentType_ || !isAssignable(currentType_, std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::TRUE, "bool", "", 0, 0)))) {
            errorHandler_.reportError("While condition must be boolean", stmt->token.filename,
                stmt->token.line, stmt->token.column,
                error::ErrorSeverity::ERROR);
        }
        check(stmt->body);
    }

    void TypeChecker::visitForStmt(ast::ForStmt* stmt) {
        stmt->iterable->accept(*this);
        auto iterableType = currentType_;
        if (!iterableType) {
            errorHandler_.reportError("Invalid iterable type", stmt->token.filename,
                stmt->token.line, stmt->token.column,
                error::ErrorSeverity::ERROR);
            return;
        }

        pushScope();
        environment_->define(stmt->variable, stmt->variableType ? stmt->variableType :
            std::make_shared<ast::SimpleType>(
                lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0)), false);
        check(stmt->body);
        popScope();
    }

    void TypeChecker::visitFunctionStmt(ast::FunctionStmt* stmt) {
        std::vector<ast::TypePtr> paramTypes;
        for (const auto& param : stmt->parameters) {
            paramTypes.push_back(param.type);
        }
        auto funcType = std::make_shared<ast::FunctionType>(
            stmt->token, paramTypes, stmt->returnType);

        environment_->define(stmt->name, funcType, true);

        pushScope();
        inAsyncContext_ = stmt->isAsync;
        expectedReturnType_ = stmt->returnType;
        for (const auto& param : stmt->parameters) {
            environment_->define(param.name, param.type, false);
        }
        check(stmt->body);
        inAsyncContext_ = false;
        expectedReturnType_ = nullptr;
        popScope();
    }

    void TypeChecker::visitReturnStmt(ast::ReturnStmt* stmt) {
        if (!expectedReturnType_) {
            errorHandler_.reportError("Return statement outside function",
                stmt->token.filename, stmt->token.line, stmt->token.column,
                error::ErrorSeverity::ERROR);
            return;
        }
        if (stmt->value) {
            stmt->value->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, expectedReturnType_)) {
                errorHandler_.reportError("Return type does not match function signature",
                    stmt->token.filename, stmt->token.line, stmt->token.column,
                    error::ErrorSeverity::ERROR);
            }
        }
        else if (!isAssignable(std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::NIL, "None", "", 0, 0)), expectedReturnType_)) {
            errorHandler_.reportError("Missing return value", stmt->token.filename,
                stmt->token.line, stmt->token.column,
                error::ErrorSeverity::ERROR);
        }
    }

    void TypeChecker::visitClassStmt(ast::ClassStmt* stmt) {
        auto classType = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, stmt->name, "", 0, 0));
        environment_->define(stmt->name, classType, true);

        pushScope();
        for (const auto& field : stmt->fields) {
            check(field);
        }
        for (const auto& method : stmt->methods) {
            check(method);
        }
        popScope();
    }

    void TypeChecker::visitImportStmt(ast::ImportStmt* stmt) {
        // Simplified: Assume module type is resolved externally
        currentType_ = nullptr;
    }

    void TypeChecker::visitMatchStmt(ast::MatchStmt* stmt) {
        stmt->value->accept(*this);
        auto valueType = currentType_;
        if (!valueType) {
            return;
        }

        for (const auto& case_ : stmt->cases) {
            case_.first->accept(*this);
            if (!currentType_ || !isAssignable(currentType_, valueType)) {
                errorHandler_.reportError("Case pattern type does not match match value",
                    stmt->token.filename, stmt->token.line, stmt->token.column,
                    error::ErrorSeverity::ERROR);
            }
            check(case_.second);
        }
        if (stmt->defaultCase) {
            check(stmt->defaultCase);
        }
    }

} // namespace type_checker
