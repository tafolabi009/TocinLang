#pragma once

#include "ast.h"

namespace ast
{

/**
 * @brief AST node for a deferred statement
 * 
 * Represents a statement that will be executed when the current function exits,
 * similar to Go's defer statement. Deferred statements are executed in LIFO order.
 */
class DeferStmt : public Stmt
{
public:
    ExprPtr expression; // The expression to defer (typically a function call)

    DeferStmt(ExprPtr expression)
        : expression(expression) {}

    virtual void accept(Visitor &visitor) override
    {
        visitor.visitDeferStmt(this);
    }
};

/**
 * @brief Implementation for deferred function calls
 * 
 * This is a specialized form of DeferStmt that specifically handles function calls,
 * which is the most common use case for defer.
 */
class DeferCallStmt : public DeferStmt
{
public:
    DeferCallStmt(ExprPtr callExpr)
        : DeferStmt(callExpr) {}

    /**
     * @brief Get the underlying call expression
     * 
     * @return CallExpr* The call expression being deferred
     */
    CallExpr* getCallExpr() const
    {
        return dynamic_cast<CallExpr*>(expression.get());
    }
};

/**
 * @brief Extension to the Visitor interface for deferred statements
 */
class DeferVisitor
{
public:
    virtual void visitDeferStmt(DeferStmt *stmt) = 0;
};

} // namespace ast 
