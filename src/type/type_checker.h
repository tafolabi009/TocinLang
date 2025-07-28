#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include "feature_integration.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

// Forward declaration to break circular dependency
namespace tocin {
namespace compiler {
    class CompilationContext;
}
}

namespace type_checker
{

    /**
     * @brief Environment for tracking variable and function types in a scope.
     */
    class Environment
    {
    public:
        Environment() = default;
        explicit Environment(std::shared_ptr<Environment> parent) : parent_(parent) {}

        void define(const std::string &name, ast::TypePtr type, bool isConstant);
        ast::TypePtr lookup(const std::string &name) const;
        bool assign(const std::string &name, ast::TypePtr type);

        // Add a getter for parent
        std::shared_ptr<Environment> getParent() const { return parent_; }

        // Module support
        void setModule(const std::string &moduleName) { currentModule = moduleName; }
        std::string getModule() const { return currentModule; }
        void addExport(const std::string &name) { exportedSymbols.insert(name); }
        bool isExported(const std::string &name) const { return exportedSymbols.count(name) > 0; }
        std::unordered_set<std::string> getExportedSymbols() const { return exportedSymbols; }

    private:
        std::unordered_map<std::string, std::pair<ast::TypePtr, bool>> variables_;
        std::shared_ptr<Environment> parent_;
        std::string currentModule;
        std::unordered_set<std::string> exportedSymbols;
    };

    /**
     * @brief Type checker for validating AST nodes.
     */
    class TypeChecker : public ast::Visitor
    {
    public:
        explicit TypeChecker(error::ErrorHandler &errorHandler,
                             tocin::compiler::CompilationContext &compilationContext,
                             FeatureManager *featureManager = nullptr);

        /**
         * @brief Type checks the given AST.
         * @param stmt The root statement to check.
         * @return The inferred type or nullptr if invalid.
         */
        ast::TypePtr check(ast::StmtPtr stmt);

        void visitBinaryExpr(ast::BinaryExpr *expr) override;
        void visitGroupingExpr(ast::GroupingExpr *expr) override;
        void visitLiteralExpr(ast::LiteralExpr *expr) override;
        void visitUnaryExpr(ast::UnaryExpr *expr) override;
        void visitVariableExpr(ast::VariableExpr *expr) override;
        void visitAssignExpr(ast::AssignExpr *expr) override;
        void visitCallExpr(ast::CallExpr *expr) override;
        void visitGetExpr(ast::GetExpr *expr) override;
        void visitSetExpr(ast::SetExpr *expr) override;
        void visitListExpr(ast::ListExpr *expr) override;
        void visitDictionaryExpr(ast::DictionaryExpr *expr) override;
        void visitLambdaExpr(ast::LambdaExpr *expr) override;
        void visitAwaitExpr(ast::AwaitExpr *expr) override;
        void visitNewExpr(ast::NewExpr *expr) override;
        void visitDeleteExpr(ast::DeleteExpr *expr) override;
        void visitStringInterpolationExpr(ast::StringInterpolationExpr *expr) override;
        void visitExpressionStmt(ast::ExpressionStmt *stmt) override;
        void visitVariableStmt(ast::VariableStmt *stmt) override;
        void visitBlockStmt(ast::BlockStmt *stmt) override;
        void visitIfStmt(ast::IfStmt *stmt) override;
        void visitWhileStmt(ast::WhileStmt *stmt) override;
        void visitForStmt(ast::ForStmt *stmt) override;
        void visitFunctionStmt(ast::FunctionStmt *stmt) override;
        void visitReturnStmt(ast::ReturnStmt *stmt) override;
        void visitClassStmt(ast::ClassStmt *stmt) override;
        void visitImportStmt(ast::ImportStmt *stmt) override;
        void visitExportStmt(ast::ExportStmt *stmt) override;
        void visitModuleStmt(ast::ModuleStmt *stmt) override;
        void visitMatchStmt(ast::MatchStmt *stmt) override;
        void visitArrayLiteralExpr(ast::ArrayLiteralExpr *expr) override;
        void visitMoveExpr(void *expr) override;
        void visitGoExpr(void *expr) override;
        void visitRuntimeChannelSendExpr(void *expr) override;
        void visitRuntimeChannelReceiveExpr(void *expr) override;
        void visitRuntimeSelectStmt(void *stmt) override;
        void visitChannelSendExpr(ast::ChannelSendExpr *expr) override;
        void visitChannelReceiveExpr(ast::ChannelReceiveExpr *expr) override;
        void visitSelectStmt(ast::SelectStmt *stmt) override;
        void visitGoStmt(ast::GoStmt* stmt) override;
        void visitTraitStmt(ast::TraitStmt *stmt) override;
        void visitImplStmt(ast::ImplStmt *stmt) override;

        bool checkExpression(const std::shared_ptr<ast::Expression> &expr,
                             tocin::compiler::CompilationContext &compilationContext,
                             const std::string &expectedType = "");

    private:
        ast::TypePtr currentType_;
        std::shared_ptr<Environment> environment_;
        std::shared_ptr<Environment> globalEnv_;
        error::ErrorHandler &errorHandler_;
        tocin::compiler::CompilationContext &compilationContext_;
        FeatureManager *featureManager_;
        bool inAsyncContext_ = false;
        ast::TypePtr expectedReturnType_ = nullptr;
        std::string currentModuleName_;

        void pushScope();
        void popScope();
        bool isAssignable(ast::TypePtr from, ast::TypePtr to);
        ast::TypePtr resolveType(ast::TypePtr type);
        void registerBuiltins();

        // Module related methods
        bool loadModule(const std::string &moduleName);
        bool importSymbol(const std::string &moduleName, const std::string &symbolName,
                          const std::string &alias = "");
        void setCurrentModule(const std::string &moduleName);
        std::string getCurrentModule() const;
        void addExport(const std::string &name);
        bool checkCircularImports(const std::string &moduleName);

        bool canRunAsGoroutine(ast::ExprPtr expr);
        bool validateGoroutineLaunch(ast::ExprPtr function, const std::vector<ast::ExprPtr>& arguments);
    };

} // namespace type_checker

#endif // TYPE_CHECKER_H
