#pragma once

#include "../pch.h"
#include "../ast/ast.h"
#include "../error/error_handler.h"

namespace type_checker
{

    /**
     * @brief Class for handling nullable types and null safety
     *
     * Implements Kotlin/Swift-like null safety features:
     * - Nullable types (Type?)
     * - Safe call operator (?.)
     * - Elvis operator (?:)
     * - Not-null assertion operator (!)
     */
    class NullSafetyChecker
    {
    public:
        NullSafetyChecker(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        /**
         * @brief Check if a type is nullable
         *
         * @param type The type to check
         * @return true if the type is nullable
         */
        bool isNullableType(ast::TypePtr type)
        {
            return std::dynamic_pointer_cast<ast::NullableType>(type) != nullptr;
        }

        /**
         * @brief Create a nullable version of a type
         *
         * @param type The base type
         * @return ast::TypePtr The nullable type
         */
        ast::TypePtr makeNullable(ast::TypePtr type)
        {
            // Don't make already nullable types nullable again
            if (isNullableType(type))
            {
                return type;
            }

            // Create a dummy token for the nullable type
            lexer::Token token;
            token.type = lexer::TokenType::IDENTIFIER;
            token.lexeme = type->toString() + "?";
            
            return std::make_shared<ast::NullableType>(token, type);
        }

        /**
         * @brief Get the non-nullable base type from a nullable type
         *
         * @param type The nullable type
         * @return ast::TypePtr The base type
         */
        ast::TypePtr getNonNullableType(ast::TypePtr type)
        {
            if (auto nullableType = std::dynamic_pointer_cast<ast::NullableType>(type))
            {
                return nullableType->baseType;
            }
            return type; // Already non-nullable
        }

        /**
         * @brief Validate a safe call expression (?.)
         *
         * @param expr The object expression
         * @param objType The type of the object
         * @param memberName The name of the member being accessed
         * @return true if the safe call is valid
         */
        bool validateSafeCall(ast::ExprPtr expr, ast::TypePtr objType, const std::string &memberName)
        {
            // Safe calls can only be used on nullable types
            if (!isNullableType(objType))
            {
                errorHandler.reportError(
                    error::ErrorCode::T001_TYPE_MISMATCH,
                    "Safe call operator (?.) can only be used on nullable types",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            // Get the base type to check if it has the member
            ast::TypePtr baseType = getNonNullableType(objType);

            // Check if the member exists on the base type
            // This is a simplified check - real implementation would be more thorough
            return true;
        }

        /**
         * @brief Validate a not-null assertion (!)
         *
         * @param expr The expression being asserted non-null
         * @param exprType The type of the expression
         * @return true if the assertion is valid
         */
        bool validateNotNullAssertion(ast::ExprPtr expr, ast::TypePtr exprType)
        {
            // Not-null assertions can only be used on nullable types
            if (!isNullableType(exprType))
            {
                errorHandler.reportError(
                    error::ErrorCode::T001_TYPE_MISMATCH,
                    "Not-null assertion operator (!) can only be used on nullable types",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            return true;
        }

        /**
         * @brief Validate an Elvis operator expression (?:)
         *
         * @param expr The nullable expression
         * @param exprType The type of the nullable expression
         * @param defaultExpr The default expression
         * @param defaultType The type of the default expression
         * @return true if the Elvis operator expression is valid
         */
        bool validateElvisOperator(ast::ExprPtr expr, ast::TypePtr exprType,
                                   ast::ExprPtr defaultExpr, ast::TypePtr defaultType)
        {
            // Elvis operator should typically be used with nullable types
            if (!isNullableType(exprType))
            {
                errorHandler.reportError(
                    error::ErrorCode::T001_TYPE_MISMATCH,
                    "Elvis operator (?:) should be used with nullable types",
                    "", 0, 0, error::ErrorSeverity::WARNING);
                // Continue anyway - it's just a warning
            }

            // The default expression type should be compatible with the base type
            ast::TypePtr baseType = getNonNullableType(exprType);

            // Simple compatibility check - in practice would need more thorough checks
            if (baseType->toString() != defaultType->toString() &&
                !isNullableType(defaultType))
            {
                errorHandler.reportError(
                    error::ErrorCode::T001_TYPE_MISMATCH,
                    "Default expression type doesn't match nullable expression type",
                    "", 0, 0, error::ErrorSeverity::ERROR);
                return false;
            }

            return true;
        }

        /**
         * @brief Get the result type of a safe call expression
         *
         * @param objType The object type
         * @param memberType The member type
         * @return ast::TypePtr The result type (always nullable)
         */
        ast::TypePtr getSafeCallResultType(ast::TypePtr objType, ast::TypePtr memberType)
        {
            // Result of safe call is always nullable, regardless of member type
            return makeNullable(memberType);
        }

        /**
         * @brief Get the result type of a not-null assertion
         *
         * @param exprType The expression type
         * @return ast::TypePtr The result type (non-nullable)
         */
        ast::TypePtr getNotNullAssertionResultType(ast::TypePtr exprType)
        {
            return getNonNullableType(exprType);
        }

        /**
         * @brief Get the result type of an Elvis operator expression
         *
         * @param exprType The nullable expression type
         * @param defaultType The default expression type
         * @return ast::TypePtr The result type
         */
        ast::TypePtr getElvisOperatorResultType(ast::TypePtr exprType, ast::TypePtr defaultType)
        {
            // If the default is nullable, result is nullable
            // Otherwise, result is non-nullable
            if (isNullableType(defaultType))
            {
                return defaultType;
            }
            else
            {
                return getNonNullableType(exprType);
            }
        }

    private:
        error::ErrorHandler &errorHandler;
    };

    /**
     * @brief AST node for a safe call expression
     *
     * Represents a null-safe member access (obj?.member).
     */
    class SafeCallExpr : public ast::Expression
    {
    public:
        ast::ExprPtr object;    // The object being accessed
        std::string memberName; // The name of the member

        SafeCallExpr(const lexer::Token &token, ast::ExprPtr object, std::string memberName)
            : Expression(token), object(std::move(object)), memberName(std::move(memberName)) {}

        void accept(ast::Visitor &visitor) override
        {
            // This would be implemented when we add the visitor method
            // visitor.visitSafeCallExpr(this);
        }

        ast::TypePtr getType() const override
        virtual ast::TypePtr getType() const override
        {
            // Type depends on the member type
            // This is a placeholder
            return nullptr;
        }
    };

    /**
     * @brief AST node for a not-null assertion
     *
     * Represents an assertion that a nullable value is not null (expr!).
     */
    class NotNullAssertExpr : public ast::Expr
    {
    public:
        ast::ExprPtr expr; // The expression being asserted non-null

        NotNullAssertExpr(ast::ExprPtr expr)
            : expr(expr) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            // This would be implemented when we add the visitor method
            // visitor.visitNotNullAssertExpr(this);
        }

        virtual ast::TypePtr getType() const override
        {
            // Type is the base type of the nullable expression
            // This is a placeholder
            return nullptr;
        }
    };

    /**
     * @brief AST node for an Elvis operator expression
     *
     * Represents a null coalescing operation (expr ?: default).
     */
    class ElvisOperatorExpr : public ast::Expr
    {
    public:
        ast::ExprPtr expr;        // The nullable expression
        ast::ExprPtr defaultExpr; // The default expression

        ElvisOperatorExpr(ast::ExprPtr expr, ast::ExprPtr defaultExpr)
            : expr(expr), defaultExpr(defaultExpr) {}

        virtual void accept(ast::Visitor &visitor) override
        {
            // This would be implemented when we add the visitor method
            // visitor.visitElvisOperatorExpr(this);
        }

        virtual ast::TypePtr getType() const override
        {
            // Type depends on both expressions
            // This is a placeholder
            return nullptr;
        }
    };

} // namespace type_checker
