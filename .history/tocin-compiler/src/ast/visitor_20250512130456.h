#pragma once

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
    };

} // namespace ast
