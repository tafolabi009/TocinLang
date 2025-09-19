#pragma once

#include "../pch.h"
#include "../lexer/token.h"

namespace ast
{
    // Forward declarations
    class Expr;
    class Stmt;
    class Visitor;
    class TypeVisitor;

    using ExprPtr = std::shared_ptr<Expr>;
    using StmtPtr = std::shared_ptr<Stmt>;
    using ExprList = std::vector<ExprPtr>;
    using StmtList = std::vector<StmtPtr>;

    // Base Type class and common types
    class Type
    {
    public:
        virtual ~Type() = default;
        virtual std::string toString() const = 0;
        virtual void accept(TypeVisitor &visitor) = 0;
    };

    using TypePtr = std::shared_ptr<Type>;

    class PrimitiveType : public Type
    {
    public:
        explicit PrimitiveType(const std::string &name) : name(name) {}
        std::string toString() const override { return name; }
        void accept(TypeVisitor &visitor) override;

        std::string name;
    };

    class GenericType : public Type
    {
    public:
        GenericType(const std::string &name, const std::vector<TypePtr> &typeArguments)
            : name(name), typeArguments(typeArguments) {}

        std::string toString() const override;
        void accept(TypeVisitor &visitor) override;

        std::string name;
        std::vector<TypePtr> typeArguments;
    };

    class FunctionType : public Type
    {
    public:
        FunctionType(const TypePtr &returnType, const std::vector<TypePtr> &paramTypes)
            : returnType(returnType), paramTypes(paramTypes) {}

        std::string toString() const override;
        void accept(TypeVisitor &visitor) override;

        TypePtr returnType;
        std::vector<TypePtr> paramTypes;
    };

    class TraitType : public Type
    {
    public:
        explicit TraitType(const std::string &name) : name(name) {}
        std::string toString() const override { return "trait " + name; }
        void accept(TypeVisitor &visitor) override;

        std::string name;
        std::vector<TypePtr> requiredMethods;
    };

    class UnionType : public Type
    {
    public:
        explicit UnionType(const std::vector<TypePtr> &types) : types(types) {}
        std::string toString() const override;
        void accept(TypeVisitor &visitor) override;

        std::vector<TypePtr> types;
    };

    class OptionalType : public Type
    {
    public:
        explicit OptionalType(const TypePtr &innerType) : innerType(innerType) {}
        std::string toString() const override { return innerType->toString() + "?"; }
        void accept(TypeVisitor &visitor) override;

        TypePtr innerType;
    };

    struct Parameter
    {
        lexer::Token name;
        TypePtr type;
        bool isOptional = false;
        ExprPtr defaultValue = nullptr;
    };

    // Base Expression class
    class Expr
    {
    public:
        virtual ~Expr() = default;
        virtual void accept(Visitor &visitor) = 0;
        TypePtr type; // Type information set during type checking
    };

    class BinaryExpr : public Expr
    {
    public:
        BinaryExpr(ExprPtr left, lexer::Token op, ExprPtr right)
            : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}

        void accept(Visitor &visitor) override;

        ExprPtr left;
        lexer::Token op;
        ExprPtr right;
    };

    class GroupingExpr : public Expr
    {
    public:
        explicit GroupingExpr(ExprPtr expression) : expression(std::move(expression)) {}

        void accept(Visitor &visitor) override;

        ExprPtr expression;
    };

    class LiteralExpr : public Expr
    {
    public:
        explicit LiteralExpr(lexer::Token value) : value(std::move(value)) {}

        void accept(Visitor &visitor) override;

        lexer::Token value;
    };

    class UnaryExpr : public Expr
    {
    public:
        UnaryExpr(lexer::Token op, ExprPtr right) : op(std::move(op)), right(std::move(right)) {}

        void accept(Visitor &visitor) override;

        lexer::Token op;
        ExprPtr right;
    };

    class VariableExpr : public Expr
    {
    public:
        explicit VariableExpr(lexer::Token name) : name(std::move(name)) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
    };

    class AssignExpr : public Expr
    {
    public:
        AssignExpr(lexer::Token name, ExprPtr value) : name(std::move(name)), value(std::move(value)) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
        ExprPtr value;
    };

    class CallExpr : public Expr
    {
    public:
        CallExpr(ExprPtr callee, std::vector<ExprPtr> arguments, lexer::Token paren)
            : callee(std::move(callee)), arguments(std::move(arguments)), paren(std::move(paren)) {}

        void accept(Visitor &visitor) override;

        ExprPtr callee;
        std::vector<ExprPtr> arguments;
        lexer::Token paren;
    };

    class GetExpr : public Expr
    {
    public:
        GetExpr(ExprPtr object, lexer::Token name)
            : object(std::move(object)), name(std::move(name)) {}

        void accept(Visitor &visitor) override;

        ExprPtr object;
        lexer::Token name;
    };

    class SetExpr : public Expr
    {
    public:
        SetExpr(ExprPtr object, lexer::Token name, ExprPtr value)
            : object(std::move(object)), name(std::move(name)), value(std::move(value)) {}

        void accept(Visitor &visitor) override;

        ExprPtr object;
        lexer::Token name;
        ExprPtr value;
    };

    class LambdaExpr : public Expr
    {
    public:
        LambdaExpr(std::vector<Parameter> params, StmtPtr body, TypePtr returnType)
            : params(std::move(params)), body(std::move(body)), returnType(std::move(returnType)) {}

        void accept(Visitor &visitor) override;

        std::vector<Parameter> params;
        StmtPtr body;
        TypePtr returnType;
    };

    class ListExpr : public Expr
    {
    public:
        explicit ListExpr(std::vector<ExprPtr> elements, TypePtr type = nullptr)
            : elements(std::move(elements)), type(std::move(type)) {}

        void accept(Visitor &visitor) override;

        std::vector<ExprPtr> elements;
        TypePtr type; // Type annotation for the list, e.g., list<int>
    };

    class DictionaryExpr : public Expr
    {
    public:
        DictionaryExpr(std::vector<ExprPtr> keys, std::vector<ExprPtr> values, TypePtr type = nullptr)
            : keys(std::move(keys)), values(std::move(values)), type(std::move(type)) {}

        void accept(Visitor &visitor) override;

        std::vector<ExprPtr> keys;
        std::vector<ExprPtr> values;
        TypePtr type; // Type annotation for the dictionary, e.g., dict<string, int>
    };

    class AwaitExpr : public Expr
    {
    public:
        explicit AwaitExpr(ExprPtr expression) : expression(std::move(expression)) {}

        void accept(Visitor &visitor) override;

        ExprPtr expression;
    };

    class NewExpr : public Expr
    {
    public:
        NewExpr(ExprPtr typeExpr, ExprPtr sizeExpr = nullptr,
                std::vector<ExprPtr> *arguments = nullptr)
            : typeExpr(std::move(typeExpr)), sizeExpr(std::move(sizeExpr)),
              arguments(arguments) {}

        void accept(Visitor &visitor) override;

        ExprPtr getTypeExpr() const { return typeExpr; }
        ExprPtr getSizeExpr() const { return sizeExpr; }
        std::vector<ExprPtr> *getArguments() const { return arguments; }

    private:
        ExprPtr typeExpr;                // Type being allocated
        ExprPtr sizeExpr;                // Optional size for arrays
        std::vector<ExprPtr> *arguments; // Optional constructor arguments
    };

    class DeleteExpr : public Expr
    {
    public:
        explicit DeleteExpr(ExprPtr expr) : expr(std::move(expr)) {}

        void accept(Visitor &visitor) override;

        ExprPtr getExpr() const { return expr; }

    private:
        ExprPtr expr;
    };

    class PromiseExpr : public Expr
    {
    public:
        explicit PromiseExpr(ExprPtr value) : value(std::move(value)) {}

        void accept(Visitor &visitor) override;

        ExprPtr value;
    };

    class SpreadExpr : public Expr
    {
    public:
        explicit SpreadExpr(ExprPtr expression) : expression(std::move(expression)) {}

        void accept(Visitor &visitor) override;

        ExprPtr expression;
    };

    class YieldExpr : public Expr
    {
    public:
        explicit YieldExpr(ExprPtr expression) : expression(std::move(expression)) {}

        void accept(Visitor &visitor) override;

        ExprPtr expression;
    };

    class AsyncArrowExpr : public Expr
    {
    public:
        AsyncArrowExpr(std::vector<Parameter> params, StmtPtr body, TypePtr returnType)
            : params(std::move(params)), body(std::move(body)), returnType(std::move(returnType)) {}

        void accept(Visitor &visitor) override;

        std::vector<Parameter> params;
        StmtPtr body;
        TypePtr returnType;
    };

    // Base Statement class
    class Stmt
    {
    public:
        virtual ~Stmt() = default;
        virtual void accept(Visitor &visitor) = 0;
    };

    class BlockStmt : public Stmt
    {
    public:
        explicit BlockStmt(std::vector<StmtPtr> statements) : statements(std::move(statements)) {}

        void accept(Visitor &visitor) override;

        std::vector<StmtPtr> statements;
    };

    class ExpressionStmt : public Stmt
    {
    public:
        explicit ExpressionStmt(ExprPtr expression) : expression(std::move(expression)) {}

        void accept(Visitor &visitor) override;

        ExprPtr expression;
    };

    class VariableStmt : public Stmt
    {
    public:
        VariableStmt(lexer::Token name, ExprPtr initializer, TypePtr type = nullptr, bool isConst = false)
            : name(std::move(name)), initializer(std::move(initializer)),
              type(std::move(type)), isConst(isConst) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
        ExprPtr initializer;
        TypePtr type;
        bool isConst;
    };

    class FunctionStmt : public Stmt
    {
    public:
        FunctionStmt(lexer::Token name, std::vector<Parameter> params,
                     std::vector<StmtPtr> body, TypePtr returnType, bool isAsync = false)
            : name(std::move(name)), params(std::move(params)),
              body(std::make_shared<BlockStmt>(std::move(body))),
              returnType(std::move(returnType)), isAsync(isAsync) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
        std::vector<Parameter> params;
        std::shared_ptr<BlockStmt> body;
        TypePtr returnType;
        bool isAsync;
    };

    class ReturnStmt : public Stmt
    {
    public:
        ReturnStmt(lexer::Token keyword, ExprPtr value = nullptr)
            : keyword(std::move(keyword)), value(std::move(value)) {}

        void accept(Visitor &visitor) override;

        lexer::Token keyword;
        ExprPtr value;
    };

    class ClassStmt : public Stmt
    {
    public:
        ClassStmt(lexer::Token name, ExprPtr superclass, std::vector<StmtPtr> members)
            : name(std::move(name)), superclass(std::move(superclass)),
              members(std::move(members)) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
        ExprPtr superclass;
        std::vector<StmtPtr> members;
    };

    class IfStmt : public Stmt
    {
    public:
        IfStmt(ExprPtr condition, StmtPtr thenBranch, StmtPtr elseBranch = nullptr)
            : condition(std::move(condition)), thenBranch(std::move(thenBranch)),
              elseBranch(std::move(elseBranch)) {}

        void accept(Visitor &visitor) override;

        ExprPtr condition;
        StmtPtr thenBranch;
        StmtPtr elseBranch;
    };

    class WhileStmt : public Stmt
    {
    public:
        WhileStmt(ExprPtr condition, StmtPtr body)
            : condition(std::move(condition)), body(std::move(body)) {}

        void accept(Visitor &visitor) override;

        ExprPtr condition;
        StmtPtr body;
    };

    class ForStmt : public Stmt
    {
    public:
        ForStmt(StmtPtr initializer, ExprPtr condition, ExprPtr increment, StmtPtr body)
            : initializer(std::move(initializer)), condition(std::move(condition)),
              increment(std::move(increment)), body(std::move(body)) {}

        void accept(Visitor &visitor) override;

        StmtPtr initializer;
        ExprPtr condition;
        ExprPtr increment;
        StmtPtr body;
    };

    class MatchStmt : public Stmt
    {
    public:
        struct Case
        {
            ExprPtr pattern;
            StmtPtr body;
        };

        MatchStmt(ExprPtr value, std::vector<Case> cases)
            : value(std::move(value)), cases(std::move(cases)) {}

        void accept(Visitor &visitor) override;

        ExprPtr value;
        std::vector<Case> cases;
    };

    class ImportStmt : public Stmt
    {
    public:
        ImportStmt(lexer::Token path, std::vector<lexer::Token> symbols, lexer::Token alias = lexer::Token())
            : path(std::move(path)), symbols(std::move(symbols)), alias(std::move(alias)) {}

        void accept(Visitor &visitor) override;

        lexer::Token path;
        std::vector<lexer::Token> symbols;
        lexer::Token alias;
    };

    class TraitStmt : public Stmt
    {
    public:
        TraitStmt(lexer::Token name, std::vector<StmtPtr> methods)
            : name(std::move(name)), methods(std::move(methods)) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
        std::vector<StmtPtr> methods; // Only function declarations
    };

    class ImplementStmt : public Stmt
    {
    public:
        ImplementStmt(lexer::Token traitName, lexer::Token className, std::vector<StmtPtr> methods)
            : traitName(std::move(traitName)), className(std::move(className)),
              methods(std::move(methods)) {}

        void accept(Visitor &visitor) override;

        lexer::Token traitName;
        lexer::Token className;
        std::vector<StmtPtr> methods;
    };

    class AsyncFunctionStmt : public Stmt
    {
    public:
        AsyncFunctionStmt(lexer::Token name, std::vector<Parameter> params,
                          std::vector<StmtPtr> body, TypePtr returnType)
            : name(std::move(name)), params(std::move(params)),
              body(std::make_shared<BlockStmt>(std::move(body))),
              returnType(std::move(returnType)) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
        std::vector<Parameter> params;
        std::shared_ptr<BlockStmt> body;
        TypePtr returnType;
    };

    class GeneratorFunctionStmt : public Stmt
    {
    public:
        GeneratorFunctionStmt(lexer::Token name, std::vector<Parameter> params,
                              std::vector<StmtPtr> body, TypePtr yieldType)
            : name(std::move(name)), params(std::move(params)),
              body(std::make_shared<BlockStmt>(std::move(body))),
              yieldType(std::move(yieldType)) {}

        void accept(Visitor &visitor) override;

        lexer::Token name;
        std::vector<Parameter> params;
        std::shared_ptr<BlockStmt> body;
        TypePtr yieldType;
    };

    // Visitor pattern for expressions and statements
    class Visitor
    {
    public:
        virtual ~Visitor() = default;

        // Statements
        virtual void visitBlockStmt(BlockStmt *stmt) = 0;
        virtual void visitExpressionStmt(ExpressionStmt *stmt) = 0;
        virtual void visitVariableStmt(VariableStmt *stmt) = 0;
        virtual void visitFunctionStmt(FunctionStmt *stmt) = 0;
        virtual void visitReturnStmt(ReturnStmt *stmt) = 0;
        virtual void visitClassStmt(ClassStmt *stmt) = 0;
        virtual void visitIfStmt(IfStmt *stmt) = 0;
        virtual void visitWhileStmt(WhileStmt *stmt) = 0;
        virtual void visitForStmt(ForStmt *stmt) = 0;
        virtual void visitMatchStmt(MatchStmt *stmt) = 0;
        virtual void visitImportStmt(ImportStmt *stmt) = 0;
        virtual void visitTraitStmt(TraitStmt *stmt) { /* Default implementation */ }
        virtual void visitImplementStmt(ImplementStmt *stmt) { /* Default implementation */ }
        virtual void visitAsyncFunctionStmt(AsyncFunctionStmt *stmt) { /* Default implementation */ }
        virtual void visitGeneratorFunctionStmt(GeneratorFunctionStmt *stmt) { /* Default implementation */ }

        // Expressions
        virtual void visitBinaryExpr(BinaryExpr *expr) = 0;
        virtual void visitGroupingExpr(GroupingExpr *expr) = 0;
        virtual void visitLiteralExpr(LiteralExpr *expr) = 0;
        virtual void visitUnaryExpr(UnaryExpr *expr) = 0;
        virtual void visitVariableExpr(VariableExpr *expr) = 0;
        virtual void visitAssignExpr(AssignExpr *expr) = 0;
        virtual void visitCallExpr(CallExpr *expr) = 0;
        virtual void visitGetExpr(GetExpr *expr) = 0;
        virtual void visitSetExpr(SetExpr *expr) = 0;
        virtual void visitListExpr(ListExpr *expr) = 0;
        virtual void visitDictionaryExpr(DictionaryExpr *expr) = 0;
        virtual void visitLambdaExpr(LambdaExpr *expr) = 0;
        virtual void visitAwaitExpr(AwaitExpr *expr) = 0;
        virtual void visitNewExpr(NewExpr *expr) = 0;
        virtual void visitDeleteExpr(DeleteExpr *expr) = 0;
        virtual void visitPromiseExpr(PromiseExpr *expr) { /* Default implementation */ }
        virtual void visitSpreadExpr(SpreadExpr *expr) { /* Default implementation */ }
        virtual void visitYieldExpr(YieldExpr *expr) { /* Default implementation */ }
        virtual void visitAsyncArrowExpr(AsyncArrowExpr *expr) { /* Default implementation */ }
    };

    // Type visitor for type checking
    class TypeVisitor
    {
    public:
        virtual ~TypeVisitor() = default;
        virtual void visitPrimitiveType(PrimitiveType *type) = 0;
        virtual void visitGenericType(GenericType *type) = 0;
        virtual void visitFunctionType(FunctionType *type) = 0;
        virtual void visitUnionType(UnionType *type) = 0;
        virtual void visitTraitType(TraitType *type) { /* Default implementation */ }
        virtual void visitOptionalType(OptionalType *type) { /* Default implementation */ }
    };

} // namespace ast
