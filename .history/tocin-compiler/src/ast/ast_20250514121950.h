#ifndef AST_H
#define AST_H

#include "../lexer/token.h"
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <map>
#include "types.h"

/**
 * @brief Namespace for abstract syntax tree (AST) definitions.
 */
namespace ast
{
    // Forward declarations for all classes
    class Node;
    class Expression;
    class Statement;
    class BinaryExpr;
    class GroupingExpr;
    class LiteralExpr;
    class UnaryExpr;
    class VariableExpr;
    class AssignExpr;
    class CallExpr;
    class GetExpr;
    class SetExpr;
    class ListExpr;
    class DictionaryExpr;
    class LambdaExpr;
    class AwaitExpr;
    class ExpressionStmt;
    class VariableStmt;
    class BlockStmt;
    class IfStmt;
    class WhileStmt;
    class ForStmt;
    class FunctionStmt;
    class ReturnStmt;
    class ClassStmt;
    class ImportStmt;
    class MatchStmt;
    class NewExpr;
    class DeleteExpr;
    class ExportStmt;
    class ModuleStmt;
    class StringInterpolationExpr;
    class TraitType;
    class ChannelSendExpr;
    class ChannelReceiveExpr;
    class SelectStmt;

    // Define shared pointers for common types - Type is already defined in types.h
    using ExprPtr = std::shared_ptr<Expression>;
    using StmtPtr = std::shared_ptr<Statement>;

    /**
     * @brief Visitor interface for traversing the AST.
     */
    class Visitor
    {
    public:
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
        virtual void visitExpressionStmt(ExpressionStmt *stmt) = 0;
        virtual void visitVariableStmt(VariableStmt *stmt) = 0;
        virtual void visitBlockStmt(BlockStmt *stmt) = 0;
        virtual void visitIfStmt(IfStmt *stmt) = 0;
        virtual void visitWhileStmt(WhileStmt *stmt) = 0;
        virtual void visitForStmt(ForStmt *stmt) = 0;
        virtual void visitFunctionStmt(FunctionStmt *stmt) = 0;
        virtual void visitReturnStmt(ReturnStmt *stmt) = 0;
        virtual void visitClassStmt(ClassStmt *stmt) = 0;
        virtual void visitImportStmt(ImportStmt *stmt) = 0;
        virtual void visitMatchStmt(MatchStmt *stmt) = 0;
        virtual void visitNewExpr(NewExpr *expr) = 0;
        virtual void visitDeleteExpr(DeleteExpr *expr) = 0;
        virtual void visitExportStmt(ExportStmt *stmt) = 0;
        virtual void visitModuleStmt(ModuleStmt *stmt) = 0;
        virtual void visitStringInterpolationExpr(StringInterpolationExpr *expr) = 0;
        virtual void visitChannelSendExpr(ChannelSendExpr *expr) = 0;
        virtual void visitChannelReceiveExpr(ChannelReceiveExpr *expr) = 0;
        virtual void visitSelectStmt(SelectStmt *stmt) = 0;
        virtual ~Visitor() = default;
    };

    /**
     * @brief Represents a generic type parameter with optional constraints.
     */
    class TypeParameter
    {
    public:
        TypeParameter(const lexer::Token &token, const std::string &name)
            : token(token), name(name), constraints() {}

        TypeParameter(const lexer::Token &token, const std::string &name, std::vector<std::shared_ptr<TraitType>> constraints)
            : token(token), name(name), constraints(std::move(constraints)) {}

        const std::string &getName() const { return name; }
        const std::vector<std::shared_ptr<TraitType>> &getConstraints() const { return constraints; }
        const lexer::Token &getToken() const { return token; }

    private:
        lexer::Token token;
        std::string name;
        std::vector<std::shared_ptr<TraitType>> constraints;
    };

    /**
     * @brief Base class for all AST nodes.
     */
    class Node
    {
    public:
        explicit Node(const lexer::Token &token) : token(token) {}
        virtual ~Node() = default;
        lexer::Token token;
    };

    /**
     * @brief Simple type (e.g., int, string).
     */
    class SimpleType : public Type
    {
    public:
        SimpleType(const lexer::Token &token) : Type(token) {}
        std::string toString() const override { return token.value; }
    };

    /**
     * @brief Generic type (e.g., list<int>).
     */
    class GenericType : public Type
    {
    public:
        GenericType(const lexer::Token &token, const std::string &name, std::vector<TypePtr> typeArguments)
            : Type(token), name(name), typeArguments(std::move(typeArguments))
        {
            if (typeArguments.empty())
            {
                throw std::runtime_error("GenericType requires non-empty type arguments");
            }
        }
        std::string toString() const override
        {
            std::string result = name + "<";
            for (size_t i = 0; i < typeArguments.size(); ++i)
            {
                result += typeArguments[i]->toString();
                if (i < typeArguments.size() - 1)
                    result += ", ";
            }
            return result + ">";
        }
        std::string name;
        std::vector<TypePtr> typeArguments;
    };

    /**
     * @brief Type parameter reference (for use inside generic functions/classes)
     */
    class TypeParameterType : public Type
    {
    public:
        TypeParameterType(const lexer::Token &token, const std::string &name)
            : Type(token), name(name) {}

        std::string toString() const override { return name; }
        const std::string &getName() const { return name; }

    private:
        std::string name;
    };

    /**
     * @brief Trait type for constraints (e.g., T: Comparable)
     */
    class TraitType : public Type
    {
    public:
        TraitType(const lexer::Token &token, const std::string &name)
            : Type(token), name(name) {}

        std::string toString() const override { return name; }
        const std::string &getName() const { return name; }

    private:
        std::string name;
    };

    /**
     * @brief Function type (e.g., (int, string) -> bool).
     */
    class FunctionType : public Type
    {
    public:
        FunctionType(const lexer::Token &token, std::vector<TypePtr> paramTypes, TypePtr returnType)
            : Type(token), paramTypes(std::move(paramTypes)), returnType(std::move(returnType))
        {
            if (!returnType)
            {
                throw std::runtime_error("FunctionType returnType cannot be null");
            }
        }
        std::string toString() const override
        {
            std::string result = "(";
            for (size_t i = 0; i < paramTypes.size(); ++i)
            {
                result += paramTypes[i]->toString();
                if (i < paramTypes.size() - 1)
                    result += ", ";
            }
            return result + ") -> " + returnType->toString();
        }
        std::vector<TypePtr> paramTypes;
        TypePtr returnType;
    };

    /**
     * @brief Union type (e.g., int | string).
     */
    class UnionType : public Type
    {
    public:
        UnionType(const lexer::Token &token, std::vector<TypePtr> types)
            : Type(token), types(std::move(types))
        {
            if (types.empty())
            {
                throw std::runtime_error("UnionType requires non-empty types");
            }
        }
        std::string toString() const override
        {
            std::string result;
            for (size_t i = 0; i < types.size(); ++i)
            {
                result += types[i]->toString();
                if (i < types.size() - 1)
                    result += " | ";
            }
            return result;
        }
        std::vector<TypePtr> types;
    };

    /**
     * @brief Class type (e.g., MyClass).
     */
    class ClassType : public Type
    {
    public:
        ClassType(const lexer::Token &token, const std::string &name)
            : Type(token), name(name) {}

        std::string toString() const override { return name; }
        std::string name;
    };

    /**
     * @brief Parameter definition for functions and lambdas.
     */
    class Parameter
    {
    public:
        Parameter(const std::string &name, TypePtr type)
            : name(name), type(std::move(type)), isMoved(false)
        {
            if (!type)
            {
                throw std::runtime_error("Parameter type cannot be null");
            }
        }

        // Constructor with token param
        Parameter(const lexer::Token &token, const std::string &name, TypePtr type)
            : name(name), type(std::move(type)), isMoved(false)
        {
            if (!type)
            {
                throw std::runtime_error("Parameter type cannot be null");
            }
        }

        std::string name;
        TypePtr type;
        bool isMoved;
    };

    // Base class for expressions.
    class Expression : public Node
    {
    public:
        explicit Expression(const lexer::Token &token) : Node(token) {}
        virtual void accept(Visitor &visitor) = 0;
        virtual TypePtr getType() const = 0;
    };

    // Base class for statements.
    class Statement : public Node
    {
    public:
        explicit Statement(const lexer::Token &token) : Node(token) {}
        virtual void accept(Visitor &visitor) = 0;
    };

    /**
     * @brief Binary expression (e.g., a + b).
     */
    class BinaryExpr : public Expression
    {
    public:
        BinaryExpr(const lexer::Token &token, ExprPtr left, lexer::Token op, ExprPtr right)
            : Expression(token), left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
        void accept(Visitor &visitor) override;
        ExprPtr left;
        lexer::Token op;
        ExprPtr right;
        TypePtr getType() const override { return nullptr; }
    };

    /**
     * @brief Grouping expression (e.g., (expr)).
     */
    class GroupingExpr : public Expression
    {
    public:
        GroupingExpr(const lexer::Token &token, ExprPtr expression)
            : Expression(token), expression(std::move(expression)) {}
        void accept(Visitor &visitor) override;
        ExprPtr expression;
        TypePtr getType() const override { return expression ? expression->getType() : nullptr; }
    };

    /**
     * @brief Literal expression (e.g., 42, "hello").
     */
    class LiteralExpr : public Expression
    {
    public:
        enum class LiteralType
        {
            INTEGER,
            FLOAT,
            BOOLEAN,
            STRING,
            NIL
        };
        LiteralExpr(const lexer::Token &token, const std::string &value, LiteralType literalType)
            : Expression(token), value(value), literalType(literalType) {}
        void accept(Visitor &visitor) override;
        std::string value;
        LiteralType literalType;
        TypePtr getType() const override { return nullptr; }
    };

    /**
     * @brief Unary expression (e.g., -x).
     */
    class UnaryExpr : public Expression
    {
    public:
        UnaryExpr(const lexer::Token &token, lexer::Token op, ExprPtr right)
            : Expression(token), op(std::move(op)), right(std::move(right)) {}
        void accept(Visitor &visitor) override;
        lexer::Token op;
        ExprPtr right;
        TypePtr getType() const override { return right ? right->getType() : nullptr; }
    };

    /**
     * @brief Variable expression (e.g., x).
     */
    class VariableExpr : public Expression
    {
    public:
        VariableExpr(const lexer::Token &token, const std::string &name)
            : Expression(token), name(name) {}
        void accept(Visitor &visitor) override;
        std::string name;
        TypePtr getType() const override { return nullptr; }
    };

    /**
     * @brief Assignment expression (e.g., x = 5).
     */
    class AssignExpr : public Expression
    {
    public:
        // Legacy constructor for simple variable assignments
        AssignExpr(const lexer::Token &token, const std::string &name, ExprPtr value)
            : Expression(token), name(name), value(std::move(value)), target(std::make_shared<VariableExpr>(token, name)) {}

        // New constructor for extended assignments (obj.prop = val, arr[i] = val, etc.)
        AssignExpr(const lexer::Token &token, ExprPtr target, ExprPtr value)
            : Expression(token), name(""), target(std::move(target)), value(std::move(value)) {}

        void accept(Visitor &visitor) override;
        std::string name; // Kept for backward compatibility
        ExprPtr target;   // New target expression (VariableExpr, GetExpr, etc.)
        ExprPtr value;
        TypePtr getType() const override { return value ? value->getType() : nullptr; }

        // Helper method to check if this is a traditional variable assignment
        bool isVariableAssignment() const { return !name.empty(); }
    };

    /**
     * @brief Function call expression (e.g., foo(1, 2)).
     */
    class CallExpr : public Expression
    {
    public:
        CallExpr(const lexer::Token &token, ExprPtr callee, std::vector<ExprPtr> arguments)
            : Expression(token), callee(std::move(callee)), arguments(std::move(arguments)) {}
        void accept(Visitor &visitor) override;
        ExprPtr callee;
        std::vector<ExprPtr> arguments;
        TypePtr getType() const override { return callee ? callee->getType() : nullptr; }
    };

    /**
     * @brief Field access expression (e.g., obj.field).
     */
    class GetExpr : public Expression
    {
    public:
        GetExpr(const lexer::Token &token, ExprPtr object, const std::string &name)
            : Expression(token), object(std::move(object)), name(name) {}
        void accept(Visitor &visitor) override;
        ExprPtr object;
        std::string name;
        TypePtr getType() const override { return object ? object->getType() : nullptr; }
    };

    /**
     * @brief Field set expression (e.g., obj.field = value).
     */
    class SetExpr : public Expression
    {
    public:
        SetExpr(const lexer::Token &token, ExprPtr object, const std::string &name, ExprPtr value)
            : Expression(token), object(std::move(object)), name(name), value(std::move(value)) {}
        void accept(Visitor &visitor) override;
        ExprPtr object;
        std::string name;
        ExprPtr value;
        TypePtr getType() const override { return value ? value->getType() : nullptr; }
    };

    /**
     * @brief List expression (e.g., [1, 2, 3]).
     */
    class ListExpr : public Expression
    {
    public:
        ListExpr(const lexer::Token &token, std::vector<ExprPtr> elements)
            : Expression(token), elements(std::move(elements)) {}
        void accept(Visitor &visitor) override;
        std::vector<ExprPtr> elements;
        TypePtr getType() const override { return nullptr; }
    };

    /**
     * @brief Dictionary expression (e.g., {"key": value}).
     */
    class DictionaryExpr : public Expression
    {
    public:
        DictionaryExpr(const lexer::Token &token, std::vector<std::pair<ExprPtr, ExprPtr>> entries)
            : Expression(token), entries(std::move(entries)) {}
        void accept(Visitor &visitor) override;
        std::vector<std::pair<ExprPtr, ExprPtr>> entries;
        TypePtr getType() const override { return nullptr; }
    };

    /**
     * @brief Lambda expression (e.g., lambda x: x + 1).
     */
    class LambdaExpr : public Expression
    {
    public:
        LambdaExpr(const lexer::Token &token, std::vector<Parameter> parameters, TypePtr returnType, ExprPtr body)
            : Expression(token), parameters(std::move(parameters)), returnType(std::move(returnType)), body(std::move(body))
        {
            if (!returnType)
            {
                throw std::runtime_error("LambdaExpr returnType cannot be null");
            }
        }
        void accept(Visitor &visitor) override;
        std::vector<Parameter> parameters;
        TypePtr returnType;
        ExprPtr body;
        TypePtr getType() const override { return returnType; }
    };

    /**
     * @brief Await expression (e.g., await expr).
     */
    class AwaitExpr : public Expression
    {
    public:
        AwaitExpr(const lexer::Token &token, ExprPtr expression)
            : Expression(token), expression(std::move(expression)) {}
        void accept(Visitor &visitor) override;
        ExprPtr expression;
        TypePtr getType() const override { return expression ? expression->getType() : nullptr; }
    };

    /**
     * @brief Expression statement (e.g., expr;).
     */
    class ExpressionStmt : public Statement
    {
    public:
        ExpressionStmt(const lexer::Token &token, ExprPtr expression)
            : Statement(token), expression(std::move(expression)) {}
        void accept(Visitor &visitor) override;
        ExprPtr expression;
    };

    /**
     * @brief Variable declaration statement (e.g., let x: int = 5).
     */
    class VariableStmt : public Statement
    {
    public:
        VariableStmt(const lexer::Token &token, const std::string &name, TypePtr type, ExprPtr initializer, bool isConstant)
            : Statement(token), name(name), type(std::move(type)), initializer(std::move(initializer)), isConstant(isConstant) {}
        void accept(Visitor &visitor) override;
        std::string name;
        TypePtr type;
        ExprPtr initializer;
        bool isConstant;
    };

    /**
     * @brief Block statement (e.g., { stmt1; stmt2; }).
     */
    class BlockStmt : public Statement
    {
    public:
        BlockStmt(const lexer::Token &token, std::vector<StmtPtr> statements)
            : Statement(token), statements(std::move(statements)) {}
        void accept(Visitor &visitor) override;
        std::vector<StmtPtr> statements;
    };

    /**
     * @brief If statement (e.g., if cond { ... } else { ... }).
     */
    class IfStmt : public Statement
    {
    public:
        IfStmt(const lexer::Token &token, ExprPtr condition, StmtPtr thenBranch,
               std::vector<std::pair<ExprPtr, StmtPtr>> elifBranches, StmtPtr elseBranch)
            : Statement(token), condition(std::move(condition)), thenBranch(std::move(thenBranch)),
              elifBranches(std::move(elifBranches)), elseBranch(std::move(elseBranch)) {}
        void accept(Visitor &visitor) override;
        ExprPtr condition;
        StmtPtr thenBranch;
        std::vector<std::pair<ExprPtr, StmtPtr>> elifBranches;
        StmtPtr elseBranch;
    };

    /**
     * @brief While statement (e.g., while cond { ... }).
     */
    class WhileStmt : public Statement
    {
    public:
        WhileStmt(const lexer::Token &token, ExprPtr condition, StmtPtr body)
            : Statement(token), condition(std::move(condition)), body(std::move(body)) {}
        void accept(Visitor &visitor) override;
        ExprPtr condition;
        StmtPtr body;
    };

    /**
     * @brief For statement (e.g., for x in iterable { ... }).
     */
    class ForStmt : public Statement
    {
    public:
        ForStmt(const lexer::Token &token, const std::string &variable, TypePtr variableType, ExprPtr iterable, StmtPtr body)
            : Statement(token), variable(variable), variableType(std::move(variableType)),
              iterable(std::move(iterable)), body(std::move(body)) {}
        void accept(Visitor &visitor) override;
        std::string variable;
        TypePtr variableType;
        ExprPtr iterable;
        StmtPtr body;
    };

    /**
     * @brief Function declaration statement (e.g., def foo<T: Comparable>(x: T) -> T { ... }).
     */
    class FunctionStmt : public Statement
    {
    public:
        FunctionStmt(const lexer::Token &token, const std::string &name, std::vector<Parameter> parameters,
                     TypePtr returnType, StmtPtr body, bool isAsync)
            : Statement(token), name(name), typeParameters(), parameters(std::move(parameters)),
              returnType(std::move(returnType)), body(std::move(body)), isAsync(isAsync) {}

        // Constructor with generic type parameters
        FunctionStmt(const lexer::Token &token, const std::string &name,
                     std::vector<TypeParameter> typeParameters,
                     std::vector<Parameter> parameters,
                     TypePtr returnType, StmtPtr body, bool isAsync)
            : Statement(token), name(name), typeParameters(std::move(typeParameters)),
              parameters(std::move(parameters)), returnType(std::move(returnType)),
              body(std::move(body)), isAsync(isAsync) {}

        void accept(Visitor &visitor) override;

        std::string name;
        std::vector<TypeParameter> typeParameters; // Generic type parameters
        std::vector<Parameter> parameters;
        TypePtr returnType;
        StmtPtr body;
        bool isAsync;

        bool isGeneric() const { return !typeParameters.empty(); }
    };

    /**
     * @brief Return statement (e.g., return expr).
     */
    class ReturnStmt : public Statement
    {
    public:
        ReturnStmt(const lexer::Token &token, ExprPtr value)
            : Statement(token), value(std::move(value)) {}
        void accept(Visitor &visitor) override;
        ExprPtr value;
    };

    /**
     * @brief Class declaration statement (e.g., class C<T> { ... }).
     */
    class ClassStmt : public Statement
    {
    public:
        ClassStmt(const lexer::Token &token, const std::string &name,
                  std::vector<StmtPtr> fields, std::vector<StmtPtr> methods)
            : Statement(token), name(name), typeParameters(),
              superclass(), interfaces(), fields(std::move(fields)),
              methods(std::move(methods)) {}

        // Constructor with generic type parameters and inheritance
        ClassStmt(const lexer::Token &token, const std::string &name,
                  std::vector<TypeParameter> typeParameters,
                  TypePtr superclass, std::vector<TypePtr> interfaces,
                  std::vector<StmtPtr> fields, std::vector<StmtPtr> methods)
            : Statement(token), name(name), typeParameters(std::move(typeParameters)),
              superclass(std::move(superclass)), interfaces(std::move(interfaces)),
              fields(std::move(fields)), methods(std::move(methods)) {}

        void accept(Visitor &visitor) override;

        std::string name;
        std::vector<TypeParameter> typeParameters; // Generic type parameters
        TypePtr superclass;                        // Optional superclass
        std::vector<TypePtr> interfaces;           // Implemented interfaces/traits
        std::vector<StmtPtr> fields;
        std::vector<StmtPtr> methods;

        bool isGeneric() const { return !typeParameters.empty(); }
    };

    /**
     * @brief Import statement (e.g., import module.name).
     */
    class ImportStmt : public Statement
    {
    public:
        // Regular import (import module)
        ImportStmt(const lexer::Token &token, const std::string &moduleName)
            : Statement(token), moduleName(moduleName), importAll(true), symbols() {}

        // Import specific symbols (import module.{symbol1, symbol2 as alias})
        ImportStmt(const lexer::Token &token, const std::string &moduleName,
                   std::map<std::string, std::string> symbols)
            : Statement(token), moduleName(moduleName), importAll(false),
              symbols(std::move(symbols)) {}

        void accept(Visitor &visitor) override;

        std::string moduleName;                     // Name of the module to import
        bool importAll;                             // Whether to import all symbols
        std::map<std::string, std::string> symbols; // Symbols to import with optional aliases

        // Optional aliases for the entire module (import module as alias)
        std::string moduleAlias;
    };

    /**
     * @brief Export statement (e.g., export var x, class C, def f()).
     */
    class ExportStmt : public Statement
    {
    public:
        // Export individual symbols
        ExportStmt(const lexer::Token &token, std::vector<std::string> symbols)
            : Statement(token), symbols(std::move(symbols)), exportAll(false),
              declaration(nullptr) {}

        // Export declaration (export def f() {})
        ExportStmt(const lexer::Token &token, StmtPtr declaration)
            : Statement(token), exportAll(false), declaration(std::move(declaration)) {}

        // Export all (export *)
        explicit ExportStmt(const lexer::Token &token, bool exportAll = true)
            : Statement(token), exportAll(exportAll), declaration(nullptr) {}

        void accept(Visitor &visitor) override;

        std::vector<std::string> symbols; // Names of symbols to export
        bool exportAll;                   // Whether to export all declarations
        StmtPtr declaration;              // Optional declaration to export
    };

    /**
     * @brief Module declaration (e.g., module mymodule {}).
     */
    class ModuleStmt : public Statement
    {
    public:
        ModuleStmt(const lexer::Token &token, const std::string &name, std::vector<StmtPtr> body)
            : Statement(token), name(name), body(std::move(body)) {}

        void accept(Visitor &visitor) override;

        std::string name;          // Name of the module
        std::vector<StmtPtr> body; // Module body
    };

    /**
     * @brief Match statement (e.g., match value { case ... }).
     */
    class MatchStmt : public Statement
    {
    public:
        MatchStmt(const lexer::Token &token, ExprPtr value, std::vector<std::pair<ExprPtr, StmtPtr>> cases, StmtPtr defaultCase)
            : Statement(token), value(std::move(value)), cases(std::move(cases)), defaultCase(std::move(defaultCase)) {}
        void accept(Visitor &visitor) override;
        ExprPtr value;
        std::vector<std::pair<ExprPtr, StmtPtr>> cases;
        StmtPtr defaultCase;
    };

    /**
     * @brief New expression for dynamic allocation
     */
    class NewExpr : public Expression
    {
    public:
        NewExpr(const lexer::Token &token, std::shared_ptr<Expression> typeExpr, std::shared_ptr<Expression> sizeExpr = nullptr)
            : Expression(token), typeExpr(std::move(typeExpr)), sizeExpr(std::move(sizeExpr)) {}

        void accept(Visitor &visitor) override;
        TypePtr getType() const override { return nullptr; }

        // Getters
        std::shared_ptr<Expression> getTypeExpr() const { return typeExpr; }
        std::shared_ptr<Expression> getSizeExpr() const { return sizeExpr; }
        const lexer::Token &getKeyword() const { return token; }

    private:
        std::shared_ptr<Expression> typeExpr;
        std::shared_ptr<Expression> sizeExpr; // nullptr for non-array allocations
    };

    /**
     * @brief Delete expression for dynamic deallocation
     */
    class DeleteExpr : public Expression
    {
    public:
        DeleteExpr(const lexer::Token &token, std::shared_ptr<Expression> expr)
            : Expression(token), expr(std::move(expr)) {}

        void accept(Visitor &visitor) override;
        TypePtr getType() const override { return nullptr; }

        // Getters
        std::shared_ptr<Expression> getExpr() const { return expr; }
        const lexer::Token &getKeyword() const { return token; }

    private:
        std::shared_ptr<Expression> expr;
    };

    /**
     * @brief String interpolation expression (e.g., f"Hello {name}!")
     */
    class StringInterpolationExpr : public Expression
    {
    public:
        StringInterpolationExpr(const lexer::Token &token,
                                std::vector<std::string> textParts,
                                std::vector<ExprPtr> expressions)
            : Expression(token), textParts(std::move(textParts)),
              expressions(std::move(expressions)) {}

        const std::vector<std::string> &getTextParts() const { return textParts; }
        const std::vector<ExprPtr> &getExpressions() const { return expressions; }

        TypePtr getType() const override
        {
            // Create a SimpleType representing string
            return std::make_shared<SimpleType>(
                lexer::Token(lexer::TokenType::STRING, "string", "", 0, 0));
        }

        void accept(Visitor &visitor) override;

    private:
        std::vector<std::string> textParts; // Static text parts
        std::vector<ExprPtr> expressions;   // Expressions to evaluate and insert
    };

    /**
     * @brief Channel send expression (e.g., ch <- value)
     */
    class ChannelSendExpr : public Expression
    {
    public:
        ChannelSendExpr(const lexer::Token &token, ExprPtr channel, ExprPtr value)
            : Expression(token), channel(std::move(channel)), value(std::move(value)) {}

        void accept(Visitor &visitor) override;
        TypePtr getType() const override { return nullptr; }

        ExprPtr channel;
        ExprPtr value;
    };

    /**
     * @brief Channel receive expression (e.g., <-ch)
     */
    class ChannelReceiveExpr : public Expression
    {
    public:
        ChannelReceiveExpr(const lexer::Token &token, ExprPtr channel)
            : Expression(token), channel(std::move(channel)) {}

        void accept(Visitor &visitor) override;
        TypePtr getType() const override { return nullptr; }

        ExprPtr channel;
    };

    /**
     * @brief Select statement for handling multiple channel operations
     */
    class SelectStmt : public Statement
    {
    public:
        struct Case
        {
            ExprPtr channel;
            StmtPtr body;
            bool isDefault;

            Case(ExprPtr ch, StmtPtr b, bool def = false)
                : channel(std::move(ch)), body(std::move(b)), isDefault(def) {}
        };

        SelectStmt(const lexer::Token &token, std::vector<Case> cases)
            : Statement(token), cases(std::move(cases)) {}

        void accept(Visitor &visitor) override;

        std::vector<Case> cases;
    };

    /**
     * @brief Array literal expression (e.g., [1, 2, 3]).
     */
    class ArrayLiteralExpr : public Expression
    {
    public:
        ArrayLiteralExpr(const lexer::Token &token, std::vector<ExprPtr> elements)
            : Expression(token), elements(std::move(elements)) {}

        void accept(Visitor &visitor) override { visitor.visitListExpr(this); } // Reuse ListExpr visitor for now
        std::vector<ExprPtr> elements;
        TypePtr getType() const override { return nullptr; }
    };

    // Implementations of the accept methods
    inline void BinaryExpr::accept(Visitor &visitor) { visitor.visitBinaryExpr(this); }
    inline void GroupingExpr::accept(Visitor &visitor) { visitor.visitGroupingExpr(this); }
    inline void LiteralExpr::accept(Visitor &visitor) { visitor.visitLiteralExpr(this); }
    inline void UnaryExpr::accept(Visitor &visitor) { visitor.visitUnaryExpr(this); }
    inline void VariableExpr::accept(Visitor &visitor) { visitor.visitVariableExpr(this); }
    inline void AssignExpr::accept(Visitor &visitor) { visitor.visitAssignExpr(this); }
    inline void CallExpr::accept(Visitor &visitor) { visitor.visitCallExpr(this); }
    inline void GetExpr::accept(Visitor &visitor) { visitor.visitGetExpr(this); }
    inline void SetExpr::accept(Visitor &visitor) { visitor.visitSetExpr(this); }
    inline void ListExpr::accept(Visitor &visitor) { visitor.visitListExpr(this); }
    inline void DictionaryExpr::accept(Visitor &visitor) { visitor.visitDictionaryExpr(this); }
    inline void LambdaExpr::accept(Visitor &visitor) { visitor.visitLambdaExpr(this); }
    inline void AwaitExpr::accept(Visitor &visitor) { visitor.visitAwaitExpr(this); }
    inline void ExpressionStmt::accept(Visitor &visitor) { visitor.visitExpressionStmt(this); }
    inline void VariableStmt::accept(Visitor &visitor) { visitor.visitVariableStmt(this); }
    inline void BlockStmt::accept(Visitor &visitor) { visitor.visitBlockStmt(this); }
    inline void IfStmt::accept(Visitor &visitor) { visitor.visitIfStmt(this); }
    inline void WhileStmt::accept(Visitor &visitor) { visitor.visitWhileStmt(this); }
    inline void ForStmt::accept(Visitor &visitor) { visitor.visitForStmt(this); }
    inline void FunctionStmt::accept(Visitor &visitor) { visitor.visitFunctionStmt(this); }
    inline void ReturnStmt::accept(Visitor &visitor) { visitor.visitReturnStmt(this); }
    inline void ClassStmt::accept(Visitor &visitor) { visitor.visitClassStmt(this); }
    inline void ImportStmt::accept(Visitor &visitor) { visitor.visitImportStmt(this); }
    inline void MatchStmt::accept(Visitor &visitor) { visitor.visitMatchStmt(this); }
    inline void NewExpr::accept(Visitor &visitor) { visitor.visitNewExpr(this); }
    inline void DeleteExpr::accept(Visitor &visitor) { visitor.visitDeleteExpr(this); }
    inline void ExportStmt::accept(Visitor &visitor) { visitor.visitExportStmt(this); }
    inline void ModuleStmt::accept(Visitor &visitor) { visitor.visitModuleStmt(this); }
    inline void StringInterpolationExpr::accept(Visitor &visitor) { visitor.visitStringInterpolationExpr(this); }
    inline void ChannelSendExpr::accept(Visitor &visitor) { visitor.visitChannelSendExpr(this); }
    inline void ChannelReceiveExpr::accept(Visitor &visitor) { visitor.visitChannelReceiveExpr(this); }
    inline void SelectStmt::accept(Visitor &visitor) { visitor.visitSelectStmt(this); }

} // namespace ast

#endif // AST_H
