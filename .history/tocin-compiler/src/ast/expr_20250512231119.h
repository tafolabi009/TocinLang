#pragma once

#include "../lexer/token.h"
#include <memory>

namespace ast
{

    // Forward declarations
    class Visitor;
    class Type;
    using TypePtr = std::shared_ptr<Type>;

    /**
     * @brief Base class for all expression nodes in the AST.
     */
    class Expression
    {
    public:
        explicit Expression(const lexer::Token &token) : token(token) {}
        virtual ~Expression() = default;

        /**
         * @brief Accept a visitor for processing this expression.
         * @param visitor The visitor to accept.
         */
        virtual void accept(Visitor &visitor) = 0;

        /**
         * @brief Get the type of this expression.
         * @return The type of the expression or nullptr if not typed.
         */
        virtual TypePtr getType() const = 0;

        /**
         * @brief Get the token associated with this expression.
         * @return The token.
         */
        const lexer::Token &getToken() const { return token; }

    protected:
        lexer::Token token;
    };

    // Define shared_ptr type for expressions
    using ExprPtr = std::shared_ptr<Expression>;

} // namespace ast
