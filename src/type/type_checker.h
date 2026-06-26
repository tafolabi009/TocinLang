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
        void visitIndexExpr(ast::IndexExpr *expr) override;
        void visitEnumStmt(ast::EnumStmt *stmt) override;
        void visitTryStmt(ast::TryStmt *stmt) override;
        void visitThrowStmt(ast::ThrowStmt *stmt) override;
        void visitBreakStmt(ast::BreakStmt *stmt) override;
        void visitContinueStmt(ast::ContinueStmt *stmt) override;
        void visitDeferStmt(ast::DeferStmt *stmt) override;
        void visitDestructureStmt(ast::DestructureStmt *stmt) override;
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
        bool isMovableType(ast::TypePtr type);
        ast::TypePtr getChannelElementType(ast::TypePtr channelType);
        bool typesCompatible(ast::TypePtr type1, ast::TypePtr type2);
        ast::TypePtr resolveType(ast::TypePtr type);
        void registerBuiltins();

        // --- Real type-checker helpers (added) ---
        // Entry point that performs two-pass checking over the program root.
        void checkProgram(ast::Statement *root);
        // Pass 1: hoist a top-level declaration (function/class) into globalEnv_.
        void hoistDeclaration(ast::Statement *stmt);
        // Convenience: build a BasicType shared_ptr.
        ast::TypePtr makeBasic(ast::TypeKind kind) const;
        // Map a declared/annotated type to its canonical BasicType when it names a
        // primitive (int/float/bool/string/void); otherwise returns the input.
        ast::TypePtr canonicalize(ast::TypePtr type) const;
        // Is this type a numeric (int or float) primitive?
        bool isNumeric(ast::TypePtr type) const;
        // Structural type equality that is correct for BasicType (whose built-in
        // equals() is pointer-identity only). Canonicalizes named primitives.
        bool sameType(ast::TypePtr a, ast::TypePtr b) const;
        // Convenience: the BasicType kind of a (canonicalized) type, or UNKNOWN.
        ast::TypeKind kindOf(ast::TypePtr type) const;
        // Treat null/UNKNOWN as "don't know" so we stay permissive.
        bool isUnknown(ast::TypePtr type) const;
        // Build a FunctionType from a FunctionStmt's signature (for hoisting/lookup).
        ast::TypePtr functionTypeOf(ast::FunctionStmt *fn) const;
        // The parser stores an *unannotated* return type as SimpleType("None")
        // (not null). Recognize "no explicit annotation" so we can infer instead
        // of treating it as an explicit void.
        bool isUnannotatedReturn(ast::TypePtr returnType) const;

        // During body checking, returns encountered inside the current function are
        // recorded here so an unannotated function can infer its return type.
        std::vector<ast::TypePtr> *currentReturnTypes_ = nullptr;
        // True while checking inside a function whose return type was explicitly
        // annotated (so we can flag clear mismatches).
        bool returnTypeIsExplicit_ = false;
        // Depth guard so we only treat the outermost statement as the program root.
        int checkDepth_ = 0;
        // True only while visiting the program-root block/module so its direct
        // children share the global environment (top-level scope).
        bool atProgramRoot_ = false;
        // Type-parameter names of the enclosing generic class (if any); methods
        // treat these as abstract types and check permissively.
        std::unordered_set<std::string> classTypeParams_;

        // Algebraic-enum tracking for exhaustiveness checking. Populated in
        // visitEnumStmt for enums that have at least one payload-carrying variant.
        std::unordered_map<std::string, std::string> adtVariantEnum_;                  // variant name -> enum name
        std::unordered_map<std::string, std::unordered_set<std::string>> adtEnumVariants_; // enum name -> variant set
        std::unordered_map<std::string, std::vector<ast::TypePtr>> adtVariantFields_;  // variant name -> payload field types

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
