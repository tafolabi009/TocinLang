#include "ast.h"
// Remove the include of visitor.h since it causes redefinition conflicts
// #include "visitor.h"
#include <sstream>

namespace ast
{

    // ... existing code ...

    class NewExpr : public Expression
    {
    public:
        NewExpr(const lexer::Token &keyword, ast::ExprPtr typeExpr, ast::ExprPtr sizeExpr)
            : Expression(keyword), keyword(keyword), typeExpr(std::move(typeExpr)), sizeExpr(std::move(sizeExpr)) {}

        void accept(Visitor &visitor) override
        {
            visitor.visitNewExpr(this);
        }

        TypePtr getType() const override
        {
            return typeExpr ? typeExpr->getType() : nullptr;
        }

        const lexer::Token &getKeyword() const { return keyword; }
        const ast::ExprPtr &getTypeExpr() const { return typeExpr; }
        const ast::ExprPtr &getSizeExpr() const { return sizeExpr; }

    private:
        lexer::Token keyword;
        ast::ExprPtr typeExpr;
        ast::ExprPtr sizeExpr; // nullptr for non-array allocations
    };

    class DeleteExpr : public Expression
    {
    public:
        DeleteExpr(const lexer::Token &keyword, ast::ExprPtr expr)
            : Expression(keyword), keyword(keyword), expr(std::move(expr)) {}

        void accept(Visitor &visitor) override
        {
            visitor.visitDeleteExpr(this);
        }

        TypePtr getType() const override
        {
            return nullptr; // delete expression has no type
        }

        const lexer::Token &getKeyword() const { return keyword; }
        const ast::ExprPtr &getExpr() const { return expr; }

    private:
        lexer::Token keyword;
        ast::ExprPtr expr;
    };

    // ... rest of the file ...
}
