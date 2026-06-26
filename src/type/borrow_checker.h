#pragma once

// Opt-in ownership / borrow checker (move + use-after-move analysis).
//
// Tocin is GC-based and lets class instances alias freely by default, so this
// pass is OFF unless `--borrow-check` is passed. When on, it enforces Rust-like
// move semantics on *owned* values (class/struct instances): moving a value
// (binding it elsewhere, passing it by value, returning it, or `move x`)
// invalidates the source, and using a moved value is a compile error.
// Reassigning a moved variable revives it. Copy types (int/float/bool/string)
// are never moved. The pass never changes codegen; it only reports diagnostics.

#include "../ast/ast.h"
#include "../error/error_handler.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace type_checker
{
    class BorrowChecker
    {
    public:
        explicit BorrowChecker(error::ErrorHandler &errorHandler)
            : errorHandler(errorHandler) {}

        // Returns true if no ownership errors were found.
        bool check(const ast::StmtPtr &program);

    private:
        enum class State { Owned, Moved };
        using ScopeMap = std::unordered_map<std::string, State>;
        using Snapshot = std::vector<ScopeMap>;

        error::ErrorHandler &errorHandler;
        std::unordered_set<std::string> ownedClasses; // class/struct type names
        std::vector<ScopeMap> scopes;                 // stack of owned-local states
        bool sawError = false;

        // Pre-pass: gather class/struct names (the set of owned types).
        void collectClasses(const ast::StmtPtr &stmt);
        bool typeIsOwned(const ast::TypePtr &type) const;
        bool isOwnedDecl(ast::VariableStmt *v);

        // Scope + state helpers.
        void pushScope() { scopes.emplace_back(); }
        void popScope() { if (!scopes.empty()) scopes.pop_back(); }
        void declareOwned(const std::string &name);
        State *find(const std::string &name); // nearest scope entry, or null

        // Recursive analysis.
        void checkStmt(const ast::StmtPtr &stmt);
        void analyzeFunction(ast::FunctionStmt *fn);
        void checkValue(const ast::ExprPtr &expr); // read context
        void consume(const ast::ExprPtr &expr);    // move (by-value) context
        const ast::VariableExpr *asBareVar(const ast::ExprPtr &expr);

        // Branch join: a variable moved on ANY analyzed path is moved after.
        Snapshot snapshot() const { return scopes; }
        void restore(const Snapshot &s) { scopes = s; }
        void mergeMoved(const std::vector<Snapshot> &ends);

        void errMoved(const std::string &name, const lexer::Token &tok);
    };
} // namespace type_checker
