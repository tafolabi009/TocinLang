#pragma once

#include "expr.h"
#include "../lexer/token.h"
#include "../type/option_result_types.h"
#include <memory>

namespace ast
{

    // Forward declarations
    class Visitor;

    /**
     * Represents an Option constructor expression.
     * Can be 'Some(expr)' or 'None'
     */
    class OptionExpr : public Expression
    {
    public:
        enum class Variant
        {
            SOME,
            NONE
        };

        // Constructor for Some variant
        OptionExpr(const lexer::Token &token, ExprPtr value)
            : Expression(token), variant(Variant::SOME), value(std::move(value)) {}

        // Constructor for None variant
        OptionExpr(const lexer::Token &token)
            : Expression(token), variant(Variant::NONE), value(nullptr) {}

        // Accept method for the visitor pattern
        void accept(Visitor &visitor) override;

        // Add implementation of getType method required by Expression base class
        TypePtr getType() const override { return nullptr; }

        // Getters
        Variant getVariant() const { return variant; }
        const ExprPtr &getValue() const { return value; }
        bool isSome() const { return variant == Variant::SOME; }
        bool isNone() const { return variant == Variant::NONE; }

    private:
        Variant variant;
        ExprPtr value; // nullptr for None variant
    };

    /**
     * Represents a Result constructor expression.
     * Can be 'Ok(expr)' or 'Err(expr)'
     */
    class ResultExpr : public Expression
    {
    public:
        enum class Variant
        {
            OK,
            ERR
        };

        ResultExpr(const lexer::Token &token, Variant variant, ExprPtr value)
            : Expression(token), variant(variant), value(std::move(value)) {}

        // Accept method for the visitor pattern
        void accept(Visitor &visitor) override;

        // Add implementation of getType method required by Expression base class
        TypePtr getType() const override { return nullptr; }

        // Getters
        Variant getVariant() const { return variant; }
        const ExprPtr &getValue() const { return value; }
        bool isOk() const { return variant == Variant::OK; }
        bool isErr() const { return variant == Variant::ERR; }

    private:
        Variant variant;
        ExprPtr value;
    };

} // namespace ast
