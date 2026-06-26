#pragma once

// Opt-in ownership / borrow checker (move + use-after-move + reference borrows).
//
// Tocin is GC-based and lets class instances alias freely by default, so this
// pass is OFF unless `--borrow-check` is passed. When on, it enforces Rust-like
// rules on *owned* values (class/struct instances):
//
//   Moves: moving a value (binding it elsewhere, passing it by value, returning
//   it, or `move x`) invalidates the source; using a moved value is a compile
//   error (B001). Reassigning a moved variable revives it. Copy types
//   (int/float/bool/string) are never moved.
//
//   Borrows: `&x` takes a shared borrow and `&mut x` an exclusive one. While a
//   `&mut x` is live you cannot use, move, mutate, or re-borrow `x`; while any
//   `&x` is live you cannot mutably borrow, move, or mutate `x` (B002). Borrows
//   are released when the borrowing binding leaves scope (lexical lifetimes).
//
// The pass never changes codegen; it only reports diagnostics.

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

        // Per-variable ownership + borrow bookkeeping.
        struct VarInfo
        {
            State state = State::Owned;
            int shared = 0;          // # of live shared borrows OF this variable
            bool mut = false;        // a live &mut borrow OF this variable exists
            std::string borrowOf;    // if this var is a reference, the var it borrows
            bool borrowMut = false;  // ... and whether that borrow is mutable
        };
        using ScopeMap = std::unordered_map<std::string, VarInfo>;
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
        void popScope();                              // releases borrows held here
        void declareOwned(const std::string &name);
        VarInfo *find(const std::string &name);       // nearest scope entry, or null

        // Recursive analysis.
        void checkStmt(const ast::StmtPtr &stmt);
        void analyzeFunction(ast::FunctionStmt *fn);
        void checkValue(const ast::ExprPtr &expr); // read context
        void consume(const ast::ExprPtr &expr);    // move (by-value) context
        const ast::VariableExpr *asBareVar(const ast::ExprPtr &expr);

        // Borrow helpers. Returns true if `expr` was a borrow and was handled.
        bool isBorrow(const ast::ExprPtr &expr, bool &isMut, const ast::VariableExpr *&target);
        void takeBorrow(const std::string &refName, const ast::VariableExpr *target, bool isMut);
        bool hasLiveBorrow(const std::string &name); // shared>0 or mut

        // Branch join: a variable moved on ANY analyzed path is moved after.
        Snapshot snapshot() const { return scopes; }
        void restore(const Snapshot &s) { scopes = s; }
        void mergeMoved(const std::vector<Snapshot> &ends);

        void errMoved(const std::string &name, const lexer::Token &tok);
        void errBorrow(const std::string &msg, const lexer::Token &tok);
    };
} // namespace type_checker
