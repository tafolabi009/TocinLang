#include "borrow_checker.h"

namespace type_checker
{
    template <typename T>
    static std::shared_ptr<T> as(const ast::ExprPtr &e) { return std::dynamic_pointer_cast<T>(e); }
    template <typename T>
    static std::shared_ptr<T> asS(const ast::StmtPtr &s) { return std::dynamic_pointer_cast<T>(s); }

    bool BorrowChecker::check(const ast::StmtPtr &program)
    {
        collectClasses(program);
        pushScope();
        checkStmt(program);
        popScope();
        return !sawError;
    }

    void BorrowChecker::collectClasses(const ast::StmtPtr &stmt)
    {
        if (!stmt) return;
        if (auto blk = asS<ast::BlockStmt>(stmt))
        {
            for (auto &s : blk->statements) collectClasses(s);
        }
        else if (auto mod = asS<ast::ModuleStmt>(stmt))
        {
            for (auto &s : mod->body) collectClasses(s);
        }
        else if (auto cls = asS<ast::ClassStmt>(stmt))
        {
            ownedClasses.insert(cls->name);
        }
    }

    bool BorrowChecker::typeIsOwned(const ast::TypePtr &type) const
    {
        return type && ownedClasses.count(type->toString()) > 0;
    }

    void BorrowChecker::declareOwned(const std::string &name)
    {
        if (!scopes.empty()) scopes.back()[name] = State::Owned;
    }

    BorrowChecker::State *BorrowChecker::find(const std::string &name)
    {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
        {
            auto f = it->find(name);
            if (f != it->end()) return &f->second;
        }
        return nullptr;
    }

    const ast::VariableExpr *BorrowChecker::asBareVar(const ast::ExprPtr &expr)
    {
        ast::ExprPtr cur = expr;
        while (auto g = as<ast::GroupingExpr>(cur)) cur = g->expression;
        if (auto v = as<ast::VariableExpr>(cur)) return v.get();
        return nullptr;
    }

    void BorrowChecker::errMoved(const std::string &name, const lexer::Token &tok)
    {
        sawError = true;
        errorHandler.reportError(error::ErrorCode::B001_USE_AFTER_MOVE,
                                 "use of moved value '" + name + "'", tok,
                                 error::ErrorSeverity::ERROR);
    }

    bool BorrowChecker::isOwnedDecl(ast::VariableStmt *v)
    {
        if (typeIsOwned(v->type))
            return true;
        if (auto call = as<ast::CallExpr>(v->initializer))
            if (auto cv = as<ast::VariableExpr>(call->callee))
                if (ownedClasses.count(cv->name))
                    return true;
        // Initializer is an existing owned local (ownership received via move).
        if (auto bv = asBareVar(v->initializer))
            if (find(bv->name))
                return true;
        return false;
    }

    void BorrowChecker::analyzeFunction(ast::FunctionStmt *fn)
    {
        pushScope();
        for (auto &p : fn->parameters)
            if (typeIsOwned(p.type))
                declareOwned(p.name);
        if (fn->body)
            checkStmt(fn->body);
        popScope();
    }

    void BorrowChecker::checkStmt(const ast::StmtPtr &stmt)
    {
        if (!stmt) return;

        if (auto blk = asS<ast::BlockStmt>(stmt))
        {
            pushScope();
            for (auto &s : blk->statements) checkStmt(s);
            popScope();
            return;
        }
        if (auto mod = asS<ast::ModuleStmt>(stmt))
        {
            for (auto &s : mod->body) checkStmt(s);
            return;
        }
        if (auto fn = asS<ast::FunctionStmt>(stmt))
        {
            analyzeFunction(fn.get());
            return;
        }
        if (auto cls = asS<ast::ClassStmt>(stmt))
        {
            for (auto &m : cls->methods)
                if (auto mfn = asS<ast::FunctionStmt>(m))
                    analyzeFunction(mfn.get());
            return;
        }
        if (auto vs = asS<ast::VariableStmt>(stmt))
        {
            bool owned = isOwnedDecl(vs.get());
            consume(vs->initializer);      // RHS: move a bare owned source, else read
            if (owned) declareOwned(vs->name);
            return;
        }
        if (auto es = asS<ast::ExpressionStmt>(stmt))
        {
            checkValue(es->expression);
            return;
        }
        if (auto rs = asS<ast::ReturnStmt>(stmt))
        {
            consume(rs->value);            // `return x` moves x out
            return;
        }
        if (auto is = asS<ast::IfStmt>(stmt))
        {
            checkValue(is->condition);
            Snapshot s0 = snapshot();
            std::vector<Snapshot> ends;
            checkStmt(is->thenBranch); ends.push_back(snapshot()); restore(s0);
            for (auto &eb : is->elifBranches)
            {
                checkValue(eb.first);
                checkStmt(eb.second);
                ends.push_back(snapshot());
                restore(s0);
            }
            if (is->elseBranch)
            {
                checkStmt(is->elseBranch);
                ends.push_back(snapshot());
                restore(s0);
            }
            else
            {
                ends.push_back(s0); // no-else fall-through keeps the entry state
            }
            mergeMoved(ends);
            return;
        }
        if (auto ws = asS<ast::WhileStmt>(stmt))
        {
            checkValue(ws->condition);
            checkStmt(ws->body);           // single pass; moves persist (conservative)
            return;
        }
        if (auto fs = asS<ast::ForStmt>(stmt))
        {
            checkValue(fs->iterable);
            pushScope();
            if (typeIsOwned(fs->variableType))
                declareOwned(fs->variable);
            checkStmt(fs->body);
            popScope();
            return;
        }
        // try/match/throw/go/select etc. are not move-analyzed in v1 (lenient).
    }

    void BorrowChecker::consume(const ast::ExprPtr &expr)
    {
        if (!expr) return;
        if (const ast::VariableExpr *v = asBareVar(expr))
        {
            State *st = find(v->name);
            if (st)
            {
                if (*st == State::Moved) errMoved(v->name, v->token);
                *st = State::Moved;
            }
            return; // bare var: either moved (owned) or an untracked copy
        }
        checkValue(expr); // non-bare expression in a consuming position: read parts
    }

    void BorrowChecker::checkValue(const ast::ExprPtr &expr)
    {
        if (!expr) return;
        if (auto v = as<ast::VariableExpr>(expr))
        {
            State *st = find(v->name);
            if (st && *st == State::Moved) errMoved(v->name, v->token);
            return;
        }
        if (auto g = as<ast::GroupingExpr>(expr)) { checkValue(g->expression); return; }
        if (auto get = as<ast::GetExpr>(expr)) { checkValue(get->object); return; }
        if (auto idx = as<ast::IndexExpr>(expr)) { checkValue(idx->object); checkValue(idx->index); return; }
        if (auto bin = as<ast::BinaryExpr>(expr)) { checkValue(bin->left); checkValue(bin->right); return; }
        if (auto un = as<ast::UnaryExpr>(expr))
        {
            if (un->op.type == lexer::TokenType::MOVE) consume(un->right);
            else checkValue(un->right);
            return;
        }
        if (auto call = as<ast::CallExpr>(expr))
        {
            checkValue(call->callee);
            for (auto &a : call->arguments) consume(a); // by-value args move owned sources
            return;
        }
        if (auto asg = as<ast::AssignExpr>(expr))
        {
            consume(asg->value);
            if (!asg->name.empty())
            {
                if (State *st = find(asg->name)) *st = State::Owned; // reassignment revives
            }
            else if (asg->target)
            {
                checkValue(asg->target); // obj.f = ... reads obj
            }
            return;
        }
        if (auto si = as<ast::StringInterpolationExpr>(expr))
        {
            for (auto &e : si->getExpressions()) checkValue(e);
            return;
        }
        if (auto aw = as<ast::AwaitExpr>(expr)) { checkValue(aw->expression); return; }
        if (auto lst = as<ast::ListExpr>(expr))
        {
            for (auto &e : lst->elements) consume(e); // stored into the list
            return;
        }
        // Literals and other leaves: nothing to check.
    }

    void BorrowChecker::mergeMoved(const std::vector<Snapshot> &ends)
    {
        // `scopes` currently holds the entry snapshot. Mark a variable Moved if
        // it is Moved at the end of any analyzed path.
        for (const auto &snap : ends)
            for (size_t i = 0; i < scopes.size() && i < snap.size(); ++i)
                for (const auto &kv : snap[i])
                    if (kv.second == State::Moved)
                    {
                        auto it = scopes[i].find(kv.first);
                        if (it != scopes[i].end()) it->second = State::Moved;
                    }
    }
} // namespace type_checker
