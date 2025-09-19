#pragma once

#ifndef AST_VISITOR_RENAME_H
#define AST_VISITOR_RENAME_H

namespace ast
{

    // Forward declarations
    class BlockStmt;
    class ExpressionStmt;
    class PrintStmt;
    class VarStmt;
    class FunctionStmt;
    class ReturnStmt;
    class ClassStmt;
    class IfStmt;
    class WhileStmt;
    class ForStmt;
    class MatchStmt; // Added match statement

    class BinaryExpr;
    class GroupingExpr;
    class LiteralExpr;
    class UnaryExpr;
    class VariableExpr;
    class AssignExpr;
    class LogicalExpr;
    class CallExpr;
    class GetExpr;
    class SetExpr;
    class ThisExpr;
    class SuperExpr;
    class LambdaExpr;
    class ArrayExpr;
    class IndexExpr;
    class OptionExpr; // Added Option expression
    class ResultExpr; // Added Result expression
    class NewExpr;    // Added New expression
    class DeleteExpr; // Added Delete expression

    // Pattern classes for match expressions
    class WildcardPattern;
    class LiteralPattern;
    class VariablePattern;
    class ConstructorPattern;
    class TuplePattern;
    class StructPattern;
    class OrPattern;

    /**
     * Visitor interface for traversing the AST.
     * Implements the Visitor design pattern.
     */
    class Visitor
    {
    public:
        virtual ~Visitor() = default;

        // Statements
        virtual void visitBlockStmt(BlockStmt *stmt) = 0;
        virtual void visitExpressionStmt(ExpressionStmt *stmt) = 0;
        virtual void visitPrintStmt(PrintStmt *stmt) = 0;
        virtual void visitVarStmt(VarStmt *stmt) = 0;
        virtual void visitFunctionStmt(FunctionStmt *stmt) = 0;
        virtual void visitReturnStmt(ReturnStmt *stmt) = 0;
        virtual void visitClassStmt(ClassStmt *stmt) = 0;
        virtual void visitIfStmt(IfStmt *stmt) = 0;
        virtual void visitWhileStmt(WhileStmt *stmt) = 0;
        virtual void visitForStmt(ForStmt *stmt) = 0;
        virtual void visitMatchStmt(MatchStmt *stmt) = 0; // Added for match statement

        // Expressions
        virtual void visitBinaryExpr(BinaryExpr *expr) = 0;
        virtual void visitGroupingExpr(GroupingExpr *expr) = 0;
        virtual void visitLiteralExpr(LiteralExpr *expr) = 0;
        virtual void visitUnaryExpr(UnaryExpr *expr) = 0;
        virtual void visitVariableExpr(VariableExpr *expr) = 0;
        virtual void visitAssignExpr(AssignExpr *expr) = 0;
        virtual void visitLogicalExpr(LogicalExpr *expr) = 0;
        virtual void visitCallExpr(CallExpr *expr) = 0;
        virtual void visitGetExpr(GetExpr *expr) = 0;
        virtual void visitSetExpr(SetExpr *expr) = 0;
        virtual void visitThisExpr(ThisExpr *expr) = 0;
        virtual void visitSuperExpr(SuperExpr *expr) = 0;
        virtual void visitLambdaExpr(LambdaExpr *expr) = 0;
        virtual void visitArrayExpr(ArrayExpr *expr) = 0;
        virtual void visitIndexExpr(IndexExpr *expr) = 0;
        virtual void visitOptionExpr(OptionExpr *expr) = 0; // Added for Option expression
        virtual void visitResultExpr(ResultExpr *expr) = 0; // Added for Result expression
        virtual void visitNewExpr(NewExpr *expr) = 0;       // Added for New expression
        virtual void visitDeleteExpr(DeleteExpr *expr) = 0; // Added for Delete expression

        // Pattern visitors
        virtual void visitWildcardPattern(WildcardPattern *pattern) = 0;
        virtual void visitLiteralPattern(LiteralPattern *pattern) = 0;
        virtual void visitVariablePattern(VariablePattern *pattern) = 0;
        virtual void visitConstructorPattern(ConstructorPattern *pattern) = 0;
        virtual void visitTuplePattern(TuplePattern *pattern) = 0;
        virtual void visitStructPattern(StructPattern *pattern) = 0;
        virtual void visitOrPattern(OrPattern *pattern) = 0;
    };

    /**
     * Extended Visitor interface
     * Extends the base Visitor from ast.h with additional visitors
     */
    class ExtendedVisitor : public Visitor
    {
    public:
        virtual ~ExtendedVisitor() = default;

        // Additional statements not in the base Visitor
        virtual void visitPrintStmt(PrintStmt *stmt) = 0;

        // Additional expressions not in the base Visitor
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
    };

} // namespace ast

#endif // AST_VISITOR_RENAME_H
