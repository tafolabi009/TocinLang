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
        if (!scopes.empty()) scopes.back()[name] = VarInfo{};
    }

    BorrowChecker::VarInfo *BorrowChecker::find(const std::string &name)
    {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
        {
            auto f = it->find(name);
            if (f != it->end()) return &f->second;
        }
        return nullptr;
    }

    // Releasing a scope returns the borrows its bindings held back to the
    // borrowed variables (lexical lifetimes: a borrow ends when its reference
    // goes out of scope).
    void BorrowChecker::popScope()
    {
        if (scopes.empty()) return;
        ScopeMap dying = scopes.back();
        scopes.pop_back();
        for (auto &kv : dying)
        {
            if (kv.second.borrowOf.empty()) continue;
            if (VarInfo *t = find(kv.second.borrowOf))
            {
                if (kv.second.borrowMut) t->mut = false;
                else if (t->shared > 0) t->shared--;
            }
        }
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

    void BorrowChecker::errBorrow(const std::string &msg, const lexer::Token &tok)
    {
        sawError = true;
        errorHandler.reportError(error::ErrorCode::B002_BORROW_CONFLICT,
                                 msg, tok, error::ErrorSeverity::ERROR);
    }

    bool BorrowChecker::hasLiveBorrow(const std::string &name)
    {
        VarInfo *v = find(name);
        return v && (v->shared > 0 || v->mut);
    }

    // Recognise `&x` / `&mut x` (possibly parenthesised). Sets isMut/target.
    bool BorrowChecker::isBorrow(const ast::ExprPtr &expr, bool &isMut,
                                 const ast::VariableExpr *&target)
    {
        ast::ExprPtr cur = expr;
        while (auto g = as<ast::GroupingExpr>(cur)) cur = g->expression;
        auto u = as<ast::UnaryExpr>(cur);
        if (!u) return false;
        if (u->op.type == lexer::TokenType::BORROW) isMut = false;
        else if (u->op.type == lexer::TokenType::MUTABLE_BORROW) isMut = true;
        else return false;
        target = asBareVar(u->right);
        return target != nullptr;
    }

    // `let r = &x` / `let r = &mut x`: record the borrow on x and bind r to it.
    void BorrowChecker::takeBorrow(const std::string &refName,
                                   const ast::VariableExpr *target, bool isMut)
    {
        VarInfo *t = find(target->name);
        if (!t)
        {
            // Borrowing a non-owned (Copy) value carries no aliasing hazard;
            // still declare the reference binding so its scope is tracked.
            if (!scopes.empty()) scopes.back()[refName] = VarInfo{};
            return;
        }
        if (t->state == State::Moved)
            errMoved(target->name, target->token);
        if (isMut)
        {
            if (t->mut || t->shared > 0)
                errBorrow("cannot borrow '" + target->name +
                              "' as mutable: it is already borrowed",
                          target->token);
            else
                t->mut = true;
        }
        else
        {
            if (t->mut)
                errBorrow("cannot borrow '" + target->name +
                              "' as shared: it is already mutably borrowed",
                          target->token);
            else
                t->shared++;
        }
        VarInfo ref;
        ref.borrowOf = target->name;
        ref.borrowMut = isMut;
        if (!scopes.empty()) scopes.back()[refName] = ref;
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
            // `let r = &x` / `let r = &mut x` records a borrow, not a move.
            bool isMut = false;
            const ast::VariableExpr *tgt = nullptr;
            if (isBorrow(vs->initializer, isMut, tgt))
            {
                takeBorrow(vs->name, tgt, isMut);
                return;
            }
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
        // A borrow in a consuming position is not a move of the borrowed var;
        // validate it as a value (checks borrow conflicts) and stop.
        bool isMut = false;
        const ast::VariableExpr *bt = nullptr;
        if (isBorrow(expr, isMut, bt))
        {
            checkValue(expr);
            return;
        }
        if (const ast::VariableExpr *v = asBareVar(expr))
        {
            VarInfo *st = find(v->name);
            if (st)
            {
                if (st->state == State::Moved)
                    errMoved(v->name, v->token);
                else if (st->shared > 0 || st->mut)
                    errBorrow("cannot move '" + v->name + "' while it is borrowed",
                              v->token);
                st->state = State::Moved;
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
            VarInfo *st = find(v->name);
            if (st)
            {
                if (st->state == State::Moved)
                    errMoved(v->name, v->token);
                else if (st->mut)
                    errBorrow("cannot use '" + v->name +
                                  "' while it is mutably borrowed",
                              v->token);
            }
            return;
        }
        if (auto g = as<ast::GroupingExpr>(expr)) { checkValue(g->expression); return; }
        if (auto get = as<ast::GetExpr>(expr)) { checkValue(get->object); return; }
        if (auto idx = as<ast::IndexExpr>(expr)) { checkValue(idx->object); checkValue(idx->index); return; }
        if (auto bin = as<ast::BinaryExpr>(expr)) { checkValue(bin->left); checkValue(bin->right); return; }
        if (auto un = as<ast::UnaryExpr>(expr))
        {
            if (un->op.type == lexer::TokenType::MOVE) { consume(un->right); return; }
            if (un->op.type == lexer::TokenType::BORROW ||
                un->op.type == lexer::TokenType::MUTABLE_BORROW)
            {
                // A temporary borrow (e.g. `f(&mut a)`): check for a conflict at
                // the borrow site. It is released immediately (no binding holds
                // it), so no lasting borrow is recorded.
                bool isMut = un->op.type == lexer::TokenType::MUTABLE_BORROW;
                if (const ast::VariableExpr *t = asBareVar(un->right))
                {
                    VarInfo *vi = find(t->name);
                    if (vi)
                    {
                        if (vi->state == State::Moved)
                            errMoved(t->name, t->token);
                        else if (isMut && (vi->mut || vi->shared > 0))
                            errBorrow("cannot borrow '" + t->name +
                                          "' as mutable: it is already borrowed",
                                      t->token);
                        else if (!isMut && vi->mut)
                            errBorrow("cannot borrow '" + t->name +
                                          "' as shared: it is already mutably borrowed",
                                      t->token);
                    }
                }
                else
                {
                    checkValue(un->right);
                }
                return;
            }
            checkValue(un->right);
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
                if (VarInfo *st = find(asg->name))
                {
                    if (st->shared > 0 || st->mut)
                        errBorrow("cannot assign to '" + asg->name +
                                      "' while it is borrowed",
                                  asg->token);
                    st->state = State::Owned; // reassignment revives
                }
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
                    if (kv.second.state == State::Moved)
                    {
                        auto it = scopes[i].find(kv.first);
                        if (it != scopes[i].end()) it->second.state = State::Moved;
                    }
    }
} // namespace type_checker
