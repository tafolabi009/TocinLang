#pragma once

#include "../ast/types.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "ownership.h"
#include <memory>
#include <string>

namespace type_checker {

/**
 * @brief Represents an rvalue reference type (T&&)
 */
class RValueReference : public ast::Type {
public:
    ast::TypePtr baseType;

    RValueReference(const lexer::Token& token, ast::TypePtr baseType)
        : ast::Type(token), baseType(std::move(baseType)) {}

    std::string toString() const override {
        return baseType->toString() + "&&";
    }

    ast::TypePtr clone() const override {
        return std::make_shared<RValueReference>(token, baseType->clone());
    }

    static bool isRValueRefType(ast::TypePtr type) {
        return std::dynamic_pointer_cast<RValueReference>(type) != nullptr;
    }

    static ast::TypePtr createRValueRefType(ast::TypePtr baseType) {
        lexer::Token token; // Default token
        return std::make_shared<RValueReference>(token, baseType);
    }
};

/**
 * @brief Represents a move expression
 */
class MoveExpr : public ast::Expression {
public:
    ast::ExprPtr expr;

    MoveExpr(const lexer::Token& token, ast::ExprPtr expr)
        : ast::Expression(token), expr(std::move(expr)) {}

    void accept(ast::Visitor& visitor) override {
        visitor.visitMoveExpr(this);
    }

    ast::TypePtr getType() const override {
        if (expr) {
            return RValueReference::createRValueRefType(expr->getType());
        }
        return nullptr;
    }

    ast::ExprPtr getExpr() const { return expr; }
};

/**
 * @brief Handles move semantics checking
 */
class MoveChecker {
public:
    MoveChecker(error::ErrorHandler& errorHandler, OwnershipChecker& ownershipChecker)
        : errorHandler_(errorHandler), ownershipChecker_(ownershipChecker) {}

    /**
     * @brief Check if a move operation is valid
     */
    bool validateMove(ast::ExprPtr expr);

    /**
     * @brief Check if a type supports move semantics
     */
    bool supportsMove(ast::TypePtr type);

    /**
     * @brief Generate move constructor/assignment if needed
     */
    void generateMoveOperations(const std::string& typeName);

private:
    error::ErrorHandler& errorHandler_;
    OwnershipChecker& ownershipChecker_;
};

} // namespace type_checker
