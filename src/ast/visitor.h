#pragma once

// Include ast.h to ensure we're working with the same definitions
#include "ast.h"

namespace ast
{
    // Forward declarations for classes that are specific to this file and not in ast.h
    class PrintStmt;
    class LogicalExpr;
    class ThisExpr;
    class SuperExpr;
    class ArrayExpr;
    class IndexExpr;
    class OptionExpr;
    class ResultExpr;

    // Pattern classes for match expressions - if not already defined in ast.h
    class WildcardPattern;
    class LiteralPattern;
    class VariablePattern;
    class ConstructorPattern;
    class TuplePattern;
    class StructPattern;
    class OrPattern;

    // Trait-related forward declarations
    class TraitStmt;
    class ImplStmt;

    /**
     * Enhanced Visitor interface for additional AST node types
     * This class now inherits from the base Visitor to combine functionality
     */
    class EnhancedVisitor : public Visitor
    {
    public:
        virtual ~EnhancedVisitor() = default;

        // Additional statements
        virtual void visitPrintStmt(PrintStmt *stmt) = 0;

        // Additional expressions
        virtual void visitLogicalExpr(LogicalExpr *expr) = 0;
        virtual void visitThisExpr(ThisExpr *expr) = 0;
        virtual void visitSuperExpr(SuperExpr *expr) = 0;
        virtual void visitArrayExpr(ArrayExpr *expr) = 0;
        virtual void visitIndexExpr(IndexExpr *expr) = 0;
        virtual void visitOptionExpr(OptionExpr *expr) = 0;
        virtual void visitResultExpr(ResultExpr *expr) = 0;

        // Pattern visitors
        virtual void visitWildcardPattern(WildcardPattern *pattern) = 0;
        virtual void visitLiteralPattern(LiteralPattern *pattern) = 0;
        virtual void visitVariablePattern(VariablePattern *pattern) = 0;
        virtual void visitConstructorPattern(ConstructorPattern *pattern) = 0;
        virtual void visitTuplePattern(TuplePattern *pattern) = 0;
        virtual void visitStructPattern(StructPattern *pattern) = 0;
        virtual void visitOrPattern(OrPattern *pattern) = 0;

        // Trait-related visitors
        virtual void visitTraitStmt(TraitStmt *stmt) = 0;
        virtual void visitImplStmt(ImplStmt *stmt) = 0;
    };

} // namespace ast
