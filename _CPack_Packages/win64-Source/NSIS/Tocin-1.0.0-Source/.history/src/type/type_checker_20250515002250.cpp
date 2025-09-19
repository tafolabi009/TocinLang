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
} // namespace type_checker 
