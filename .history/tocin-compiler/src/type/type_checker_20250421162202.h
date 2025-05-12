#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "ast/ast.h"
#include "../error/error_handler.h"
#include <absl/container/flat_hash_map.h>
#include <memory>
#include <string>

namespace type_checker {

    /**
     * @brief Environment for tracking variable and function types in a scope.
     */
    class Environment {
    public:
        Environment() = default;
        explicit Environment(std::shared_ptr<Environment> parent) : parent_(parent) {}

        void define(const std::string& name, ast::TypePtr type, bool isConstant);
        ast::TypePtr lookup(const std::string& name) const;
        bool assign(const std::string& name, ast::TypePtr type);

    private:
        absl::flat_hash_map<std::string, std::pair<ast::TypePtr, bool>> variables_;
        std::shared_ptr<Environment> parent_;
    };

    /**
     * @brief Type checker for validating AST nodes.
     */
    class TypeChecker : public ast::Visitor {
    public:
        explicit TypeChecker(error::ErrorHandler& errorHandler);

        /**
         * @brief Type checks the given AST.
         * @param stmt The root statement to check.
         * @return The inferred type or nullptr if invalid.
         */
        ast::TypePtr check(ast::StmtPtr stmt);

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

    private:
        ast::TypePtr currentType_;
        std::shared_ptr<Environment> environment_;
        std::shared_ptr<Environment> globalEnv_;
        error::ErrorHandler& errorHandler_;
        bool inAsyncContext_ = false;
        ast::TypePtr expectedReturnType_ = nullptr;

        void pushScope();
        void popScope();
        bool isAssignable(ast::TypePtr from, ast::TypePtr to);
        ast::TypePtr resolveType(ast::TypePtr type);
    };

} // namespace type_checker

#endif // TYPE_CHECKER_H