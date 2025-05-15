#include "type_checker.h"
#include <stdexcept>

namespace type_checker
{
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

    void TypeChecker::visitMoveExpr(void *expr)
    {
        // This will be properly implemented when move semantics are fully integrated
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));

        errorHandler_.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                  "MoveExpr not yet implemented in type checker",
                                  "", 0, 0, error::ErrorSeverity::WARNING);
    }

    void TypeChecker::visitGoExpr(void *expr)
    {
        // This will be properly implemented when concurrency is fully integrated
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));

        errorHandler_.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                  "GoExpr not yet implemented in type checker",
                                  "", 0, 0, error::ErrorSeverity::WARNING);
    }

    void TypeChecker::visitRuntimeChannelSendExpr(void *expr)
    {
        // This will be properly implemented when concurrency is fully integrated
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));

        errorHandler_.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                  "Runtime ChannelSendExpr not yet implemented in type checker",
                                  "", 0, 0, error::ErrorSeverity::WARNING);
    }

    void TypeChecker::visitRuntimeChannelReceiveExpr(void *expr)
    {
        // This will be properly implemented when concurrency is fully integrated
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "any", "", 0, 0));

        errorHandler_.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                  "Runtime ChannelReceiveExpr not yet implemented in type checker",
                                  "", 0, 0, error::ErrorSeverity::WARNING);
    }

    void TypeChecker::visitRuntimeSelectStmt(void *stmt)
    {
        // This will be properly implemented when concurrency is fully integrated
        currentType_ = std::make_shared<ast::SimpleType>(
            lexer::Token(lexer::TokenType::IDENTIFIER, "void", "", 0, 0));

        errorHandler_.reportError(error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
                                  "Runtime SelectStmt not yet implemented in type checker",
                                  "", 0, 0, error::ErrorSeverity::WARNING);
    }

    // Implementation for channel-related visitor methods
    void TypeChecker::visitChannelSendExpr(ast::ChannelSendExpr *expr)
    {
        // Placeholder implementation for channel send expression
        currentType_ = nullptr;
        errorHandler_.reportError(
            error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
            "Channel send expressions are not yet implemented",
            std::string(expr->token.filename), expr->token.line, expr->token.column,
            error::ErrorSeverity::WARNING);
    }

    void TypeChecker::visitChannelReceiveExpr(ast::ChannelReceiveExpr *expr)
    {
        // Placeholder implementation for channel receive expression
        currentType_ = nullptr;
        errorHandler_.reportError(
            error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
            "Channel receive expressions are not yet implemented",
            std::string(expr->token.filename), expr->token.line, expr->token.column,
            error::ErrorSeverity::WARNING);
    }

    void TypeChecker::visitSelectStmt(ast::SelectStmt *stmt)
    {
        // Placeholder implementation for select statement
        errorHandler_.reportError(
            error::ErrorCode::C001_UNIMPLEMENTED_FEATURE,
            "Select statements are not yet implemented",
            std::string(stmt->token.filename), stmt->token.line, stmt->token.column,
            error::ErrorSeverity::WARNING);
    }
} // namespace type_checker
