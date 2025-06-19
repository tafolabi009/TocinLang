#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "../tocin-compiler/src/ast/ast.h"
#include "../tocin-compiler/src/error/error_handler.h"
#include <memory>
#include <unordered_map>
#include <string>
#include "Environment.h"

namespace interpreter {

    class Interpreter : public ast::Visitor {
    public:
        explicit Interpreter(error::ErrorHandler& errorHandler);
        // Implement visitor methods for each AST node type
        void visitBinaryExpr(ast::BinaryExpr* expr) override;
        void visitGroupingExpr(ast::GroupingExpr* expr) override;
        void visitLiteralExpr(ast::LiteralExpr* expr) override;
        void visitUnaryExpr(ast::UnaryExpr* expr) override;
        void visitVariableExpr(ast::VariableExpr* expr) override;
        void visitAssignExpr(ast::AssignExpr* expr) override;
        void visitCallExpr(ast::CallExpr* expr) override;
        void visitGetExpr(ast::GetExpr* expr) override;
        void visitSetExpr(ast::SetExpr* expr) override;
        void visitListExpr(ast::ListExpr* expr) override;
        void visitDictionaryExpr(ast::DictionaryExpr* expr) override;
        void visitLambdaExpr(ast::LambdaExpr* expr) override;
        void visitAwaitExpr(ast::AwaitExpr* expr) override;
        void visitExpressionStmt(ast::ExpressionStmt* stmt) override;
        void visitVariableStmt(ast::VariableStmt* stmt) override;
        void visitBlockStmt(ast::BlockStmt* stmt) override;
        void visitIfStmt(ast::IfStmt* stmt) override;
        void visitWhileStmt(ast::WhileStmt* stmt) override;
        void visitForStmt(ast::ForStmt* stmt) override;
        void visitFunctionStmt(ast::FunctionStmt* stmt) override;
        void visitReturnStmt(ast::ReturnStmt* stmt) override;
        void visitClassStmt(ast::ClassStmt* stmt) override;
        void visitImportStmt(ast::ImportStmt* stmt) override;
        void visitMatchStmt(ast::MatchStmt* stmt) override;
        void visitNewExpr(ast::NewExpr* expr) override;
        void visitDeleteExpr(ast::DeleteExpr* expr) override;
        void visitExportStmt(ast::ExportStmt* stmt) override;
        void visitModuleStmt(ast::ModuleStmt* stmt) override;
        void visitStringInterpolationExpr(ast::StringInterpolationExpr* expr) override;
        void visitChannelSendExpr(ast::ChannelSendExpr* expr) override;
        void visitChannelReceiveExpr(ast::ChannelReceiveExpr* expr) override;
        void visitSelectStmt(ast::SelectStmt* stmt) override;
        void visitArrayLiteralExpr(ast::ArrayLiteralExpr* expr) override;
        void visitMoveExpr(void* expr) override;
        void visitGoExpr(void* expr) override;
        void visitRuntimeChannelSendExpr(void* expr) override;
        void visitRuntimeChannelReceiveExpr(void* expr) override;
        void visitRuntimeSelectStmt(void* stmt) override;
        void visitGoStmt(ast::GoStmt* stmt) override;

        // Method to interpret a statement
        void interpret(ast::StmtPtr stmt);

    private:
        error::ErrorHandler& errorHandler;
        std::shared_ptr<Environment> environment;
        // Add any additional state or helper methods here
    };

} // namespace interpreter

#endif // INTERPRETER_H 